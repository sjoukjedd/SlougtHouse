# VU-AMS Team Meeting — Product Optimization
## Meeting 001 | 2026-05-09 | Slough House

**Agenda:** Review current product status across all workstreams. Identify and prioritize optimization opportunities.

**Present:** Müller (Firmware), Chen (iOS/Apple platforms), Nair (Electronics/PCB), Vasquez (Biomedical signal processing), Reyes (Java/VU-DAMS), Bell (QA & Testing), Voss (Industrial design/housing), Hart (Art/UX/web), Quinn (Documentation), Kim (DevOps), Wolff (Regulatory/compliance)
**Chair:** Lam

---

**Lam:** Right. No speeches. Everyone knows why we're here. The product exists on paper and in firmware headers. I want to know what's broken, what's slow, and what's stupid. We go around the table. Keep it concrete. If you say "improve the UX" without a specific proposal, I'll send you back to your desk. Müller, you're first.

---

## 1. Firmware — Müller

**Müller:** Two things that are costing us, and one thing that's going to bite us.

First: the SD card is on SPI mode. We're sharing SPI2_HOST between the ADS1256 ADC and the ICM-20948 IMU. The SD is a third device on that same bus. At 1 kHz DRDY interrupts from the ADC, every SPI transaction for the SD requires bus arbitration. I've got a TODO in `config.h` right now — `evaluate SDMMC native 4-bit mode (GPIO35–40)` — that I've been kicking down the road. We need to move the SD to SDMMC native mode. It frees the SPI bus for ADC and IMU and gives us SDIO at up to 40 MHz instead of the SPI ceiling. The throughput concern for sustained recording isn't there yet, but as soon as we start buffering raw ICG waveforms — which we need to do — the SPI SD becomes a bottleneck.

Second: the PSRAM ring buffer is set at 7 MB, leaving 1 MB for the main heap. That's tight. Right now we're only storing A-blocks, I-blocks, M-blocks, P-blocks, S-blocks, and T-blocks. The I-block is per-beat, T-block is 4 Hz, P-block is 100 Hz — low volume. But the raw ICG waveform — once we add it — runs at 1 kHz on two channels just like the ECG. That doubles the A-block data rate. I want to revisit the `PSRAM_RINGBUF_SIZE` and the `PSRAM_SAFE_MARGIN` now, before we're mid-recording with a crashing buffer, not after.

**Vasquez:** That second point is directly connected to something I'm flagging in signal quality. Hold that thought. On the raw ICG — Müller, the I-block right now carries six floats per heartbeat. That's 24 bytes per beat, maybe 70 beats per minute, negligible. But the issue is I cannot validate B-point and X-point detection without the raw waveform on disk. The spec document says it explicitly. What's the actual obstacle to a new block type — call it a Z-block — carrying raw impedance samples at 1 kHz?

**Müller:** The obstacle is the SPI bus contention I just described, and queue depth. `BLE_BLOCK_QUEUE_DEPTH` is 16. A Z-block at 1 kHz, 250 samples per block same as the A-block, gives us 4 blocks per second. That's fine for SD. Over BLE it's another 2016 bytes per 250 ms on top of the A-block. We're already at the budget. I'd stream the Z-block to SD only, not BLE, and define a separate UUID in reserve. But this needs a GATT decision — Chen needs a new characteristic or we agree Z-blocks are SD-only.

**Chen:** SD-only is fine for raw ICG. The live display on iPhone uses the I-block for haemodynamic parameters. No one needs raw impedance on the phone in real time.

**Müller:** Good. Then the Z-block is firmware work. I can spec it. One more thing — the watchdog. FW-INT-005 in Bell's test plan requires `CONFIG_ESP_TASK_WDT_TIMEOUT_S=5`. I haven't confirmed that's actually set in `sdkconfig`. Bell, that test is blocked until I verify the build config. I'll do it today.

**Bell:** Noted. I flagged that as a precondition — it should be in the defect tracker.

---

## 2. Signal Quality — Vasquez

**Vasquez:** Three observations.

First, and this connects to what Müller said: the I-block `pep_ms` field is R-to-B interval, not Q-to-B. I documented this in SP-001 section 3.5. True PEP is Q-to-B. We are systematically overestimating PEP by the Q-to-R interval, which at resting heart rate is roughly 30–50 ms. For absolute PEP values that's a significant bias. For tracking within-subject PEP change over time — which is the stress metric — the bias is constant, so it washes out. But the moment someone compares our PEP values against published normative data, they'll see a 30–50 ms offset and call our device wrong. We need Q-wave detection. It's future work in the spec, but I want it on the priority list.

Second: the SCL architecture decision is still open. Nair's document — the component selection — has an unresolved item: does the AD5933 output digital impedance values over I2C to the ESP32 firmware, or do we want analog voltages on ADC channels 6 and 7? I need to close this. My preference is digital I2C — it gives us calibrated impedance directly, avoids adding two more analog channels to an already saturated ADC, and keeps the SCL pipeline clean. But Nair flagged that if we go analog-to-ADC, we get better time alignment with ECG and ICG samples since everything runs through the same ADS1256. For SCR event timing — skin conductance responses time-locked to a stimulus — that alignment matters. Nair and I need to resolve this before the schematic is captured.

**Nair:** I'm leaning toward the digital path. The AD5933 computes impedance on-chip — the DFT is done in hardware. The I2C result is already calibrated. Running analog wires from the AD5933 output DAC to ADC channels 6 and 7 adds layout complexity and introduces noise coupling on what is already a constrained 52×25 mm board. If timing alignment is the concern, we synchronise the AD5933 polling to the ADS1256 DRDY interrupt in firmware. Müller, is that feasible?

**Müller:** Yes. DRDY fires at 1 kHz; we poll the AD5933 at 100 Hz — every 10th DRDY. The jitter will be sub-millisecond. That should be adequate for SCR event timing.

**Vasquez:** Agreed. Let's close that item today. Digital I2C, polling synchronised to DRDY at 100 Hz. Nair, update the component doc. Müller, that's a firmware task.

**Vasquez:** Third point: the ECG quality index in SP-001 section 6.1 defines an SQI based on power ratio in the QRS band versus noise band. That SQI exists in the spec for VU-DAMS offline analysis. It does not currently exist in firmware or in the iOS app. Bell's test E2E-003 checks signal quality flags, but those flags are set by the iOS app based on simple presence checks, not a real SQI. We should implement at least a basic flat-line detector and saturation check in firmware — two of the three SQI criteria — and expose a quality byte in the STATUS characteristic so the iOS app displays something meaningful.

**Chen:** The STATUS characteristic `A5D5B009` exists. Right now we're publishing battery level and event bits. Adding an ECG quality byte is straightforward — one field change to the status payload, and I update the iOS parser. I support this.

---

## 3. iOS App — Chen

**Chen:** Two proposals.

One: the BLE queue depth for the iOS side. When BLE drops notifications — which Bell's FW-INT-002 test is designed to catch — the iOS app currently has no recovery path. It just drops the block and continues. For the live display that's acceptable. For the recording completeness check in E2E-001, the SD card is the authoritative source, not BLE. But researchers sometimes run the device without an SD card — firmware supports this, nothing in config.h prevents it. In that case, BLE IS the only storage path, and a dropped BLE block means lost data. I want to add a sequence number to the status characteristic — the ESP32 publishes a block counter, and the iOS app can detect gaps. If it detects a gap, it can request retransmission via the CONTROL characteristic.

**Müller:** Retransmission over BLE from PSRAM is feasible — the ring buffer holds 7 MB, that's roughly 3400 A-blocks, about 14 minutes of ECG at 1 kHz. As long as the iOS app requests the retransmit within that window it can recover. But this is a non-trivial firmware feature. I'd need a new command type in `system_cmd_t` and a BLE protocol extension. Not this sprint.

**Chen:** Agreed — not this sprint. But I want it on the roadmap. Second item, and this is smaller: the `BLE_BLOCK_QUEUE_DEPTH` is 16. At 4 A-blocks per second, that's 4 seconds of buffer. If the iOS app is in the background and CoreBluetooth delivery is delayed — which happens when the phone screen is off — we can get 3–5 second delivery gaps according to Apple's CoreBluetooth documentation. 16 blocks is just barely enough. I want to raise `BLE_BLOCK_QUEUE_DEPTH` to 32. It's a two-character change in `config.h`. What's the memory cost, Müller?

**Müller:** A-block is 2016 bytes. 32 × 2016 = 64 KB of queue space in internal SRAM. That's acceptable. But the queue holds pointers to PSRAM blocks, not copies — so it's actually 32 × 4 bytes = 128 bytes of queue memory plus the 2016-byte blocks already in PSRAM. No meaningful overhead. I'll change it.

**Chen:** Good. Also — and I should have said this at the start — the simulator peripheral app referenced in Bell's IOS-INT-001 test. It doesn't exist yet. I need to build it. Bell has tests blocked on it. That's a priority.

**Bell:** Yes, IOS-INT-001 through IOS-INT-004 are all BLOCKED pending the simulator peripheral. I've flagged this.

---

## 4. PCB/Electronics — Nair

**Nair:** Two concerns beyond what I already covered with Vasquez.

First: the ADS1256 ADC reference voltage. Vasquez's SP-001 says "confirm with Nair — nominal 2.5 V." The selected reference is the REF5025 — 2.5 V, ±0.05% initial accuracy. But the ADS1256 in differential mode has a full-scale range of ±V_REF, so ±2.5 V. ECG signal after the INA333 at G=100 is in the ±250 mV range — we're using only 10% of the ADC's dynamic range. That's 21 effective bits on the ECG, which is fine, but we're wasting headroom. I want to revisit the INA333 gain resistor value and increase gain to G=500 or G=1000 to bring the conditioned ECG signal up to ±1 V. That gives us better noise performance at the ADC input and doesn't require changing any silicon. Vasquez needs to confirm the maximum ECG amplitude expected — if a subject has a large QRS of 5 mV, at G=500 that's 2.5 V, right at the ADC rail. We need a clipping analysis.

**Vasquez:** Typical ECG amplitude at the surface is 0.5–5 mV. Worst case 5 mV at G=500 is 2.5 V — yes, that clips. G=400 gives 2.0 V for a 5 mV input, within the ±2.5 V window with 500 mV margin. I'd say G=400 as a target. Run it past me before you change the resistor.

**Nair:** Agreed. Second: the PCB is 52×25 mm. The AD630 balanced demodulator is an SOIC-16 package — 10×6 mm footprint. Combined with the AD8221 SOIC-8 and the ADS1256 SSOP-28, the lower-left quadrant of the board is very crowded. I'm concerned about routing the 32 kHz carrier signal from the SG-210STF oscillator to the AD630 reference input and the ESP32 GPIO monitor pin without picking up noise from the SPI bus running alongside. The SPI CLK at 1.92 MHz is a harmonic risk for the 32 kHz carrier path. I need to add a guard ring around the demodulator reference trace and consider moving the oscillator to the top edge of the board. This is a layout decision that affects the mechanical cutout — Voss needs to know.

**Voss:** I've got the oscillator in the current housing sketch at the bottom-right. If it moves to the top edge, I need to revise the internal standoff positions. It's not a crisis but tell me before tape-out, not after.

---

## 5. Housing — Voss

**Voss:** Two things from my side.

First: the electrode connector placement. The current housing design has the ICG current-source electrodes (front chest, back) exiting through the top edge of the enclosure. The ECG electrodes exit from the bottom. When a researcher puts this on a subject, they're threading four cables from two different edges simultaneously. That's a workflow problem. Every wearable study I've seen with multi-lead biopotential devices bundles connectors on one face. I want to revise the routing to bring all four electrode cables out from the bottom edge, aligned in a row. It requires changing the PCB layout — specifically where the protection resistors and electrode connectors land on the lower PCB — but the signal routing doesn't change.

**Nair:** Bottom edge is possible. The protection resistors are near the electrode headers at the moment. I'd need to re-route, but it's not a deep change. This makes sense from a PCB perspective too — keeps the high-impedance analog front-end traces away from the BLE antenna area on the upper PCB.

**Voss:** Good. Second: the USB-C port. Right now it's on the right face of the enclosure. I want it on the bottom, next to the electrode connectors, so charging and the electrode harness are all on one face and the other three faces are clean. The `PIN_USB_DETECT` signal is already GPIO2 — just a cable routing change internally. The mechanical change is a face milling revision. Minimal.

**Hart:** From a user-facing perspective, single-face cabling is a significant UX improvement. The current layout in the design mockups looks like a device that grew connectors randomly. Consolidating to one face also gives me a clean visual on the packaging renders — right now I can't show a clean product photo from any angle. I support both of Voss's proposals.

---

## 6. Testing — Bell

**Bell:** I have three coverage gaps to flag, not two.

First: there are no tests for the I-block production pipeline. The firmware computes `z0`, `dZdt_peak`, `pep_ms`, `lvet_ms`, `co_lpm`, `sv_ml` in real-time on the ESP32. There are zero unit tests for any of those computations. FW-UNIT-006 tests block assembly framing — it doesn't validate the numerical content of the I-block. If the B-point detection in firmware produces wrong PEP values, we have no automated way to catch it. I want to add FW-UNIT-008 through FW-UNIT-012 covering the haemodynamic parameter computations with known synthetic ICG waveforms.

**Vasquez:** I can provide reference waveforms. I have synthetic ICG signals with known B-point and X-point locations — I use them to validate the VU-DAMS algorithm. I'll package them as test fixtures for Müller's Unity test suite.

**Müller:** I need them as C arrays. Can you export as a header file?

**Vasquez:** Yes.

**Bell:** Second: E2E-002 — the 28-hour battery test — has no defined pass criterion for mean current draw. The test plan says "tbd by hardware team." That's a gap. We can't run the test properly without a current budget. Nair, what's the estimate?

**Nair:** Back-of-envelope: ADS1256 at 1 kHz continuous is about 1.65 mA. INA333 ×2 = 100 µA. AD8221 = ~1 mA. AD630 = ~7 mA. ICG current source 1 mA constant. LT3042 quiescent = 2 mA. ICM-20948 = ~3 mA. MAX30101 = ~1 mA. AD5933 = ~15 mA. ESP32-S3 active dual-core at 240 MHz = ~80 mA. BLE advertising idle = ~5 mA on top of that. Rough total: 115–120 mA average. SD write bursts add ~30–50 mA peak. If we're budgeting for a 3400 mAh cell at 120 mA average, that's 28.3 hours. Tight against the 28-hour requirement. Very tight.

**Bell:** That's the number I need documented in the test plan. I'll update E2E-002 with 120 mA ± 20 mA as the current budget acceptance range.

**Nair:** The AD5933 at 15 mA is the outlier. If we reduce its measurement duty cycle — continuous measurement isn't needed for SCL, 100 Hz polling but with the AD5933 in standby between measurements — we could cut that to 3–4 mA average. Fifteen milliamps on a device that runs 28 hours is significant.

**Müller:** I can add an AD5933 sleep/wake cycle in firmware. It supports standby mode via I2C command. If we only need 100 Hz, I ping it, read the result, put it back to sleep. Active for perhaps 5 ms per measurement. That's 0.5% duty cycle — drops the AD5933 contribution from 15 mA to under 0.1 mA average. I'll do it.

**Bell:** Third gap: there are no tests for the delta codec decode path on iOS. FW-UNIT-004 and FW-UNIT-005 test the ESP32-side encoder. IOS-UNIT-001 tests a round-trip. But there is no test where we take a known firmware-encoded byte sequence — binary, as it would arrive from real hardware over BLE — and verify the iOS decoder produces the original int32 values. The round-trip test uses the iOS encoder on both ends. We need a cross-platform binary compatibility test. I'll write it once I have a reference encoded binary from Müller's test suite.

**Müller:** I'll add a test output artifact to FW-UNIT-004 — dump the `out[]` array to a file during the host test. Bell can use that binary.

---

## 7. Regulatory — Wolff

**Wolff:** I'm going to be concise because the team tends to glaze over when I talk, but this matters.

First: the ICG current source. Nair's component doc flags at the bottom that the IEC 60601-1 auxiliary patient current limit at 32 kHz requires explicit regulatory review. The document estimates approximately 1 mA is permissible by interpolation, and our design targets exactly 1 mA. That is too close to the limit to proceed without a formal calculation. Table 1 of IEC 60601-1 gives the f-dependent limit for AC auxiliary current as 100 µA × (f_kHz / 1 kHz) for frequencies between 1 kHz and 100 kHz, capped at 10 mA at 100 kHz. At 32 kHz: 100 µA × 32 = 3.2 mA. So the actual limit is 3.2 mA, not 1 mA as the document implies. Our 1 mA target is well within the limit. But Nair needs to fix that note — the number in the document is wrong and if a notified body reads it, we look like we're operating at the limit.

**Nair:** Thank you. I'll correct it. I was interpolating linearly without applying the frequency scaling correctly.

**Wolff:** Second: the device classification. We're targeting IEC 60601-1 Type BF — battery-only, floating patient circuit. That's appropriate for a research-grade wearable. But as soon as we add the USB-C charge port, we have a potential mains-connected patient circuit if someone charges while wearing the device. The `PIN_USB_DETECT` GPIO exists specifically to detect USB connection. Müller, the firmware must enforce a hard lock: if `PIN_USB_DETECT` is high, recording is prohibited and any active recording must halt with a status event. This is not optional — it is a Type BF classification requirement. Is this currently implemented?

**Müller:** It is not. There is USB detection in config.h but no recording interlock in firmware. I'll add it. It's a check in the `CMD_RECORDING_START` handler and a monitoring check on `EVT_BLE_CONNECTED` / recording state. Simple logic.

**Wolff:** Good. That needs to be tested. Bell, add a test case: FW-INT-006, assert that recording cannot start when USB is detected.

**Bell:** Will do.

---

## 8. VU-DAMS — Reyes

**Reyes:** I've got the SP-001 spec open and two issues.

First: the raw ICG waveform problem. Section 3 of the spec has a long note saying the A-block does not carry ICG data — only ECG channels 1 and 2. The waveform-based pipeline in sections 3.1 through 3.6 — B-point detection, X-point detection, dZ/dt derivation — cannot be executed until a future Z-block is defined and implemented. For now I'm consuming I-block fields directly per section 3.7. But I've implemented all of sections 3.1 through 3.6 in VU-DAMS already, anticipating the Z-block. They sit dormant. Every time I run the code against a real recording file, those methods return empty results and log a warning. This creates noise in the output and confuses the validation runs. I want either: Müller commits to a Z-block timeline so I can plan, or Müller formally says "not this release" and I gate those methods behind a feature flag so they stop generating warnings.

**Müller:** Z-block is next sprint if we resolve the SPI bus issue by moving SD to SDMMC. If SD moves off SPI, I have the bus bandwidth for a third ADC channel read, and I can assemble a Z-block. I'd say: feature flag it for now, Z-block comes in sprint 2.

**Reyes:** Fine. I'll add a `hasRawICGWaveform` flag to the file parser — checks for the presence of Z-block type in the file header, gates the waveform pipeline. Second issue: the electrode distance parameter `L` in the Kubicek formula is currently hardcoded in my test implementation at 20 cm, which is a rough average. SP-001 says it must be read from the participant data file header. That file header format does not exist yet — there is no specification for the recording metadata file. Every recording needs a header with subject ID, electrode distance, and at minimum a recording start timestamp in wall-clock time. Right now the only timestamp is `esp_timer_get_time()` in the blocks, which is device uptime in microseconds — not wall clock. When does the recording start relative to real time? There's no way to know from the current file format.

**Müller:** The STATUS characteristic carries the system timestamp. On iOS, the app knows wall-clock time at connection. The iOS app can write a metadata record — date, subject ID, electrode distance L — to SD at recording start via the CONTROL characteristic. I'd need a new command format. Or the iOS app writes a sidecar file over the BLE connection.

**Chen:** I'll design the metadata sidecar. It's a small JSON or binary header written to a known filename alongside the recording file. The iOS app populates it at `CMD_RECORDING_START` with timestamp, subject ID, and whatever parameters are needed. Reyes, you tell me what fields you need and I'll build the schema.

**Reyes:** Minimum: `recording_start_unix_ms` (wall clock), `electrode_distance_L_cm`, `subject_id`, `ams_firmware_version`, `adc_vref_mv` (so Vasquez's voltage conversion formula has the correct Vref). I'll send you the full list.

---

## 9. Documentation — Quinn

**Quinn:** I've been reviewing what exists. The gap is structural, not incidental.

First: there is no system-level architecture document. We have `config.h`, `data_blocks.h`, the electronics component selection, the signal processing spec, and the test plan. None of these documents explains how the system works end to end — how a sample acquired by the ADS1256 travels through the firmware task graph, onto the PSRAM ring buffer, simultaneously to the SD writer and the BLE streamer, arrives on the iPhone, gets decoded and displayed, and eventually ends up analysed in VU-DAMS. A new team member has no map. I want to write one — two pages maximum, block diagram plus narrative. I need Müller to walk me through the task architecture and Chen to walk me through the iOS data flow.

**Müller:** Schedule time with me this week. One hour.

**Chen:** Same.

**Quinn:** Second: the component selection document has a section in Dutch. The oscillator rationale — the SG-210STF section — is written in Dutch. The note to Müller at the bottom is in Dutch. This is a professional technical document that may be reviewed by a notified body, by a subcontractor, or by an investor. Everything must be in English. Nair, can you translate those sections?

**Nair:** I wrote them in Dutch because the original brief for that section came from the principal in Dutch. Apologies. I'll translate. Done by end of day.

**Wolff:** Quinn is right that this matters for regulatory. A notified body reviewing a technical file with mixed-language sections will flag it immediately.

---

## 10. DevOps — Kim

**Kim:** Two observations.

First: there is no CI pipeline. None. The firmware is built manually with `idf.py build`. The iOS app is built with Xcode manually. The Unity host tests in Bell's test plan reference `idf.py -T components/vuams_tests build` — that command requires a build target that may or may not exist in the project. If Bell tries to run FW-UNIT-001 today and the `vuams_tests` component doesn't exist, the test is blocked at build, not at logic. I need to set up a GitHub Actions workflow — or equivalent — that builds the firmware, runs the Unity host tests, builds the iOS app, and runs the XCTest suite on an iOS Simulator. Until that exists, "did the tests pass" is a question that only gets answered when someone manually runs them, which means they often don't get run.

**Bell:** This is directly related to why my test execution checklist in TP-001 is empty in every column. I can't consistently execute tests that require manual environment setup on each run.

**Kim:** Exactly. I'll start with the firmware host test pipeline — it's the cheapest to automate since it runs on Linux with the ESP-IDF docker image. iOS CI requires macOS runners which are expensive on GitHub Actions, but I can use a self-hosted runner on any Mac in the office. Second: there is no version tagging on the firmware. The `data_blocks.h` file defines `BLOCK_VERSION 0x01`. When Müller adds the Z-block type and bumps the block version, how does VU-DAMS know whether a file was recorded with firmware v1 or v2 format? There needs to be a firmware version field in the recording metadata — separate from the block version — and a git tagging convention that links firmware builds to block format versions. Reyes, you're already asking for `ams_firmware_version` in the metadata header. That's the right instinct.

**Reyes:** Yes. And VU-DAMS needs a format version table so it knows what block types to expect from a given firmware version.

**Kim:** I'll define the versioning convention and enforce it via CI tag checks. No release tag without a corresponding `BLOCK_VERSION` bump documented in a changelog.

---

## Lam's Close

**Lam:** All right. We've got a coherent picture. The critical path is: SPI bus consolidation enables raw ICG storage, which unblocks Vasquez's full algorithm pipeline, which unblocks Reyes's VU-DAMS validation. Parallel to that, the USB recording interlock is a safety requirement — Wolff's point — and it ships before anything else touches a human subject. The simulator peripheral is blocking four iOS tests. Kim's CI pipeline is blocking consistent test execution across the board.

Nobody leaves without knowing what they're doing next and when. Let's go.

---

## Action Items

| ID | Owner | Description | Deadline |
|----|-------|-------------|----------|
| ACT-001 | Müller | Evaluate and implement SD card migration to SDMMC native 4-bit mode (GPIO35–40); remove SD from SPI2_HOST | 2026-05-23 |
| ACT-002 | Müller | Define Z-block type in `data_blocks.h`; add GATT UUID for Z-block (SD-only, no BLE streaming); implement Z-block assembly in firmware | 2026-05-23 |
| ACT-003 | Müller | Increase `BLE_BLOCK_QUEUE_DEPTH` from 16 to 32 in `config.h` | 2026-05-16 |
| ACT-004 | Müller | Implement USB-C recording interlock: block `CMD_RECORDING_START` and halt active recording when `PIN_USB_DETECT` is high | 2026-05-16 |
| ACT-005 | Müller | Implement AD5933 standby/wake duty cycle in firmware; measure/confirm average AD5933 current drop | 2026-05-16 |
| ACT-006 | Müller | Confirm `CONFIG_ESP_TASK_WDT_TIMEOUT_S=5` is set in `sdkconfig`; unblock FW-INT-005 | 2026-05-13 |
| ACT-007 | Müller | Export reference encoded binary from FW-UNIT-004 for Bell's cross-platform delta codec compatibility test | 2026-05-16 |
| ACT-008 | Nair | Close SCL architecture: confirm digital I2C path (AD5933 native mode); update `component_selection_analog.md` | 2026-05-13 |
| ACT-009 | Nair | Translate Dutch sections of `component_selection_analog.md` to English (SG-210STF rationale + Müller note) | 2026-05-13 |
| ACT-010 | Nair | Correct IEC 60601-1 auxiliary current limit note in `component_selection_analog.md`: limit at 32 kHz is 3.2 mA, not ~1 mA | 2026-05-13 |
| ACT-011 | Nair | Perform INA333 gain increase analysis (target G≈400); confirm headroom against worst-case 5 mV ECG; update component doc | 2026-05-23 |
| ACT-012 | Nair | Revise PCB layout: consolidate all electrode connectors and USB-C to bottom edge; notify Voss of oscillator placement requirements | 2026-05-23 |
| ACT-013 | Vasquez | Provide synthetic ICG test fixtures (C header arrays with known B-point/X-point ground truth) to Müller for FW-UNIT-008–012 | 2026-05-16 |
| ACT-014 | Vasquez + Nair | Joint sign-off: confirm V_ref = 2.5 V (REF5025) in `signal_processing_spec_001.md` open items table; close that item | 2026-05-16 |
| ACT-015 | Chen | Build iOS Simulator peripheral app (`operations/ios_simulator`); unblock IOS-INT-001 through IOS-INT-004 | 2026-05-23 |
| ACT-016 | Chen | Design recording metadata sidecar schema (fields: `recording_start_unix_ms`, `electrode_distance_L_cm`, `subject_id`, `ams_firmware_version`, `adc_vref_mv`); implement iOS-side write at `CMD_RECORDING_START` | 2026-05-23 |
| ACT-017 | Chen | Add ECG quality byte to STATUS characteristic payload; implement display in iOS app | 2026-05-23 |
| ACT-018 | Reyes | Gate waveform-based ICG pipeline (sections 3.1–3.6 of SP-001) behind `hasRawICGWaveform` feature flag in VU-DAMS parser | 2026-05-16 |
| ACT-019 | Reyes | Implement recording metadata sidecar reader in VU-DAMS; use `electrode_distance_L_cm` for Kubicek formula | 2026-05-23 |
| ACT-020 | Bell | Update E2E-002 battery test with current budget acceptance range (120 mA ± 20 mA average) | 2026-05-13 |
| ACT-021 | Bell | Write FW-INT-006: assert recording cannot start / halts when USB-C detected | 2026-05-16 |
| ACT-022 | Bell | Write FW-UNIT-008 through FW-UNIT-012 covering I-block haemodynamic parameter computations using Vasquez's reference fixtures | 2026-05-23 |
| ACT-023 | Bell | Write cross-platform delta codec binary compatibility test using Müller's reference-encoded output (ACT-007) | 2026-05-23 |
| ACT-024 | Quinn | Write system architecture overview document: sample acquisition → firmware task graph → PSRAM → SD/BLE → iOS → VU-DAMS (max 2 pages + block diagram); schedule walkthroughs with Müller and Chen this week | 2026-05-23 |
| ACT-025 | Kim | Set up CI pipeline: firmware host test build + Unity test run (ESP-IDF Docker); iOS XCTest on self-hosted macOS runner | 2026-05-23 |
| ACT-026 | Kim | Define firmware version tagging convention; document `BLOCK_VERSION` → firmware version mapping; enforce via CI tag check | 2026-05-23 |
| ACT-027 | Voss | Revise housing mechanical design to reflect bottom-edge consolidation of electrode connectors and USB-C | 2026-05-23 |
| ACT-028 | Voss + Nair | Confirm oscillator (SG-210STF) placement on PCB top edge; update housing internal standoff design accordingly | 2026-05-23 |

---

## Priority Ranking — Top 5 Optimization Items (Consensus)

| Rank | Item | Rationale |
|------|------|-----------|
| 1 | **USB-C recording interlock (ACT-004)** | Safety-critical. Type BF classification requires it before any human subject recording. Blocks all clinical use. |
| 2 | **SD → SDMMC migration (ACT-001)** | Unblocks raw ICG storage (Z-block), which is required for validated PEP/LVET computation and Vasquez's full algorithm pipeline. All haemodynamic accuracy improvements depend on it. |
| 3 | **Raw ICG Z-block implementation (ACT-002)** | Directly enables B/X-point detection in VU-DAMS, unlocks validated SV and CO computation, and removes the largest known gap between firmware outputs and the signal processing spec. |
| 4 | **AD5933 duty-cycle power reduction (ACT-005)** | Estimated 14 mA average current saving moves the 28-hour battery runtime from a margin risk to comfortable headroom. Without it, E2E-002 is borderline. |
| 5 | **CI pipeline (ACT-025)** | All 25 tests in TP-001 are currently manual-only. Without automated execution, regression is invisible. This is a multiplier on every other improvement — it is the mechanism by which we know the other four items are actually done. |

---

*Meeting 001 closed.*  
*Next meeting: 2026-05-23 | Slough House*

*"We don't celebrate finishing things. We just start the next thing." — Lam*
