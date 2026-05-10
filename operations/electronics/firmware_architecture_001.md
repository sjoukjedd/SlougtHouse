# VU-AMS Firmware Architecture — FreeRTOS Task Design
## Document 001

**Author:** Kai Müller (Firmware)  
**Date:** 2026-05-08  
**Platform:** ESP32-S3-MINI-1-N8R8 — Xtensa LX7 dual-core, 240 MHz, 8 MB Flash, 8 MB PSRAM, FreeRTOS  
**Status:** Draft — for review by Nair (hardware interface confirmation pending)

---

## 1. System Overview

The firmware runs on two LX7 cores with a strict division of labour:

- **Core 0 (PRO_CPU):** All real-time acquisition tasks. Time-critical interrupt-driven work. No BLE stack.
- **Core 1 (APP_CPU):** BLE stack (Bluedroid/NimBLE), SD writer, block assembler, system management. Latency-tolerant work.

This separation keeps the BLE stack's periodic interrupt activity off Core 0, preventing jitter on ECG/ICG sampling.

```
Core 0 (PRO_CPU) — ACQUISITION              Core 1 (APP_CPU) — OUTPUT & MANAGEMENT
─────────────────────────────────           ──────────────────────────────────────────
TASK_ADC_ACQUIRE      (prio 8)              TASK_BLOCK_ASSEMBLER    (prio 6)
TASK_IMU_ACQUIRE      (prio 7)              TASK_SD_WRITER          (prio 5)
TASK_I2C_SENSORS      (prio 6)              TASK_BLE_STREAM         (prio 4)
                                            TASK_BATTERY_MONITOR    (prio 2)
                                            TASK_WATCHDOG           (prio 3)
```

---

## 2. SPI Bus Arbitration

The SPI bus is shared between two devices:
- **24-bit ADC** (ADS1256 or equivalent) — continuous acquisition at 1 kHz, 8 channels
- **ICM-20948 IMU** — burst reads at 100 Hz

### Arbitration scheme: Binary mutex

A single `SemaphoreHandle_t spi_bus_mutex` governs access. Rules:

1. `TASK_ADC_ACQUIRE` holds the mutex for the duration of each ADC read cycle. Because the ADC runs at 1 kHz, each acquisition window is 1 ms. The task takes the mutex, clocks out all 8 channel reads via DMA, then releases it.
2. `TASK_IMU_ACQUIRE` requests the mutex at 100 Hz (10 ms period). Because the IMU window (10 ms) is 10× the ADC window (1 ms), IMU contention is brief. The IMU task waits with `xSemaphoreTake(spi_bus_mutex, pdMS_TO_TICKS(2))` — a 2 ms timeout. If the ADC does not release within 2 ms, the IMU task skips the sample and increments a miss counter (reported to `TASK_WATCHDOG`).
3. No other task touches the SPI bus. The BMP390 barometer is I²C only.

**Chip-select lines** are GPIO, managed inside each task before/after SPI transactions. The mutex does not replace CS management — both are required.

**DMA:** The ADC task uses ESP-IDF SPI DMA (SPI_TRANS_DMA_BUFFER_ALIGN) for continuous streaming. The IMU task uses a short non-DMA transaction (burst read of 12 bytes accelerometer + gyro registers). This is acceptable given the IMU's 100 Hz rate.

---

## 3. PSRAM Ring Buffer Layout

The 8 MB PSRAM is used as a circular ring buffer decoupling acquisition (producer, Core 0) from SD and BLE output (consumers, Core 1). The ADC produces ~36 kB/s of raw data at 1 kHz × 8 channels × 24-bit. The full multi-sensor stream is approximately:

| Source | Rate | Bytes/s |
|--------|------|---------|
| ADC (8 ch × 3 bytes) | 1000 Hz | 24,000 |
| IMU (12 bytes + temp 2B) | 100 Hz | 1,400 |
| PPG (8 bytes) | 100 Hz | 800 |
| SCL (already in ADC ch6/7) | — | — (included above) |
| TMP117 (4 bytes) | 4 Hz | 16 |
| BMP390 (8 bytes) | 10 Hz | 80 |
| **Total raw** | | **~26,300 bytes/s** |

With 8 MB PSRAM allocated as ring buffer, this provides approximately **320 seconds (~5 minutes) of headroom** before overflow if SD or BLE stall. Actual headroom will be less once assembled block overhead is added; targeting ≥ 60 s effective buffer.

### Buffer structure

```c
#define PSRAM_RINGBUF_BASE   0x3C000000   // PSRAM mapped start
#define PSRAM_RINGBUF_SIZE   (7 * 1024 * 1024)  // 7 MB — leave 1 MB for heap

typedef struct {
    uint32_t write_head;      // byte offset, wraps at PSRAM_RINGBUF_SIZE
    uint32_t sd_read_head;    // consumer 1: SD writer
    uint32_t ble_read_head;   // consumer 2: BLE streamer
    uint32_t sample_count;    // monotonic counter, never wraps
} RingBufControl_t;
```

The control struct lives in internal SRAM (not PSRAM) for fast atomic access. Writes and reads to PSRAM use `esp_cache_msync()` where coherence is required.

**Two independent read heads** allow SD and BLE to consume at different rates. SD consumes all data at full resolution. BLE may downsample or subset the data for bandwidth management (handled inside `TASK_BLE_STREAM`). Overflow detection: if `write_head` overtakes either consumer's read head, an overflow event is posted to the watchdog event group and the affected consumer's head is advanced to `write_head - SAFE_MARGIN` (data is dropped, not corrupted).

---

## 4. FreeRTOS Task Definitions

### TASK_ADC_ACQUIRE

| Parameter | Value |
|-----------|--------|
| Priority | 8 (highest acquisition priority) |
| Core | 0 |
| Stack | 4096 bytes |
| Trigger | Hardware timer ISR at 1 kHz (1 ms period); ISR posts to `xTaskNotifyFromISR` |

**Responsibility:** Drives the external 24-bit SPI ADC continuously at 1 kHz. On each timer tick, acquires all 8 channels (ICG Z0, V_source, ECG1, ECG2, Temp_legacy, Vbatt, SCL_tonic, SCL_phasic) via SPI DMA under the SPI mutex. Places raw 8-channel sample into the PSRAM ring buffer as a timestamped raw ADC frame.

**Input:** 1 kHz hardware timer notification; `spi_bus_mutex` semaphore  
**Output:** Raw ADC frames written to PSRAM ring buffer; `adc_frame_ready` event bit set in `acq_event_group` after each frame

---

### TASK_IMU_ACQUIRE

| Parameter | Value |
|-----------|--------|
| Priority | 7 |
| Core | 0 |
| Stack | 3072 bytes |
| Trigger | `vTaskDelayUntil` at 100 Hz (10 ms); SPI mutex acquired per read |

**Responsibility:** Reads ICM-20948 IMU at 100 Hz via SPI: 6-axis accelerometer + gyroscope (12 bytes) and IMU die temperature (2 bytes). Packages the 14-byte burst into an M_block-compatible frame with a 64-bit timestamp and writes it to the PSRAM ring buffer. On SPI contention, waits up to 2 ms; skips sample and increments miss counter if not acquired.

**Input:** Periodic timer via `vTaskDelayUntil`; `spi_bus_mutex`  
**Output:** M_block frames written to PSRAM ring buffer; `imu_frame_ready` event bit in `acq_event_group`; `imu_miss_count` counter accessible to `TASK_WATCHDOG`

---

### TASK_I2C_SENSORS

| Parameter | Value |
|-----------|--------|
| Priority | 6 |
| Core | 0 |
| Stack | 4096 bytes |
| Trigger | Mixed: MAX30101 FIFO watermark interrupt (GPIO, posted to task via `xTaskNotifyFromISR`) for PPG; software timer tick for TMP117 (4 Hz) and BMP390 (10 Hz) |

**Responsibility:** Services all three I²C sensors on a shared I²C bus. The MAX30101 PPG sensor raises a FIFO watermark interrupt when 25 samples (~250 ms at 100 Hz) are ready; the task bursts all pending samples from the FIFO in one I²C transaction. TMP117 and BMP390 are polled at their respective rates using internal software timer comparisons against a tick counter. All three sensors share the I²C bus sequentially within this single task — no I²C arbitration mutex needed since only this task owns the bus.

**Input:** MAX30101 DRDY GPIO interrupt notification; internal tick counters for TMP117 (4 Hz) and BMP390 (10 Hz)  
**Output:** P_block frames (PPG), T_block frames (temperature), D_block frames (barometer) written to PSRAM ring buffer; corresponding event bits set in `acq_event_group`

---

### TASK_BLOCK_ASSEMBLER

| Parameter | Value |
|-----------|--------|
| Priority | 6 |
| Core | 1 |
| Stack | 6144 bytes |
| Trigger | `acq_event_group` event bits from acquisition tasks; wakes when any acquisition event fires |

**Responsibility:** Reads raw frames from the PSRAM ring buffer and assembles them into typed data blocks (A_block, I_block, M_block, P_block, S_block, T_block, D_block, B_block) according to the block format specifications. Applies decimation and digital filtering where required (SCL tonic low-pass, SCL phasic band-pass, ECG/ICG FIR filters, ICG differentiation to dZ/dt). Places completed blocks onto two output queues: `sd_block_queue` (all blocks, full resolution) and `ble_block_queue` (blocks eligible for BLE streaming, potentially downsampled).

**Input:** PSRAM ring buffer (SD read head); `acq_event_group` event bits  
**Output:** `sd_block_queue` (QueueHandle_t, depth 32, full-resolution blocks); `ble_block_queue` (QueueHandle_t, depth 16, BLE-rate blocks)

---

### TASK_SD_WRITER

| Parameter | Value |
|-----------|--------|
| Priority | 5 |
| Core | 1 |
| Stack | 8192 bytes |
| Trigger | `sd_block_queue` — blocks pending write; task blocks on `xQueueReceive` |

**Responsibility:** Dequeues assembled data blocks from `sd_block_queue` and writes them to the SD card in a binary file format (one continuous session file per recording, with a header block at offset 0 and sequential data blocks appended). Uses multi-block buffered writes (512-byte sectors aligned) and `f_sync()` periodically to prevent data loss on power failure. Manages the SD card lifecycle: mount on startup, file creation with session timestamp, graceful unmount on shutdown command. Logs write errors and SD card full conditions to `TASK_WATCHDOG`.

**Input:** `sd_block_queue`; `system_cmd_queue` for start/stop recording commands  
**Output:** Binary session file on SD card; `sd_status_bits` in `system_event_group` (SD_READY, SD_ERROR, SD_FULL, SD_WRITING)

---

### TASK_BLE_STREAM

| Parameter | Value |
|-----------|--------|
| Priority | 4 |
| Core | 1 |
| Stack | 6144 bytes |
| Trigger | `ble_block_queue` — blocks pending transmission; task blocks on `xQueueReceive` with 100 ms timeout |

**Responsibility:** Dequeues data blocks from `ble_block_queue` and packetises them into BLE GATT notification payloads according to the agreed GATT profile (interface pending confirmation with Chen / iOS team). Manages BLE connection state (advertising, connected, disconnected, MTU negotiation). On connection, sends a session descriptor characteristic. During streaming, uses BLE 5.0 2M PHY where available to maximise throughput. Applies flow control: if the BLE stack's TX queue is full (connection-interval-limited), drops the oldest BLE block and increments a BLE drop counter reported to `TASK_WATCHDOG`. Does not block acquisition.

**Input:** `ble_block_queue`; BLE stack callbacks (connection, disconnection, MTU, write events)  
**Output:** BLE GATT notifications to connected iOS Central; `ble_status_bits` in `system_event_group` (BLE_CONNECTED, BLE_STREAMING, BLE_DROP_EVENT)

---

### TASK_BATTERY_MONITOR

| Parameter | Value |
|-----------|--------|
| Priority | 2 |
| Core | 1 |
| Stack | 2048 bytes |
| Trigger | `vTaskDelay` 10-second poll period |

**Responsibility:** Reads the `Vbatt` ADC channel value from the most recent B_block in the PSRAM ring buffer (no direct SPI access — consumes assembled data). Converts raw ADC counts to voltage using the calibration curve for the single-cell LiPo (3.0 V = empty, 4.2 V = full). Computes state-of-charge (SOC) percentage and posts the result to `battery_status` (a global struct in internal SRAM protected by a mutex). Posts a `BATTERY_LOW` event to `system_event_group` at <15% SOC and a `BATTERY_CRITICAL` event at <5% SOC (triggers graceful shutdown via `system_cmd_queue`).

**Input:** B_block Vbatt field from assembled data; `vTaskDelay` timer  
**Output:** `battery_status` struct (SOC%, voltage); `BATTERY_LOW` / `BATTERY_CRITICAL` event bits in `system_event_group`

---

### TASK_WATCHDOG

| Parameter | Value |
|-----------|--------|
| Priority | 3 |
| Core | 1 |
| Stack | 3072 bytes |
| Trigger | `vTaskDelayUntil` 1-second tick; event bits in `system_event_group` |

**Responsibility:** System health monitor and last-resort recovery handler. On each 1-second tick: checks that `TASK_ADC_ACQUIRE`, `TASK_IMU_ACQUIRE`, and `TASK_I2C_SENSORS` have each incremented their respective heartbeat counters since the last watchdog tick (stall detection). Monitors `imu_miss_count`, BLE drop counter, SD error flags, and battery critical events. Feeds the ESP32 hardware watchdog timer (`esp_task_wdt_reset()`) only if all monitored tasks are alive. On stall detection, logs the fault to SD (if available) and initiates a controlled restart via `esp_restart()`. Posts system health summary to `health_status` struct polled by `TASK_BLE_STREAM` for transmission to the iOS app.

**Input:** Per-task heartbeat counters (atomic uint32_t in SRAM); `system_event_group` bits; `imu_miss_count`; SD error flags; BLE drop counter; `battery_status`  
**Output:** ESP32 hardware WDT feed; `health_status` struct; `SYSTEM_FAULT` event bit in `system_event_group`; controlled restart on stall

---

## 5. Data Block Formats

All blocks share a common 12-byte header. Fields are little-endian. All timestamps are 64-bit microseconds since device boot (sourced from `esp_timer_get_time()`).

### Common Block Header (12 bytes)

```c
typedef struct __attribute__((packed)) {
    uint8_t  block_type;      // 'A'=0x41, 'I'=0x49, 'M'=0x4D, 'P'=0x50,
                               // 'S'=0x53, 'T'=0x54, 'D'=0x44, 'B'=0x42
    uint8_t  version;         // block format version, currently 0x01
    uint16_t payload_length;  // bytes following this header
    uint64_t timestamp_us;    // µs since boot (esp_timer_get_time)
} BlockHeader_t;              // 12 bytes total
```

---

### A_block — ECG / ICG (existing, documented for reference)

Assembled from ADC channels 0–3 at 1 kHz. One sample per block (or N samples batched — batch size TBD with Chen for BLE efficiency). Per-sample payload:

```c
typedef struct __attribute__((packed)) {
    int32_t  ecg1_raw;         // ADC ch2, 24-bit sign-extended to 32-bit
    int32_t  ecg2_raw;         // ADC ch3
    int32_t  z0_raw;           // ADC ch0, ICG baseline impedance
    int32_t  vsource_raw;      // ADC ch1, ICG current monitor
    int32_t  filterdZ0_dZ;     // dZ/dt, computed in TASK_BLOCK_ASSEMBLER
    int32_t  filterdZ0_resp;   // respiratory component of Z0
    int32_t  filterdECG1;      // filtered ECG1
    int32_t  filterdECG2;      // filtered ECG2
    uint8_t  beat;             // R-peak flag (0 or 1)
    uint8_t  electrode_status; // bitfield: b0=E1, b1=E2, b2=E3, b3=E4, b4=E5 contact
    uint8_t  _pad[2];
} A_sample_t;                  // 38 bytes per sample
```

Batched A_block payload: N × `A_sample_t`. Recommended batch size: 10 samples (10 ms of data, 380 bytes payload + 12 byte header = 392 bytes).

---

### I_block — ICG derived (existing, for reference)

Populated by the block assembler from Z0 after dZ/dt computation. Contains the same ICG-derived fields as A_block; the separation exists for legacy compatibility with the offline analysis platform (Reyes). Format identical to the ICG fields in A_block, without ECG fields.

---

### M_block — IMU motion (existing, extended)

```c
typedef struct __attribute__((packed)) {
    int16_t  ax;              // accelerometer X (raw counts)
    int16_t  ay;
    int16_t  az;
    int16_t  gx;              // gyroscope X (raw counts)
    int16_t  gy;
    int16_t  gz;
    int16_t  temp_imu_raw;   // ICM-20948 die temperature raw
    uint8_t  _pad[2];
} M_sample_t;                 // 16 bytes per sample
```

M_block payload: single M_sample_t per block (one block per 10 ms IMU interrupt). Total block size: 12 + 16 = 28 bytes.

---

### P_block — PPG / SpO₂ (new)

Sourced from MAX30101 internal 18-bit ADC via I²C FIFO. The MAX30101 produces Red (660 nm) and IR (940 nm) channel counts. HR and SpO₂ may be computed on-device (if algorithm is available) or deferred to the iOS app.

```c
typedef struct __attribute__((packed)) {
    uint32_t ppg_red;          // MAX30101 Red channel ADC count (18-bit, in 32-bit field)
    uint32_t ppg_ir;           // MAX30101 IR channel ADC count (18-bit, in 32-bit field)
    float    spo2_pct;         // SpO₂ estimate (0–100 %). NaN if not computed on-device.
    uint8_t  hr_ppg_bpm;       // Heart rate from PPG (BPM). 0 if not computed on-device.
    uint8_t  ppg_valid;        // 0x01 = sensor contact detected; 0x00 = no contact / ambient overflow
    uint8_t  led_current_red;  // Active LED current setting, Red (register value, 0–255)
    uint8_t  led_current_ir;   // Active LED current setting, IR (register value, 0–255)
} P_sample_t;                  // 16 bytes per sample
```

P_block payload: batched samples from FIFO drain. At 100 Hz with a watermark at 25 samples, each P_block carries 25 × P_sample_t = 400 bytes payload + 12 byte header = 412 bytes. All 25 samples share the block timestamp (timestamp = time of FIFO read; per-sample times reconstructed by index × 10 ms).

**Total P_block size:** 412 bytes.

---

### S_block — Skin Conductance Level / EDA (new)

Sourced from ADC channels 6 (SCL_tonic) and 7 (SCL_phasic) at 1 kHz raw rate. TASK_BLOCK_ASSEMBLER applies:
- SCL_tonic: 4th-order Butterworth low-pass at 1 Hz, decimated to 32 Hz output
- SCL_phasic: 4th-order Butterworth band-pass 0.01–5 Hz, decimated to 32 Hz output

Conversion from ADC counts to µS is applied using a calibration factor derived from the transimpedance amplifier gain and excitation voltage (Nair to provide the transfer function).

```c
typedef struct __attribute__((packed)) {
    float    scl_tonic_uS;     // Tonic SCL in µS (slow baseline EDA)
    float    scl_phasic_uS;    // Phasic SCR amplitude in µS (event-driven EDA)
    int32_t  scl_tonic_raw;    // Raw ADC ch6 count (before calibration), for validation
    int32_t  scl_phasic_raw;   // Raw ADC ch7 count (before calibration)
    uint8_t  electrode_contact; // 0x01 = E6 and E7 contact OK; 0x00 = contact suspect
    uint8_t  _pad[3];
} S_sample_t;                   // 17 bytes → padded to 20 bytes
```

S_block payload: 32 samples per block (1 second of 32 Hz data) = 32 × 20 = 640 bytes payload + 12 byte header = 652 bytes.

**Total S_block size:** 652 bytes.

---

### T_block — Body Temperature (new)

Sourced from TMP117 via I²C at 4 Hz. The TMP117 delivers a 16-bit signed temperature register; resolution 0.0078°C.

```c
typedef struct __attribute__((packed)) {
    float    skin_temp_C;      // Converted skin surface temperature in °C
    int16_t  temp_raw;         // Raw TMP117 register value (for diagnostic use)
    uint8_t  alert_flags;      // TMP117 alert register (high/low limit flags)
    uint8_t  _pad;
} T_sample_t;                   // 8 bytes per sample
```

T_block payload: 4 samples per block (1 second of 4 Hz data) = 4 × 8 = 32 bytes payload + 12 byte header = 44 bytes.

**Total T_block size:** 44 bytes.

---

## 6. Inter-Task Communication Summary

| Object | Type | Producer | Consumer(s) | Notes |
|--------|------|----------|-------------|-------|
| `spi_bus_mutex` | SemaphoreHandle_t | — | TASK_ADC_ACQUIRE, TASK_IMU_ACQUIRE | Binary mutex; ADC has implicit priority via task priority |
| `acq_event_group` | EventGroupHandle_t | TASK_ADC_ACQUIRE, TASK_IMU_ACQUIRE, TASK_I2C_SENSORS | TASK_BLOCK_ASSEMBLER | Bits: ADC_FRAME, IMU_FRAME, PPG_FRAME, TEMP_FRAME, BARO_FRAME |
| `system_event_group` | EventGroupHandle_t | Multiple tasks | TASK_WATCHDOG, TASK_BLE_STREAM | Bits: SD_READY, SD_ERROR, BLE_CONNECTED, BATTERY_LOW, BATTERY_CRITICAL, SYSTEM_FAULT |
| `sd_block_queue` | QueueHandle_t | TASK_BLOCK_ASSEMBLER | TASK_SD_WRITER | Depth 32; full-resolution assembled blocks |
| `ble_block_queue` | QueueHandle_t | TASK_BLOCK_ASSEMBLER | TASK_BLE_STREAM | Depth 16; BLE-eligible blocks |
| `system_cmd_queue` | QueueHandle_t | TASK_BATTERY_MONITOR, TASK_WATCHDOG | TASK_SD_WRITER, TASK_BLE_STREAM | Start, stop, shutdown commands |
| PSRAM ring buffer | Shared memory | TASK_ADC_ACQUIRE, TASK_IMU_ACQUIRE, TASK_I2C_SENSORS | TASK_BLOCK_ASSEMBLER | Controlled by `RingBufControl_t` in SRAM |
| `battery_status` | Global struct + mutex | TASK_BATTERY_MONITOR | TASK_WATCHDOG, TASK_BLE_STREAM | SOC%, voltage |
| `health_status` | Global struct | TASK_WATCHDOG | TASK_BLE_STREAM | Heartbeat counters, miss counts, error flags |
| Per-task heartbeat counters | `atomic_uint32_t` | Each acquisition task | TASK_WATCHDOG | Incremented each task iteration |

---

## 7. Stack and Memory Budget

| Task | Stack (bytes) | Core | Notes |
|------|---------------|------|-------|
| TASK_ADC_ACQUIRE | 4096 | 0 | DMA descriptor locals; SPI transaction structs |
| TASK_IMU_ACQUIRE | 3072 | 0 | Burst read buffer on stack |
| TASK_I2C_SENSORS | 4096 | 0 | MAX30101 FIFO burst (25 × 6 bytes) on stack |
| TASK_BLOCK_ASSEMBLER | 6144 | 1 | FIR filter state, decimation buffers |
| TASK_SD_WRITER | 8192 | 1 | FatFS work buffer; 512-byte sector staging |
| TASK_BLE_STREAM | 6144 | 1 | BLE GATT stack depth; notification payload buffers |
| TASK_BATTERY_MONITOR | 2048 | 1 | Minimal arithmetic |
| TASK_WATCHDOG | 3072 | 1 | Log buffer for fault recording |
| **Total** | **36,864** | | Well within 512 KB internal SRAM |

---

## 8. Open Items — Hardware Interface Dependencies

The following firmware parameters cannot be finalised until Nair provides the hardware interface specification (`intel/interfaces.md` — Hardware-Firmware Interface section):

1. **SPI pin assignments** — MOSI, MISO, SCLK, CS_ADC, CS_IMU
2. **ADC DRDY pin** — GPIO for data-ready interrupt from 24-bit ADC (used to trigger 1 kHz sampling ISR precisely, instead of free-running hardware timer)
3. **MAX30101 INT pin** — GPIO for FIFO watermark interrupt
4. **SPI clock speed** — max clock for ADC SPI (ADS1256 typ. 1.92 MHz; ICM-20948 typ. 7 MHz); SPI mode (CPOL/CPHA) for each device
5. **I²C bus address confirmation** — TMP117 (0x48 or 0x49 depending on ADD0 pin), MAX30101 (0x57 fixed), BMP390 (0x76 or 0x77)
6. **SCL transimpedance gain + excitation voltage** — required to compute the ADC-count-to-µS conversion factor for S_block
7. **ADC reference voltage** — required for Z0 and Vbatt calibration coefficients in A_block and B_block

---

## 9. Open Items — Interface Alignment with Chen (iOS)

The following must be resolved before `TASK_BLE_STREAM` can be finalised:

1. **BLE GATT characteristics** — one characteristic per block type vs. multiplexed on a single characteristic with block_type field
2. **MTU size** — target MTU negotiation (iOS supports up to 517 bytes with BLE 5.0; impacts batch size selection for A_block and P_block)
3. **Connection interval** — target CI for streaming mode (7.5 ms min on iOS for background; 15–30 ms recommended for battery efficiency)
4. **BLE downsampling policy** — does iOS want full 1 kHz ECG/ICG or a downsampled version for live display? SD always gets full resolution regardless.

---

---

## 10. BLE Downsampling & Compression Policy

**Decision (principal, 2026-05-08):** SD card receives full resolution on all channels. BLE streaming receives a reduced-rate, compressed version for live display in the iOS app.

### Design constraints

- BLE 5.0 effective throughput budget: **~20 kB/s** across all channels combined (conservative estimate on a 15 ms connection interval with 2M PHY, accounting for ATT overhead and iOS stack variability).
- iOS app requires live waveform display for: ECG, ICG dZ/dt, SCL phasic, PPG.
- Derived / slow parameters (HR, CO, SV, SpO₂, SCL tonic) are sufficient at a low update rate (≤ 1 Hz for trend display; beat-to-beat for HR/SV/CO).
- Compression method: **delta-encoding** of successive samples (inter-sample differences are small for oversampled physiological signals), followed by fixed-width quantisation (8-bit or 10-bit) for BLE payloads. SD always stores the original 24-bit or float values — no lossy step on the SD path.

---

### Per-channel policy

#### ECG (channels: ECG1, ECG2)

| Parameter | Value |
|-----------|-------|
| SD sample rate | 1000 Hz (full ADC rate, 24-bit) |
| BLE sample rate | 250 Hz (downsample 4:1) |
| BLE encoding | Delta-encode successive 250 Hz samples; quantise delta to **10-bit signed** (range ±512 LSB at 250 Hz is sufficient for inter-sample ECG variation at 1 kHz original rate). Pack two 10-bit deltas per 3 bytes. |
| Downsampling rationale | 250 Hz is the clinical minimum for ECG waveform fidelity (QRS morphology preserved; highest relevant frequency <125 Hz). Factor of 4 reduces ECG contribution to BLE budget from ~6 kB/s to ~1.6 kB/s per channel. |

---

#### ICG dZ/dt

| Parameter | Value |
|-----------|-------|
| SD sample rate | 1000 Hz (computed in TASK_BLOCK_ASSEMBLER, stored as float32) |
| BLE sample rate | 200 Hz (downsample 5:1) |
| BLE encoding | Delta-encode successive 200 Hz samples; quantise delta to **10-bit signed**. Pack two deltas per 3 bytes. |
| Downsampling rationale | ICG dZ/dt highest diagnostic frequency content is the C-wave and X-notch, both well below 50 Hz. 200 Hz provides comfortable margin (Nyquist at 100 Hz). Factor of 5 reduces ICG BLE load from ~4 kB/s to ~800 B/s. |

---

#### SCL phasic (EDA event-related)

| Parameter | Value |
|-----------|-------|
| SD sample rate | 32 Hz (already decimated in TASK_BLOCK_ASSEMBLER from 1 kHz ADC) |
| BLE sample rate | 16 Hz (downsample 2:1 from the 32 Hz SD output) |
| BLE encoding | Delta-encode successive 16 Hz samples; quantise delta to **8-bit signed** (SCL phasic changes slowly; inter-sample delta at 16 Hz is small). 1 byte per sample. |
| Downsampling rationale | SCL phasic responses evolve over 1–5 seconds. 16 Hz is ample for live display. Factor of 2 from the already-decimated 32 Hz stream; total factor 64 from raw ADC. BLE contribution: ~16 B/s — negligible. |

---

#### SCL tonic (slow EDA baseline) — derived parameter, not a display waveform

| Parameter | Value |
|-----------|-------|
| SD sample rate | 32 Hz (decimated in TASK_BLOCK_ASSEMBLER) |
| BLE update rate | 1 Hz (slow trend only) |
| BLE encoding | Send raw float32 (4 bytes) directly — no delta encoding needed at 1 Hz. |
| Rationale | Tonic baseline drifts over tens of seconds. 1 Hz update is sufficient for live trend display. 4 B/s, negligible budget impact. |

---

#### PPG (Red + IR channels)

| Parameter | Value |
|-----------|-------|
| SD sample rate | 100 Hz (MAX30101 native, 18-bit per channel) |
| BLE sample rate | 50 Hz (downsample 2:1) |
| BLE encoding | Delta-encode successive 50 Hz samples per channel (Red and IR separately); quantise delta to **10-bit signed**. Pack two deltas per 3 bytes per channel. |
| Downsampling rationale | PPG pulse waveform contains energy up to ~10 Hz. 50 Hz (Nyquist 25 Hz) is sufficient for live pulse waveform display. Factor of 2 from 100 Hz; BLE contribution: ~600 B/s for both channels. |

---

#### SpO₂ and HR (PPG-derived) — derived parameters

| Parameter | Value |
|-----------|-------|
| SD sample rate | Stored per P_block event (beat-to-beat or per second depending on on-device algorithm output) |
| BLE update rate | 1 Hz (or per-beat if HR algorithm fires, but rate-limited to ≤ 2 Hz) |
| BLE encoding | hr_ppg_bpm: uint8 (1 byte). spo2_pct: uint8 scaled 0–100 (1 byte). 2 bytes total per update. |
| Rationale | HR and SpO₂ are slow parameters for clinical interpretation. Beat-to-beat trend at ≤ 2 Hz is fully adequate for live display. 4 B/s total, negligible. |

---

#### IMU (accelerometer + gyroscope)

| Parameter | Value |
|-----------|-------|
| SD sample rate | 100 Hz (6 axes, 16-bit per axis) |
| BLE sample rate | 25 Hz (downsample 4:1) |
| BLE encoding | Delta-encode successive 25 Hz samples per axis; quantise to **8-bit signed** delta (motion at 25 Hz has small inter-sample delta for typical body movement). 6 bytes per sample (1 byte per axis × 6 axes). |
| Downsampling rationale | IMU is used for artefact rejection and activity classification in live display — full motion waveform fidelity is not required over BLE. 25 Hz is adequate to detect gross movement. Factor of 4; BLE contribution: ~150 B/s. |

---

#### Derived cardiovascular parameters (CO, SV) — computed offline or deferred

| Parameter | Value |
|-----------|-------|
| SD storage | Per-beat values computed by offline Java platform (Reyes), not on-device |
| BLE | Not streamed in real-time; if on-device estimation is added later, 1 Hz update rate, 4 bytes each (float32) |
| Rationale | Stroke volume and cardiac output require validated ICG algorithms (Vasquez). On-device real-time estimation is out of scope for firmware v1. |

---

### Delta-encoding implementation note

For each waveform channel, `TASK_BLE_STREAM` maintains a `prev_sample` value (last transmitted sample). On each BLE frame:

```c
int16_t delta = (int16_t)(current_sample - prev_sample);
// Clamp to encoding range:
if (delta > max_val) delta = max_val;   // saturate — avoids encoding overflow
if (delta < min_val) delta = min_val;
// Pack into 8-bit or 10-bit field in BLE payload
prev_sample = current_sample;
```

On reconnect or after a BLE drop event, `prev_sample` is reset and the next BLE frame carries an absolute (non-delta) value in the first sample slot (flagged by a header bit), allowing the iOS app to resynchronise the display baseline without waiting for a new recording block.

---

### BLE bandwidth budget

Effective BLE budget: **~20,000 bytes/s**. Budget below is calculated at the BLE streaming rates defined above.

| Channel | BLE rate | Encoding | Bytes per sample | Bytes/s | Notes |
|---------|----------|----------|-----------------|---------|-------|
| ECG1 | 250 Hz | 10-bit delta, 2 deltas/3 B | 1.5 B/sample | **375** | One of two ECG channels |
| ECG2 | 250 Hz | 10-bit delta, 2 deltas/3 B | 1.5 B/sample | **375** | |
| ICG dZ/dt | 200 Hz | 10-bit delta, 2 deltas/3 B | 1.5 B/sample | **300** | |
| SCL phasic | 16 Hz | 8-bit delta | 1 B/sample | **16** | |
| SCL tonic | 1 Hz | float32 absolute | 4 B/update | **4** | Trend only |
| PPG Red | 50 Hz | 10-bit delta, 2 deltas/3 B | 1.5 B/sample | **75** | |
| PPG IR | 50 Hz | 10-bit delta, 2 deltas/3 B | 1.5 B/sample | **75** | |
| SpO₂ + HR | ≤ 2 Hz | uint8 × 2 | 2 B/update | **4** | |
| IMU (6 axes) | 25 Hz | 8-bit delta × 6 axes | 6 B/sample | **150** | |
| Block headers + framing overhead | — | — | — | **~200** | Estimated 10% overhead on above |
| **Total** | | | | **~1,574 B/s** | |

**Total estimated BLE data rate: ~1.6 kB/s — well within the 20 kB/s budget.**

This leaves approximately **18.4 kB/s of headroom**, which accommodates:
- Health status / watchdog frames from `TASK_WATCHDOG` (small, infrequent)
- Battery status characteristic (4 bytes, 0.1 Hz)
- Any future additional channels or diagnostic streams added in later firmware versions
- Worst-case BLE stack and ATT header overhead growth at shorter connection intervals

The conservative design is intentional: BLE streaming must never compete with SD writing for CPU time on Core 1, and the BLE stack must not cause TASK_BLE_STREAM to starve other Core 1 tasks under high-throughput conditions.

---

*Section added 2026-05-08. Müller. Decision by principal.*

---

*Document 001 — Firmware Architecture. Müller, 2026-05-08.*  
*Supersedes: no previous firmware architecture document exists.*  
*Next: pending hardware interface spec from Nair before implementation can begin.*
