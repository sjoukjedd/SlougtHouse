#pragma once

/* ==========================================================================
 * VU-AMS Firmware — IMU acquisition task (ICM-20948)
 * xTaskCreatePinnedToCore: priority 7, core 0, stack 4096
 * Author: Kai Müller
 * ========================================================================== */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief  Queue handle for outgoing M-blocks (m_block_t).
 *         Populated by task_imu, consumed by task_block_assembler.
 */
extern QueueHandle_t g_imu_block_queue;

/**
 * @brief  Initialise SPI device handle for ICM-20948.
 *         Must be called from app_main after spi_bus_initialize().
 */
void task_imu_init(void);

/**
 * @brief  FreeRTOS task entry point.
 *         Pass NULL as pvParameters.
 */
void task_imu(void *pvParameters);
