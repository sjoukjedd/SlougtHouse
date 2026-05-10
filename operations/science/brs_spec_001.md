# BRAVO: Baroreflex Sensitivity Algorithm Specification
**Document ID:** BRS-001  
**Author:** Dr. Elena Vasquez — Biomedical Signal Processing Engineer  
**Date:** 2026-05-10  
**Status:** Draft v1.0  
**Audience:** Reyes (Java offline analysis / VU-DAMS), Chen (iOS live display)  
**Depends on:** SP-001 (VU-AMS Signal Processing Specification)

---

## Purpose

This document specifies the complete algorithmic pipeline for computing Baroreflex Sensitivity (BRS) within the BRAVO module of VU-DAMS. It covers scientific background, both primary (Sequence Method) and secondary (Spectral Method) algorithms, the blood pressure surrogate derivation from Pulse Transit Time (PTT), all output metrics, and integration notes for Reyes. Reyes implements exactly what is described here. Deviations require Vasquez sign-off.

---

## 1. Scientific Background: What BRS Measures

### 1.1 The Baroreflex Arc

The arterial baroreflex is the primary short-latency feedback mechanism by which the autonomic nervous system maintains beat-to-beat arterial blood pressure homeostasis. Mechanoreceptors (baroreceptors) located in the carotid sinus and aortic arch respond to stretch caused by arterial pressure changes. Their afferent signals project to the nucleus tractus solitarius in the medulla, which in turn modulates both vagal (parasympathetic) efferent outflow to the sinoatrial node and sympathetic efferent outflow to the heart and vasculature.

The functional consequence is a reflex coupling between systolic blood pressure (SBP) and the R-R interval (RRI): when SBP rises, the reflex lengthens RRI (heart rate falls) via increased vagal tone and reduced sympathetic tone; when SBP falls, RRI shortens (heart rate rises). The gain of this reflex — the magnitude of the RRI change per unit change in SBP — is Baroreflex Sensitivity.

$$\text{BRS} = \frac{\Delta \text{RRI}}{\Delta \text{SBP}} \quad \text{units: ms/mmHg}$$

### 1.2 Autonomic Balance and BRS

BRS reflects the integrated gain of both limbs of the reflex arc. High BRS indicates robust vagal dominance and effective pressure buffering — a hallmark of healthy cardiovascular function. Low BRS indicates impaired reflex gain, associated with reduced vagal tone and/or increased sympathetic drive.

BRS has directional asymmetry in healthy subjects: the vagal-mediated limb (rising SBP → RRI lengthening, "up-sequences") typically has higher gain than the sympathetically-mediated limb (falling SBP → RRI shortening, "down-sequences"). This asymmetry is itself a physiological signal.

**BRS up-sequences** are primarily mediated by cardiac vagal efferents and reflect parasympathetic responsiveness. **BRS down-sequences** involve both withdrawal of vagal tone and activation of cardiac sympathetic efferents; their gain is lower and their latency longer. Computing these separately (BRS_seq_up and BRS_seq_down) provides mechanistic resolution that aggregate BRS does not.

### 1.3 Clinical and Research Relevance

BRS is a validated marker in peer-reviewed cardiovascular physiology. Its principal associations:

- **Cardiac risk:** Reduced BRS is an independent predictor of mortality following acute myocardial infarction (La Rovere et al., *Lancet* 1988; ATRAMI study, 1998). BRS < 3 ms/mmHg identifies a high-risk group.
- **Autonomic neuropathy:** Diabetic and hypertensive autonomic neuropathy reduce BRS before HRV changes become apparent. BRS is more sensitive to early impairment than RMSSD or SDNN in some populations.
- **Acute stress:** Laboratory mental and physical stressors consistently reduce BRS, particularly BRS_up, reflecting the shift from vagal to sympathetic dominance during the stress response. This is the primary research context for BRAVO in VU-AMS users.
- **Heart failure:** Severely depressed BRS (< 2 ms/mmHg) in heart failure patients correlates with exercise intolerance and is a target for autonomic rehabilitation interventions.
- **Healthy ageing:** BRS declines monotonically with age; normative reference ranges require age stratification.

For BRAVO RUO positioning, the relevant context is research in cardiovascular physiology, autonomic stress assessment, and pharmacological or intervention studies where BRS is used as an outcome measure.

---

## 2. Input Data Requirements

BRAVO consumes two per-beat time series, both already produced by the SP-001 pipeline. No additional raw data access is needed.

| Input | Source (SP-001) | Unit | Required fields |
|---|---|---|---|
| R-peak times | Section 2.2 (Pan-Tompkins) | ms from recording start | `r_peak_time_ms`, `rri_valid` flag |
| RRI series | Section 2.3 | ms | `rri_ms`, `rri_valid` |
| PEP per beat | Section 3.5 | ms | `pep_ms` |
| I-block per beat | Firmware I-block | various | `z0`, `dZdt_peak`, `pep_ms`, `lvet_ms` |

**Minimum data requirements before BRS computation is attempted:**

- At least 300 consecutive valid beats (approximately 5 minutes at 60 bpm). This ensures adequate spontaneous blood pressure oscillations — particularly Mayer waves in the LF band (0.04–0.15 Hz) — are represented.
- Less than 30% rejected beats in the analysis window (ectopic rejection flags from SP-001 Section 2.3).
- If either condition is unmet, all BRS metrics are set to NaN and a `brs_insufficient_data` flag is set in the output.

---

## 3. Blood Pressure Estimation from ICG: Pulse Transit Time

### 3.1 The PTT-SBP Relationship

VU-AMS carries no tonometric or oscillometric blood pressure sensor. Direct SBP values in mmHg are unavailable. BRS can nevertheless be computed in relative terms using Pulse Transit Time (PTT) as a surrogate for systolic blood pressure.

PTT is defined as the time from cardiac electrical activation (ECG R-peak) to the mechanical pulse arrival at a peripheral site. In the VU-AMS context, the most proximal available mechanical event is the ICG B-point — the onset of ventricular ejection (aortic valve opening). This gives a central PTT variant:

$$\text{PTT}[k] = t_B[k] - t_R[k] \quad \text{(ms)}$$

where $t_R[k]$ is the R-peak time of beat $k$ (from SP-001 Section 2.2) and $t_B[k]$ is the B-point time of beat $k$ (from SP-001 Section 3.3). This interval is equivalent to PEP.

**Note:** In the current file format, $t_B$ is not separately stored. Use `pep_ms` from the I-block (or offline-computed PEP from SP-001 Section 3.5) as the PTT proxy. Label outputs accordingly.

### 3.2 Physiological Basis of PTT as a BP Surrogate

The relationship between PTT and arterial blood pressure derives from the Bramwell-Hill equation for pulse wave velocity (PWV) in an elastic tube:

$$\text{PWV} = \sqrt{\frac{V \cdot \Delta P}{\rho \cdot \Delta V}}$$

where $V$ is arterial volume, $\Delta P$ is pulse pressure, $\rho$ is blood density, and $\Delta V$ is the volume change. Since PWV = distance / PTT, it follows that PTT is inversely related to PWV and therefore inversely related to arterial pressure:

$$\text{SBP} \propto \frac{1}{\text{PTT}^2}$$

For small perturbations around a baseline, the relationship is approximately linear and inverse: **longer PTT corresponds to lower SBP, shorter PTT corresponds to higher SBP**.

### 3.3 PTT as a Relative BP Proxy

Since PTT is in milliseconds and SBP is in mmHg, direct use of PTT in the BRS formula would yield dimensionless BRS in ms/ms rather than ms/mmHg. This is the correct approach when no absolute BP calibration is available.

Define the PTT-derived SBP surrogate as:

$$\text{SBP}_{PTT}[k] = -\text{PTT}[k]$$

The negation ensures that a rise in the surrogate corresponds to a rise in true SBP (since PTT is inversely related to SBP). Use $\text{SBP}_{PTT}$ in all sequence identification and regression steps described in Section 4.

**When PTT-based BRS is computed, all output metrics are in ms/ms (dimensionless). This must be clearly labelled in every output field, report, and UI display. Do not express PTT-based BRS in ms/mmHg — this would imply a calibrated absolute BP measurement that does not exist.**

### 3.4 Limitations

The following limitations must be documented and surfaced to the user in the VU-DAMS report:

1. **No absolute BP calibration.** PTT reflects relative BP changes within a recording session. Inter-session and inter-individual comparisons of BRS magnitude require caution. Only within-session, within-participant comparisons of BRS are strictly valid.

2. **PTT = PEP conflation.** The R-to-B interval (PEP) reflects both electromechanical coupling and pre-ejection dynamics. True pulse transit time should be measured from a peripheral pressure or optical pulse sensor. Using PEP as a PTT proxy conflates cardiac pre-ejection delay with vascular transit time. PEP is sensitive to sympathetic drive (it shortens under stress independently of BP), which may introduce artefactual BRS changes during sympathetic arousal.

3. **Non-linear PTT-SBP relationship.** The linearisation is valid only for small (< 10 mmHg) spontaneous SBP fluctuations typical of resting conditions. During large haemodynamic perturbations the approximation degrades.

4. **Raw ICG waveform limitation.** B-point detection (and therefore PTT accuracy) depends on offline waveform analysis (SP-001 Sections 3.3–3.5), which is not available in the current file format. For the current I-block-only format, `pep_ms` from the firmware is used. Firmware PEP is less precise than offline B-point detection. BRS quality flags should reflect this.

5. **If a future firmware version provides a PPG peripheral signal** (Müller's Multi-Node BAN concept), PTT should be recomputed as ECG R-peak to PPG foot (peripheral arrival time). This would give true PTT and allow population-level calibration. This document will be revised at that point.

---

## 4. Primary Algorithm: Sequence Method

### 4.1 Overview

The Sequence Method (Parati et al., 1988; Di Rienzo et al., 1985) identifies naturally occurring sequences of consecutive cardiac beats in which SBP and RRI change monotonically in the same direction — a rise in SBP accompanied by a lengthening RRI (up-sequence), or a fall in SBP accompanied by a shortening RRI (down-sequence). These sequences represent spontaneous baroreflex engagement. BRS is estimated as the slope of the linear regression of RRI on SBP across all valid sequences.

### 4.2 Pre-processing

Before sequence identification, apply the following preparation to the RRI and PTT series:

1. **Use only valid beats.** Remove all beats flagged as ectopic (SP-001 Section 2.3 `rri_valid == false`). Do not interpolate across gaps; instead, treat each contiguous run of valid beats as a separate sub-series. Sequence identification does not cross sub-series boundaries.

2. **Align RRI and PTT series with physiological lag.** The baroreflex has a latency: the RRI response to a SBP change is not instantaneous but delayed by approximately one cardiac cycle (the afferent-efferent loop takes roughly one beat). Therefore, align the series such that each SBP (PTT) value is paired with the **subsequent** RRI:

$$\text{Pairs}[k] = \bigl(\text{SBP}_{PTT}[k],\; \text{RRI}[k+1]\bigr)$$

This one-beat lag is standard in the Sequence Method literature (Bertinieri et al., 1985). If no lag is applied (lag = 0), also compute BRS at lag = 0 as a secondary output for researchers who prefer the zero-lag convention. Label clearly.

3. **No additional smoothing or detrending** of RRI or PTT prior to sequence detection. Detrending would remove the slow oscillations that drive the sequences.

### 4.3 Sequence Identification

Iterate over each contiguous valid-beat sub-series. For each beat $k$:

**Up-sequence detection:**

A beat $k$ initiates or continues an up-sequence if:

$$\text{SBP}_{PTT}[k] - \text{SBP}_{PTT}[k-1] \geq \delta_{SBP}$$

AND (with one-beat lag):

$$\text{RRI}[k+1] - \text{RRI}[k] \geq \delta_{RRI}$$

where the minimum step thresholds are:

$$\delta_{SBP} = 1\,\text{ms} \quad \text{(PTT units, corresponding to } \approx 1\,\text{mmHg surrogate change)}$$
$$\delta_{RRI} = 1\,\text{ms}$$

**Down-sequence detection:**

A beat $k$ initiates or continues a down-sequence if:

$$\text{SBP}_{PTT}[k] - \text{SBP}_{PTT}[k-1] \leq -\delta_{SBP}$$

AND:

$$\text{RRI}[k+1] - \text{RRI}[k] \leq -\delta_{RRI}$$

**Sequence accumulation:**

A sequence begins when the above conditions are met for two consecutive beats and is extended as long as they continue to hold. The sequence terminates when either condition fails.

**Minimum length criterion:**

A sequence is retained as a candidate only if it spans at least 3 consecutive beats (i.e., at least 3 SBP values and 3 corresponding RRI values — the minimum for linear regression). Sequences of 2 or fewer beats are discarded.

Formally, a candidate sequence of length $n \geq 3$ is the ordered pairs:

$$\mathcal{S} = \{(P_1, R_1), (P_2, R_2), \ldots, (P_n, R_n)\}$$

where $P_i = \text{SBP}_{PTT}[k_i]$ and $R_i = \text{RRI}[k_i + 1]$ for up-sequences (and symmetrically for down-sequences).

### 4.4 Quality Filter: Minimum Correlation Coefficient

For each candidate sequence $\mathcal{S}$ of length $n$, compute the Pearson correlation coefficient $r$ between the $P_i$ and $R_i$ values:

$$r = \frac{\sum_{i=1}^{n}(P_i - \bar{P})(R_i - \bar{R})}{\sqrt{\sum_{i=1}^{n}(P_i - \bar{P})^2 \cdot \sum_{i=1}^{n}(R_i - \bar{R})^2}}$$

Accept the sequence only if:

$$r^2 \geq 0.85$$

This threshold (equivalent to $|r| \geq 0.922$) ensures that the SBP-RRI relationship within the sequence is sufficiently linear to justify the regression slope as a BRS estimate. Sequences with $r^2 < 0.85$ are discarded.

**Rationale:** $r^2 \geq 0.85$ is the standard threshold in the BRS literature (Parati 2000; ESH Working Group on Blood Pressure Variability 2012). It prevents noisy or accidental monotonic runs (driven by cardiac arrhythmia or motion artefact) from biasing the mean BRS estimate.

### 4.5 BRS Slope Estimation per Sequence

For each accepted sequence, compute the ordinary least-squares (OLS) regression slope of RRI on SBP_PTT:

$$\hat{\beta} = \frac{\sum_{i=1}^{n}(P_i - \bar{P})(R_i - \bar{R})}{\sum_{i=1}^{n}(P_i - \bar{P})^2}$$

This slope $\hat{\beta}$ is the BRS estimate for this sequence, in ms/ms (when using PTT units for SBP_PTT).

The intercept is computed but not used for BRS:

$$\hat{\alpha} = \bar{R} - \hat{\beta} \cdot \bar{P}$$

### 4.6 Aggregate BRS Computation

After processing all sequences in the analysis window:

**BRS_seq_up:** Mean slope across all accepted up-sequences:

$$\text{BRS\_seq\_up} = \frac{1}{N_{up}} \sum_{j=1}^{N_{up}} \hat{\beta}_{up,j}$$

**BRS_seq_down:** Mean slope across all accepted down-sequences:

$$\text{BRS\_seq\_down} = \frac{1}{N_{down}} \sum_{j=1}^{N_{down}} \hat{\beta}_{down,j}$$

**BRS_seq_mean:** Mean slope across all accepted sequences (up and down combined):

$$\text{BRS\_seq\_mean} = \frac{1}{N_{up} + N_{down}} \sum_{j=1}^{N_{up}+N_{down}} \hat{\beta}_{j}$$

If $N_{up} < 5$ or $N_{down} < 5$, set the corresponding metric to NaN and flag `brs_seq_low_count`. Five sequences is the minimum for a reliable mean estimate; fewer than this suggests inadequate spontaneous BP variability or excessive artefact in the recording.

**Weighted mean (optional, secondary output):** Weight each sequence slope by its $r^2$ value:

$$\text{BRS\_seq\_weighted} = \frac{\sum_j r_j^2 \cdot \hat{\beta}_j}{\sum_j r_j^2}$$

This gives more influence to higher-quality sequences. Report alongside the unweighted mean; do not substitute it.

### 4.7 Numerical Example (for Reyes validation)

Given a 5-beat up-sequence with the following paired values (PTT in ms, RRI in ms):

| Beat | SBP_PTT (ms) | RRI (ms) |
|---|---|---|
| 1 | 102 | 810 |
| 2 | 104 | 818 |
| 3 | 106 | 825 |
| 4 | 108 | 831 |
| 5 | 110 | 840 |

Mean PTT = 106, mean RRI = 824.8.

$$\hat{\beta} = \frac{(102-106)(810-824.8) + (104-106)(818-824.8) + \ldots}{(102-106)^2 + (104-106)^2 + \ldots}$$

$$= \frac{(-4)(-14.8) + (-2)(-6.8) + (0)(0.2) + (2)(6.2) + (4)(15.2)}{16 + 4 + 0 + 4 + 16}$$

$$= \frac{59.2 + 13.6 + 0 + 12.4 + 60.8}{40} = \frac{146}{40} = 3.65 \text{ ms/ms}$$

Check $r^2$: compute $r$ and verify $r^2 \geq 0.85$. In this near-linear example, $r^2 \approx 0.999$ — well above threshold.

Reyes must produce this exact value (within floating-point precision) for this input. Include this case in the validation suite.

---

## 5. Secondary Algorithm: Spectral Transfer Function Method

### 5.1 Rationale

The Spectral Method (Robbe et al., 1987; Parati et al., 1995) estimates BRS as the transfer function gain between the RRI power spectrum and the SBP (PTT) power spectrum in the low-frequency band. It uses all beats in the analysis window rather than only those participating in identified sequences, and is therefore more statistically efficient with short recordings. However, it requires coherence between the two signals, which is not always guaranteed.

The two methods are complementary and should be reported together. Discordance between Sequence-BRS and Spectral-BRS is itself a clinically informative finding (it may indicate non-linear baroreflex dynamics or respiratory confounding of the LF band).

### 5.2 Time Series Preparation

**Input:** The RRI series and the PTT series (both from valid beats only, using the same ectopic-flagged series as the Sequence Method).

**Resampling:** Both series are unevenly spaced (event-driven, not fixed-rate). Resample both to a uniform time series at $F_s = 4\,\text{Hz}$ using cubic spline interpolation, identical to the HRV spectral analysis resampling in SP-001 Section 2.4.4.

$$\text{RRI}_{4Hz}(t),\quad \text{PTT}_{4Hz}(t)$$

**Detrending:** Apply linear detrending to both resampled series before spectral analysis. This removes slow baseline drifts that would inflate VLF and LF power.

### 5.3 Cross-Spectral Welch Estimation

Apply Welch's method simultaneously to both time series to produce the auto-spectral and cross-spectral density estimates. Use the same Welch parameters as SP-001 Section 2.4.4 for consistency:

| Parameter | Value |
|---|---|
| Segment length | 256 points (64 s at 4 Hz) |
| Window | Hann: $w[n] = 0.5(1 - \cos(2\pi n/N))$ |
| Overlap | 50% (128 points) |
| FFT length | 256 points |
| Frequency resolution | $\Delta f = 4/256 = 0.015625$ Hz |

For each Welch segment $m$ of length $N = 256$:

1. Apply Hann window to both $\text{RRI}_{4Hz}$ and $\text{PTT}_{4Hz}$ segments.
2. Compute the FFT of each windowed segment: $\text{RRI}_m(f)$, $\text{PTT}_m(f)$.
3. Accumulate the following spectral estimates across all $M$ segments:

**Auto-spectral density of RRI:**
$$S_{RR}(f) = \frac{1}{M} \sum_{m=1}^{M} \bigl|\text{RRI}_m(f)\bigr|^2$$

**Auto-spectral density of PTT:**
$$S_{PP}(f) = \frac{1}{M} \sum_{m=1}^{M} \bigl|\text{PTT}_m(f)\bigr|^2$$

**Cross-spectral density (RRI to PTT):**
$$S_{RP}(f) = \frac{1}{M} \sum_{m=1}^{M} \text{RRI}_m(f) \cdot \text{PTT}_m^*(f)$$

where $^*$ denotes complex conjugate. Note: $S_{RP}(f)$ is complex-valued.

Normalise all spectral estimates by the window power correction factor $U = \frac{1}{N}\sum_{n=0}^{N-1}w[n]^2$ to obtain properly scaled PSDs.

### 5.4 Transfer Function and Coherence

**Transfer function magnitude (BRS gain):**

$$|H(f)| = \sqrt{\frac{S_{RR}(f)}{S_{PP}(f)}}$$

This is the standard alpha-index formulation (DeBoer et al., 1987). The alternative formulation using the cross-spectrum directly is:

$$H(f) = \frac{S_{RP}(f)}{S_{PP}(f)}$$

Use the alpha-index (square root of the PSD ratio) as the primary estimate — it is more robust to noise. Report $|H(f)|$ as the transfer function magnitude.

**Squared coherence function:**

$$K^2(f) = \frac{|S_{RP}(f)|^2}{S_{RR}(f) \cdot S_{PP}(f)}$$

$K^2(f) \in [0, 1]$. A value of 1 indicates perfect linear coupling between RRI and PTT at frequency $f$; 0 indicates no linear coupling.

**Coherence threshold:** Only compute BRS_LF if the mean squared coherence across the LF band meets:

$$\overline{K^2}_{LF} = \frac{1}{N_{LF}} \sum_{f \in [0.04, 0.15]\,\text{Hz}} K^2(f) \geq 0.50$$

where $N_{LF}$ is the number of frequency bins within the LF band. If the threshold is not met, set `BRS_LF = NaN` and flag `brs_lf_low_coherence`.

### 5.5 BRS_LF Computation

Compute the BRS_LF estimate as the mean transfer function magnitude in the LF band, weighted by squared coherence:

$$\text{BRS\_LF} = \frac{\sum_{f \in [0.04, 0.15]\,\text{Hz}} K^2(f) \cdot |H(f)|}{\sum_{f \in [0.04, 0.15]\,\text{Hz}} K^2(f)}$$

Weighting by $K^2(f)$ ensures that frequency bins with stronger linear RRI-PTT coupling contribute more to the BRS estimate. This is the coherence-weighted alpha-index.

**Unit:** ms/ms when PTT is used as the BP surrogate (consistent with Sequence Method units). If a future calibrated BP signal is available, re-express in ms/mmHg.

---

## 6. Output Metrics

### 6.1 Per-Analysis-Window BRS Metrics

All BRS metrics are computed per analysis window. The default window is the entire continuous valid-beat segment of the recording. A sliding window mode (configurable, suggested default: 5 minutes, step 1 minute) is described in Section 6.3.

| Field | Unit | Description |
|---|---|---|
| `brs_seq_up` | ms/ms | Mean BRS from up-sequences (rising SBP_PTT, lengthening RRI) |
| `brs_seq_down` | ms/ms | Mean BRS from down-sequences (falling SBP_PTT, shortening RRI) |
| `brs_seq_mean` | ms/ms | Mean BRS across all sequences (up + down combined) |
| `brs_seq_weighted` | ms/ms | Coherence-weighted mean BRS across all sequences (secondary) |
| `brs_lf` | ms/ms | Spectral BRS: coherence-weighted alpha-index in LF band (0.04–0.15 Hz) |
| `n_seq_up` | count | Number of accepted up-sequences |
| `n_seq_down` | count | Number of accepted down-sequences |
| `n_seq_total` | count | Total accepted sequences |
| `mean_coherence_lf` | dimensionless | Mean squared coherence in LF band (0–1) |
| `brs_window_start_ms` | ms | Analysis window start time from recording start |
| `brs_window_end_ms` | ms | Analysis window end time |
| `n_beats_in_window` | count | Total beats in window (valid + invalid) |
| `n_beats_valid` | count | Valid beats used for BRS computation |
| `ptt_source` | string | "pep_firmware" or "pep_offline" — identifies the PTT proxy used |
| `brs_bp_unit` | string | Always "ms/ms" for PTT-based computation — prevents unit confusion |

### 6.2 Quality Flags

| Flag | Condition |
|---|---|
| `brs_insufficient_data` | Fewer than 300 valid beats or more than 30% rejected beats in window |
| `brs_seq_low_count` | $N_{up} < 5$ or $N_{down} < 5$ |
| `brs_lf_low_coherence` | Mean LF coherence $< 0.50$ |
| `brs_ptt_firmware_only` | PTT derived from firmware PEP (lower precision than offline B-point detection) |
| `brs_motion_contaminated` | More than 20% of window beats fall in motion-contaminated epochs (from SP-001 Section 6.3) |

### 6.3 Sliding Window Mode

For time-resolved BRS analysis (e.g., tracking BRS across experimental conditions):

- **Window length:** 300 beats (configurable; minimum 150 beats — below this, sequence count is insufficient for stable BRS estimation)
- **Step:** 60 beats (configurable)
- **Output:** One row per window in the per-epoch output table (Section 6.4)

Sliding window mode should be enabled by default. Full-recording BRS (Section 6.1) is the aggregate summary.

### 6.4 Additional Per-Beat Output Fields

Extend the SP-001 per-beat output table (Section 7.5) with the following fields:

| Field | Unit | Description |
|---|---|---|
| `ptt_ms` | ms | PTT proxy value for this beat (= `pep_ms`) |
| `sbp_ptt` | ms | $\text{SBP}_{PTT} = -\text{PTT}$ for this beat |
| `seq_id` | int | Sequence ID this beat belongs to (0 if not in a BRS sequence) |
| `seq_type` | string | "up", "down", or "" |
| `seq_accepted` | bool | True if the sequence this beat belongs to passed the $r^2 \geq 0.85$ threshold |

These per-beat fields allow researchers to inspect individual sequences and verify algorithm behaviour.

### 6.5 Validation Targets

For resting adult subjects (supine, eyes open, 5-minute recording), expected BRS ranges from the literature using the Sequence Method:

| Metric | Expected range (healthy resting adult) |
|---|---|
| BRS_seq_up | 8–25 ms/mmHg (calibrated); in ms/ms with PTT proxy, 3–15 ms/ms |
| BRS_seq_down | 5–20 ms/mmHg (calibrated); typically 20–40% lower than BRS_seq_up |
| BRS_seq_mean | 6–20 ms/mmHg (calibrated) |
| n_seq_total (5 min) | 10–50 sequences |
| BRS_LF | Comparable magnitude to BRS_seq_mean ± 30% |
| mean_coherence_lf | > 0.5 in resting subjects |

**Implementation note:** Since VU-DAMS uses PTT-based (uncalibrated) BRS, direct comparison against mmHg-referenced literature values is not valid. Use the ms/ms outputs for within-session relative comparisons only. When the Müller multi-node PPG node is available, revisit calibration.

Reyes must validate the Sequence Method implementation against the numerical example in Section 4.7 before declaring the module complete. A second validation case (with known $r^2 < 0.85$, to verify rejection logic) must also pass. Report validation results to Vasquez.

---

## 7. Implementation Notes for Reyes (Java / VU-DAMS)

### 7.1 Integration into SP-001 Processing Order

Insert the BRAVO pipeline as Step 9, after all existing SP-001 steps:

```
1. Parse blocks
2. ECG bandpass → R-peaks → RRI → HRV metrics          [SP-001 §2]
3. ICG → B-point, X-point → PEP, LVET, SV, CO           [SP-001 §3]
4. ECG baseline → respiratory rate                       [SP-001 §4]
5. EDA / SCL processing                                  [SP-001 §5]
6. IMU → motion artefact flags                           [SP-001 §6]
7. Assemble per-beat and per-epoch output                [SP-001 §7]
8. Validate all SP-001 modules against reference data    [SP-001 §8]
→ 9. BRAVO: PTT series construction                     [BRS-001 §3]
→ 10. BRAVO: Sequence Method BRS                        [BRS-001 §4]
→ 11. BRAVO: Spectral Method BRS                        [BRS-001 §5]
→ 12. BRAVO: Assemble BRS output metrics                [BRS-001 §6]
```

BRAVO must not run until Steps 2–7 are complete — it depends on the validated RRI series, PEP values, and motion flags produced by those steps.

### 7.2 Data Structures

Define a `BrsResult` class with the following fields (all public, nullable for NaN cases):

```java
public class BrsResult {
    // Window metadata
    public double windowStartMs;
    public double windowEndMs;
    public int nBeatsInWindow;
    public int nBeatsValid;
    public String pttSource;           // "pep_firmware" | "pep_offline"
    public String brsUnit;             // always "ms/ms"

    // Sequence method
    public Double brsSeqUp;            // null if NaN
    public Double brsSeqDown;
    public Double brsSeqMean;
    public Double brsSeqWeighted;
    public int nSeqUp;
    public int nSeqDown;
    public int nSeqTotal;

    // Spectral method
    public Double brsLf;
    public Double meanCoherenceLf;

    // Quality flags
    public boolean flagInsufficientData;
    public boolean flagSeqLowCount;
    public boolean flagLfLowCoherence;
    public boolean flagPttFirmwareOnly;
    public boolean flagMotionContaminated;
}
```

### 7.3 Sequence Detection Implementation

The sequence detector is a single-pass algorithm over the per-beat array. Recommended implementation:

```java
List<BrsSequence> detectSequences(double[] sbpPtt, double[] rri, boolean[] valid, 
                                   boolean upDirection) {
    List<BrsSequence> sequences = new ArrayList<>();
    List<double[]> current = new ArrayList<>();
    double sign = upDirection ? 1.0 : -1.0;

    for (int k = 0; k < sbpPtt.length - 1; k++) {
        if (!valid[k] || !valid[k+1]) {
            if (current.size() >= 3) sequences.add(finalise(current));
            current.clear();
            continue;
        }
        double dSBP = sbpPtt[k] - (k > 0 ? sbpPtt[k-1] : sbpPtt[k]);
        double dRRI  = rri[k+1] - rri[k];          // one-beat lag
        if (sign * dSBP >= 1.0 && sign * dRRI >= 1.0) {
            if (current.isEmpty()) {
                // include the starting beat pair
                current.add(new double[]{sbpPtt[k-1 >= 0 ? k-1 : k], rri[k]});
            }
            current.add(new double[]{sbpPtt[k], rri[k+1]});
        } else {
            if (current.size() >= 3) sequences.add(finalise(current));
            current.clear();
        }
    }
    if (current.size() >= 3) sequences.add(finalise(current));
    return sequences;
}
```

The `finalise` method computes $r^2$ and $\hat{\beta}$ for the accumulated pairs and sets the `accepted` flag. This is pseudocode — Reyes adjusts for edge cases (start-of-array, sub-series boundaries on ectopic gaps). The key invariant: **never cross a gap in the valid-beat series**.

### 7.4 Spectral Method: Reuse Existing Welch Infrastructure

The Welch PSD computation in SP-001 (Section 2.4.4) uses `org.apache.commons.math3.transform.FastFourierTransformer`. Reuse the same infrastructure for the BRS spectral analysis. The only additions are:

1. A second resampled series (`pttResampled`) computed identically to `rriResampled`.
2. Accumulation of the cross-spectral density $S_{RP}(f)$ as a complex array (use `Complex[]` from `org.apache.commons.math3.complex`).
3. Coherence and transfer function computation after Welch averaging.

Extract a `WelchCrossSpectral` utility class that accepts two `double[]` inputs and returns $S_{RR}$, $S_{PP}$, $S_{RP}$, $K^2$, $|H|$ as arrays indexed by frequency bin. This utility is also useful for future spectral coupling analyses (e.g., PEP-RRI coupling for sympathovagal balance metrics).

### 7.5 Suggested UI Placement in VU-DAMS

**Analysis Report tab:** Add a new "Baroreflex Sensitivity (BRAVO)" panel after the existing HRV Frequency Domain panel. Content:

- Summary table: BRS_seq_up, BRS_seq_down, BRS_seq_mean, BRS_LF, n_seq_total, mean_coherence_LF
- Time-series plot: Sliding-window BRS_seq_mean over time (overlaid on the existing RMSSD trend plot, or on a dedicated sub-panel)
- Scatter plot: SBP_PTT vs RRI for all accepted sequences, colour-coded up (green) / down (red), with individual regression lines

**Warning banner:** If `brs_bp_unit == "ms/ms"`, display a permanent non-dismissible note: *"BRS computed using PTT surrogate (ms/ms). Absolute values are not comparable to mmHg-referenced literature. Within-session comparisons only."*

**Chen (iOS):** For the live display BRAVO panel, show only BRS_seq_mean (updated every 60 seconds using the most recent 300 beats) and n_seq_total. The spectral method is offline-only — do not attempt real-time cross-spectral computation on-device.

### 7.6 Libraries

No additional libraries are required beyond those already specified in SP-001 Section 7.1. The complex cross-spectral computation uses `org.apache.commons.math3.complex.Complex`, which is part of the existing `commons-math3` dependency.

---

## 8. Open Items and Assumptions

| Item | Status | Owner |
|---|---|---|
| PTT = PEP proxy — accuracy vs true B-point PTT | Open — acceptable for Phase 1; revisit when raw ICG waveform stored | Vasquez / Müller |
| Calibrated BP input (mmHg) when multi-node PPG available | Future — BRS unit conversion to ms/mmHg pending | Vasquez |
| Optimal one-beat lag vs zero-lag convention — validate against reference dataset | Open — both lags to be computed in Phase 1; choose based on validation | Vasquez / Reyes |
| Age-stratified BRS reference ranges for BRAVO RUO report | Future — requires normative dataset; not blocking Phase 1 | Vasquez / Hart |
| Validation dataset for Sequence Method (reference BRS from invasive BP) | Vasquez to identify suitable public dataset (e.g., PhysioNet MIMIC, Autonomic Aging dataset) | Vasquez |
| iOS live BRS update interval (60 s suggested — confirm with Chen) | Open | Chen / Vasquez |
| HF-band BRS (BRS_HF, 0.15–0.40 Hz) — respiratory baroreflex component | Deferred to v2 — respiratory artefact in PTT/PEP at HF band makes this unreliable with current proxy | Vasquez |

---

## 9. References

The following peer-reviewed publications underpin the algorithms specified in this document. Reyes does not need to read these; they are provided for Vasquez's methods-section drafting and for researchers who need to cite BRAVO outputs.

- Bertinieri G et al. (1985). A new approach to analysis of the arterial baroreflex. *J Hypertens*, 3(Suppl 3):S79–81. — Original Sequence Method description.
- Parati G et al. (1988). Spectral analysis of blood pressure and heart rate variability in evaluating cardiovascular regulation. *Hypertension*, 12(6):600–610.
- Robbe HW et al. (1987). Assessment of baroreflex sensitivity by means of spectral analysis. *Hypertension*, 10(5):538–543. — Spectral (alpha-index) method.
- DeBoer RW et al. (1987). Hemodynamic fluctuations and baroreflex sensitivity in humans: a beat-to-beat model. *Am J Physiol*, 253(3):H680–9. — Transfer function model.
- La Rovere MT et al. (1988). Baroreflex sensitivity as a cardiac mortality risk factor after MI. *Lancet*, 332(8610):607–609. — Clinical validation of BRS prognostic value.
- ATRAMI Investigators (1998). Baroreflex sensitivity and heart-rate variability in prediction of total cardiac mortality after myocardial infarction. *Lancet*, 351(9101):478–484.
- Parati G et al. (2000). How to measure baroreflex sensitivity: from the cardiovascular laboratory to daily life. *J Hypertens*, 18(1):7–19. — Review and methodological guidelines.
- ESH Working Group on Blood Pressure Variability (2012). European Society of Hypertension position paper on the management of white-coat hypertension. *J Hypertens*, 31:1912–1938. — Includes BRS methodology consensus.

---

*Dr. Elena Vasquez — Slow Horses Signal Processing*  
*"BRS is where the autonomic nervous system writes its autobiography. You just have to learn to read."*
