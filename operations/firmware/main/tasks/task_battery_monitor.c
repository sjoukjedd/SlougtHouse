/* ==========================================================================
 * VU-AMS Firmware — Battery monitor task
 * xTaskCreatePinnedToCore(task_battery_monitor, "task_bat", 2048, NULL, 2, NULL, 1)
 * Priority: 2  |  Core: 1  |  Stack: 2048 bytes
 * Author: Kai Müller
 * Date:   2026-05-08
 *
 * Samples the LiPo voltage via ADC1_CH0 (GPIO1), computes SoC,
 * and posts EVT_BATTERY_LOW / EVT_BATTERY_CRIT to the system event group.
 * ========================================================================== */

#include "task_battery_monitor.h"
#include "config.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "task_bat";

/* ADC handles */
static adc_oneshot_unit_handle_t s_adc_handle   = NULL;
static adc_cali_handle_t         s_cali_handle  = NULL;

/* ADC channel corresponding to PIN_BATT_ADC (GPIO1 = ADC1_CH0) */
#define BATT_ADC_CHANNEL    ADC_CHANNEL_0
#define BATT_ADC_UNIT       ADC_UNIT_1
#define BATT_ADC_ATTEN      ADC_ATTEN_DB_12     /* Full-scale ~3.1 V */
#define BATT_SAMPLE_PERIOD_MS   5000            /* Sample every 5 seconds */
#define BATT_OVERSAMPLE         8               /* Average 8 readings */

/* Voltage divider ratio: adjust to match hardware (e.g. 2:1 gives factor 2) */
#define BATT_VDIV_FACTOR    2

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void task_battery_monitor_init(void)
{
    /* Configure ADC unit */
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = BATT_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &s_adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit: %s", esp_err_to_name(err));
        return;
    }

    /* Configure channel */
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = BATT_ADC_ATTEN,
    };
    err = adc_oneshot_config_channel(s_adc_handle, BATT_ADC_CHANNEL, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel: %s", esp_err_to_name(err));
        return;
    }

    /* Calibration (curve fitting if available, else line fitting) */
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = BATT_ADC_UNIT,
        .chan     = BATT_ADC_CHANNEL,
        .atten   = BATT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle);
#else
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id  = BATT_ADC_UNIT,
        .atten    = BATT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali_handle);
#endif
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration unavailable: %s", esp_err_to_name(err));
        s_cali_handle = NULL;
    }

    ESP_LOGI(TAG, "Battery monitor init done");
}

/* --------------------------------------------------------------------------
 * Internal: read calibrated millivolts
 * -------------------------------------------------------------------------- */

static int read_vbatt_mv(void)
{
    int raw_sum = 0;
    for (int i = 0; i < BATT_OVERSAMPLE; i++) {
        int raw = 0;
        adc_oneshot_read(s_adc_handle, BATT_ADC_CHANNEL, &raw);
        raw_sum += raw;
    }
    int raw_avg = raw_sum / BATT_OVERSAMPLE;

    int mv = 0;
    if (s_cali_handle != NULL) {
        adc_cali_raw_to_voltage(s_cali_handle, raw_avg, &mv);
    } else {
        /* Fallback: crude linear approximation for 12-bit, 3.1 V FS */
        mv = (raw_avg * 3100) / 4095;
    }

    return mv * BATT_VDIV_FACTOR;
}

/* --------------------------------------------------------------------------
 * Internal: compute SoC percentage (linear approximation)
 * -------------------------------------------------------------------------- */

static uint8_t vbatt_to_soc(int mv)
{
    if (mv >= BATT_VMAX_MV) { return 100; }
    if (mv <= BATT_VMIN_MV) { return 0;   }
    return (uint8_t)(((mv - BATT_VMIN_MV) * 100) / (BATT_VMAX_MV - BATT_VMIN_MV));
}

/* --------------------------------------------------------------------------
 * Task
 * -------------------------------------------------------------------------- */

void task_battery_monitor(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "Battery monitor task started on core %d", xPortGetCoreID());

    while (1) {
        if (s_adc_handle != NULL) {
            int mv   = read_vbatt_mv();
            uint8_t soc = vbatt_to_soc(mv);

            ESP_LOGI(TAG, "VBatt: %d mV, SoC: %u%%", mv, soc);

            if (soc <= BATT_CRITICAL_PCT) {
                ESP_LOGE(TAG, "CRITICAL battery — initiating graceful shutdown");
                /* TODO: set EVT_BATTERY_CRIT in system event group          */
                /* TODO: post CMD_SHUTDOWN to system_cmd_queue               */
            } else if (soc <= BATT_LOW_PCT) {
                ESP_LOGW(TAG, "Battery low");
                /* TODO: set EVT_BATTERY_LOW in system event group           */
            }
        }

        vTaskDelay(pdMS_TO_TICKS(BATT_SAMPLE_PERIOD_MS));
    }
}
