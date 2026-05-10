/* ==========================================================================
 * VU-AMS Firmware — USB-C presence guard (Type BF safety interlock)
 * xTaskCreatePinnedToCore(task_usb_guard, "task_usbg", 2048, NULL, 2, NULL, 0)
 * Priority: 2  |  Core: 0  |  Stack: 2048 bytes
 * Author: Kai Müller
 * Date:   2026-05-09
 *
 * Polls PIN_USB_DETECT (GPIO20) every 100 ms.
 * Accepts a state change only after 3 consecutive identical readings
 * (debounce window = 300 ms), which is well within the human-reaction time
 * required for a Type BF alert but fast enough to be clinically useful.
 *
 * On confirmed connect  → ESP_LOGW + registered callback(true)
 * On confirmed disconnect → ESP_LOGI + registered callback(false)
 * ========================================================================== */

#include "task_usb_guard.h"
#include "config.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>
#include <stdatomic.h>

static const char *TAG = "usb_guard";

/* Number of identical consecutive readings required before accepting a
 * state change.  At 100 ms poll interval this gives a 300 ms debounce. */
#define DEBOUNCE_COUNT      3
#define POLL_INTERVAL_MS    100

/* Atomic so usb_guard_is_connected() is safe from any calling task. */
static atomic_bool s_usb_connected = ATOMIC_VAR_INIT(false);

/* Single callback slot.  NULL if not registered. */
static void (*s_callback)(bool connected) = NULL;

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void task_usb_guard_init(void)
{
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << PIN_USB_DETECT),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,   /* 100 kΩ external + internal pull-down */
        .intr_type    = GPIO_INTR_DISABLE,      /* Polled — no interrupt needed          */
    };
    esp_err_t err = gpio_config(&io_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed for PIN_USB_DETECT (GPIO%d): %s",
                 PIN_USB_DETECT, esp_err_to_name(err));
        return;
    }

    /* Read initial state immediately so usb_guard_is_connected() is valid
     * the moment init returns, before the polling task has run. */
    bool initial = (gpio_get_level(PIN_USB_DETECT) == 1);
    atomic_store(&s_usb_connected, initial);

    ESP_LOGI(TAG, "USB guard init: GPIO%d, initial state: %s",
             PIN_USB_DETECT, initial ? "CONNECTED" : "disconnected");
}

void usb_guard_register_callback(void (*cb)(bool connected))
{
    s_callback = cb;
}

bool usb_guard_is_connected(void)
{
    return atomic_load(&s_usb_connected);
}

/* --------------------------------------------------------------------------
 * Task
 * -------------------------------------------------------------------------- */

void task_usb_guard(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "USB guard task started on core %d", xPortGetCoreID());

    /* Seed the debounce counter from the current confirmed state so we don't
     * fire a spurious callback immediately on startup. */
    bool     confirmed_state = atomic_load(&s_usb_connected);
    int      debounce_hits   = 0;           /* consecutive readings matching candidate */
    bool     candidate       = confirmed_state;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));

        bool raw = (gpio_get_level(PIN_USB_DETECT) == 1);

        if (raw == candidate) {
            /* Reading is consistent with current candidate */
            if (candidate != confirmed_state) {
                /* We are working toward a state change */
                debounce_hits++;
                if (debounce_hits >= DEBOUNCE_COUNT) {
                    /* Confirmed transition */
                    confirmed_state = candidate;
                    atomic_store(&s_usb_connected, confirmed_state);

                    if (confirmed_state) {
                        ESP_LOGW(TAG,
                            "USB-C connected — Type BF interlock ACTIVE: "
                            "recording is blocked (IEC 60601-1)");
                    } else {
                        ESP_LOGI(TAG,
                            "USB-C disconnected — Type BF interlock RELEASED: "
                            "recording permitted");
                    }

                    if (s_callback != NULL) {
                        s_callback(confirmed_state);
                    }

                    debounce_hits = 0;
                }
            }
            /* else: stable, nothing to do */
        } else {
            /* Reading changed — start a fresh debounce sequence toward raw */
            candidate     = raw;
            debounce_hits = 1;
        }
    }
}
