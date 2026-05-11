#pragma once

/* ==========================================================================
 * VU-AMS Firmware — ICG Analysis: B-point detection, PEP, LVET
 * Author: Kai Müller
 * Date:   2026-05-11
 *
 * Computes per-beat impedance cardiography parameters:
 *   PEP  — Pre-ejection period (R-to-B interval) [ms]
 *   LVET — Left ventricular ejection time (B-to-X interval) [ms]
 *   dZdt_peak — peak –dZ/dt amplitude [ADC counts/s, scaled by sample rate]
 *   z0   — baseline impedance (low-pass rolling average of raw ICG)
 *
 * SAFETY NOTE: This module produces wellness/research metrics only.
 * It is NOT a clinical alarm and has NOT been validated for diagnostic use.
 * Do not use for safety-critical decisions.
 * ========================================================================== */

#include <stdint.h>
#include <stdbool.h>

/* Search window limits, all relative to R-peak onset [ms] ------------ */
#define ICG_B_SEARCH_START_MS    40    /* earliest B-point post-R */
#define ICG_B_SEARCH_END_MS     200    /* latest  B-point post-R  */
#define ICG_X_SEARCH_START_MS   150    /* earliest X-point post-B */
#define ICG_X_SEARCH_END_MS     450    /* latest  X-point post-B  */

/* Maximum number of samples buffered per beat window.
 * At 1 kHz: R + 650 ms > B + 450 ms so 700 samples is sufficient.    */
#define ICG_BEAT_BUF_SIZE       700

/* --------------------------------------------------------------------------
 * Output struct: one per completed beat
 * -------------------------------------------------------------------------- */
typedef struct {
    float pep_ms;       /* Pre-ejection period: R-to-B interval [ms]          */
    float lvet_ms;      /* Left ventricular ejection time: B-to-X interval [ms] */
    float dZdt_peak;    /* Peak –dZ/dt amplitude [ADC counts/s, scaled]        */
    float z0;           /* Baseline impedance [rolling average, ADC counts]    */
} icg_beat_t;

/* --------------------------------------------------------------------------
 * Analyser state — all fields are private; do not access directly.
 * -------------------------------------------------------------------------- */
typedef struct {
    float   sample_rate_hz;

    /* Beat capture buffer — filled sample by sample after an R-peak           */
    int32_t beat_buf[ICG_BEAT_BUF_SIZE];
    int32_t beat_len;       /* samples written so far in this beat window      */
    bool    capturing;      /* true when filling beat_buf after R-peak         */
    int32_t max_capture;    /* maximum samples to capture (= B_end + X_end)    */

    /* Rolling z0 baseline (IIR low-pass, alpha = 0.0001 per sample ≈ 0.01 Hz) */
    float   z0_accum;
    bool    z0_init;

    /* Pre-computed sample counts from ms limits and sample rate */
    int32_t b_start_samp;
    int32_t b_end_samp;
    int32_t x_start_samp;   /* relative to B-point index within beat_buf */
    int32_t x_end_samp;
} icg_analyzer_t;

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief  Initialise the ICG analyser.
 * @param  a               Analyser struct (caller owns memory).
 * @param  sample_rate_hz  Sample rate in Hz (must match ECG analyser).
 */
void icg_analyzer_init(icg_analyzer_t *a, float sample_rate_hz);

/**
 * @brief  Feed one ICG sample into the analyser.
 *
 * @param  a            Analyser state.
 * @param  icg_sample   Raw differential ICG ADC value (ch3 - ch4, 24-bit
 *                      sign-extended to int32_t).
 * @param  r_peak_flag  Set to true on the sample where an R-peak was detected
 *                      by ecg_analyzer_feed.
 * @param  out          Output struct.  Only valid when the function returns true.
 * @return true when a complete beat has been analysed and *out is populated.
 */
bool icg_analyzer_feed(icg_analyzer_t *a, int32_t icg_sample,
                       bool r_peak_flag, icg_beat_t *out);
