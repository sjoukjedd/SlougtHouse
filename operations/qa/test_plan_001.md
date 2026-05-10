# VU-AMS Test Plan 001
**Document ID:** TP-001  
**Product:** VU-AMS Wearable Physiological Monitor  
**Firmware Target:** ESP32-S3-MINI-1-N8R8 (dual-core LX7, 240 MHz, 8 MB Flash, 8 MB PSRAM)  
**Author:** Marcus Bell, QA & Test Engineering  
**Date:** 2026-05-09  
**Status:** DRAFT  

---

## 1. Scope

This plan covers verification of the VU-AMS firmware (ESP-IDF / FreeRTOS, C++) and iOS application (Swift / CoreBluetooth) at unit, integration, and end-to-end levels. It does not cover hardware bring-up, RF compliance, or clinical validation of physiological algorithms.

---

## 2. References

| Ref | File | Description |
|-----|------|-------------|
| R1 | `operations/firmware/main/config.h` | Hardware pin assignments, buffer sizes, UUIDs |
| R2 | `operations/firmware/main/data_blocks.h` | On-wire block layout, field sizes |
| R3 | `operations/firmware/main/tasks/task_adc.h` | ADC task API, priority, core assignment |
| R4 | `operations/firmware/main/tasks/task_ble_stream.h` | BLE streaming task API |
| R5 | `operations/firmware/main/tasks/task_sd_writer.h` | SD writer task API |
| R6 | `operations/ios_app/BLE/BLEManager.swift` | iOS BLE connection and parsing logic |

---

## 3. Test Environment

### 3.1 Firmware Unit Tests (Host)

- **Host OS:** macOS 14+ or Ubuntu 22.04 LTS
- **Toolchain:** `idf.py` host test build (`IDF_TARGET=linux`)
- **Framework:** Unity (bundled with ESP-IDF `components/unity`)
- **Build command:** `idf.py -T components/vuams_tests build`
- **Run command:** `./build/vuams_tests.elf`

### 3.2 Firmware Integration Tests (On-Device)

- **Hardware:** VU-AMS prototype board with ESP32-S3-MINI-1-N8R8
- **Flash tool:** `idf.py flash monitor`
- **Instruments:** Logic analyser (Saleae Logic Pro 8 or equivalent) on DRDY (GPIO9), SPI CLK (GPIO12)
- **BLE sniffer:** nRF Sniffer for Bluetooth LE (Wireshark plug-in)
- **SD card:** Formatted FAT32, 32 GB class 10

### 3.3 iOS Unit Tests

- **IDE:** Xcode 16+
- **Framework:** XCTest
- **Run command:** `xcodebuild test -scheme VU-AMS -destination 'platform=iOS Simulator,name=iPhone 16'`

### 3.4 iOS Integration Tests

- **Simulator peripheral:** `operations/ios_simulator` app running in a second Simulator instance
- **Framework:** XCTest + `CBCentralManager` test shim or the simulator peripheral app over the macOS Bluetooth adapter

### 3.5 End-to-End Tests

- **Hardware:** VU-AMS prototype + iPhone 15 or later running iOS 17+
- **Recording duration:** 30 minutes continuous
- **Battery test:** Full charge (4.20 V) to automatic shutdown

---

## 4. Pass/Fail Conventions

- **PASS:** All expected results are observed within stated tolerances.
- **FAIL:** Any expected result is not observed, or any prohibited condition occurs.
- **BLOCKED:** Precondition not met; test cannot run. Log reason and test reference in defect tracker.

---

## 5. Firmware Unit Tests (Host-Side, Unity)

### FW-UNIT-001 — PSRAM Ringbuffer: Basic Push/Pop

| Field | Value |
|-------|-------|
| **ID** | FW-UNIT-001 |
| **Title** | PSRAM ringbuffer basic push and pop |
| **Component** | `psram_ringbuf` |
| **Framework** | Unity (host build) |

**Preconditions**
- Host test binary built successfully against the `psram_ringbuf` module.
- Buffer instantiated with `PSRAM_RINGBUF_SIZE` (7 340 032 bytes).

**Steps**
1. Initialise a ringbuffer of capacity 7 MB via `psram_ringbuf_init()`.
2. Push a single 2016-byte A-block payload (`sizeof(a_block_t)` = 16 + 2000 = 2016 bytes) with known byte pattern `0xAB` fill.
3. Call `psram_ringbuf_pop()` into a scratch buffer of equal size.
4. Compare popped bytes against original using `TEST_ASSERT_EQUAL_MEMORY`.
5. Verify `psram_ringbuf_used()` returns 0 after pop.
6. Verify `psram_ringbuf_free()` returns full capacity after pop.

**Expected Result**
- Popped data is byte-identical to pushed data.
- Used bytes = 0, free bytes = 7 MB.

**Pass Criteria**
- `TEST_ASSERT_EQUAL_MEMORY` passes with zero mismatches.
- Used and free byte counts match expected values.

---

### FW-UNIT-002 — PSRAM Ringbuffer: Overflow Behaviour

| Field | Value |
|-------|-------|
| **ID** | FW-UNIT-002 |
| **Title** | PSRAM ringbuffer overflow drops oldest data and respects safe margin |
| **Component** | `psram_ringbuf` |
| **Framework** | Unity (host build) |

**Preconditions**
- Ringbuffer initialised as in FW-UNIT-001.
- `PSRAM_SAFE_MARGIN` = 262 144 bytes available.

**Steps**
1. Fill the buffer until `psram_ringbuf_free()` < `PSRAM_SAFE_MARGIN` + `sizeof(a_block_t)`.
2. Record the sequence number of the oldest block currently in the buffer (`seq_oldest`).
3. Push one additional A-block.
4. Query `psram_ringbuf_free()` after push.
5. Pop the first block and inspect its sequence number (`seq_after`).

**Expected Result**
- After overflow push, `psram_ringbuf_free()` >= `PSRAM_SAFE_MARGIN`.
- `seq_after` > `seq_oldest` (oldest data was discarded, not the new data).
- No crash, no assertion failure.

**Pass Criteria**
- Free bytes ≥ 262 144 after overflow push.
- Oldest surviving block has a later sequence number than the discarded block.

---

### FW-UNIT-003 — PSRAM Ringbuffer: Thread Safety Under Concurrent Push/Pop

| Field | Value |
|-------|-------|
| **ID** | FW-UNIT-003 |
| **Title** | PSRAM ringbuffer data integrity under concurrent producer and consumer threads |
| **Component** | `psram_ringbuf` |
| **Framework** | Unity (host build) with pthreads |

**Preconditions**
- Host build with pthreads available (`-lpthread`).
- Ringbuffer initialised with 7 MB.

**Steps**
1. Spawn a producer thread that pushes 10 000 blocks of 2016 bytes each; each block is filled with a unique incrementing 32-bit sequence counter in bytes 0–3, remainder `0x00`.
2. Spawn a consumer thread that pops blocks as fast as available and records the sequence counter from each block.
3. Both threads run concurrently from `t=0`.
4. Join both threads after producer has pushed all 10 000 blocks and consumer queue is drained.
5. Verify no block has a corrupt sequence counter (i.e., partial write visible to consumer).
6. Verify the consumer never reads the same block twice.

**Expected Result**
- No torn reads: every consumed block has a consistent sequence counter (bytes 0–3 form a valid uint32 written atomically from the producer's perspective).
- No block is popped more than once.
- No deadlock: both threads complete within 30 s.

**Pass Criteria**
- Zero corrupt sequence counters observed.
- Zero duplicate blocks.
- Test completes in < 30 s.

---

### FW-UNIT-004 — Delta Encoding: Known Input to Expected Output

| Field | Value |
|-------|-------|
| **ID** | FW-UNIT-004 |
| **Title** | Delta encoder produces correct encoded output for known ECG sample sequence |
| **Component** | `delta_codec` |
| **Framework** | Unity (host build) |

**Preconditions**
- `delta_encode()` function under test accepts `int32_t *in`, `int16_t *out`, `size_t n`, returns `esp_err_t`.

**Steps**
1. Define input array `in[8]` = `{0, 100, 250, 250, 200, 150, 150, 175}`.
2. Call `delta_encode(in, out, 8)`.
3. Assert `out[0]` == 0 (first sample stored verbatim as baseline, cast to int16).
4. Assert `out[1]` == 100 (delta: 100 - 0).
5. Assert `out[2]` == 150 (delta: 250 - 100).
6. Assert `out[3]` == 0 (delta: 250 - 250).
7. Assert `out[4]` == -50 (delta: 200 - 250).
8. Assert `out[5]` == -50 (delta: 150 - 200).
9. Assert `out[6]` == 0 (delta: 150 - 150).
10. Assert `out[7]` == 25 (delta: 175 - 150).
11. Verify return value is `ESP_OK`.

**Expected Result**
- All eight assertions pass.
- Return value is `ESP_OK`.

**Pass Criteria**
- Zero assertion failures from `TEST_ASSERT_EQUAL_INT16`.

---

### FW-UNIT-005 — Delta Encoding: Saturation Clamp

| Field | Value |
|-------|-------|
| **ID** | FW-UNIT-005 |
| **Title** | Delta encoder clamps overflow delta to INT16 range |
| **Component** | `delta_codec` |
| **Framework** | Unity (host build) |

**Preconditions**
- Same as FW-UNIT-004.

**Steps**
1. Define input array `in[3]` = `{0, 40000, -40000}`.
2. Call `delta_encode(in, out, 3)`.
3. Assert `out[0]` == 0.
4. Assert `out[1]` == INT16_MAX (32767): delta 40000 is clamped.
5. Assert `out[2]` == INT16_MIN (-32768): delta -80000 is clamped.
6. Verify return value is `ESP_OK` (not an error; saturation is expected behaviour).

**Expected Result**
- Deltas exceeding int16 range are clamped, not wrapped, not causing UB.

**Pass Criteria**
- `out[1]` == 32767, `out[2]` == -32768.

---

### FW-UNIT-006 — Block Assembly: Correct Framing and Header Fields

| Field | Value |
|-------|-------|
| **ID** | FW-UNIT-006 |
| **Title** | Block assembler emits correct header type, version, payload_len, and timestamp |
| **Component** | `block_assembler` |
| **Framework** | Unity (host build) |

**Preconditions**
- `block_assembler_init()` called.
- A stub ADC ISR feeds 250 sample pairs into `block_assembler_feed_adc()` with a known `timestamp_us` = 1 000 000.

**Steps**
1. Feed 249 samples; assert no block is emitted yet (queue empty).
2. Feed the 250th sample.
3. Dequeue one `a_block_t` from `g_adc_block_queue`.
4. Assert `header.type` == `BLOCK_TYPE_A` (0x41).
5. Assert `header.version` == `BLOCK_VERSION` (0x01).
6. Assert `header.payload_len` == 2000 (2 × 250 × 4).
7. Assert `header.timestamp_us` == 1 000 000.
8. Verify ECG channel 1 sample at index 0 matches first fed value.
9. Verify ECG channel 1 sample at index 249 matches last fed value.

**Expected Result**
- Block is emitted only on the 250th sample.
- All header fields match expected values.
- ECG sample array is populated in order.

**Pass Criteria**
- All `TEST_ASSERT_EQUAL_*` assertions pass.
- No blocks emitted before the 250th sample.

---

### FW-UNIT-007 — Block Assembly: CRC Integrity

| Field | Value |
|-------|-------|
| **ID** | FW-UNIT-007 |
| **Title** | Block assembler appends correct CRC-32 trailer |
| **Component** | `block_assembler` |
| **Framework** | Unity (host build) |

**Preconditions**
- Same as FW-UNIT-006.
- Block layout appends a 4-byte CRC-32/MPEG-2 after the struct payload.

**Steps**
1. Assemble one complete A-block as in FW-UNIT-006.
2. Read the 4-byte CRC trailer from bytes `sizeof(a_block_t)` to `sizeof(a_block_t)+3` of the output buffer.
3. Independently compute CRC-32 over bytes 0 to `sizeof(a_block_t)-1`.
4. Assert computed CRC equals the appended CRC.
5. Corrupt byte 10 of the block; recompute CRC; assert the result does NOT equal the appended CRC.

**Expected Result**
- CRC trailer matches independent calculation.
- Single-byte corruption is detected.

**Pass Criteria**
- Both assertions pass.

---

## 6. Firmware Integration Tests (On-Device)

### FW-INT-001 — ADC Acquisition: 1 kHz Sample Rate via DRDY Timing

| Field | Value |
|-------|-------|
| **ID** | FW-INT-001 |
| **Title** | ADS1256 DRDY interrupt fires at 1000 Hz ± 0.1% |
| **Component** | `task_adc`, ADS1256 |
| **Hardware** | Logic analyser on GPIO9 (DRDY) |

**Preconditions**
- Firmware flashed and running.
- ADS1256 configured for 1 kHz data rate (`ADC_SAMPLE_RATE_HZ` = 1000).
- Logic analyser capturing GPIO9 at ≥ 4 MHz sample rate.
- Recording started via CMD_RECORDING_START (command byte 0x01 on CONTROL characteristic or UART trigger).

**Steps**
1. Start logic analyser capture.
2. Send `CMD_RECORDING_START` to the device.
3. Capture DRDY falling edges for 10 seconds.
4. Export edge timestamps.
5. Compute intervals between consecutive falling edges.
6. Calculate mean interval and standard deviation.
7. Count total edges in 10 s.

**Expected Result**
- Mean DRDY interval = 1.000 ms ± 0.001 ms (1 kHz ± 0.1%).
- Standard deviation of intervals < 5 µs.
- Total edges in 10 s: 10 000 ± 10.

**Pass Criteria**
- Mean interval within 0.999–1.001 ms.
- No missed samples: edge count 9990–10010 over 10 s.
- No consecutive interval > 2.0 ms (no single-sample dropout).

---

### FW-INT-002 — BLE Throughput: Bytes per Second Within Budget

| Field | Value |
|-------|-------|
| **ID** | FW-INT-002 |
| **Title** | BLE data rate does not exceed 1.6 kB/s budget |
| **Component** | `task_ble_stream` |
| **Hardware** | nRF Sniffer + Wireshark on the BLE channel |

**Preconditions**
- iPhone (or nRF52840 dongle running nRF Sniffer firmware) in range.
- Device advertising service UUID `A5D5B001-5A5A-4B4B-8888-1A2B3C4D5E6F`.
- iOS app (or nRF Connect) connected with all notify characteristics subscribed.
- nRF Sniffer capturing on the correct advertising channel.

**Steps**
1. Connect iOS app or nRF Connect to the device.
2. Subscribe notifications on all block characteristics (A, I, M, P, S, T).
3. Start recording (`CMD_RECORDING_START`).
4. Capture BLE traffic for 60 seconds.
5. In Wireshark, filter on `btle && btle.data_header.llid == 0x2` (L2CAP data PDUs from peripheral).
6. Sum total payload bytes delivered over 60 s.
7. Divide by 60 to get bytes/sec.

**Expected Result**
- Average throughput ≤ 1638 bytes/sec (1.6 kB/s).
- No `EVT_BLE_DROP` event logged (system event group bit 6) during the capture window.

**Pass Criteria**
- Measured throughput ≤ 1638 B/s.
- Zero BLE drop events in 60 s under normal operating conditions.

---

### FW-INT-003 — SD Write: Sustained Write Speed and File Integrity

| Field | Value |
|-------|-------|
| **ID** | FW-INT-003 |
| **Title** | SD card write throughput is sufficient and data is intact after clean stop |
| **Component** | `task_sd_writer` |
| **Hardware** | SD card, logic analyser optional |

**Preconditions**
- FAT32-formatted SD card inserted (class 10 minimum, 32 GB).
- `EVT_SD_READY` bit set in system event group within 3 s of power-on.
- Device not connected via BLE (to avoid queue contention).

**Steps**
1. Send `CMD_RECORDING_START`.
2. Record for exactly 5 minutes.
3. Send `CMD_RECORDING_STOP`.
4. Wait for `EVT_SD_WRITING` bit to clear (indicates flush complete).
5. Remove SD card.
6. Mount card on a host PC and open the recording file.
7. Parse all blocks in the file: verify each block's header type, version, payload_len, and CRC.
8. Count total A-blocks: expect 5 min × 60 s × 4 blocks/s = 1200 A-blocks.
9. Verify no block has a truncated payload (EOF mid-struct).
10. Verify timestamps are monotonically increasing across all blocks.

**Expected Result**
- All 1200 A-blocks present (±2 for boundary rounding).
- Every block passes CRC check.
- No truncated blocks.
- Timestamps strictly increasing.

**Pass Criteria**
- Block count 1198–1202.
- CRC pass rate 100%.
- No truncation detected.

---

### FW-INT-004 — SD Write: File Integrity After Uncontrolled Power Cut

| Field | Value |
|-------|-------|
| **ID** | FW-INT-004 |
| **Title** | SD file is parseable (no filesystem corruption) after hard power cut during write |
| **Component** | `task_sd_writer`, FAT filesystem |
| **Hardware** | SD card, power supply with enable relay |

**Preconditions**
- Same SD card as FW-INT-003.
- Power supply able to cut VCC within < 1 ms on software trigger.

**Steps**
1. Start recording as in FW-INT-003.
2. After exactly 90 seconds, cut power abruptly (do not send CMD_RECORDING_STOP).
3. Re-apply power; wait for boot (do not start a new recording).
4. Remove SD card; mount on host PC.
5. Run `fsck.fat` (Linux) or `chkdsk` (Windows) on the partition.
6. Attempt to open and parse all complete blocks in the recording file.

**Expected Result**
- `fsck.fat` / `chkdsk` reports no filesystem errors (or repairs only the open-file metadata).
- At least 90% of the expected blocks (approximately 360 A-blocks) are recoverable and pass CRC.
- No block data is silently corrupted (i.e., a block that passes CRC contains valid field ranges).

**Pass Criteria**
- Filesystem check passes or repairs cleanly.
- ≥ 324 A-blocks (90% of 360) recovered with passing CRC.

---

### FW-INT-005 — Watchdog: Task Restart After Simulated Hang

| Field | Value |
|-------|-------|
| **ID** | FW-INT-005 |
| **Title** | FreeRTOS watchdog detects a hung task and triggers system restart |
| **Component** | `task_adc`, ESP-IDF task watchdog timer (TWDT) |
| **Hardware** | USB serial console for log capture |

**Preconditions**
- Firmware built with `CONFIG_ESP_TASK_WDT=y` and `CONFIG_ESP_TASK_WDT_TIMEOUT_S=5`.
- Debug build with a hook: a magic command byte (e.g., `0xFF` on CONTROL characteristic) causes `task_adc` to enter an infinite loop without feeding the watchdog.
- Serial console open at 115200 baud.

**Steps**
1. Connect serial console; start device; confirm normal log output (1 kHz ADC, SD write).
2. Send magic hang command `0xFF` to CONTROL characteristic.
3. Observe serial output for up to 10 seconds.
4. Record timestamp of watchdog panic message.
5. Observe automatic reboot.
6. After reboot, wait 15 seconds and confirm normal operation resumes.

**Expected Result**
- Watchdog panic log appears within 5 s ± 1 s of hang injection.
- Log contains `"Task watchdog got triggered"` (ESP-IDF TWDT message) and identifies `task_adc`.
- Device reboots and returns to normal acquisition within 15 s.

**Pass Criteria**
- Panic within 6 s of hang injection.
- Correct task identified in watchdog message.
- Normal ADC log output resumes within 15 s post-reboot.

---

## 7. iOS Unit Tests (XCTest)

### IOS-UNIT-001 — DeltaDecoder: Round-Trip Encode / Decode

| Field | Value |
|-------|-------|
| **ID** | IOS-UNIT-001 |
| **Title** | DeltaDecoder round-trip encode then decode returns original values |
| **Component** | `DeltaDecoder` |
| **Framework** | XCTest |

**Preconditions**
- `DeltaDecoder` struct/class has `encode(_ samples: [Int32]) -> [Int16]` and `decode(_ deltas: [Int16], first: Int32) -> [Int32]` methods (or equivalent).
- Test target includes `DeltaDecoder`.

**Steps**
1. Define `original: [Int32]` = 250 synthesised ECG-like values generated by `(0..<250).map { Int32(sin(Double($0) * 0.1) * 5000) }`.
2. Call `encoded = DeltaDecoder.encode(original)`.
3. Call `decoded = DeltaDecoder.decode(encoded, first: original[0])`.
4. Assert `decoded.count == 250`.
5. For each index `i` in 0..<250, assert `decoded[i] == original[i]`.

**Expected Result**
- All 250 values are identical after round-trip.

**Pass Criteria**
- `XCTAssertEqual(decoded[i], original[i])` passes for all i.

---

### IOS-UNIT-002 — DataBlocks.parse(from:): A-Block Correct Field Extraction

| Field | Value |
|-------|-------|
| **ID** | IOS-UNIT-002 |
| **Title** | ABlock.parse(from:) extracts all header and payload fields correctly from known binary |
| **Component** | `ABlock` (Swift model) |
| **Framework** | XCTest |

**Preconditions**
- `ABlock.parse(from: Data) throws -> ABlock` exists and corresponds to `a_block_t` layout in `data_blocks.h`.
- `a_block_t` total size = 2016 bytes (16-byte header + 2000-byte payload).

**Steps**
1. Construct a 2016-byte `Data` blob in Swift:
   - Byte 0: `0x41` (`BLOCK_TYPE_A`)
   - Byte 1: `0x01` (`BLOCK_VERSION`)
   - Bytes 2–3: `UInt16(2000).littleEndianBytes` (payload_len)
   - Bytes 4–11: `UInt64(1_500_000_000).littleEndianBytes` (timestamp_us)
   - Bytes 12–15: padding zero (header is 16 bytes total — bytes 12–15 are the end of `uint64_t` timestamp if using little-endian; adjust if 8-byte timestamp occupies bytes 4–11 already — use actual struct layout).
   - Bytes 16–2015: fill ECG1 samples 0–249 with `Int32(i * 10)` and ECG2 samples 0–249 with `Int32(i * -10)`, in little-endian byte order.
2. Call `let block = try ABlock.parse(from: data)`.
3. Assert `block.type == 0x41`.
4. Assert `block.version == 0x01`.
5. Assert `block.payloadLen == 2000`.
6. Assert `block.timestampUs == 1_500_000_000`.
7. Assert `block.ecg1[0] == 0`.
8. Assert `block.ecg1[1] == 10`.
9. Assert `block.ecg1[249] == 2490`.
10. Assert `block.ecg2[0] == 0`.
11. Assert `block.ecg2[249] == -2490`.

**Expected Result**
- All field assertions pass.
- No exception thrown.

**Pass Criteria**
- All 11 `XCTAssertEqual` calls pass without throws.

---

### IOS-UNIT-003 — DataBlocks.parse(from:): Rejects Truncated Data

| Field | Value |
|-------|-------|
| **ID** | IOS-UNIT-003 |
| **Title** | ABlock.parse(from:) throws on data shorter than expected block size |
| **Component** | `ABlock` (Swift model) |
| **Framework** | XCTest |

**Preconditions**
- Same as IOS-UNIT-002.

**Steps**
1. Create a `Data` blob of 100 bytes (less than 2016).
2. Call `XCTAssertThrowsError(try ABlock.parse(from: shortData))`.
3. Also test with exactly 2015 bytes (one byte short).
4. Also test with 0 bytes.

**Expected Result**
- All three calls throw a parsing error.

**Pass Criteria**
- `XCTAssertThrowsError` passes for all three inputs.

---

### IOS-UNIT-004 — DataBlocks.parse(from:): Wrong Block Type

| Field | Value |
|-------|-------|
| **ID** | IOS-UNIT-004 |
| **Title** | ABlock.parse(from:) throws when header type byte is not 0x41 |
| **Component** | `ABlock` (Swift model) |
| **Framework** | XCTest |

**Preconditions**
- Same as IOS-UNIT-002.

**Steps**
1. Construct a valid 2016-byte A-block blob as in IOS-UNIT-002.
2. Replace byte 0 with `0x4D` (M-block type).
3. Call `XCTAssertThrowsError(try ABlock.parse(from: wrongTypeData))`.

**Expected Result**
- Parse throws a type-mismatch error.

**Pass Criteria**
- `XCTAssertThrowsError` passes.

---

### IOS-UNIT-005 — SignalBuffer: Concurrent Read/Write, No Data Races

| Field | Value |
|-------|-------|
| **ID** | IOS-UNIT-005 |
| **Title** | SignalBuffer sustains concurrent appends and reads without data races or crashes |
| **Component** | `SignalBuffer` |
| **Framework** | XCTest with Thread Sanitizer (TSan) enabled |

**Preconditions**
- `SignalBuffer(seconds: 5, sampleRate: 1000)` creates a 5000-sample ring buffer as seen in `BLEManager.swift` (`ecg1Buffer`).
- TSan enabled in the test scheme: Edit Scheme → Test → Diagnostics → Thread Sanitizer.

**Steps**
1. Instantiate `SignalBuffer(seconds: 5, sampleRate: 1000)`.
2. Dispatch 4 writer tasks onto a concurrent queue; each appends 1000 samples of `[Float]` in a tight loop for 2 seconds.
3. Dispatch 2 reader tasks onto the same concurrent queue; each calls `buffer.latest(count: 1000)` in a tight loop for 2 seconds.
4. Let all tasks run; wait with `XCTestExpectation` (timeout 10 s).
5. Check TSan output; assert no races reported.
6. Assert the buffer does not crash (no EXC_BAD_ACCESS).

**Expected Result**
- TSan reports zero data race warnings.
- Test completes without crash within 10 s.

**Pass Criteria**
- Zero TSan warnings.
- No crash.
- Expectation fulfilled within 10 s.

---

### IOS-UNIT-006 — SignalBuffer: Sample Count and Capacity Rollover

| Field | Value |
|-------|-------|
| **ID** | IOS-UNIT-006 |
| **Title** | SignalBuffer holds exactly the configured sample count and discards oldest on overflow |
| **Component** | `SignalBuffer` |
| **Framework** | XCTest |

**Preconditions**
- `SignalBuffer(seconds: 5, sampleRate: 1000)` = 5000-sample capacity.

**Steps**
1. Append 3000 samples with values 0.0–2999.0.
2. Read `buffer.latest(count: 5000)` — assert count returned is 3000 (buffer not yet full).
3. Append another 3000 samples with values 3000.0–5999.0 (total 6000 appended).
4. Read `buffer.latest(count: 5000)` — assert count returned is 5000.
5. Assert first value returned is 1000.0 (samples 0–999 were dropped).
6. Assert last value returned is 5999.0.

**Expected Result**
- Buffer capacity is strictly enforced at 5000 samples.
- Oldest samples are evicted first.

**Pass Criteria**
- All count and value assertions pass.

---

## 8. iOS Integration Tests (Simulator Peripheral)

### IOS-INT-001 — BLE Connection: Connect and Subscribe All Characteristics

| Field | Value |
|-------|-------|
| **ID** | IOS-INT-001 |
| **Title** | BLEManager connects to simulator peripheral and subscribes all block and status characteristics |
| **Component** | `BLEManager` |
| **Framework** | XCTest + Simulator peripheral app |

**Preconditions**
- `ios_simulator` app running in a second iOS Simulator instance on the same Mac (uses virtual Bluetooth bridge).
- Simulator peripheral advertising service UUID `A5D5B001-5A5A-4B4B-8888-1A2B3C4D5E6F`.
- `BLEManager` instantiated in the test host.

**Steps**
1. Call `bleManager.startScanning()`.
2. Wait up to 5 s for `discoveredPeripherals` to contain the simulator peripheral.
3. Call `bleManager.connect(discoveredPeripherals[0].peripheral)`.
4. Wait up to 5 s for `connectionState` to become `.connected`.
5. Wait up to 5 s for all 7 UUIDs (A, I, M, P, S, T blocks + STATUS) to appear in `bleManager.characteristics`.
6. Assert `connectionState == .connected(...)`.
7. Assert `characteristics` contains keys for all 7 expected UUIDs.

**Expected Result**
- Connection established within 5 s of initiation.
- All 7 characteristics discovered and subscribed.

**Pass Criteria**
- `connectionState` is `.connected`.
- `characteristics.count >= 7`.
- All known UUIDs present as keys.

---

### IOS-INT-002 — Data Flow: 10 Seconds of ECG Received and Buffered

| Field | Value |
|-------|-------|
| **ID** | IOS-INT-002 |
| **Title** | App receives 10 s of A-block notifications and populates ECG buffers correctly |
| **Component** | `BLEManager`, `SignalBuffer` |
| **Framework** | XCTest + Simulator peripheral |

**Preconditions**
- Connected as in IOS-INT-001.
- Simulator peripheral configured to emit A-blocks at 4 blocks/s (250 samples × 4 = 1000 Hz) with synthetic ECG data (sine wave at 1.2 Hz, amplitude 5000 counts).

**Steps**
1. After connection, wait for simulator to start emitting A-blocks.
2. Wait 10 seconds.
3. Assert `bleManager.latestABlock` is not nil.
4. Assert `bleManager.signalQuality["ecg1"] == true`.
5. Assert `bleManager.signalQuality["ecg2"] == true`.
6. Read `bleManager.ecg1Buffer.latest(count: 5000)` — assert count == 5000 (buffer full).
7. Read `bleManager.ecg2Buffer.latest(count: 5000)` — assert count == 5000.
8. Assert no values in ecg1Buffer are exactly 0 (all filled with valid sine data).

**Expected Result**
- Both ECG buffers fully populated after 10 s.
- Signal quality flags true for ECG channels.
- Buffer contains non-zero synthetic ECG data.

**Pass Criteria**
- `latestABlock != nil`.
- Both signal quality flags true.
- `ecg1Buffer` and `ecg2Buffer` each hold 5000 non-zero samples.

---

### IOS-INT-003 — Reconnection: App Reconnects After Peripheral Disappears

| Field | Value |
|-------|-------|
| **ID** | IOS-INT-003 |
| **Title** | BLEManager transitions to scanning and reconnects automatically when the peripheral disconnects |
| **Component** | `BLEManager` |
| **Framework** | XCTest + Simulator peripheral |

**Preconditions**
- Connected and streaming as established in IOS-INT-002.
- Simulator peripheral has a "disconnect" command that terminates the BLE connection without powering off.

**Steps**
1. Confirm `connectionState == .connected`.
2. Trigger disconnect from the simulator peripheral side (simulate signal loss by terminating the peripheral's connection).
3. Wait 1 s; assert `connectionState == .disconnected`.
4. Assert `signalQuality` all values are `false` (reset on disconnect).
5. Call `bleManager.startScanning()`.
6. Wait up to 10 s for the simulator peripheral to be rediscovered.
7. Call `bleManager.connect(discoveredPeripherals[0].peripheral)`.
8. Wait up to 5 s for `connectionState` to return to `.connected`.
9. Wait 5 s; assert data flow resumes (`latestABlock` updates at least once).

**Expected Result**
- App detects disconnect within 1 s.
- State resets correctly.
- Reconnection succeeds within 15 s of disconnect.

**Pass Criteria**
- Disconnect detected and state = `.disconnected` within 1 s.
- Reconnect successful (`.connected`) within 15 s total.
- Data flowing again within 5 s of reconnect.

---

### IOS-INT-004 — Connection: Scan Stops When Bluetooth Is Off

| Field | Value |
|-------|-------|
| **ID** | IOS-INT-004 |
| **Title** | BLEManager handles Bluetooth-off state without crash and resets to disconnected |
| **Component** | `BLEManager` |
| **Framework** | XCTest + CBCentralManager mock |

**Preconditions**
- `CBCentralManager` delegate under test with a mock that can simulate `.poweredOff` state update.

**Steps**
1. Instantiate `BLEManager`.
2. Simulate `centralManagerDidUpdateState` with `central.state == .poweredOff`.
3. Assert `connectionState == .disconnected`.
4. Assert `signalQuality` all values are `false`.
5. Assert no crash.
6. Simulate `central.state == .poweredOn`; call `startScanning()`.
7. Assert `connectionState == .scanning`.

**Expected Result**
- State resets cleanly on Bluetooth-off event.
- Scanning resumes when powered on.

**Pass Criteria**
- State transitions match expected values.
- No crash.

---

## 9. End-to-End Tests

### E2E-001 — 30-Minute Recording: SD Completeness and BLE Continuity

| Field | Value |
|-------|-------|
| **ID** | E2E-001 |
| **Title** | 30-minute recording is complete on SD card and BLE stream shows no major gaps |
| **Hardware** | VU-AMS prototype + iPhone 15 |
| **Duration** | 30 minutes |

**Preconditions**
- Fully charged device (battery ≥ 4.18 V measured at test start).
- New SD card formatted FAT32; confirm available space ≥ 500 MB.
- VU-AMS worn on subject (or bench dummy load for electrodes).
- iPhone running VU-AMS iOS app; BLE connected and streaming confirmed before test start.
- Recording log enabled in iOS app (timestamps of each received A-block written to in-app log).

**Steps**
1. Confirm device `EVT_SD_READY` and `EVT_BLE_STREAMING` in status characteristic before starting.
2. Send `CMD_RECORDING_START` via iOS app control.
3. Note start time T0 from iPhone clock.
4. Allow uninterrupted recording for exactly 30 minutes.
5. Send `CMD_RECORDING_STOP` at T0 + 30:00.
6. Wait for `EVT_SD_WRITING` to clear (up to 30 s).
7. **SD verification:**
   a. Remove SD card and mount on host PC.
   b. Run block parser on the recording file.
   c. Count total A-blocks (expect 7200 ± 5: 30 min × 60 s × 4 blocks/s × 1 block per 250 ms).
   d. Verify all A-block CRCs pass.
   e. Verify timestamps are monotonically increasing with no gap > 300 ms between consecutive A-blocks.
8. **BLE verification:**
   a. Export iOS app A-block receive log.
   b. Count received A-block timestamps.
   c. Identify gaps > 500 ms between consecutive received blocks.
   d. Sum total gap duration.

**Expected Result**
- SD file contains 7195–7205 A-blocks.
- 100% of A-block CRCs pass.
- No A-block timestamp gap on SD > 300 ms.
- BLE total gap duration < 5 s over 30 minutes.
- No EVT_BLE_DROP events logged.

**Pass Criteria**
- SD block count: 7195–7205.
- CRC pass rate: 100%.
- SD gap: zero gaps > 300 ms.
- BLE cumulative gap: < 5 s.
- No system fault events.

---

### E2E-002 — Battery Runtime ≥ 28 Hours at Nominal Load

| Field | Value |
|-------|-------|
| **ID** | E2E-002 |
| **Title** | Device operates for at least 28 hours continuously before low-battery shutdown |
| **Hardware** | VU-AMS prototype, bench power monitor (e.g., Otii Arc) |
| **Duration** | Up to 32 hours |

**Preconditions**
- Battery fully charged to 4.200 V ± 0.010 V, verified with multimeter before test.
- SD card inserted with ≥ 100 GB free (or auto-rotating log files configured).
- Device not connected via BLE (BLE advertising enabled but no central connected — represents solo recording mode, typical clinical use).
- Bench power monitor logging current at 10 Hz for anomaly detection.
- Ambient temperature 20–25 °C.

**Steps**
1. Fully charge battery to 4.200 V. Confirm with multimeter.
2. Attach current probe from bench power monitor.
3. Start device; confirm normal boot, `EVT_SD_READY`, all sensors active.
4. Send `CMD_RECORDING_START`.
5. Note start time T0.
6. Allow device to run uninterrupted.
7. Monitor for `EVT_BATTERY_LOW` event (≤ 15% SOC, battery ≈ 3.56 V — calculated from linear SOC model).
8. Monitor for `EVT_BATTERY_CRIT` event (≤ 5% SOC, battery ≈ 3.15 V) and graceful shutdown sequence.
9. Note shutdown time T1.
10. Compute runtime = T1 − T0.
11. From bench power monitor log, compute mean current draw and identify any unexpected current spikes > 2× mean.

**Expected Result**
- Device runs from T0 to T1 without crash, hang, or watchdog reset.
- `EVT_BATTERY_LOW` is posted before `EVT_BATTERY_CRIT`.
- Graceful shutdown on `EVT_BATTERY_CRIT` (CMD_RECORDING_STOP triggered internally, SD flush completes, device powers off).
- Runtime T1 − T0 ≥ 28.0 hours.
- Mean current draw is within expected design budget (tbd by hardware team).

**Pass Criteria**
- Runtime ≥ 28.0 hours.
- Graceful shutdown observed (no data corruption on SD card post-test).
- No watchdog resets in firmware log during the test window.
- SD file parseable after graceful shutdown (all blocks have valid CRC).

---

### E2E-003 — Sensor Signal Validity Across All Modalities

| Field | Value |
|-------|-------|
| **ID** | E2E-003 |
| **Title** | All sensor modalities produce valid data ranges during a 5-minute recording on a human subject |
| **Hardware** | VU-AMS prototype + iPhone 15, worn on subject |
| **Duration** | 5 minutes |

**Preconditions**
- Subject: healthy adult, electrodes correctly placed (ECG and ICG per VU-AMS protocol).
- PPG sensor in contact with skin.
- SCL electrodes on non-dominant hand.
- Device fully charged and tested on bench (E2E-001 passed).

**Steps**
1. Attach all sensors to subject. Confirm electrode impedance is within acceptable range (ICG: Z0 expected 20–150 Ω per I-block `z0` field).
2. Start iOS app, connect via BLE, confirm all signal quality flags go true.
3. Start recording; collect 5 minutes.
4. Export received blocks from iOS app log.
5. For each modality verify the following value ranges:
   - **ECG (A-block `ecg1`, `ecg2`):** values within ±500 000 counts (24-bit sign-extended); R-wave peaks identifiable visually.
   - **ICG (I-block):** `z0` 20.0–150.0 Ω; `dZdt_peak` > 0; `pep_ms` 50–150 ms; `lvet_ms` 200–400 ms; `co_lpm` 2.0–8.0 L/min; `sv_ml` 40–120 mL.
   - **IMU (M-block):** accelerometer values consistent with gravity (vector magnitude ≈ 1 g = 16 384 LSB for ±2 g range); gyroscope near zero at rest.
   - **PPG (P-block):** `ppg_valid == 1`; `hr_ppg` 40–120 bpm; `spo2_pct` 90.0–100.0%.
   - **SCL (S-block):** `electrode_contact == 1`; `scl_tonic_uS` 0.5–50.0 µS.
   - **Temperature (T-block):** `skin_temp_c` 28.0–38.0 °C.

**Expected Result**
- All modalities produce at least 80% of frames within valid physiological ranges.
- Signal quality flags remain true for ≥ 90% of the test duration.

**Pass Criteria**
- Each modality: ≥ 80% of blocks within stated range.
- Signal quality flags true for ≥ 90% of duration for ECG, ICG, PPG, SCL.
- No `EVT_SYSTEM_FAULT` observed.

---

## 10. Test Execution Checklist

| ID | Title | Assignee | Result | Date |
|----|-------|----------|--------|------|
| FW-UNIT-001 | PSRAM ringbuffer push/pop | | | |
| FW-UNIT-002 | PSRAM ringbuffer overflow | | | |
| FW-UNIT-003 | PSRAM ringbuffer thread safety | | | |
| FW-UNIT-004 | Delta encoding known input | | | |
| FW-UNIT-005 | Delta encoding saturation clamp | | | |
| FW-UNIT-006 | Block assembly framing | | | |
| FW-UNIT-007 | Block assembly CRC | | | |
| FW-INT-001 | ADC 1 kHz sample rate | | | |
| FW-INT-002 | BLE throughput budget | | | |
| FW-INT-003 | SD sustained write + integrity | | | |
| FW-INT-004 | SD integrity after power cut | | | |
| FW-INT-005 | Watchdog task restart | | | |
| IOS-UNIT-001 | DeltaDecoder round-trip | | | |
| IOS-UNIT-002 | ABlock.parse() correct fields | | | |
| IOS-UNIT-003 | ABlock.parse() rejects truncated | | | |
| IOS-UNIT-004 | ABlock.parse() rejects wrong type | | | |
| IOS-UNIT-005 | SignalBuffer concurrent access | | | |
| IOS-UNIT-006 | SignalBuffer rollover | | | |
| IOS-INT-001 | BLE connect + subscribe all chars | | | |
| IOS-INT-002 | 10 s ECG data flow | | | |
| IOS-INT-003 | Reconnection after disconnect | | | |
| IOS-INT-004 | Bluetooth-off state handling | | | |
| E2E-001 | 30-minute recording completeness | | | |
| E2E-002 | Battery runtime ≥ 28 hours | | | |
| E2E-003 | All-modality signal validity | | | |

---

## 11. Defect Classification

| Severity | Definition | Example |
|----------|------------|---------|
| **S1 — Critical** | Data loss, safety risk, or complete loss of function | SD corruption after power cut; ADC silent dropout |
| **S2 — Major** | Core feature non-functional; test failure blocks integration | BLE characteristic not subscribed; CRC mismatch rate > 0 |
| **S3 — Minor** | Feature degraded but workaround exists | Throughput 1% over budget; reconnect takes 20 s instead of 15 s |
| **S4 — Cosmetic** | Display or logging issue only | Signal quality flag delayed by 1 block |

---

*Test Plan TP-001 — VU-AMS — Marcus Bell, QA & Test Engineering — 2026-05-09*
