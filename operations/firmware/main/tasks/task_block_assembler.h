#pragma once

/* ==========================================================================
 * VU-AMS Firmware — Block assembler task
 * xTaskCreatePinnedToCore: priority 6, core 1, stack 8192
 * Author: Kai Müller
 *
 * Drains all per-sensor queues, writes blocks into the PSRAM ring buffer,
 * and forwards pointers to the SD writer and BLE streamer queues.
 * ========================================================================== */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief  Initialise the assembler's output queues.
 *         Must be called from app_main before tasks are started.
 */
void task_block_assembler_init(void);

/**
 * @brief  FreeRTOS task entry point.
 *         Pass NULL as pvParameters.
 */
void task_block_assembler(void *pvParameters);
