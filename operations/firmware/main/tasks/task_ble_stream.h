#pragma once

/* ==========================================================================
 * VU-AMS Firmware — BLE streaming task
 * xTaskCreatePinnedToCore: priority 4, core 1, stack 8192
 * Author: Kai Müller
 * ========================================================================== */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief  Queue handle for incoming block pointers (void *) to transmit via BLE.
 *         Populated by task_block_assembler, drained by task_ble_stream.
 *         Best-effort: blocks dropped if queue full or client disconnected.
 */
extern QueueHandle_t g_ble_tx_queue;

/**
 * @brief  Initialise BLE stack (NimBLE), create GATT service, and start advertising.
 *         Must be called from app_main after nvs_flash_init().
 */
void task_ble_stream_init(void);

/**
 * @brief  FreeRTOS task entry point.
 *         Pass NULL as pvParameters.
 */
void task_ble_stream(void *pvParameters);

/**
 * @brief  Return the electrode distance last set by the CONFIGURE_SESSION BLE
 *         command (0x10).  Default is 30.0 cm until overridden by the host.
 *
 * @return Electrode distance in centimetres.
 */
float ble_get_electrode_distance_cm(void);
