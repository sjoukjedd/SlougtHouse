#pragma once

/* ==========================================================================
 * VU-AMS Firmware — Watchdog task
 * xTaskCreatePinnedToCore: priority 3, core 1, stack 2048
 * Author: Kai Müller
 * ========================================================================== */

/**
 * @brief  Initialise the ESP Task Watchdog Timer (TWDT) and register all
 *         critical tasks for monitoring.
 *         Must be called from app_main after tasks are created.
 */
void task_watchdog_init(void);

/**
 * @brief  FreeRTOS task entry point.
 *         Pass NULL as pvParameters.
 */
void task_watchdog(void *pvParameters);
