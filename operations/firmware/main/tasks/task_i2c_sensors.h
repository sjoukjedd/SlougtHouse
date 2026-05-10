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
 * @brief  Queue handle for outgoing B-blocks (b_block_t).
 *         Populated by task_i2c_sensors at 25 Hz, consumed by task_block_assembler.
 */
extern QueueHandle_t g_baro_block_queue;

/**
 * @brief  Initialise I2C driver and configure all connected sensors.
 *         Must be called from app_main after i2c_param_config / i2c_driver_install.
 */
void task_i2c_sensors_init(void);

/**
 * @brief  Initialise BMP390: set normal mode, 25 Hz ODR, x4 oversampling.
 *         Called internally from task_i2c_sensors_init.
 * @return ESP_OK on success.
 */
esp_err_t bmp390_init(void);

/**
 * @brief  Read one pressure+temperature sample from the BMP390.
 *         Applies integer compensation per BMP390 datasheet Section 8.5.
 * @param[out] pressure_pa   Compensated pressure in Pa.
 * @param[out] temp_c        Compensated temperature in °C.
 * @return ESP_OK on success.
 */
esp_err_t bmp390_read(float *pressure_pa, float *temp_c);

/**
 * @brief  FreeRTOS task entry point.
 *         Pass NULL as pvParameters.
 */
void task_i2c_sensors(void *pvParameters);
