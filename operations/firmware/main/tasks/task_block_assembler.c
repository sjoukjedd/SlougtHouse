/* ==========================================================================
 * VU-AMS Firmware — Block assembler task
 * xTaskCreatePinnedToCore(task_block_assembler, "task_asm", 8192, NULL, 6, NULL, 1)
 * Priority: 6  |  Core: 1  |  Stack: 8192 bytes
 * Author: Kai Müller
 * Date:   2026-05-08
 *
 * Responsibilities:
 *  1. Drain blocks from all sensor queues (ADC, IMU, I2C).
 *  2. Optionally derive I-blocks from raw ECG/ICG (TODO: Vasquez algorithm).
 *  3. Write serialised blocks into the PSRAM ring buffer.
 *  4. Forward block pointers to g_sd_write_queue and g_ble_tx_queue.
 * ========================================================================== */

#include "task_block_assembler.h"
#include "task_adc.h"
#include "task_imu.h"
#include "task_i2c_sensors.h"
#include "task_sd_writer.h"
#include "task_ble_stream.h"
#include "psram_buffer.h"
#include "data_blocks.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "task_asm";

/* Reference to the global PSRAM ring buffer (defined in main.c) */
extern psram_ringbuf_t g_psram_buf;

/* --------------------------------------------------------------------------
 * Internal: write a block to PSRAM and optionally forward to output queues
 * -------------------------------------------------------------------------- */

static void dispatch_block(const void *block_ptr, size_t block_size, uint8_t type)
{
    /* Write to PSRAM ring buffer */
    size_t written = psram_buf_write(&g_psram_buf, block_ptr, block_size);
    if (written != block_size) {
        ESP_LOGW(TAG, "PSRAM write incomplete: %u/%u bytes, type=0x%02X",
                 (unsigned)written, (unsigned)block_size, type);
    }

    /* Forward to SD writer */
    /* TODO: allocate from a pool allocator instead of heap for determinism */
    void *sd_copy = malloc(block_size);
    if (sd_copy) {
        memcpy(sd_copy, block_ptr, block_size);
        if (xQueueSend(g_sd_write_queue, &sd_copy, pdMS_TO_TICKS(2)) != pdTRUE) {
            ESP_LOGW(TAG, "sd_write_queue full — block 0x%02X dropped", type);
            free(sd_copy);
        }
    }

    /* Forward to BLE streamer (separate copy — different lifetime) */
    void *ble_copy = malloc(block_size);
    if (ble_copy) {
        memcpy(ble_copy, block_ptr, block_size);
        if (xQueueSend(g_ble_tx_queue, &ble_copy, 0) != pdTRUE) {
            /* BLE queue is best-effort: silently drop if full */
            free(ble_copy);
        }
    }
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void task_block_assembler_init(void)
{
    /* Output queues are owned by task_sd_writer and task_ble_stream;
     * those inits must be called before this one. */
    ESP_LOGI(TAG, "Block assembler init done");
}

/* --------------------------------------------------------------------------
 * Task
 * -------------------------------------------------------------------------- */

void task_block_assembler(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "Block assembler task started on core %d", xPortGetCoreID());

    while (1) {
        bool did_work = false;

        /* Drain ADC (A-blocks) */
        a_block_t *a_blk = NULL;
        while (xQueueReceive(g_adc_block_queue, &a_blk, 0) == pdTRUE) {
            dispatch_block(a_blk, sizeof(a_block_t), BLOCK_TYPE_A);
            /* TODO: feed into ICG processing pipeline (Vasquez) */
            /* TODO: when ICG produces an I-block, dispatch_block(i_blk, ...) */
            free(a_blk);
            a_blk = NULL;
            did_work = true;
        }

        /* Drain IMU (M-blocks) */
        m_block_t *m_blk = NULL;
        while (xQueueReceive(g_imu_block_queue, &m_blk, 0) == pdTRUE) {
            dispatch_block(m_blk, sizeof(m_block_t), BLOCK_TYPE_M);
            free(m_blk);
            m_blk = NULL;
            did_work = true;
        }

        /* Drain PPG (P-blocks) */
        p_block_t *p_blk = NULL;
        while (xQueueReceive(g_ppg_block_queue, &p_blk, 0) == pdTRUE) {
            dispatch_block(p_blk, sizeof(p_block_t), BLOCK_TYPE_P);
            free(p_blk);
            p_blk = NULL;
            did_work = true;
        }

        /* Drain Temperature (T-blocks) */
        t_block_t *t_blk = NULL;
        while (xQueueReceive(g_temp_block_queue, &t_blk, 0) == pdTRUE) {
            dispatch_block(t_blk, sizeof(t_block_t), BLOCK_TYPE_T);
            free(t_blk);
            t_blk = NULL;
            did_work = true;
        }

        /* Drain SCL (S-blocks) */
        s_block_t *s_blk = NULL;
        while (xQueueReceive(g_scl_block_queue, &s_blk, 0) == pdTRUE) {
            dispatch_block(s_blk, sizeof(s_block_t), BLOCK_TYPE_S);
            free(s_blk);
            s_blk = NULL;
            did_work = true;
        }

        /* Drain Z-blocks (raw ICG waveform) — SD only, not forwarded to BLE */
        z_block_t *z_blk = NULL;
        while (xQueueReceive(g_z_block_queue, &z_blk, 0) == pdTRUE) {
            /* Write to PSRAM ring buffer */
            size_t written = psram_buf_write(&g_psram_buf, z_blk, sizeof(z_block_t));
            if (written != sizeof(z_block_t)) {
                ESP_LOGW(TAG, "PSRAM write incomplete: %u/%u bytes, type=Z",
                         (unsigned)written, (unsigned)sizeof(z_block_t));
            }

            /* Forward to SD writer only */
            void *sd_copy = malloc(sizeof(z_block_t));
            if (sd_copy) {
                memcpy(sd_copy, z_blk, sizeof(z_block_t));
                if (xQueueSend(g_sd_write_queue, &sd_copy, pdMS_TO_TICKS(2)) != pdTRUE) {
                    ESP_LOGW(TAG, "sd_write_queue full — Z-block dropped");
                    free(sd_copy);
                }
            }

            free(z_blk);
            z_blk = NULL;
            did_work = true;
        }

        if (!did_work) {
            /* Nothing in any queue — yield briefly */
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}
