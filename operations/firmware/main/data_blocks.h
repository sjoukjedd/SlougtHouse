#pragma once

/* ==========================================================================
 * VU-AMS Firmware — Data Block Definitions
 * Target: ESP32-S3-MINI-1-N8R8
 * Author: Kai Müller
 * Date:   2026-05-08
 *
 * All blocks share a common header. Structs are packed so that sizeof()
 * matches exactly the on-wire and on-SD layout. Do not reorder fields.
 * ========================================================================== */

#include <stdint.h>

/* --------------------------------------------------------------------------
 * Block type identifiers (header.type)
 * -------------------------------------------------------------------------- */
#define BLOCK_TYPE_A    0x41    /* 'A' — ECG raw samples        */
#define BLOCK_TYPE_B    0x42    /* 'B' — Barometric pressure    */
#define BLOCK_TYPE_I    0x49    /* 'I' — ICG derived parameters */
#define BLOCK_TYPE_M    0x4D    /* 'M' — IMU raw samples        */
#define BLOCK_TYPE_P    0x50    /* 'P' — PPG                    */
#define BLOCK_TYPE_S    0x53    /* 'S' — SCL / EDA              */
#define BLOCK_TYPE_T    0x54    /* 'T' — Temperature            */
#define BLOCK_TYPE_V    0x56    /* 'V' — high-ODR accelerometer for SAD (SD only) */
#define BLOCK_TYPE_Z    0x5A    /* 'Z' — raw ICG impedance waveform (SD only) */

/* Current schema version */
#define BLOCK_VERSION   0x01

/* --------------------------------------------------------------------------
 * Common block header  (16 bytes)
 * -------------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    uint8_t  type;          /* BLOCK_TYPE_* above                          */
    uint8_t  version;       /* BLOCK_VERSION                               */
    uint16_t payload_len;   /* Bytes after this header                     */
    uint64_t timestamp_us;  /* esp_timer_get_time() at first sample in block */
} vuams_block_header_t;

/* --------------------------------------------------------------------------
 * A-block — ECG raw, 1 kHz, 2 channels, 250 samples per block
 * One block covers 250 ms of recording.
 * payload_len = 2 × 250 × 4 = 2000 bytes
 * -------------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    vuams_block_header_t header;
    int32_t ecg1[250];      /* ECG channel 1 — 24-bit ADS1256 value sign-extended to 32 bit */
    int32_t ecg2[250];      /* ECG channel 2                                                 */
} a_block_t;

/* --------------------------------------------------------------------------
 * I-block — ICG-derived haemodynamic parameters, one entry per heartbeat
 * Produced by the block_assembler from the raw ICG waveform.
 * payload_len = 6 × 4 = 24 bytes
 * -------------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    vuams_block_header_t header;
    float z0;           /* Baseline impedance [Ω]                    */
    float dZdt_peak;    /* Peak –dZ/dt amplitude [Ω/s]               */
    float pep_ms;       /* Pre-ejection period [ms]                  */
    float lvet_ms;      /* Left ventricular ejection time [ms]       */
    float co_lpm;       /* Cardiac output estimate [L/min]           */
    float sv_ml;        /* Stroke volume estimate [mL]               */
} i_block_t;

/* --------------------------------------------------------------------------
 * M-block — IMU raw, 100 Hz, 10 samples per block  (100 ms window)
 * payload_len = 9 channels × 10 samples × 2 bytes = 180 bytes
 * -------------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    vuams_block_header_t header;
    int16_t ax[10];     /* Accelerometer X [raw LSB] */
    int16_t ay[10];     /* Accelerometer Y            */
    int16_t az[10];     /* Accelerometer Z            */
    int16_t gx[10];     /* Gyroscope X [raw LSB]      */
    int16_t gy[10];     /* Gyroscope Y                */
    int16_t gz[10];     /* Gyroscope Z                */
    int16_t mx[10];     /* Magnetometer X [raw LSB]   */
    int16_t my[10];     /* Magnetometer Y             */
    int16_t mz[10];     /* Magnetometer Z             */
} m_block_t;

/* --------------------------------------------------------------------------
 * P-block — PPG (MAX30101), one block per poll cycle (~10 ms)
 * payload_len = 4+4+4+1+1 = 14 bytes
 * -------------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    vuams_block_header_t header;
    uint32_t ppg_red;       /* Red LED raw count                    */
    uint32_t ppg_ir;        /* IR LED raw count                     */
    float    spo2_pct;      /* SpO₂ estimate [%], NaN if invalid    */
    uint8_t  hr_ppg;        /* Heart rate from PPG [bpm], 0 = invalid */
    uint8_t  ppg_valid;     /* 1 if finger present and signal good  */
} p_block_t;

/* --------------------------------------------------------------------------
 * S-block — Skin Conductance Level / EDA (AD5933 impedance)
 * payload_len = 4+4+1 = 9 bytes
 * -------------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    vuams_block_header_t header;
    float   scl_tonic_uS;       /* Tonic SCL [µS]              */
    float   scl_phasic_uS;      /* Phasic component [µS]       */
    uint8_t electrode_contact;  /* 1 = good contact            */
} s_block_t;

/* --------------------------------------------------------------------------
 * T-block — Temperature (TMP117)
 * payload_len = 4+2 = 6 bytes
 * -------------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    vuams_block_header_t header;
    float   skin_temp_c;    /* Converted temperature [°C]           */
    int16_t temp_raw;       /* TMP117 raw register value            */
} t_block_t;

/* --------------------------------------------------------------------------
 * B-block — Barometric pressure / altitude (BMP390), 25 Hz
 * payload_len = 4+4 = 8 bytes
 * -------------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    vuams_block_header_t header;
    float baro_pressure_pa;  /* Compensated pressure [Pa]   */
    float baro_temp_c;       /* Compensated temperature [°C] */
} b_block_t;

/* --------------------------------------------------------------------------
 * Z-block — raw ICG impedance waveform, 250 samples @ 1 kHz (250 ms window)
 * SD-only: never forwarded to BLE.
 * z_raw[i] = ch3_sample - ch4_sample (ICG differential, 24-bit in int32_t)
 * payload_len = 250 × 4 + 2 (crc) = 1002 bytes  (crc is end-of-struct field)
 * -------------------------------------------------------------------------- */
#define Z_BLOCK_SAMPLES 250

typedef struct __attribute__((packed)) {
    uint8_t  block_type;                /* BLOCK_TYPE_Z = 0x5A                        */
    uint16_t seq;                       /* Rolling sequence number                     */
    uint32_t timestamp_us;              /* esp_timer_get_time() at first sample        */
    int32_t  z_raw[Z_BLOCK_SAMPLES];    /* raw differential ICG: ch3 - ch4, 24-bit    */
    uint16_t crc;                       /* CRC-16 over preceding bytes (future use)    */
} z_block_t;

/* --------------------------------------------------------------------------
 * V-block — high-ODR accelerometer for Speech Activity Detection (SAD)
 * 1 kHz, 100 samples per block (100 ms window), SD-only (never sent over BLE)
 * payload: 3 axes × 100 samples × 2 bytes = 600 bytes
 * -------------------------------------------------------------------------- */
#define V_BLOCK_SAMPLES 100

typedef struct __attribute__((packed)) {
    uint8_t  block_type;                 /* BLOCK_TYPE_V = 0x56                       */
    uint16_t seq;                        /* Rolling sequence number                    */
    uint32_t timestamp_us;               /* esp_timer_get_time() at first sample       */
    int16_t  ax[V_BLOCK_SAMPLES];        /* 1 kHz accelerometer X, raw 16-bit LSB      */
    int16_t  ay[V_BLOCK_SAMPLES];        /* 1 kHz accelerometer Y                      */
    int16_t  az[V_BLOCK_SAMPLES];        /* 1 kHz accelerometer Z                      */
    uint16_t crc;                        /* CRC-16 over preceding bytes (future use)   */
} v_block_t;

/* --------------------------------------------------------------------------
 * X-block — HAR/SAD context annotation, generated every 2 seconds by task_har
 * Published via BLE AND stored to SD (13 bytes on-wire).
 * -------------------------------------------------------------------------- */
#define BLOCK_TYPE_X    0x58    /* 'X' — context / annotation block          */

typedef struct __attribute__((packed)) {
    uint8_t  block_type;        /* BLOCK_TYPE_X = 0x58                       */
    uint16_t seq;               /* Rolling sequence number                    */
    uint32_t timestamp_us;      /* esp_timer_get_time() at block generation   */
    uint8_t  activity_class;    /* har_class_t value (0–8)                   */
    uint16_t cadence_spm;       /* Steps per minute × 10 (fixed point)       */
    uint8_t  motion_intensity;  /* 0–255, normalised from 0.0–1.0             */
    uint8_t  speaking;          /* 0 = not speaking, 1 = speaking            */
    uint8_t  speaking_fraction; /* Last 30 s speaking fraction, 0–100 (%)   */
    uint16_t crc;               /* CRC-16 over preceding bytes (placeholder) */
} x_block_t;

/* --------------------------------------------------------------------------
 * Generic block pointer — used by queue items
 * -------------------------------------------------------------------------- */
typedef struct {
    uint8_t type;           /* BLOCK_TYPE_* — identifies which union member is valid */
    union {
        a_block_t a;
        b_block_t b;
        i_block_t i;
        m_block_t m;
        p_block_t p;
        s_block_t s;
        t_block_t t;
    };
} vuams_block_t;
