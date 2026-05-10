# VU-AMS BLE API Reference v0.1

**Document number:** VU-AMS-BLE-001  
**Revision:** 0.1 — First draft  
**Date:** 2026-05-09  
**Author:** Müller / Quinn (documentation)  
**Status:** Draft — pending Chen review and Lam sign-off

---

## Table of Contents

1. [Overview](#1-overview)
2. [GATT Service](#2-gatt-service)
3. [Characteristic Reference](#3-characteristic-reference)
   - 3.1 [A-Block: ECG/ICG Raw — A5D5B002](#31-a-block-ecgicg-raw--a5d5b002)
   - 3.2 [I-Block: ICG Derived — A5D5B003](#32-i-block-icg-derived--a5d5b003)
   - 3.3 [M-Block: IMU — A5D5B004](#33-m-block-imu--a5d5b004)
   - 3.4 [P-Block: PPG — A5D5B005](#34-p-block-ppg--a5d5b005)
   - 3.5 [S-Block: SCL/EDA — A5D5B006](#35-s-block-scleda--a5d5b006)
   - 3.6 [T-Block: Temperature — A5D5B007](#36-t-block-temperature--a5d5b007)
   - 3.7 [Status — A5D5B009](#37-status--a5d5b009)
   - 3.8 [Control — A5D5B00A](#38-control--a5d5b00a)
4. [Delta Encoding Scheme](#4-delta-encoding-scheme)
5. [Control Command Reference](#5-control-command-reference)
6. [Connection Sequence](#6-connection-sequence)
7. [Error Handling and Reliability](#7-error-handling-and-reliability)

---

## 1. Overview

The VU-AMS device exposes a single custom BLE (Bluetooth Low Energy) GATT service. The central device (iPhone, iPad) connects to this service to receive streaming physiological data and to issue recording control commands.

**BLE role:** VU-AMS is the GATT Server (Peripheral). The app is the GATT Client (Central).

**Transport:** Bluetooth Low Energy 5.0, operating in the 2.4 GHz ISM band. The VU-AMS firmware runs on an ESP32-S3-MINI-1-N8R8 (dual-core LX7 at 240 MHz).

**Throughput budget:** After delta encoding, the combined streaming data rate across all channels is approximately **1.6 kB/s** (see table below). This is well within the practical sustained throughput of BLE 5.0 at 2M PHY (~100 kB/s theoretical; ~20–30 kB/s practical). BLE 2M PHY is recommended where the iOS device supports it, to reduce per-packet overhead and connection event congestion. The firmware uses a 16-deep BLE block queue; if the queue overflows (e.g. due to sustained poor RSSI), older samples are dropped and the Status characteristic will reflect the `BLE_DROP` flag.

**Per-channel bandwidth estimate (encoded):**

| Channel | Rate | Encoded bytes/block | Blocks/s | kB/s |
|---------|------|---------------------|----------|------|
| A-Block (ECG, 2 ch) | 1 kHz | ~648 B (2 ch × 316 B + 16 B header) | 4 | ~2.5 |
| I-Block (ICG derived) | ~1 Hz | 40 B | ~1 | ~0.04 |
| M-Block (IMU, 9 ch) | 100 Hz | 115 B (99 B encoded + 16 B header) | 10 | ~1.1 |
| P-Block (PPG) | 100 Hz | 30 B | ~100 | ~3.0 |
| S-Block (SCL) | 10 Hz | 25 B | 10 | ~0.25 |
| T-Block (Temperature) | 1 Hz | 22 B | 1 | ~0.02 |
| **Total** | | | | **~7.0 kB/s raw; ~1.6 kB/s typical** |

> Note: The ~1.6 kB/s figure reflects typical ambulatory use, where clean ECG signals compress to ~4× and PPG is subsampled or streamed at reduced resolution. The worst-case (no compression benefit, all channels at maximum rate) is ~7 kB/s and remains within BLE capacity. App implementations should be designed to handle peaks at the higher figure.

**Byte order:** All multi-byte integer fields are **little-endian** unless otherwise noted.

**Floating-point:** All `float` fields are 32-bit IEEE 754, little-endian.

---

## 2. GATT Service

| Field | Value |
|-------|-------|
| Service name | VU-AMS Physiological Monitor |
| Service UUID | `A5D5B001-5A5A-4B4B-8888-1A2B3C4D5E6F` |
| Type | Custom 128-bit UUID |

The VU-AMS does not expose any standard GATT services (e.g. Heart Rate Service, Battery Service) in this version. All data is accessed exclusively through the custom service.

---

## 3. Characteristic Reference

### 3.1 A-Block: ECG/ICG Raw — A5D5B002

| Field | Value |
|-------|-------|
| UUID | `A5D5B002-5A5A-4B4B-8888-1A2B3C4D5E6F` |
| Name | VUAMS_A_BLOCK |
| Properties | NOTIFY |
| CCCD required | Yes — client must write 0x0001 to enable notifications |
| Update rate | 4 Hz (one notification every 250 ms) |
| Nominal payload | 16 bytes header + delta-encoded payload (variable length; see Section 4) |

**Description:** Carries 250 ms of raw ECG data from both ECG channels, sampled at 1000 Hz by the ADS1256 24-bit ADC. Each block contains 250 samples per channel, accumulated from the `int32_t` ADC values (24-bit values sign-extended to 32 bits). On the BLE interface, samples are delta-encoded to reduce payload size (see Section 4).

**Block header (16 bytes, common to all block types):**

| Byte offset | Field | Type | Description |
|-------------|-------|------|-------------|
| 0 | `type` | uint8 | Block type identifier: `0x41` ('A') |
| 1 | `version` | uint8 | Schema version: `0x01` |
| 2–3 | `payload_len` | uint16 LE | Number of bytes following this header |
| 4–11 | `timestamp_us` | uint64 LE | Device microsecond timer (`esp_timer_get_time()`) at first sample in block |

**A-block payload (follows header):**

The payload consists of two delta-encoded sequences, one per ECG channel (ECG1 followed by ECG2). Each sequence encodes 250 samples using the scheme described in Section 4. The first sample in each sequence is transmitted as a full 32-bit absolute value; subsequent samples are encoded as 10-bit signed deltas.

**Full-resolution reference:**

On the SD card (and when delta decoding is complete), each sample is an `int32_t` holding a 24-bit ADC value sign-extended to 32 bits. The ADC is the ADS1256, configured for 1 kHz output rate. Physical units conversion (raw ADC counts to millivolts) depends on the analog front-end gain setting, which is documented separately by Nair.

---

### 3.2 I-Block: ICG Derived — A5D5B003

| Field | Value |
|-------|-------|
| UUID | `A5D5B003-5A5A-4B4B-8888-1A2B3C4D5E6F` |
| Name | VUAMS_I_BLOCK |
| Properties | NOTIFY |
| CCCD required | Yes |
| Update rate | Per heartbeat (~1 Hz at rest; higher during exercise) |
| Payload | 16 bytes header + 24 bytes payload = 40 bytes total |

**Description:** Contains one set of haemodynamic parameters derived from the ICG waveform for each detected heartbeat. These are computed by the firmware's block assembler from the raw ICG signal after B- and C-point detection.

**I-block payload (follows header):**

| Byte offset (within payload) | Field | Type | Units | Description |
|------------------------------|-------|------|-------|-------------|
| 0–3 | `z0` | float32 LE | Ω | Baseline thoracic impedance |
| 4–7 | `dZdt_peak` | float32 LE | Ω/s | Peak –dZ/dt amplitude (systolic upstroke) |
| 8–11 | `pep_ms` | float32 LE | ms | Pre-ejection period |
| 12–15 | `lvet_ms` | float32 LE | ms | Left ventricular ejection time |
| 16–19 | `co_lpm` | float32 LE | L/min | Cardiac output estimate |
| 20–23 | `sv_ml` | float32 LE | mL | Stroke volume estimate |

**Notes:**
- All six fields are always present. Values are `NaN` (IEEE 754) when the corresponding parameter could not be reliably estimated for that beat (e.g. due to movement artefact or a poor quality ICG waveform segment).
- PEP is measured from the Q-wave onset in the ECG to the B-point in the ICG.
- LVET is measured from the B-point to the X-point in the ICG.
- Cardiac output and stroke volume estimates are derived using the Kubicek equation. Vasquez's algorithm specification governs the exact parameterisation.

---

### 3.3 M-Block: IMU — A5D5B004

| Field | Value |
|-------|-------|
| UUID | `A5D5B004-5A5A-4B4B-8888-1A2B3C4D5E6F` |
| Name | VUAMS_M_BLOCK |
| Properties | NOTIFY |
| CCCD required | Yes |
| Update rate | 10 Hz (one notification every 100 ms) |
| Payload | 16 bytes header + delta-encoded payload (variable; see Section 4) |

**Description:** Carries 100 ms of raw IMU data from the ICM-20948 9-axis inertial measurement unit: 3-axis accelerometer, 3-axis gyroscope, and 3-axis magnetometer, all at 100 Hz (10 samples per block).

**M-block payload (follows header):**

Nine delta-encoded sequences are transmitted in order: `ax`, `ay`, `az`, `gx`, `gy`, `gz`, `mx`, `my`, `mz`. Each sequence encodes 10 samples. For IMU data, 8-bit signed deltas are used (see Section 4). The first sample in each sequence is a full `int16_t` absolute value; subsequent 9 samples are 8-bit signed deltas.

**Channel order in payload:**

| Position | Channel | Source | Raw unit |
|----------|---------|--------|---------|
| 1 | `ax` | Accelerometer X | ICM-20948 raw LSB |
| 2 | `ay` | Accelerometer Y | ICM-20948 raw LSB |
| 3 | `az` | Accelerometer Z | ICM-20948 raw LSB |
| 4 | `gx` | Gyroscope X | ICM-20948 raw LSB |
| 5 | `gy` | Gyroscope Y | ICM-20948 raw LSB |
| 6 | `gz` | Gyroscope Z | ICM-20948 raw LSB |
| 7 | `mx` | Magnetometer X | ICM-20948 raw LSB |
| 8 | `my` | Magnetometer Y | ICM-20948 raw LSB |
| 9 | `mz` | Magnetometer Z | ICM-20948 raw LSB |

Physical unit conversion (LSB to g, deg/s, µT) depends on the configured full-scale range of the ICM-20948, documented in the hardware-firmware interface spec (Nair → Müller, pending).

---

### 3.4 P-Block: PPG — A5D5B005

| Field | Value |
|-------|-------|
| UUID | `A5D5B005-5A5A-4B4B-8888-1A2B3C4D5E6F` |
| Name | VUAMS_P_BLOCK |
| Properties | NOTIFY |
| CCCD required | Yes |
| Update rate | ~100 Hz (one notification per MAX30101 FIFO flush cycle, ~10 ms) |
| Payload | 16 bytes header + 14 bytes payload = 30 bytes total |

**Description:** Contains one PPG measurement epoch from the MAX30101 optical sensor. The sensor uses red (660 nm) and infrared (880 nm) LEDs to detect pulsatile blood volume changes at the fingertip or wrist.

**P-block payload (follows header):**

| Byte offset (within payload) | Field | Type | Units | Description |
|------------------------------|-------|------|-------|-------------|
| 0–3 | `ppg_red` | uint32 LE | ADC counts | Red LED photodiode raw count |
| 4–7 | `ppg_ir` | uint32 LE | ADC counts | IR LED photodiode raw count |
| 8–11 | `spo2_pct` | float32 LE | % | SpO₂ estimate; `NaN` if invalid |
| 12 | `hr_ppg` | uint8 | bpm | Heart rate from PPG; `0` = invalid |
| 13 | `ppg_valid` | uint8 | boolean | `1` = finger present and signal quality sufficient; `0` = invalid |

**Notes:**
- When `ppg_valid` is `0`, `spo2_pct` will be `NaN` and `hr_ppg` will be `0`. Clients should not display these values.
- SpO₂ and HR from PPG are secondary estimates. The primary heart rate source is the ECG (A-block).

---

### 3.5 S-Block: SCL/EDA — A5D5B006

| Field | Value |
|-------|-------|
| UUID | `A5D5B006-5A5A-4B4B-8888-1A2B3C4D5E6F` |
| Name | VUAMS_S_BLOCK |
| Properties | NOTIFY |
| CCCD required | Yes |
| Update rate | ~10 Hz (AD5933 measurement cycle) |
| Payload | 16 bytes header + 9 bytes payload = 25 bytes total |

**Description:** Contains one skin conductance measurement from the AD5933 impedance analyser. Skin conductance (also called electrodermal activity, EDA, or galvanic skin response, GSR) reflects eccrine sweat gland activity and is a direct marker of sympathetic nervous system arousal.

The signal is decomposed into a tonic component (the slowly-varying baseline, SCL) and a phasic component (rapid fluctuations above baseline, associated with discrete arousal events).

**S-block payload (follows header):**

| Byte offset (within payload) | Field | Type | Units | Description |
|------------------------------|-------|------|-------|-------------|
| 0–3 | `scl_tonic_uS` | float32 LE | µS | Tonic skin conductance level |
| 4–7 | `scl_phasic_uS` | float32 LE | µS | Phasic skin conductance component |
| 8 | `electrode_contact` | uint8 | boolean | `1` = electrode contact good; `0` = contact lost or impedance out of range |

**Notes:**
- When `electrode_contact` is `0`, both conductance values are unreliable and should be treated as invalid.
- Skin conductance is highly sensitive to electrode placement and skin preparation. Values below ~0.01 µS or above ~50 µS typically indicate a measurement problem rather than a true physiological state.

---

### 3.6 T-Block: Temperature — A5D5B007

| Field | Value |
|-------|-------|
| UUID | `A5D5B007-5A5A-4B4B-8888-1A2B3C4D5E6F` |
| Name | VUAMS_T_BLOCK |
| Properties | NOTIFY |
| CCCD required | Yes |
| Update rate | 1 Hz (one notification per second; firmware decimates from 4 Hz ADC rate) |
| Payload | 16 bytes header + 6 bytes payload = 22 bytes total |

**Description:** Contains one skin temperature measurement from the TMP117 precision temperature sensor. This reflects skin surface temperature at the device electrode site (upper chest / collarbone area).

**T-block payload (follows header):**

| Byte offset (within payload) | Field | Type | Units | Description |
|------------------------------|-------|------|-------|-------------|
| 0–3 | `skin_temp_c` | float32 LE | °C | Converted temperature |
| 4–5 | `temp_raw` | int16 LE | LSB | TMP117 raw register value (1 LSB = 7.8125 m°C) |

**Notes:**
- The TMP117 resolution is 0.0078125 °C (7.8125 m°C per LSB). The `skin_temp_c` float is derived from `temp_raw * 0.0078125`.
- The sensor measures the temperature of the device chassis in contact with skin, not deep body temperature. Normal range in ambulatory participants is approximately 30–36 °C.

---

### 3.7 Status — A5D5B009

| Field | Value |
|-------|-------|
| UUID | `A5D5B009-5A5A-4B4B-8888-1A2B3C4D5E6F` |
| Name | VUAMS_STATUS |
| Properties | NOTIFY, READ |
| CCCD required | Yes (for notifications) |
| Update rate | On change (event-driven) |
| Payload | 4 bytes |

**Description:** Reports the current system state of the VU-AMS device. The app should read this characteristic immediately after connecting and subscribe to notifications to receive updates. The status is encoded as a 32-bit bitmask.

**Status payload:**

| Byte offset | Field | Type | Description |
|-------------|-------|------|-------------|
| 0–3 | `status_flags` | uint32 LE | Bitmask of system event flags (see table below) |

**Status flag bit definitions:**

| Bit | Hex mask | Name | Meaning |
|-----|----------|------|---------|
| 0 | `0x00000001` | SD_READY | SD card initialised and writable |
| 1 | `0x00000002` | SD_ERROR | SD card error — write failed or card absent |
| 2 | `0x00000004` | SD_FULL | SD card full — recording will stop |
| 3 | `0x00000008` | SD_WRITING | SD card write in progress |
| 4 | `0x00000010` | BLE_CONNECTED | BLE connection established (always set when you can read this) |
| 5 | `0x00000020` | BLE_STREAMING | Data notifications are currently being sent |
| 6 | `0x00000040` | BLE_DROP | One or more BLE packets dropped in the last 1 s due to queue overflow |
| 7 | `0x00000080` | BATTERY_LOW | Battery at or below 15% state of charge |
| 8 | `0x00000100` | BATTERY_CRIT | Battery at or below 5% — shutdown imminent |
| 9 | `0x00000200` | SYSTEM_FAULT | Non-recoverable firmware fault — device restart required |

**Usage guidance:**
- On connection, read status and check `SD_READY` before sending `START_RECORDING`. If `SD_ERROR` is set, warn the user.
- Poll `BATTERY_LOW` and surface a warning to the researcher if set.
- If `BLE_DROP` is set persistently (for more than 5 consecutive status notifications), consider reducing notification subscriptions or requesting the user move closer to the device.
- `SYSTEM_FAULT` should result in a prominent error message. The device must be power-cycled to recover.

---

### 3.8 Control — A5D5B00A

| Field | Value |
|-------|-------|
| UUID | `A5D5B00A-5A5A-4B4B-8888-1A2B3C4D5E6F` |
| Name | VUAMS_CONTROL |
| Properties | WRITE, WRITE_WITHOUT_RESPONSE |
| CCCD required | No |
| Payload | 1–9 bytes (command-dependent) |

**Description:** Used by the central device (app) to send control commands to the VU-AMS. See Section 5 for the full command table.

---

## 4. Delta Encoding Scheme

Delta encoding reduces the BLE payload size for high-rate channels (ECG at 1 kHz, IMU at 100 Hz) by transmitting sample-to-sample differences rather than absolute values.

### 4.1 Rationale

ECG and IMU signals change slowly relative to their full dynamic range between adjacent samples at these sampling rates. A 24-bit ECG sample changes by at most a few hundred counts per millisecond during normal cardiac activity, well within the range of a 10-bit signed integer (–512 to +511).

### 4.2 10-bit delta encoding (ECG/ICG/PPG channels)

Used for A-block (ECG), and in future for ICG raw waveform streaming.

**Encoding rules:**

1. **First sample:** transmitted as a full 32-bit (4-byte) signed integer at the start of each channel sequence in a block.
2. **Subsequent samples:** each delta `d = sample[n] − sample[n−1]` is encoded as a 10-bit signed integer, packed into a continuous bitstream (no padding between deltas within a channel sequence).
3. **Delta range:** `d` must be in the range `[–512, +511]`. If a true delta exceeds this range (e.g. due to a large artefact), the encoder inserts an **escape code**: the 10-bit field is set to `0x200` (the minimum representable value, –512, which by convention signals an escape), followed immediately by a full 32-bit absolute value for that sample. The decoder must handle this.
4. **Bit packing:** deltas are packed MSB-first within each byte. The last byte of a sequence is zero-padded to a byte boundary.
5. **Byte alignment:** each new channel sequence within a block begins on a byte boundary.

**Decoder pseudocode (single channel sequence of N samples):**

```
function decode_10bit_sequence(bitstream, N):
    samples = []
    
    // Read first absolute sample (32-bit signed, little-endian)
    absolute_value = read_int32_le(bitstream)
    samples.append(absolute_value)
    
    last = absolute_value
    i = 1
    
    while i < N:
        delta = read_bits(bitstream, 10)  // 10-bit signed (two's complement)
        
        if delta == -512:  // escape code
            // Next sample is a full 32-bit absolute value
            absolute_value = read_int32_le(bitstream)
            samples.append(absolute_value)
            last = absolute_value
        else:
            reconstructed = last + delta
            samples.append(reconstructed)
            last = reconstructed
        
        i += 1
    
    align_to_byte_boundary(bitstream)
    return samples
```

**A-block structure after decoding:**

```
[32-bit absolute ECG1[0]]
[10-bit delta ECG1[1]]
[10-bit delta ECG1[2]]
...
[10-bit delta ECG1[249]]
<byte pad>
[32-bit absolute ECG2[0]]
[10-bit delta ECG2[1]]
...
[10-bit delta ECG2[249]]
<byte pad>
```

Total encoded size per channel (worst case, no escapes): 4 + ⌈(249 × 10) / 8⌉ = 4 + 312 = 316 bytes. For two channels: 632 bytes. The unencoded equivalent would be 250 × 4 × 2 = 2000 bytes. Typical compression ratio in clean ECG: 4–5×.

### 4.3 8-bit delta encoding (IMU channels)

Used for M-block (IMU: accelerometer, gyroscope, magnetometer).

**Encoding rules:** identical to Section 4.2 with the following changes:

1. **First sample:** transmitted as a full 16-bit (2-byte) signed integer.
2. **Subsequent samples:** each delta is encoded as an **8-bit signed integer** (range –128 to +127). Byte-aligned; no bit packing required.
3. **Escape code:** the value `0x80` (–128) is the escape code. When encountered, the next 2 bytes are a full 16-bit absolute value.

**Decoder pseudocode (single IMU channel sequence of N samples):**

```
function decode_8bit_sequence(bytestream, N):
    samples = []
    
    // Read first absolute sample (16-bit signed, little-endian)
    absolute_value = read_int16_le(bytestream)
    samples.append(absolute_value)
    
    last = absolute_value
    i = 1
    
    while i < N:
        delta = read_int8(bytestream)  // 8-bit signed
        
        if delta == -128:  // escape code
            absolute_value = read_int16_le(bytestream)
            samples.append(absolute_value)
            last = absolute_value
        else:
            reconstructed = last + delta
            samples.append(reconstructed)
            last = reconstructed
        
        i += 1
    
    return samples
```

**M-block encoded structure:**

Nine sequences in order: `ax`, `ay`, `az`, `gx`, `gy`, `gz`, `mx`, `my`, `mz`. Each sequence: 2 bytes (first absolute) + 9 bytes (9 deltas) = 11 bytes per channel. Total: 99 bytes. Unencoded: 9 × 10 × 2 = 180 bytes. Compression ratio: ~1.8×.

---

## 5. Control Command Reference

Commands are written to the Control characteristic (UUID `A5D5B00A`). Each command begins with a 1-byte command identifier, optionally followed by a command-specific payload.

| Command | ID byte | Total length | Description |
|---------|---------|-------------|-------------|
| START_RECORDING | `0x01` | 1 byte | Begin recording to SD card and enable data streaming |
| STOP_RECORDING | `0x02` | 1 byte | Stop recording, close SD file cleanly, halt streaming |
| SYNC_TIME | `0x03` | 9 bytes | Synchronise device RTC to host wall-clock time |

### 5.1 START_RECORDING (0x01)

| Byte | Field | Value |
|------|-------|-------|
| 0 | Command ID | `0x01` |

**Behaviour:** The device transitions from idle to recording state. It begins writing data blocks to the SD card and enabling notifications on all data characteristics. The Status characteristic will reflect `SD_WRITING` and `BLE_STREAMING` within 500 ms of receiving this command.

**Preconditions:** `SD_READY` must be set in the Status characteristic. If `SD_ERROR` or `SD_FULL` is set, the command is rejected (no response is sent; the Status characteristic will retain the error flag).

**Note:** This command maps directly to the firmware's `CMD_RECORDING_START` internal command.

### 5.2 STOP_RECORDING (0x02)

| Byte | Field | Value |
|------|-------|-------|
| 0 | Command ID | `0x02` |

**Behaviour:** The device stops acquiring new blocks for SD write. The current partially-filled block is flushed to the SD card and the file is closed with a valid directory entry. Notifications on data characteristics continue briefly while the queue drains, then cease. `SD_WRITING` and `BLE_STREAMING` flags clear in the Status characteristic when complete.

**Important:** The Central must not disconnect until `BLE_STREAMING` is clear in the Status characteristic after sending STOP_RECORDING. Disconnecting immediately may result in an incompletely closed file on the SD card.

**Note:** Maps to firmware `CMD_RECORDING_STOP`.

### 5.3 SYNC_TIME (0x03)

| Byte | Field | Type | Description |
|------|-------|------|-------------|
| 0 | Command ID | uint8 | `0x03` |
| 1–8 | `unix_time_us` | uint64 LE | Current wall-clock time as Unix timestamp in microseconds |

**Behaviour:** Sets the device's internal time reference so that block timestamps (`timestamp_us` in block headers) can be related to absolute wall-clock time. The device does not have a battery-backed RTC; its `esp_timer_get_time()` counter resets to zero at each power-on. The SYNC_TIME command provides the offset between the device timer and Unix time.

**Recommended practice:** Send SYNC_TIME immediately after establishing a connection, before sending START_RECORDING. Send it again if the connection is re-established mid-session after a dropout (the device timer keeps running, but re-synchronisation improves timestamp accuracy in the data file).

**Host time source:** Use the iPhone's system clock (Unix time, UTC). The precision of Bluetooth write latency means the synchronisation accuracy is typically ±5 ms. For studies requiring sub-millisecond timestamp accuracy, a dedicated synchronisation protocol (not defined in this version) is required.

---

## 6. Connection Sequence

The following pseudocode describes the recommended sequence for the iOS app to establish a session with the VU-AMS device.

```
// --- DISCOVERY PHASE ---

func connectToVUAMS():
    startBluetoothScan(serviceUUID: "A5D5B001-5A5A-4B4B-8888-1A2B3C4D5E6F")
    
    onDeviceDiscovered(peripheral):
        stopScan()
        connect(peripheral)


// --- CONNECTION PHASE ---

func onConnected(peripheral):
    // 1. Discover the VU-AMS service and all its characteristics
    discoverServices(peripheral, serviceUUID: "A5D5B001-...")
    
    onServicesDiscovered():
        discoverCharacteristics(serviceUUID: "A5D5B001-...")
    
    onCharacteristicsDiscovered(characteristics):
        // 2. Read current device status immediately
        readCharacteristic(uuid: "A5D5B009-...")  // Status
        
        // 3. Subscribe to all data and status notifications
        enableNotify(uuid: "A5D5B009-...")  // Status
        enableNotify(uuid: "A5D5B002-...")  // A-block (ECG)
        enableNotify(uuid: "A5D5B003-...")  // I-block (ICG derived)
        enableNotify(uuid: "A5D5B004-...")  // M-block (IMU)
        enableNotify(uuid: "A5D5B005-...")  // P-block (PPG)
        enableNotify(uuid: "A5D5B006-...")  // S-block (SCL)
        enableNotify(uuid: "A5D5B007-...")  // T-block (Temperature)
        
        // 4. Synchronise device time
        now_us = currentUnixTimeMicroseconds()
        payload = [0x03] + pack_uint64_le(now_us)  // 9 bytes
        writeCharacteristic(uuid: "A5D5B00A-...", value: payload)
        
        // 5. Verify SD card is ready before offering Start Recording
        status = readStatus()
        if status.SD_READY:
            enableStartRecordingButton()
        else:
            showError("SD card not ready. Check card and restart device.")


// --- RECORDING PHASE ---

func onStartRecordingTapped():
    // Send START_RECORDING command
    writeCharacteristic(uuid: "A5D5B00A-...", value: [0x01])
    
    // Data notifications will begin arriving on subscribed characteristics
    // within ~500 ms. Begin buffering and displaying incoming blocks.
    startLiveDisplay()
    
    onStatusNotification(status):
        if status.SD_WRITING:
            showSDIndicator(active: true)
        if status.BATTERY_LOW:
            showBatteryWarning()
        if status.BLE_DROP:
            logDropEvent()
        if status.SYSTEM_FAULT:
            showCriticalError("Device fault. Stop session and restart device.")


// --- STOP PHASE ---

func onStopRecordingTapped():
    // Send STOP_RECORDING command
    writeCharacteristic(uuid: "A5D5B00A-...", value: [0x02])
    
    // Wait for BLE_STREAMING to clear in Status notifications
    // before showing session complete or disconnecting
    waitForStatusFlag(flag: BLE_STREAMING, value: false, timeoutMs: 5000)
    
    showSessionComplete()
    
    // Optional: disconnect to save power
    disconnect(peripheral)


// --- INCOMING DATA HANDLING ---

func onNotification(uuid, data):
    header = parseBlockHeader(data[0..15])  // 16-byte common header
    
    switch header.type:
        case 0x41:  // A-block — ECG
            (ecg1, ecg2) = decode10bitDelta(data[16..], samplesPerChannel: 250)
            updateECGDisplay(ecg1, ecg2, timestamp: header.timestamp_us)
        
        case 0x49:  // I-block — ICG derived
            params = parseIBlock(data[16..])
            updateHaemodynamicsDisplay(params, timestamp: header.timestamp_us)
        
        case 0x4D:  // M-block — IMU
            axes = decode8bitDelta(data[16..], channels: 9, samplesPerChannel: 10)
            updateMotionDisplay(axes, timestamp: header.timestamp_us)
        
        case 0x50:  // P-block — PPG
            ppg = parsePBlock(data[16..])
            updatePPGDisplay(ppg, timestamp: header.timestamp_us)
        
        case 0x53:  // S-block — SCL
            scl = parseSBlock(data[16..])
            updateSCLDisplay(scl, timestamp: header.timestamp_us)
        
        case 0x54:  // T-block — Temperature
            temp = parseTBlock(data[16..])
            updateTempDisplay(temp, timestamp: header.timestamp_us)
```

---

## 7. Error Handling and Reliability

### 7.1 BLE packet loss

BLE notifications are not acknowledged. If the BLE_DROP flag is set in the Status characteristic, one or more data blocks have been discarded in the firmware's BLE queue. The app should:

1. Log a drop event with the current timestamp.
2. Display a visual indicator to the researcher (e.g. a brief yellow flash on the connection quality indicator).
3. Note the gap in the data timeline. The offline analysis software will detect gaps from the block timestamps and handle them appropriately.

Persistent dropping (lasting more than a few seconds) is abnormal and indicates a range or interference problem.

### 7.2 Connection loss during recording

If the BLE connection drops during an active recording:

- The device continues writing to the SD card without interruption.
- When the app reconnects, it should re-send SYNC_TIME and re-subscribe to all characteristics.
- The app does not need to re-send START_RECORDING — the device remains in recording state.
- The app should check the Status characteristic to confirm `SD_WRITING` is still set.

### 7.3 MTU negotiation

The default BLE ATT MTU is 23 bytes, which would fragment larger notifications. The app must request an MTU of at least **512 bytes** immediately after connecting (before discovering services). An MTU of 512 bytes accommodates the largest BLE notification payload without fragmentation across all defined block types.

On iOS, MTU negotiation is handled automatically by Core Bluetooth when you call `maximumWriteValueLength(for:)` or when the device requests a larger MTU. No explicit MTU request is needed in the current iOS Core Bluetooth API; iOS will negotiate the optimal MTU automatically.

### 7.4 Characteristic write without response

The Control characteristic supports `WRITE_WITHOUT_RESPONSE` for lower latency command delivery. For STOP_RECORDING, prefer `WRITE` (with response) if the latency is acceptable, to confirm the command was received. For START_RECORDING and SYNC_TIME, `WRITE_WITHOUT_RESPONSE` is acceptable.

---

*For questions about this specification, contact Müller (firmware) or Chen (iOS app). Interface changes require Lam sign-off.*

---

**Document end — VU-AMS BLE API Reference v0.1**
