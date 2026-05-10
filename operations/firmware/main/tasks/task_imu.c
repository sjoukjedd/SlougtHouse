/* ==========================================================================
 * VU-AMS Firmware — IMU acquisition task (ICM-20948)
 * xTaskCreatePinnedToCore(task_imu, "task_imu", 4096, NULL, 7, NULL, 0)
 * Priority: 7  |  Core: 0  |  Stack: 4096 bytes
 * Author: Kai Müller
 * Date:   2026-05-08
 *
 * ODR configuration:
 *  - Accelerometer: 1125 Hz (ACCEL_SMPLRT_DIV=0, closest to 1 kHz)
 *    Required for Speech Activity Detection (SAD) — see har_sad_spec_001.md.
 *  - Gyroscope:      100 Hz (GYRO_SMPLRT_DIV=10)
 *  - Magnetometer:   ~100 Hz via sub-I2C (AK09916, ODR=100 Hz mode)
 *
 * Two accumulation buffers run in parallel:
 *  1. M-block (m_block_t): 9-channel, 10 samples @ 100 Hz — for HAR.
 *     Decimates accelerometer from 1125 Hz → 100 Hz by sampling every 11th
 *     accel interrupt tick.  Gyro and magnetometer are read at that same tick.
 *  2. V-block (v_block_t): 3-channel accel only, 100 samples @ ~1125 Hz
 *     (spec target 1 kHz; 1125 Hz is the ICM-20948 minimum SMPLRT_DIV=0 rate).
 *     SD-only; never forwarded to BLE.
 * ========================================================================== */

#include "task_imu.h"
#include "config.h"
#include "data_blocks.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "task_imu";

QueueHandle_t g_imu_block_queue = NULL;
QueueHandle_t g_v_block_queue   = NULL;

static spi_device_handle_t s_imu_spi = NULL;

/* --------------------------------------------------------------------------
 * M-block accumulation (100 Hz, 10 samples)
 * -------------------------------------------------------------------------- */
static m_block_t s_m_block;
static uint8_t   s_m_sample_count = 0;

/* --------------------------------------------------------------------------
 * V-block accumulation (1125 Hz, 100 samples)
 * -------------------------------------------------------------------------- */
static v_block_t  s_v_block;
static uint8_t    s_v_sample_count = 0;
static uint16_t   s_v_seq          = 0;

/* Decimation counter: M-block samples every IMU_ACCEL_RATE_HZ/IMU_GYRO_RATE_HZ
 * accel ticks.  1125 / 100 ≈ 11.25 — use 11 to avoid drift accumulation;
 * the gyroscope SMPLRT_DIV=10 register setting independently gates gyro reads.
 * The task loop runs at the accelerometer rate (every ~0.889 ms at 1125 Hz);
 * gyro and magnetometer are read at the coarser 100 Hz cadence.             */
#define ACCEL_TICKS_PER_GYRO_SAMPLE  11   /* floor(1125/100) */

static uint16_t s_accel_tick = 0;   /* counts accel interrupts; wraps at UINT16_MAX */

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void task_imu_init(void)
{
    g_imu_block_queue = xQueueCreate(4, sizeof(m_block_t *));
    if (g_imu_block_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create imu_block_queue");
        return;
    }

    /* g_v_block_queue is created in main.c before task_imu_init() is called,
     * following the same pattern as g_z_block_queue. */
    if (g_v_block_queue == NULL) {
        ESP_LOGE(TAG, "g_v_block_queue not initialised — call xQueueCreate in main.c first");
        return;
    }

    /* Configure CS GPIO */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_IMU_CS),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_IMU_CS, 1);

    /* Register ICM-20948 on shared SPI bus */
    spi_device_interface_config_t dev_cfg = {
        .command_bits   = 0,
        .address_bits   = 0,
        .dummy_bits     = 0,
        .mode           = 0,                /* ICM-20948: CPOL=0, CPHA=0 */
        .clock_speed_hz = SPI_CLK_IMU_HZ,
        .spics_io_num   = -1,               /* CS handled manually */
        .queue_size     = 4,
    };
    esp_err_t err = spi_bus_add_device(SPI_HOST_DEV, &dev_cfg, &s_imu_spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(err));
    }

    /* --------------------------------------------------------------------- *
     * ICM-20948 ODR register configuration (USER_BANK_2)
     *
     * Accelerometer: ACCEL_SMPLRT_DIV_1 (0x10) = 0x00, ACCEL_SMPLRT_DIV_2 (0x11) = 0x00
     *   ODR = 1125 / (1 + 0) = 1125 Hz  (closest available to IMU_ACCEL_RATE_HZ 1000)
     *
     * Gyroscope: GYRO_SMPLRT_DIV (0x00) = 0x0A (decimal 10)
     *   ODR = 1100 / (1 + 10) = 100 Hz  (matches IMU_GYRO_RATE_HZ 100)
     *
     * TODO: Switch to USER_BANK_2 via REG_BANK_SEL (0x7F, value 0x20)
     * TODO: Write GYRO_SMPLRT_DIV  (reg 0x00) = 0x0A
     * TODO: Write ACCEL_SMPLRT_DIV_1 (reg 0x10) = 0x00
     * TODO: Write ACCEL_SMPLRT_DIV_2 (reg 0x11) = 0x00
     * TODO: Switch back to USER_BANK_0 via REG_BANK_SEL (0x7F, value 0x00)
     * --------------------------------------------------------------------- */

    /* Initialise M-block header */
    memset(&s_m_block, 0, sizeof(s_m_block));
    s_m_block.header.type        = BLOCK_TYPE_M;
    s_m_block.header.version     = BLOCK_VERSION;
    s_m_block.header.payload_len = sizeof(s_m_block) - sizeof(vuams_block_header_t);

    /* Initialise V-block header fields that never change */
    memset(&s_v_block, 0, sizeof(s_v_block));
    s_v_block.block_type = BLOCK_TYPE_V;

    ESP_LOGI(TAG, "IMU init done — accel ODR ~%d Hz (SMPLRT_DIV=0), gyro ODR %d Hz (SMPLRT_DIV=10)",
             IMU_ACCEL_RATE_HZ, IMU_GYRO_RATE_HZ);
}

/* --------------------------------------------------------------------------
 * Task
 *
 * The loop runs at the accelerometer rate (~1125 Hz → period ~0.889 ms).
 * vTaskDelayUntil is used with a 1 ms tick period as the closest integer
 * approximation; the ICM-20948 FIFO or DRDY interrupt should gate actual
 * reads in the final implementation (marked TODO below).
 * -------------------------------------------------------------------------- */

void task_imu(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "IMU task started on core %d", xPortGetCoreID());

    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        /* TODO: Poll INT_STATUS (USER_BANK_0, reg 0x19) or await DRDY GPIO   */
        /* TODO: Assert CS, select USER_BANK_0 (REG_BANK_SEL=0x00)            */
        /* TODO: Burst-read ACCEL_XOUT_H..ACCEL_ZOUT_L (6 bytes, regs 0x2D-0x32)  */
        /* TODO: Deassert CS                                                   */

        /* Placeholder raw accelerometer values — replace with SPI read */
        int16_t raw_ax = 0;
        int16_t raw_ay = 0;
        int16_t raw_az = 0;

        /* ------------------------------------------------------------------ *
         * V-block accumulation — every accel sample (1125 Hz)
         * ------------------------------------------------------------------ */
        if (s_v_sample_count == 0) {
            s_v_block.timestamp_us = (uint32_t)esp_timer_get_time();
        }
        s_v_block.ax[s_v_sample_count] = raw_ax;
        s_v_block.ay[s_v_sample_count] = raw_ay;
        s_v_block.az[s_v_sample_count] = raw_az;
        s_v_sample_count++;

        if (s_v_sample_count >= V_BLOCK_SAMPLES) {
            s_v_block.seq = s_v_seq++;
            /* CRC placeholder — compute over block bytes when CRC library is wired */
            s_v_block.crc = 0x0000;

            v_block_t *vblk = (v_block_t *)malloc(sizeof(v_block_t));
            if (vblk != NULL) {
                memcpy(vblk, &s_v_block, sizeof(v_block_t));
                if (xQueueSend(g_v_block_queue, &vblk, pdMS_TO_TICKS(5)) != pdTRUE) {
                    ESP_LOGW(TAG, "v_block_queue full — V-block dropped");
                    free(vblk);
                }
            } else {
                ESP_LOGE(TAG, "malloc failed for v_block_t");
            }
            s_v_sample_count = 0;
        }

        /* ------------------------------------------------------------------ *
         * M-block accumulation — every ACCEL_TICKS_PER_GYRO_SAMPLE accel ticks
         * (~100 Hz), including gyro and magnetometer
         * ------------------------------------------------------------------ */
        if (s_accel_tick % ACCEL_TICKS_PER_GYRO_SAMPLE == 0) {
            /* TODO: Burst-read GYRO_XOUT_H..GYRO_ZOUT_L (6 bytes, regs 0x33-0x38)  */
            /* TODO: Read magnetometer via AK09916 sub-I2C (ICM slave I2C)           */
            int16_t raw_gx = 0, raw_gy = 0, raw_gz = 0;
            int16_t raw_mx = 0, raw_my = 0, raw_mz = 0;

            if (s_m_sample_count == 0) {
                s_m_block.header.timestamp_us = esp_timer_get_time();
            }

            s_m_block.ax[s_m_sample_count] = raw_ax;
            s_m_block.ay[s_m_sample_count] = raw_ay;
            s_m_block.az[s_m_sample_count] = raw_az;
            s_m_block.gx[s_m_sample_count] = raw_gx;
            s_m_block.gy[s_m_sample_count] = raw_gy;
            s_m_block.gz[s_m_sample_count] = raw_gz;
            s_m_block.mx[s_m_sample_count] = raw_mx;
            s_m_block.my[s_m_sample_count] = raw_my;
            s_m_block.mz[s_m_sample_count] = raw_mz;
            s_m_sample_count++;

            if (s_m_sample_count >= 10) {
                m_block_t *mblk = (m_block_t *)malloc(sizeof(m_block_t));
                if (mblk != NULL) {
                    memcpy(mblk, &s_m_block, sizeof(m_block_t));
                    if (xQueueSend(g_imu_block_queue, &mblk, pdMS_TO_TICKS(5)) != pdTRUE) {
                        ESP_LOGW(TAG, "imu_block_queue full — M-block dropped");
                        free(mblk);
                    }
                } else {
                    ESP_LOGE(TAG, "malloc failed for m_block_t");
                }
                s_m_sample_count = 0;
            }
        }

        s_accel_tick++;

        /* ~1 ms tick period — closest FreeRTOS tick to 1125 Hz sample period.
         * Final implementation should use ICM-20948 DRDY interrupt instead.  */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1));
    }
}
