/* ==========================================================================
 * VU-AMS Firmware — Block assembler task
 * xTaskCreatePinnedToCore(task_block_assembler, "task_asm", 8192, NULL, 6, NULL, 1)
 * Priority: 6  |  Core: 1  |  Stack: 8192 bytes
 * Author: Kai Müller
 * Date:   2026-05-08  (cardiac analysis added 2026-05-11)
 *
 * Responsibilities:
 *  1. Drain blocks from all sensor queues (ADC, IMU, I2C).
 *  2. Derive I-blocks (ICG haemodynamics) and Y-blocks (HRV) from ECG/ICG.
 *  3. Write serialised blocks into the PSRAM ring buffer.
 *  4. Forward block pointers to g_sd_write_queue and g_ble_tx_queue.
 * ========================================================================== */

#include "task_block_assembler.h"
#include "task_adc.h"
#include "task_imu.h"
#include "task_i2c_sensors.h"
#include "task_sd_writer.h"
#include "task_ble_stream.h"
#include "task_har.h"
#include "task_sad.h"
#include "ecg_analysis.h"
#include "icg_analysis.h"
#include "psram_buffer.h"
#include "data_blocks.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "task_asm";

/* Reference to the global PSRAM ring buffer (defined in main.c) */
extern psram_ringbuf_t g_psram_buf;

/* --------------------------------------------------------------------------
 * Cardiac analysis state — static, allocated once in BSS.
 * ECG sample rate: 1000 Hz (ADS1256 DRATE_1000SPS, ECG channels).
 * -------------------------------------------------------------------------- */
#define CARDIAC_SAMPLE_RATE_HZ   1000.0f

/* Y-block is emitted every this many valid beats */
#define Y_BLOCK_BEAT_INTERVAL    30

/* Kubicek stroke-volume formula constants */
#define KUBICEK_L_CM  30.0f  /* TODO: replace with per-session value from subject metadata once session setup is wired */

static ecg_analyzer_t s_ecg_an;
static icg_analyzer_t s_icg_an;

/* Valid-beat counter for Y-block emission cadence */
static uint8_t  s_beat_count   = 0;
static float    s_last_rri_ms  = 0.0f;    /* most recent accepted RRI   */

/* One-sample R-peak latch: set by the ECG drain, consumed by the ICG drain */
static bool     s_r_peak_latch = false;

/* --------------------------------------------------------------------------
 * PPG-RIIV respiratory rate state
 * Fs_ppg = 50 Hz; 30-second window = 1500 samples; 5-second step = 250 samples
 * -------------------------------------------------------------------------- */
#define RIIV_FS_HZ           50
#define RIIV_WINDOW_SAMPLES  1500    /* 30 s */
#define RIIV_STEP_SAMPLES    250     /* 5 s  */
#define RIIV_MIN_PEAK_SEP    75      /* 1.5 s × 50 Hz */
#define RIIV_MAX_PEAK_SEP    600     /* 12 s × 50 Hz (minimum 5 br/min) */

/* 4th-order Butterworth bandpass 0.1–0.5 Hz at Fs=50 Hz (two biquad sections).
 * Coefficients from Vasquez RESP-001 Section 5.2.
 * IMPORTANT: Validate against scipy.signal.butter(4, [0.1, 0.5], btype='bandpass',
 * fs=50, output='sos') before production use. */
typedef struct {
    double b0, b1, b2;
    double a1, a2;
    double z1, z2;   /* Direct Form II transposed state */
} biquad_t;

static biquad_t s_riiv_sos[2] = {
    { 1.889e-3,  0.0, -1.889e-3,  -1.9926, 0.99622, 0.0, 0.0 },
    { 1.889e-3,  0.0, -1.889e-3,  -1.9805, 0.99622, 0.0, 0.0 }
};

static float  s_riiv_buf[RIIV_WINDOW_SAMPLES];  /* circular buffer of filtered samples */
static int    s_riiv_write  = 0;                /* write index into s_riiv_buf */
static int    s_riiv_count  = 0;                /* total samples received */
static int    s_riiv_step   = 0;                /* samples since last R-block emission */

/* Forward declaration — dispatch_block is defined after emit_r_block */
static void dispatch_block(const void *block_ptr, size_t block_size, uint8_t type);

/* --------------------------------------------------------------------------
 * Internal: biquad cascade filter (Direct Form II transposed)
 * -------------------------------------------------------------------------- */

static float biquad_cascade(biquad_t *sos, int n_stages, float x)
{
    double y = (double)x;
    for (int i = 0; i < n_stages; i++) {
        double w  = y - sos[i].a1 * sos[i].z1 - sos[i].a2 * sos[i].z2;
        y         = sos[i].b0 * w + sos[i].b1 * sos[i].z1 + sos[i].b2 * sos[i].z2;
        sos[i].z2 = sos[i].z1;
        sos[i].z1 = w;
    }
    return (float)y;
}

/* --------------------------------------------------------------------------
 * Internal: emit an R-block (PPG-RIIV respiratory rate)
 * -------------------------------------------------------------------------- */

static void emit_r_block(void)
{
    /* Peak detection on the last 30 seconds of filtered RIIV signal.
     * Returns instantaneous respiratory rate in breaths/min, or NAN.
     *
     * Algorithm: scan the circular buffer for local maxima separated by
     * at least RIIV_MIN_PEAK_SEP samples (1.5 s) and at most RIIV_MAX_PEAK_SEP
     * samples (12 s).  Require prominence ≥ 5% of local range.  Average the
     * instantaneous rates from successive peak pairs.
     */

    /* Copy window into a linear scratch array (unroll circular buffer) */
    float win[RIIV_WINDOW_SAMPLES];
    int base = s_riiv_write % RIIV_WINDOW_SAMPLES;
    for (int i = 0; i < RIIV_WINDOW_SAMPLES; i++) {
        win[i] = s_riiv_buf[(base + i) % RIIV_WINDOW_SAMPLES];
    }

    /* Find local maxima */
    int   peak_idx[50];
    int   n_peaks = 0;
    float sig_min =  1e30f;
    float sig_max = -1e30f;
    for (int i = 0; i < RIIV_WINDOW_SAMPLES; i++) {
        if (win[i] < sig_min) sig_min = win[i];
        if (win[i] > sig_max) sig_max = win[i];
    }
    float prominence_thr = 0.05f * (sig_max - sig_min);

    int last_peak = -RIIV_MIN_PEAK_SEP;
    for (int i = 1; i < RIIV_WINDOW_SAMPLES - 1; i++) {
        if (win[i] > win[i-1] && win[i] > win[i+1] &&
            win[i] - sig_min > prominence_thr &&
            (i - last_peak) >= RIIV_MIN_PEAK_SEP) {
            if (n_peaks < 50) peak_idx[n_peaks++] = i;
            last_peak = i;
        }
    }

    float rr_bpm = NAN;
    uint8_t rr_quality = 0;

    if (n_peaks >= 2) {
        float sum_rr = 0.0f;
        int   n_valid = 0;
        for (int k = 0; k + 1 < n_peaks; k++) {
            int gap = peak_idx[k+1] - peak_idx[k];
            if (gap >= RIIV_MIN_PEAK_SEP && gap <= RIIV_MAX_PEAK_SEP) {
                sum_rr += (float)RIIV_FS_HZ * 60.0f / (float)gap;
                n_valid++;
            }
        }
        if (n_valid > 0) {
            rr_bpm = sum_rr / (float)n_valid;
            if (rr_bpm < 4.0f || rr_bpm > 40.0f) {
                rr_bpm = NAN;
            } else {
                /* Quality: scale by n_valid / expected peaks in 30 s.
                 * At 15 br/min × 30 s = 7.5 peaks → 7 expected gaps. */
                int expected_gaps = (int)(rr_bpm / 60.0f * 30.0f);
                if (expected_gaps < 1) expected_gaps = 1;
                rr_quality = (uint8_t)((n_valid * 100) / (expected_gaps + 1));
                if (rr_quality > 100) rr_quality = 100;
            }
        }
    }

    r_block_t r_blk;
    memset(&r_blk, 0, sizeof(r_blk));
    r_blk.header.type        = BLOCK_TYPE_R;
    r_blk.header.version     = BLOCK_VERSION;
    r_blk.header.payload_len = (uint16_t)(sizeof(r_blk) - sizeof(vuams_block_header_t));
    r_blk.header.timestamp_us = esp_timer_get_time();
    r_blk.rr_bpm        = rr_bpm;
    r_blk.rr_valid      = isnan(rr_bpm) ? 0x00 : 0x01;
    r_blk.rr_quality    = isnan(rr_bpm) ? 0xFF : rr_quality;
    r_blk.rr_method     = 0x00;   /* PPG-RIIV */
    r_blk.rr_conflict   = 0x00;
    if (!isnan(rr_bpm)) {
        r_blk.rr_confounder = (rr_bpm < 12.0f || rr_bpm > 20.0f) ? 0x01 : 0x00;
        r_blk.rr_caution    = (rr_bpm <  8.0f || rr_bpm > 22.0f) ? 0x01 : 0x00;
    }

    dispatch_block(&r_blk, sizeof(r_block_t), BLOCK_TYPE_R);
}

/* --------------------------------------------------------------------------
 * Internal: write a block to PSRAM and optionally forward to output queues
 * -------------------------------------------------------------------------- */

static void dispatch_block(const void *block_ptr, size_t block_size, uint8_t type)
{
    /* Write to PSRAM ring buffer */
    size_t written = psram_buf_write(&g_psram_buf, block_ptr, block_size);
    if (written != block_size) {
        ESP_LOGW(TAG, "PSRAM write incomplete: %u/%u bytes, type=0x%02X",
                 (unsigned)written, (unsigned)block_size, type);
    }

    /* Forward to SD writer */
    /* TODO: allocate from a pool allocator instead of heap for determinism */
    void *sd_copy = malloc(block_size);
    if (sd_copy) {
        memcpy(sd_copy, block_ptr, block_size);
        if (xQueueSend(g_sd_write_queue, &sd_copy, pdMS_TO_TICKS(2)) != pdTRUE) {
            ESP_LOGW(TAG, "sd_write_queue full — block 0x%02X dropped", type);
            free(sd_copy);
        }
    }

    /* Forward to BLE streamer (separate copy — different lifetime) */
    void *ble_copy = malloc(block_size);
    if (ble_copy) {
        memcpy(ble_copy, block_ptr, block_size);
        if (xQueueSend(g_ble_tx_queue, &ble_copy, 0) != pdTRUE) {
            /* BLE queue is best-effort: silently drop if full */
            free(ble_copy);
        }
    }
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void task_block_assembler_init(void)
{
    /* Output queues are owned by task_sd_writer and task_ble_stream;
     * those inits must be called before this one. */

    /* Initialise cardiac analysers */
    ecg_analyzer_init(&s_ecg_an, CARDIAC_SAMPLE_RATE_HZ);
    icg_analyzer_init(&s_icg_an, CARDIAC_SAMPLE_RATE_HZ);
    s_beat_count  = 0;
    s_last_rri_ms = 0.0f;

    /* Initialise PPG-RIIV respiratory rate state */
    memset(s_riiv_buf, 0, sizeof(s_riiv_buf));
    s_riiv_write = 0;
    s_riiv_count = 0;
    s_riiv_step  = 0;

    ESP_LOGI(TAG, "Block assembler init done (cardiac analysis enabled)");
}

/* --------------------------------------------------------------------------
 * Task
 * -------------------------------------------------------------------------- */

void task_block_assembler(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "Block assembler task started on core %d", xPortGetCoreID());

    while (1) {
        bool did_work = false;

        /* Drain ADC (A-blocks) ------------------------------------------------
         * Each A-block carries 250 ECG samples (250 ms) at 1 kHz plus the
         * ICG differential is carried by g_z_block_queue (Z-blocks).
         *
         * We process the A-block ECG samples one by one through the cardiac
         * analyser.  The ICG analyser runs in the Z-block drain below, but
         * needs the R-peak flag that ecg_analyzer_feed produces.  Because the
         * ECG and ICG streams are captured synchronously (same ADS1256 cycle),
         * they are naturally aligned — the R-peak flag set here is picked up
         * by the ICG path in the Z-block drain that follows in the same
         * task-loop iteration.
         *
         * To propagate the R-peak flag across the two drain loops we use a
         * one-sample "latch": s_r_peak_pending is set when an R-peak is
         * detected and consumed by the first ICG sample that follows.
         * ------------------------------------------------------------------- */
        a_block_t *a_blk = NULL;
        while (xQueueReceive(g_adc_block_queue, &a_blk, 0) == pdTRUE) {
            dispatch_block(a_blk, sizeof(a_block_t), BLOCK_TYPE_A);

            /* --- Feed ECG samples into R-peak detector -------------------- */
            for (int s = 0; s < 250; s++) {
                float    rri_ms   = 0.0f;
                bool     r_peak   = ecg_analyzer_feed(&s_ecg_an,
                                                      a_blk->ecg1[s],
                                                      &rri_ms);
                if (r_peak) {
                    s_last_rri_ms   = rri_ms;
                    s_r_peak_latch  = true;   /* consumed by Z-block drain */
                }
            }

            free(a_blk);
            a_blk    = NULL;
            did_work = true;
        }

        /* Drain IMU (M-blocks) */
        m_block_t *m_blk = NULL;
        while (xQueueReceive(g_imu_block_queue, &m_blk, 0) == pdTRUE) {
            dispatch_block(m_blk, sizeof(m_block_t), BLOCK_TYPE_M);

            /* Forward a copy to the HAR task's dedicated input queue.
             * HAR runs at lower priority on the same core, so this copy is
             * safe.  Drop silently if the HAR queue is full (HAR is behind). */
            if (g_har_m_block_queue != NULL) {
                m_block_t *har_copy = (m_block_t *)malloc(sizeof(m_block_t));
                if (har_copy != NULL) {
                    memcpy(har_copy, m_blk, sizeof(m_block_t));
                    if (xQueueSend(g_har_m_block_queue, &har_copy, 0) != pdTRUE) {
                        free(har_copy); /* HAR is behind — drop */
                    }
                }
            }

            free(m_blk);
            m_blk = NULL;
            did_work = true;
        }

        /* Drain PPG (P-blocks) */
        p_block_t *p_blk = NULL;
        while (xQueueReceive(g_ppg_block_queue, &p_blk, 0) == pdTRUE) {
            dispatch_block(p_blk, sizeof(p_block_t), BLOCK_TYPE_P);

            /* Feed IR sample into PPG-RIIV bandpass filter */
            if (p_blk->ppg_valid) {
                float filtered = biquad_cascade(s_riiv_sos, 2, (float)p_blk->ppg_ir);
                s_riiv_buf[s_riiv_write % RIIV_WINDOW_SAMPLES] = filtered;
                s_riiv_write++;
                if (s_riiv_count < RIIV_WINDOW_SAMPLES) s_riiv_count++;
                s_riiv_step++;

                /* Emit R-block every RIIV_STEP_SAMPLES (5 seconds) */
                if (s_riiv_step >= RIIV_STEP_SAMPLES && s_riiv_count >= RIIV_WINDOW_SAMPLES) {
                    s_riiv_step = 0;
                    emit_r_block();
                }
            } else {
                /* PPG contact lost — reset filter state so transient on re-connect
                 * doesn't corrupt the bandpass state variables */
                for (int i = 0; i < 2; i++) {
                    s_riiv_sos[i].z1 = 0.0;
                    s_riiv_sos[i].z2 = 0.0;
                }
                s_riiv_count = 0;
                s_riiv_write = 0;
                s_riiv_step  = 0;
            }

            free(p_blk);
            p_blk = NULL;
            did_work = true;
        }

        /* Drain Temperature (T-blocks) */
        t_block_t *t_blk = NULL;
        while (xQueueReceive(g_temp_block_queue, &t_blk, 0) == pdTRUE) {
            dispatch_block(t_blk, sizeof(t_block_t), BLOCK_TYPE_T);
            free(t_blk);
            t_blk = NULL;
            did_work = true;
        }

        /* Drain SCL (S-blocks) */
        s_block_t *s_blk = NULL;
        while (xQueueReceive(g_scl_block_queue, &s_blk, 0) == pdTRUE) {
            dispatch_block(s_blk, sizeof(s_block_t), BLOCK_TYPE_S);
            free(s_blk);
            s_blk = NULL;
            did_work = true;
        }

        /* Drain Barometer (B-blocks) — SD only, not forwarded to BLE.
         * HAR stair detection requires barometric altitude; BLE transmission
         * is omitted because the block is low-rate and BLE bandwidth is reserved
         * for physiological signals.                                            */
        b_block_t *b_blk = NULL;
        while (xQueueReceive(g_baro_block_queue, &b_blk, 0) == pdTRUE) {
            /* Write to PSRAM ring buffer */
            size_t written = psram_buf_write(&g_psram_buf, b_blk, sizeof(b_block_t));
            if (written != sizeof(b_block_t)) {
                ESP_LOGW(TAG, "PSRAM write incomplete: %u/%u bytes, type=B",
                         (unsigned)written, (unsigned)sizeof(b_block_t));
            }

            /* Forward to SD writer only */
            void *sd_copy = malloc(sizeof(b_block_t));
            if (sd_copy) {
                memcpy(sd_copy, b_blk, sizeof(b_block_t));
                if (xQueueSend(g_sd_write_queue, &sd_copy, pdMS_TO_TICKS(2)) != pdTRUE) {
                    ESP_LOGW(TAG, "sd_write_queue full — B-block dropped");
                    free(sd_copy);
                }
            }

            /* Forward pressure float to HAR's barometer history queue.
             * This avoids HAR competing with the assembler for g_baro_block_queue. */
            if (g_har_baro_queue != NULL) {
                float p = b_blk->baro_pressure_pa;
                if (xQueueSend(g_har_baro_queue, &p, 0) != pdTRUE) {
                    /* HAR queue full — oldest baro reading will be stale; acceptable */
                }
            }

            free(b_blk);
            b_blk = NULL;
            did_work = true;
        }

        /* Drain Z-blocks (raw ICG waveform) — SD only, not forwarded to BLE.
         * Each Z-block also drives the ICG analyser to produce I-blocks.     */
        z_block_t *z_blk = NULL;
        while (xQueueReceive(g_z_block_queue, &z_blk, 0) == pdTRUE) {
            /* Write to PSRAM ring buffer */
            size_t written = psram_buf_write(&g_psram_buf, z_blk, sizeof(z_block_t));
            if (written != sizeof(z_block_t)) {
                ESP_LOGW(TAG, "PSRAM write incomplete: %u/%u bytes, type=Z",
                         (unsigned)written, (unsigned)sizeof(z_block_t));
            }

            /* Forward to SD writer only */
            void *sd_copy = malloc(sizeof(z_block_t));
            if (sd_copy) {
                memcpy(sd_copy, z_blk, sizeof(z_block_t));
                if (xQueueSend(g_sd_write_queue, &sd_copy, pdMS_TO_TICKS(2)) != pdTRUE) {
                    ESP_LOGW(TAG, "sd_write_queue full — Z-block dropped");
                    free(sd_copy);
                }
            }

            /* --- Feed ICG samples into haemodynamic analyser -------------- *
             * z_raw[i] = ch3_sample - ch4_sample (differential ICG, 24-bit)
             *
             * The R-peak latch (s_r_peak_latch) is passed into the first
             * sample of this block if it was set by the ECG drain above, then
             * cleared.  This is an approximation: within a 250 ms block the
             * true R-peak sample offset is not preserved.  The resulting
             * timing error is at most ±125 ms — acceptable for a PEP
             * measurement that is not safety-critical.                        */
            for (int s = 0; s < Z_BLOCK_SAMPLES; s++) {
                bool r_flag = s_r_peak_latch;
                if (s_r_peak_latch) {
                    s_r_peak_latch = false;  /* consume latch on first sample */
                }

                icg_beat_t beat;
                bool beat_ready = icg_analyzer_feed(&s_icg_an,
                                                    z_blk->z_raw[s],
                                                    r_flag,
                                                    &beat);
                if (beat_ready) {
                    /* ---- Emit I-block ------------------------------------- */
                    i_block_t i_blk;
                    memset(&i_blk, 0, sizeof(i_blk));
                    i_blk.header.type        = BLOCK_TYPE_I;
                    i_blk.header.version     = BLOCK_VERSION;
                    i_blk.header.payload_len = (uint16_t)(sizeof(i_blk) -
                                               sizeof(vuams_block_header_t));
                    i_blk.header.timestamp_us = esp_timer_get_time();
                    i_blk.pep_ms    = beat.pep_ms;
                    i_blk.lvet_ms   = beat.lvet_ms;
                    i_blk.dZdt_peak = beat.dZdt_peak;
                    i_blk.z0        = beat.z0;

                    /* Kubicek stroke-volume formula (Patterson 1989).
                     * rho = 135.0 Ω·cm; L = KUBICEK_L_CM (electrode distance).
                     * SV [mL] = rho × L² / Z0² × dZdt_peak × LVET_s
                     * CO [L/min] = SV [mL] × HR [bpm] / 1000 */
                    i_blk.sv_ml = (beat.z0 <= 0.0f || beat.lvet_ms <= 0.0f) ? NAN :
                        (135.0f * (KUBICEK_L_CM * KUBICEK_L_CM) /
                         (beat.z0 * beat.z0) *
                         beat.dZdt_peak *
                         (beat.lvet_ms / 1000.0f));
                    if (!isnan(i_blk.sv_ml) &&
                        (i_blk.sv_ml < 20.0f || i_blk.sv_ml > 200.0f)) {
                        i_blk.sv_ml = NAN;
                    }
                    i_blk.co_lpm = isnan(i_blk.sv_ml) ? NAN :
                        (i_blk.sv_ml * ecg_analyzer_hr_bpm(&s_ecg_an)) / 1000.0f;

                    dispatch_block(&i_blk, sizeof(i_block_t), BLOCK_TYPE_I);

                    /* ---- Accumulate beat count for Y-block ---------------- */
                    s_beat_count++;
                    if (s_beat_count >= Y_BLOCK_BEAT_INTERVAL) {
                        float rmssd  = ecg_analyzer_rmssd(&s_ecg_an);
                        float hr_bpm = ecg_analyzer_hr_bpm(&s_ecg_an);

                        y_block_t y_blk;
                        memset(&y_blk, 0, sizeof(y_blk));
                        y_blk.header.type        = BLOCK_TYPE_Y;
                        y_blk.header.version     = BLOCK_VERSION;
                        y_blk.header.payload_len = (uint16_t)(sizeof(y_blk) -
                                                   sizeof(vuams_block_header_t));
                        y_blk.header.timestamp_us = esp_timer_get_time();
                        y_blk.rmssd_ms    = rmssd;
                        y_blk.rri_last_ms = s_last_rri_ms;
                        y_blk.hr_ecg_bpm  = hr_bpm;
                        y_blk.beat_count  = (uint8_t)(s_beat_count > 30 ? 30 :
                                                       s_beat_count);
                        y_blk.reserved    = 0;

                        dispatch_block(&y_blk, sizeof(y_block_t), BLOCK_TYPE_Y);

                        ESP_LOGD(TAG, "Y-block: RMSSD=%.1f ms HR=%.1f bpm",
                                 (double)rmssd, (double)hr_bpm);

                        s_beat_count = 0;
                    }
                }
            }

            free(z_blk);
            z_blk    = NULL;
            did_work = true;
        }

        /* Drain X-blocks (HAR/SAD context annotation) — BLE + SD.
         * Generated every 2 s by task_har; 13 bytes on-wire.                        */
        x_block_t *x_blk = NULL;
        while (xQueueReceive(g_x_block_queue, &x_blk, 0) == pdTRUE) {
            dispatch_block(x_blk, sizeof(x_block_t), BLOCK_TYPE_X);
            free(x_blk);
            x_blk = NULL;
            did_work = true;
        }

        /* Drain V-blocks (high-ODR SAD accelerometer) — SD only, not forwarded to BLE.
         * 1 kHz accel, 100 samples per block, 6 kB/s sustained write load.          */
        v_block_t *v_blk = NULL;
        while (xQueueReceive(g_v_block_queue, &v_blk, 0) == pdTRUE) {
            /* Write to PSRAM ring buffer */
            size_t written = psram_buf_write(&g_psram_buf, v_blk, sizeof(v_block_t));
            if (written != sizeof(v_block_t)) {
                ESP_LOGW(TAG, "PSRAM write incomplete: %u/%u bytes, type=V",
                         (unsigned)written, (unsigned)sizeof(v_block_t));
            }

            /* Forward to SD writer only (BLE not used for 1 kHz stream) */
            void *sd_copy = malloc(sizeof(v_block_t));
            if (sd_copy) {
                memcpy(sd_copy, v_blk, sizeof(v_block_t));
                if (xQueueSend(g_sd_write_queue, &sd_copy, pdMS_TO_TICKS(2)) != pdTRUE) {
                    ESP_LOGW(TAG, "sd_write_queue full — V-block dropped");
                    free(sd_copy);
                }
            }

            /* Forward a copy to SAD's dedicated input queue */
            if (g_sad_v_block_queue != NULL) {
                v_block_t *sad_copy = (v_block_t *)malloc(sizeof(v_block_t));
                if (sad_copy != NULL) {
                    memcpy(sad_copy, v_blk, sizeof(v_block_t));
                    if (xQueueSend(g_sad_v_block_queue, &sad_copy, 0) != pdTRUE) {
                        free(sad_copy); /* SAD is behind — drop */
                    }
                }
            }

            free(v_blk);
            v_blk = NULL;
            did_work = true;
        }

        if (!did_work) {
            /* Nothing in any queue — yield briefly */
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}
