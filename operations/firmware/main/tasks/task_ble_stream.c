/* ==========================================================================
 * VU-AMS Firmware — BLE streaming task
 * xTaskCreatePinnedToCore(task_ble_stream, "task_ble", 8192, NULL, 4, NULL, 1)
 * Priority: 4  |  Core: 1  |  Stack: 8192 bytes
 * Author: Kai Müller
 * Date:   2026-05-08
 *
 * Implements the VUAMS BLE GATT service.
 * One notify characteristic per block type (A/I/M/P/S/T/Y).
 * Uses ESP-IDF NimBLE stack (CONFIG_BT_NIMBLE_ENABLED=y).
 *
 * UUIDs as defined in config.h — must match Chen's iOS stack exactly.
 * ========================================================================== */

#include "task_ble_stream.h"
#include "config.h"
#include "pool_alloc.h"
#include "data_blocks.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdlib.h>

/* NimBLE headers — available when CONFIG_BT_NIMBLE_ENABLED=y */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "task_ble";

QueueHandle_t g_ble_tx_queue = NULL;

/* Connection handle — 0xFFFF = no client connected */
static uint16_t s_conn_handle = 0xFFFF;

/* --------------------------------------------------------------------------
 * Session configuration — updated via BLE command 0x10 CONFIGURE_SESSION
 * -------------------------------------------------------------------------- */
#define BLE_CMD_CONFIGURE_SESSION  0x10u

/* Default electrode distance matches the KUBICEK_L_CM compile-time constant
 * previously used in task_block_assembler.c.  Overwritten at session start
 * by the iOS app sending command 0x10 with the subject's actual measurement. */
static _Atomic float s_electrode_distance_cm = 30.0f;

float ble_get_electrode_distance_cm(void)
{
    return s_electrode_distance_cm;
}

/* GATT characteristic value handles (populated after service registration) */
static uint16_t s_chr_handle_a = 0;
static uint16_t s_chr_handle_i = 0;
static uint16_t s_chr_handle_m = 0;
static uint16_t s_chr_handle_p = 0;
static uint16_t s_chr_handle_s = 0;
static uint16_t s_chr_handle_t = 0;
static uint16_t s_chr_handle_y = 0;
static uint16_t s_chr_handle_r    = 0;
static uint16_t s_chr_handle_ctrl = 0;  /* writable control characteristic */

/* --------------------------------------------------------------------------
 * NimBLE GATT service definition
 * -------------------------------------------------------------------------- */

/* Minimal GATT access callback — all notify-only characteristics */
static int gatt_chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    /* Notify-only characteristics need no read/write handler */
    return BLE_ATT_ERR_UNLIKELY;
}

/* --------------------------------------------------------------------------
 * Control characteristic write callback
 * Handles writable commands sent by the iOS app.
 *
 * Command 0x10 CONFIGURE_SESSION
 *   Payload: 5 bytes — [0] = command byte (0x10),
 *                      [1..4] = electrode_distance_cm as little-endian float32
 * -------------------------------------------------------------------------- */
static int gatt_ctrl_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len < 1) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint8_t payload[16];
    if (len > sizeof(payload)) { len = sizeof(payload); }
    os_mbuf_copydata(ctxt->om, 0, len, payload);

    uint8_t cmd = payload[0];
    switch (cmd) {
        case BLE_CMD_CONFIGURE_SESSION:
            /* Expect exactly 5 bytes: cmd byte + 4-byte LE float */
            if (len < 5) {
                ESP_LOGW(TAG, "CMD 0x10: payload too short (%u B)", len);
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
            float dist_cm;
            memcpy(&dist_cm, &payload[1], sizeof(float));  /* LE host = native LE */
            if (dist_cm < 1.0f || dist_cm > 100.0f) {
                ESP_LOGW(TAG, "CMD 0x10: electrode_distance_cm=%.1f out of range",
                         (double)dist_cm);
                return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
            }
            s_electrode_distance_cm = dist_cm;
            ESP_LOGI(TAG, "CONFIGURE_SESSION: electrode_distance_cm=%.1f cm",
                     (double)s_electrode_distance_cm);
            break;

        default:
            ESP_LOGW(TAG, "Unknown BLE command 0x%02X", cmd);
            return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }

    return 0;
}

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = BLE_UUID128_DECLARE(
            0x6F, 0x5E, 0x4D, 0x3C, 0x2B, 0x1A, 0x88, 0x88,
            0x4B, 0x4B, 0x5A, 0x5A, 0x01, 0xB0, 0xD5, 0xA5),
        .characteristics = (struct ble_gatt_chr_def[]) {
            /* A-block: ECG */
            {
                .uuid       = BLE_UUID128_DECLARE(
                    0x6F, 0x5E, 0x4D, 0x3C, 0x2B, 0x1A, 0x88, 0x88,
                    0x4B, 0x4B, 0x5A, 0x5A, 0x02, 0xB0, 0xD5, 0xA5),
                .access_cb  = gatt_chr_access_cb,
                .val_handle = &s_chr_handle_a,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            /* I-block: ICG derived */
            {
                .uuid       = BLE_UUID128_DECLARE(
                    0x6F, 0x5E, 0x4D, 0x3C, 0x2B, 0x1A, 0x88, 0x88,
                    0x4B, 0x4B, 0x5A, 0x5A, 0x03, 0xB0, 0xD5, 0xA5),
                .access_cb  = gatt_chr_access_cb,
                .val_handle = &s_chr_handle_i,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            /* M-block: IMU */
            {
                .uuid       = BLE_UUID128_DECLARE(
                    0x6F, 0x5E, 0x4D, 0x3C, 0x2B, 0x1A, 0x88, 0x88,
                    0x4B, 0x4B, 0x5A, 0x5A, 0x04, 0xB0, 0xD5, 0xA5),
                .access_cb  = gatt_chr_access_cb,
                .val_handle = &s_chr_handle_m,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            /* P-block: PPG */
            {
                .uuid       = BLE_UUID128_DECLARE(
                    0x6F, 0x5E, 0x4D, 0x3C, 0x2B, 0x1A, 0x88, 0x88,
                    0x4B, 0x4B, 0x5A, 0x5A, 0x05, 0xB0, 0xD5, 0xA5),
                .access_cb  = gatt_chr_access_cb,
                .val_handle = &s_chr_handle_p,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            /* S-block: SCL */
            {
                .uuid       = BLE_UUID128_DECLARE(
                    0x6F, 0x5E, 0x4D, 0x3C, 0x2B, 0x1A, 0x88, 0x88,
                    0x4B, 0x4B, 0x5A, 0x5A, 0x06, 0xB0, 0xD5, 0xA5),
                .access_cb  = gatt_chr_access_cb,
                .val_handle = &s_chr_handle_s,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            /* T-block: Temperature */
            {
                .uuid       = BLE_UUID128_DECLARE(
                    0x6F, 0x5E, 0x4D, 0x3C, 0x2B, 0x1A, 0x88, 0x88,
                    0x4B, 0x4B, 0x5A, 0x5A, 0x07, 0xB0, 0xD5, 0xA5),
                .access_cb  = gatt_chr_access_cb,
                .val_handle = &s_chr_handle_t,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            /* Y-block: HRV summary (RMSSD, HR, RRI) — one per 30-beat window */
            {
                .uuid       = BLE_UUID128_DECLARE(
                    0x6F, 0x5E, 0x4D, 0x3C, 0x2B, 0x1A, 0x88, 0x88,
                    0x4B, 0x4B, 0x5A, 0x5A, 0x08, 0xB0, 0xD5, 0xA5),
                .access_cb  = gatt_chr_access_cb,
                .val_handle = &s_chr_handle_y,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            /* R-block: Respiratory rate */
            {
                .uuid       = BLE_UUID128_DECLARE(
                    0x6F, 0x5E, 0x4D, 0x3C, 0x2B, 0x1A, 0x88, 0x88,
                    0x4B, 0x4B, 0x5A, 0x5A, 0x0E, 0xB0, 0xD5, 0xA5),
                .access_cb  = gatt_chr_access_cb,
                .val_handle = &s_chr_handle_r,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            /* Control point — write-only; receives session configuration commands.
             * UUID byte [12] = 0x0F, matching the VUAMS interface register §4.3. */
            {
                .uuid       = BLE_UUID128_DECLARE(
                    0x6F, 0x5E, 0x4D, 0x3C, 0x2B, 0x1A, 0x88, 0x88,
                    0x4B, 0x4B, 0x5A, 0x5A, 0x0F, 0xB0, 0xD5, 0xA5),
                .access_cb  = gatt_ctrl_access_cb,
                .val_handle = &s_chr_handle_ctrl,
                .flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { 0 } /* terminator */
        },
    },
    { 0 } /* terminator */
};

/* --------------------------------------------------------------------------
 * NimBLE GAP event handler
 * -------------------------------------------------------------------------- */

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "Client connected, handle=%d", event->connect.conn_handle);
                s_conn_handle = event->connect.conn_handle;
                /* TODO: set EVT_BLE_CONNECTED in system event group */
            } else {
                ESP_LOGW(TAG, "Connection failed, status=%d", event->connect.status);
                s_conn_handle = 0xFFFF;
                /* TODO: restart advertising */
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Client disconnected, reason=%d", event->disconnect.reason);
            s_conn_handle = 0xFFFF;
            /* TODO: clear EVT_BLE_CONNECTED */
            start_advertising();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "Subscribe event: attr_handle=%d, notify=%d",
                     event->subscribe.attr_handle, event->subscribe.cur_notify);
            /* TODO: set/clear EVT_BLE_STREAMING per characteristic */
            break;

        default:
            break;
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * Start advertising
 * -------------------------------------------------------------------------- */

static void start_advertising(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields  adv_fields;
    const char *name = "VU-AMS";

    memset(&adv_fields, 0, sizeof(adv_fields));
    adv_fields.flags                 = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.name                  = (const uint8_t *)name;
    adv_fields.name_len              = strlen(name);
    adv_fields.name_is_complete      = 1;
    adv_fields.tx_pwr_lvl_is_present = 1;
    adv_fields.tx_pwr_lvl            = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    esp_err_t err = ble_gap_adv_set_fields(&adv_fields);
    if (err != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields: %d", err);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode  = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode  = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min   = BLE_GAP_ADV_ITVL_MS(100);
    adv_params.itvl_max   = BLE_GAP_ADV_ITVL_MS(200);

    err = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                             &adv_params, gap_event_handler, NULL);
    if (err != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start: %d", err);
    } else {
        ESP_LOGI(TAG, "Advertising started");
    }
}

/* --------------------------------------------------------------------------
 * NimBLE host task (runs on its own FreeRTOS task created by nimble_port)
 * -------------------------------------------------------------------------- */

static void nimble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void on_sync(void)
{
    ESP_LOGI(TAG, "NimBLE host sync");
    start_advertising();
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "NimBLE host reset, reason=%d", reason);
}

/* --------------------------------------------------------------------------
 * Internal: notify a block on its characteristic
 * -------------------------------------------------------------------------- */

static void notify_block(uint16_t chr_handle, const void *data, size_t len)
{
    if (s_conn_handle == 0xFFFF || chr_handle == 0) {
        return;
    }
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        ESP_LOGW(TAG, "notify: mbuf alloc failed");
        return;
    }
    int rc = ble_gattc_notify_custom(s_conn_handle, chr_handle, om);
    if (rc != 0) {
        ESP_LOGW(TAG, "notify failed: %d", rc);
    }
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void task_ble_stream_init(void)
{
    g_ble_tx_queue = xQueueCreate(BLE_BLOCK_QUEUE_DEPTH, sizeof(void *));
    if (g_ble_tx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create ble_tx_queue");
        return;
    }

    nimble_port_init();
    ble_hs_cfg.sync_cb  = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc != 0) { ESP_LOGE(TAG, "ble_gatts_count_cfg: %d", rc); return; }

    rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0) { ESP_LOGE(TAG, "ble_gatts_add_svcs: %d", rc); return; }

    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(TAG, "BLE init done");
}

/* --------------------------------------------------------------------------
 * Task
 * -------------------------------------------------------------------------- */

void task_ble_stream(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "BLE stream task started on core %d", xPortGetCoreID());

    while (1) {
        void *block_ptr = NULL;

        if (xQueueReceive(g_ble_tx_queue, &block_ptr, pdMS_TO_TICKS(20)) != pdTRUE) {
            continue;
        }
        if (block_ptr == NULL) { continue; }

        uint8_t type = *(uint8_t *)block_ptr;

        switch (type) {
            case BLOCK_TYPE_A:
                /* A-blocks are large (2016 B) — fragment or use L2CAP CoC */
                /* TODO: split into MTU-sized chunks with sequence numbers  */
                notify_block(s_chr_handle_a, block_ptr, sizeof(a_block_t));
                break;
            case BLOCK_TYPE_I:
                notify_block(s_chr_handle_i, block_ptr, sizeof(i_block_t));
                break;
            case BLOCK_TYPE_M:
                notify_block(s_chr_handle_m, block_ptr, sizeof(m_block_t));
                break;
            case BLOCK_TYPE_P:
                notify_block(s_chr_handle_p, block_ptr, sizeof(p_block_t));
                break;
            case BLOCK_TYPE_S:
                notify_block(s_chr_handle_s, block_ptr, sizeof(s_block_t));
                break;
            case BLOCK_TYPE_T:
                notify_block(s_chr_handle_t, block_ptr, sizeof(t_block_t));
                break;
            case BLOCK_TYPE_Y:
                notify_block(s_chr_handle_y, block_ptr, sizeof(y_block_t));
                break;
            case BLOCK_TYPE_R:
                notify_block(s_chr_handle_r, block_ptr, sizeof(r_block_t));
                break;
            default:
                ESP_LOGW(TAG, "Unknown block type 0x%02X", type);
                break;
        }

        pool_free(block_ptr);
    }
}
