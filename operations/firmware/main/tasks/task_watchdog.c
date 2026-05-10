/* ==========================================================================
 * VU-AMS Firmware — Watchdog task
 * xTaskCreatePinnedToCore(task_watchdog, "task_wdt", 2048, NULL, 3, NULL, 1)
 * Priority: 3  |  Core: 1  |  Stack: 2048 bytes
 * Author: Kai Müller
 * Date:   2026-05-08
 *
 * Periodically checks system health indicators:
 *  - Resets (pets) the ESP Task Watchdog Timer.
 *  - Monitors system event group for fault flags.
 *  - Logs free heap and PSRAM statistics.
 *  - TODO: check queue high-water marks and set fault event if stuck.
 * ========================================================================== */

#include "task_watchdog.h"
#include "config.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "task_wdt";

#define WDT_TIMEOUT_S           10      /* Reset if this task doesn't pet within 10 s */
#define HEALTH_LOG_PERIOD_MS    10000   /* Log heap stats every 10 seconds             */

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void task_watchdog_init(void)
{
    /* Configure the Task Watchdog Timer */
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms     = WDT_TIMEOUT_S * 1000,
        .idle_core_mask = 0,    /* Do not watch idle tasks */
        .trigger_panic  = true,
    };

    esp_err_t err = esp_task_wdt_reconfigure(&wdt_cfg);
    if (err == ESP_ERR_NOT_FOUND) {
        /* TWDT not yet initialised — init it first */
        err = esp_task_wdt_init(&wdt_cfg);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TWDT init failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "TWDT configured, timeout=%d s", WDT_TIMEOUT_S);
    }
}

/* --------------------------------------------------------------------------
 * Task
 * -------------------------------------------------------------------------- */

void task_watchdog(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "Watchdog task started on core %d", xPortGetCoreID());

    /* Register this task with the TWDT */
    esp_err_t err = esp_task_wdt_add(NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_task_wdt_add failed: %s", esp_err_to_name(err));
    }

    TickType_t last_wake    = xTaskGetTickCount();
    uint32_t   log_counter  = 0;

    while (1) {
        /* Pet the watchdog */
        esp_task_wdt_reset();

        /* Periodic health logging */
        if ((log_counter % (HEALTH_LOG_PERIOD_MS / 1000)) == 0) {
            size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            size_t free_psram    = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            ESP_LOGI(TAG, "Heap — internal: %u B free, PSRAM: %u B free",
                     (unsigned)free_internal, (unsigned)free_psram);

            /* TODO: check system event group for EVT_SYSTEM_FAULT            */
            /* TODO: check EVT_SD_ERROR — attempt SD remount                  */
            /* TODO: dump task high-water marks via uxTaskGetStackHighWaterMark */
        }

        log_counter++;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000)); /* Pet every 1 s */
    }
}
