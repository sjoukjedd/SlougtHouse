#pragma once

/* ==========================================================================
 * VU-AMS Firmware — ECG Analysis: R-peak detection, RRI, RMSSD
 * Author: Kai Müller
 * Date:   2026-05-11
 *
 * SAFETY NOTE: This module produces wellness/research metrics only.
 * It is NOT a clinical alarm and has NOT been validated for diagnostic use.
 * Do not use for safety-critical decisions.
 * ========================================================================== */

#include <stdint.h>
#include <stdbool.h>

/* Number of RRI values in the ring buffer (must be >= RMSSD_WINDOW + 2) */
#define ECG_RRI_BUF_SIZE    32

/* RMSSD is computed over this many successive valid RRIs */
#define ECG_RMSSD_WINDOW    30

/* --------------------------------------------------------------------------
 * Analyser state — all fields are private; do not access directly.
 * Declared here so callers can allocate it on the stack or as a static.
 * -------------------------------------------------------------------------- */
typedef struct {
    /* Sample rate, stored for threshold/blanking calculations */
    float   sample_rate_hz;

    /* --- IIR filter state (high-pass + low-pass cascade) --- */
    /* 2-pole high-pass, fc = 5 Hz */
    float   hp_x1, hp_x2;   /* delayed inputs  */
    float   hp_y1, hp_y2;   /* delayed outputs */
    /* 2-pole low-pass, fc = 15 Hz */
    float   lp_x1, lp_x2;   /* delayed inputs  */
    float   lp_y1, lp_y2;   /* delayed outputs */

    /* --- Derivative + squaring state --- */
    float   deriv_prev;      /* one-sample delay for finite-difference derivative */

    /* --- Adaptive threshold --- */
    float   threshold;       /* current detection threshold (squared signal units) */
    bool    threshold_init;  /* true once the threshold has been seeded             */
    float   init_peak;       /* running max during the first-second seeding phase   */
    int32_t init_samples;    /* sample counter for the seeding phase                */

    /* --- Blanking period --- */
    int32_t blank_samples;   /* samples remaining in blanking window (0 = not blank) */
    int32_t blank_period;    /* blanking period in samples (= 0.25 s × Fs)           */

    /* --- RRI ring buffer --- */
    float   rri_buf[ECG_RRI_BUF_SIZE];  /* circular buffer of valid RRI values [ms] */
    uint8_t rri_head;                   /* write index                               */
    uint8_t rri_count;                  /* number of valid entries (max ECG_RRI_BUF_SIZE) */

    /* --- Timestamp of last R-peak (in samples) --- */
    int64_t last_peak_sample;
    int64_t sample_counter;
} ecg_analyzer_t;

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief  Initialise the ECG analyser state.
 *         Must be called once before any calls to ecg_analyzer_feed.
 * @param  a               Pointer to the analyser struct (caller owns memory).
 * @param  sample_rate_hz  ADC sample rate in Hz (typically 1000.0f).
 */
void ecg_analyzer_init(ecg_analyzer_t *a, float sample_rate_hz);

/**
 * @brief  Feed one ECG sample into the analyser.
 *
 * @param  a        Analyser state.
 * @param  sample   Raw 24-bit ECG ADC value (sign-extended to int32_t).
 * @param  rri_ms   Output: RRI in milliseconds from the previous R-peak to
 *                  this one.  Only valid when the function returns true.
 * @return true if an R-peak was detected on this sample (rri_ms is valid).
 *         false otherwise.
 */
bool ecg_analyzer_feed(ecg_analyzer_t *a, int32_t sample, float *rri_ms);

/**
 * @brief  Return RMSSD computed over the last ECG_RMSSD_WINDOW valid RRIs.
 *
 * @param  a  Analyser state (read-only).
 * @return RMSSD in milliseconds, or NaN if fewer than 2 valid RRIs are available.
 */
float ecg_analyzer_rmssd(const ecg_analyzer_t *a);

/**
 * @brief  Return the instantaneous heart rate derived from the last RRI.
 *
 * @param  a  Analyser state (read-only).
 * @return Heart rate in bpm, or NaN if no valid RRI is available.
 */
float ecg_analyzer_hr_bpm(const ecg_analyzer_t *a);
