/* ==========================================================================
 * VU-AMS Firmware — ECG Analysis: R-peak detection, RRI, RMSSD
 * Author: Kai Müller
 * Date:   2026-05-11
 *
 * Algorithm: Pan-Tompkins-lite (simplified adaptive threshold detector).
 *   1. 2-pole IIR bandpass (HP 5 Hz + LP 15 Hz) for QRS enhancement.
 *   2. Finite-difference derivative to enhance rapid QRS slope.
 *   3. Squaring to make all values positive and emphasise large slopes.
 *   4. Adaptive threshold: seeded from the first-second peak, then updated
 *      as  threshold = 0.875 * threshold + 0.125 * last_peak  after each
 *      detection.
 *   5. 250 ms blanking window after each detection to suppress re-triggers.
 *   6. RRI artefact rejection: discard intervals outside [300, 2000] ms.
 *   7. 32-sample RRI ring buffer; RMSSD computed over last 30 valid RRIs.
 *
 * SAFETY NOTE: This module produces wellness/research metrics only.
 * It is NOT a clinical alarm and has NOT been validated for diagnostic use.
 * Do not use for safety-critical decisions.
 * ========================================================================== */

#include "ecg_analysis.h"
#include <string.h>
#include <math.h>

/* --------------------------------------------------------------------------
 * IIR filter coefficients (bilinear transform, fs = 1000 Hz)
 *
 * High-pass Butterworth 2-pole, fc = 5 Hz
 *   Generated with: scipy.signal.butter(2, 5/500, btype='high')
 *   b = [0.98611194, -1.97222388,  0.98611194]
 *   a = [1.0,        -1.97223372,  0.97235540]
 * -------------------------------------------------------------------------- */
#define HP_B0   ( 0.98611194f)
#define HP_B1   (-1.97222388f)
#define HP_B2   ( 0.98611194f)
#define HP_A1   (-1.97223372f)
#define HP_A2   ( 0.97235540f)

/* Low-pass Butterworth 2-pole, fc = 15 Hz
 *   Generated with: scipy.signal.butter(2, 15/500, btype='low')
 *   b = [0.00055434, 0.00110869, 0.00055434]
 *   a = [1.0,       -1.77863178, 0.80084916]
 * -------------------------------------------------------------------------- */
#define LP_B0   (0.00055434f)
#define LP_B1   (0.00110869f)
#define LP_B2   (0.00055434f)
#define LP_A1   (-1.77863178f)
#define LP_A2   ( 0.80084916f)

/* Seeding phase duration (samples).  At 1 kHz this is 1 second. */
#define SEED_DURATION_SAMPLES   1000

/* Adaptive threshold update weights */
#define THRESH_DECAY   0.875f
#define THRESH_GAIN    0.125f

/* RRI validity limits [ms] */
#define RRI_MIN_MS   300.0f
#define RRI_MAX_MS   2000.0f

/* --------------------------------------------------------------------------
 * Inline helpers
 * -------------------------------------------------------------------------- */

static inline float apply_hp(ecg_analyzer_t *a, float x)
{
    float y = HP_B0 * x + HP_B1 * a->hp_x1 + HP_B2 * a->hp_x2
              - HP_A1 * a->hp_y1 - HP_A2 * a->hp_y2;
    a->hp_x2 = a->hp_x1;  a->hp_x1 = x;
    a->hp_y2 = a->hp_y1;  a->hp_y1 = y;
    return y;
}

static inline float apply_lp(ecg_analyzer_t *a, float x)
{
    float y = LP_B0 * x + LP_B1 * a->lp_x1 + LP_B2 * a->lp_x2
              - LP_A1 * a->lp_y1 - LP_A2 * a->lp_y2;
    a->lp_x2 = a->lp_x1;  a->lp_x1 = x;
    a->lp_y2 = a->lp_y1;  a->lp_y1 = y;
    return y;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void ecg_analyzer_init(ecg_analyzer_t *a, float sample_rate_hz)
{
    memset(a, 0, sizeof(*a));
    a->sample_rate_hz  = sample_rate_hz;
    a->threshold_init  = false;
    a->threshold       = 0.0f;
    a->init_peak       = 0.0f;
    a->init_samples    = 0;
    a->blank_period    = (int32_t)(0.25f * sample_rate_hz + 0.5f); /* 250 ms */
    a->blank_samples   = 0;
    a->last_peak_sample = -1;
    a->sample_counter  = 0;
    a->rri_head        = 0;
    a->rri_count       = 0;
}

bool ecg_analyzer_feed(ecg_analyzer_t *a, int32_t sample, float *rri_ms)
{
    bool detected = false;

    /* --- Step 1: bandpass filter ----------------------------------------- */
    float x   = (float)sample;
    float hp  = apply_hp(a, x);
    float bp  = apply_lp(a, hp);

    /* --- Step 2: derivative (finite difference) --------------------------- */
    float deriv = bp - a->deriv_prev;
    a->deriv_prev = bp;

    /* --- Step 3: squaring ------------------------------------------------- */
    float sq = deriv * deriv;

    a->sample_counter++;

    /* --- Step 4: threshold seeding phase (first SEED_DURATION_SAMPLES) --- */
    if (!a->threshold_init) {
        if (sq > a->init_peak) {
            a->init_peak = sq;
        }
        a->init_samples++;
        if (a->init_samples >= SEED_DURATION_SAMPLES) {
            a->threshold      = 0.5f * a->init_peak;
            a->threshold_init = true;
        }
        /* No detections until threshold is seeded */
        return false;
    }

    /* --- Step 5: blanking countdown --------------------------------------- */
    if (a->blank_samples > 0) {
        a->blank_samples--;
        return false;
    }

    /* --- Step 6: threshold crossing detection ----------------------------- */
    if (sq > a->threshold) {
        /* R-peak detected on this sample */

        /* Compute RRI */
        if (a->last_peak_sample >= 0) {
            int64_t delta_samples = a->sample_counter - a->last_peak_sample;
            float   rri = (float)delta_samples * 1000.0f / a->sample_rate_hz;

            /* Artefact rejection: keep only physiologically plausible RRIs */
            if (rri >= RRI_MIN_MS && rri <= RRI_MAX_MS) {
                /* Store in ring buffer */
                a->rri_buf[a->rri_head] = rri;
                a->rri_head = (a->rri_head + 1) % ECG_RRI_BUF_SIZE;
                if (a->rri_count < ECG_RRI_BUF_SIZE) {
                    a->rri_count++;
                }

                if (rri_ms != NULL) {
                    *rri_ms = rri;
                }
                detected = true;
            }
        }

        /* Update threshold (whether RRI was accepted or not) */
        a->threshold = THRESH_DECAY * a->threshold + THRESH_GAIN * sq;

        /* Record this peak position and start blanking */
        a->last_peak_sample = a->sample_counter;
        a->blank_samples    = a->blank_period;
    }

    return detected;
}

float ecg_analyzer_rmssd(const ecg_analyzer_t *a)
{
    /* Need at least 2 RRI values to form one successive difference */
    if (a->rri_count < 2) {
        return NAN;
    }

    /* Determine how many pairs we can compute over (up to RMSSD_WINDOW RRIs) */
    uint8_t n = (a->rri_count < ECG_RMSSD_WINDOW) ? a->rri_count : ECG_RMSSD_WINDOW;

    /* Walk the ring buffer backwards from the most recent entry.
     * rri_head points to the *next* write slot, so the most recent value is
     * at (rri_head - 1 + BUF_SIZE) % BUF_SIZE.                              */
    float sum_sq_diff = 0.0f;
    uint8_t pairs = 0;

    /* Collect the last n indices in chronological order */
    uint8_t indices[ECG_RMSSD_WINDOW];
    for (uint8_t i = 0; i < n; i++) {
        /* i=0 → most recent, i=n-1 → oldest of the window */
        int idx = (int)a->rri_head - 1 - (int)i;
        if (idx < 0) idx += ECG_RRI_BUF_SIZE;
        indices[n - 1 - i] = (uint8_t)idx;   /* store in chronological order */
    }

    for (uint8_t i = 1; i < n; i++) {
        float diff = a->rri_buf[indices[i]] - a->rri_buf[indices[i - 1]];
        sum_sq_diff += diff * diff;
        pairs++;
    }

    if (pairs == 0) {
        return NAN;
    }

    return sqrtf(sum_sq_diff / (float)pairs);
}

float ecg_analyzer_hr_bpm(const ecg_analyzer_t *a)
{
    if (a->rri_count == 0) {
        return NAN;
    }

    /* Most recent RRI is at (rri_head - 1) mod BUF_SIZE */
    int idx = (int)a->rri_head - 1;
    if (idx < 0) idx += ECG_RRI_BUF_SIZE;

    float rri = a->rri_buf[(uint8_t)idx];
    if (rri <= 0.0f) {
        return NAN;
    }

    return 60000.0f / rri;
}
