#pragma once

/* ==========================================================================
 * VU-AMS Firmware — SD card writer task
 * xTaskCreatePinnedToCore: priority 5, core 1, stack 8192
 * Author: Kai Müller
 * ========================================================================== */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief  Queue handle for incoming block pointers (void *).
 *         Populated by task_block_assembler, drained by task_sd_writer.
 *         Blocks are heap-allocated by the assembler and freed here after write.
 */
extern QueueHandle_t g_sd_write_queue;

/**
 * @brief  Initialise the SD write queue and mount the FAT filesystem.
 *         Must be called from app_main after SPI bus initialisation.
 */
void task_sd_writer_init(void);

/**
 * @brief  FreeRTOS task entry point.
 *         Pass NULL as pvParameters.
 */
void task_sd_writer(void *pvParameters);
