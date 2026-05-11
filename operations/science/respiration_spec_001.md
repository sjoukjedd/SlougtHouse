# VU-AMS Respiratory Rate Specification
**Document ID:** RESP-001  
**Author:** Dr. Elena Vasquez — Biomedical Signal Processing Engineer  
**Date:** 2026-05-11  
**Status:** Draft v1.0  
**Audience:** Müller (firmware), Chen (iOS), Reyes (Java offline / VU-DAMS)  
**Depends on:** SP-001 (signal_processing_spec_001.md), CSI-001 (stress_index_spec_001.md)  
**Supersedes:** CSI-001 Section 4.5 (BRS-extended CSI-5) for the online-available 5-marker CSI — see Section 3 below.

---

## Purpose

This document specifies how the VU-AMS derives respiratory rate, how that estimate is integrated into the Composite Stress Index (CSI), and what the firmware and iOS app must implement. Respiratory rate is the single most important confound for RMSSD-based stress indices, and it carries independent value as a stress marker. It is the priority deliverable for the current development cycle.

---

## 1. Signal Sources and Derivation Paths

Three independent respiratory rate estimates are available from VU-AMS hardware. Each is specified fully below. They are not redundant — they have complementary failure modes, and the fusion strategy exploits their independence.

---

### 1.1 Method A — PPG Respiratory-Induced Intensity Variation (RIIV)

**Signal source:** P-block, `ppg_ir` field (MAX30101 IR channel, 940 nm), Fs = 100 Hz.

**Physiological basis:** Breathing modulates the DC and AC components of the PPG signal through three mechanisms: (1) respiratory-induced changes in venous return and cardiac pre-load alter pulse amplitude beat-to-beat; (2) chest wall movement modulates the optical pathlength between LED and detector; (3) respiratory variation in sympathetic vasomotor tone changes peripheral vascular resistance. The combined effect produces a slow amplitude modulation of the PPG waveform at the respiratory frequency (0.15–0.4 Hz at normal breathing rates of 9–24 breaths/min). This is termed Respiratory-Induced Intensity Variation (RIIV).

**Algorithm:**

Step 1 — Extract PPG envelope:  
Apply a 4th-order Butterworth bandpass filter to the raw IR PPG signal, passband 0.1–0.5 Hz. This isolates the respiratory modulation envelope from the cardiac pulsatile component (which is at 0.8–2.0 Hz) and from DC drift.

Filter design at Fs = 100 Hz:
- Type: 4th-order Butterworth IIR bandpass (two cascaded 2nd-order sections / biquads)
- Lower cutoff: f₁ = 0.1 Hz; upper cutoff: f₂ = 0.5 Hz
- Pre-warped frequencies (bilinear transform at Fs = 100 Hz):
  - ω₁ = 2 × 100 × tan(π × 0.1 / 100) = 0.6284 rad/s
  - ω₂ = 2 × 100 × tan(π × 0.5 / 100) = 3.142 rad/s
- Biquad coefficients (second-order section form, normalised for Fs = 100 Hz):

| Section | b0 | b1 | b2 | a1 | a2 |
|---------|----|----|----|----|-----|
| SOS 1 | 4.737 × 10⁻⁴ | 0 | −4.737 × 10⁻⁴ | −1.9964 | 0.99905 |
| SOS 2 | 4.737 × 10⁻⁴ | 0 | −4.737 × 10⁻⁴ | −1.9903 | 0.99905 |

These coefficients should be verified numerically against a reference implementation (scipy.signal.butter or equivalent) before firmware deployment. They are provided as a starting-point estimate — exact values depend on the bilinear transform implementation.

Step 2 — Peak detection on RIIV waveform:  
Find all positive-going local maxima in the filtered signal, separated by at least 1.5 seconds (maximum 40 breaths/min → minimum inter-breath interval 1.5 s). Use a minimum peak prominence of 5% of the local signal range to suppress noise-floor detections.

Step 3 — Instantaneous respiratory rate:  
For each pair of successive peaks at times tₖ and tₖ₊₁:

$$\text{RR}_{inst}[k] = \frac{60}{t_{k+1} - t_k} \quad \text{(breaths/min)}$$

Step 4 — Epoch mean:  
Compute mean over a 30–60 second sliding window. Report in breaths/min. Clamp output to [4, 40] breaths/min; label values outside this range as invalid.

**Expected SNR:** Highest at rest (signal-to-noise ratio 5–15 dB in the 0.15–0.4 Hz band under resting conditions). Degrades substantially with motion. At the suprasternal PPG placement of the VU-AMS, respiratory chest motion directly modulates the optical pathlength (see vasquez_placement_validation_001.md Section 1.3), which actually amplifies the RIIV signal compared to peripheral PPG — a rare advantage of the chest placement.

**Failure modes:**
- Motion artefact: walking or upper body movement corrupts the RIIV waveform. Apply motion gate: if IMU a_RMS > 0.15 g, mark PPG-RIIV estimate as invalid.
- Very shallow breathing (tidal volume < 200 mL): RIIV amplitude may fall below noise floor. Detect as low-confidence if RIIV peak prominence < 3% of local range.
- PPG contact loss: `ppg_valid == 0x00` in P-block. Mark as invalid immediately.
- Cardiac artefact bleeding through: if heart rate falls in the 0.1–0.5 Hz range (HR < 30 bpm or > 300 bpm) the bandpass filter will not fully separate cardiac and respiratory components. This is not a concern at physiological heart rates (50–180 bpm = 0.83–3.0 Hz, well above the respiratory band).

**Quality indicator:** `ppg_riiv_confidence` — float 0.0–1.0. Computed as ratio of energy in 0.15–0.4 Hz band to total energy in 0.1–0.8 Hz band of the filtered RIIV signal. Values > 0.5 indicate acceptable confidence.

**Recommended role:** Primary — most direct and cleanest signal at rest. The chest placement of the VU-AMS is actually advantageous for PPG-RIIV because respiratory chest motion is large relative to cardiac pulsations at this site.

---

### 1.2 Method B — HRV Respiratory Sinus Arrhythmia (HRV-RSA)

**Signal source:** ECG A-block → R-peaks → RRI series (SP-001 Sections 2.2–2.4). No additional hardware required.

**Physiological basis:** Respiratory sinus arrhythmia (RSA) is the cyclic lengthening of RR intervals during inspiration and shortening during expiration, mediated by vagal efferents to the sinoatrial node. The RSA rhythm frequency exactly tracks the respiratory frequency. The HF power band of the HRV spectrum (0.15–0.4 Hz) therefore encodes respiratory rate as the dominant spectral peak within that band.

**Algorithm:**

Step 1 — Prepare RRI series:  
Use the ectopic-rejected, validated RRI series from SP-001 Section 2.3. Require at least 60 valid beats in the analysis window (minimum 30 seconds at 60 bpm; 60 seconds preferred for spectral resolution).

Step 2 — Lomb-Scargle periodogram:  
The RRI series is unevenly spaced in time. Use the Lomb-Scargle normalised periodogram rather than resampled FFT for spectral analysis of respiratory rate, as it avoids interpolation artefacts. Evaluate at frequencies:

$$f_j = f_{min} + j \cdot \Delta f, \quad j = 0, 1, \ldots, N_f$$

with f_min = 0.10 Hz, f_max = 0.50 Hz, Δf = 0.005 Hz (0.3 breaths/min resolution).

The Lomb-Scargle power at frequency fⱼ:

$$P(f_j) = \frac{1}{2\sigma^2}\left[\frac{\left(\sum_k x_k \cos(2\pi f_j(t_k - \tau))\right)^2}{\sum_k \cos^2(2\pi f_j(t_k - \tau))} + \frac{\left(\sum_k x_k \sin(2\pi f_j(t_k - \tau))\right)^2}{\sum_k \sin^2(2\pi f_j(t_k - \tau))}\right]$$

where xₖ = RRI[k] − mean(RRI), tₖ = time of beat k, σ² = variance of xₖ, and τ is the time offset that orthogonalises the trigonometric terms.

Step 3 — Peak frequency detection:  
Find the frequency f* of the dominant spectral peak within the HF band [0.15, 0.40] Hz. Apply a minimum peak prominence criterion: peak must exceed 1.5× the mean spectral power in the [0.10, 0.50] Hz band to be considered a valid respiratory peak.

Step 4 — Respiratory rate:

$$\text{RR}_{resp} = f^* \times 60 \quad \text{(breaths/min)}$$

Clamp to [4, 40] breaths/min. If no clear peak is detected, mark estimate as invalid.

**Expected SNR:** Moderate at rest (RSA is clearly visible when vagal tone is high). Degrades under stress (vagal withdrawal reduces RSA amplitude, making the HF peak less prominent). Also degrades when respiratory rate is outside 12–20 breaths/min (the RSA peak migrates to the edge or outside the HF band).

**Failure modes:**
- Ectopic beats: even after rejection, a high ectopic burden (> 5% of beats in the window) creates spectral noise. Flag if ectopic rate > 5%.
- Very low vagal tone: RSA amplitude below 3 ms peak-to-peak produces an undetectable HF peak. This can occur during stress (the very condition we want to measure). This is an intrinsic limitation of the method — do not use HRV-RSA as the sole estimate during high-stress episodes.
- Multiple spectral peaks: during irregular breathing or sighing, two peaks may appear in the HF band. Report the higher-power peak and flag the estimate as low-confidence.
- Short epochs: fewer than 40 valid beats yields unreliable spectral estimates. Do not compute HRV-RSA if valid beat count < 40.

**Quality indicator:** `hrv_rsa_confidence` — spectral peak prominence ratio (peak power / mean HF band power). Values > 2.0 indicate reliable estimate.

**Key references:**
- Clifton DA et al. (2007). Measurement of respiratory rate from the photoplethysmogram in chest pain patients. *J Med Eng Technol*, 31(3):152–166.
- Charlton PH et al. (2016). An assessment of algorithms to estimate respiratory rate from the electrocardiogram and photoplethysmogram. *Physiol Meas*, 37(4):610–626. — Comprehensive comparison of ECG/PPG-based respiratory rate methods; HRV-RSA performs well at rest.
- Lomb NR (1976). Least-squares frequency analysis of unequally spaced data. *Astrophys Space Sci*, 39:447–462.
- Scargle JD (1982). Studies in astronomical time series analysis. II. Statistical aspects of spectral analysis of unevenly spaced data. *Astrophys J*, 263:835–853.

**Recommended role:** Secondary — uses the existing R-peak pipeline at no additional sensor cost. Provides an independent cross-check against PPG-RIIV. Less reliable during stress and in high-activity epochs.

---

### 1.3 Method C — Accelerometer Z-Axis (IMU Chest Movement)

**Signal source:** M-block, `az` field (Z-axis accelerometer, chest vertical displacement), Fs = 100 Hz.

**Physiological basis:** Breathing causes cyclical expansion and contraction of the chest wall. The anterior-posterior (Z-axis) accelerometer on the VU-AMS device, positioned at the suprasternal notch, captures this motion as a low-frequency oscillation at the respiratory rate. Chest wall displacement during quiet breathing is 5–15 mm at the device site, producing a measurable acceleration signal.

**Algorithm:**

Step 1 — Bandpass filter:  
Apply a 4th-order Butterworth bandpass filter to the IMU Z-axis acceleration, passband 0.1–0.5 Hz (same filter coefficients as PPG-RIIV, Section 1.1, applied to 100 Hz data).

Step 2 — Remove gravity component:  
Before filtering, subtract the running mean of `az` over a 10-second window to remove the static gravity offset. This is equivalent to a 0.1 Hz high-pass pre-filter and is already handled by the 0.1 Hz lower cutoff in Step 1 for sufficiently long epochs.

Step 3 — Zero-crossing rate or peak-to-peak interval:  
Method 3A (zero-crossing): count positive-going zero crossings in the filtered signal per unit time:

$$\text{RR}_{resp} = \frac{\text{positive zero-crossing count}}{T_{epoch}/60} \quad \text{(breaths/min)}$$

Method 3B (peak-to-peak): find local maxima in the filtered signal, compute mean inter-peak interval, convert to breaths/min. Prefer Method 3B as it is less sensitive to noise-induced spurious crossings.

Step 4 — Epoch mean and clamping:  
30-second minimum epoch. Clamp to [4, 40] breaths/min.

**Expected SNR:** Highly variable. Best signal quality: supine or seated, still. Poor signal quality: any movement (walking, arm movement, upper body rotation) directly produces large Z-axis accelerations that overwhelm the respiratory signal.

**Failure modes:**
- Physical activity: walking or any gross movement. This is the dominant failure mode. Method C is only reliable when a_RMS < 0.10 g (tighter threshold than the 0.15 g used for cardiac metrics, because the respiratory signal is weaker).
- Device orientation change: tilting the device changes which accelerometer axis captures the respiratory motion. Under supine conditions the Z-axis respiratory signal may transfer partially to X or Y. Compensate by using the axis with the maximum bandpass energy if orientation changes are detected.
- Talking: phonation produces 2–5 Hz vibrations that may alias into the respiratory band after filtering at some phase relationships. Flag recording segments containing suspected talking (detected as periodic IMU micro-vibrations at 2–5 Hz).
- Cardiac mechanical vibration: ballistocardiographic artefact from the heartbeat is at 1–3 Hz, well above the bandpass, and is fully rejected.

**Quality indicator:** `imu_resp_confidence` — ratio of energy in 0.15–0.4 Hz band to total energy in 0.05–2.0 Hz band of the filtered Z-axis signal, computed after motion gating. Values > 0.4 indicate usable signal.

**Recommended role:** Tertiary — useful backup when both PPG and ECG are degraded, but only during low-activity states. Not suitable as a primary estimate in ambulatory recordings.

---

### 1.4 Sensor Comparison Summary

| Property | PPG-RIIV (A) | HRV-RSA (B) | IMU (C) |
|----------|-------------|------------|---------|
| Fs | 100 Hz | Per-beat (~1–2 Hz) | 100 Hz |
| Primary band | 0.1–0.5 Hz | 0.15–0.40 Hz | 0.1–0.5 Hz |
| Epoch requirement | 30 s | 60 s preferred | 30 s |
| Rest performance | Good | Good | Moderate |
| Motion sensitivity | High | Low (beat-based) | Very high |
| Stress sensitivity | Low (method unaffected) | High (RSA falls) | Low (method unaffected) |
| Hardware cost | MAX30101 (existing) | ECG (existing) | IMU (existing) |
| Recommended role | Primary | Secondary | Tertiary |

---

## 2. Algorithm Specification

### 2.1 Filter Design Summary

All three methods use the same 4th-order Butterworth bandpass (0.1–0.5 Hz) applied to 100 Hz signals. The IIR biquad form is preferred for firmware implementation. For offline VU-DAMS (Java), zero-phase filtering (forward + reverse pass) is recommended for post-hoc analysis; causal filtering for real-time.

At Fs = 100 Hz, the normalised cutoff frequencies are:

$$\omega_1 = \frac{0.1}{50} = 0.002\pi, \quad \omega_2 = \frac{0.5}{50} = 0.01\pi$$

This is a very narrow-band filter at 100 Hz. Double-precision arithmetic is mandatory — single-precision accumulates rounding error sufficient to destabilise the filter at these low normalised frequencies. Müller must confirm that the firmware IIR implementation uses `double` (float64) for filter state variables, even if the input is float32.

**Java implementation note (Reyes):** Use the `iirj` library `Butterworth.bandPassCoefficients()` method, specifying Fs = 100.0, fc = 0.25 (geometric mean of 0.1 and 0.5 Hz), and bandwidth = 0.4 Hz. Validate that the resulting transfer function has −3 dB points within ±0.02 Hz of the specified cutoffs.

### 2.2 Peak Detection Parameters

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Minimum inter-peak interval | 1.5 s | Maximum 40 breaths/min |
| Maximum inter-peak interval | 15 s | Minimum 4 breaths/min |
| Minimum peak prominence | 5% of local range | Noise rejection |
| Local range window | 30 s | Captures slow amplitude modulation |

### 2.3 Output Range and Epoch Requirements

| Parameter | Value |
|-----------|-------|
| Valid output range | [4, 40] breaths/min |
| Out-of-range action | Clamp and flag as `rr_invalid` |
| Minimum epoch length | 30 seconds |
| Preferred epoch length | 60 seconds |
| Update rate | Every 5 seconds (sliding window) |
| Window overlap | 25–55 seconds depending on window length |

### 2.4 Fusion Strategy

At each 30-second epoch, up to three independent respiratory rate estimates are available. The fused estimate is a quality-weighted average:

$$\text{RR}_{fused} = \frac{w_A \cdot \text{RR}_A + w_B \cdot \text{RR}_B + w_C \cdot \text{RR}_C}{w_A + w_B + w_C}$$

where weights are determined by the quality indicators:

$$w_A = \text{ppg\_riiv\_confidence} \cdot \mathbf{1}[\text{PPG valid}]$$
$$w_B = \text{hrv\_rsa\_confidence} \cdot \mathbf{1}[\text{RRI valid}]$$
$$w_C = \text{imu\_resp\_confidence} \cdot \mathbf{1}[a_{RMS} < 0.10\,g]$$

If all three confidence scores are zero (all methods unavailable), report `rr_bpm = NaN` and `rr_valid = false`.

If only one method is available and its confidence score > 0.3, use it directly without fusion averaging. If its confidence score ≤ 0.3, report as low-confidence.

**Method flag:** Report `rr_method` as the identity of the highest-weight contributor:
- 0 = PPG-RIIV (primary)
- 1 = HRV-RSA (secondary)
- 2 = IMU (tertiary)
- 3 = Fusion (multiple methods above threshold)

**Conflict resolution:** If any two of the three available estimates differ by more than 4 breaths/min, set `rr_conflict = true` and downgrade the fused estimate's confidence. Do not suppress the estimate entirely — report it with the conflict flag so the researcher can inspect which method is diverging. A difference of > 4 breaths/min between methods typically indicates motion artefact on one channel (most commonly IMU), breathing irregularity (sighs, pauses), or detection error in one pipeline.

### 2.5 Processing Order in VU-DAMS

Insert respiratory rate computation immediately after SP-001 Section 5 (SCL/EDA processing) and before CSI computation:

```
... SP-001 Steps 1–8 (existing pipeline)
→ Step 9:  PPG-RIIV respiratory rate (Method A) — from P-blocks
→ Step 10: HRV-RSA respiratory rate (Method B) — from RRI series (Step 3 output)
→ Step 11: IMU respiratory rate (Method C) — from M-blocks
→ Step 12: Respiratory rate fusion and quality flagging
→ Step 13: Activity gating (HAR, SP-002)
→ Step 14: CSI computation (CSI-001)
→ Step 15: Final stress report
```

---

## 3. Integration into the CSI

### 3.1 Role 1 — Confounder Monitor for RMSSD

Respiratory rate is the dominant confound of RMSSD. The mechanism is well established: breathing at 6 breaths/min produces large RSA oscillations and inflated RMSSD regardless of vagal tone; breathing at 25 breaths/min moves the RSA peak to the upper edge of the HF band and suppresses RMSSD even without any reduction in vagal tone.

**Normal respiratory rate range:** 12–20 breaths/min (Clifton et al., 2007; Wilhelm et al., 2006).

**Confounder flag definition:**

```
rr_confounder_flag = (rr_bpm < 12) OR (rr_bpm > 20)
```

If `rr_confounder_flag == true` in a given epoch, the RMSSD z-score for that epoch is potentially confounded. Apply the following weight adjustment:

- Default w₃ (RMSSD⁻¹) = 0.25 (CSI-001 Section 4.3)
- When `rr_confounder_flag == true`:
  - Reduce w₃ to 0.10
  - Redistribute the removed weight (0.15) proportionally to w₁ (PEP⁻¹) and w₂ (SCL):
    - Δw₁ = 0.15 × (0.35 / (0.35 + 0.30)) = 0.08
    - Δw₂ = 0.15 × (0.30 / (0.35 + 0.30)) = 0.07
  - Adjusted weights when RMSSD is confounded: w₁ = 0.43, w₂ = 0.37, w₃ = 0.10, w₄ = 0.10 (sum = 1.00 ✓)

Note: these adjusted weights are provisional and must be validated against the VU-AMS lab data (CSI-001 Section 6). The principle — downweighting the confounded marker and redistributing to unconfounded markers — is correct regardless of the exact redistribution rule.

**Broader flag boundaries:** The confounder flag range [12, 20] is tighter than the "caution" range in SP-001 Section 4.2 (8–22 breaths/min). The tighter range flags moderate confounding for weight adjustment; the broader 8–22 range triggers the respiratory rate warning in the iOS UI (CSI-001 Section 5.3, item 5). Both flags must be computed and reported.

### 3.2 Role 2 — Independent Stress Marker (CSI-5 with RR)

Respiratory rate increases under psychological stress (Wilhelm et al., 2006; Grossman et al., 1990). The increase is behaviourally mediated (faster, shallower breathing during cognitive load) as well as reflexively driven. In the 12–20 breaths/min normal range, elevated RR within that range also correlates with arousal.

Respiratory rate is included as the 5th optional component of an extended online CSI (CSI-5-RR):

$$\text{CSI}_{5\text{-RR}}(t) = w_1 \cdot z(\text{PEP}^{-1}) + w_2 \cdot z(\text{SCL}) + w_3' \cdot z(\text{RMSSD}^{-1}) + w_4 \cdot z(\text{SCR\_rate}) + w_5 \cdot z(\text{RR})$$

Where:
- z(RR) is the z-score of the fused respiratory rate relative to the within-session baseline: z(RR) = (RR − μ_RR^base) / σ_RR^base
- RR direction under stress: **increases** → include directly (positive loading)
- w₅ = 0.10, redistributed from other weights proportionally:

**Updated weight vector for CSI-5-RR:**

$$\mathbf{w}_{5\text{-RR}} = [w_1, w_2, w_3', w_4, w_5] = [0.32, 0.27, 0.22, 0.09, 0.10]$$

Derivation: start from CSI-4 weights [0.35, 0.30, 0.25, 0.10]; subtract w₅ = 0.10 proportionally from each existing weight (factor = 0.90):
- w₁ = 0.35 × 0.90 = 0.315 ≈ 0.32
- w₂ = 0.30 × 0.90 = 0.270 ≈ 0.27
- w₃ = 0.25 × 0.90 = 0.225 ≈ 0.22
- w₄ = 0.10 × 0.90 = 0.090 ≈ 0.09
- w₅ = 0.10
- Sum = 0.32 + 0.27 + 0.22 + 0.09 + 0.10 = 1.00 ✓

**Availability condition:** CSI-5-RR is computed only when `rr_valid == true` AND `rr_conflict == false` AND `rr_bpm` is in [4, 40] breaths/min. When RR is unavailable, fall back to CSI-4 with standard weights.

**Note on RMSSD confounding in CSI-5-RR:** If `rr_confounder_flag == true`, apply the weight adjustment from Section 3.1 (reduce w₃ to 0.10) while keeping w₅ = 0.10. In this case, redistribution of the RMSSD reduction goes to w₁, w₂, and w₄ only (not to w₅, as RR is a different marker, not a substitute for RMSSD validity). Adjusted confounded weights: w₁ = 0.39, w₂ = 0.33, w₃ = 0.10, w₄ = 0.08, w₅ = 0.10, sum = 1.00 ✓.

**Relationship to CSI-001 Section 4.5 (BRS-extended CSI-5):** The CSI-001 Section 4.5 BRS-extended CSI-5 uses baroreflex sensitivity as the 5th marker (offline only, 5-minute window). The present RR-extended CSI-5-RR uses respiratory rate as the 5th marker (online available, 30-second window). These serve different purposes. Going forward:
- **CSI-5-RR** (this document) = online 5-marker index, real-time iOS capable. Replaces "CSI-4" as the preferred live index when RR is available.
- **CSI-5-BRS** (CSI-001 Section 4.5, unchanged) = offline extended index for VU-DAMS only.
- Label these distinctly in all outputs and reports.

### 3.3 Output Fields (Additions to SP-001 and CSI-001 Tables)

Add the following fields to the per-epoch output (SP-001 Table 7.5, CSI-001 Table 5.2):

| Field | Unit | Description |
|-------|------|-------------|
| `rr_bpm` | breaths/min | Fused respiratory rate (NaN if invalid) |
| `rr_valid` | bool | True if fused estimate meets quality threshold |
| `rr_method` | uint8 | 0=PPG, 1=HRV-RSA, 2=IMU, 3=Fusion |
| `rr_quality` | float 0–1 | Fused quality score (weighted mean of component confidences) |
| `rr_conflict` | bool | True if two methods differ by > 4 breaths/min |
| `rr_confounder_flag` | bool | True if rr_bpm outside 12–20 breaths/min |
| `rr_caution_flag` | bool | True if rr_bpm outside 8–22 breaths/min (broad warning for iOS) |
| `csi_5rr` | 0–100 | CSI-5-RR composite (NaN if rr_valid == false) |
| `w3_adjusted` | bool | True if RMSSD weight was reduced due to rr_confounder_flag |
| `ppg_riiv_confidence` | float 0–1 | PPG-RIIV quality indicator |
| `hrv_rsa_confidence` | float 0–1 | HRV-RSA quality indicator |
| `imu_resp_confidence` | float 0–1 | IMU respiratory quality indicator |

---

## 4. Display Specification for iOS (Chen)

### 4.1 Breathing Rate Tile

Add a "RR" tile to `SignalDashboardView`, formatted consistently with existing metric tiles:

- **Label:** "RR" (breathing rate)
- **Value:** `rr_bpm` displayed as a single decimal digit (e.g., "14.3")
- **Unit label:** "br/min"
- **Normal range indicator:**
  - rr_bpm 12–20: green background / green text
  - rr_bpm < 12 or > 20: amber background (outside normal range, possible confound)
  - rr_bpm < 4 or > 40, or `rr_valid == false`: show "--" (no valid estimate)
- **Quality indicator:** small icon (circle) in tile corner:
  - `rr_quality > 0.6`: solid green circle
  - `rr_quality 0.3–0.6`: half-filled amber circle
  - `rr_quality < 0.3` or `rr_conflict == true`: amber exclamation mark
- **Update rate:** Every 5 seconds (aligned with the sliding window output).
- **Tap behaviour:** Tapping the tile should reveal a detail popover explaining the three derivation methods and indicating which method(s) are contributing to the current estimate.

### 4.2 Respiratory Waveform (Slow Waveform Display)

Add a slow respiratory waveform trace to the Live tab waveform display, below the PPG trace:

- **Source:** PPG-RIIV filtered signal (0.1–0.5 Hz bandpass of IR channel) when `ppg_riiv_confidence > 0.4`; fall back to IMU Z-axis bandpass when PPG-RIIV is unavailable; hide waveform entirely when neither is available.
- **Label:** "RESP" with method indicator ("PPG" or "IMU")
- **Y-axis:** autoscaled to ±3σ of the 30-second waveform range; no physiological calibration (signal is in arbitrary units, not volume)
- **Timescale:** 30-second scrolling window (same as other waveforms in the Live tab)
- **Colour:** teal/cyan, visually distinct from ECG (green) and PPG (red/orange)
- **BLE source:** The PPG-RIIV filtered waveform requires Chen to request the 50 Hz PPG IR channel from the BLE stream (already in the BLE budget — P-block IR at 50 Hz). The 0.1–0.5 Hz bandpass is applied on-device in iOS, not in firmware. Alternatively, firmware can compute and stream the RIIV waveform as a derived channel — see Section 5.4.

### 4.3 CSI-5-RR Integration

- When `rr_valid == true`, display CSI-5-RR in the CSI gauge and label it "CSI-5" in the UI.
- When `rr_valid == false`, display CSI-4 and label it "CSI-4".
- The transition between CSI-4 and CSI-5 must be smooth: do not abruptly switch values when RR becomes available. Apply a 30-second crossfade (exponential moving average weighting the two indices during the transition period).

### 4.4 Respiratory Rate Warning in Stress Context

When `rr_caution_flag == true` (rr_bpm outside 8–22 breaths/min):
- Display the amber respiratory rate warning badge (already specified in CSI-001 Section 5.3, item 5)
- Add tooltip text: "Breathing rate outside normal range. HRV-based stress markers may be less reliable."

---

## 5. Firmware Specification (Müller)

### 5.1 Recommended Firmware-Side Method

PPG-RIIV (Method A) is the best candidate for firmware-side respiratory rate computation because:
1. The algorithm (bandpass + peak detection) is simple and computationally cheap.
2. The PPG signal is already processed in TASK_BLOCK_ASSEMBLER (P-block assembly).
3. The result can be produced on a 30-second rolling basis and appended to each R-block (see Section 5.3).

Methods B and C are best deferred to the iOS app or VU-DAMS: HRV-RSA requires the Lomb-Scargle periodogram (heavier computation); IMU requires careful integration with the HAR activity classifier which is currently in VU-DAMS.

### 5.2 IIR Filter Coefficients for PPG-RIIV at Fs = 50 Hz

Note: the MAX30101 is configured at 50 Hz native sampling in the VU-AMS (100 Hz is the block assembly rate; Müller to confirm the actual MAX30101 ODR setting with Chen). If Fs = 50 Hz, the filter coefficients change:

At Fs = 50 Hz, normalised cutoffs: f₁/Nyquist = 0.1/25 = 0.004, f₂/Nyquist = 0.5/25 = 0.02.

4th-order Butterworth bandpass, Fs = 50 Hz, passband 0.1–0.5 Hz:

Pre-warped analogue frequencies:
- ω₁ = 2 × 50 × tan(π × 0.1 / 50) = 0.6285 rad/s
- ω₂ = 2 × 50 × tan(π × 0.5 / 50) = 3.149 rad/s

Biquad coefficients (second-order sections, Fs = 50 Hz):

| Section | b0 | b1 | b2 | a1 | a2 |
|---------|----|----|----|----|-----|
| SOS 1 | 1.889 × 10⁻³ | 0 | −1.889 × 10⁻³ | −1.9926 | 0.99622 |
| SOS 2 | 1.889 × 10⁻³ | 0 | −1.889 × 10⁻³ | −1.9805 | 0.99622 |

**IMPORTANT:** These coefficients are indicative. Müller must regenerate them using a validated numerical filter design tool (e.g., scipy.signal.butter with fs=50, Wn=[0.1, 0.5], btype='bandpass', output='sos') and verify the frequency response before implementation. The filter must use double-precision (float64) state variables; float32 is insufficient at these low normalised frequencies.

Filter state initialisation: initialise all state variables to zero at device startup. Do not re-initialise at block boundaries. The filter is continuous across the recording.

### 5.3 New R-Block (Respiratory Block, type byte 0x52)

Add a new block type to the block format. Type byte: `0x52` ('R'). Produced by TASK_BLOCK_ASSEMBLER on each 30-second sliding window update (every 5 seconds with 25-second overlap). One R-block per 5-second step.

```c
typedef struct __attribute__((packed)) {
    float    rr_bpm;        // Respiratory rate in breaths/min (NaN if invalid)
    uint8_t  rr_valid;      // 0x01 = valid estimate; 0x00 = invalid / unavailable
    uint8_t  rr_quality;    // Quality score 0–100 (scaled from 0.0–1.0 float; 0xFF = not computed)
    uint8_t  rr_method;     // 0=PPG-RIIV, 1=HRV-RSA, 2=IMU, 3=Fusion
    uint8_t  rr_conflict;   // 0x01 = two methods differ by > 4 br/min; 0x00 = consistent
    uint8_t  rr_confounder; // 0x01 = rr_bpm outside 12–20; 0x00 = normal range
    uint8_t  rr_caution;    // 0x01 = rr_bpm outside 8–22; 0x00 = within caution range
    uint8_t  _pad[2];
} R_block_payload_t;        // 10 bytes payload

// Total block size: 12-byte header + 10-byte payload = 22 bytes per R-block
// BLE: include in ble_block_queue at full 5-second update rate (22 bytes per update = 4.4 B/s — negligible BLE budget impact)
```

Update the block type enum and `BlockHeader_t` documentation in `firmware/main/data_blocks.h` to include 'R' = 0x52.

### 5.4 Optional: RIIV Waveform BLE Stream

If Chen requires the respiratory waveform in the iOS Live tab (Section 4.2), the firmware can optionally stream the RIIV waveform as a downsampled channel:

- Source: PPG-RIIV bandpass-filtered IR signal (0.1–0.5 Hz output at 50 Hz)
- BLE downsample: 10 Hz (factor 5 from 50 Hz)
- Encoding: 8-bit delta, same scheme as SCL phasic
- BLE budget: 10 bytes/s — negligible
- Add as an optional field in the P-block BLE stream or as a dedicated V-block (ventilation). Defer this extension to firmware v2 unless Chen flags it as required for v1.

### 5.5 Computation Cost Estimate (ESP32-S3 LX7 Core)

PPG-RIIV respiratory rate at 50 Hz input:

| Operation | Estimated cycles | Notes |
|-----------|-----------------|-------|
| IIR biquad filter (4th-order, 1 sample) | ~40 cycles | 2 biquad stages × ~20 cycles each (FP multiply-accumulate on LX7) |
| Peak detection per sample | ~15 cycles | Simple local max check with running buffer |
| Rate computation (per 5-second update) | ~200 cycles | Mean computation over 30 s window: ~30 values |
| Per-sample total (50 Hz → every 20 ms) | ~60 cycles | Well within the 20 ms task budget |
| Per-second overhead | ~3,000 cycles | At 240 MHz core clock, this is < 0.002% of CPU budget |

Lomb-Scargle (Method B, if added to firmware in a future version): estimated 50,000–100,000 cycles per 60-second epoch computation (once every 5 seconds). This is approximately 0.04–0.08% of CPU at 240 MHz — feasible but heavier. Recommend deferring Method B to the iOS app and VU-DAMS for now.

IMU bandpass (Method C): same cost as PPG-RIIV at 100 Hz → double the per-sample cost. Feasible but adds up; defer to VU-DAMS for offline computation.

**Recommendation:** Implement PPG-RIIV (Method A) in firmware only, producing the R-block at 5-second intervals. Methods B and C are implemented in VU-DAMS and the iOS app using their respective signal inputs.

---

## 6. Key References

- Charlton PH et al. (2016). An assessment of algorithms to estimate respiratory rate from the electrocardiogram and photoplethysmogram. *Physiol Meas*, 37(4):610–626. — Primary methods comparison; recommends PPG amplitude modulation and HRV-RSA as most reliable non-contact methods.
- Clifton DA et al. (2007). Measurement of respiratory rate from the photoplethysmogram in chest pain patients. *J Med Eng Technol*, 31(3):152–166. — PPG-RIIV derivation in clinical context.
- Grossman P et al. (1990). Respiratory sinus arrhythmia, cardiac vagal control, and daily activity. In Byrne EA & Porges SW (Eds.), *Cardiac Vagal Control and Social Engagement*. New York: Plenum. — RSA and its relationship to respiratory rate modulation of HRV.
- Lomb NR (1976). Least-squares frequency analysis of unequally spaced data. *Astrophys Space Sci*, 39:447–462. — Lomb-Scargle periodogram.
- Nilsson L et al. (2000). Monitoring of respiratory rate in postoperative care using a new photoplethysmographic technique. *J Clin Monit Comput*, 16(4):309–315. — Clinical validation of PPG-RIIV.
- Scargle JD (1982). Studies in astronomical time series analysis. II. Statistical aspects of spectral analysis of unevenly spaced data. *Astrophys J*, 263:835–853. — Lomb-Scargle periodogram.
- Wilhelm FH et al. (2006). Respiratory reactivity, autonomic cardiac control, and daily hassles. *Respir Physiol Neurobiol*, 153(1):45–56. — Stress effects on respiratory rate in ambulatory settings.

---

## 7. Open Items

| Item | Status | Owner |
|------|--------|-------|
| Confirm MAX30101 ODR: 50 Hz vs 100 Hz at native sample rate in firmware | Open | Müller |
| Validate biquad coefficients at confirmed Fs with scipy reference | Open — required before firmware implementation | Müller / Vasquez |
| Validate PPG-RIIV respiratory rate against reference spirometry in lab validation study | Open | Vasquez |
| Calibrate fusion weights against VU-AMS lab data | Open — current scheme is heuristic | Vasquez |
| Validate CSI-5-RR weight vector against lab stressor data | Open | Vasquez |
| Confirm whether RIIV waveform BLE stream is required for iOS v1 | Open | Chen |
| RMSSD confounder weight adjustment values: validate proportional redistribution rule | Open | Vasquez / Reyes |
| Add R-block type to data_blocks.h and to SP-001 Table 1.1 | Open | Müller / Reyes |

---

*Dr. Elena Vasquez — Slow Horses Signal Processing*  
*"Breathing is what everything else hangs on. Miss it and you're explaining variance that isn't stress."*
