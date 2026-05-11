/* ==========================================================================
 * VU-AMS Firmware — ICG Analysis: B-point detection, PEP, LVET
 * Author: Kai Müller
 * Date:   2026-05-11
 *
 * Algorithm (simplified B-point / X-point detection):
 *
 *   The raw ICG channel carries the differential impedance waveform Z(t).
 *   The derivative –dZ/dt is approximated by a finite difference over the
 *   buffered samples.  Key fiducial points in the –dZ/dt waveform:
 *
 *     B-point: local minimum of –dZ/dt in [40, 200] ms post-R.
 *              Represents aortic valve opening / onset of ejection.
 *              PEP = time from R-peak to B-point.
 *
 *     C-point: global maximum (peak) of –dZ/dt in the beat window.
 *              Used only to anchor the X-search window.
 *
 *     X-point: local minimum of –dZ/dt after C-point, within
 *              [150, 450] ms post-B.  Represents aortic valve closure.
 *              LVET = time from B-point to X-point.
 *
 *   z0 baseline: a very slow IIR low-pass (alpha ≈ 0.0001 per sample,
 *   equivalent to ~0.01 Hz at 1 kHz) tracking the DC level of the raw ICG.
 *
 * SAFETY NOTE: This module produces wellness/research metrics only.
 * It is NOT a clinical alarm and has NOT been validated for diagnostic use.
 * Do not use for safety-critical decisions.
 * ========================================================================== */

#include "icg_analysis.h"
#include <string.h>
#include <math.h>
#include <float.h>

/* IIR low-pass alpha for z0 baseline tracking.
 * tau = 1 / (2*pi*fc); fc = 0.01 Hz; alpha ≈ 1 - exp(-1/(fs*tau))
 * At fs=1000 Hz: alpha ≈ 0.0000628 — rounded to 1e-4 for simplicity.    */
#define Z0_ALPHA   1e-4f

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void icg_analyzer_init(icg_analyzer_t *a, float sample_rate_hz)
{
    memset(a, 0, sizeof(*a));
    a->sample_rate_hz = sample_rate_hz;
    a->capturing      = false;
    a->beat_len       = 0;
    a->z0_init        = false;
    a->z0_accum       = 0.0f;

    /* Pre-compute sample-domain limits from ms limits */
    a->b_start_samp = (int32_t)(ICG_B_SEARCH_START_MS * sample_rate_hz / 1000.0f + 0.5f);
    a->b_end_samp   = (int32_t)(ICG_B_SEARCH_END_MS   * sample_rate_hz / 1000.0f + 0.5f);
    a->x_start_samp = (int32_t)(ICG_X_SEARCH_START_MS * sample_rate_hz / 1000.0f + 0.5f);
    a->x_end_samp   = (int32_t)(ICG_X_SEARCH_END_MS   * sample_rate_hz / 1000.0f + 0.5f);

    /* Capture window: need enough samples to search for X-point after B.
     * Worst case: B at b_end (200 ms) + X at x_end (450 ms) = 650 ms.
     * Add a small margin: 700 ms = 700 samples at 1 kHz.                 */
    a->max_capture = a->b_end_samp + a->x_end_samp;
    if (a->max_capture > ICG_BEAT_BUF_SIZE) {
        a->max_capture = ICG_BEAT_BUF_SIZE;   /* clamp to buffer size */
    }
}

bool icg_analyzer_feed(icg_analyzer_t *a, int32_t icg_sample,
                       bool r_peak_flag, icg_beat_t *out)
{
    /* --- Update z0 baseline (runs every sample regardless of capture state) */
    if (!a->z0_init) {
        a->z0_accum = (float)icg_sample;
        a->z0_init  = true;
    } else {
        a->z0_accum += Z0_ALPHA * ((float)icg_sample - a->z0_accum);
    }

    /* --- R-peak: start a new capture window -------------------------------- */
    if (r_peak_flag) {
        /* Abandon any previous incomplete capture and start fresh */
        a->capturing = true;
        a->beat_len  = 0;
    }

    /* --- Fill capture buffer ----------------------------------------------- */
    if (a->capturing) {
        if (a->beat_len < ICG_BEAT_BUF_SIZE) {
            a->beat_buf[a->beat_len] = icg_sample;
            a->beat_len++;
        }

        /* Once we have collected enough samples, run the analysis */
        if (a->beat_len >= a->max_capture) {
            a->capturing = false;

            /* ---- Compute finite-difference derivative of ICG waveform ---- *
             * dzdt[i] = beat_buf[i+1] - beat_buf[i]  (in ADC counts/sample)
             * Scaled to counts/s by multiplying by sample_rate_hz.
             * We work in-place over local index variables to avoid extra heap.
             * dzdt lives logically at sample positions [0 .. max_capture-2].  */

            int32_t n = a->beat_len - 1;  /* number of derivative samples */

            /* ---- Find B-point (local minimum of dzdt in B window) --------- */
            int32_t b_idx   = -1;
            float   b_min   = FLT_MAX;
            int32_t b_start = a->b_start_samp;
            int32_t b_end   = a->b_end_samp;
            if (b_end   > n) b_end   = n;
            if (b_start > n) b_start = n;

            for (int32_t i = b_start; i < b_end; i++) {
                float dz = (float)(a->beat_buf[i + 1] - a->beat_buf[i])
                           * a->sample_rate_hz;
                if (dz < b_min) {
                    b_min = dz;
                    b_idx = i;
                }
            }

            if (b_idx < 0) {
                /* No B-point found — skip this beat */
                return false;
            }

            /* ---- Find C-point (global maximum of dzdt after B-point) ------ */
            int32_t c_idx   = -1;
            float   c_max   = -FLT_MAX;
            /* Search from B-point to end of buffer */
            for (int32_t i = b_idx; i < n; i++) {
                float dz = (float)(a->beat_buf[i + 1] - a->beat_buf[i])
                           * a->sample_rate_hz;
                if (dz > c_max) {
                    c_max = dz;
                    c_idx = i;
                }
            }

            if (c_idx < 0) {
                return false;
            }

            /* dZdt_peak = peak positive –dZ/dt amplitude (at C-point).
             * Convention: C-point is a peak in the positive direction of
             * the –dZ/dt waveform.                                          */
            float dZdt_peak = c_max;

            /* ---- Find X-point (local minimum of dzdt after C, within
             *      [x_start, x_end] samples after B-point) ------------------ */
            int32_t x_search_start = b_idx + a->x_start_samp;
            int32_t x_search_end   = b_idx + a->x_end_samp;
            if (x_search_start < c_idx) x_search_start = c_idx;  /* must be after C */
            if (x_search_end   > n)     x_search_end   = n;

            int32_t x_idx = -1;
            float   x_min = FLT_MAX;

            for (int32_t i = x_search_start; i < x_search_end; i++) {
                float dz = (float)(a->beat_buf[i + 1] - a->beat_buf[i])
                           * a->sample_rate_hz;
                if (dz < x_min) {
                    x_min = dz;
                    x_idx = i;
                }
            }

            if (x_idx < 0) {
                return false;
            }

            /* ---- Populate output ------------------------------------------ */
            out->pep_ms    = (float)b_idx * 1000.0f / a->sample_rate_hz;
            out->lvet_ms   = (float)(x_idx - b_idx) * 1000.0f / a->sample_rate_hz;
            out->dZdt_peak = dZdt_peak;
            out->z0        = a->z0_accum;

            return true;
        }
    }

    return false;
}
