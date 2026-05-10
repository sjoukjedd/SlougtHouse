#pragma once

/* ==========================================================================
 * VU-AMS Firmware — Battery monitor task
 * xTaskCreatePinnedToCore: priority 2, core 1, stack 2048
 * Author: Kai Müller
 * ========================================================================== */

/**
 * @brief  Configure ADC1 channel for PIN_BATT_ADC.
 *         Must be called from app_main.
 */
void task_battery_monitor_init(void);

/**
 * @brief  FreeRTOS task entry point.
 *         Pass NULL as pvParameters.
 */
void task_battery_monitor(void *pvParameters);
