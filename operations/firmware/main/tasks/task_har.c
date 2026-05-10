/* ==========================================================================
 * VU-AMS Firmware — Human Activity Recognition (HAR) task
 * xTaskCreatePinnedToCore: priority 3, core 1, stack 8192
 * Author: Kai Müller
 * Date:   2026-05-10
 *
 * Algorithm overview (spec: har_sad_spec_001.md §2.3 – §2.4):
 *
 *  Inputs:
 *   - g_har_m_block_queue  : m_block_t* pointers forwarded by task_block_assembler
 *   - g_har_baro_queue     : float pressure values (Pa) from task_i2c_sensors
 *
 *  1. Accumulate M-block samples (10 samples/block, 100 Hz) into a 300-sample
 *     linear buffer (WIN_SAMPLES=200 + HOP_SAMPLES=100 headroom).  Process a
 *     feature window every HOP_SAMPLES (100) new samples; window covers the
 *     latest WIN_SAMPLES (200) samples = 2 s with 1 s hop (50% overlap).
 *
 *  2. Feature extraction per window (spec §2.3):
 *     - Accel magnitude, RMS, per-axis mean
 *     - Tilt angles from mean gravity vector (radians, converted to m/s²)
 *     - Dominant frequency via autocorrelation of mean-subtracted magnitude
 *     - Step regularity index (normalised autocorrelation peak, lag 0.4–2.0 s)
 *     - Gyroscope magnitude RMS
 *     - Barometric pressure rate dP/dt (Pa/s) over 5 s history
 *
 *  3. Rule-based classifier (spec §2.4.2), priority order:
 *     LYING → SITTING → STANDING → STAIRS_UP → STAIRS_DOWN →
 *     RUNNING → WALKING → CYCLING → UNKNOWN
 *
 *  4. Post-processing: 3-window majority vote + 4-window minimum hold.
 *
 *  5. Step counting: positive zero-crossings of mean-subtracted magnitude
 *     during WALKING/RUNNING; cadence derived over rolling 60-window (60 s).
 *
 *  6. Publish X-block to g_x_block_queue every window.
 *
 * Scale factors (ICM-20948 ±2g / ±250°/s defaults):
 *   Accelerometer: 1 LSB = 1/16384 g
 *   Gyroscope:     1 LSB = 1/131 °/s → × π/180 for rad/s
 * ========================================================================== */

#include "task_har.h"
#include "task_sad.h"
#include "data_blocks.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>

static const char *TAG = "task_har";

/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */

#define G_PER_LSB           (1.0f / 16384.0f)   /* ±2g accel scale           */
#define G_TO_MS2            9.80665f              /* 1 g in m/s²               */
#ifndef M_PI
#define M_PI                3.14159265358979323846f
#endif
#define RAD_PER_DEG         (M_PI / 180.0f)
#define DEG_PER_LSB_GYRO    (1.0f / 131.0f)      /* ±250°/s gyro scale        */
#define GYRO_RADPS_PER_LSB  (DEG_PER_LSB_GYRO * RAD_PER_DEG)

/* Window / hop */
#define WIN_SAMPLES         200    /* 2 s @ 100 Hz                             */
#define HOP_SAMPLES         100    /* 1 s hop                                  */
#define FS_HZ               100.0f

/* Circular buffer: WIN_SAMPLES + HOP_SAMPLES avoids wrap-around in extraction */
#define CBUF_LEN            (WIN_SAMPLES + HOP_SAMPLES)

/* Barometer history: 5 s @ 25 Hz = 125 samples                               */
#define BARO_HIST_LEN       125
#define BARO_FS_HZ          25.0f
#define BARO_5S_SAMPLES     ((int)(BARO_FS_HZ * 5.0f + 0.5f))  /* 125 */

/* Post-processing */
#define VOTE_WIN            3      /* majority-vote window (windows)           */
#define MIN_HOLD_WINDOWS    4      /* minimum 4 windows (4 s) before switching */

/* Queue depths for HAR's dedicated input queues */
#define HAR_M_BLOCK_QUEUE_DEPTH   8    /* 8 M-blocks = 800 ms                 */
#define HAR_BARO_QUEUE_DEPTH      32   /* 32 pressure floats ≈ 1.3 s @ 25 Hz  */
#define X_BLOCK_QUEUE_DEPTH       8

/* Autocorrelation lag limits */
#define AC_STEP_LAG_MIN     40     /* 0.4 s = 2.5 Hz upper cadence           */
#define AC_STEP_LAG_MAX     199    /* 2.0 s = 0.5 Hz lower cadence           */

/* --------------------------------------------------------------------------
 * Internal types
 * -------------------------------------------------------------------------- */
typedef struct {
    float ax[CBUF_LEN];
    float ay[CBUF_LEN];
    float az[CBUF_LEN];
    float gx[CBUF_LEN];
    float gy[CBUF_LEN];
    float gz[CBUF_LEN];
} sample_buf_t;

typedef struct {
    float rms_mag;
    float ax_mean, ay_mean, az_mean;
    float gyro_rms;
    float theta_x, theta_y, theta_z; /* radians                              */
    float dominant_freq_hz;
    float step_regularity;
    int   step_lag_samples;
    float baro_rate_pa_s;
    bool  baro_valid;
} har_features_t;

/* --------------------------------------------------------------------------
 * Module-level state
 * -------------------------------------------------------------------------- */

QueueHandle_t g_x_block_queue       = NULL;
QueueHandle_t g_har_m_block_queue   = NULL;
QueueHandle_t g_har_baro_queue      = NULL;

/* Sample ring buffer */
static sample_buf_t s_buf;
static int          s_write_pos = 0;   /* next write slot (mod CBUF_LEN)      */
static int          s_new_count = 0;   /* samples since last window process   */
static bool         s_buf_full  = false;

/* Barometer circular buffer */
static float s_baro_hist[BARO_HIST_LEN];
static int   s_baro_head  = 0;
static int   s_baro_count = 0;

/* Post-processing state */
static har_class_t s_class_history[VOTE_WIN];
static int         s_hist_idx     = 0;
static har_class_t s_voted_class  = HAR_UNKNOWN;
static har_class_t s_held_class   = HAR_UNKNOWN;
static int         s_hold_counter = 0;

/* X-block sequence */
static uint16_t s_x_seq = 0;

/* Cadence / step accumulation */
static float s_step_accum  = 0.0f;
static int   s_win_for_cad = 0;
static float s_rms_accum   = 0.0f;
static int   s_rms_count   = 0;

/* Atomic outputs */
static _Atomic(uint32_t) s_atomic_class;
static _Atomic(uint32_t) s_atomic_cadence_x10;
static _Atomic(uint32_t) s_atomic_intensity_x1000;

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */
static void        ingest_m_block(const m_block_t *blk);
static void        extract_features(har_features_t *f);
static float       autocorr_dominant_freq(const float *mag, int n,
                                          float fs, float fmin, float fmax,
                                          int *out_lag);
static float       step_regularity(const float *mag, int n, int *out_lag);
static har_class_t classify(const har_features_t *f);
static har_class_t postprocess(har_class_t raw);
static float       baro_rate_pa_s(void);
static void        publish_x_block(har_class_t cls,
                                   float cadence, float intensity);

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void task_har_init(void)
{
    memset(&s_buf,           0, sizeof(s_buf));
    memset(s_baro_hist,      0, sizeof(s_baro_hist));
    memset(s_class_history,  0, sizeof(s_class_history));

    atomic_store(&s_atomic_class,           (uint32_t)HAR_UNKNOWN);
    atomic_store(&s_atomic_cadence_x10,     0u);
    atomic_store(&s_atomic_intensity_x1000, 0u);

    g_x_block_queue     = xQueueCreate(X_BLOCK_QUEUE_DEPTH,    sizeof(x_block_t *));
    g_har_m_block_queue = xQueueCreate(HAR_M_BLOCK_QUEUE_DEPTH, sizeof(m_block_t *));
    g_har_baro_queue    = xQueueCreate(HAR_BARO_QUEUE_DEPTH,    sizeof(float));

    if (!g_x_block_queue || !g_har_m_block_queue || !g_har_baro_queue) {
        ESP_LOGE(TAG, "Queue creation failed");
    }
    ESP_LOGI(TAG, "HAR init done");
}

har_class_t task_har_get_class(void)
{
    return (har_class_t)atomic_load(&s_atomic_class);
}

float task_har_get_cadence_spm(void)
{
    return (float)atomic_load(&s_atomic_cadence_x10) / 10.0f;
}

float task_har_get_motion_intensity(void)
{
    return (float)atomic_load(&s_atomic_intensity_x1000) / 1000.0f;
}

/* --------------------------------------------------------------------------
 * Task
 * -------------------------------------------------------------------------- */

void task_har(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "HAR task started on core %d", xPortGetCoreID());

    while (1) {
        /* --- Drain baro pressure queue (non-blocking) ---------------------- */
        float baro_pa = 0.0f;
        while (g_har_baro_queue != NULL &&
               xQueueReceive(g_har_baro_queue, &baro_pa, 0) == pdTRUE) {
            s_baro_hist[s_baro_head] = baro_pa;
            s_baro_head = (s_baro_head + 1) % BARO_HIST_LEN;
            if (s_baro_count < BARO_HIST_LEN) s_baro_count++;
        }

        /* --- Receive one M-block (block up to 10 ms for pacing) ------------ */
        m_block_t *m_blk = NULL;
        if (xQueueReceive(g_har_m_block_queue, &m_blk, pdMS_TO_TICKS(10)) != pdTRUE
            || m_blk == NULL) {
            continue;
        }

        ingest_m_block(m_blk);
        free(m_blk);

        /* --- Process window when HOP_SAMPLES new samples accumulated -------- */
        if (s_new_count < HOP_SAMPLES || !s_buf_full) continue;
        s_new_count = 0;

        har_features_t feat;
        extract_features(&feat);

        har_class_t raw = classify(&feat);

        /* Step count from last hop window (mean-subtracted positive crossings) */
        if (raw == HAR_WALKING || raw == HAR_RUNNING) {
            int hop_start = (s_write_pos - HOP_SAMPLES + CBUF_LEN * 2) % CBUF_LEN;
            float hop_mag[HOP_SAMPLES];
            float hop_sum = 0.0f;
            for (int i = 0; i < HOP_SAMPLES; i++) {
                int idx = (hop_start + i) % CBUF_LEN;
                float m = sqrtf(s_buf.ax[idx]*s_buf.ax[idx] +
                                s_buf.ay[idx]*s_buf.ay[idx] +
                                s_buf.az[idx]*s_buf.az[idx]);
                hop_mag[i] = m;
                hop_sum   += m;
            }
            float hop_mean = hop_sum / (float)HOP_SAMPLES;
            int steps = 0;
            float prev = hop_mag[0] - hop_mean;
            for (int i = 1; i < HOP_SAMPLES; i++) {
                float curr = hop_mag[i] - hop_mean;
                if (prev <= 0.0f && curr > 0.0f) steps++;
                prev = curr;
            }
            s_step_accum += (float)steps;
        }
        s_win_for_cad++;
        s_rms_accum += feat.rms_mag;
        s_rms_count++;

        /* Cadence estimate over 60-window rolling epoch */
        float cadence_spm = 0.0f;
        if (s_win_for_cad >= 60) {
            cadence_spm  = s_step_accum;  /* steps in 60 s */
            s_step_accum = 0.0f;
            s_win_for_cad = 0;
        } else if (s_win_for_cad > 0) {
            /* rolling extrapolation */
            cadence_spm = (s_step_accum / (float)s_win_for_cad) * 60.0f;
        }

        /* Motion intensity: clamp((mean_rms - 1.0) / 2.0, 0, 1) */
        float mean_rms = (s_rms_count > 0) ?
                         (s_rms_accum / (float)s_rms_count) : 1.0f;
        float intensity = (mean_rms - 1.0f) / 2.0f;
        if (intensity < 0.0f) intensity = 0.0f;
        if (intensity > 1.0f) intensity = 1.0f;

        har_class_t smooth = postprocess(raw);

        /* Update atomics */
        atomic_store(&s_atomic_class,           (uint32_t)smooth);
        atomic_store(&s_atomic_cadence_x10,     (uint32_t)(cadence_spm * 10.0f + 0.5f));
        atomic_store(&s_atomic_intensity_x1000, (uint32_t)(intensity * 1000.0f + 0.5f));

        publish_x_block(smooth, cadence_spm, intensity);
    }
}

/* --------------------------------------------------------------------------
 * Ingest 10 samples from one M-block into the circular buffer
 * -------------------------------------------------------------------------- */
static void ingest_m_block(const m_block_t *blk)
{
    for (int i = 0; i < 10; i++) {
        int idx = s_write_pos % CBUF_LEN;

        s_buf.ax[idx] = (float)blk->ax[i] * G_PER_LSB;
        s_buf.ay[idx] = (float)blk->ay[i] * G_PER_LSB;
        s_buf.az[idx] = (float)blk->az[i] * G_PER_LSB;
        s_buf.gx[idx] = (float)blk->gx[i] * GYRO_RADPS_PER_LSB;
        s_buf.gy[idx] = (float)blk->gy[i] * GYRO_RADPS_PER_LSB;
        s_buf.gz[idx] = (float)blk->gz[i] * GYRO_RADPS_PER_LSB;

        s_write_pos++;
        if (s_write_pos >= CBUF_LEN) {
            s_write_pos %= CBUF_LEN;
            s_buf_full = true;
        }
        s_new_count++;
    }
}

/* --------------------------------------------------------------------------
 * Autocorrelation-based dominant frequency
 *
 *   R[k] = Σ x'[n]·x'[n+k]  for n = 0 … N-1-k
 *   Normalise R[k] / R[0]
 *   f_dom = FS / k_peak  where k_peak = argmax R[k] for k in [FS/fmax, FS/fmin]
 * -------------------------------------------------------------------------- */
static float autocorr_dominant_freq(const float *mag, int n,
                                    float fs, float fmin, float fmax,
                                    int *out_lag)
{
    /* Mean-subtract on stack — n ≤ WIN_SAMPLES */
    float x[WIN_SAMPLES];
    if (n > WIN_SAMPLES) n = WIN_SAMPLES;

    float sum = 0.0f;
    for (int i = 0; i < n; i++) sum += mag[i];
    float mean = sum / (float)n;
    for (int i = 0; i < n; i++) x[i] = mag[i] - mean;

    float r0 = 0.0f;
    for (int i = 0; i < n; i++) r0 += x[i] * x[i];
    if (r0 < 1e-12f) {
        if (out_lag) *out_lag = 0;
        return 0.0f;
    }

    int lag_lo = (int)(fs / fmax);
    int lag_hi = (int)(fs / fmin);
    if (lag_lo < 1)    lag_lo = 1;
    if (lag_hi >= n)   lag_hi = n - 1;

    float best_r   = -1e30f;
    int   best_lag = lag_lo;

    for (int k = lag_lo; k <= lag_hi; k++) {
        float r = 0.0f;
        for (int i = 0; i < n - k; i++) r += x[i] * x[i + k];
        r /= r0;
        if (r > best_r) { best_r = r; best_lag = k; }
    }

    if (out_lag) *out_lag = best_lag;
    return (best_r > 0.0f) ? (fs / (float)best_lag) : 0.0f;
}

/* --------------------------------------------------------------------------
 * Step regularity: normalised autocorrelation peak for lags [40, 199] samples
 * Returns 0.0–1.0.  out_lag → lag in samples at peak.
 * -------------------------------------------------------------------------- */
static float step_regularity(const float *mag, int n, int *out_lag)
{
    float x[WIN_SAMPLES];
    if (n > WIN_SAMPLES) n = WIN_SAMPLES;

    float sum = 0.0f;
    for (int i = 0; i < n; i++) sum += mag[i];
    float mean = sum / (float)n;
    for (int i = 0; i < n; i++) x[i] = mag[i] - mean;

    float r0 = 0.0f;
    for (int i = 0; i < n; i++) r0 += x[i] * x[i];
    if (r0 < 1e-12f) {
        if (out_lag) *out_lag = AC_STEP_LAG_MIN;
        return 0.0f;
    }

    int lag_max = AC_STEP_LAG_MAX;
    if (lag_max >= n) lag_max = n - 1;

    float best_r   = -1e30f;
    int   best_lag = AC_STEP_LAG_MIN;

    for (int k = AC_STEP_LAG_MIN; k <= lag_max; k++) {
        float r = 0.0f;
        for (int i = 0; i < n - k; i++) r += x[i] * x[i + k];
        r /= r0;
        if (r > best_r) { best_r = r; best_lag = k; }
    }

    if (out_lag) *out_lag = best_lag;
    return (best_r > 0.0f) ? best_r : 0.0f;
}

/* --------------------------------------------------------------------------
 * Extract feature vector from the latest WIN_SAMPLES in s_buf
 * -------------------------------------------------------------------------- */
static void extract_features(har_features_t *f)
{
    float mag[WIN_SAMPLES];
    float ax_s = 0, ay_s = 0, az_s = 0;
    float gx_s = 0, gy_s = 0, gz_s = 0;
    float mag_sq = 0, gyro_sq = 0;

    /* Ring buffer extraction: newest WIN_SAMPLES samples ending at s_write_pos */
    int start = (s_write_pos - WIN_SAMPLES + CBUF_LEN * 4) % CBUF_LEN;

    for (int i = 0; i < WIN_SAMPLES; i++) {
        int idx = (start + i) % CBUF_LEN;
        float ax = s_buf.ax[idx], ay = s_buf.ay[idx], az = s_buf.az[idx];
        float gx = s_buf.gx[idx], gy = s_buf.gy[idx], gz = s_buf.gz[idx];

        float m = sqrtf(ax*ax + ay*ay + az*az);
        mag[i] = m;

        ax_s += ax;  ay_s += ay;  az_s += az;
        gx_s += gx;  gy_s += gy;  gz_s += gz;
        mag_sq   += m * m;
        gyro_sq  += gx*gx + gy*gy + gz*gz;
    }

    float inv = 1.0f / (float)WIN_SAMPLES;
    f->ax_mean = ax_s * inv;
    f->ay_mean = ay_s * inv;
    f->az_mean = az_s * inv;

    f->rms_mag  = sqrtf(mag_sq  * inv);
    f->gyro_rms = sqrtf(gyro_sq * inv);

    /* Tilt angles (radians) from mean gravity vector */
    float ax_m = f->ax_mean, ay_m = f->ay_mean, az_m = f->az_mean;
    f->theta_x = atan2f(ax_m, sqrtf(ay_m*ay_m + az_m*az_m));
    f->theta_y = atan2f(ay_m, sqrtf(ax_m*ax_m + az_m*az_m));
    f->theta_z = atan2f(az_m, sqrtf(ax_m*ax_m + ay_m*ay_m));

    /* Dominant frequency (0.5–10 Hz via autocorrelation) */
    int dom_lag = 0;
    f->dominant_freq_hz = autocorr_dominant_freq(mag, WIN_SAMPLES, FS_HZ,
                                                  0.5f, 10.0f, &dom_lag);

    /* Step regularity (0.4–2.0 s) */
    int st_lag = 0;
    f->step_regularity   = step_regularity(mag, WIN_SAMPLES, &st_lag);
    f->step_lag_samples  = st_lag;

    /* Barometric rate */
    f->baro_rate_pa_s = baro_rate_pa_s();
    f->baro_valid     = (s_baro_count >= BARO_5S_SAMPLES);
}

/* --------------------------------------------------------------------------
 * Barometric pressure rate of change over ≤5 s history (Pa/s)
 * -------------------------------------------------------------------------- */
static float baro_rate_pa_s(void)
{
    if (s_baro_count < 2) return 0.0f;

    int span     = (s_baro_count < BARO_HIST_LEN) ? s_baro_count : BARO_HIST_LEN;
    int lookback = BARO_5S_SAMPLES;
    if (lookback > span) lookback = span;
    if (lookback < 2)    return 0.0f;

    /* newest = head - 1 (mod), oldest = head - lookback (mod) */
    int newest = (s_baro_head - 1 + BARO_HIST_LEN) % BARO_HIST_LEN;
    int oldest = (s_baro_head - lookback + BARO_HIST_LEN) % BARO_HIST_LEN;

    float dp = s_baro_hist[newest] - s_baro_hist[oldest];
    float dt = (float)(lookback - 1) / BARO_FS_HZ;
    if (dt < 0.001f) return 0.0f;

    return dp / dt;
}

/* --------------------------------------------------------------------------
 * Rule-based classifier (spec §2.4.2, priority order)
 *
 * All thresholds work on:
 *   rms_mag   — g (gravity-inclusive)
 *   ax_mean/ay_mean/az_mean — g, converted to m/s² only for rules that use m/s²
 *   gyro_rms  — rad/s
 *   baro_rate — Pa/s
 * -------------------------------------------------------------------------- */
static har_class_t classify(const har_features_t *f)
{
    float ax_ms2   = f->ax_mean * G_TO_MS2;
    float ay_ms2   = f->ay_mean * G_TO_MS2;
    float az_ms2   = f->az_mean * G_TO_MS2;
    float lat_ms2  = sqrtf(ax_ms2*ax_ms2 + ay_ms2*ay_ms2);
    float fdom     = f->dominant_freq_hz;

    /* Rule 1 — Lying
     *   |az| > 9.0 m/s²  AND  lateral < 1.5 m/s²  AND  rms_mag < 1.2 g     */
    if (fabsf(az_ms2) > 9.0f && lat_ms2 < 1.5f && f->rms_mag < 1.2f)
        return HAR_LYING;

    /* Rule 2 — Sitting
     *   (|ax| > 7.0 OR |ay| > 7.0) m/s²  AND  rms_mag < 1.3 g               */
    if ((fabsf(ax_ms2) > 7.0f || fabsf(ay_ms2) > 7.0f) && f->rms_mag < 1.3f)
        return HAR_SITTING;

    /* Rule 3 — Standing
     *   |az| > 8.5 m/s²  AND  rms_mag < 1.15 g  AND  f_dom < 0.5 Hz         */
    if (fabsf(az_ms2) > 8.5f && f->rms_mag < 1.15f && fdom < 0.5f)
        return HAR_STANDING;

    /* Rules 4/5 — Stairs (barometer required) */
    if (f->baro_valid) {
        /* Rule 4 — Ascending: dP/dt < -0.5 Pa/s  AND  f_dom 1.5–2.5 Hz     */
        if (f->baro_rate_pa_s < -0.5f && fdom >= 1.5f && fdom <= 2.5f)
            return HAR_STAIRS_UP;

        /* Rule 5 — Descending: dP/dt > +0.5 Pa/s  AND  f_dom 1.5–2.5 Hz    */
        if (f->baro_rate_pa_s > 0.5f && fdom >= 1.5f && fdom <= 2.5f)
            return HAR_STAIRS_DOWN;
    }

    /* Rule 7 — Running (before Walking: overlap at 2.0–2.5 Hz resolved by rms)
     *   f_dom 2.0–4.0 Hz  AND  rms_mag > 1.8 g                               */
    if (fdom >= 2.0f && fdom <= 4.0f && f->rms_mag > 1.8f)
        return HAR_RUNNING;

    /* Rule 6 — Walking
     *   f_dom 1.0–2.5 Hz  AND  step_reg > 0.7  AND  rms_mag 1.1–1.6 g       */
    if (fdom >= 1.0f && fdom <= 2.5f &&
        f->step_regularity > 0.7f &&
        f->rms_mag >= 1.1f && f->rms_mag <= 1.6f)
        return HAR_WALKING;

    /* Rule 8 — Cycling
     *   f_dom 0.5–2.0 Hz  AND  gyro_rms < 0.5 rad/s  AND  |az| 8.0–10.5 m/s² */
    if (fdom >= 0.5f && fdom <= 2.0f &&
        f->gyro_rms < 0.5f &&
        fabsf(az_ms2) >= 8.0f && fabsf(az_ms2) <= 10.5f)
        return HAR_CYCLING;

    /* Rule 9 — Unknown */
    return HAR_UNKNOWN;
}

/* --------------------------------------------------------------------------
 * Post-processing: 3-window majority vote + 4-window minimum hold
 * -------------------------------------------------------------------------- */
static har_class_t majority_vote(void)
{
    int counts[9] = {0};
    for (int i = 0; i < VOTE_WIN; i++) {
        int c = (int)s_class_history[i];
        if (c >= 0 && c < 9) counts[c]++;
    }
    int best = 0;
    for (int i = 1; i < 9; i++) {
        if (counts[i] > counts[best]) best = i;
    }
    return (har_class_t)best;
}

static har_class_t postprocess(har_class_t raw)
{
    s_class_history[s_hist_idx % VOTE_WIN] = raw;
    s_hist_idx++;

    if (s_hist_idx < VOTE_WIN) return HAR_UNKNOWN;

    har_class_t voted = majority_vote();

    if (voted != s_held_class) {
        s_held_class   = voted;
        s_hold_counter = 1;
    } else {
        s_hold_counter++;
    }

    if (s_hold_counter >= MIN_HOLD_WINDOWS) {
        s_voted_class = voted;
    }

    return s_voted_class;
}

/* --------------------------------------------------------------------------
 * Publish X-block to g_x_block_queue
 * SAD fields (speaking, speaking_fraction) are populated as 0 here.
 * task_sad updates s_atomic values which task_har reads and packs into the block.
 * -------------------------------------------------------------------------- */
static void publish_x_block(har_class_t cls, float cadence, float intensity)
{
    x_block_t *xblk = (x_block_t *)malloc(sizeof(x_block_t));
    if (xblk == NULL) {
        ESP_LOGE(TAG, "malloc x_block_t failed");
        return;
    }

    xblk->block_type     = BLOCK_TYPE_X;
    xblk->seq            = s_x_seq++;
    xblk->timestamp_us   = (uint32_t)esp_timer_get_time();
    xblk->activity_class = (uint8_t)cls;

    uint32_t cad_fp = (uint32_t)(cadence * 10.0f + 0.5f);
    xblk->cadence_spm    = (cad_fp > 0xFFFFu) ? 0xFFFFu : (uint16_t)cad_fp;

    uint32_t intens = (uint32_t)(intensity * 255.0f + 0.5f);
    xblk->motion_intensity  = (intens > 255u) ? 255u : (uint8_t)intens;

    /* SAD fields set to 0; task_sad provides its own atomic getters and the
     * X-block is the HAR's primary output.  If tighter integration is needed,
     * task_har can call task_sad_is_speaking() / task_sad_get_speaking_fraction()
     * since those are lock-free atomic reads.                                  */
    xblk->speaking          = task_sad_is_speaking() ? 1u : 0u;
    xblk->speaking_fraction = (uint8_t)(task_sad_get_speaking_fraction() * 100.0f + 0.5f);
    xblk->crc               = 0x0000;

    if (g_x_block_queue == NULL ||
        xQueueSend(g_x_block_queue, &xblk, pdMS_TO_TICKS(5)) != pdTRUE) {
        ESP_LOGW(TAG, "x_block_queue full or NULL — X-block dropped");
        free(xblk);
    }
}
