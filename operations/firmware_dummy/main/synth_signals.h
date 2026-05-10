#pragma once

/* ==========================================================================
 * VU-AMS Dummy Firmware — Synthetic Signal Generators
 * Author: Kai Müller
 * Date:   2026-05-09
 *
 * All generators are deterministic given t_us (microseconds since boot).
 * They share the same DUMMY_HEART_RATE_BPM heartbeat phase so waveforms
 * are physiologically consistent with each other.
 * ========================================================================== */

#include <stdint.h>

/**
 * @brief ECG sample at time t_us.
 *
 * Returns a realistic PQRST complex using a sum-of-Gaussians model at
 * DUMMY_HEART_RATE_BPM. R-peak amplitude ≈ 1000 LSB, baseline 0.
 * Noise ±5 LSB.
 *
 * @param t_us  esp_timer_get_time() value [µs]
 * @return      int16_t sample
 */
int16_t synth_ecg_sample(uint32_t t_us);

/**
 * @brief ICG (impedance cardiogram) sample at time t_us.
 *
 * Returns a dZ/dt-like waveform: slow positive baseline ~500 LSB with a
 * sharp negative dip at each R-peak.
 *
 * @param t_us  esp_timer_get_time() value [µs]
 * @return      int16_t sample
 */
int16_t synth_icg_sample(uint32_t t_us);

/**
 * @brief IMU accelerometer / gyroscope / magnetometer sample.
 *
 * Gentle random walk ±20 LSB. Axis 2 (Z, accelerometer) adds +980 for gravity.
 *
 * @param axis  0–8 mapping: ax=0,ay=1,az=2, gx=3,gy=4,gz=5, mx=6,my=7,mz=8
 * @return      int16_t raw LSB value
 */
int16_t synth_imu_sample(uint8_t axis);

/**
 * @brief PPG (photoplethysmography) sample at time t_us.
 *
 * Sinusoidal at DUMMY_HEART_RATE_BPM, amplitude 800 LSB, offset 2000 LSB.
 * Phase-delayed from R-peak by ~100 ms (systolic foot).
 *
 * @param t_us  esp_timer_get_time() value [µs]
 * @return      int16_t sample
 */
int16_t synth_ppg_sample(uint32_t t_us);

/**
 * @brief Skin Conductance Level (SCL/EDA) sample at time t_us.
 *
 * Slow drift: 5000 LSB baseline ±200 LSB with 0.05 Hz sine modulation.
 *
 * @param t_us  esp_timer_get_time() value [µs]
 * @return      int16_t sample
 */
int16_t synth_scl_sample(uint32_t t_us);

/**
 * @brief Skin temperature.
 *
 * Returns 36.8 °C ± 0.02 °C with small random noise.
 *
 * @return float temperature in Celsius
 */
float synth_temp_celsius(void);
