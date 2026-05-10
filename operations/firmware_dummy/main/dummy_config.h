#pragma once

/* ==========================================================================
 * VU-AMS Dummy Firmware — Configuration
 * Target: ESP32-S3-DevKitC-1
 * Author: Kai Müller
 * Date:   2026-05-09
 *
 * GATT UUIDs are identical to the real firmware so the iOS app cannot
 * distinguish this device from genuine VU-AMS hardware.
 * ========================================================================== */

/* --------------------------------------------------------------------------
 * GATT service and characteristic UUIDs — copied verbatim from config.h
 * -------------------------------------------------------------------------- */
#define VUAMS_SERVICE_UUID      "A5D5B001-5A5A-4B4B-8888-1A2B3C4D5E6F"
#define VUAMS_A_BLOCK_UUID      "A5D5B002-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* ECG/ICG */
#define VUAMS_I_BLOCK_UUID      "A5D5B003-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* ICG derived */
#define VUAMS_M_BLOCK_UUID      "A5D5B004-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* IMU */
#define VUAMS_P_BLOCK_UUID      "A5D5B005-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* PPG */
#define VUAMS_S_BLOCK_UUID      "A5D5B006-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* SCL/EDA */
#define VUAMS_T_BLOCK_UUID      "A5D5B007-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* Temperature */
#define VUAMS_STATUS_UUID       "A5D5B009-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* Status */
#define VUAMS_CONTROL_UUID      "A5D5B00A-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* Control */

/* --------------------------------------------------------------------------
 * Synthetic signal parameters
 * -------------------------------------------------------------------------- */
#define DUMMY_ECG_RATE_HZ       1000
#define DUMMY_IMU_RATE_HZ       100
#define DUMMY_PPG_RATE_HZ       100
#define DUMMY_SCL_RATE_HZ       10

/* Heart rate used by all synthetic waveforms — must be consistent */
#define DUMMY_HEART_RATE_BPM    68

/* Derived: period between heartbeats in microseconds */
#define DUMMY_BEAT_PERIOD_US    (60000000U / DUMMY_HEART_RATE_BPM)   /* ≈ 882352 µs */
