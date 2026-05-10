/* ==========================================================================
 * VU-AMS Dummy Firmware — Synthetic Signal Generators (implementation)
 * Author: Kai Müller
 * Date:   2026-05-09
 * ========================================================================== */

#include "synth_signals.h"
#include "dummy_config.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

/* Gaussian pulse: amplitude * exp(-(x-mu)^2 / (2*sigma^2)) */
static inline float gaussian(float x, float mu, float sigma, float amplitude)
{
    float d = (x - mu) / sigma;
    return amplitude * expf(-0.5f * d * d);
}

/* Phase within current beat, normalised to [0, 1).
 * t_us: absolute time in microseconds.
 */
static inline float beat_phase(uint32_t t_us)
{
    /* Use 64-bit arithmetic to avoid overflow on long runs */
    uint64_t t = (uint64_t)t_us;
    uint64_t period = DUMMY_BEAT_PERIOD_US;          /* ≈ 882352 µs */
    uint64_t pos    = t % period;
    return (float)pos / (float)period;               /* [0, 1) */
}

/* Simple LCG for lightweight noise — not thread-safe but close enough */
static uint32_t s_lcg_state = 0xDEADBEEFu;

static inline int32_t lcg_rand(void)
{
    s_lcg_state = s_lcg_state * 1664525u + 1013904223u;
    return (int32_t)(s_lcg_state >> 16) - 32768;    /* roughly ±32768 */
}

/* Scaled noise in range [-range, +range] */
static inline int16_t noise(int range)
{
    if (range == 0) { return 0; }
    int32_t r = lcg_rand();
    return (int16_t)(r % (range * 2 + 1));
}

/* --------------------------------------------------------------------------
 * ECG — PQRST sum-of-Gaussians
 *
 * Phases (within one beat, normalised 0–1):
 *   P  wave : centre 0.15, sigma 0.03, amplitude +60
 *   Q  dip  : centre 0.32, sigma 0.01, amplitude -30
 *   R  peak : centre 0.35, sigma 0.012, amplitude +1000
 *   S  dip  : centre 0.38, sigma 0.01, amplitude -80
 *   T  wave : centre 0.55, sigma 0.045, amplitude +200
 * -------------------------------------------------------------------------- */
int16_t synth_ecg_sample(uint32_t t_us)
{
    float ph = beat_phase(t_us);

    float v = 0.0f;
    v += gaussian(ph, 0.15f, 0.030f,  60.0f);   /* P  */
    v += gaussian(ph, 0.32f, 0.010f, -30.0f);   /* Q  */
    v += gaussian(ph, 0.35f, 0.012f, 1000.0f);  /* R  */
    v += gaussian(ph, 0.38f, 0.010f, -80.0f);   /* S  */
    v += gaussian(ph, 0.55f, 0.045f, 200.0f);   /* T  */

    int32_t sample = (int32_t)v + noise(5);

    /* Clamp to int16 range */
    if (sample >  32767) { sample =  32767; }
    if (sample < -32768) { sample = -32768; }
    return (int16_t)sample;
}

/* --------------------------------------------------------------------------
 * ICG — dZ/dt-like waveform
 *
 * Positive baseline ~500 LSB.
 * Sharp negative dip at the R-peak position (phase 0.35):
 *   - Onset:  Gaussian negative at 0.33, sigma 0.008, amplitude -400
 *   - Return: Gaussian positive  at 0.42, sigma 0.015, amplitude +150
 * -------------------------------------------------------------------------- */
int16_t synth_icg_sample(uint32_t t_us)
{
    float ph = beat_phase(t_us);

    float v = 500.0f;                                         /* baseline Z0 */
    v += gaussian(ph, 0.33f, 0.008f, -400.0f);               /* dZ/dt dip    */
    v += gaussian(ph, 0.42f, 0.015f,  150.0f);               /* return       */

    int32_t sample = (int32_t)v + noise(8);

    if (sample >  32767) { sample =  32767; }
    if (sample < -32768) { sample = -32768; }
    return (int16_t)sample;
}

/* --------------------------------------------------------------------------
 * IMU — gentle random walk per axis
 *
 * State for nine axes: ax, ay, az (accel), gx, gy, gz (gyro), mx, my, mz (mag).
 * az includes +980 for gravity (1 g ≈ 980 LSB at ±4 g / 16384 LSB/g scale
 * would be 4096, but here we use a simplified ±2 g scale: 1 g ≈ 980 raw).
 * -------------------------------------------------------------------------- */
static int16_t s_imu_state[9] = {0, 0, 980, 0, 0, 0, 0, 0, 0};

int16_t synth_imu_sample(uint8_t axis)
{
    if (axis >= 9) { return 0; }

    /* Random walk step: ±4 LSB per sample */
    int32_t v = (int32_t)s_imu_state[axis] + noise(4);

    /* Soft clamp: gravity axis stays near +980, others near 0 */
    int16_t centre = (axis == 2) ? 980 : 0;
    int32_t max_dev = 20;
    if (v > centre + max_dev) { v = centre + max_dev; }
    if (v < centre - max_dev) { v = centre - max_dev; }

    s_imu_state[axis] = (int16_t)v;
    return s_imu_state[axis];
}

/* --------------------------------------------------------------------------
 * PPG — sinusoidal at heart rate, phase-delayed from R-peak
 *
 * Phase delay ≈ 100 ms / BEAT_PERIOD_US  ≈ 0.113 of a beat.
 * Amplitude 800 LSB, DC offset 2000 LSB.
 * -------------------------------------------------------------------------- */
int16_t synth_ppg_sample(uint32_t t_us)
{
    float ph = beat_phase(t_us);

    /* Shift phase: PPG foot lags R-peak by ~0.113 beats */
    float ph_ppg = ph - 0.113f;
    if (ph_ppg < 0.0f) { ph_ppg += 1.0f; }

    /* Use (1 - cosine)/2 for a smooth pulse shape, peak at ph=0 of the shifted frame */
    float v = 2000.0f + 800.0f * (1.0f - cosf(2.0f * (float)M_PI * ph_ppg)) * 0.5f;

    int32_t sample = (int32_t)v + noise(10);
    if (sample >  32767) { sample =  32767; }
    if (sample <      0) { sample =      0; }
    return (int16_t)sample;
}

/* --------------------------------------------------------------------------
 * SCL — slow 0.05 Hz modulated baseline
 *
 * Baseline 5000 LSB, ±200 LSB sine at 0.05 Hz.
 * t_us wraps at 2^32 ≈ 71 min which is fine for testing.
 * -------------------------------------------------------------------------- */
int16_t synth_scl_sample(uint32_t t_us)
{
    /* 0.05 Hz => period = 20 s = 20 000 000 µs */
    float t_s   = (float)t_us * 1e-6f;
    float v     = 5000.0f + 200.0f * sinf(2.0f * (float)M_PI * 0.05f * t_s);

    int32_t sample = (int32_t)v + noise(3);
    if (sample >  32767) { sample =  32767; }
    if (sample < -32768) { sample = -32768; }
    return (int16_t)sample;
}

/* --------------------------------------------------------------------------
 * Temperature — 36.8 °C ± 0.02 °C
 * -------------------------------------------------------------------------- */
float synth_temp_celsius(void)
{
    /* noise in range ±0.02: lcg gives ±32768, scale to ±0.02 */
    float jitter = (float)lcg_rand() * (0.02f / 32768.0f);
    return 36.8f + jitter;
}
