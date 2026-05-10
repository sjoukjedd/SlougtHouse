/* ==========================================================================
 * VU-AMS Firmware — I2C sensors task (MAX30101, TMP117, BMP390, AD5933)
 * xTaskCreatePinnedToCore(task_i2c_sensors, "task_i2c", 4096, NULL, 6, NULL, 0)
 * Priority: 6  |  Core: 0  |  Stack: 4096 bytes
 * Author: Kai Müller
 * Date:   2026-05-08
 * ========================================================================== */

#include "task_i2c_sensors.h"
#include "config.h"
#include "data_blocks.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "task_i2c";

/* --------------------------------------------------------------------------
 * AD5933 register addresses
 * -------------------------------------------------------------------------- */
#define AD5933_REG_CONTROL_HB   0x80    /* Control register — high byte        */
#define AD5933_REG_STATUS        0x8F    /* Status register                     */
#define AD5933_REG_REAL_HB       0x94    /* Real data — high byte               */
#define AD5933_REG_REAL_LB       0x95    /* Real data — low byte                */
#define AD5933_REG_IMAG_HB       0x96    /* Imaginary data — high byte          */
#define AD5933_REG_IMAG_LB       0x97    /* Imaginary data — low byte           */

/* AD5933 control register commands (high byte, D7:D4) */
#define AD5933_CMD_INIT_FREQ     0x10    /* Initialize with start frequency     */
#define AD5933_CMD_START_SWEEP   0x20    /* Start frequency sweep               */
#define AD5933_CMD_STANDBY       0xB0    /* Standby mode                        */
#define AD5933_CMD_POWER_DOWN    0xA0    /* Power-down mode                     */

/* AD5933 status register bits */
#define AD5933_STATUS_DATA_VALID (1 << 1) /* Bit 1: valid real/imaginary data   */

/* Gain factor placeholder — set to 1.0 until manufacturing calibration */
#define AD5933_GAIN_FACTOR       1.0f

/* Poll limits for data-valid bit */
#define AD5933_POLL_MAX_RETRIES  50
#define AD5933_POLL_DELAY_MS     1

QueueHandle_t g_ppg_block_queue  = NULL;
QueueHandle_t g_temp_block_queue = NULL;
QueueHandle_t g_scl_block_queue  = NULL;

/* Counters for decimation */
static uint32_t s_tick_count = 0;

/* --------------------------------------------------------------------------
 * AD5933 helpers — low-level I²C register access
 * -------------------------------------------------------------------------- */

/**
 * @brief Write one byte to an AD5933 register.
 */
static esp_err_t ad5933_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR_AD5933 << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, buf, sizeof(buf), true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief Read one byte from an AD5933 register.
 */
static esp_err_t ad5933_read_reg(uint8_t reg, uint8_t *value)
{
    /* Address pointer set */
    uint8_t addr_cmd[2] = { 0xB0, reg }; /* block-read command byte + register */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR_AD5933 << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, addr_cmd, sizeof(addr_cmd), true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) return ret;

    /* Read one byte */
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR_AD5933 << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/* --------------------------------------------------------------------------
 * AD5933 power management and measurement
 * -------------------------------------------------------------------------- */

/**
 * @brief Put AD5933 into standby then power-down mode.
 *
 * The datasheet requires a transition through standby before power-down.
 * At power-down the device draws <1 µA.
 */
static void ad5933_power_down(void)
{
    esp_err_t ret;

    ret = ad5933_write_reg(AD5933_REG_CONTROL_HB, AD5933_CMD_STANDBY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AD5933 standby failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = ad5933_write_reg(AD5933_REG_CONTROL_HB, AD5933_CMD_POWER_DOWN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AD5933 power-down failed: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief Perform a single-point impedance measurement.
 *
 * Sequence: Init → 10 ms settling → Start sweep → poll status → read
 * real/imaginary → compute magnitude → convert to impedance → power down.
 *
 * @param[out] magnitude_ohms  Measured impedance in ohms.
 * @return ESP_OK on success, error code on failure (device left in power-down).
 */
static esp_err_t ad5933_measure_impedance(float *magnitude_ohms)
{
    esp_err_t ret;

    /* 1. Initialize with start frequency */
    ret = ad5933_write_reg(AD5933_REG_CONTROL_HB, AD5933_CMD_INIT_FREQ);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AD5933 init freq failed: %s", esp_err_to_name(ret));
        ad5933_power_down();
        return ret;
    }

    /* 2. Allow internal oscillator to settle (10 ms) */
    vTaskDelay(pdMS_TO_TICKS(10));

    /* 3. Start frequency sweep */
    ret = ad5933_write_reg(AD5933_REG_CONTROL_HB, AD5933_CMD_START_SWEEP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AD5933 start sweep failed: %s", esp_err_to_name(ret));
        ad5933_power_down();
        return ret;
    }

    /* 4. Poll status register until data-valid bit is set */
    uint8_t status = 0;
    int retries = 0;
    do {
        vTaskDelay(pdMS_TO_TICKS(AD5933_POLL_DELAY_MS));
        ret = ad5933_read_reg(AD5933_REG_STATUS, &status);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "AD5933 status read failed: %s", esp_err_to_name(ret));
            ad5933_power_down();
            return ret;
        }
        retries++;
    } while (!(status & AD5933_STATUS_DATA_VALID) && retries < AD5933_POLL_MAX_RETRIES);

    if (!(status & AD5933_STATUS_DATA_VALID)) {
        ESP_LOGE(TAG, "AD5933 data not valid after %d retries", AD5933_POLL_MAX_RETRIES);
        ad5933_power_down();
        return ESP_ERR_TIMEOUT;
    }

    /* 5. Read real and imaginary registers (2 bytes each, big-endian) */
    uint8_t real_hb = 0, real_lb = 0, imag_hb = 0, imag_lb = 0;

    ret  = ad5933_read_reg(AD5933_REG_REAL_HB, &real_hb);
    ret |= ad5933_read_reg(AD5933_REG_REAL_LB, &real_lb);
    ret |= ad5933_read_reg(AD5933_REG_IMAG_HB, &imag_hb);
    ret |= ad5933_read_reg(AD5933_REG_IMAG_LB, &imag_lb);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AD5933 data register read failed");
        ad5933_power_down();
        return ret;
    }

    /* 6. Reconstruct signed 16-bit values (two's complement, big-endian) */
    int16_t real_raw = (int16_t)((real_hb << 8) | real_lb);
    int16_t imag_raw = (int16_t)((imag_hb << 8) | imag_lb);

    /* 7. Compute magnitude */
    float magnitude = sqrtf((float)real_raw * (float)real_raw +
                            (float)imag_raw * (float)imag_raw);

    /* 8. Convert to impedance using gain factor (placeholder = 1.0) */
    if (magnitude < 1e-9f) {
        ESP_LOGE(TAG, "AD5933 magnitude near zero — open circuit?");
        ad5933_power_down();
        return ESP_ERR_INVALID_RESPONSE;
    }
    *magnitude_ohms = 1.0f / (magnitude * AD5933_GAIN_FACTOR);

    /* 9. Power down between measurements */
    ad5933_power_down();

    return ESP_OK;
}

/* --------------------------------------------------------------------------
 * Internal: post a block to a queue (malloc + memcpy pattern)
 * -------------------------------------------------------------------------- */

static void post_p_block(const p_block_t *src)
{
    p_block_t *blk = (p_block_t *)malloc(sizeof(p_block_t));
    if (blk == NULL) { ESP_LOGE(TAG, "malloc p_block_t"); return; }
    memcpy(blk, src, sizeof(p_block_t));
    if (xQueueSend(g_ppg_block_queue, &blk, pdMS_TO_TICKS(5)) != pdTRUE) {
        ESP_LOGW(TAG, "ppg_block_queue full");
        free(blk);
    }
}

static void post_t_block(const t_block_t *src)
{
    t_block_t *blk = (t_block_t *)malloc(sizeof(t_block_t));
    if (blk == NULL) { ESP_LOGE(TAG, "malloc t_block_t"); return; }
    memcpy(blk, src, sizeof(t_block_t));
    if (xQueueSend(g_temp_block_queue, &blk, pdMS_TO_TICKS(5)) != pdTRUE) {
        ESP_LOGW(TAG, "temp_block_queue full");
        free(blk);
    }
}

static void post_s_block(const s_block_t *src)
{
    s_block_t *blk = (s_block_t *)malloc(sizeof(s_block_t));
    if (blk == NULL) { ESP_LOGE(TAG, "malloc s_block_t"); return; }
    memcpy(blk, src, sizeof(s_block_t));
    if (xQueueSend(g_scl_block_queue, &blk, pdMS_TO_TICKS(5)) != pdTRUE) {
        ESP_LOGW(TAG, "scl_block_queue full");
        free(blk);
    }
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void task_i2c_sensors_init(void)
{
    g_ppg_block_queue  = xQueueCreate(8, sizeof(p_block_t *));
    g_temp_block_queue = xQueueCreate(4, sizeof(t_block_t *));
    g_scl_block_queue  = xQueueCreate(4, sizeof(s_block_t *));

    if (!g_ppg_block_queue || !g_temp_block_queue || !g_scl_block_queue) {
        ESP_LOGE(TAG, "Queue creation failed");
        return;
    }

    /* TODO: MAX30101 — write CONFIG registers: red+IR LEDs, 100 sps, 18-bit */
    /* TODO: TMP117  — set conversion mode, 1-shot or continuous             */
    /* TODO: BMP390  — set oversampling ratios, IIR filter                   */
    /* TODO: AD5933  — set frequency sweep parameters for EDA measurement    */

    ESP_LOGI(TAG, "AD5933 duty-cycle mode: ~10Hz, power-down between measurements");
    ESP_LOGI(TAG, "I2C sensors init done");
}

/* --------------------------------------------------------------------------
 * Task — main loop ticks at 10 ms (100 Hz base rate)
 * -------------------------------------------------------------------------- */

void task_i2c_sensors(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "I2C sensors task started on core %d", xPortGetCoreID());

    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        uint64_t now_us = esp_timer_get_time();

        /* --- PPG (MAX30101): 100 Hz — every tick ----------------------------*/
        {
            p_block_t blk;
            memset(&blk, 0, sizeof(blk));
            blk.header.type        = BLOCK_TYPE_P;
            blk.header.version     = BLOCK_VERSION;
            blk.header.payload_len = sizeof(blk) - sizeof(vuams_block_header_t);
            blk.header.timestamp_us = now_us;

            /* TODO: Read FIFO_DATA register (3 bytes red + 3 bytes IR)        */
            /* TODO: Run SpO2/HR algorithm (MAX30101 application note)         */
            blk.spo2_pct  = NAN; /* placeholder until algorithm is wired      */
            blk.hr_ppg    = 0;
            blk.ppg_valid = 0;

            post_p_block(&blk);
        }

        /* --- Temperature (TMP117): ~4 Hz — every 25 ticks -------------------*/
        if (s_tick_count % 25 == 0) {
            t_block_t blk;
            memset(&blk, 0, sizeof(blk));
            blk.header.type         = BLOCK_TYPE_T;
            blk.header.version      = BLOCK_VERSION;
            blk.header.payload_len  = sizeof(blk) - sizeof(vuams_block_header_t);
            blk.header.timestamp_us = now_us;

            /* TODO: Read TMP117 RESULT register (2 bytes big-endian)          */
            /* TODO: Convert: temp_c = raw * 0.0078125f                        */
            blk.temp_raw    = 0;
            blk.skin_temp_c = 0.0f;

            post_t_block(&blk);
        }

        /* --- EDA / SCL (AD5933): ~10 Hz — duty-cycled with power-down ------
         * Runs every 10 ticks (100 ms period at the 100 Hz base rate).
         * ad5933_measure_impedance() issues Init→10 ms settle→Start Sweep→
         * poll (up to 50 ms) → Read → PowerDown.  Active time ≈ 11–61 ms,
         * duty cycle ≈ 11–61 % of the 100 ms window → avg current ~1.65–9 mA
         * over the active window, ~0 mA otherwise.  The vTaskDelayUntil call
         * below absorbs the measurement time and resynchronises the tick.
         * --------------------------------------------------------------------- */
        if (s_tick_count % 10 == 0) {
            s_block_t blk;
            memset(&blk, 0, sizeof(blk));
            blk.header.type         = BLOCK_TYPE_S;
            blk.header.version      = BLOCK_VERSION;
            blk.header.payload_len  = sizeof(blk) - sizeof(vuams_block_header_t);
            blk.header.timestamp_us = now_us;

            float impedance_ohms = 0.0f;
            esp_err_t mret = ad5933_measure_impedance(&impedance_ohms);
            if (mret == ESP_OK) {
                /* Convert impedance to conductance in µS (1/Ω → S → µS) */
                blk.scl_tonic_uS     = (impedance_ohms > 0.0f)
                                       ? (1.0f / impedance_ohms) * 1e6f
                                       : 0.0f;
                blk.scl_phasic_uS    = 0.0f; /* phasic decomposition not yet implemented */
                blk.electrode_contact = 1;
            } else {
                blk.scl_tonic_uS     = 0.0f;
                blk.scl_phasic_uS    = 0.0f;
                blk.electrode_contact = 0;
            }

            post_s_block(&blk);
        }

        /* BMP390 barometer: not yet wired to a block type — TODO             */

        s_tick_count++;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}
