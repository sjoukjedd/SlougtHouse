#pragma once

/* ==========================================================================
 * VU-AMS Firmware — USB-C presence guard (Type BF safety interlock)
 * Author: Kai Müller
 * Date:   2026-05-09
 *
 * Polls PIN_USB_DETECT every 100 ms.  Debounces over 3 consecutive identical
 * readings before accepting a state change.  Fires registered callbacks on
 * every confirmed connect/disconnect event.
 *
 * IEC 60601-1 Type BF requirement: recording (SD write + BLE stream) MUST be
 * blocked whenever USB-C VBUS is present.  Use usb_guard_is_connected() as
 * the gate in all recording-path tasks.
 * ========================================================================== */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Initialise GPIO and internal state.
 * Must be called once from app_main before starting any tasks.
 * -------------------------------------------------------------------------- */
void task_usb_guard_init(void);

/* --------------------------------------------------------------------------
 * FreeRTOS task entry point.
 * xTaskCreatePinnedToCore(task_usb_guard, "task_usbg", 2048, NULL, 2, NULL, 0)
 * -------------------------------------------------------------------------- */
void task_usb_guard(void *pvParameters);

/* --------------------------------------------------------------------------
 * Returns the current debounced USB-C connection state.
 * Safe to call from any task at any time (atomic read).
 * -------------------------------------------------------------------------- */
bool usb_guard_is_connected(void);

/* --------------------------------------------------------------------------
 * Register a callback invoked on every confirmed state change.
 * cb(true)  — USB-C just connected
 * cb(false) — USB-C just disconnected
 *
 * Only one callback slot is supported.  Call before starting task_usb_guard.
 * The callback is invoked from the task_usb_guard task context.
 * -------------------------------------------------------------------------- */
void usb_guard_register_callback(void (*cb)(bool connected));

#ifdef __cplusplus
}
#endif
