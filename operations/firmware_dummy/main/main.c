/* ==========================================================================
 * VU-AMS Dummy Firmware — app_main
 * Target: ESP32-S3-DevKitC-1
 * Author: Kai Müller
 * Date:   2026-05-09
 *
 * Generates synthetic physiological signals and streams them over BLE
 * using exactly the same GATT service as the real VU-AMS firmware.
 *
 * Timer architecture
 * ------------------
 * We use four esp_timer periodic timers to generate signals at the correct
 * sample rates, then pack them into blocks and hand them to ble_dummy.
 *
 *  Timer A  — 1000 Hz  — ECG (a_block_t, 250-sample batches every 250 ms)
 *                         also drives ICG (same rate, same block)
 *  Timer M  — 100 Hz   — IMU (m_block_t, 10-sample batches every 100 ms)
 *  Timer P  — 100 Hz   — PPG (p_block_t, one entry per tick)
 *  Timer S  — 10 Hz    — SCL (s_block_t, one entry per tick)
 *  Timer T  — 0.25 Hz  — Temperature (t_block_t, once every 4 s)
 *  Timer I  — same as heartbeat (~1.13 Hz) — I-block (derived haemodynamics)
 *
 * A-block construction:
 *   250 samples @ 1000 Hz = 250 ms window.
 *   The timer fires every 1 ms (1000 Hz). We accumulate 250 ECG+ICG samples
 *   in a staging buffer, then on the 250th sample we submit the complete
 *   a_block_t to BLE.
 *
 * All timing is wall-clock driven: each callback reads esp_timer_get_time()
 * and passes it to the synth_* functions so waveforms are phase-coherent
 * across callbacks even if a timer fires slightly late.
 * ========================================================================== */

#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "dummy_config.h"
#include "data_blocks.h"
#include "synth_signals.h"
#include "ble_dummy.h"

static const char *TAG = "main";

/* --------------------------------------------------------------------------
 * Block queue — timer callbacks enqueue pointers to heap-allocated blocks;
 * the tx task dequeues and sends them via BLE, then frees the memory.
 * -------------------------------------------------------------------------- */
#define BLOCK_QUEUE_DEPTH   32

static QueueHandle_t s_block_queue = NULL;

/* --------------------------------------------------------------------------
 * A-block staging — accumulate 250 ECG+ICG samples before submitting
 * -------------------------------------------------------------------------- */
#define A_BLOCK_SAMPLES     250

static a_block_t  s_a_staging;
static int        s_a_count   = 0;
static uint64_t   s_a_ts_us   = 0;    /* timestamp of first sample in this batch */

/* --------------------------------------------------------------------------
 * M-block staging — accumulate 10 IMU samples before submitting
 * -------------------------------------------------------------------------- */
#define M_BLOCK_SAMPLES     10

static m_block_t  s_m_staging;
static int        s_m_count   = 0;
static uint64_t   s_m_ts_us   = 0;

/* --------------------------------------------------------------------------
 * Sequence counters for block headers
 * -------------------------------------------------------------------------- */
static uint16_t s_seq_a = 0;
static uint16_t s_seq_i = 0;
static uint16_t s_seq_m = 0;
static uint16_t s_seq_p = 0;
static uint16_t s_seq_s = 0;
static uint16_t s_seq_t = 0;

/* --------------------------------------------------------------------------
 * Helper: enqueue a heap-allocated copy of a vuams_block_t.
 * The block pointer is freed by the tx task after sending.
 * -------------------------------------------------------------------------- */
static void enqueue_block(const vuams_block_t *blk, size_t sz)
{
    vuams_block_t *copy = (vuams_block_t *)malloc(sz);
    if (copy == NULL) {
        ESP_LOGW(TAG, "enqueue_block: malloc failed (sz=%u)", (unsigned)sz);
        return;
    }
    memcpy(copy, blk, sz);

    if (xQueueSend(s_block_queue, &copy, 0) != pdTRUE) {
        /* Queue full — drop silently (best-effort BLE) */
        free(copy);
    }
}

/* --------------------------------------------------------------------------
 * Timer A callback — fires at 1000 Hz
 * Generates one ECG + one ICG sample and accumulates into the staging block.
 * Every 250 samples the complete a_block_t is enqueued.
 * -------------------------------------------------------------------------- */
static void timer_a_cb(void *arg)
{
    (void)arg;

    uint64_t now_us = (uint64_t)esp_timer_get_time();

    if (s_a_count == 0) {
        /* Start of a new block — record timestamp and reset staging */
        s_a_ts_us = now_us;
        memset(&s_a_staging, 0, sizeof(s_a_staging));
        s_a_staging.header.type        = BLOCK_TYPE_A;
        s_a_staging.header.version     = BLOCK_VERSION;
        s_a_staging.header.payload_len = sizeof(a_block_t) - sizeof(vuams_block_header_t);
        s_a_staging.header.timestamp_us = s_a_ts_us;
    }

    /* Generate one ECG sample (sign-extended to int32 as real firmware does) */
    int16_t ecg = synth_ecg_sample((uint32_t)now_us);
    int16_t icg = synth_icg_sample((uint32_t)now_us);

    s_a_staging.ecg1[s_a_count] = (int32_t)ecg;
    s_a_staging.ecg2[s_a_count] = (int32_t)icg;

    s_a_count++;

    if (s_a_count >= A_BLOCK_SAMPLES) {
        /* Block complete — enqueue it */
        vuams_block_t blk;
        blk.type = BLOCK_TYPE_A;
        memcpy(&blk.a, &s_a_staging, sizeof(a_block_t));
        enqueue_block(&blk, sizeof(vuams_block_t));
        s_a_count = 0;
        s_seq_a++;
    }
}

/* --------------------------------------------------------------------------
 * Timer M callback — fires at 100 Hz
 * Generates one sample per axis and accumulates into the staging m_block_t.
 * Every 10 samples the complete m_block_t is enqueued.
 * -------------------------------------------------------------------------- */
static void timer_m_cb(void *arg)
{
    (void)arg;

    uint64_t now_us = (uint64_t)esp_timer_get_time();

    if (s_m_count == 0) {
        s_m_ts_us = now_us;
        memset(&s_m_staging, 0, sizeof(s_m_staging));
        s_m_staging.header.type         = BLOCK_TYPE_M;
        s_m_staging.header.version      = BLOCK_VERSION;
        s_m_staging.header.payload_len  = sizeof(m_block_t) - sizeof(vuams_block_header_t);
        s_m_staging.header.timestamp_us = s_m_ts_us;
    }

    int i = s_m_count;
    s_m_staging.ax[i] = synth_imu_sample(0);
    s_m_staging.ay[i] = synth_imu_sample(1);
    s_m_staging.az[i] = synth_imu_sample(2);
    s_m_staging.gx[i] = synth_imu_sample(3);
    s_m_staging.gy[i] = synth_imu_sample(4);
    s_m_staging.gz[i] = synth_imu_sample(5);
    s_m_staging.mx[i] = synth_imu_sample(6);
    s_m_staging.my[i] = synth_imu_sample(7);
    s_m_staging.mz[i] = synth_imu_sample(8);

    s_m_count++;

    if (s_m_count >= M_BLOCK_SAMPLES) {
        vuams_block_t blk;
        blk.type = BLOCK_TYPE_M;
        memcpy(&blk.m, &s_m_staging, sizeof(m_block_t));
        enqueue_block(&blk, sizeof(vuams_block_t));
        s_m_count = 0;
        s_seq_m++;
    }
}

/* --------------------------------------------------------------------------
 * Timer P callback — fires at 100 Hz
 * Produces one p_block_t per tick (single-sample PPG block).
 * -------------------------------------------------------------------------- */
static void timer_p_cb(void *arg)
{
    (void)arg;

    uint64_t now_us = (uint64_t)esp_timer_get_time();

    p_block_t pb;
    memset(&pb, 0, sizeof(pb));
    pb.header.type         = BLOCK_TYPE_P;
    pb.header.version      = BLOCK_VERSION;
    pb.header.payload_len  = sizeof(p_block_t) - sizeof(vuams_block_header_t);
    pb.header.timestamp_us = now_us;

    int16_t ppg = synth_ppg_sample((uint32_t)now_us);
    /* Map int16 value to uint32 IR count (scale to plausible MAX30101 range) */
    /* synth_ppg returns 0..2800 (offset 2000, amplitude 800)
     * Scale to a plausible MAX30101 count range (50000–70000) */
    pb.ppg_ir     = 60000U + (uint32_t)((int32_t)ppg * 10);
    pb.ppg_red    = 58000U + (uint32_t)((int32_t)ppg * 9); /* red tracks IR, slightly lower */
    pb.spo2_pct   = 98.0f + ((float)(ppg % 5)) * 0.1f;  /* 98.0–98.4 % */
    pb.hr_ppg     = DUMMY_HEART_RATE_BPM;
    pb.ppg_valid  = 1;

    vuams_block_t blk;
    blk.type = BLOCK_TYPE_P;
    memcpy(&blk.p, &pb, sizeof(p_block_t));
    enqueue_block(&blk, sizeof(vuams_block_t));
    s_seq_p++;
}

/* --------------------------------------------------------------------------
 * Timer S callback — fires at 10 Hz
 * Produces one s_block_t per tick.
 * -------------------------------------------------------------------------- */
static void timer_s_cb(void *arg)
{
    (void)arg;

    uint64_t now_us = (uint64_t)esp_timer_get_time();

    s_block_t sb;
    memset(&sb, 0, sizeof(sb));
    sb.header.type         = BLOCK_TYPE_S;
    sb.header.version      = BLOCK_VERSION;
    sb.header.payload_len  = sizeof(s_block_t) - sizeof(vuams_block_header_t);
    sb.header.timestamp_us = now_us;

    int16_t scl = synth_scl_sample((uint32_t)now_us);
    /* Scale raw LSB to µS: assume 1 LSB ≈ 0.001 µS */
    sb.scl_tonic_uS    = (float)scl * 0.001f;
    sb.scl_phasic_uS   = 0.05f;   /* small synthetic phasic component */
    sb.electrode_contact = 1;

    vuams_block_t blk;
    blk.type = BLOCK_TYPE_S;
    memcpy(&blk.s, &sb, sizeof(s_block_t));
    enqueue_block(&blk, sizeof(vuams_block_t));
    s_seq_s++;
}

/* --------------------------------------------------------------------------
 * Timer T callback — fires at 0.25 Hz (every 4000 ms)
 * Produces one t_block_t per tick.
 * -------------------------------------------------------------------------- */
static void timer_t_cb(void *arg)
{
    (void)arg;

    uint64_t now_us = (uint64_t)esp_timer_get_time();

    t_block_t tb;
    memset(&tb, 0, sizeof(tb));
    tb.header.type         = BLOCK_TYPE_T;
    tb.header.version      = BLOCK_VERSION;
    tb.header.payload_len  = sizeof(t_block_t) - sizeof(vuams_block_header_t);
    tb.header.timestamp_us = now_us;

    float temp     = synth_temp_celsius();
    tb.skin_temp_c = temp;
    /* TMP117 raw: 1 LSB = 0.0078125 °C */
    tb.temp_raw    = (int16_t)(temp / 0.0078125f);

    vuams_block_t blk;
    blk.type = BLOCK_TYPE_T;
    memcpy(&blk.t, &tb, sizeof(t_block_t));
    enqueue_block(&blk, sizeof(vuams_block_t));
    s_seq_t++;
}

/* --------------------------------------------------------------------------
 * Timer I callback — fires at ~1.13 Hz (one per heartbeat at 68 BPM)
 * Produces one i_block_t per tick with synthetic haemodynamic values.
 * -------------------------------------------------------------------------- */
static void timer_i_cb(void *arg)
{
    (void)arg;

    uint64_t now_us = (uint64_t)esp_timer_get_time();

    i_block_t ib;
    memset(&ib, 0, sizeof(ib));
    ib.header.type         = BLOCK_TYPE_I;
    ib.header.version      = BLOCK_VERSION;
    ib.header.payload_len  = sizeof(i_block_t) - sizeof(vuams_block_header_t);
    ib.header.timestamp_us = now_us;

    /* Plausible synthetic haemodynamic values with small variation */
    ib.z0          = 28.0f  + 0.1f  * sinf((float)now_us * 1e-6f * 0.03f);
    ib.dZdt_peak   = -1.8f  + 0.05f * cosf((float)now_us * 1e-6f * 0.07f);
    ib.pep_ms      = 88.0f  + 2.0f  * sinf((float)now_us * 1e-6f * 0.05f);
    ib.lvet_ms     = 310.0f + 5.0f  * cosf((float)now_us * 1e-6f * 0.04f);
    ib.co_lpm      = 5.2f   + 0.1f  * sinf((float)now_us * 1e-6f * 0.02f);
    ib.sv_ml       = 70.0f  + 2.0f  * cosf((float)now_us * 1e-6f * 0.06f);

    vuams_block_t blk;
    blk.type = BLOCK_TYPE_I;
    memcpy(&blk.i, &ib, sizeof(i_block_t));
    enqueue_block(&blk, sizeof(vuams_block_t));
    s_seq_i++;
}

/* --------------------------------------------------------------------------
 * TX task — dequeues blocks and forwards them to BLE
 * -------------------------------------------------------------------------- */
static void tx_task(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "TX task started on core %d", xPortGetCoreID());

    while (1) {
        vuams_block_t *blk = NULL;

        if (xQueueReceive(s_block_queue, &blk, pdMS_TO_TICKS(50)) != pdTRUE) {
            continue;
        }
        if (blk == NULL) {
            continue;
        }

        ble_dummy_send_block(blk);
        free(blk);
    }
}

/* --------------------------------------------------------------------------
 * app_main
 * -------------------------------------------------------------------------- */
void app_main(void)
{
    ESP_LOGI(TAG, "VU-AMS Dummy Firmware v1.0 — synthetic signals active");
    ESP_LOGI(TAG, "Heart rate: %d BPM, ECG@%d Hz, IMU@%d Hz, PPG@%d Hz, SCL@%d Hz",
             DUMMY_HEART_RATE_BPM,
             DUMMY_ECG_RATE_HZ, DUMMY_IMU_RATE_HZ,
             DUMMY_PPG_RATE_HZ, DUMMY_SCL_RATE_HZ);

    /* --- NVS flash (required by BLE stack) --- */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* --- Block queue --- */
    s_block_queue = xQueueCreate(BLOCK_QUEUE_DEPTH, sizeof(vuams_block_t *));
    if (s_block_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create block queue");
        return;
    }

    /* --- BLE --- */
    ble_dummy_init();

    /* --- TX task on core 1 (same as real firmware) --- */
    xTaskCreatePinnedToCore(tx_task, "tx_task", 8192, NULL, 4, NULL, 1);

    /* --- Signal generation timers --- */

    /* Timer A: 1000 Hz = 1000 µs period */
    esp_timer_handle_t timer_a;
    esp_timer_create_args_t args_a = {
        .callback        = timer_a_cb,
        .arg             = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "tmr_ecg",
    };
    ESP_ERROR_CHECK(esp_timer_create(&args_a, &timer_a));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_a, 1000));   /* µs */

    /* Timer M: 100 Hz = 10 000 µs period */
    esp_timer_handle_t timer_m;
    esp_timer_create_args_t args_m = {
        .callback        = timer_m_cb,
        .arg             = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "tmr_imu",
    };
    ESP_ERROR_CHECK(esp_timer_create(&args_m, &timer_m));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_m, 10000));  /* µs */

    /* Timer P: 100 Hz = 10 000 µs period */
    esp_timer_handle_t timer_p;
    esp_timer_create_args_t args_p = {
        .callback        = timer_p_cb,
        .arg             = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "tmr_ppg",
    };
    ESP_ERROR_CHECK(esp_timer_create(&args_p, &timer_p));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_p, 10000));  /* µs */

    /* Timer S: 10 Hz = 100 000 µs period */
    esp_timer_handle_t timer_s;
    esp_timer_create_args_t args_s = {
        .callback        = timer_s_cb,
        .arg             = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "tmr_scl",
    };
    ESP_ERROR_CHECK(esp_timer_create(&args_s, &timer_s));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_s, 100000)); /* µs */

    /* Timer T: 0.25 Hz = 4 000 000 µs period */
    esp_timer_handle_t timer_t;
    esp_timer_create_args_t args_t = {
        .callback        = timer_t_cb,
        .arg             = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "tmr_temp",
    };
    ESP_ERROR_CHECK(esp_timer_create(&args_t, &timer_t));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_t, 4000000)); /* µs */

    /* Timer I: one per heartbeat = DUMMY_BEAT_PERIOD_US */
    esp_timer_handle_t timer_i;
    esp_timer_create_args_t args_i = {
        .callback        = timer_i_cb,
        .arg             = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "tmr_icg",
    };
    ESP_ERROR_CHECK(esp_timer_create(&args_i, &timer_i));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_i, DUMMY_BEAT_PERIOD_US));

    ESP_LOGI(TAG, "All timers started — advertising as VU-AMS");

    /* app_main returns; all work is done in timers + tasks */
}
