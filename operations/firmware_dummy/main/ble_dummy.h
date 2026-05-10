#pragma once

/* ==========================================================================
 * VU-AMS Dummy Firmware — BLE peripheral (NimBLE)
 * Author: Kai Müller
 * Date:   2026-05-09
 *
 * Exposes the same GATT service as the real VU-AMS firmware.
 * The iOS app cannot tell this apart from genuine hardware.
 * ========================================================================== */

#include <stdbool.h>
#include "data_blocks.h"

/**
 * @brief  Initialise NimBLE stack, register GATT service, start advertising.
 *         Must be called after nvs_flash_init().
 */
void ble_dummy_init(void);

/**
 * @brief  Send a pre-built block to the appropriate GATT characteristic.
 *         No-op if no client is connected or streaming is not active.
 *
 * @param block  Pointer to a fully populated vuams_block_t.
 */
void ble_dummy_send_block(const vuams_block_t *block);

/**
 * @brief  Return true if a BLE client is connected and has started streaming
 *         (sent 0x01 on the Control characteristic).
 */
bool ble_dummy_is_streaming(void);
