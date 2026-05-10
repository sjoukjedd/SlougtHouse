/* ==========================================================================
 * VU-AMS Firmware — SD card writer task
 * xTaskCreatePinnedToCore(task_sd_writer, "task_sd", 8192, NULL, 5, NULL, 1)
 * Priority: 5  |  Core: 1  |  Stack: 8192 bytes
 * Author: Kai Müller
 * Date:   2026-05-09
 *
 * Writes binary block stream to SD card via FAT filesystem (SDMMC driver).
 * SD card is accessed in native 4-bit SDMMC mode at 40 MHz (~20 MB/s).
 * SPI2_HOST is now exclusively used by ADS1256 and ICM-20948.
 * File format: sequential packed structs, one file per recording session.
 * File naming: /sdcard/VUAMS_<timestamp>.bin
 * ========================================================================== */

#include "task_sd_writer.h"
#include "task_usb_guard.h"
#include "config.h"
#include "data_blocks.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG     = "task_sd";
#define MOUNT_POINT "/sdcard"

QueueHandle_t g_sd_write_queue = NULL;

static sdmmc_card_t *s_card    = NULL;
static FILE         *s_fp      = NULL;

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void task_sd_writer_init(void)
{
    g_sd_write_queue = xQueueCreate(SD_BLOCK_QUEUE_DEPTH, sizeof(void *));
    if (g_sd_write_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create sd_write_queue");
        return;
    }

    /* SDMMC host — slot 1, 40 MHz high-speed */
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot         = SDMMC_HOST_SLOT_1;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;  /* 40 000 kHz */

    /* Slot config — 4-bit bus, explicit GPIO mapping, card-detect pull-up */
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width    = 4;
    slot_config.clk      = PIN_SDMMC_CLK;
    slot_config.cmd      = PIN_SDMMC_CMD;
    slot_config.d0       = PIN_SDMMC_D0;
    slot_config.d1       = PIN_SDMMC_D1;
    slot_config.d2       = PIN_SDMMC_D2;
    slot_config.d3       = PIN_SDMMC_D3;
    slot_config.cd       = PIN_SDMMC_CD;
    slot_config.flags   |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;  /* pull-up on CD */

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files              = 4,
        .allocation_unit_size   = 16 * 1024,
    };

    esp_err_t err = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config,
                                             &mount_config, &s_card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %s", esp_err_to_name(err));
        return;
    }

    sdmmc_card_print_info(stdout, s_card);
    ESP_LOGI(TAG, "SD card mounted at %s (SDMMC 4-bit, %d kHz)",
             MOUNT_POINT, host.max_freq_khz);
}

/* --------------------------------------------------------------------------
 * Internal: open a new session file
 * -------------------------------------------------------------------------- */

static void open_session_file(void)
{
    char path[64];
    snprintf(path, sizeof(path), MOUNT_POINT "/VUAMS_%llu.bin",
             (unsigned long long)esp_timer_get_time());

    s_fp = fopen(path, "wb");
    if (s_fp == NULL) {
        ESP_LOGE(TAG, "Failed to open session file: %s", path);
    } else {
        ESP_LOGI(TAG, "Recording to %s", path);
    }
}

/* --------------------------------------------------------------------------
 * Internal: close the current session file cleanly
 * -------------------------------------------------------------------------- */

static void close_session_file(void)
{
    if (s_fp != NULL) {
        fflush(s_fp);
        fclose(s_fp);
        s_fp = NULL;
        ESP_LOGI(TAG, "Session file closed");
    }
}

/* --------------------------------------------------------------------------
 * Task
 * -------------------------------------------------------------------------- */

void task_sd_writer(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "SD writer task started on core %d", xPortGetCoreID());

    /* --- Type BF interlock: refuse to start recording if USB is present --- */
    if (usb_guard_is_connected()) {
        ESP_LOGE(TAG,
            "Recording blocked: USB connected (Type BF safety interlock). "
            "Disconnect USB-C before recording.");
        /* s_fp remains NULL; the write loop will drain and discard the queue. */
    } else if (s_card != NULL) {
        open_session_file();
    } else {
        ESP_LOGW(TAG, "SD card not mounted — writer idle");
    }

    while (1) {
        void *block_ptr = NULL;

        /* Block indefinitely waiting for the next block */
        if (xQueueReceive(g_sd_write_queue, &block_ptr, portMAX_DELAY) == pdTRUE) {
            if (block_ptr == NULL) {
                continue;
            }

            /* --- Type BF interlock: USB connected mid-recording ----------- */
            if (usb_guard_is_connected()) {
                if (s_fp != NULL) {
                    /* USB connected while a session was open — stop cleanly. */
                    ESP_LOGE(TAG,
                        "USB connected during recording — stopping session "
                        "(Type BF safety interlock, IEC 60601-1)");
                    close_session_file();
                } else {
                    ESP_LOGE(TAG,
                        "Recording blocked: USB connected (Type BF safety interlock)");
                }
                free(block_ptr);
                continue;
            }

            if (s_fp == NULL) {
                /* SD not mounted or not yet available — discard. */
                free(block_ptr);
                continue;
            }

            /* Determine block size from type field (first byte of header) */
            uint8_t type = *(uint8_t *)block_ptr;
            size_t write_size = 0;
            switch (type) {
                case BLOCK_TYPE_A: write_size = sizeof(a_block_t); break;
                case BLOCK_TYPE_I: write_size = sizeof(i_block_t); break;
                case BLOCK_TYPE_M: write_size = sizeof(m_block_t); break;
                case BLOCK_TYPE_P: write_size = sizeof(p_block_t); break;
                case BLOCK_TYPE_S: write_size = sizeof(s_block_t); break;
                case BLOCK_TYPE_T: write_size = sizeof(t_block_t); break;
                case BLOCK_TYPE_B: write_size = sizeof(b_block_t); break;
                case BLOCK_TYPE_V: write_size = sizeof(v_block_t); break;
                case BLOCK_TYPE_Z: write_size = sizeof(z_block_t); break;
                default:
                    ESP_LOGW(TAG, "Unknown block type 0x%02X — skipping", type);
                    free(block_ptr);
                    continue;
            }

            size_t written = fwrite(block_ptr, 1, write_size, s_fp);
            if (written != write_size) {
                ESP_LOGE(TAG, "fwrite incomplete: %u/%u", (unsigned)written, (unsigned)write_size);
                /* TODO: set EVT_SD_ERROR in system event group */
            }

            /* TODO: periodic fflush — every N blocks or on CMD_RECORDING_STOP */

            free(block_ptr);
        }
    }
}
