#pragma once

/* ==========================================================================
 * VU-AMS Firmware — I2C sensors task (MAX30101, TMP117, BMP390, AD5933)
 * xTaskCreatePinnedToCore: priority 6, core 0, stack 4096
 * Author: Kai Müller
 * ========================================================================== */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief  Queue handle for outgoing P-blocks (p_block_t).
 *         Populated by task_i2c_sensors, consumed by task_block_assembler.
 */
extern QueueHandle_t g_ppg_block_queue;

/**
 * @brief  Queue handle for outgoing T-blocks (t_block_t).
 */
extern QueueHandle_t g_temp_block_queue;

/**
 * @brief  Queue handle for outgoing S-blocks (s_block_t).
 */
extern QueueHandle_t g_scl_block_queue;

/**
 * @brief  Initialise I2C driver and configure all connected sensors.
 *         Must be called from app_main after i2c_param_config / i2c_driver_install.
 */
void task_i2c_sensors_init(void);

/**
 * @brief  FreeRTOS task entry point.
 *         Pass NULL as pvParameters.
 */
void task_i2c_sensors(void *pvParameters);
