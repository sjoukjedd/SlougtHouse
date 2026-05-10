#pragma once

/* ==========================================================================
 * VU-AMS Firmware — ADC acquisition task (ADS1256)
 * xTaskCreatePinnedToCore: priority 8, core 0, stack 4096
 * Author: Kai Müller
 * ========================================================================== */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief  Queue handle for outgoing A-blocks (a_block_t).
 *         Populated by task_adc, consumed by task_block_assembler.
 *         Created in task_adc_init(); extern for main.c.
 */
extern QueueHandle_t g_adc_block_queue;

/**
 * @brief  Queue handle for outgoing Z-blocks (z_block_t).
 *         Populated by task_adc, consumed by task_block_assembler (SD only).
 *         Created in app_main (xQueueCreate); extern declared here.
 */
extern QueueHandle_t g_z_block_queue;

/**
 * @brief  Initialise SPI device handle and DRDY GPIO interrupt for ADS1256.
 *         Must be called from app_main after spi_bus_initialize().
 */
void task_adc_init(void);

/**
 * @brief  FreeRTOS task entry point.
 *         Pass NULL as pvParameters.
 */
void task_adc(void *pvParameters);
