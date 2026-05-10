# VU-AMS Signal Processing Specification — HAR & SAD Modules
**Document ID:** SP-002  
**Author:** Dr. Elena Vasquez — Biomedical Signal Processing Engineer  
**Date:** 2026-05-10  
**Status:** Draft v1.0  
**Audience:** Reyes (Java offline analysis / VU-DAMS), Müller (firmware), Nair (hardware)  
**Related documents:** SP-001 (signal_processing_spec_001.md), data_blocks.h, config.h

---

## Purpose

This document specifies two new analysis modules for VU-DAMS:

1. **SAD** — Speech Activity Detection from the chest accelerometer
2. **HAR** — Human Activity Recognition (posture and locomotion classification)

Both modules consume M-block IMU data and produce per-epoch output that integrates with the physiological metrics defined in SP-001. This document is authoritative. Deviations require Vasquez sign-off.

---

## Hardware Context (from config.h and data_blocks.h)

- **IMU:** ICM-20948 9-axis (accelerometer + gyroscope + magnetometer)
- **Current IMU ODR:** 100 Hz (`IMU_SAMPLE_RATE_HZ 100` in config.h)
- **M-block layout:** 10 samples per block, 9 channels (ax, ay, az, gx, gy, gz, mx, my, mz), int16_t, packed
- **Barometer:** BMP390 confirmed present — `I2C_ADDR_BMP390 0x77`, `BMP390_SAMPLE_RATE_HZ 10` (config.h). `EVT_BARO_FRAME` event bit is defined. **No B-block (barometer block) currently exists in data_blocks.h** — see Section 5.3 for the required addition.
- **Scale factor:** ICM-20948 default ±2g range: 1 LSB = 1/16384 g (confirm with Müller — open item carried from SP-001 §9).

---

## NOTICE TO MULLER — IMU ODR CHANGE REQUIRED FOR SAD

**Action required before SAD can be enabled.**

Speech Activity Detection (Part 1 of this specification) requires accelerometer data in the 80–400 Hz band to capture body-conducted vocal vibrations. The Nyquist criterion mandates a sampling rate of at least **800 Hz** for the 400 Hz upper limit of the speech band; a minimum operational ODR of **1 kHz** is specified here to provide a comfortable anti-aliasing margin and to match the ADS1256 sample rate already in use.

The ICM-20948 supports ODR up to 4 kHz on the accelerometer. The current firmware setting of 100 Hz (`IMU_SAMPLE_RATE_HZ 100`) is insufficient — it aliases all speech-band energy below 50 Hz and captures none of the vocal frequency content.

**Required change:**

- Add a `IMU_ACCEL_ODR_SAD_HZ 1000` define to config.h (or a mode-selectable ODR).
- The gyroscope and magnetometer do not need to run at 1 kHz; they can be decimated to 100 Hz for HAR. Only the accelerometer needs 1 kHz for SAD.
- The M-block struct will need a variant or a separate block type (suggest `BLOCK_TYPE_V` for "vocal/vibration accelerometer") carrying 100 int16_t samples per block (100 ms of 1 kHz data) for the three accelerometer axes only. The existing M-block (100 Hz, all 9 channels) continues for HAR.
- Data rate increase: 3 axes × 100 samples × 2 bytes = 600 bytes per block at 10 blocks/s = 6 kB/s additional SD write load. BLE transmission of 1 kHz accelerometer data will saturate the existing MTU; recommend SD-only for the high-ODR accelerometer stream.

This is a firmware architecture change. Vasquez will provide algorithm parameters; Müller decides the block format. Co-ordinate before implementing.

**Until the ODR is raised, SAD output must not be computed. The module should return all NaN and set `sad_available = false` in the output.**

---

## NOTICE TO NAIR — BAROMETER BLOCK FORMAT REQUIRED

**BMP390 is confirmed present in config.h** (`I2C_ADDR_BMP390 0x77`, `BMP390_SAMPLE_RATE_HZ 10`, `EVT_BARO_FRAME` event bit defined). The hardware is there.

**What is missing:** No `B-block` (barometer block) is defined in data_blocks.h, and no `VUAMS_B_BLOCK_UUID` is defined in config.h. The barometer data is being acquired by the firmware (the event bit exists) but is not being written to the SD file or forwarded over BLE.

**Required additions:**

1. A `BLOCK_TYPE_B 0x42` barometer block in data_blocks.h with fields: `pressure_pa` (float, Pa), `temperature_c` (float, °C), `altitude_m` (float, derived from pressure, metres above sea level using standard atmosphere). At 10 Hz this is lightweight — 12 bytes payload per block.
2. A `VUAMS_B_BLOCK_UUID` GATT characteristic in config.h for BLE streaming.
3. Müller to wire the BMP390 task to the block assembler.

Without this, stair detection in HAR falls back to accelerometer-only heuristics (Section 3.4.5, rule 4 and 5), which are less reliable. Barometric stair detection is strongly preferred for distinguishing climbing stairs from walking on a slope.

---

## Part 1: Speech Activity Detection (SAD)

### 1.1 Background and Rationale

The chest-worn VU-AMS sits on the sternum, mechanically coupled to the ribcage. Vocal cord vibrations transmit through bone and soft tissue and appear as broadband vibrations in the accelerometer signal, concentrated in the 80–400 Hz band. This band lies above typical locomotion and postural sway frequencies (< 20 Hz) and can be separated from body motion by high-pass filtering.

Detecting speech activity serves two purposes in stress research:

1. **Experimental protocol validation:** Trier Social Stress Test (TSST) and related paradigms use speaking tasks as the primary stressor. Confirming that the subject was actually speaking during the designated intervals validates the manipulation.
2. **Naturalistic monitoring:** In ambulatory recordings, `speaking_fraction` per epoch contextualises elevated sympathetic responses — social speaking is itself a potent stressor. It allows HRV, PEP, and SCL metrics to be stratified by speaking vs. non-speaking intervals post-hoc.

**Fundamental limitation:** The method cannot distinguish the subject's own voice from ambient speech (other people speaking nearby). Body-conducted vibrations from another person's voice are attenuated but not eliminated, particularly at close range or high vocal intensity. This limitation is flagged in the output via `ambient_noise_suspected`. A directional microphone or throat microphone would resolve this ambiguity but is not available in the current hardware revision.

### 1.2 Input Signal Requirements

**Required:** Accelerometer data at Fs ≥ 1000 Hz (see Notice to Müller above).

**Signal source:** M-block accelerometer axes ax, ay, az (int16_t LSB).

**Pre-processing:** Convert to physical units (g) before processing:

$$a_{axis}[n] = \frac{\text{raw}_{axis}[n]}{16384} \quad \text{(±2g range, 1 LSB = 1/16384 g)}$$

**Accelerometer magnitude:**

$$a_{mag}[n] = \sqrt{a_x^2[n] + a_y^2[n] + a_z^2[n]}$$

Unit: g. All subsequent SAD processing operates on $a_{mag}$.

### 1.3 Step 1 — High-Pass Filter

Apply a **4th-order Butterworth high-pass filter** with cutoff frequency $f_c = 80$ Hz at $F_s = 1000$ Hz:

$$x_f[n] = \text{HPF}_{80\,\text{Hz},\,\text{4th order}}(a_{mag}[n])$$

**Rationale:** Removes gravity (DC), posture-change components (< 1 Hz), locomotion (1–20 Hz), and residual cardiorespiratory vibrations (< 40 Hz). Retains the vocal frequency band (80–400 Hz). The 80 Hz cutoff is conservative — fundamental vocal frequency (F0) in adults ranges 85–255 Hz (male/female), and the overtone series extends to 400+ Hz.

**Filter design:** Apply as two cascaded 2nd-order biquad sections (numerically stable). Design using bilinear transform with pre-warping at 80 Hz:

$$\omega_{pre} = 2 F_s \tan\!\left(\frac{\pi \times 80}{1000}\right) = 2000 \tan(0.2513) = 514.5 \text{ rad/s}$$

For Java implementation use the `iirj` `Butterworth` class (`HighPass` type, order 4, Fs = 1000.0, fc = 80.0). Store filter state across M-blocks — do not reinitialise at block boundaries.

**Optional anti-aliasing pre-filter:** If the IMU ODR is set to 1000 Hz and has an on-chip digital low-pass filter (DLPF), configure the ICM-20948 DLPF to the widest setting that still provides anti-aliasing below 500 Hz (Nyquist). The ICM-20948 DLPF options include 473 Hz bandwidth — use this or the nearest available option above 400 Hz. Confirm with Müller.

### 1.4 Step 2 — Short-Time Energy (STE)

Frame the filtered signal $x_f[n]$ into overlapping windows:

- **Frame length:** $N = 20$ ms = 20 samples at 1000 Hz
- **Frame hop:** $H = 10$ ms = 10 samples (50% overlap)

Short-time energy for frame $m$:

$$E[m] = \sum_{n=0}^{N-1} x_f^2[mH + n]$$

Unit: g². This is a non-normalised energy measure sensitive to the amplitude of the vocal vibrations. Do not divide by $N$ — keeping it as a sum makes the threshold scale with both amplitude and frame length, which simplifies threshold selection.

Number of frames in a 30-second epoch: $(30\,000\,\text{ms} - 20\,\text{ms}) / 10\,\text{ms} + 1 = 2999$ frames.

### 1.5 Step 3 — Adaptive Threshold

Compute an adaptive threshold using a **10-second sliding window** of STE values:

$$\mu_E[m] = \text{mean}\bigl(E[m - K \ldots m]\bigr)$$
$$\sigma_E[m] = \text{std}\bigl(E[m - K \ldots m]\bigr)$$

where $K = 10\,000\,\text{ms} / 10\,\text{ms} = 1000$ frames (the number of frames in 10 seconds).

$$\text{Thr}[m] = \mu_E[m] + 1.5 \times \sigma_E[m]$$

**Speech decision (per frame):**

$$d[m] = \begin{cases} 1 & \text{if } E[m] > \text{Thr}[m] \\ 0 & \text{otherwise} \end{cases}$$

**Minimum run rule:** Apply a **3-frame minimum** — a frame sequence $d[m]$ is classified as speech only if $d[m-2] + d[m-1] + d[m] \geq 3$ (three consecutive frames all exceed threshold). Single-frame and two-frame triggers are rejected as noise. This corresponds to a minimum speech burst of 30 ms before the post-processing minimum (Section 1.6).

**Threshold initialisation:** For the first 10 seconds of a recording (before the sliding window is populated), use the global mean and std of all available STE values up to the current frame. This produces a conservative (lower) threshold at recording onset — acceptable since false positives in the first 10 seconds will be filtered by post-processing.

### 1.6 Step 4 — Post-Processing

Apply the following rules in sequence to the binary speech decision sequence $d[m]$ (frame-level, 10 ms resolution):

**Rule 1 — Gap filling:** Merge speech segments separated by a silence gap of less than **200 ms** (< 20 frames). If two speech segments are separated by fewer than 20 non-speech frames, fill the gap and treat them as a single continuous segment.

**Rule 2 — Minimum segment duration:** Discard any speech segment shorter than **300 ms** (< 30 frames). These are impulse noise, coughs, or mechanical artefacts — not sustained speech.

**Result:** A list of speech segments, each defined by an onset time and offset time (in milliseconds from recording start).

### 1.7 Output Metrics (per 30-second epoch, aligned with SP-001 epochs)

| Field | Type | Unit | Description |
|---|---|---|---|
| `speaking_fraction` | float | 0–1 | Proportion of epoch classified as speech |
| `speech_segments` | int | count | Number of distinct speech segments in epoch |
| `mean_speech_duration_ms` | float | ms | Mean duration of speech segments in epoch; NaN if `speech_segments` = 0 |
| `ste_baseline_mean` | float | g² | Mean STE over epoch (baseline energy level, dimensionless quality indicator) |
| `ste_baseline_std` | float | g² | Std of STE over epoch |
| `ambient_noise_suspected` | bool | — | True if `ste_baseline_std / ste_baseline_mean > 0.8` (high relative variance suggests non-subject vocalisation or other broadband vibration source) |
| `sad_available` | bool | — | False if IMU ODR < 1000 Hz; all other SAD fields NaN in that case |

**Computation of `speaking_fraction`:**

$$\text{speaking\_fraction} = \frac{\sum_{m \in \text{epoch}} d_{post}[m] \cdot H}{T_{epoch}}$$

where $d_{post}[m]$ is the post-processed binary decision, $H = 10$ ms is the frame hop, and $T_{epoch} = 30\,000$ ms.

### 1.8 Integration with SP-001

- `speaking_fraction` and `speech_segments` are added to the per-epoch output table (SP-001 §7.5).
- HRV, PEP, and SCL metrics should be stratified by `speaking_fraction > 0.1` (subject speaking for > 10% of epoch) vs. `speaking_fraction ≤ 0.1` in the analysis report output.
- In TSST protocols, expected `speaking_fraction` during the speaking task is > 0.6. If the detected fraction is < 0.2 during the designated speaking interval, flag a protocol compliance warning in the recording summary.

### 1.9 Validation Targets

On a 5-minute laboratory recording with alternating 60-second speaking and 60-second silence blocks (speech from the subject, no other speakers, quiet room):

| Metric | Expected |
|---|---|
| Sensitivity (speech frames correctly detected) | > 85% |
| Specificity (silence frames correctly rejected) | > 90% |
| `speaking_fraction` during speaking blocks | > 0.70 |
| `speaking_fraction` during silence blocks | < 0.10 |

Test dataset and ground-truth labels to be provided by Vasquez.

---

## Part 2: Human Activity Recognition (HAR)

### 2.1 Background and Rationale

Physical activity is a major confound in psychophysiological stress research. HRV, PEP, and SCL are all modulated by locomotion independently of psychological state. Without activity context, it is impossible to distinguish a stress-induced HRV decrease from an ambulation artefact. HAR provides this context.

The VU-AMS is chest-worn (sternum), not wrist-worn. This placement has important consequences for signal characteristics:

- **Advantage over wrist:** Lower motion artefact from arm swing; better coupling to respiratory and cardiac vibrations.
- **Different from wrist:** Tilt angles encode trunk orientation (upright vs. lying), not hand position. Step detection works via trunk vertical acceleration, not forearm acceleration. The dominant axis for gravity is device-dependent — the device mounting orientation on the chest strap must be consistent across participants for tilt-based classification to work.
- **Axis convention:** Define Z as approximately superior-inferior (towards head), X as anterior-posterior, Y as lateral. Confirm mounting orientation with Nair.

### 2.2 Input Signals and Sampling Rates

| Signal | Source | Fs (Hz) | Block type |
|---|---|---|---|
| Accelerometer (ax, ay, az) | ICM-20948, M-block | 100 | `BLOCK_TYPE_M` |
| Gyroscope (gx, gy, gz) | ICM-20948, M-block | 100 | `BLOCK_TYPE_M` |
| Magnetometer (mx, my, mz) | ICM-20948, M-block | 100 | `BLOCK_TYPE_M` |
| Barometer (pressure) | BMP390, B-block | 10 | `BLOCK_TYPE_B` (pending — see Notice to Nair) |

**Unit conversion (same as SP-001 §6.3 and SAD §1.2):**

- Accelerometer: raw LSB / 16384 → g
- Gyroscope: confirm scale with Müller — ICM-20948 default ±250°/s range: 1 LSB = 1/131 °/s = 0.00763 °/s. Convert to rad/s: multiply by π/180.
- Magnetometer: confirm scale with Müller — ICM-20948 AK09916 magnetometer: 1 LSB = 0.15 µT.
- Barometer: BMP390 outputs compensated pressure in Pa. Altitude from pressure using hypsometric formula (see §2.7.5).

### 2.3 Feature Extraction

Features are computed on **2-second windows with 50% overlap** at 100 Hz = **200 samples per window**, with **100-sample hop** (1 second).

At 100 Hz, 2 seconds = 200 samples per window. Number of windows in a 30-second epoch = (30 − 2) / 1 + 1 = 29 windows.

#### 2.3.1 Accelerometer Features

For each window, compute the following on the converted (g) accelerometer data:

**Per-axis statistics (computed for each of ax, ay, az):**

$$\mu_{a,j} = \frac{1}{N}\sum_{n=0}^{N-1} a_j[n], \quad j \in \{x, y, z\}$$

$$\sigma_{a,j} = \sqrt{\frac{1}{N-1}\sum_{n=0}^{N-1}(a_j[n] - \mu_{a,j})^2}$$

$$\text{RMS}_{a,j} = \sqrt{\frac{1}{N}\sum_{n=0}^{N-1} a_j^2[n]}$$

**Signal Magnitude Area (SMA):**

$$\text{SMA} = \frac{1}{N}\sum_{n=0}^{N-1}\bigl(|a_x[n]| + |a_y[n]| + |a_z[n]|\bigr)$$

Unit: g.

**Acceleration magnitude:**

$$a_{mag}[n] = \sqrt{a_x^2[n] + a_y^2[n] + a_z^2[n]}$$

**RMS of magnitude:**

$$\text{RMS}_{mag} = \sqrt{\frac{1}{N}\sum_{n=0}^{N-1} a_{mag}^2[n]}$$

Unit: g.

**Zero-crossing rate of magnitude (ZCR):**

$$\text{ZCR} = \frac{1}{N-1}\sum_{n=1}^{N-1} \mathbf{1}\bigl[(a_{mag}[n] - \bar{a}_{mag})(a_{mag}[n-1] - \bar{a}_{mag}) < 0\bigr]$$

where $\bar{a}_{mag}$ is the mean of $a_{mag}$ over the window. ZCR is computed on the mean-subtracted signal, making it equivalent to a zero-crossing count of the de-meaned waveform. Unit: crossings per sample (dimensionless, range 0–0.5).

**Dominant frequency:**

Apply a 256-point FFT (zero-pad the 200-sample window to 256) to $a_{mag}$, after removing the mean:

$$X[k] = \text{FFT}\bigl(a_{mag}[n] - \bar{a}_{mag}\bigr)$$

Frequency resolution: $\Delta f = 100 / 256 = 0.391$ Hz.

Dominant frequency $f_{dom}$: the frequency bin with maximum magnitude within **0.5–10 Hz**:

$$f_{dom} = \arg\max_{f \in [0.5, 10]\,\text{Hz}} |X[f]|$$

If all bin magnitudes in this band are below a noise floor of 0.005 g (spectral amplitude), set $f_{dom} = 0$ and `no_dominant_freq = true`.

**Tilt angles** (static tilt from mean gravity vector over window):

$$\theta_x = \arctan\!\left(\frac{\mu_{a,x}}{\sqrt{\mu_{a,y}^2 + \mu_{a,z}^2}}\right)$$

$$\theta_y = \arctan\!\left(\frac{\mu_{a,y}}{\sqrt{\mu_{a,x}^2 + \mu_{a,z}^2}}\right)$$

$$\theta_z = \arctan\!\left(\frac{\mu_{a,z}}{\sqrt{\mu_{a,x}^2 + \mu_{a,y}^2}}\right)$$

Unit: radians. These angles represent the static inclination of the device axes relative to gravity. For a subject lying flat with device Z aligned vertically, $|\theta_z|$ approaches π/2 (90°). For upright posture, $|\theta_z|$ is near 0 and $|\theta_x|$ or $|\theta_y|$ approaches π/2, depending on mounting.

Note: these are the tilt angles of the mean gravity vector and are only meaningful during quasi-static postures. During motion, the dynamic acceleration contaminates the mean — this is acceptable for the threshold-based classifier since static rules already require low RMS_magnitude.

#### 2.3.2 Gyroscope Features

**Per-axis statistics:**

$$\mu_{g,j} = \frac{1}{N}\sum_{n=0}^{N-1} g_j[n], \quad j \in \{x, y, z\}$$

$$\sigma_{g,j} = \sqrt{\frac{1}{N-1}\sum_{n=0}^{N-1}(g_j[n] - \mu_{g,j})^2}$$

Unit: rad/s.

**Angular velocity magnitude RMS:**

$$\text{RMS}_{gyro} = \sqrt{\frac{1}{N}\sum_{n=0}^{N-1}\bigl(g_x^2[n] + g_y^2[n] + g_z^2[n]\bigr)}$$

Unit: rad/s.

#### 2.3.3 Magnetometer Features

**Tilt-compensated heading (yaw):**

After removing the gravitational component via tilt angles $\theta_x$, $\theta_y$, compute the tilt-compensated magnetic field components:

$$M_x^{comp} = m_x \cos\theta_y + m_z \sin\theta_y$$

$$M_y^{comp} = m_x \sin\theta_x \sin\theta_y + m_y \cos\theta_x - m_z \sin\theta_x \cos\theta_y$$

Heading (yaw angle, degrees from magnetic north):

$$\psi = \arctan2\!\bigl(-M_y^{comp},\; M_x^{comp}\bigr) \times \frac{180}{\pi}$$

Note: the sign convention on $M_y^{comp}$ depends on the coordinate system orientation. Confirm axis convention with Nair when magnetometer calibration is established.

**Heading variance over window:**

$$\text{Var}_\psi = \frac{1}{W-1}\sum_{i=1}^{W} (\psi_i - \bar{\psi})^2$$

where $\psi_i$ is the heading for each sample in the window (or computed per sub-window if single-window resolution is insufficient). High variance ($\text{Var}_\psi > 200\,\text{deg}^2$) indicates turning or rotation. Unit: deg².

**Hard-iron calibration note:** The magnetometer requires hard-iron and soft-iron calibration before heading is reliable. Calibration coefficients (offset vector and scale matrix) should be stored in the recording metadata and applied before computing $\psi$. This is out of scope for Phase 1 — heading-based features are included for completeness but should be flagged as uncalibrated unless the recording metadata confirms calibration was performed.

#### 2.3.4 Barometer Features (requires B-block — see Notice to Nair)

Barometer data is at 10 Hz; resample to align with the 100 Hz IMU timeline using linear interpolation before computing the following.

**Pressure rate of change** (computed over a 5-second sliding window centred on the analysis window):

$$\Delta P = P(t + 2.5\,\text{s}) - P(t - 2.5\,\text{s}) \quad \text{[Pa]}$$

**Altitude rate:**

$$\frac{d h}{d t} \approx -\frac{R_d \cdot T_{env}}{M \cdot g} \cdot \frac{\Delta P}{\Delta t}$$

For simplicity, use the standard approximation: $\Delta h \approx -\Delta P / 12.0$ metres per Pa (valid near sea level in standard atmosphere). This gives approximately 0.083 m per Pa, or equivalently 12 Pa per metre of altitude change.

**Stair detection thresholds:**

$$\frac{d P}{d t} > +0.5\,\text{Pa/s} \Rightarrow \text{descending (pressure increasing)}$$

$$\frac{d P}{d t} < -0.5\,\text{Pa/s} \Rightarrow \text{ascending (pressure decreasing)}$$

Note on sign: atmospheric pressure increases as altitude decreases. Ascending stairs → lower pressure → negative $dP/dt$. Confirm sign convention when B-block implementation is tested.

**Altitude change over epoch:**

$$\Delta h_{epoch} = -\frac{P_{epoch\_end} - P_{epoch\_start}}{12.0} \quad \text{[metres]}$$

If B-block is not available, set `barometer_available = false` and all barometer features to NaN.

#### 2.3.5 Step Detection and Regularity

**Step detection:** Apply a peak detection algorithm to $a_{mag}[n]$ (gravity-included signal) after bandpass filtering 0.5–5 Hz (2nd-order Butterworth, Fs = 100 Hz). Each local maximum exceeding 1.1 g and separated from the previous peak by at least 200 ms (0.2 s, corresponding to a maximum cadence of 300 steps/min) is counted as a step.

**Step regularity (autocorrelation method):**

Compute the autocorrelation of the magnitude signal $a_{mag}[n]$ (mean-subtracted) over the 2-second window:

$$R[k] = \sum_{n=0}^{N-1-k} a_{mag}^{'}[n] \cdot a_{mag}^{'}[n+k], \quad k = 0, 1, \ldots, N-1$$

where $a_{mag}^{'}[n] = a_{mag}[n] - \bar{a}_{mag}$.

Normalise: $\hat{R}[k] = R[k] / R[0]$ (so $\hat{R}[0] = 1$).

Find the dominant peak of $\hat{R}[k]$ for $k$ corresponding to lags in the range [0.4 s, 2.0 s] (cadence 0.5–2.5 steps/s). The normalised peak value is the **step regularity index**:

$$\text{step\_regularity} = \max_{k \in [40, 200]\,\text{samples}} \hat{R}[k]$$

Range: 0–1. Values > 0.7 indicate highly regular, periodic movement consistent with walking or running. Values < 0.3 indicate irregular or non-periodic motion.

The lag at the peak gives the **step period** $T_{step}$ (samples), from which cadence is derived:

$$\text{cadence\_spm} = \frac{60 \times F_s}{T_{step}} \quad \text{[steps per minute]}$$

### 2.4 Classification Algorithm

#### 2.4.1 Overview

A deterministic, threshold-based rule classifier is used for Phase 1. No machine learning or training data is required. Rules are evaluated in strict priority order — the first matching rule wins.

**Input to classifier:** The feature vector for a 2-second window, as defined in §2.3.

**Output:** One activity class code from the following set:

| Class | Code | Description |
|---|---|---|
| Lying down | LYI | Horizontal, stationary |
| Sitting | SIT | Upright trunk, seated, stationary |
| Standing | STD | Upright trunk, stationary |
| Walking | WLK | Bipedal locomotion, 1–2.5 Hz cadence |
| Running | RUN | Bipedal locomotion, > 2 Hz, high impact |
| Cycling | CYC | Pedalling, low trunk angular velocity |
| Climbing stairs (ascending) | STU | Step frequency + pressure decrease |
| Descending stairs (descending) | STD_DOWN | Step frequency + pressure increase |
| Unknown / transition | UNK | No rule matched |

#### 2.4.2 Rule Definitions

Rules are applied in the order listed. All thresholds operate on the feature values for the current 2-second window. Gravity-inclusive RMS_mag includes the ~1g gravity contribution; this is intentional — the rules are calibrated to g-inclusive values.

**Rule 1 — Lying:**

$$|a_z^{mean}| > 9.0\,\text{m/s}^2 \quad \text{AND} \quad \sqrt{(\mu_{a,x})^2 + (\mu_{a,y})^2} < 1.5\,\text{m/s}^2 \quad \text{AND} \quad \text{RMS}_{mag} < 1.2\,\text{g}$$

Note: 9.0 m/s² ≈ 0.918 g. The condition $|\mu_{a,z}| > 0.918$ g with low lateral mean acceleration indicates the device Z-axis is aligned with gravity, i.e., the subject is supine with the chest facing up. RMS_mag < 1.2 g confirms no dynamic movement. The low lateral condition rules out standing with a tilted device.

**Rule 2 — Sitting:**

$$\bigl(|\mu_{a,x}| > 7.0\,\text{m/s}^2\; \text{OR}\; |\mu_{a,y}| > 7.0\,\text{m/s}^2\bigr) \quad \text{AND} \quad \text{RMS}_{mag} < 1.3\,\text{g}$$

Note: 7.0 m/s² ≈ 0.714 g projected onto a horizontal axis indicates forward or lateral trunk tilt consistent with a seated posture where gravity projects more strongly onto X or Y than Z. This rule follows Lying in priority because Lying has a stricter Z-axis criterion — sitting with moderate forward lean would otherwise match here. RMS_mag < 1.3 g confirms stationary.

**Rule 3 — Standing:**

$$|\mu_{a,z}| > 8.5\,\text{m/s}^2 \quad \text{AND} \quad \text{RMS}_{mag} < 1.15\,\text{g} \quad \text{AND} \quad f_{dom} < 0.5\,\text{Hz}$$

Note: 8.5 m/s² ≈ 0.867 g on Z with very low RMS (close to static 1g) and no dominant oscillation below 0.5 Hz indicates upright quiet standing. Stricter RMS threshold than Sitting because standing subjects exhibit less trunk flexion artefact.

**Rule 4 — Climbing stairs (ascending):**

$$\frac{dP}{dt} < -0.5\,\text{Pa/s} \quad \text{AND} \quad f_{dom} \in [1.5, 2.5]\,\text{Hz}$$

Requires barometer. If `barometer_available = false`, this rule is skipped.

**Rule 5 — Descending stairs:**

$$\frac{dP}{dt} > +0.5\,\text{Pa/s} \quad \text{AND} \quad f_{dom} \in [1.5, 2.5]\,\text{Hz}$$

Requires barometer. If `barometer_available = false`, this rule is skipped.

**Rule 6 — Walking:**

$$f_{dom} \in [1.0, 2.5]\,\text{Hz} \quad \text{AND} \quad \text{step\_regularity} > 0.7 \quad \text{AND} \quad \text{RMS}_{mag} \in [1.1, 1.6]\,\text{g}$$

**Rule 7 — Running:**

$$f_{dom} \in [2.0, 4.0]\,\text{Hz} \quad \text{AND} \quad \text{RMS}_{mag} > 1.8\,\text{g}$$

Note: Running and walking frequency bands overlap at 2.0–2.5 Hz. The RMS criterion separates them — fast walking produces lower trunk impact than running. If both rules 6 and 7 match (e.g., competitive racewalking), rule 7 takes priority because it is evaluated first.

**Rule 8 — Cycling:**

$$f_{dom} \in [0.5, 2.0]\,\text{Hz} \quad \text{AND} \quad \text{RMS}_{gyro} < 0.5\,\text{rad/s} \quad \text{AND} \quad |\mu_{a,z}| \in [8.0, 10.5]\,\text{m/s}^2$$

Note: Cycling produces a low-frequency cadence oscillation (0.5–2.0 Hz for 30–120 rpm) with low trunk angular velocity (unlike walking, which involves regular trunk rotation) and a device Z-axis near 1g (upright posture on a bicycle). The gyroscope criterion is the key discriminator between cycling and walking at similar cadences.

**Rule 9 — Unknown:**

If no rule above matched, classify as UNK.

#### 2.4.3 Post-Processing — Temporal Smoothing

Apply a **median filter with kernel width 3 windows** to the raw per-window class sequence:

The class code sequence $c[w]$ (2-second windows) is filtered such that $c_{smooth}[w]$ is the mode (most frequent class) among $c[w-1], c[w], c[w+1]$.

This eliminates single-window misclassifications at transitions. At 2-second windows it corresponds to a 6-second effective smoothing span.

**Minimum class duration:** After median filtering, remove any class segment shorter than **4 seconds** (< 4 windows) and replace with the preceding class or UNK if at recording start.

### 2.5 Epoch-Level Output (30-second epochs, aligned with SP-001)

For each 30-second epoch, aggregate the per-window classifications and feature values:

| Field | Type | Unit | Description |
|---|---|---|---|
| `epoch_start_ms` | long | ms | Epoch start timestamp from recording start |
| `activity_class` | string | class code | Dominant class (most frequent in epoch after smoothing) |
| `activity_fractions` | map | 0–1 | Fraction of epoch in each class code (sum = 1.0) |
| `step_count` | int | steps | Count of steps detected during WLK or RUN windows in epoch |
| `cadence_spm` | float | steps/min | Mean cadence during WLK or RUN windows; NaN if no WLK/RUN in epoch |
| `altitude_change_m` | float | m | Barometric altitude change over epoch; NaN if `barometer_available = false` |
| `motion_intensity` | float | 0–1 | Mean RMS_mag over epoch, normalised: (RMS_mag − 1.0) / 2.0, clamped to [0, 1] (subtracts 1g gravity contribution) |
| `barometer_available` | bool | — | True if B-block data present for this epoch |
| `har_available` | bool | — | True if M-block data present for this epoch |

**`motion_intensity` normalisation:**

$$\text{motion\_intensity} = \text{clamp}\!\left(\frac{\text{mean(RMS}_{mag}\text{)} - 1.0}{2.0},\; 0.0,\; 1.0\right)$$

At rest, mean RMS_mag ≈ 1.0 g → motion_intensity ≈ 0. Running (RMS_mag ≈ 2.5 g) → motion_intensity ≈ 0.75.

### 2.6 Integration with SP-001 Physiological Pipeline

#### 2.6.1 HRV and ICG Epoch Annotation

All per-epoch output fields in SP-001 §7.5 gain two additional columns:

| Field | Type | Description |
|---|---|---|
| `har_activity_class` | string | HAR dominant class for this epoch |
| `har_motion_intensity` | float | Motion intensity 0–1 for this epoch |

These allow post-hoc stratification of physiological metrics by activity.

#### 2.6.2 Motion Artefact Flag (update to SP-001 §6.3)

The motion artefact flag `motion_flag` in SP-001 §6.3 should be updated as follows:

**Current SP-001 rule:** Flag any 5-second epoch with $a_{RMS} > 0.15$ g (after subtracting 1g gravity) as motion-contaminated.

**Updated rule (when HAR is available):** Flag any epoch where `activity_class` ∈ {WLK, RUN, STU, STD_DOWN} as motion-contaminated, regardless of $a_{RMS}$ value.

Additionally, retain the $a_{RMS}$ threshold as a fallback for epochs where HAR is UNK — an UNK epoch with high $a_{RMS}$ is still flagged.

The compound flag condition becomes:

$$\text{motion\_flag} = \bigl(\text{activity\_class} \in \{\text{WLK, RUN, STU, STD\_DOWN}\}\bigr) \;\text{OR}\; \bigl(\text{activity\_class} = \text{UNK}\; \text{AND}\; a_{RMS} > 0.15\,\text{g}\bigr)$$

#### 2.6.3 Speaking Fraction Integration

The SAD output `speaking_fraction` and `speech_segments` are stored alongside HRV metrics in the per-epoch output. This allows statistical models to control for speaking as a stressor variable. Additionally:

- Epochs with `activity_class` ∈ {LYI, SIT, STD} and `speaking_fraction > 0.3` are flagged as "speaking episodes" in the summary report.
- Epochs with `activity_class` ∈ {WLK, RUN} and `speaking_fraction > 0.3` are annotated as "speaking while mobile" (which may artificially inflate SAD false positives due to motion vibration).

### 2.7 Data Flow in VU-DAMS (update to SP-001 §7.4 Processing Order)

Add the following steps to the processing order after step 7:

7a. Process M-blocks → HAR feature extraction (§2.3) → per-window classification (§2.4) → post-processing → 30-second epoch aggregation (§2.5).

7b. If high-ODR accelerometer block (V-block or equivalent, Fs ≥ 1000 Hz) is available: process → SAD feature extraction (§1.3–§1.5) → post-processing (§1.6) → 30-second epoch aggregation (§1.7). Otherwise set `sad_available = false`.

7c. Annotate all SP-001 per-epoch rows with HAR class and SAD speaking fraction.

7d. Recompute `motion_flag` using updated rule (§2.6.2).

### 2.8 Validation Targets

#### HAR Validation

Using a labelled test dataset (5 participants, each performing 2 minutes per activity class in the target list, device on chest strap):

| Metric | Target |
|---|---|
| Overall accuracy (29-class confusion matrix) | > 85% |
| Lying sensitivity | > 95% |
| Standing sensitivity | > 85% |
| Walking sensitivity | > 90% |
| Running sensitivity | > 90% |
| Cycling sensitivity | > 80% |
| Stair (both directions) sensitivity | > 75% |
| UNK rate on known activities | < 10% |

Confusion between STD (standing) and SIT (sitting) is expected to be the largest source of error given the chest-worn placement — these produce similar tilt angles and RMS profiles depending on seating posture. This is acceptable for motion artefact purposes since both are stationary.

#### Barometer Validation

Apply a 5-minute stair-climbing protocol (5 floors up, 5 floors down, repeated twice). Confirm:
- STU detected for ≥ 80% of ascending intervals
- STD_DOWN detected for ≥ 80% of descending intervals
- Altitude change estimate within ±15% of measured stair height (typically 3 m per floor)

---

## 5. Block Format Additions Required

### 5.1 High-ODR Accelerometer Block (V-block) — for SAD

Pending Müller's firmware decision, the suggested format for a high-ODR accelerometer block:

```c
/* BLOCK_TYPE_V = 0x56  — High-ODR accelerometer, 1 kHz, 100 samples per block */
/* payload_len = 3 × 100 × 2 = 600 bytes */
typedef struct __attribute__((packed)) {
    vuams_block_header_t header;
    int16_t ax[100];   /* Accelerometer X [raw LSB] at 1 kHz */
    int16_t ay[100];   /* Accelerometer Y */
    int16_t az[100];   /* Accelerometer Z */
} v_block_t;
```

Each block covers 100 ms. The gyroscope and magnetometer are not included — they remain in the 100 Hz M-block.

Add `VUAMS_V_BLOCK_UUID "A5D5B00B-5A5A-4B4B-8888-1A2B3C4D5E6F"` to config.h if BLE streaming is desired (not recommended at 1 kHz — SD-only is preferred).

### 5.2 Barometer Block (B-block) — for HAR stair detection

```c
/* BLOCK_TYPE_B = 0x42  — Barometer (BMP390), 10 Hz, 1 sample per block */
/* payload_len = 4+4+4 = 12 bytes */
typedef struct __attribute__((packed)) {
    vuams_block_header_t header;
    float pressure_pa;      /* Compensated pressure [Pa]        */
    float temperature_c;    /* BMP390 temperature [°C]          */
    float altitude_m;       /* Derived altitude [m], standard atmosphere */
} b_block_t;
```

Add `VUAMS_B_BLOCK_UUID "A5D5B00C-5A5A-4B4B-8888-1A2B3C4D5E6F"` to config.h.

### 5.3 Note on Existing BMP390 Firmware Integration

config.h already defines `I2C_ADDR_BMP390 0x77`, `BMP390_SAMPLE_RATE_HZ 10`, and `EVT_BARO_FRAME (1 << 4)`. The acquisition event infrastructure exists. The only missing pieces are:

1. A task reading the BMP390 and posting `EVT_BARO_FRAME`
2. The `b_block_t` struct in data_blocks.h
3. The block assembler writing B-blocks to the SD queue and BLE queue

This should be a small amount of firmware work given the infrastructure is already in place.

---

## 6. Open Items

| Item | Status | Owner |
|---|---|---|
| IMU ODR change to 1 kHz for SAD (V-block format) | Open — see Notice to Müller | Müller |
| B-block implementation (BMP390 data to file) | Open — see Notice to Nair | Müller / Nair |
| Confirm ICM-20948 gyroscope scale factor (±250°/s default?) | Open | Müller |
| Confirm ICM-20948 accelerometer scale factor (±2g → 1/16384 g/LSB) | Open — carried from SP-001 §9 | Müller |
| Confirm AK09916 magnetometer scale (0.15 µT/LSB) | Open | Müller |
| Magnetometer hard-iron / soft-iron calibration protocol | Open (Phase 2) | Vasquez / Nair |
| Confirm axis convention (Z = superior-inferior?) for tilt angle interpretation | Open | Nair |
| Validate HAR thresholds on chest-worn device data (vs. wrist-derived literature defaults) | Open — test dataset needed | Vasquez |
| SAD sensitivity / specificity validation dataset | Open | Vasquez |
| Q-wave detection for true PEP | Carried from SP-001 | Vasquez |

---

## 7. Summary of Changes to SP-001 Outputs

When this module is active, the following fields are added to the per-epoch output table defined in SP-001 §7.5:

| New field | Unit | Module |
|---|---|---|
| `har_activity_class` | class code | HAR |
| `har_activity_fractions` | map | HAR |
| `har_step_count` | steps | HAR |
| `har_cadence_spm` | steps/min | HAR |
| `har_altitude_change_m` | m | HAR |
| `har_motion_intensity` | 0–1 | HAR |
| `har_barometer_available` | bool | HAR |
| `speaking_fraction` | 0–1 | SAD |
| `speech_segments` | count | SAD |
| `mean_speech_duration_ms` | ms | SAD |
| `ambient_noise_suspected` | bool | SAD |
| `sad_available` | bool | SAD |

The `motion_flag` field is updated to use the HAR-based rule (§2.6.2).

---

*Dr. Elena Vasquez — Slow Horses Signal Processing*  
*"Classification is just organised uncertainty. Make sure your uncertainty is the right shape."*
