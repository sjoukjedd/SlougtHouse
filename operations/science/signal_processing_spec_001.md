# VU-AMS Signal Processing Specification
**Document ID:** SP-001  
**Author:** Dr. Elena Vasquez — Biomedical Signal Processing Engineer  
**Date:** 2026-05-09  
**Status:** Draft v1.0  
**Audience:** Reyes (Java offline analysis / VU-DAMS)

---

## Purpose

This document is the authoritative algorithm specification for the VU-DAMS offline analysis pipeline. It defines every signal processing stage from raw block data to physiological output metrics. Reyes must implement exactly what is described here. Deviations require Vasquez sign-off.

---

## 1. Data Ingestion and Sampling Rates

All input data is read from the binary block file written by the ESP32 firmware. The block format is defined in `firmware/main/data_blocks.h`. The common header (`vuams_block_header_t`) is 16 bytes and precedes every payload.

### 1.1 Block types and sampling rates

| Block type | Type byte | Signal | Fs (Hz) | Source (config.h) |
|---|---|---|---|---|
| A-block (`0x41`) | ECG ch1 + ch2 | ECG | 1000 | `ADC_SAMPLE_RATE_HZ 1000` |
| I-block (`0x49`) | ICG haemodynamic params | ICG-derived | per-beat | Produced by firmware |
| M-block (`0x4D`) | IMU 9-axis | Accelerometer / gyro / mag | 100 | `IMU_SAMPLE_RATE_HZ 100` |
| S-block (`0x53`) | SCL / EDA | Skin conductance | 100 | `PPG_SAMPLE_RATE_HZ 100` (AD5933 via I2C at same poll rate) |
| P-block (`0x50`) | PPG | Optical | 100 | `PPG_SAMPLE_RATE_HZ 100` |
| T-block (`0x54`) | Temperature | TMP117 | 4 | `TMP117_SAMPLE_RATE_HZ 4` |

**Critical:** The `timestamp_us` field in the header is from `esp_timer_get_time()` and represents the time of the **first sample** in the block in microseconds. Use this to reconstruct absolute sample timestamps. Do not assume contiguous blocks — gaps may exist if SD writes were dropped.

### 1.2 ECG sample reconstruction

Each A-block contains 250 `int32_t` samples per channel (250 ms of 1 kHz data). Sample values are 24-bit ADS1256 ADC values sign-extended to 32 bits. Convert to voltage before filtering:

$$V[n] = \frac{\text{raw}[n]}{2^{23} - 1} \times V_{ref}$$

where $V_{ref}$ is the ADS1256 reference voltage (confirm with Nair — nominal 2.5 V, differential, so full-scale range ±2.5 V). For algorithm purposes, processing on raw LSB counts is acceptable provided the final metric units are correctly labelled.

---

## 2. ECG Processing

**Input:** A-block `ecg1[]` at Fs = 1000 Hz  
**Output:** R-peak timestamps (ms), RRI series, HRV metrics

### 2.1 Bandpass Filter

Apply a **4th-order Butterworth bandpass filter**, passband 0.5–40 Hz, before any detection step.

**Rationale:** 0.5 Hz lower cutoff removes baseline wander without distorting the QRS complex. 40 Hz upper cutoff removes high-frequency noise (EMG, powerline harmonics above 40 Hz) while preserving QRS morphology at Fs = 1000 Hz.

**Filter design (Java — Commons Math):**

Use `org.apache.commons.math3.filter` or implement the bilinear transform manually. The recommended approach is to design as two cascaded 2nd-order sections (biquads) for numerical stability.

Butterworth pole positions for 4th-order lowpass prototype:

$$s_k = e^{j\pi(2k+n-1)/(2n)}, \quad k = 1,\ldots,4$$

Apply bilinear transform with pre-warping at both cutoff frequencies $f_1 = 0.5$ Hz and $f_2 = 40$ Hz:

$$\omega_{pre} = 2 F_s \tan\!\left(\frac{\pi f_c}{F_s}\right)$$

with $F_s = 1000$ Hz.

**Implementation note:** Use `org.apache.commons.math3.analysis.polynomials` or the `iirj` library (Apache-licensed, purpose-built for IIR filter design in Java). The `iirj` `Butterworth` class supports direct bandpass design:

```java
Butterworth butter = new Butterworth();
double[][] coeffs = butter.bandPassCoefficients(order=4, Fs=1000.0, fc=20.25, bw=39.5);
// fc = geometric mean of 0.5 and 40 Hz = sqrt(0.5*40) = 4.47 Hz
// For bandpass: design as lowpass prototype then frequency-transform
// Preferred: use iirj BandPass filter directly
```

Store filter state across blocks — do not re-initialise at block boundaries.

### 2.2 Pan-Tompkins R-Peak Detection

Apply the full Pan-Tompkins pipeline to the bandpass-filtered ECG signal at Fs = 1000 Hz.

#### Stage 1: Derivative Filter

Emphasise the steep slope of the QRS complex using a 5-point derivative:

$$y[n] = \frac{1}{8}\bigl(-x[n-2] - 2x[n-1] + 2x[n+1] + x[n+2]\bigr)$$

This approximates the first derivative and attenuates low-frequency T-waves and P-waves relative to the QRS.

#### Stage 2: Squaring

Square each sample to make all values positive and amplify large peaks relative to smaller ones:

$$y[n] = x[n]^2$$

#### Stage 3: Moving Window Integration (MWI)

Apply a rectangular moving window integrator of length $N = 150$ ms (= 150 samples at Fs = 1000 Hz):

$$y[n] = \frac{1}{N} \sum_{k=0}^{N-1} x[n-k]$$

This produces a smooth waveform with broad peaks aligned to QRS complexes.

#### Stage 4: Adaptive Thresholding

Maintain two running estimates:

- $\text{SPKI}$ — signal peak (running mean of confirmed QRS peaks in the MWI output)
- $\text{NPKI}$ — noise peak (running mean of all other peaks)

Thresholds:

$$\text{THRESHOLD1}_{I} = \text{NPKI} + 0.25 \times (\text{SPKI} - \text{NPKI})$$

$$\text{THRESHOLD2}_{I} = 0.5 \times \text{THRESHOLD1}_{I}$$

A candidate peak exceeding THRESHOLD1 is classified as a QRS. Update rules:

- If peak $\geq$ THRESHOLD1: $\text{SPKI} \leftarrow 0.125 \times \text{peak} + 0.875 \times \text{SPKI}$
- Otherwise: $\text{NPKI} \leftarrow 0.125 \times \text{peak} + 0.875 \times \text{NPKI}$

Apply a **refractory period** of 200 ms (200 samples) after each confirmed QRS — no new detection is possible within this window.

#### Stage 5: Searchback

If no QRS is detected within 1.66 × the mean RR interval, search backwards using the lower threshold THRESHOLD2 within the last RR interval window.

#### Stage 6: R-peak refinement

After a QRS is confirmed in the MWI output, locate the true R-peak in the **original (pre-filter, or bandpass-filtered) signal** by finding the maximum within ±75 ms of the MWI-detected peak time. Record the R-peak time in microseconds using the block `timestamp_us` + sample offset.

### 2.3 RRI Extraction and Ectopic Beat Rejection

Compute the RR interval series from consecutive R-peak times:

$$\text{RRI}[k] = t_{R}[k+1] - t_{R}[k] \quad \text{(milliseconds)}$$

Apply the following ectopic rejection criteria in sequence:

**Criterion 1 — Physiological bounds:** Discard any RRI outside [300, 2000] ms. These correspond to heart rates above 200 bpm (physiologically impossible in resting adults) or below 30 bpm (bradycardia artefact).

**Criterion 2 — Local deviation rule (Malik criterion):** For each RRI[k], compute the mean of the surrounding interval $\overline{\text{RRI}}_{local}$ using a 5-beat running median (beats $k-2$ to $k+2$, excluding $k$). Reject if:

$$\left|\frac{\text{RRI}[k] - \overline{\text{RRI}}_{local}}{\overline{\text{RRI}}_{local}}\right| > 0.20$$

That is, reject beats deviating more than 20% from the local median.

**Criterion 3 — Compensatory pause:** If RRI[k] is short (< 0.8 × local median) and RRI[k+1] is long (> 1.2 × local median), and RRI[k] + RRI[k+1] ≈ 2 × local median (within 10%), flag both as a premature beat + compensatory pause. Replace both with the mean of the surrounding valid beats for HRV analysis, or exclude from the series depending on the metric being computed.

Mark rejected beats with a flag; do not silently delete from the series — Reyes must preserve the flag array alongside the RRI array so Vasquez can inspect artefact rates.

**Minimum valid segment:** Require at least 60 consecutive valid beats for any HRV metric computation. If a segment has fewer valid beats, mark metrics as NaN.

### 2.4 HRV Metrics

Compute HRV metrics on artifact-free RRI segments. All time-domain metrics operate directly on the RRI series in milliseconds.

#### 2.4.1 RMSSD

Root mean square of successive differences:

$$\text{RMSSD} = \sqrt{\frac{1}{N-1} \sum_{k=1}^{N-1} (\text{RRI}[k] - \text{RRI}[k-1])^2}$$

Unit: ms. Reflects parasympathetic (vagal) tone. Primary HRV metric for stress assessment.

#### 2.4.2 SDNN

Standard deviation of NN intervals:

$$\text{SDNN} = \sqrt{\frac{1}{N-1} \sum_{k=1}^{N} (\text{RRI}[k] - \overline{\text{RRI}})^2}$$

Unit: ms. Reflects overall HRV including sympathetic and parasympathetic contributions.

#### 2.4.3 pNN50

Proportion of successive differences exceeding 50 ms:

$$\text{pNN50} = \frac{1}{N-1} \sum_{k=1}^{N-1} \mathbf{1}\bigl[|\text{RRI}[k] - \text{RRI}[k-1]| > 50\bigr] \times 100\%$$

Unit: percent.

#### 2.4.4 Frequency-Domain HRV (Welch PSD)

Compute the power spectral density of the RRI series using Welch's method.

**Pre-processing:** The RRI series is unevenly spaced in time. Resample to a uniform 4 Hz series using cubic spline interpolation before spectral analysis.

**Welch parameters:**
- Segment length: 256 points (64 seconds at 4 Hz)
- Window: Hann (Hanning) window
- Overlap: 50% (128 points)
- FFT length: 256 points
- Frequency resolution: $\Delta f = 4/256 = 0.015625$ Hz

**Frequency bands:**

| Band | Range | Physiological meaning |
|---|---|---|
| VLF | 0.003–0.04 Hz | Thermoregulation, RAAS |
| LF | 0.04–0.15 Hz | Mixed sympathetic + parasympathetic |
| HF | 0.15–0.40 Hz | Parasympathetic (respiratory) |

**Power computation:** Integrate the PSD over each frequency band using the trapezoidal rule:

$$P_{band} = \int_{f_{low}}^{f_{high}} S(f)\, df \approx \sum_{f_i \in [f_{low}, f_{high}]} S(f_i) \cdot \Delta f$$

Unit: ms².

**LF/HF ratio:**

$$\text{LF/HF} = \frac{P_{LF}}{P_{HF}}$$

Dimensionless. Values > 2.0 suggest sympathetic dominance (stress). Report alongside absolute LF and HF powers — the ratio alone is an incomplete metric.

**Implementation note:** Use `org.apache.commons.math3.transform.FastFourierTransformer` with `DftNormalization.STANDARD` and `TransformType.FORWARD`. Apply Hann window coefficients ($w[n] = 0.5(1 - \cos(2\pi n / N))$) to each segment before FFT. Average the one-sided periodograms across segments.

---

## 3. ICG Processing

> **ICG signal source — resolved 2026-05-09:**
> The raw ICG waveform (chest impedance Z and its derivative dZ/dt) is **not** stored in the A-block.
> `task_adc.c` reads ADS1256 channels 3 (`ADC_CH_ICG_SENSE`) and 4 (`ADC_CH_ICG_REF`) each conversion cycle, but only channels 0 (`ADC_CH_ECG_RA`) and 1 (`ADC_CH_ECG_LL`) are written into `a_block_t.ecg1[]` / `ecg2[]`.
> The **I-block** (`i_block_t`, type byte `0x49`) is the sole source of ICG-derived data in the current file format.
> It is produced by the firmware's block assembler (one I-block per heartbeat) and carries six `float` fields in this order:
> `z0` (baseline impedance, Ω), `dZdt_peak` (peak −dZ/dt amplitude, Ω/s),
> `pep_ms`, `lvet_ms`, `co_lpm`, `sv_ml`.
> **Consequence for Reyes:** Sections 3.1–3.6 (waveform-based B/X-point detection) cannot be executed until a future firmware version persists the raw ICG waveform in a dedicated block.
> For the current file format, use I-block fields directly for all Kubicek SV computation (Section 3.7) and cross-checks (Section 3.9).
> The erroneous statement below about `ecg2[]` carrying the impedance signal is retained for historical reference and will be removed when raw ICG storage is implemented.

**Input:** I-block `z0`, `dZdt_peak`, `pep_ms`, `lvet_ms` fields (one block per heartbeat). Raw ICG waveform is not currently stored; the A-block `ecg2[]` field carries ECG channel 2 (LL lead), not impedance. See note above.  
**Fs:** Per-beat (I-block is event-driven, not fixed-rate)  
**Output:** PEP, LVET, stroke volume, cardiac output per beat

### 3.1 ICG Bandpass Filter

Apply a **4th-order Butterworth bandpass filter**, passband 0.5–30 Hz.

Same design approach as Section 2.1. Lower cutoff 0.5 Hz removes DC and very-low-frequency impedance drift. Upper cutoff 30 Hz preserves the rapid systolic upstroke (dZ/dt peak) while rejecting high-frequency noise.

### 3.2 dZ/dt Derivation

Compute the first derivative of the filtered impedance signal $Z(t)$ using a 5-point Savitzky-Golay smoothing derivative (order 1, window 11 samples):

$$\frac{dZ}{dt}[n] = \frac{1}{252} \bigl(-2x[n-5] - x[n-4] + x[n-3] + 2x[n-2] + x[n-1] + x[n+1] + 2x[n+2] + \ldots\bigr)$$

Alternatively, use the simple central difference as a minimum viable option:

$$\frac{dZ}{dt}[n] = \frac{x[n+1] - x[n-1]}{2 / F_s}$$

Unit: $\Omega/\text{s}$. The signal is negative during systole (impedance decreases as blood enters the aorta). Report as $-dZ/dt$ so the peak is positive.

### 3.3 B-Point Detection

The B-point marks the onset of ventricular ejection (aortic valve opening). It corresponds to the **minimum of dZ/dt** (i.e., the maximum of $-dZ/dt$... wait — the B-point is specifically the **inflection point before the main dZ/dt peak**, not the peak itself).

**Correct definition:** The B-point is the **local minimum in dZ/dt that immediately precedes the large negative systolic peak**. In $-dZ/dt$ it appears as a small local minimum just before the main positive peak.

**Search window:** Search for the B-point in the window **[R-peak − 150 ms, R-peak]** (i.e., 150 samples before each R-peak at Fs = 1000 Hz).

**Algorithm:**

1. Identify the R-peak time $t_R$ from the ECG processing pipeline (Section 2.2).
2. In the $-dZ/dt$ signal, define the search window $[t_R - 150\,\text{ms},\; t_R]$.
3. Within this window, find the sample index with the **minimum value** of $-dZ/dt$ (the trough just before the upstroke). This is the B-point.
4. Record $t_B$ (B-point time in ms).

$$t_B = \arg\min_{t \in [t_R - 150\,\text{ms},\; t_R]} \left(-\frac{dZ}{dt}\right)(t)$$

**Quality check:** If no clear minimum exists (flat region), or the minimum is within 10 ms of $t_R$, flag the beat as B-point detection failure and exclude from PEP computation.

### 3.4 X-Point Detection

The X-point marks aortic valve closure (end of systole). It is the **minimum of dZ/dt after the T-wave** (i.e., a notch or trough in $-dZ/dt$ following the systolic peak).

**T-wave end estimate:** Use $t_R + 400$ ms as a conservative T-wave end estimate (adjust if heart rate-adaptive T-end detection is added later).

**Search window:** $[t_R + 200\,\text{ms},\; t_R + 550\,\text{ms}]$

**Algorithm:**

1. In the $-dZ/dt$ signal, define the window $[t_R + 200\,\text{ms},\; t_R + 550\,\text{ms}]$.
2. Find the sample with the **minimum value** of $-dZ/dt$ within this window. This is the X-point.
3. Record $t_X$ (X-point time in ms).

$$t_X = \arg\min_{t \in [t_R + 200\,\text{ms},\; t_R + 550\,\text{ms}]} \left(-\frac{dZ}{dt}\right)(t)$$

### 3.5 Pre-Ejection Period (PEP)

$$\text{PEP} = t_R - t_B \quad \text{(ms)}$$

PEP is the interval from the R-peak (electrical activation) to the B-point (mechanical onset of ejection). Unit: ms. Normal range: 60–120 ms. PEP increases under sympathetic stress.

**Note:** This definition is a simplification — true PEP should be measured from Q-wave onset to B-point. In the absence of reliable Q-detection, R-peak is used as the electrical reference. Flag in output that PEP values are R-to-B, not Q-to-B.

### 3.6 Left Ventricular Ejection Time (LVET)

$$\text{LVET} = t_X - t_B \quad \text{(ms)}$$

LVET is the duration of the ventricular ejection phase, from aortic valve opening (B-point) to aortic valve closure (X-point). Unit: ms. Normal range: 280–340 ms at resting heart rate.

### 3.7 Stroke Volume — Kubicek Formula

$$SV = \rho \cdot \frac{L^2}{Z_0^2} \cdot \left(\frac{dZ}{dt}\right)_{max} \cdot \text{LVET}$$

Where:

| Parameter | Symbol | Value / Source |
|---|---|---|
| Blood resistivity | $\rho$ | 150 $\Omega\cdot\text{cm}$ (standard value for whole blood) |
| Electrode distance | $L$ | Distance between inner (voltage-sensing) ICG electrodes in cm — **read from participant data file header; must be entered at recording setup** |
| Baseline thoracic impedance | $Z_0$ | Mean impedance during diastole, $\Omega$ — computed per beat as the mean of the Z signal in the window $[t_R - 200\,\text{ms},\; t_B]$ |
| Peak dZ/dt | $(dZ/dt)_{max}$ | Maximum of $|dZ/dt|$ during systole (the main negative peak), $\Omega/\text{s}$ — from firmware I-block `dZdt_peak` or computed directly |
| LVET | | Left ventricular ejection time in **seconds** — convert from ms: $\text{LVET}_s = \text{LVET}_{ms} / 1000$ |

**Result:** $SV$ in mL (cubic centimetres, since $\rho$ in $\Omega\cdot\text{cm}$, $L$ in cm, $Z_0$ in $\Omega$, $dZ/dt$ in $\Omega/\text{s}$, LVET in s — verify dimensional consistency in implementation).

**Dimensional verification:**

$$[\Omega\cdot\text{cm}] \cdot \frac{[\text{cm}^2]}{[\Omega^2]} \cdot [\Omega/\text{s}] \cdot [\text{s}] = \frac{\Omega\cdot\text{cm}^3}{\Omega^2} \cdot \frac{\Omega}{\text{s}} \cdot \text{s} = \text{cm}^3 = \text{mL} \checkmark$$

### 3.8 Cardiac Output

$$\text{CO} = \text{SV} \cdot \text{HR}$$

Where HR is heart rate in beats per minute. Convert to L/min:

$$\text{CO}_{L/\text{min}} = \frac{\text{SV}_{mL} \cdot \text{HR}_{bpm}}{1000}$$

This should match `co_lpm` in the I-block within ~10%. Large discrepancies indicate a B/X-point detection error — log a warning.

### 3.9 Cross-check with I-block

The firmware `i_block_t` carries `z0`, `dZdt_peak`, `pep_ms`, `lvet_ms`, `co_lpm`, `sv_ml` computed in real time on the ESP32. In VU-DAMS, compare the offline-computed values against these firmware estimates:

- If |offline PEP − firmware PEP| > 20 ms, flag beat for review.
- If |offline SV − firmware SV| > 15 mL, flag beat for review.

Offline computation is authoritative; firmware values are a quality cross-check only.

---

## 4. Respiration

**Input:** ECG channel 1, Fs = 1000 Hz  
**Output:** Respiratory rate (breaths/min), instantaneous respiration waveform

Respiratory activity modulates the ECG baseline at the breathing frequency. This is exploited to extract a surrogate respiration signal without a dedicated respiratory sensor.

### 4.1 ECG Baseline Extraction

The ECG baseline wander occupies 0.15–0.4 Hz. Extract it by applying a **2nd-order Butterworth bandpass filter** at 0.15–0.4 Hz to the raw (unfiltered) ECG signal.

**Design:** Same bilinear transform approach as Section 2.1, but 2nd-order and narrowband. At Fs = 1000 Hz this is a very narrow filter — ensure numerical precision with double-precision arithmetic throughout.

**Result:** A quasi-sinusoidal waveform tracking respiratory effort. Amplitude is not calibrated to respiratory volume; only rate is reliable.

### 4.2 Respiratory Rate

Apply peak detection to the filtered baseline waveform:

1. Find all local maxima separated by at least 1.0 s (corresponds to maximum rate of 60 breaths/min).
2. Compute inter-peak intervals (seconds).
3. Respiratory rate:

$$\text{RR}_{resp} = \frac{60}{\overline{T}_{peak}} \quad \text{breaths/min}$$

where $\overline{T}_{peak}$ is the mean inter-peak interval in seconds.

Report respiratory rate as a sliding 30-second window mean, updated every 5 seconds.

**Reliability note:** ECG-derived respiration is unreliable during motion artefacts. Cross-check against IMU data — if accelerometer RMS in the M-block exceeds a threshold (define empirically, suggest 0.1 g), mark respiratory rate as unreliable for that epoch.

---

## 5. SCL / EDA Processing

**Input:** S-block `scl_tonic_uS` and `scl_phasic_uS` — the firmware (AD5933 via I2C) already outputs tonic and phasic components. However, the raw impedance must be independently reprocessed in VU-DAMS for full control.

**Note to Reyes:** The S-block currently carries pre-computed tonic and phasic values from the firmware. For Phase 1, use these directly. The pipeline below applies if/when raw EDA samples are added to a future block format.

**Assumed raw input Fs:** 100 Hz (matching `PPG_SAMPLE_RATE_HZ` — AD5933 is polled at the same I2C cycle rate).

All amplitude units: **µS (microsiemens)**.

### 5.1 Low-Pass Filter (Anti-noise)

Apply a **4th-order Butterworth low-pass filter** at 1 Hz cutoff.

$$f_c = 1\,\text{Hz}, \quad \text{order} = 4, \quad F_s = 100\,\text{Hz}$$

This removes high-frequency electrical noise from the impedance measurement while preserving both the slow tonic baseline and the faster phasic SCR peaks (which reach up to ~0.2 Hz).

### 5.2 Tonic Component (SCL)

The tonic component is the **slow-moving baseline** of the EDA signal, representing the overall sweat gland secretion level.

Extract by applying a **low-pass filter at 0.05 Hz** (4th-order Butterworth) to the 1 Hz-filtered signal:

$$\text{SCL}(t) = \text{LPF}_{0.05\,\text{Hz}}\bigl[\text{EDA}_{filtered}(t)\bigr]$$

Alternatively, compute SCL as a 20-second moving median of the 1 Hz-filtered EDA signal — this is more robust to transient artefacts.

Unit: µS.

### 5.3 Phasic Component (SCR)

The phasic component captures fast skin conductance responses (SCRs) time-locked to psychological stimuli.

Extract by bandpass filtering the 1 Hz-filtered EDA signal at **0.01–0.2 Hz** (2nd-order Butterworth):

$$\text{SCR}(t) = \text{BPF}_{[0.01, 0.2]\,\text{Hz}}\bigl[\text{EDA}_{filtered}(t)\bigr]$$

Or equivalently, compute as:

$$\text{SCR}(t) = \text{EDA}_{filtered}(t) - \text{SCL}(t)$$

**SCR event detection:**

1. Find positive-going peaks in the SCR waveform exceeding a threshold of 0.05 µS (typical minimum SCR amplitude).
2. Minimum inter-peak interval: 1.0 s (refractory period).
3. For each detected SCR event, record:
   - Onset time (ms, referenced to recording start)
   - Peak amplitude (µS)
   - Rise time (onset to peak, ms)

Unit of all amplitudes: µS.

### 5.4 Using Pre-Computed S-Block Values (Phase 1)

When consuming S-blocks directly:

- `scl_tonic_uS`: use as the SCL tonic value for the timestamp of the block.
- `scl_phasic_uS`: use as the SCR amplitude envelope for the timestamp of the block.
- `electrode_contact == 0`: mark the entire block interval as invalid — do not interpolate across contact loss events.

Resample the 100 Hz S-block stream to align timestamps with ECG R-peaks where event-locked analysis is needed.

---

## 6. Signal Quality and Artefact Handling

### 6.1 ECG Quality Index

For each 5-second epoch, compute a signal quality index (SQI):

1. Compute the ratio of power in the QRS band (10–30 Hz) to power in the noise band (100–200 Hz, if available, otherwise 40–100 Hz after appropriate downsampling). SQI > 0.5 = acceptable quality.
2. Check for flat-line artefacts: if signal range < 50 LSB over 1 second, flag as electrode off.
3. Check for saturation: if |raw sample| > 0.95 × $2^{23}$, flag as ADC saturation.

### 6.2 ICG Quality Index

1. Check that $Z_0 \in [15, 50]\,\Omega$ — values outside this range indicate electrode placement error.
2. Check that $(dZ/dt)_{max} \in [0.5, 5.0]\,\Omega/\text{s}$ — values outside suggest noise or artefact.
3. If fewer than 70% of beats in a 60-second segment pass quality checks, mark the segment's haemodynamic metrics as unreliable.

### 6.3 Motion Artefact

IMU data (M-blocks, Fs = 100 Hz) provides motion context. Compute RMS acceleration magnitude per epoch:

$$a_{RMS} = \sqrt{\frac{1}{N}\sum_{n=1}^{N}(a_x^2[n] + a_y^2[n] + a_z^2[n])}$$

Convert from LSB to g using the IMU scale factor (confirm with Müller — ICM-20948 default ±2g range: 1 LSB = 1/16384 g).

Flag any 5-second epoch with $a_{RMS} > 0.15\,g$ (after subtracting 1g gravity) as a motion-contaminated epoch. Metrics computed in motion-contaminated epochs should be labelled as such in the output report.

---

## 7. Implementation Notes for Reyes (Java)

### 7.1 Recommended Libraries

| Purpose | Library |
|---|---|
| IIR filter design | `iirj` (https://github.com/berndporr/iirj) — Apache 2.0 |
| FFT / Welch PSD | `org.apache.commons.math3.transform.FastFourierTransformer` |
| Spline interpolation | `org.apache.commons.math3.analysis.interpolation.SplineInterpolator` |
| Statistics (RMSSD, SDNN etc.) | `org.apache.commons.math3.stat.descriptive` |

### 7.2 Filter Initialisation

- All filters must be initialised with zero state at recording start.
- **Do not re-initialise between blocks.** The signal is continuous; block boundaries are a file format artefact.
- Apply filters in causal (forward-only) mode for real-time compatibility. For offline analysis, zero-phase filtering (forward + reverse pass) is permitted and recommended for ECG baseline extraction and ICG filtering — it eliminates phase distortion. Use zero-phase filtering for all offline signal conditioning steps.
- For zero-phase: apply filter forward, reverse the output, apply filter again, reverse again. This doubles the effective filter order.

### 7.3 Timestamp Alignment

All blocks carry `timestamp_us` (uint64, microseconds). Build a unified timeline:

1. Read `timestamp_us` from each block header.
2. For A-blocks: sample $k$ within the block has timestamp $t_k = \text{timestamp\_us} + k \times 1000\,\mu\text{s}$ (since Fs = 1000 Hz, inter-sample = 1 ms = 1000 µs).
3. For M-blocks: sample $k$ has timestamp $t_k = \text{timestamp\_us} + k \times 10000\,\mu\text{s}$ (Fs = 100 Hz).
4. For S-blocks: single sample per block at `timestamp_us`.
5. Convert all timestamps to milliseconds (double precision) relative to first block timestamp.

### 7.4 Processing Order

Process signals in this order to respect dependencies:

1. Parse all blocks into memory (or use a streaming reader for large files).
2. Reconstruct ECG sample array with timestamps.
3. Apply ECG bandpass filter → R-peak detection → RRI series → HRV metrics.
4. Apply ICG bandpass filter → dZ/dt → B-point and X-point detection (requires R-peaks from step 3) → PEP, LVET, SV, CO.
5. Apply ECG baseline bandpass (0.15–0.4 Hz) → respiratory rate.
6. Process SCL/EDA from S-blocks.
7. Process IMU from M-blocks → motion artefact flags.
8. Assemble output report with per-beat and per-epoch metrics, with quality flags.

### 7.5 Output Data Format

Per-beat output (one row per cardiac cycle):

| Field | Unit | Description |
|---|---|---|
| `beat_index` | — | Sequential beat number |
| `r_peak_time_ms` | ms | R-peak time from recording start |
| `rri_ms` | ms | RR interval (current to next) |
| `rri_valid` | bool | False if ectopic-rejected |
| `pep_ms` | ms | Pre-ejection period (R-to-B) |
| `lvet_ms` | ms | Left ventricular ejection time |
| `sv_ml` | mL | Stroke volume (Kubicek) |
| `co_lpm` | L/min | Cardiac output |
| `z0_ohm` | Ω | Baseline impedance |
| `dzdt_peak` | Ω/s | Peak dZ/dt |
| `icg_quality` | bool | ICG quality flag |

Per-epoch output (one row per 5-second epoch, or configurable window):

| Field | Unit | Description |
|---|---|---|
| `epoch_start_ms` | ms | Epoch start time |
| `rmssd_ms` | ms | RMSSD for epoch |
| `sdnn_ms` | ms | SDNN for epoch |
| `pnn50_pct` | % | pNN50 for epoch |
| `lf_power_ms2` | ms² | LF power (Welch) |
| `hf_power_ms2` | ms² | HF power (Welch) |
| `lf_hf_ratio` | — | LF/HF ratio |
| `resp_rate_bpm` | breaths/min | Respiratory rate |
| `scl_mean_uS` | µS | Mean tonic SCL |
| `scr_events` | count | SCR events in epoch |
| `motion_flag` | bool | True if motion-contaminated |
| `ecg_quality` | float | SQI 0–1 |

---

## 8. Validation Targets

Reyes should validate the implementation against these reference values using the standard test dataset (to be provided by Vasquez):

| Metric | Expected range (resting adult) |
|---|---|
| RMSSD | 20–80 ms |
| SDNN | 30–100 ms |
| pNN50 | 10–40% |
| LF power | 500–2000 ms² |
| HF power | 200–1500 ms² |
| LF/HF | 0.5–3.0 |
| PEP | 70–115 ms |
| LVET | 270–340 ms |
| SV | 60–100 mL |
| CO | 4.0–8.0 L/min |
| Resp rate | 12–20 breaths/min |

Any implementation producing values consistently outside these ranges on resting-state data has an algorithm error. Run the standard test dataset before declaring any module complete and report the validation results to Vasquez.

---

## 6. Stroke Volume and Cardiac Output Calibration (Kubicek Method)

**Document scope:** This section specifies the Kubicek formula as implemented in the VU-AMS pipeline — unit conversions, subject data entry, electrode geometry, valid output ranges, and the CO derivation from HR. It is addressed primarily to Müller (firmware implementation, populating the NaN `co_lpm` and `sv_ml` fields in the I-block) and to Reyes (offline validation and cross-checking in VU-DAMS).

### 6.1 The Kubicek Formula

The Kubicek (1966) formula estimates stroke volume (SV) from thoracic impedance cardiography:

$$SV = \rho \cdot \frac{L^2}{Z_0^2} \cdot (-\dot{Z}_{max}) \cdot \text{LVET}$$

Where:

| Symbol | Name | Value / Source | Unit |
|--------|------|---------------|------|
| ρ | Blood resistivity | 135 Ω·cm (population average; see note below) | Ω·cm |
| L | Inner electrode distance | Subject-specific; entered at setup (see Section 6.2) | cm |
| Z₀ | Baseline thoracic impedance | Mean impedance during diastole; `z0` field in I-block | Ω |
| −dZ/dt_max | Peak ICG derivative magnitude | `dZdt_peak` field in I-block (positive value, already negated) | Ω/s |
| LVET | Left ventricular ejection time | `lvet_ms` field in I-block, **converted to seconds** | s |

**On ρ = 135 Ω·cm:** The original Kubicek (1966) formula used ρ = 150 Ω·cm, and SP-001 Section 3.7 uses 150 Ω·cm. This section corrects that value to 135 Ω·cm, which is the more widely cited estimate for whole-blood resistivity at 37°C based on subsequent validation studies (Patterson 1989; Sramek et al. 1983). The difference represents approximately 10% in absolute SV output. Reyes must confirm that VU-DAMS uses 135 Ω·cm in its Kubicek implementation. Update SP-001 Section 3.7 to 135 Ω·cm correspondingly.

Note: SP-001 Section 3.7 also carries a dimensional verification with ρ = 150 — the dimensional analysis is correct regardless of the constant value. The corrected value does not change the dimensional derivation.

### 6.2 Unit Conversions and Exact Formula

All quantities must be in the specified units for the formula to produce SV in mL:

$$SV\,[\text{mL}] = 135\,[\Omega\!\cdot\!\text{cm}] \times \frac{L^2\,[\text{cm}^2]}{Z_0^2\,[\Omega^2]} \times (-\dot{Z}_{max})\,[\Omega/\text{s}] \times \text{LVET}\,[\text{s}]$$

**LVET conversion (mandatory):** The I-block `lvet_ms` field carries LVET in milliseconds. Convert before applying the formula:

$$\text{LVET}_s = \frac{\text{lvet\_ms}}{1000}$$

Failure to convert will produce SV values approximately 1000× too large. This is a critical implementation trap.

**Dimensional verification:**

$$[\Omega\!\cdot\!\text{cm}] \cdot \frac{[\text{cm}^2]}{[\Omega^2]} \cdot \frac{[\Omega]}{[\text{s}]} \cdot [\text{s}] = \frac{\Omega\!\cdot\!\text{cm}^3}{\Omega^2} \cdot \frac{\Omega}{\text{s}} \cdot \text{s} = \text{cm}^3 = \text{mL} \checkmark$$

**Explicit formula for firmware (Müller):**

```c
float kubicek_sv_ml(float rho_ohm_cm, float L_cm, float z0_ohm,
                    float dzdt_peak_ohm_s, float lvet_ms) {
    if (z0_ohm <= 0.0f || lvet_ms <= 0.0f) return NAN;
    float lvet_s = lvet_ms / 1000.0f;
    float sv = rho_ohm_cm * (L_cm * L_cm) / (z0_ohm * z0_ohm)
               * dzdt_peak_ohm_s * lvet_s;
    // Range check: valid SV is 50–120 mL
    if (sv < 20.0f || sv > 200.0f) return NAN;  // flag extreme outliers
    return sv;
}
```

Call with `rho_ohm_cm = 135.0f`, `L_cm` from session metadata, `z0_ohm` from current I-block, `dzdt_peak_ohm_s` from current I-block, `lvet_ms` from current I-block.

### 6.3 Subject Data Entry — Electrode Distance L

L is the **distance in centimetres between the inner (voltage-sensing) ICG electrodes E1 and E2**, measured along the anterior body surface:

- E1: Between the collarbones, at the suprasternal notch — the VU-AMS device body itself
- E2: Solar plexus, midline anterior, approximately at the xiphoid process level

**Measurement protocol:**
1. Ask the subject to stand upright, relaxed.
2. Using a soft tape measure, measure from the lower edge of the device housing (which sits at the suprasternal notch between the collarbones) along the anterior midline to the lower edge of the solar plexus electrode (E2) placement site.
3. Record in centimetres to one decimal place.
4. Enter into the recording metadata before recording start. In VU-DAMS, this is stored as `electrode_distance_cm` (float32 LE) in the `subjectMetadata` section of the .vua file header (see SP-001 Open Items — closed item for L entry).

**Typical value:** L = 25–35 cm for adult subjects. Values outside this range should be flagged as potentially erroneous (subject height extremes or incorrect measurement technique). The VU-DAMS setup screen should display a warning if L < 20 cm or L > 45 cm.

**Note on electrode topology confirmation:** The electrode placement diagram (operations/electronics/electrode_placement.svg) and the brief (brief_001_analog_frontend.md) both confirm that E1 (between collarbones, ICG Vsense+) and E2 (solar plexus, ICG Vsense−) are the inner voltage-sensing electrodes. The outer current injection electrodes are E4 (nape of neck, I+) and E5 (lower back, I−). L is therefore the distance from suprasternal notch to solar plexus — the classical ICG electrode separation in the Kubicek configuration. This is consistent with the published Kubicek protocol (Kubicek et al. 1966; Sherwood et al. 1990).

**Sensitivity of SV to L:** SV scales as L². A 1 cm error in L of ±5% (e.g., entering 30 instead of 31.5 cm) produces a ~10% error in SV. Accurate measurement is critical. Provide a measurement guide with a reference image in the VU-DAMS subject setup dialog.

### 6.4 VU-AMS Electrode Geometry

From the electrode placement diagram (operations/electronics/electrode_placement.svg):

| Electrode | Location | ICG Role |
|-----------|----------|----------|
| E4 | Nape of neck (back) | Current injection I+ |
| E1 | Between collarbones, suprasternal (front) | Voltage sense Vsense+ (inner, upper) |
| E2 | Solar plexus, midline anterior (front) | Voltage sense Vsense− (inner, lower) |
| E5 | Lower back (back) | Current injection I− |

L = distance from E1 to E2 along anterior surface = suprasternal notch to solar plexus. Typical range: 25–35 cm.

This tetrapolar configuration follows the standard bioimpedance measurement principle: current is injected through the outer electrodes (E4, E5); voltage is sensed by the inner electrodes (E1, E2). The impedance seen at the sensing electrodes is the thoracic impedance from neck to xiphoid, which is the classical measurement volume for Kubicek-method ICG.

### 6.5 Expected Output Ranges and Validity Flags

**Stroke volume:**
- Normal range: 50–120 mL (Sherwood et al. 1990; normal adults at rest)
- Broad physiological range: 30–200 mL (exercise, pathology, very small subjects)
- Flag as out-of-range: SV < 50 mL or SV > 120 mL → set `sv_range_flag = true`
- Flag as invalid: SV < 20 mL or SV > 200 mL → set `sv_ml = NaN`

**Cardiac output:**
- Normal range: 3–10 L/min (rest to moderate exercise, adults)
- Flag as out-of-range: CO < 3 L/min or CO > 10 L/min → set `co_range_flag = true`
- Flag as invalid: CO < 1.5 L/min or CO > 15 L/min → set `co_lpm = NaN`

These flags are separate from the ICG quality flags in SP-001 Section 6.2. An I-block may have ICG quality = good but SV out of range (if, for example, LVET was correctly detected but Z₀ is anomalous). Both flag types must be checked.

**Input validity pre-checks (Müller):** Before computing Kubicek SV, verify:
1. `z0_ohm` is within ICG quality range [15, 50] Ω (SP-001 Section 6.2)
2. `dzdt_peak_ohm_s` is within [0.5, 5.0] Ω/s (SP-001 Section 6.2)
3. `lvet_ms` is within [150, 500] ms (physiologically plausible LVET at any heart rate)
4. L_cm has been entered and is in [20, 45] cm
5. Z₀ > 0 and LVET > 0 (division-by-zero guard)

If any pre-check fails, set `sv_ml = NaN` and `co_lpm = NaN` for that beat. Do not compute partial results.

### 6.6 Cardiac Output

$$\text{CO}_{L/\text{min}} = \frac{SV_{mL} \times \text{HR}_{bpm}}{1000}$$

HR is derived from the ECG R-peak pipeline (SP-001 Section 2.2): instantaneous HR from the current RRI:

$$\text{HR}_{bpm} = \frac{60000}{\text{rri\_ms}}$$

Use the RRI from the beat immediately preceding (or corresponding to) the I-block timestamp for the per-beat CO estimate. This is the same HR used by the firmware's existing `co_lpm` field in the I-block. The offline VU-DAMS value should match within ~10% (SP-001 Section 3.8 cross-check criterion).

**Cross-check:** Compare the Kubicek-derived `co_lpm` from VU-DAMS against the firmware `co_lpm` in the I-block. If the offline and firmware values differ by more than 20% (not the 10% specified in SP-001 Section 3.8 — the 10% criterion is aspirational; 20% is the practical fault threshold given firmware real-time constraints), log a beat-level warning. Systematic deviations across an entire recording indicate either: (a) L_cm was entered incorrectly, (b) a firmware computation error, or (c) a Z₀ calibration offset.

### 6.7 Session-Level CO and SV Reporting

For clinical and research reporting, compute epoch-level mean values (Reyes, VU-DAMS):

**Per-beat output fields (additions to SP-001 Table 7.5):**

| Field | Unit | Description |
|-------|------|-------------|
| `sv_ml` | mL | Stroke volume per beat (Kubicek offline) |
| `co_lpm` | L/min | Cardiac output per beat |
| `sv_range_flag` | bool | True if SV outside 50–120 mL |
| `co_range_flag` | bool | True if CO outside 3–10 L/min |

**Per-epoch output fields (additions to SP-001 Table 7.5):**

| Field | Unit | Description |
|-------|------|-------------|
| `sv_mean_ml` | mL | Mean SV per 30-second epoch (valid beats only) |
| `sv_sd_ml` | mL | SD of SV per epoch |
| `co_mean_lpm` | L/min | Mean CO per 30-second epoch |
| `sv_n_valid` | count | Number of valid beats contributing to epoch SV |

### 6.8 Key References

- Kubicek WG et al. (1966). Development and evaluation of an impedance cardiac output system. *Aerospace Med*, 37(12):1208–1212. — Original Kubicek formula and electrode placement.
- Patterson RP (1989). Fundamentals of impedance cardiography. *IEEE Eng Med Biol Mag*, 8(1):35–38. — Blood resistivity values and practical Kubicek implementation notes.
- Sherwood A et al. (1990). Methodological guidelines for impedance cardiography. *Psychophysiology*, 27(1):1–23. — Definitive methodological consensus; electrode placement, L measurement procedure, expected SV ranges.
- Sramek BB et al. (1983). Stroke volume equation with a linear base impedance model and its accuracy, as compared to thermodilution and magnetic flowmeter techniques in humans and animals. *Proc ACEMB*, 25:367. — ρ = 135 Ω·cm validation.

### 6.9 Open Items for Kubicek Implementation

| Item | Status | Owner |
|------|--------|-------|
| Confirm ρ value: update SP-001 Section 3.7 from 150 to 135 Ω·cm | Open — requires coordinated update | Reyes / Vasquez |
| Add L entry field to VU-DAMS subject setup UI with range warning (< 20 or > 45 cm) | Open | Reyes |
| Add subject measurement guide diagram for L measurement to setup UI | Open | Reyes |
| Firmware: populate `co_lpm` and `sv_ml` I-block fields using Kubicek formula | Open — currently NaN in I-block | Müller |
| Firmware: implement input validity pre-checks (Section 6.5) before Kubicek computation | Open | Müller |
| Validate firmware SV output against VU-DAMS offline SV on lab dataset (|diff| < 20%) | Open | Reyes / Vasquez |
| Validate absolute SV values against Doppler ultrasound reference in lab study (n ≥ 10) | Open — future validation milestone | Vasquez |

---

## 9. Open Items and Assumptions

| Item | Status | Owner |
|---|---|---|
| Confirm ICG on ECG channel 1 vs channel 2 in A-block | **Closed** — ICG raw waveform is not in A-block at all; I-block carries ICG-derived params. See Section 3 note. | Müller / Vasquez |
| Confirm electrode distance L parameter entry in recording metadata | **Closed** — L stored as `electrode_distance_cm` (float32 LE) in new `subjectMetadata` section of .vua file header. See architecture.md §File Format and VUAFileReader. | Reyes / Vasquez |
| Confirm ADS1256 Vref value for voltage conversion | Open | Nair |
| Confirm ICM-20948 accelerometer scale (±2g default or otherwise) | Open | Müller |
| Confirm AD5933 EDA sample rate (100 Hz assumed) | Open | Müller / Nair |
| Zero-phase vs causal filtering decision for online (Chen) pipeline | Out of scope this doc | Chen / Vasquez |
| Q-wave detection for true PEP (currently R-to-B) | Future work | Vasquez |
| Heart-rate-adaptive T-end detection for X-point search window | Future work | Vasquez |

---

*Dr. Elena Vasquez — Slow Horses Signal Processing*  
*"The signals don't lie. The interpretation is where we earn our pay."*
