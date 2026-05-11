# VU-DAMS CSI Offline Analysis Module — Design Document
**Document ID:** VUDAM-CSI-001  
**Author:** David Reyes — Java Software Engineer  
**Date:** 2026-05-11  
**Status:** Draft v1.0  
**Audience:** Reyes (implementation), Vasquez (review), Lam (sign-off)  
**Depends on:** CSI-001 (stress_index_spec_001.md), SP-001 (signal_processing_spec_001.md), BRS-001 (brs_spec_001.md), VUDAM architecture.md

---

## 1. Module Overview

### Class name

`CsiAnalyser` — located at `nl.vuams.vudam.analysis.CsiAnalyser`

### Role

`CsiAnalyser` is the offline Composite Stress Index computation engine for VU-DAMS. It runs after the full SP-001 and BRS-001 pipelines have completed and consumes their assembled output to produce a time-series of stress epochs for the entire recording.

### Primary input

A `RecordingSession` object — the fully parsed and processed representation of a `.vua` binary file. It holds:

- The complete `List<DataBlock>` (A, I, S, M, P, T blocks) as parsed by `VUAFileReader`
- The R-peak series and RRI array from `RPeakDetector`
- The HRV metrics object from `HRVAnalyser`
- The per-beat I-block stream (PEP, LVET, SV, CO per beat)
- The S-block stream (sclTonic, sclPhasic per S-block timestamp)
- The Y-block stream if present (firmware RMSSD per 30-beat window), or `null`
- The HAR activity classification (one label per 5-second epoch, from the IMU pipeline)
- A `BrsResult` list from BRAVO (one entry per sliding window), or an empty list if BRS was not computed
- The `SubjectMetadata` header (subject ID, recording date, electrode distance)

### Primary output

`CsiResult` — a complete stress analysis result for the session, containing:
- A `List<CsiEpoch>` — one entry per 30-second contiguous epoch
- A `SessionSummary` — scalar summary statistics for the whole recording
- A `BaselineStats` — the baseline parameters actually used for z-score normalisation

---

## 2. Data Pipeline

`CsiAnalyser` is a pure consumer of existing pipeline outputs. It does not re-read the binary file and does not re-run signal processing. All inputs are already assembled in `RecordingSession` by the time `analyse()` is called.

### 2.1 What it consumes from existing infrastructure

| Signal | Source class / field | Unit | Notes |
|---|---|---|---|
| R-peak timestamps | `RPeakDetector` → `rPeakTimesMs[]` | ms | Used to assign beats to 30-s epochs |
| RRI series + validity flags | `RPeakDetector` → `rriMs[]`, `rriValid[]` | ms / bool | Ectopic-rejected per SP-001 §2.3 |
| PEP per beat | `RecordingSession.iBlocks` → `pep_ms` field | ms | One I-block per beat; firmware value used in Phase 1 |
| SCL tonic per S-block | `RecordingSession.sBlocks` → `scl_tonic_uS` | µS | Pre-computed by firmware; use directly per SP-001 §5.4 |
| SCL phasic per S-block | `RecordingSession.sBlocks` → `scl_phasic_uS` | µS | Used for SCR event detection |
| RMSSD per window | `RecordingSession.yBlocks` → `rmssd_ms` | ms | 30-beat firmware windows; fall back to RRI computation if absent |
| HAR activity labels | `RecordingSession.harEpochs` → `primaryState` | enum | One label per 5-s epoch from IMU pipeline |
| Motion flags | `RecordingSession.motionFlags` → `aRms` | g | Per SP-001 §6.3; threshold 0.15 g |
| BRS results | `RecordingSession.brsResults` | ms/ms | Sliding-window BRAVO output; may be empty |
| Electrode contact flag | `RecordingSession.sBlocks` → `electrode_contact` | bool | S-blocks with contact==0 are excluded from SCL/SCR |

### 2.2 RMSSD fallback logic

If Y-blocks are present and non-empty, extract `rmssd_ms` directly from the firmware 30-beat windows. Assign the RMSSD value to the epoch(s) whose time range overlaps with the Y-block window.

If Y-blocks are absent or if a given 30-second epoch contains no Y-block coverage, compute RMSSD from the RRI series of valid beats within the epoch using the formula from SP-001 §2.4.1:

```
RMSSD = sqrt( (1/(N-1)) * sum( (RRI[k] - RRI[k-1])^2 ) )
```

where only `rriValid == true` beats within the epoch window are included. If fewer than 20 valid beats are available, RMSSD is set to NaN for that epoch.

### 2.3 BRS assignment to epochs

BRS is computed on 5-minute sliding windows by BRAVO. For each 30-second CSI epoch, assign the `brs_seq_mean` from the BRAVO window whose centre is closest to the epoch midpoint. If no BRAVO window falls within ±3 minutes of the epoch midpoint, `brs_seq_mean` for that epoch is set to NaN, and CSI-5 is not computed (CSI-4 is used instead). Flag `brsAssigned = false` in the epoch.

---

## 3. Epoch Structure

### 3.1 Definition

- **Epoch length:** 30 seconds
- **Overlap:** 0 — contiguous, non-overlapping epochs
- **Epoch start:** Recording start time (timestamp of first valid data block)
- **Epoch end:** Last complete 30-second window (trailing partial epoch is discarded)

Each epoch is identified by its `epochIndex` (0-based) and `epochStartMs` (milliseconds from recording start).

### 3.2 Activity gating

For each 30-second epoch, examine the HAR labels of the constituent 5-second sub-epochs (up to 6 sub-epochs per 30-second window). An epoch is activity-gated (excluded from CSI computation) if:

- More than 20% of its constituent 5-second sub-epochs are classified as a high-activity state: `WALKING`, `RUNNING`, `CYCLING`, or `STAIR_CLIMBING`
- OR any 5-second sub-epoch within the epoch has `aRms > 0.15 g` (SP-001 §6.3 motion contamination)

Activity-gated epochs have `csiActivityGated = true` and `csi = NaN` in the output.

### 3.3 Post-activity recovery window

After any epoch that triggered activity gating, apply a mandatory 5-minute (10-epoch) post-activity exclusion window. All epochs within this window have `postActivityRecovery = true` and `csi = NaN`. The recovery window resets if another activity-gated epoch occurs within it.

Implementation:

```java
int recoveryEpochsRemaining = 0;
for (CsiEpoch epoch : epochs) {
    if (epoch.csiActivityGated) {
        recoveryEpochsRemaining = 10;  // 10 x 30 s = 5 min
    } else if (recoveryEpochsRemaining > 0) {
        epoch.postActivityRecovery = true;
        recoveryEpochsRemaining--;
    }
}
```

### 3.4 Minimum valid beats

An epoch requires a minimum of 20 valid beats (i.e., `rriValid == true` beats whose R-peak falls within the epoch window) to be considered valid for RMSSD and PEP computation. If the beat count is below this threshold, the epoch is marked `epochValid = false` and all marker values are set to NaN. The epoch still appears in the output with its validity flag; it is excluded from baseline and CSI computation.

---

## 4. Per-Epoch Computation

For each epoch that is not activity-gated, not in a recovery window, and has `epochValid == true`, compute the following four primary markers.

### 4.1 Median PEP

Collect all I-block `pep_ms` values whose associated beat R-peak timestamp falls within the epoch window. Apply ectopic exclusion: discard PEP values for beats where `rriValid == false`. Compute the **median** of the remaining PEP values.

Rationale for median over mean: PEP is subject to occasional outlier B-point detection failures from the firmware; the median is more robust.

Store as `pepMs` (raw, ms). Compute `pepInv = 1000.0 / pepMs` (ms⁻¹ × 1000, dimensionless scaled inverse) for z-scoring.

If fewer than 5 valid PEP values are available in the epoch, set `pepMs = NaN` and decrement `nMarkersAvailable`.

### 4.2 RMSSD

Use Y-block value if available (Section 2.2), otherwise compute from RRI series. Store as `rmssdMs` (ms). Compute `rmssdInv = 1000.0 / rmssdMs` for z-scoring (same scaling as PEP inverse).

### 4.3 SCL tonic mean

Collect all S-block `scl_tonic_uS` values whose timestamp falls within the epoch window. Exclude blocks where `electrode_contact == 0`. Compute the arithmetic mean of the remaining values.

Store as `sclMeanUs` (µS). Used directly (no inversion) for z-scoring.

If no valid S-blocks are available in the epoch, set `sclMeanUs = NaN` and decrement `nMarkersAvailable`.

### 4.4 SCR event count and rate

Count the number of SCR events within the epoch window. An SCR event is a positive-going peak in the phasic EDA (`scl_phasic_uS`) that:
- Exceeds a threshold of **0.05 µS** above the local baseline
- Is separated from the preceding event by at least **1 second** (refractory period)
- Falls in an S-block with `electrode_contact == 1`

Note: per CSI-001 §2.5, SCR rate is ideally computed over 60 seconds for a stable estimate. For 30-second epochs, compute the count and normalise to events/min by multiplying by 2. Flag `scrRateShortEpoch = true` so that downstream analysis can weight this marker appropriately.

Store as:
- `scrEventCount` (integer, raw count in epoch)
- `scrRatePerMin` (double, events/min = count × 2.0 for 30-s epochs)

### 4.5 Storage of raw marker values and z-scores

For each epoch, store both the raw marker values and their z-scores:

```
pepMs, pepInvZscore
rmssdMs, rmssdInvZscore
sclMeanUs, sclZscore
scrRatePerMin, scrRateZscore
brsSeqMean (if assigned), brsInvZscore
nMarkersAvailable (0–4, or 0–5 with BRS)
```

Z-scores are computed in the normalisation step (Section 5). Raw values are stored before normalisation.

---

## 5. Baseline and Normalisation

### 5.1 Baseline selection (user-selectable in VU-DAMS)

Three baseline modes are exposed to the user in the VU-DAMS session configuration panel:

**Mode 1 — Session baseline (default)**

Use the first 5 minutes (10 epochs) of the recording that meet all of the following:
- Epoch is not activity-gated and not in a post-activity recovery window
- `epochValid == true`
- `nMarkersAvailable >= 3`

If fewer than 5 minutes of qualifying data exist in the first 10 minutes of the recording, extend the search through the first 15 minutes. If still insufficient, fall back to Mode 2 and set `baselineInferred = true`.

Per CSI-001 §4.3, the baseline period should correspond to a seated resting state. VU-DAMS does not enforce this automatically in the current version (no protocol file support yet), but it flags `baselineInferred = true` if the baseline was not explicitly designated.

**Mode 2 — Population norms (fallback)**

Use hardcoded population µ/σ values derived from literature (Kelsey 2000, Hjortskov 2004). These values apply to R-to-B PEP and chest-placement EDA — they are not palmar EDA norms.

| Marker | µ (population) | σ (population) | Source |
|---|---|---|---|
| PEP⁻¹ (1000/ms) | 9.52 (≈ 105 ms PEP) | 1.43 (≈ 15 ms SD) | Sherwood 1990; Kelsey 2000 |
| RMSSD⁻¹ (1000/ms) | 25.0 (≈ 40 ms RMSSD) | 8.33 (≈ 20 ms SD) | Task Force 1996; Munoz 2015 |
| SCL (µS) | 3.0 | 1.5 | Boucsein 2012 (torso placement) |
| SCR rate (events/min) | 2.0 | 1.5 | Boucsein 2012 |
| BRS⁻¹ (1/brs_seq_mean) | 0.15 (≈ 6.7 ms/ms) | 0.07 | BRS-001 §6.5 |

**Important:** Population norms produce a cross-individual absolute CSI rather than an intra-individual relative measure. Flag all outputs generated with population norms with `baselineMode = POPULATION_NORMS`. Vasquez must review any publications using population-normed CSI.

**Mode 3 — Custom baseline**

The user imports a reference recording (another `.vua` file) designated as the baseline session. `CsiAnalyser` extracts the stable low-activity mean and SD of each marker from that reference file and applies them to the current session. This enables longitudinal comparisons across sessions.

Store as `baselineMode = CUSTOM` in the output, with a `baselineSourceFile` string field.

### 5.2 Baseline computation

For the selected baseline period, compute per-marker mean and standard deviation:

```java
double muPepInv     = mean(pepInvValues_baseline);
double sigmaPepInv  = stddev(pepInvValues_baseline);
// ... same for rmssdInv, scl, scrRate, brsInv
```

Minimum valid epochs for stable baseline statistics: 5 epochs per marker. If fewer than 5 valid epochs are available for a marker during the baseline period, set that marker's baseline σ to the population norm σ and flag `markerBaselineInsufficient[i] = true`.

### 5.3 Z-score computation

For each valid epoch and each available marker:

```
z_i(t) = (x_i(t) - mu_i_base) / sigma_i_base
z_i_clamped(t) = clamp(z_i(t), -3.0, 3.0)
```

**Inversion direction (per CSI-001 §4.2):**
- PEP: higher stress → lower PEP → use `pepInv = 1000/pep`. Positive z = more stress.
- RMSSD: higher stress → lower RMSSD → use `rmssdInv = 1000/rmssd`. Positive z = more stress.
- SCL: higher stress → higher SCL. Use directly. Positive z = more stress.
- SCR rate: higher stress → more events. Use directly. Positive z = more stress.
- BRS: higher stress → lower BRS → use `brsInv = 1.0/brs_seq_mean`. Positive z = more stress.

**Marker available check before z-score:** If a marker value is NaN for the epoch (missing data), skip the z-score for that marker and use the proportional reweighting scheme defined in Section 6.

---

## 6. CSI-5 Formula (Offline Only)

### 6.1 Standard 4-marker CSI (CSI-4)

Used when BRS data is not available or `brsAssigned == false` for the epoch.

```
CSI_raw(t) = w1 * z(PEP_inv) + w2 * z(SCL) + w3 * z(RMSSD_inv) + w4 * z(SCR_rate)
```

Default weights per CSI-001 §4.3: `w = [0.35, 0.30, 0.25, 0.10]`

**Weights are not hard-coded.** They are loaded from a JSON configuration file at analysis start (see Section 9.2). The default JSON ships with VU-DAMS.

### 6.2 Extended 5-marker CSI (CSI-5)

Used when a valid `brs_seq_mean` is assigned to the epoch and `brsAssigned == true`.

```
CSI5_raw(t) = w1' * z(PEP_inv)  + w2' * z(SCL)
            + w3' * z(RMSSD_inv) + w4' * z(SCR_rate) + w5' * z(BRS_inv)
```

Default extended weights per CSI-001 §4.5: `w' = [0.30, 0.25, 0.20, 0.10, 0.15]`

These are also loaded from the JSON config, not hard-coded.

Report CSI-4 and CSI-5 as separate fields in the output. Do not overwrite CSI-4 with CSI-5.

### 6.3 Missing marker reweighting

When fewer than 4 markers are available, reweight the available markers proportionally:

```java
double weightSum = 0.0;
for (int i = 0; i < 4; i++) {
    if (markerAvailable[i]) weightSum += weights[i];
}
for (int i = 0; i < 4; i++) {
    effectiveWeight[i] = markerAvailable[i] ? weights[i] / weightSum : 0.0;
}
```

If `nMarkersAvailable < 2`, set `csi = NaN` and flag `csiUnreliable = true`. Do not output a CSI score for this epoch. A single-marker (PEP only) estimate is stored separately as `pepStressIndex` if PEP is the sole valid marker.

### 6.4 Sigmoid mapping to [0, 100]

```
CSI(t) = 100.0 / (1.0 + exp(-k * CSI_raw(t)))
```

where `k = 1.5` (per CSI-001 §4.3, Step 4).

This maps:
- `CSI_raw = 0` (at baseline) → CSI ≈ 50
- `CSI_raw = 3` (maximum clamped stress) → CSI ≈ 99
- `CSI_raw = -3` (maximum below baseline) → CSI ≈ 1

The steepness parameter `k` is configurable in the JSON config (do not hard-code).

---

## 7. Output Format

### 7.1 `CsiEpoch` — one per 30-second window

| Field | Type | Unit | Description |
|---|---|---|---|
| `epochIndex` | int | — | 0-based sequential epoch number |
| `epochStartMs` | double | ms | Epoch start time from recording start |
| `epochDurationMs` | double | ms | Always 30000.0 for complete epochs |
| `epochValid` | boolean | — | False if < 20 valid beats |
| `csiActivityGated` | boolean | — | True if excluded by activity gate |
| `postActivityRecovery` | boolean | — | True if in 5-min recovery window |
| `csi` | Double | 0–100 | 4-marker CSI score (null if unavailable) |
| `csi5` | Double | 0–100 | 5-marker extended CSI (null if BRS unavailable) |
| `pepMs` | Double | ms | Median PEP for epoch |
| `rmssdMs` | Double | ms | RMSSD for epoch |
| `sclMeanUs` | Double | µS | Mean tonic SCL |
| `scrRatePerMin` | Double | events/min | SCR rate |
| `brsSeqMean` | Double | ms/ms | BRS assigned to epoch (null if absent) |
| `pepInvZscore` | Double | — | z(PEP⁻¹), clamped [−3, 3] |
| `rmssdInvZscore` | Double | — | z(RMSSD⁻¹), clamped [−3, 3] |
| `sclZscore` | Double | — | z(SCL), clamped [−3, 3] |
| `scrRateZscore` | Double | — | z(SCR rate), clamped [−3, 3] |
| `brsInvZscore` | Double | — | z(BRS⁻¹), clamped [−3, 3] (null if absent) |
| `nMarkersAvailable` | int | 0–5 | Count of markers contributing to CSI |
| `csiUnreliable` | boolean | — | True if nMarkersAvailable < 2 |
| `brsAssigned` | boolean | — | True if a BRS window was matched |
| `respRateFlag` | boolean | — | True if respiratory rate outside 8–22 bpm |
| `scrRateShortEpoch` | boolean | — | True (always, since epochs are 30 s) |
| `effectiveWeights` | double[] | — | Actual weights used [w1..w4 or w1..w5] |

### 7.2 `SessionSummary` — scalar summary for the whole recording

| Field | Type | Unit | Description |
|---|---|---|---|
| `meanCsi` | double | 0–100 | Mean CSI across all valid, non-gated epochs |
| `sdCsi` | double | 0–100 | Standard deviation of CSI |
| `peakCsi` | double | 0–100 | Maximum CSI value |
| `peakCsiTimestampMs` | double | ms | Time of peak CSI |
| `pctTimeHighStress` | double | % | Percentage of valid epochs with CSI > 67 |
| `nEpochsTotal` | int | — | Total 30-s epochs in recording |
| `nEpochsValid` | int | — | Epochs with epochValid == true |
| `nEpochsActivityGated` | int | — | Epochs excluded by activity gate |
| `nEpochsInRecovery` | int | — | Epochs in post-activity recovery window |
| `nEpochsCsiComputed` | int | — | Epochs where CSI was successfully computed |
| `meanPepMs` | double | ms | Mean PEP across valid epochs |
| `meanRmssdMs` | double | ms | Mean RMSSD across valid epochs |
| `meanSclUs` | double | µS | Mean SCL across valid epochs |
| `meanScrRatePerMin` | double | events/min | Mean SCR rate |
| `meanBrsSeqMean` | double | ms/ms | Mean BRS (where available) |

The high-stress threshold of 67 is provisional per CSI-001 §8; it is configurable in the JSON config.

### 7.3 `BaselineStats` — documents the normalisation parameters used

| Field | Type | Description |
|---|---|---|
| `baselineMode` | enum | SESSION / POPULATION_NORMS / CUSTOM |
| `baselineSourceFile` | String | Null unless mode == CUSTOM |
| `baselineStartMs` | double | Start of baseline period in recording |
| `baselineEndMs` | double | End of baseline period |
| `nBaselineEpochs` | int | Number of epochs used for baseline |
| `baselineInferred` | boolean | True if baseline was auto-inferred |
| `muPepInv`, `sigmaPepInv` | double | Baseline stats for each marker |
| `muRmssdInv`, `sigmaRmssdInv` | double | |
| `muScl`, `sigmaScl` | double | |
| `muScrRate`, `sigmaScrRate` | double | |
| `muBrsInv`, `sigmaBrsInv` | double | Population norm values if mode != SESSION |
| `markerBaselineInsufficient` | boolean[5] | Per-marker flag for insufficient baseline data |

### 7.4 Export formats

**CSV export:** One row per `CsiEpoch`. All fields in `CsiEpoch` as columns, plus `subjectId` and `recordingDate` from the session metadata prepended. Null values exported as empty cells. Column header row included. UTF-8 encoding. File naming: `{subjectId}_{recordingDate}_csi.csv`.

**JSON export:** Full `CsiResult` serialised to JSON using Jackson (or Gson — whichever is already in the Maven `pom.xml`). Includes `CsiEpoch` list, `SessionSummary`, `BaselineStats`, and session metadata. Null fields serialised as `null`. File naming: `{subjectId}_{recordingDate}_csi.json`.

**Plot-ready data structure:** A `CsiPlotData` object consumed directly by the JavaFX chart panel (Section 9.3), containing two parallel `double[]` arrays: `timeSeconds` and `csiScores`, with NaN values at gated/invalid epochs (JavaFX LineChart will break the line at NaN values, correctly showing gaps for gated periods).

---

## 8. Java Class Skeleton

```java
package nl.vuams.vudam.analysis;

import nl.vuams.vudam.model.*;
import java.util.List;

/**
 * CsiAnalyser — offline Composite Stress Index computation for VU-DAMS.
 *
 * Consumes a fully processed RecordingSession (SP-001 + BRS-001 outputs complete)
 * and produces a CsiResult containing per-epoch CSI scores and session-level summary.
 *
 * Thread safety: not thread-safe; create a new instance per analysis run.
 *
 * Depends on: SP-001, BRS-001, HAR pipeline (all must have run before calling analyse()).
 * Called from: MainController (on the JavaFX analysis executor thread, not UI thread).
 */
public class CsiAnalyser {

    private final CsiConfig config;   // loaded from csi_config.json

    public CsiAnalyser(CsiConfig config) {
        this.config = config;
    }

    /**
     * Entry point. Run after all SP-001 and BRS-001 processing is complete.
     *
     * @param session  Fully processed recording session including R-peaks, I-blocks,
     *                 S-blocks, HAR labels, motion flags, and BRS results.
     * @param baseline Baseline configuration: mode (SESSION / POPULATION_NORMS / CUSTOM)
     *                 and optional reference file path for CUSTOM mode.
     * @return CsiResult containing epoch list, session summary, and baseline stats used.
     */
    public CsiResult analyse(RecordingSession session, BaselineConfig baseline) { ... }

    /**
     * Slice the recording into contiguous 30-second epochs, apply activity gating,
     * post-activity recovery windows, and minimum beat count validation.
     * Returns all epochs (valid, gated, and recovery) — validity flags are set on each.
     */
    private List<CsiEpoch> computeEpochs(RecordingSession session) { ... }

    /**
     * Identify the baseline period within the session (for SESSION mode).
     * Returns the BaselineStats object with mu/sigma for each marker.
     * Falls back to population norms if insufficient low-activity data found.
     */
    private BaselineStats computeBaseline(List<CsiEpoch> epochs,
                                          BaselineConfig baselineConfig,
                                          RecordingSession session) { ... }

    /**
     * Compute median PEP (ms) from I-blocks whose R-peak timestamp falls within
     * [epochStartMs, epochStartMs + 30000]. Returns NaN if fewer than 5 valid beats.
     *
     * @param iBlocks  All I-blocks for the session (filtered to epoch window internally)
     * @param beatValidity  Per-beat ectopic validity flags from RPeakDetector
     */
    private double computePep(List<IBlock> iBlocks, boolean[] beatValidity) { ... }

    /**
     * Compute RMSSD (ms) for the epoch.
     * Prefers Y-block firmware value if available and covers the epoch window.
     * Falls back to computing from RRI series using SP-001 §2.4.1 formula.
     * Returns NaN if fewer than 20 valid beats.
     *
     * @param yBlocks   Firmware RMSSD blocks (may be null or empty)
     * @param rriSeries Valid RRI values within the epoch window (pre-filtered)
     */
    private double computeRmssd(List<YBlock> yBlocks, List<Double> rriSeries) { ... }

    /**
     * Compute mean tonic SCL (µS) from S-blocks within the epoch window.
     * Excludes S-blocks with electrode_contact == 0.
     * Returns NaN if no valid S-blocks.
     *
     * @param sBlocks  S-blocks within the epoch window
     */
    private double computeScl(List<SBlock> sBlocks) { ... }

    /**
     * Detect SCR events in the phasic EDA signal within the epoch window and return
     * the rate in events/min. Threshold: 0.05 µS; refractory: 1 s.
     * Normalises count to events/min (count * 2.0 for 30-s epochs).
     * Returns NaN if no valid S-blocks with electrode contact.
     *
     * @param sBlocks  S-blocks within the epoch window (contact==1 only)
     */
    private double computeScrRate(List<SBlock> sBlocks) { ... }

    /**
     * Compute z-score with clamping to [-3, 3].
     * Applies inversion for markers where higher raw value = lower stress
     * (PEP, RMSSD, BRS) — caller passes the pre-inverted value (1000/pep, 1000/rmssd,
     * 1.0/brs), so this method applies only the z-score formula and clamping.
     *
     * @param value  Pre-transformed marker value (inverted if applicable)
     * @param mean   Baseline mean for this marker
     * @param std    Baseline standard deviation for this marker
     * @return Clamped z-score in [-3, 3], or NaN if value or std is NaN/zero
     */
    private double zscore(double value, double mean, double std) { ... }

    /**
     * Apply sigmoid mapping: 100 / (1 + exp(-k * csiRaw)).
     * k is loaded from CsiConfig (default 1.5 per CSI-001).
     *
     * @param csiRaw  Weighted sum of z-scores (typically in [-3, 3])
     * @param k       Sigmoid steepness parameter
     * @return CSI score in (0, 100)
     */
    private double sigmoid(double csiRaw, double k) { ... }

    /**
     * Apply proportional reweighting when fewer than 4 markers are available.
     * Returns effective weight array summing to 1.0.
     * Throws IllegalStateException if nMarkersAvailable < 2 (caller should check
     * and set csiUnreliable = true before calling this method).
     *
     * @param baseWeights     Configured weight vector (length 4 or 5)
     * @param markerAvailable Boolean array indicating which markers are present
     */
    private double[] reweightMarkers(double[] baseWeights, boolean[] markerAvailable) { ... }

    /**
     * Build the SessionSummary from the completed epoch list.
     */
    private SessionSummary buildSummary(List<CsiEpoch> epochs) { ... }

    /**
     * Build the CsiPlotData (x=time in seconds, y=CSI score) for the JavaFX chart.
     * NaN values at gated/invalid epochs cause line breaks in the chart.
     */
    private CsiPlotData buildPlotData(List<CsiEpoch> epochs) { ... }
}
```

### Supporting configuration class

```java
/**
 * CSI configuration loaded from csi_config.json at startup.
 * No weights, thresholds, or steepness values may be hard-coded in CsiAnalyser.
 */
public class CsiConfig {
    public double[] weights4;          // default [0.35, 0.30, 0.25, 0.10]
    public double[] weights5;          // default [0.30, 0.25, 0.20, 0.10, 0.15]
    public double sigmoidK;            // default 1.5
    public double highStressThreshold; // default 67.0
    public double scrThresholdUs;      // default 0.05 µS
    public int minValidBeatsPerEpoch;  // default 20
    public int postActivityRecoveryEpochs; // default 10 (= 5 min)
    public double activityGateMaxFraction; // default 0.20
    // Population norm mu/sigma for fallback baseline
    public double muPepInv; public double sigmaPepInv;
    public double muRmssdInv; public double sigmaRmssdInv;
    public double muScl; public double sigmaScl;
    public double muScrRate; public double sigmaScrRate;
    public double muBrsInv; public double sigmaBrsInv;
}
```

---

## 9. Integration Points

### 9.1 Position in the VU-DAMS processing order

`CsiAnalyser.analyse()` is called as Step 13 in the full pipeline, after all SP-001, BRS-001, and HAR steps are complete:

```
1–2.   VUAFileReader.read()          → List<DataBlock>
3.     RPeakDetector.detect()        → R-peaks, RRI series (SP-001 §2)
4.     HRVAnalyser.compute()         → RMSSD, SDNN, LF/HF (SP-001 §2.4)
5.     ICG pipeline                  → PEP, LVET, SV, CO per beat (SP-001 §3)
6.     Respiratory rate extraction   → resp_rate per epoch (SP-001 §4)
7.     EDA / SCL processing          → scl_tonic, scl_phasic, SCR events (SP-001 §5)
8.     IMU / motion flags            → aRms per epoch (SP-001 §6)
9.     BRAVO PTT construction        → sbp_ptt series (BRS-001 §3)
10.    BRAVO Sequence Method         → brs_seq_mean per window (BRS-001 §4)
11.    BRAVO Spectral Method         → brs_lf per window (BRS-001 §5)
12.    HAR pipeline                  → activity label per 5-s epoch (SP-002)
→ 13. CsiAnalyser.analyse()         → CsiResult (this module)
14.    UI assembly                   → update Stress Analysis tab
```

`MainController` orchestrates this sequence on a background executor and updates the JavaFX UI on the Platform.runLater() thread after Step 14.

### 9.2 Existing classes consumed

| Existing class | Used by CsiAnalyser as |
|---|---|
| `VUAFileReader` | Indirectly — `RecordingSession` is already populated |
| `RPeakDetector` | R-peak timestamps, RRI series, `rriValid[]` flags |
| `HRVAnalyser` | RMSSD per epoch (if Y-blocks absent), respiratory rate flags |
| `DataBlock` (sealed record hierarchy) | `IBlock`, `SBlock`, `MBlock`, `YBlock` record types |
| `BrsResult` | `brs_seq_mean` per sliding window for CSI-5 |

The HAR pipeline (SP-002) produces a `List<HarEpoch>` attached to `RecordingSession.harEpochs`. `CsiAnalyser` reads `harEpoch.primaryState` and the associated `motionFlags.aRms` to gate epochs.

### 9.3 New UI panels required

A new **"Stress Analysis"** tab must be added to `MainController` (new FXML tab in the existing `TabPane`). Three sub-components are needed:

**Panel 1 — CSI Timeline Plot**

A `LineChart<Number, Number>` with:
- X-axis: time in seconds
- Y-axis: CSI score 0–100
- Two series: CSI-4 (solid line) and CSI-5 (dashed line, only shown if BRS available)
- Background colour bands: green (0–45), yellow (45–65), red (65–100) — implemented as stacked `Rectangle` nodes behind the chart
- Grey vertical bands for activity-gated and post-recovery epochs
- Experimental condition label overlays if a protocol file is loaded (future)

Data source: `CsiPlotData` (x=time seconds, y=CSI) from `CsiResult`.

**Panel 2 — Session Summary**

A `GridPane` or `TableView<SummaryRow>` showing:
- Mean CSI ± SD
- Peak CSI with timestamp
- % time above 67 (high stress)
- Number of valid epochs / total epochs / gated epochs
- Baseline mode used

**Panel 3 — Epoch Table**

A `TableView<CsiEpoch>` with columns for all fields in `CsiEpoch` (Section 7.1). Sortable, filterable. Colour-coded rows: grey for gated/invalid, white for valid, amber for `csiUnreliable == true`. Context menu: "Export selected epochs to CSV".

An **Export** button in the tab toolbar triggers both CSV and JSON export using the paths defined in Section 7.4.

---

## 10. Testing Plan

### 10.1 Unit tests

All unit tests live in `src/test/java/nl/vuams/vudam/analysis/CsiAnalyserTest.java`.

**RMSSD computation (known RRI input → expected RMSSD)**

```
Input RRI series (ms): [800, 810, 795, 820, 805, 815, 800, 810, 800, 790]
Expected RMSSD: sqrt(mean_of_squared_successive_diffs)
  Successive diffs: [10, -15, 25, -15, 10, -15, 10, -10]
  Squared diffs:    [100, 225, 625, 225, 100, 225, 100, 100]
  Mean:             100 + 225 + 625 + 225 + 100 + 225 + 100 + 100 = 1700 / 8 = 212.5
  RMSSD = sqrt(212.5) ≈ 14.58 ms
```

Assert: `computeRmssd(null, rriList) ≈ 14.58 ms` (tolerance 0.01 ms).

**z-score clamping**

- Input: value=200.0, mean=100.0, std=10.0 → raw_z = 10.0 → clamped to 3.0
- Input: value=0.0, mean=100.0, std=10.0 → raw_z = −10.0 → clamped to −3.0
- Input: value=110.0, mean=100.0, std=10.0 → z = 1.0 (no clamping)

**sigmoid mapping**

- `sigmoid(0.0, 1.5)` → exactly 50.0
- `sigmoid(3.0, 1.5)` → ≈ 98.9 (assert > 98.0 && < 100.0)
- `sigmoid(-3.0, 1.5)` → ≈ 1.1 (assert > 0.0 && < 2.0)

**Known CSI-4 score**

Given four z-scores: z(PEP⁻¹)=1.5, z(SCL)=2.0, z(RMSSD⁻¹)=1.0, z(SCR)=0.5 and default weights [0.35, 0.30, 0.25, 0.10]:

```
CSI_raw = 0.35*1.5 + 0.30*2.0 + 0.25*1.0 + 0.10*0.5
        = 0.525 + 0.600 + 0.250 + 0.050 = 1.425
CSI = 100 / (1 + exp(-1.5 * 1.425))
    = 100 / (1 + exp(-2.1375))
    = 100 / (1 + 0.1179)
    = 100 / 1.1179 ≈ 89.45
```

Assert: `csi ≈ 89.45` (tolerance 0.1).

**Missing marker reweighting**

Given weights [0.35, 0.30, 0.25, 0.10] and SCR marker unavailable (markerAvailable = [true, true, true, false]):
```
Available weight sum = 0.35 + 0.30 + 0.25 = 0.90
Effective weights = [0.35/0.90, 0.30/0.90, 0.25/0.90, 0.0]
                  ≈ [0.389, 0.333, 0.278, 0.0]
```

Assert: effective weights sum to 1.0 and unavailable marker weight == 0.0.

**Activity gating**

Construct a mock `RecordingSession` with epochs where epochs 3–5 (0-indexed) are classified as `WALKING`. Assert that epochs 3–5 have `csiActivityGated == true` and epochs 6–15 (the 5-minute recovery window) have `postActivityRecovery == true`.

**SCR event detection**

Synthetic phasic EDA series with three clear peaks of 0.08 µS amplitude separated by 2 seconds each within a 30-second window. Assert `computeScrRate(sBlocks) ≈ 6.0 events/min` (3 events × 2.0 normalisation factor).

### 10.2 Integration tests

**Synthetic recording — known stress/rest periods**

The iOS simulator (future work — Chen) will produce synthetic `.vua` files with controlled physiological signals representing:
- Epochs 1–10: baseline rest (PEP ~105 ms, RMSSD ~40 ms, SCL ~2.0 µS)
- Epochs 11–20: simulated stress (PEP ~88 ms, RMSSD ~22 ms, SCL ~5.0 µS)
- Epochs 21–30: recovery (returning to baseline)

Expected behaviour:
- CSI during baseline rest: ≈ 50 (by definition of the sigmoid at zero z-score)
- CSI during stress block: > 70 on average
- CSI during recovery: declining from stress peak toward 50

Assert: `meanCsi(epochs 11–20) > meanCsi(epochs 1–10) + 15` and `meanCsi(epochs 11–20) > 65`.

**BRS integration — CSI-5 availability**

Construct a session with sufficient beat count (> 300) and a valid `BrsResult`. Assert that:
- `brsAssigned == true` for epochs within 3 minutes of a BRS window
- `csi5` is non-null for those epochs
- `csi5 != csi` (the two formulas differ in weights, so scores should differ unless all z-scores are zero)

### 10.3 Reference dataset validation

**WESAD dataset (Schmidt et al., 2018)**

Access: PhysioNet — https://physionet.org/content/wesad/ (also mirrored on UCI ML Repository).

WESAD contains chest-worn RespiBAN ECG, EDA, and accelerometer data from 15 subjects undergoing TSST, amusement, and neutral conditions. Map WESAD signals to VU-DAMS equivalents:

| WESAD signal | VU-DAMS equivalent | Notes |
|---|---|---|
| Chest ECG → R-peaks → RRI | `RPeakDetector` output | Apply SP-001 §2.2–2.3 to WESAD ECG |
| Chest EDA tonic component | S-block `scl_tonic_uS` | Apply SP-001 §5.2 offline |
| Chest EDA phasic SCR | S-block `scl_phasic_uS` | Apply SP-001 §5.3 |
| Chest ACC | M-block accelerometer | Use for activity gating |

Note: WESAD does not include ICG, so PEP is unavailable. Run CSI-3 (SCL, RMSSD, SCR only with proportional reweighting) on WESAD. This validates the EDA and HRV components.

Target: AUC ≥ 0.70 for binary classification of TSST vs. neutral epochs using the 3-marker CSI on WESAD data (lower threshold than CSI-001's AUC ≥ 0.75 requirement, because PEP is absent).

Validation output: a WESAD validation report CSV (`operations/validation/wesad_csi_validation.csv`) produced by a standalone `WesadValidator` runner class. Vasquez reviews and signs off.

---

## 11. Open Items and Assumptions

| Item | Status | Owner |
|---|---|---|
| Y-block (firmware RMSSD) format — confirm struct layout in data_blocks.h | Open | Müller / Reyes |
| HAR pipeline (SP-002) output format — confirm `HarEpoch` class and enum values | Open | Reyes (pending SP-002 spec) |
| Jackson vs Gson for JSON export — check existing pom.xml dependencies | Open | Reyes |
| `CsiConfig` JSON schema — define and document alongside csi_module_design.md | Open | Reyes |
| Weissler PEP correction (rate-adjusted PEP) — implement as optional flag per CSI-001 §8 | Open | Reyes / Vasquez |
| CSI high-stress threshold (67) — provisional; revise after lab validation | Open | Vasquez |
| Sigmoid steepness k=1.5 — revise after empirical CSI distribution from lab data | Open | Vasquez |
| iOS simulator synthetic data format — coordinate with Chen for integration test input | Open | Reyes / Chen |
| Population norm µ/σ values for torso EDA — VU-AMS-specific norms pending lab data | Open | Vasquez / Nair |
| Post-activity recovery window (5 min) — validate with VU-AMS walk-to-sit recordings | Open | Vasquez / Reyes |

---

*David Reyes — Java Software Engineer, Slow Horses*  
*"The signals say what they say. My job is to make sure the arithmetic doesn't lie."*
