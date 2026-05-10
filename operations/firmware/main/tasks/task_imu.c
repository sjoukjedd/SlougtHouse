/* ==========================================================================
 * VU-AMS Firmware — IMU acquisition task (ICM-20948)
 * xTaskCreatePinnedToCore(task_imu, "task_imu", 4096, NULL, 7, NULL, 0)
 * Priority: 7  |  Core: 0  |  Stack: 4096 bytes
 * Author: Kai Müller
 * Date:   2026-05-08
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

static spi_device_handle_t s_imu_spi = NULL;

/* Accumulation buffer — 10 samples then post as M-block */
static m_block_t s_current_block;
static uint8_t   s_sample_count = 0;

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

    /* Initialise accumulation block header */
    memset(&s_current_block, 0, sizeof(s_current_block));
    s_current_block.header.type        = BLOCK_TYPE_M;
    s_current_block.header.version     = BLOCK_VERSION;
    s_current_block.header.payload_len = sizeof(s_current_block) - sizeof(vuams_block_header_t);

    ESP_LOGI(TAG, "IMU init done");
}

/* --------------------------------------------------------------------------
 * Task
 * -------------------------------------------------------------------------- */

void task_imu(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "IMU task started on core %d", xPortGetCoreID());

    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        /* TODO: Select USER_BANK_0 via SPI register write          */
        /* TODO: Assert CS, read ACCEL_XOUT_H .. GYRO_ZOUT_L (14 B)*/
        /* TODO: Deassert CS                                        */
        /* TODO: Read magnetometer via AK09916 sub-I2C (ICM slave)  */
        /* TODO: Populate s_current_block.*[s_sample_count]        */

        if (s_sample_count == 0) {
            s_current_block.header.timestamp_us = esp_timer_get_time();
        }

        s_sample_count++;

        if (s_sample_count >= 10) {
            m_block_t *block = (m_block_t *)malloc(sizeof(m_block_t));
            if (block != NULL) {
                memcpy(block, &s_current_block, sizeof(m_block_t));
                if (xQueueSend(g_imu_block_queue, &block, pdMS_TO_TICKS(5)) != pdTRUE) {
                    ESP_LOGW(TAG, "imu_block_queue full — block dropped");
                    free(block);
                }
            } else {
                ESP_LOGE(TAG, "malloc failed for m_block_t");
            }
            s_sample_count = 0;
        }

        /* 100 Hz — 10 ms between samples */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}
