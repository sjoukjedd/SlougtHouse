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
 * BMP390 register addresses (datasheet Rev 1.8, Section 5)
 * -------------------------------------------------------------------------- */
#define BMP390_REG_CHIP_ID       0x00   /* Should read 0x60                    */
#define BMP390_REG_DATA_0        0x04   /* Pressure XLSB (bits 7:0)            */
#define BMP390_REG_DATA_1        0x05   /* Pressure LSB  (bits 15:8)           */
#define BMP390_REG_DATA_2        0x06   /* Pressure MSB  (bits 23:16)          */
#define BMP390_REG_DATA_3        0x07   /* Temperature XLSB                    */
#define BMP390_REG_DATA_4        0x08   /* Temperature LSB                     */
#define BMP390_REG_DATA_5        0x09   /* Temperature MSB                     */
#define BMP390_REG_OSR           0x1C   /* Oversampling register               */
#define BMP390_REG_ODR           0x1D   /* Output data rate register           */
#define BMP390_REG_PWR_CTRL      0x7C   /* Power control register              */

/* BMP390 register values */
/* PWR_CTRL: press_en=1 (bit0), temp_en=1 (bit1), mode=normal (bits5:4 = 11) */
#define BMP390_PWR_NORMAL        0x33
/* ODR: odr_sel=0x03 → 25 Hz (see datasheet Table 22: 0x03 = 25 Hz)          */
#define BMP390_ODR_25HZ          0x03
/* OSR: osr_p=0x02 (x4 pressure), osr_t=0x01 (x2 temperature) bits 5:3/2:0  */
#define BMP390_OSR_P4_T2         0x0A   /* bits[2:0]=010 (x4), bits[5:3]=001 (x2) */

/* BMP390 NVM trim coefficient register base (datasheet Section 4.6) */
#define BMP390_REG_TRIM_BASE     0x31   /* 21 bytes of trim data               */
#define BMP390_TRIM_LEN          21

/* BMP390 expected chip ID */
#define BMP390_CHIP_ID           0x60

/* --------------------------------------------------------------------------
 * BMP390 trim coefficient structure (integer types from datasheet Section 8.5)
 * -------------------------------------------------------------------------- */
typedef struct {
    uint16_t par_t1;
    uint16_t par_t2;
    int8_t   par_t3;
    int16_t  par_p1;
    int16_t  par_p2;
    int8_t   par_p3;
    int8_t   par_p4;
    uint16_t par_p5;
    uint16_t par_p6;
    int8_t   par_p7;
    int8_t   par_p8;
    int16_t  par_p9;
    int8_t   par_p10;
    int8_t   par_p11;
} bmp390_trim_t;

static bmp390_trim_t s_bmp390_trim;
static bool          s_bmp390_ready = false;

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
QueueHandle_t g_baro_block_queue = NULL;

/* Counters for decimation */
static uint32_t s_tick_count = 0;

/* --------------------------------------------------------------------------
 * BMP390 helpers — low-level I²C register access
 * -------------------------------------------------------------------------- */

static esp_err_t bmp390_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR_BARO << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, buf, sizeof(buf), true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t bmp390_read_regs(uint8_t reg, uint8_t *dst, size_t len)
{
    /* Write register address */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR_BARO << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) return ret;

    /* Read len bytes */
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR_BARO << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, dst, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, dst + len - 1, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/* --------------------------------------------------------------------------
 * BMP390 public API
 * -------------------------------------------------------------------------- */

esp_err_t bmp390_init(void)
{
    uint8_t chip_id = 0;
    esp_err_t ret = bmp390_read_regs(BMP390_REG_CHIP_ID, &chip_id, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BMP390 chip ID read failed: %s", esp_err_to_name(ret));
        return ret;
    }
    if (chip_id != BMP390_CHIP_ID) {
        ESP_LOGE(TAG, "BMP390 unexpected chip ID 0x%02X (expected 0x%02X)", chip_id, BMP390_CHIP_ID);
        return ESP_ERR_NOT_FOUND;
    }

    /* Read NVM trim coefficients (21 bytes from 0x31) */
    uint8_t trim_raw[BMP390_TRIM_LEN] = {0};
    ret = bmp390_read_regs(BMP390_REG_TRIM_BASE, trim_raw, BMP390_TRIM_LEN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BMP390 trim read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Parse trim per datasheet Table 23 (NVM address map, little-endian) */
    s_bmp390_trim.par_t1  = (uint16_t)(trim_raw[1]  << 8 | trim_raw[0]);
    s_bmp390_trim.par_t2  = (uint16_t)(trim_raw[3]  << 8 | trim_raw[2]);
    s_bmp390_trim.par_t3  = (int8_t)   trim_raw[4];
    s_bmp390_trim.par_p1  = (int16_t) (trim_raw[6]  << 8 | trim_raw[5]);
    s_bmp390_trim.par_p2  = (int16_t) (trim_raw[8]  << 8 | trim_raw[7]);
    s_bmp390_trim.par_p3  = (int8_t)   trim_raw[9];
    s_bmp390_trim.par_p4  = (int8_t)   trim_raw[10];
    s_bmp390_trim.par_p5  = (uint16_t)(trim_raw[12] << 8 | trim_raw[11]);
    s_bmp390_trim.par_p6  = (uint16_t)(trim_raw[14] << 8 | trim_raw[13]);
    s_bmp390_trim.par_p7  = (int8_t)   trim_raw[15];
    s_bmp390_trim.par_p8  = (int8_t)   trim_raw[16];
    s_bmp390_trim.par_p9  = (int16_t) (trim_raw[18] << 8 | trim_raw[17]);
    s_bmp390_trim.par_p10 = (int8_t)   trim_raw[19];
    s_bmp390_trim.par_p11 = (int8_t)   trim_raw[20];

    /* Set oversampling: pressure x4, temperature x2 */
    ret = bmp390_write_reg(BMP390_REG_OSR, BMP390_OSR_P4_T2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BMP390 OSR write failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Set ODR to 25 Hz */
    ret = bmp390_write_reg(BMP390_REG_ODR, BMP390_ODR_25HZ);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BMP390 ODR write failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Enable pressure + temperature in normal mode */
    ret = bmp390_write_reg(BMP390_REG_PWR_CTRL, BMP390_PWR_NORMAL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BMP390 PWR_CTRL write failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_bmp390_ready = true;
    ESP_LOGI(TAG, "BMP390 init OK — chip ID 0x%02X, ODR 25 Hz, OSR P×4 T×2", chip_id);
    return ESP_OK;
}

esp_err_t bmp390_read(float *pressure_pa, float *temp_c)
{
    if (!s_bmp390_ready) return ESP_ERR_INVALID_STATE;

    /* Read 6 bytes: DATA_0..DATA_5 (pressure XLSB/LSB/MSB, temp XLSB/LSB/MSB) */
    uint8_t raw[6] = {0};
    esp_err_t ret = bmp390_read_regs(BMP390_REG_DATA_0, raw, 6);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BMP390 data read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    uint32_t adc_P = (uint32_t)raw[2] << 16 | (uint32_t)raw[1] << 8 | raw[0];
    uint32_t adc_T = (uint32_t)raw[5] << 16 | (uint32_t)raw[4] << 8 | raw[3];

    /* ---------- Temperature compensation (datasheet Section 8.5) ------------ */
    /* Uses double for compensation accuracy; cast to float for output          */
    const bmp390_trim_t *t = &s_bmp390_trim;

    double par_t1 = (double)t->par_t1 / 0.00390625;   /* 2^-8  */
    double par_t2 = (double)t->par_t2 / 1073741824.0; /* 2^30  */
    double par_t3 = (double)t->par_t3 / 281474976710656.0; /* 2^48 */

    double partial_data1 = (double)adc_T - par_t1;
    double partial_data2 = partial_data1 * par_t2;
    double t_lin         = partial_data2 + (partial_data1 * partial_data1) * par_t3;
    *temp_c = (float)t_lin;

    /* ---------- Pressure compensation (datasheet Section 8.5) --------------- */
    double par_p1  = ((double)t->par_p1  - 16384.0) / 1048576.0;   /* 2^20 */
    double par_p2  = ((double)t->par_p2  - 16384.0) / 536870912.0; /* 2^29 */
    double par_p3  = (double)t->par_p3   / 4294967296.0;           /* 2^32 */
    double par_p4  = (double)t->par_p4   / 137438953472.0;         /* 2^37 */
    double par_p5  = (double)t->par_p5   / 0.125;                  /* 2^-3 */
    double par_p6  = (double)t->par_p6   / 64.0;                   /* 2^6  */
    double par_p7  = (double)t->par_p7   / 256.0;                  /* 2^8  */
    double par_p8  = (double)t->par_p8   / 32768.0;                /* 2^15 */
    double par_p9  = (double)t->par_p9   / 281474976710656.0;      /* 2^48 */
    double par_p10 = (double)t->par_p10  / 281474976710656.0;      /* 2^48 */
    double par_p11 = (double)t->par_p11  / 36893488147419103232.0; /* 2^65 */

    double partial_p1 = par_p6 * t_lin;
    double partial_p2 = par_p7 * (t_lin * t_lin);
    double partial_p3 = par_p8 * (t_lin * t_lin * t_lin);
    double partial_out1 = par_p5 + partial_p1 + partial_p2 + partial_p3;

    double partial_p4b = par_p2 * t_lin;
    double partial_p5b = par_p3 * (t_lin * t_lin);
    double partial_p6b = par_p4 * (t_lin * t_lin * t_lin);
    double partial_out2 = (double)adc_P * (par_p1 + partial_p4b + partial_p5b + partial_p6b);

    double partial_p7b = (double)adc_P * (double)adc_P;
    double partial_p8b = par_p9 + par_p10 * t_lin;
    double partial_out3 = partial_p7b * partial_p8b;
    double partial_out4 = partial_out3 + ((double)adc_P * (double)adc_P * (double)adc_P) * par_p11;

    *pressure_pa = (float)(partial_out1 + partial_out2 + partial_out4);
    return ESP_OK;
}

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

static void post_b_block(const b_block_t *src)
{
    b_block_t *blk = (b_block_t *)malloc(sizeof(b_block_t));
    if (blk == NULL) { ESP_LOGE(TAG, "malloc b_block_t"); return; }
    memcpy(blk, src, sizeof(b_block_t));
    if (xQueueSend(g_baro_block_queue, &blk, pdMS_TO_TICKS(5)) != pdTRUE) {
        ESP_LOGW(TAG, "baro_block_queue full");
        free(blk);
    }
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void task_i2c_sensors_init(void)
{
    g_ppg_block_queue  = xQueueCreate(8,  sizeof(p_block_t *));
    g_temp_block_queue = xQueueCreate(4,  sizeof(t_block_t *));
    g_scl_block_queue  = xQueueCreate(4,  sizeof(s_block_t *));
    g_baro_block_queue = xQueueCreate(16, sizeof(b_block_t *));

    if (!g_ppg_block_queue || !g_temp_block_queue ||
        !g_scl_block_queue || !g_baro_block_queue) {
        ESP_LOGE(TAG, "Queue creation failed");
        return;
    }

    /* TODO: MAX30101 — write CONFIG registers: red+IR LEDs, 100 sps, 18-bit */
    /* TODO: TMP117  — set conversion mode, 1-shot or continuous             */
    /* TODO: AD5933  — set frequency sweep parameters for EDA measurement    */

    /* BMP390 barometer — normal mode, 25 Hz, OSR P×4 T×2 */
    bmp390_init();

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

        /* --- Barometer (BMP390): 25 Hz — every 4 ticks (100 Hz base / 4) --- */
        if (s_tick_count % 4 == 0) {
            b_block_t blk;
            memset(&blk, 0, sizeof(blk));
            blk.header.type         = BLOCK_TYPE_B;
            blk.header.version      = BLOCK_VERSION;
            blk.header.payload_len  = sizeof(blk) - sizeof(vuams_block_header_t);
            blk.header.timestamp_us = now_us;

            float pressure_pa = 0.0f;
            float temp_c      = 0.0f;
            esp_err_t bret = bmp390_read(&pressure_pa, &temp_c);
            if (bret == ESP_OK) {
                blk.baro_pressure_pa = pressure_pa;
                blk.baro_temp_c      = temp_c;
            } else {
                blk.baro_pressure_pa = 0.0f;
                blk.baro_temp_c      = 0.0f;
                ESP_LOGW(TAG, "BMP390 read error: %s", esp_err_to_name(bret));
            }

            post_b_block(&blk);
        }

        s_tick_count++;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}
