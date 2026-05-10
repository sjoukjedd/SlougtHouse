/* ==========================================================================
 * VU-AMS Dummy Firmware — BLE peripheral implementation (NimBLE)
 * Author: Kai Müller
 * Date:   2026-05-09
 *
 * Implements the identical GATT service as task_ble_stream.c in the real
 * firmware.  UUIDs, byte order, block encoding, notify pattern — all the
 * same.  The iOS app cannot distinguish this device from live hardware.
 *
 * UUID byte-order note:
 *   BLE_UUID128_DECLARE() takes bytes LSB first (reversed from the
 *   human-readable string representation).
 *   "A5D5B001-5A5A-4B4B-8888-1A2B3C4D5E6F" becomes:
 *   6F 5E 4D 3C 2B 1A 88 88  4B 4B 5A 5A 01 B0 D5 A5
 * ========================================================================== */

#include "ble_dummy.h"
#include "dummy_config.h"
#include "data_blocks.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <string.h>
#include <stdbool.h>

static const char *TAG = "ble_dummy";

/* --------------------------------------------------------------------------
 * State
 * -------------------------------------------------------------------------- */

static uint16_t s_conn_handle  = 0xFFFF;  /* 0xFFFF = no client */
static bool     s_streaming    = false;   /* true after 0x01 on Control chr */

/* Mutex protecting s_conn_handle / s_streaming — written from GAP callback
 * (NimBLE host task), read from the signal generation task. */
static SemaphoreHandle_t s_state_mutex = NULL;

/* GATT value handles */
static uint16_t s_hdl_a      = 0;
static uint16_t s_hdl_i      = 0;
static uint16_t s_hdl_m      = 0;
static uint16_t s_hdl_p      = 0;
static uint16_t s_hdl_s      = 0;
static uint16_t s_hdl_t      = 0;
static uint16_t s_hdl_status = 0;
static uint16_t s_hdl_ctrl   = 0;

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */
static void start_advertising(void);
static void send_status(uint8_t status_byte);

/* --------------------------------------------------------------------------
 * GATT access callback — notify-only characteristics return UNLIKELY.
 * Control characteristic handles WRITE (start/stop command).
 * Status characteristic handles READ.
 * -------------------------------------------------------------------------- */
static int gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR && attr_handle == s_hdl_ctrl) {
        /* Control write: 1 byte, 0x01 = start, 0x02 = stop */
        if (OS_MBUF_PKTLEN(ctxt->om) < 1) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        uint8_t cmd = 0;
        ble_hs_mbuf_to_flat(ctxt->om, &cmd, 1, NULL);

        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        if (cmd == 0x01) {
            s_streaming = true;
            ESP_LOGI(TAG, "Streaming START command received");
        } else if (cmd == 0x02) {
            s_streaming = false;
            ESP_LOGI(TAG, "Streaming STOP command received");
        } else {
            ESP_LOGW(TAG, "Unknown control command: 0x%02X", cmd);
        }
        bool now_streaming = s_streaming;
        xSemaphoreGive(s_state_mutex);

        /* Send status notification immediately */
        send_status(now_streaming ? 0x01 : 0x00);
        return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR && attr_handle == s_hdl_status) {
        /* Status read: return current streaming state */
        uint8_t val = 0;
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        val = s_streaming ? 0x01 : 0x00;
        xSemaphoreGive(s_state_mutex);
        int rc = os_mbuf_append(ctxt->om, &val, 1);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    /* All notify-only characteristics: reads/writes not supported */
    return BLE_ATT_ERR_UNLIKELY;
}

/* --------------------------------------------------------------------------
 * GATT service definition
 *
 * UUID byte layout (LSB first for BLE_UUID128_DECLARE):
 *   "A5D5BXXX-5A5A-4B4B-8888-1A2B3C4D5E6F"
 *   6F 5E 4D 3C 2B 1A 88 88  4B 4B 5A 5A XX XX D5 A5
 *   (where XXXX is the 16-bit field from the UUID string, LE)
 * -------------------------------------------------------------------------- */

/* Helper macro: expand the 2-byte varying field (LSB, MSB) in the UUID */
#define VUAMS_UUID128(b1, b0)                                       \
    BLE_UUID128_DECLARE(                                            \
        0x6F, 0x5E, 0x4D, 0x3C, 0x2B, 0x1A, 0x88, 0x88,           \
        0x4B, 0x4B, 0x5A, 0x5A, (b0), (b1), 0xD5, 0xA5)

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = VUAMS_UUID128(0xB0, 0x01),   /* service: B001 */
        .characteristics = (struct ble_gatt_chr_def[]) {
            /* A-block: ECG/ICG raw */
            {
                .uuid       = VUAMS_UUID128(0xB0, 0x02),
                .access_cb  = gatt_access_cb,
                .val_handle = &s_hdl_a,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            /* I-block: ICG derived haemodynamics */
            {
                .uuid       = VUAMS_UUID128(0xB0, 0x03),
                .access_cb  = gatt_access_cb,
                .val_handle = &s_hdl_i,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            /* M-block: IMU */
            {
                .uuid       = VUAMS_UUID128(0xB0, 0x04),
                .access_cb  = gatt_access_cb,
                .val_handle = &s_hdl_m,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            /* P-block: PPG */
            {
                .uuid       = VUAMS_UUID128(0xB0, 0x05),
                .access_cb  = gatt_access_cb,
                .val_handle = &s_hdl_p,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            /* S-block: SCL/EDA */
            {
                .uuid       = VUAMS_UUID128(0xB0, 0x06),
                .access_cb  = gatt_access_cb,
                .val_handle = &s_hdl_s,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            /* T-block: Temperature */
            {
                .uuid       = VUAMS_UUID128(0xB0, 0x07),
                .access_cb  = gatt_access_cb,
                .val_handle = &s_hdl_t,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            /* Status: 0x00=idle, 0x01=recording */
            {
                .uuid       = VUAMS_UUID128(0xB0, 0x09),
                .access_cb  = gatt_access_cb,
                .val_handle = &s_hdl_status,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            /* Control: write 0x01=start, 0x02=stop */
            {
                .uuid       = VUAMS_UUID128(0xB0, 0x0A),
                .access_cb  = gatt_access_cb,
                .val_handle = &s_hdl_ctrl,
                .flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { 0 } /* terminator */
        },
    },
    { 0 } /* terminator */
};

/* --------------------------------------------------------------------------
 * GAP event handler
 * -------------------------------------------------------------------------- */
static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                xSemaphoreTake(s_state_mutex, portMAX_DELAY);
                s_conn_handle = event->connect.conn_handle;
                xSemaphoreGive(s_state_mutex);
                ESP_LOGI(TAG, "Client connected, handle=%d", s_conn_handle);
            } else {
                ESP_LOGW(TAG, "Connection failed, status=%d", event->connect.status);
                start_advertising();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Client disconnected, reason=%d",
                     event->disconnect.reason);
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            s_conn_handle = 0xFFFF;
            s_streaming   = false;
            xSemaphoreGive(s_state_mutex);
            start_advertising();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "Subscribe: attr=%d notify=%d",
                     event->subscribe.attr_handle,
                     event->subscribe.cur_notify);
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU updated: conn=%d mtu=%d",
                     event->mtu.conn_handle, event->mtu.value);
            break;

        default:
            break;
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * NimBLE host callbacks
 * -------------------------------------------------------------------------- */
static void on_sync(void)
{
    ESP_LOGI(TAG, "NimBLE host sync — starting advertising");
    start_advertising();
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "NimBLE host reset, reason=%d", reason);
}

static void nimble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* --------------------------------------------------------------------------
 * Advertising
 * -------------------------------------------------------------------------- */
static void start_advertising(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields  adv_fields;
    const char               *name = "VU-AMS";

    memset(&adv_fields, 0, sizeof(adv_fields));
    adv_fields.flags                 = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.name                  = (const uint8_t *)name;
    adv_fields.name_len              = (uint8_t)strlen(name);
    adv_fields.name_is_complete      = 1;
    adv_fields.tx_pwr_lvl_is_present = 1;
    adv_fields.tx_pwr_lvl            = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    int rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields: %d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min  = BLE_GAP_ADV_ITVL_MS(100);
    adv_params.itvl_max  = BLE_GAP_ADV_ITVL_MS(200);

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_handler, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "ble_gap_adv_start: %d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising as \"VU-AMS\"");
    }
}

/* --------------------------------------------------------------------------
 * Internal: raw notify helper
 * -------------------------------------------------------------------------- */
static void notify_raw(uint16_t chr_handle, const void *data, size_t len)
{
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    uint16_t conn = s_conn_handle;
    xSemaphoreGive(s_state_mutex);

    if (conn == 0xFFFF || chr_handle == 0) {
        return;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        ESP_LOGW(TAG, "notify_raw: mbuf alloc failed (len=%u)", (unsigned)len);
        return;
    }

    int rc = ble_gattc_notify_custom(conn, chr_handle, om);
    if (rc != 0 && rc != BLE_HS_ENOTCONN) {
        ESP_LOGW(TAG, "ble_gattc_notify_custom: %d", rc);
    }
}

/* --------------------------------------------------------------------------
 * Send status notification
 * -------------------------------------------------------------------------- */
static void send_status(uint8_t status_byte)
{
    notify_raw(s_hdl_status, &status_byte, 1);
}

/* --------------------------------------------------------------------------
 * Public API — send a block
 * -------------------------------------------------------------------------- */
void ble_dummy_send_block(const vuams_block_t *block)
{
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    bool streaming = s_streaming;
    xSemaphoreGive(s_state_mutex);

    if (!streaming) {
        return;
    }

    switch (block->type) {
        case BLOCK_TYPE_A:
            notify_raw(s_hdl_a, &block->a, sizeof(a_block_t));
            break;
        case BLOCK_TYPE_I:
            notify_raw(s_hdl_i, &block->i, sizeof(i_block_t));
            break;
        case BLOCK_TYPE_M:
            notify_raw(s_hdl_m, &block->m, sizeof(m_block_t));
            break;
        case BLOCK_TYPE_P:
            notify_raw(s_hdl_p, &block->p, sizeof(p_block_t));
            break;
        case BLOCK_TYPE_S:
            notify_raw(s_hdl_s, &block->s, sizeof(s_block_t));
            break;
        case BLOCK_TYPE_T:
            notify_raw(s_hdl_t, &block->t, sizeof(t_block_t));
            break;
        default:
            ESP_LOGW(TAG, "ble_dummy_send_block: unknown type 0x%02X", block->type);
            break;
    }
}

bool ble_dummy_is_streaming(void)
{
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    bool v = s_streaming;
    xSemaphoreGive(s_state_mutex);
    return v;
}

/* --------------------------------------------------------------------------
 * Initialise
 * -------------------------------------------------------------------------- */
void ble_dummy_init(void)
{
    s_state_mutex = xSemaphoreCreateMutex();
    if (s_state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        return;
    }

    nimble_port_init();

    ble_hs_cfg.sync_cb  = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    /* Set device name visible in GAP */
    int rc = ble_svc_gap_device_name_set("VU-AMS");
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_svc_gap_device_name_set: %d", rc);
    }

    rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg: %d", rc);
        return;
    }

    rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs: %d", rc);
        return;
    }

    /* NimBLE host runs on its own FreeRTOS task (stack 4096, priority 5) */
    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(TAG, "BLE init done");
}
