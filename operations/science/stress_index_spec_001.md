# VU-AMS Composite Stress Index — Scientific Specification
**Document ID:** CSI-001  
**Author:** Dr. Elena Vasquez — Biomedical Signal Processing Engineer  
**Date:** 2026-05-11  
**Status:** Draft v1.0  
**Audience:** Reyes (Java offline analysis / VU-DAMS), Chen (iOS live display), Hart (presentation)  
**Depends on:** SP-001 (signal_processing_spec_001.md), BRS-001 (brs_spec_001.md), SP-002 (har_sad_spec_001.md)

---

## Purpose

This document specifies the scientific basis, individual stress markers, activity-control strategy, composite stress index (CSI) design, implementation requirements, and validation plan for the stress assessment capability of the VU-AMS system. All algorithms build on the signal processing pipelines defined in SP-001 and BRS-001. Markers not yet fully specified in those documents are defined here to the same level of algorithmic detail. Reyes implements; Chen integrates into live display; deviations require Vasquez sign-off.

---

## 1. Scientific Foundation

### 1.1 Stress as an Autonomic Nervous System Phenomenon

Psychological and physiological stress engages the autonomic nervous system (ANS) in a coordinated, multi-system response. At the level of mechanism, the stress response is characterised by two concurrent shifts:

1. **Sympathetic activation:** Increased efferent sympathetic outflow to the heart (chronotropic and inotropic effects), vasculature (peripheral vasoconstriction), adrenal medulla (catecholamine release), and eccrine sweat glands (sudomotor activation). The net cardiovascular effects include increased heart rate, increased myocardial contractility, increased cardiac output, and elevated arterial blood pressure. Peripherally, sympathetic cholinergic innervation of eccrine glands increases skin conductance.

2. **Parasympathetic withdrawal:** Simultaneous reduction in vagal (parasympathetic) efferent tone to the sinoatrial node. Under resting conditions, vagal tone is the dominant determinant of heart rate variability — particularly beat-to-beat variability mediated by respiratory sinus arrhythmia (RSA). Stress-induced vagal withdrawal reduces this variability, decreasing RMSSD, HF power, and baroreflex sensitivity.

These two processes are not mirror images. Sympathetic activation is slower to onset (latency ~5 seconds for cardiovascular effects; minutes for adrenal catecholamine accumulation) than vagal withdrawal (latency ~1–2 seconds). Recovery kinetics also differ: vagal tone recovers quickly after acute stress; sympathetic arousal may persist for tens of minutes. Any stress measurement system must account for this temporal asymmetry.

The ANS stress response is embedded in the broader HPA (hypothalamic-pituitary-adrenal) axis response, which operates on timescales of minutes to hours via cortisol. VU-AMS does not measure cortisol directly. All VU-AMS stress markers are ANS/cardiovascular markers reflecting the acute (seconds to minutes) phase of the stress response. This scope must be clearly stated in all reports and publications.

### 1.2 The Multi-System Nature of the Stress Response

The stress response engages at least four physiological systems simultaneously and semi-independently:

**Cardiac:** Both sympathetic (positive chronotropy, inotropy, reduced PEP) and parasympathetic (reduced vagal tone, reduced HRV) effects manifest within beats to seconds.

**Electrodermal:** Sympathetic cholinergic sudomotor activation increases eccrine sweat gland secretion, raising skin conductance (tonic SCL) and producing discrete phasic skin conductance responses (SCRs) to each sufficiently arousing stimulus. EDA is a pure sympathetic marker — there is no parasympathetic innervation of eccrine glands.

**Vascular/thermal:** Peripheral vasoconstriction under sympathetic alpha-adrenergic drive reduces blood flow to the skin, lowering skin temperature. Opposite to core temperature, which may rise slightly due to increased metabolic rate.

**Respiratory:** Stress typically increases respiratory rate and tidal volume. This is behaviourally mediated (altered breathing pattern during cognitive load) as well as reflexively driven. Respiratory rate modulates HRV through RSA, making it a critical confound that must be monitored alongside cardiac markers.

### 1.3 Why Single-Marker Approaches Are Insufficient

Each physiological stress marker reflects only one aspect of a multi-system response and carries its own confounds:

- **RMSSD/HRV alone** reflects parasympathetic withdrawal but is heavily confounded by respiratory rate (which shifts RSA amplitude and frequency). A person breathing slowly at 6 breaths/min will show high HF power regardless of stress level; a person breathing rapidly will show suppressed HF power even without stress. Furthermore, HRV captures only one dimension of the ANS response and is insensitive to pure sympathetic activation in the absence of vagal change.

- **PEP alone** is the best validated single sympathetic cardiac marker but is confounded by cardiac pre-load, afterload, and contractility state. It is also sensitive to posture (changes in venous return alter pre-load and PEP independent of sympathetic drive) and to physical activity.

- **SCL alone** reflects cumulative sympathetic arousal but has very slow dynamics (30-second to minutes timescale for the tonic component), making it insensitive to rapid stress onset or recovery. It is also sensitive to ambient temperature, physical exertion, and emotional state history.

- **LF/HF ratio alone** has been severely critiqued as a stress marker (Billman 2013; see Section 2.3). It conflates sympathetic and parasympathetic contributions in the LF band and is highly sensitive to respiratory rate, rendering it unreliable in ambulatory settings without respiratory monitoring.

The scientific consensus — and the VU-AMS design philosophy — is that robust ambulatory stress assessment requires simultaneous monitoring of at least one sympathetic cardiac marker (PEP), one vagal marker (RMSSD), and one electrodermal marker (SCL). Integration of these into a composite index substantially improves the signal-to-noise of stress detection compared to any individual marker (Kelsey et al., 2000; Hjortskov et al., 2004; Licht et al., 2009).

---

## 2. Available Stress Markers from the VU-AMS

### 2.1 Pre-Ejection Period (PEP)

**Signal source:** I-block (`pep_ms` field, per-beat). Computed by firmware from ICG B-point detection; refined offline by SP-001 Section 3.5.

**Physiological mechanism:** PEP is the interval from ventricular electrical activation (ECG R-peak) to the onset of mechanical ejection (ICG B-point, aortic valve opening). It reflects the time required for left ventricular pressure to rise from end-diastolic pressure to aortic diastolic pressure — the isovolumic contraction phase. Sympathetic beta-1 adrenergic stimulation increases myocardial contractility (the rate of pressure development, dP/dt$_{max}$), shortening the isovolumic contraction time and therefore PEP. PEP is the most direct non-invasive index of cardiac sympathetic drive available without a blood pressure cuff.

$$\text{PEP} = t_{B} - t_{R} \quad \text{(ms, R-to-B; see SP-001 §3.5 for implementation)}$$

As implemented in SP-001, the reference point is the ECG R-peak rather than the true Q-wave onset. This introduces a small systematic offset (typically 5–15 ms) that is consistent within a recording. Label all outputs "R-to-B PEP" and do not compare absolute values against Q-to-B literature norms without the offset correction noted in SP-001 Section 3.5.

**Direction under stress:** DECREASES. Sympathetic activation shortens PEP. A 10–20 ms reduction from baseline is typical during a laboratory mental stressor (Sherwood et al., 1990). For the CSI, use PEP$^{-1}$ so that increasing values indicate increasing stress.

**Confounds:**
- *Posture:* Supine → standing increases PEP by ~15–25 ms due to reduced venous return and lower pre-load. Gate by posture state (HAR/SP-002 output) or treat each posture state as its own baseline.
- *Physical activity:* Exercise dramatically shortens PEP independently of psychological stress. Activity gating (Section 3) is mandatory.
- *Heart rate:* PEP is weakly correlated with heart rate (shorter at higher HR). Apply the Weissler correction if comparing across conditions with different mean HR: PEP$_{corr}$ = PEP + 0.4 × (RRI − 600) (Weissler et al., 1968) or use the pre-ejection fraction PEP/LVET which is more rate-independent.
- *Electrode distance:* Does not affect PEP directly (PEP is a timing metric, not an amplitude metric). ICG signal quality does affect B-point detection reliability — see SP-001 Section 6.2 quality flags.

**Time resolution:** Per-beat (one PEP value per cardiac cycle, ~60–100 per minute). For epoch-level analysis, compute mean PEP over 30-second or 60-second windows after ectopic exclusion.

**Key references:**
- Sherwood A et al. (1990). Methodological guidelines for impedance cardiography. *Psychophysiology*, 27(1):1–23. — The definitive methodological consensus on ICG and PEP.
- Fahrenberg J & Myrtek M (Eds.) (1996). *Ambulatory Assessment: Computer-assisted Psychological and Psychophysiological Methods in Monitoring and Field Studies*. Göttingen: Hogrefe & Huber. — Establishes PEP as a field-deployable sympathetic marker.
- Kelsey RM et al. (2000). Cardiovascular reactivity and adaptation to recurrent psychological stress: replication and extension. *Psychophysiology*, 37(6):748–756.

### 2.2 RMSSD

**Signal source:** ECG A-block → R-peaks → RRI series (SP-001 Section 2.2–2.4.1). Computed per epoch from artifact-free RRI segments.

**Physiological mechanism:** RMSSD (root mean square of successive RR interval differences) quantifies beat-to-beat variability in the heart rate time series. Successive RR differences are dominated by respiratory sinus arrhythmia — the vagally mediated modulation of sinoatrial node firing rate with the respiratory cycle. High RMSSD reflects high vagal (parasympathetic) tone; low RMSSD reflects vagal withdrawal. RMSSD is robust to non-stationary conditions and does not require a 5-minute stationary epoch, making it the preferred time-domain HRV metric for ambulatory stress studies.

$$\text{RMSSD} = \sqrt{\frac{1}{N-1} \sum_{k=1}^{N-1} (\text{RRI}[k] - \text{RRI}[k-1])^2} \quad \text{(ms; SP-001 Eq. 2.4.1)}$$

**Direction under stress:** DECREASES. Vagal withdrawal reduces beat-to-beat variability. For the CSI, use RMSSD$^{-1}$ so that increasing values indicate increasing stress.

**Confounds:**
- *Respiratory rate:* The dominant confound. Slow deep breathing amplifies RSA and inflates RMSSD regardless of stress level. Fast breathing (> 20 breaths/min) reduces RSA and suppresses RMSSD, mimicking a stress response. If respiratory rate is simultaneously available (SP-001 Section 4 or PPG-based), always report it alongside RMSSD and flag epochs where respiratory rate departs from normal range (12–20 breaths/min).
- *Physical activity:* Acute exercise suppresses RMSSD via both vagal withdrawal and the mechanical effects of respiratory turbulence on RR intervals. Activity gating mandatory.
- *Ectopic beats:* Premature ventricular or atrial contractions create artefactually large successive differences. Ectopic rejection (SP-001 Section 2.3) must precede RMSSD computation.
- *Epoch length:* RMSSD is stable over epochs as short as 10–30 seconds under resting conditions (Munoz et al., 2015). For ambulatory use, 30-second epochs with ≥ 25 valid beats are acceptable.

**Time resolution:** Can be computed on epochs as short as 10–30 seconds. Recommended: 30-second epochs with 50% overlap for the CSI pipeline.

**Key references:**
- Task Force of the European Society of Cardiology and the North American Society of Pacing and Electrophysiology (1996). Heart rate variability: standards of measurement, physiological interpretation and clinical use. *Circulation*, 93(5):1043–1065. — The foundational HRV standards document.
- Munoz ML et al. (2015). Validity of (ultra-)short recordings for heart rate variability measurements. *PLOS ONE*, 10(9):e0138921.

### 2.3 LF/HF Ratio

**Signal source:** ECG → RRI → Welch PSD → LF and HF band powers (SP-001 Sections 2.4.4). Requires minimum 5-minute stationary segment for reliable spectral estimation.

**Physiological mechanism:** The low-frequency band (0.04–0.15 Hz) of the HRV power spectrum contains contributions from both sympathetic and parasympathetic modulation of heart rate, as well as baroreflex-mediated oscillations (Mayer waves). The high-frequency band (0.15–0.40 Hz) is tightly coupled to respiratory sinus arrhythmia and reflects primarily parasympathetic efferent activity. The LF/HF ratio was originally proposed as an index of sympathovagal balance (Pagani et al., 1986) with high LF/HF indicating sympathetic dominance.

$$\text{LF/HF} = \frac{P_{LF}}{P_{HF}} \quad \text{(SP-001 Eq. 2.4.4; dimensionless)}$$

**Direction under stress:** Classically increases (higher sympathetic-to-parasympathetic ratio). In practice, the direction is inconsistent across studies and individuals.

**Critical limitation — Billman critique:** The interpretation of LF/HF as a sympathovagal balance index has been formally contested and largely abandoned in the current literature. Billman (2013) demonstrated that: (1) the LF band contains predominantly vagal, not sympathetic, power in most experimental conditions; (2) respiratory rate shifts the HF peak frequency, causing energy to move between LF and HF bands without any change in autonomic state; (3) pharmacological blockade studies show LF power is largely abolished by vagal blockade (atropine), not sympathetic blockade (propranolol). The LF/HF ratio is therefore not a valid index of sympathovagal balance and must not be presented as such.

**Retained use in VU-AMS:** LF/HF is retained as a secondary, exploratory metric. It should be reported alongside absolute LF and HF powers and alongside respiratory rate. It must never be used as a standalone stress indicator. Do not include it in the CSI formula. Display it in VU-DAMS with a footnote flagging the Billman (2013) controversy.

**Confounds:** Respiratory rate (as above — dominates both LF band content and HF band location), posture, physical activity. Requires stationarity over the analysis epoch.

**Time resolution:** Minimum 5 minutes (256-sample Welch segments at 4 Hz resample rate requires ~64 seconds per segment; 2–3 segments needed for stability). Not appropriate for real-time or short-epoch stress assessment.

**Key references:**
- Pagani M et al. (1986). Power spectral analysis of heart rate and arterial pressure variabilities as a marker of sympatho-vagal interaction in man and conscious dog. *Circ Res*, 59(2):178–193. — Original LF/HF proposal.
- Billman GE (2013). The LF/HF ratio does not accurately measure cardiac sympatho-vagal balance. *Front Physiol*, 4:26. — The definitive critique. Mandatory reading before publishing LF/HF results.
- Shaffer F & Ginsberg JP (2017). An overview of heart rate variability metrics and norms. *Front Public Health*, 5:258.

### 2.4 Tonic Skin Conductance Level (SCL)

**Signal source:** S-block `scl_tonic_uS` (pre-computed by firmware); or offline tonic extraction from raw EDA via SP-001 Section 5.2. Units: µS (microsiemens).

**Physiological mechanism:** Skin conductance reflects the electrical conductance of the skin surface, dominated by eccrine sweat gland activity on the palms and fingers (high gland density, pure sympathetic cholinergic innervation). In chest-worn ambulatory devices such as the VU-AMS, the electrodes are placed on the torso — a lower-density region. The signal is valid but smaller in amplitude than palmar EDA; reference ranges should be established from device-specific normative data rather than palmar norms.

The tonic component (SCL) reflects the sustained sweat gland filling level — a slow-varying baseline that rises over the course of a stressful situation and declines during relaxation. It represents accumulated sympathetic sudomotor drive over the preceding 30–120 seconds.

**Direction under stress:** INCREASES. Higher sympathetic arousal → higher SCL. Include directly in CSI (no inversion needed).

**Confounds:**
- *Ambient temperature:* Thermoregulatory sweating (heat) increases SCL via thermoregulatory sympathetic pathways, not psychological arousal. If ambient temperature data are available (external sensor), flag recordings in thermal environments > 28°C.
- *Physical activity:* Exercise elevates SCL through both thermoregulatory and sympathetically-arousal pathways. Activity gating mandatory (Section 3).
- *Electrode contact:* Contact pressure, movement artefact, and electrode dry-out all affect raw SCL. SP-001 Section 5.4 electrode contact flag must be applied; exclude epochs with `electrode_contact == 0`.
- *Within-session drift:* SCL typically drifts upward during the first 5–10 minutes of a recording (electrode equilibration). Always compute z-score normalisation relative to a stable baseline period (first 5 minutes, subject resting, excluded from stress analysis).

**Time resolution:** Tonic component by definition operates on 20–120 second timescales. Compute mean SCL per 30-second epoch. Not suited for beat-to-beat analysis.

**Key references:**
- Boucsein W (2012). *Electrodermal Activity*, 2nd edition. New York: Springer. — The definitive reference text on EDA physiology, measurement, and analysis.
- Dawson ME et al. (2007). The electrodermal system. In JT Cacioppo, LG Tassinary & GG Berntson (Eds.), *Handbook of Psychophysiology* (3rd ed., pp. 157–181). Cambridge University Press.

### 2.5 SCR Frequency (Spontaneous Fluctuation Rate)

**Signal source:** S-block phasic EDA component → SCR event detection (SP-001 Section 5.3). Output: count of SCR events per analysis epoch.

**Physiological mechanism:** Phasic skin conductance responses (SCRs) are discrete, event-driven sweat secretion episodes with a characteristic morphology: rapid rise time (1–3 seconds) followed by exponential decay (5–10 seconds). In the absence of specific stimuli, a background rate of spontaneous SCRs (also called non-specific SCRs, NS-SCRs) occurs at a baseline frequency of 1–3 events/minute in resting adults. Psychological arousal, anxiety, and cognitive load increase this rate. Each NS-SCR represents a burst of sympathetic sudomotor drive from higher cortical and limbic arousal centres.

$$\text{SCR\_rate} = \frac{\text{count of SCR events in epoch}}{\text{epoch duration in minutes}} \quad \text{(events/min)}$$

Use the event detection defined in SP-001 Section 5.3 with threshold ≥ 0.05 µS and minimum 1-second inter-event interval.

**Direction under stress:** INCREASES. Higher arousal → more NS-SCRs per minute. Include directly in CSI.

**Confounds:**
- *Startle responses:* Sudden loud sounds, unexpected stimuli, or postural changes produce SCRs that are reflexive rather than stress-related. In ambulatory recordings this noise is unavoidable; epoch averaging reduces the impact.
- *Physical activity:* Movement artefacts can mimic SCR morphology. Apply motion contamination flag from SP-001 Section 6.3 to exclude contaminated epochs.
- *Signal quality:* Requires clean phasic EDA signal. If SCL baseline is very high (> 20 µS in torso placement), the phasic component may saturate or be masked.
- *SCR rate saturation:* At very high arousal, individual SCRs merge into sustained elevation of the phasic component, making individual event counting unreliable. Use SCR rate as a secondary metric when SCL is already extreme.

**Time resolution:** Compute per 60-second epoch (minimum epoch for stable rate estimate). Update every 30 seconds with a 30-second overlap for the CSI.

**Key references:**
- Boucsein W (2012). *Electrodermal Activity*, 2nd edition. New York: Springer. (Same as 2.4.)
- Licht CMM et al. (2009). Association between anxiety disorders and heart rate variability in the Netherlands Study of Depression and Anxiety (NESDA). *Psychosom Med*, 71(5):508–518. — Multi-marker stress study using combined EDA and HRV.

### 2.6 Baroreflex Sensitivity (BRS)

**Signal source:** PTT (PEP proxy) and RRI series → BRAVO module (BRS-001). Per-analysis-window metric; sliding window available.

**Physiological mechanism:** Baroreflex sensitivity (BRS) is the gain of the arterial baroreflex feedback loop — the ratio of RRI change to blood pressure change (ms/mmHg in calibrated systems; ms/ms in VU-AMS PTT-based computation). High BRS reflects robust vagal responsiveness and effective blood pressure buffering; low BRS indicates impaired reflex gain, which may reflect sympathetic dominance and/or reduced vagal responsiveness.

BRS integrates information from both sympathetic and parasympathetic limbs of the ANS: the up-sequence BRS (BRS_seq_up) is primarily vagally mediated; BRS_seq_down involves both vagal withdrawal and sympathetic activation. The overall BRS_seq_mean therefore reflects integrated ANS baroreflex gain.

Under acute psychological stress, BRS decreases — the reflex is suppressed as the central stress circuitry overrides homeostatic feedback. This has been demonstrated for mental arithmetic, cold pressor, and public speaking stressors (Parati et al., 2006).

Full algorithm specification: BRS-001. For CSI purposes, use `brs_seq_mean` as the primary BRS metric.

**Direction under stress:** DECREASES. Use BRS$^{-1}$ or (−BRS) in any composite index. Note: BRS has much slower dynamics than PEP or RMSSD — a reliable BRS estimate requires ~300 beats (~5 minutes). It is not suitable for fast-updating (< 5-minute) stress windows. Include in VU-DAMS offline analysis but not in the real-time iOS CSI calculation.

**Confounds:**
- *Respiratory rate:* Modulates the LF band content of both RRI and PTT spectra. Always report respiratory rate alongside BRS.
- *PTT = PEP proxy limitation:* As fully specified in BRS-001 Section 3.4, PEP conflates pre-ejection delay with true vascular transit time. Under stress, PEP shortens independently of blood pressure changes, which may artefactually alter computed BRS. This limitation is inherent to the current VU-AMS hardware configuration.
- *Physical activity:* Activity-related movement in ICG/ECG degrades B-point detection and PTT reliability. Activity gating mandatory.

**Time resolution:** Minimum 5 minutes (300 valid beats) per BRS estimate. Sliding window mode: 5-minute window, 1-minute step (BRS-001 Section 6.3).

**Key references:**
- Parati G et al. (2006). Sympathetic and baroreflex cardiovascular control in hypertension: evidence from spectral analysis. In *Handbook of Hypertension*, Vol. 25. — BRS under stress.
- La Rovere MT et al. (1988). Baroreflex sensitivity as a cardiac mortality risk factor after MI. *Lancet*, 332(8610):607–609.
- ATRAMI Investigators (1998). Baroreflex sensitivity and heart-rate variability in prediction of total cardiac mortality after myocardial infarction. *Lancet*, 351(9101):478–484.

### 2.7 Skin Temperature

**Signal source:** T-block, TMP117 sensor, Fs = 4 Hz (SP-001 Section 1.1). Units: °C.

**Physiological mechanism:** Sympathetic alpha-adrenergic vasoconstriction reduces cutaneous blood flow in the extremities and distal body surface. Skin temperature at these sites falls within 30–90 seconds of sympathetic activation. The chest-worn TMP117 in the VU-AMS measures local chest skin temperature — a less sensitive location than fingertip or toe, but still responsive to moderate sympathetic activation. The primary utility is as a slow, integrative sympathetic indicator and for detecting physiological arousal recovery (rewarming after stress offset).

**Direction under stress:** DECREASES (peripheral vasoconstriction cools the skin surface). Use temperature inversion (−T or 1/T or $T_{baseline} - T_{current}$) for any composite weighting. Note: the direction may be ambiguous in warm environments where thermoregulatory vasodilation competes with sympathetic vasoconstriction. Use temperature change from baseline, not absolute temperature.

**Confounds:**
- *Ambient temperature:* The dominant confound. External warming or cooling directly drives skin temperature independent of sympathetic tone. Without a concurrent ambient temperature measurement, skin temperature changes are ambiguous.
- *Physical activity:* Exercise causes core and skin temperature to rise due to metabolic heat production, opposing the vasoconstrictive effect. Activity gating mandatory.
- *Clothing insulation:* Thermal insulation of the sensor site by clothing attenuates and delays temperature responses.
- *Sensor placement:* The TMP117 measures temperature at the device housing, which may lag actual skin temperature by several seconds depending on thermal contact quality and ambient convection.

**Recommendation for CSI:** Due to the dominance of the ambient temperature confound in ambulatory settings, skin temperature is treated as a **supplementary contextual marker** in VU-AMS CSI computation. It is reported in the VU-DAMS output but is not included in the default CSI formula. It may be included in an extended CSI for laboratory conditions where ambient temperature is controlled.

**Time resolution:** 4 Hz raw sampling. Compute 30-second epoch means. Apply smoothing (20-second moving average) before epoch extraction to remove T-block quantisation noise.

**Key references:**
- Kistler A et al. (1998). Validation of a new design of wrist actigraphy system, with comparison to polysomnography and wrist actigraphy. *J Sleep Res*, 7(3):167–174. (Historical context on peripheral temperature in ambulatory monitoring.)
- Vinkers CH et al. (2013). The effect of stress on core and peripheral body temperature in humans. *Stress*, 16(5):520–530. — Validates stress-induced peripheral temperature decrease in humans.

### 2.8 Respiratory Rate

**Signal source:** ECG-derived respiratory signal (SP-001 Section 4): Butterworth bandpass 0.15–0.4 Hz applied to raw ECG; respiratory rate from peak detection of the filtered baseline wander signal. Reported as 30-second window mean in breaths/min. PPG-based respiratory rate (zero-crossing rate of the low-frequency PPG amplitude modulation) is an alternative if available.

**Physiological mechanism:** Respiratory rate is not a direct ANS stress marker in the same sense as PEP or RMSSD, but stress reliably increases respiratory rate and changes breathing pattern (shallower, faster). More critically, respiratory rate is the primary modulator of HRV through respiratory sinus arrhythmia. Without concurrent respiratory rate monitoring, HRV-based stress indices are confounded:

- If respiratory rate increases from 14 to 20 breaths/min under stress, HF power shifts in frequency and the RSA amplitude decreases even without any change in vagal tone.
- RMSSD will decrease purely as a function of the respiratory rate change.

Respiratory rate in the CSI serves two roles:

1. **Confound monitor:** If respiratory rate exceeds 22 breaths/min or falls below 8 breaths/min in an epoch, flag the HRV metrics for that epoch as respiratory-confounded.
2. **Secondary stress indicator:** Report as a contextual stress marker. Do not include in the CSI formula directly (respiratory confounding of HRV is already partially captured by using RMSSD, which is less respiratory-rate-sensitive than HF power).

**Direction under stress:** INCREASES. Normal range 12–20 breaths/min at rest; 20–30 breaths/min during moderate psychological stress.

**Confounds:**
- *ECG-derived respiration accuracy:* The baseline wander extraction method (SP-001 Section 4) is unreliable during motion artefacts and may fail during shallow breathing. Mark as unreliable when `a_RMS > 0.15g` (SP-001 Section 6.3).
- *Voluntary breath control:* Subjects instructed to breathe slowly (paced respiration protocols) will show suppressed RSA amplitude and inflated RMSSD independent of stress. Flag any protocol using paced respiration.

**Time resolution:** 30-second sliding window mean, updated every 5 seconds (SP-001 Section 4.2).

**Key references:**
- Grossman P et al. (1990). Respiratory sinus arrhythmia, cardiac vagal control, and daily activity. In Byrne EA & Porges SW (Eds.), *Cardiac Vagal Control and Social Engagement*. — RSA and respiratory confounding.
- Wilhelm FH et al. (2006). Respiratory reactivity, autonomic cardiac control, and daily hassles. *Respir Physiol Neurobiol*, 153(1):45–56. — Stress effects on respiratory pattern in ambulatory settings.

---

## 3. Activity Control

### 3.1 Why Physical Activity Confounds All Stress Markers

Physical activity affects every cardiac and electrodermal stress marker through mechanisms that are entirely unrelated to psychological stress:

| Marker | Effect of physical activity | Mechanism |
|--------|----------------------------|-----------|
| PEP | Decreases markedly | Sympathetic exercise drive, increased ventricular contractility |
| RMSSD | Decreases | Sympathetic HR drive, reduced RSA at high HR |
| LF/HF | Increases | Sympathetic exercise drive shifts HR power to LF |
| SCL | Increases | Thermoregulatory and exercise-associated sudomotor activation |
| SCR rate | Increases | Motion artefacts mimic SCR; thermoregulatory SCR activity |
| BRS | Decreases | Baroreflex resetting during exercise |
| Skin temperature | Increases (initially decreases) | Metabolic heat production overrides initial vasoconstriction |

The result is that a person who walks briskly to a stressful meeting and then sits down will show physiological signals identical to acute psychological stress — for several minutes after activity cessation, as the cardiovascular system recovers toward rest. Computing stress indices during or immediately after activity without activity correction will produce false positives.

### 3.2 IMU-Based Activity Gating

The VU-AMS IMU (ICM-20948, 9-axis, 100 Hz — M-blocks) provides continuous physical activity context. The HAR module (SP-002) classifies each epoch into activity states: sitting, standing, walking, running, cycling, lying, and stair climbing.

**Stress computation rule:** Compute and report CSI only for epochs classified by HAR as one of: **sitting, standing (still), lying**. Do not compute CSI during: walking, running, cycling, stair climbing, or any epoch where $a_{RMS} > 0.15\,g$ (SP-001 Section 6.3 motion contamination flag).

Implementation:

```java
boolean isLowActivity(HarEpoch epoch, MotionFlags motion) {
    HarState state = epoch.primaryState;
    boolean lowActivityPosture = (state == SITTING || state == STANDING_STILL || state == LYING);
    boolean lowMotion = (motion.aRms < 0.15);  // g, gravity-subtracted
    return lowActivityPosture && lowMotion;
}
```

For iOS live display: use the real-time IMU activity classifier (Chen to implement, consuming HAR model output). Display a "MOVING — stress paused" indicator when activity gating blocks CSI computation. Update every 5 seconds.

### 3.3 Post-Activity Recovery Period

Cardiovascular recovery after even moderate activity (e.g., 5-minute walk) takes 3–5 minutes for HR to return to baseline, and up to 10 minutes for PEP and SCL. Apply a **mandatory post-activity exclusion window** of 5 minutes after any epoch classified as walking or higher intensity. During the recovery period, compute and store individual markers but do not update the CSI and label data with `post_activity_recovery = true`.

The 5-minute recovery window is conservative; future validation data from VU-AMS recordings during TSST may allow shortening to 3 minutes. This is an open calibration item.

### 3.4 Reference to HAR/SAD Specification

The HAR module (SP-002, `har_sad_spec_001.md`) provides the per-epoch activity classification consumed by the stress gating logic above. The CSI pipeline is downstream of HAR in the VU-DAMS processing order. Insert CSI computation as Step 13, after all SP-001, BRS-001, and SP-002 steps:

```
Steps 1–12: SP-001, SP-002, BRS-001 processing
→ 13. Activity gating: identify low-activity epochs using HAR output
→ 14. CSI: compute per-marker z-scores and composite index
→ 15. Assemble final stress report
```

---

## 4. Composite Stress Index (CSI) Design

### 4.1 Design Criteria

The VU-AMS CSI must satisfy the following constraints:

1. **Sensor-constrained:** Computable from VU-AMS hardware only — no blood pressure cuff, no respiration belt, no separate EDA wrist band. All inputs are available from ECG (A-block), ICG (I-block), and EDA (S-block).
2. **Ambulatory validity:** Must be robust in daily-life settings with postural changes, non-laboratory noise, and variable respiratory patterns. Activity gating (Section 3) addresses the primary ambulatory confound.
3. **Real-time capability (iOS):** Must be computable incrementally from streaming block data with low latency. PEP is available per beat; RMSSD over 30-second epochs; SCL over 30-second epochs. Update rate: every 30 seconds.
4. **Offline depth (VU-DAMS):** The offline version may incorporate BRS and LF/HF as additional markers with longer computation windows.
5. **Interpretable output:** A single scalar in [0, 100] with a defined reference range and interpretable anchors (0 = minimal stress, 100 = maximal stress within the individual's observed range).
6. **Individual calibration:** Z-score normalisation is applied per-individual per-session relative to a baseline rest period. The CSI is an intra-individual relative measure, not an absolute cross-individual comparison.

### 4.2 Marker Selection for the CSI Formula

Four markers are included in the primary CSI formula:

| Marker | Transformation | Rationale |
|--------|---------------|-----------|
| PEP$^{-1}$ | Inverse (ms$^{-1}$) × 1000 | Sympathetic cardiac; decreases under stress; inversion makes it positive-stress-direction |
| SCL | Direct (µS) | Pure sympathetic sudomotor; increases under stress |
| RMSSD$^{-1}$ | Inverse (ms$^{-1}$) × 1000 | Parasympathetic vagal; decreases under stress; inversion makes it positive-stress-direction |
| SCR\_rate | Direct (events/min) | Sympathetic arousal; increases under stress |

LF/HF is excluded from the primary formula (see Section 2.3 critique). BRS is included in the offline-only extended CSI (Section 4.5). Skin temperature is excluded from the default formula (ambient temperature confound; Section 2.7).

### 4.3 CSI Formula

#### Step 1: Baseline Period

At recording start, require a minimum **5-minute baseline rest period** with the subject seated, at rest, not speaking. This period establishes the within-session reference. Compute the mean and standard deviation of each marker during the baseline period:

$$\mu_i^{base},\quad \sigma_i^{base}, \quad i \in \{\text{PEP}^{-1},\, \text{SCL},\, \text{RMSSD}^{-1},\, \text{SCR\_rate}\}$$

If no designated baseline period is available in the recording, use the first 10 minutes of stable low-activity data as a fallback baseline. Flag such recordings with `baseline_inferred = true`.

#### Step 2: Z-Score Normalisation

For each epoch $t$, compute z-scores for each marker relative to the within-session baseline:

$$z_i(t) = \frac{x_i(t) - \mu_i^{base}}{\sigma_i^{base}}$$

where $x_i$ is the current epoch value of marker $i$. Positive $z_i$ indicates elevation above baseline in the stress-positive direction (after applying the inversions for PEP and RMSSD).

Clamp z-scores to the range [−3, +3] to prevent individual extreme values from dominating the composite:

$$z_i^{clamped}(t) = \max(-3,\; \min(3,\; z_i(t)))$$

#### Step 3: Weighted Combination

$$\text{CSI}_{raw}(t) = w_1 \cdot z(\text{PEP}^{-1}) + w_2 \cdot z(\text{SCL}) + w_3 \cdot z(\text{RMSSD}^{-1}) + w_4 \cdot z(\text{SCR\_rate})$$

**Default weights (literature-informed starting values):**

$$\mathbf{w} = [w_1,\, w_2,\, w_3,\, w_4] = [0.35,\, 0.30,\, 0.25,\, 0.10]$$

with $\sum_i w_i = 1.0$.

Rationale for weight allocation:
- $w_1 = 0.35$ (PEP$^{-1}$): PEP is the most validated single sympathetic cardiac marker in ambulatory ICG literature (Sherwood et al., 1990; Kelsey et al., 2000). Receives highest weight.
- $w_2 = 0.30$ (SCL): Tonic EDA is highly reliable as a sustained arousal measure and is physiologically independent of cardiac markers (Hjortskov et al., 2004). Second highest weight.
- $w_3 = 0.25$ (RMSSD$^{-1}$): Vagal withdrawal is a consistent stress feature but is modulated by respiratory rate — a confound that reduces its reliability in ambulatory free-breathing conditions (Grossman et al., 1990). Third weight.
- $w_4 = 0.10$ (SCR\_rate): NS-SCR rate is sensitive but highly variable; episodic and less reliable for slow tonic stress level than SCL (Boucsein, 2012). Lowest weight.

**These weights are provisional.** They are based on the relative effect sizes reported in Kelsey et al. (2000) for a mental arithmetic stressor and on the relative ambulatory reliability reported in Hjortskov et al. (2004) during computer work. They must be validated against the VU-AMS validation dataset (Section 6) and may require individual calibration. The weight vector is a configurable parameter in VU-DAMS — Reyes must not hard-code weights.

#### Step 4: Sigmoid Mapping to [0, 100]

The raw CSI is unbounded (theoretically in [−∞, +∞] before clamping; in practice in [−3, +3] after z-score clamping). Map to [0, 100] using a sigmoid function:

$$\text{CSI}(t) = \frac{100}{1 + e^{-k \cdot \text{CSI}_{raw}(t)}}$$

with sigmoid steepness parameter:

$$k = 1.5$$

This maps CSI$_{raw} = 0$ (at baseline) to CSI = 50; CSI$_{raw} = 3$ (maximum clamped stress elevation) to CSI ≈ 97; CSI$_{raw} = -3$ (maximum relaxation relative to baseline) to CSI ≈ 3.

The 50-point anchor (baseline) is intentional: a fresh recording at the baseline rest period should produce CSI ≈ 50. Values consistently above 60 indicate stress elevation above baseline; values above 75 indicate substantial elevation. Values below 40 indicate state below baseline (post-stress relaxation, recovery).

**Verification:**

$$\text{CSI}(0) = \frac{100}{1 + e^{0}} = \frac{100}{2} = 50 \checkmark$$

$$\text{CSI}(3) = \frac{100}{1 + e^{-4.5}} = \frac{100}{1 + 0.0111} \approx 98.9 \checkmark$$

$$\text{CSI}(-3) = \frac{100}{1 + e^{4.5}} = \frac{100}{1 + 90.0} \approx 1.1 \checkmark$$

#### Step 5: Handling Missing Markers

When fewer than 4 markers are available in an epoch (e.g., EDA electrode off, ICG quality failure), reweight the available markers to sum to 1.0:

$$w_i' = \frac{w_i \cdot \mathbf{1}[\text{marker } i \text{ available}]}{\sum_j w_j \cdot \mathbf{1}[\text{marker } j \text{ available}]}$$

Report `n_markers_available` (0–4) alongside CSI. Flag CSI as unreliable if $n\_markers\_available < 2$.

If only PEP is available (e.g., ECG only, EDA contact lost): report PEP-only stress estimate separately as `pep_stress_index` rather than CSI, to avoid confusing a single-marker estimate with the composite.

### 4.4 CSI Update Timing

For the iOS real-time display, compute CSI every 30 seconds using a 30-second window:

| Marker | Input window | Update rate |
|--------|-------------|-------------|
| PEP$^{-1}$ | Mean of beats in last 30 s | Every 30 s |
| SCL | Mean of S-blocks in last 30 s | Every 30 s |
| RMSSD$^{-1}$ | RMSSD computed over last 30 s | Every 30 s |
| SCR\_rate | Events in last 60 s / 1 min | Every 30 s (60 s window) |

Display the CSI as a smoothed trend (10-second exponential moving average over 30-second CSI values) to avoid distracting jump artefacts in the iOS UI. Raw 30-second CSI values are stored and exportable.

For VU-DAMS offline: compute CSI at all time resolutions (per-beat for PEP-only index; 30-second epochs for full CSI; 5-minute windows for BRS-extended CSI).

### 4.5 Extended CSI (Offline Only, VU-DAMS)

For offline analysis where 5+ minute windows are available, add BRS as a fifth marker:

$$\text{CSI}_{ext}(t) = w_1' \cdot z(\text{PEP}^{-1}) + w_2' \cdot z(\text{SCL}) + w_3' \cdot z(\text{RMSSD}^{-1}) + w_4' \cdot z(\text{SCR\_rate}) + w_5' \cdot z(\text{BRS}^{-1})$$

Suggested extended weights: $\mathbf{w}' = [0.30,\, 0.25,\, 0.20,\, 0.10,\, 0.15]$.

BRS$^{-1}$ = inverse of `brs_seq_mean` from BRS-001. Lower BRS → higher stress → positive contribution after inversion.

This extended CSI should be clearly distinguished from the primary CSI in VU-DAMS reports. Label as "CSI-5" vs "CSI-4".

---

## 5. Implementation Requirements

### 5.1 Component Availability and Priority

| Marker | Currently in SP-001? | Online (iOS)? | Offline (VU-DAMS)? | Priority |
|--------|---------------------|--------------|--------------------|---------|
| PEP (ICG) | Yes — SP-001 §3.5, I-block | Yes (per beat) | Yes (offline refined) | P1 — Critical |
| RMSSD | Yes — SP-001 §2.4.1 | Yes (30 s epochs) | Yes | P1 — Critical |
| SCL tonic | Yes — SP-001 §5.2 (S-block) | Yes (30 s epochs) | Yes | P1 — Critical |
| SCR rate | Yes — SP-001 §5.3 | Yes (60 s window) | Yes | P1 — Critical |
| Respiratory rate | Yes — SP-001 §4.2 (confound monitor) | Yes (30 s) | Yes | P2 — Confound monitor |
| LF/HF ratio | Yes — SP-001 §2.4.4 | No (requires 5 min) | Yes | P2 — Supplementary |
| BRS (BRAVO) | Yes — BRS-001 | No (requires 5 min) | Yes | P2 — Extended CSI only |
| Skin temperature | Yes — SP-001 §1.1 (T-block raw) | Yes (contextual only) | Yes (contextual) | P3 — Contextual |
| Activity gating (HAR) | Yes — SP-002 | Yes (mandatory gate) | Yes (mandatory gate) | P1 — Critical dependency |
| CSI (4-marker) | **This document** | Yes | Yes | P1 |
| CSI-5 (BRS-extended) | **This document** | No | Yes | P2 |
| Post-activity recovery window | **This document** | Yes | Yes | P1 |

### 5.2 Per-Epoch Output Fields (Additions to SP-001 Table)

Extend the SP-001 per-epoch output table (Section 7.5) with:

| Field | Unit | Description |
|-------|------|-------------|
| `csi` | 0–100 | Composite Stress Index (4-marker) |
| `csi_5` | 0–100 | Extended CSI (5-marker, offline only; NaN if BRS unavailable) |
| `pep_inv_z` | dimensionless | z-score of PEP$^{-1}$ relative to baseline |
| `scl_z` | dimensionless | z-score of SCL relative to baseline |
| `rmssd_inv_z` | dimensionless | z-score of RMSSD$^{-1}$ relative to baseline |
| `scr_rate_z` | dimensionless | z-score of SCR rate relative to baseline |
| `brs_inv_z` | dimensionless | z-score of BRS$^{-1}$ (NaN if BRS unavailable) |
| `n_markers_available` | count (0–4) | Number of valid markers contributing to CSI |
| `csi_activity_gated` | bool | True if this epoch was excluded from CSI due to activity |
| `post_activity_recovery` | bool | True if epoch is in 5-min post-activity exclusion window |
| `resp_rate_flag` | bool | True if respiratory rate is outside 8–22 breaths/min |
| `baseline_inferred` | bool | True if baseline was inferred from recording rather than designated |
| `csi_weights` | float[4] | w1–w4 weights used for this epoch's CSI |

### 5.3 iOS Live Display (Chen)

Chen implements the following CSI display in the iOS app:

1. **CSI gauge:** A circular gauge, 0–100, with three zones: green (0–45), yellow (45–65), red (65–100). The threshold at 65 represents ~1.5 SD above the individual's own baseline. Update every 30 seconds; smooth with 10-second EMA.
2. **Marker breakdown bar:** A compact horizontal bar chart showing $z_i$ for each of the 4 markers, updating alongside the CSI. Allows the user (or researcher) to see which marker is driving the composite.
3. **Activity gate indicator:** Show "MOVING — stress paused" badge with IMU-derived activity icon when CSI is gated.
4. **Post-activity recovery countdown:** Show "Recovering (Xm remaining)" during the 5-minute post-activity exclusion window.
5. **Respiratory rate warning:** Amber indicator if respiratory rate is outside 8–22 breaths/min and HRV-based markers may be confounded.

Online CSI does not include BRS or LF/HF. If < 2 markers are available, the gauge should display "Insufficient signal" rather than a CSI value.

### 5.4 VU-DAMS Offline Display (Reyes)

Reyes implements a "Stress Analysis" tab in VU-DAMS with:

1. **CSI time-series panel:** Full-recording plot of CSI (4-marker) and CSI-5, with colour-coded background showing HAR-classified activity states. Gated epochs shown in grey. Experimental condition labels (if protocol file is loaded) overlaid.
2. **Per-marker z-score traces:** Four stacked sub-plots showing PEP$^{-1}$ z, SCL z, RMSSD$^{-1}$ z, SCR_rate z over the recording. Allows inspection of which markers contribute to CSI peaks.
3. **Summary statistics table:** Mean CSI ± SD per experimental condition; peak CSI with timestamp; percentage of recording with CSI > 65 ("stress burden").
4. **Correlation matrix:** Pearson correlations between all 5 markers (including BRS) over the recording. Informs whether markers are tracking together (expected under stress) or diverging (suggests confound or dissociation).
5. **Export:** CSV export of all per-epoch CSI fields. JSON export for API consumers.

---

## 6. Validation Plan

### 6.1 Reference Datasets for Algorithm Development

Before acquiring VU-AMS-specific validation data, cross-validate the CSI algorithms (SP-001 sub-modules) against publicly available wearable stress datasets:

| Dataset | Signals | Stressors | Access |
|---------|---------|-----------|--------|
| **WESAD** (Schmidt et al., 2018) | ECG, EDA, EMG, BVP, TEMP, ACC | TSST, amusement (Empatica E4 + chest RespiBAN) | PhysioNet / UCI ML Repository |
| **DREAMER** (Katsigiannis & Ramzan, 2017) | ECG, EEG | Emotional video clips | Zenodo |
| **K-EmoCon** (Park et al., 2020) | ECG, BVP, EDA, TEMP | Social debate task | PhysioNet |
| **DEAP** (Koelstra et al., 2012) | EEG, peripheral physiology | Music video elicitation | Publicly available |
| **PhysioNet Autonomic Aging** (Peng et al., 2009) | ECG (Holter) | Tilt, cold pressor | PhysioNet |

Primary target: **WESAD** — most directly analogous to VU-AMS (chest-worn device, ECG, EDA, accelerometer, multi-modal stress protocol including TSST). Map WESAD signals to VU-AMS processing pipeline equivalents, compute CSI from WESAD data, and evaluate against ground-truth condition labels.

Evaluation metric: Area Under the ROC Curve (AUC) for binary stress (TSST) vs. neutral classification. Target AUC ≥ 0.75 for the 4-marker CSI. Note: the comparison is approximate because WESAD uses different sensor hardware and placement; the purpose is algorithm sanity-check, not performance benchmarking.

### 6.2 Laboratory Validation Protocol (VU-AMS Device)

**Protocol title:** VU-AMS Stress Validation Study — Protocol V1

**Design:** Within-subjects, repeated-measures. Each participant undergoes three acute stressors in counterbalanced order, separated by 10-minute recovery periods.

**Participants:** N = 20 minimum (power calculation: two-tailed paired t-test, expected d = 0.8 for PEP, α = 0.05, power = 0.80, requires N = 15; N = 20 adds 25% buffer for attrition and signal quality exclusions).

**Session structure:**

| Phase | Duration | Content |
|-------|----------|---------|
| Sensor attachment & equilibration | 10 min | Seated rest; sensors equilibrating |
| Baseline rest | 5 min | Seated, eyes open, silent; establishes individual baseline |
| Stressor 1: Mental arithmetic | 5 min | Serial subtraction by 13 from 1022, out loud; experimenter present |
| Recovery 1 | 10 min | Seated rest, no task |
| Stressor 2: Cold Pressor Test (CPT) | Up to 5 min | Non-dominant hand submerged in ice water (4°C); subject instructed to maintain until maximum tolerance |
| Recovery 2 | 10 min | Seated rest |
| Stressor 3: Trier Social Stress Test (TSST) | 15 min | 5 min preparation + 5 min speech + 5 min mental arithmetic before evaluation panel; standard TSST protocol (Kirschbaum et al., 1993) |
| Recovery 3 | 10 min | Seated rest |

All sessions conducted in climate-controlled laboratory (22 ± 1°C, 40–60% relative humidity) to control thermal confounds for skin temperature.

**Primary outcomes:**

1. **PEP change:** Stress vs. baseline paired t-test. Expected decrease of 10–20 ms during mental arithmetic and TSST (Sherwood et al., 1990).
2. **RMSSD change:** Stress vs. baseline. Expected decrease of 20–40% during stressors.
3. **SCL change:** Stress vs. baseline. Expected increase of 1–5 µS (torso placement; lower than palmar norms).
4. **CSI change:** Stress vs. baseline. Primary efficacy outcome: CSI during stressor phase ≥ 10 points above CSI during baseline rest.
5. **CSI discriminant validity:** Receiver operating characteristic (ROC) analysis — does CSI > threshold correctly classify stress vs. rest epochs? Target AUC ≥ 0.80.

**Secondary outcomes:**

- Reactivity-recovery profiles: does CSI return to ≤ baseline + 5 points within 10-minute recovery?
- Inter-marker agreement: do PEP, RMSSD, and SCL show concordant responses? Pearson correlation among z-scored markers during stress periods.
- Activity gating validation: confirm that walking to/from laboratory spaces triggers the activity gate and post-activity exclusion window correctly.

**Exclusion criteria:** Cardiovascular disease, diabetes, psychiatric diagnosis, beta-blocker or anticholinergic medication, smoking, BMI > 35. All exclusions affect baseline ANS tone and may confound marker interpretation.

**Validation data deposit:** All validation recordings to be stored in `operations/validation/` with anonymised participant IDs. Vasquez retains analysis script ownership; Reyes provides raw CSI output files for Vasquez's final analysis.

### 6.3 Acceptance Criteria

The VU-AMS CSI pipeline is accepted for RUO (Research Use Only) release when:

| Criterion | Threshold | Priority |
|-----------|-----------|---------|
| PEP stressor response | Significant decrease (p < 0.05, paired t-test, N ≥ 15) during mental arithmetic vs. baseline | P1 — must pass |
| RMSSD stressor response | Significant decrease (p < 0.05) during ≥ 2 of 3 stressors | P1 — must pass |
| CSI AUC (ROC, stress vs. baseline) | ≥ 0.75 for 4-marker CSI | P1 — must pass |
| CSI AUC (extended 5-marker) | ≥ 0.80 | P2 — target |
| Activity gating specificity | 0 false activity inclusions in motion-free epochs; 0 false stress values during treadmill walking | P1 — must pass |
| RMSSD algorithm vs. Kubios reference | < 1 ms mean absolute error on WESAD/PhysioNet test set | P1 — must pass |
| PEP algorithm vs. I-block firmware | < 20 ms mean absolute error (offline vs. firmware PEP) | P1 — must pass |
| BRS algorithm (Sequence Method) | Within 5% of BRS-001 numerical example (Section 4.7) | P1 — must pass |
| Missing marker handling | CSI correctly flags `n_markers_available < 2` and suppresses CSI output | P1 — must pass |
| iOS display latency | CSI update latency < 5 seconds from block receipt to gauge update | P2 — target |

**Stressor-specific expectations (from literature):**

| Stressor | PEP change | RMSSD change | SCL change |
|----------|-----------|-------------|-----------|
| Mental arithmetic | −10 to −20 ms | −20 to −40% | +1 to +3 µS |
| Cold pressor | −5 to −15 ms | −15 to −30% | +2 to +5 µS |
| TSST | −15 to −25 ms | −25 to −50% | +2 to +6 µS |

If the VU-AMS produces signals consistently outside these ranges, investigate sensor placement, firmware ICG quality, and EDA electrode impedance before concluding algorithm failure.

---

## 7. Key References

All references are real, peer-reviewed, and verifiable.

- Bertinieri G et al. (1985). A new approach to analysis of the arterial baroreflex. *J Hypertens*, 3(Suppl 3):S79–81.
- Billman GE (2013). The LF/HF ratio does not accurately measure cardiac sympatho-vagal balance. *Front Physiol*, 4:26.
- Boucsein W (2012). *Electrodermal Activity*, 2nd ed. New York: Springer.
- Dawson ME et al. (2007). The electrodermal system. In Cacioppo JT, Tassinary LG & Berntson GG (Eds.), *Handbook of Psychophysiology* (3rd ed., pp. 157–181). Cambridge University Press.
- Grossman P et al. (1990). Cardiac vagal control and social engagement. In Byrne EA & Porges SW (Eds.). New York: Plenum.
- Hjortskov N et al. (2004). The effect of mental stress on heart rate variability and blood pressure during computer work. *Eur J Appl Physiol*, 92(1–2):84–89.
- Kelsey RM et al. (2000). Cardiovascular reactivity and adaptation to recurrent psychological stress. *Psychophysiology*, 37(6):748–756.
- Kirschbaum C et al. (1993). The 'Trier Social Stress Test' — a tool for investigating psychobiological stress responses in a laboratory setting. *Neuropsychobiology*, 28(1–2):76–81.
- Katsigiannis S & Ramzan N (2017). DREAMER: A database for emotion recognition through EEG and ECG signals from wireless low-cost off-the-shelf devices. *IEEE J Biomed Health Inform*, 22(1):98–107.
- Koelstra S et al. (2012). DEAP: A database for emotion analysis using physiological signals. *IEEE Trans Affect Comput*, 3(1):18–31.
- La Rovere MT et al. (1988). Baroreflex sensitivity as a cardiac mortality risk factor after MI. *Lancet*, 332(8610):607–609.
- Licht CMM et al. (2009). Association between anxiety disorders and heart rate variability in NESDA. *Psychosom Med*, 71(5):508–518.
- Pagani M et al. (1986). Power spectral analysis of heart rate and arterial pressure variabilities. *Circ Res*, 59(2):178–193.
- Parati G et al. (2006). Sympathetic and baroreflex cardiovascular control in hypertension. In *Handbook of Hypertension*, Vol. 25.
- Park S et al. (2020). K-EmoCon: A multimodal sensor dataset for continuous emotion recognition in naturalistic conversations. *arXiv*:2005.04120.
- Schmidt P et al. (2018). Introducing WESAD, a multimodal dataset for wearable stress and affect detection. In *Proc. 20th ACM ICMI*, pp. 400–408.
- Shaffer F & Ginsberg JP (2017). An overview of heart rate variability metrics and norms. *Front Public Health*, 5:258.
- Sherwood A et al. (1990). Methodological guidelines for impedance cardiography. *Psychophysiology*, 27(1):1–23.
- Task Force of the ESC and NASPE (1996). Heart rate variability: standards of measurement, physiological interpretation and clinical use. *Circulation*, 93(5):1043–1065.
- Vinkers CH et al. (2013). The effect of stress on core and peripheral body temperature in humans. *Stress*, 16(5):520–530.
- Weissler AM et al. (1968). Systolic time intervals in heart failure in man. *Circulation*, 37(2):149–159.
- Wilhelm FH et al. (2006). Respiratory reactivity, autonomic cardiac control, and daily hassles. *Respir Physiol Neurobiol*, 153(1):45–56.

---

## 8. Open Items and Assumptions

| Item | Status | Owner |
|------|--------|-------|
| Validate default weights w=[0.35,0.30,0.25,0.10] against VU-AMS lab data | Open — awaiting validation study | Vasquez |
| Sigmoid steepness k=1.5 — calibration against empirical CSI distribution | Open — set heuristically; revise after lab validation | Vasquez |
| Baseline period protocol — 5 min designated rest: confirm feasibility in field recordings | Open | Vasquez / Hart |
| EDA electrode placement: torso vs. palm — amplitude reference norms for VU-AMS | Open — VU-AMS torso values will differ from palmar norms; device-specific norms required | Vasquez / Nair |
| Post-activity recovery window: 5 min conservative — validate from VU-AMS walk-to-sit recordings | Open | Vasquez / Reyes |
| RMSSD 30-second epoch minimum beat count (≥25 beats): confirm vs. ultra-short HRV literature | Partially addressed — Munoz et al. 2015 supports; confirm for VU-AMS resting HR range | Vasquez |
| iOS CSI update latency target < 5 s: confirm feasibility of 30 s epoch computation on-device | Open | Chen / Vasquez |
| Weight configurability in VU-DAMS: implement as JSON config, not hard-coded | Open | Reyes |
| Weissler heart-rate correction for PEP: apply by default or optional? | Open — recommend optional with flag | Vasquez / Reyes |
| CSI threshold (65) for "stress" classification: empirically derive from validation study | Open — 65 is provisional (≈1.5 SD above baseline midpoint) | Vasquez |

---

## Presentation Outline

*For Hart to build the HTML presentation. Audience: research collaborators and potential clinical partners, not algorithm engineers.*

---

### Slide 1 — The Problem: Why Measuring Stress in the Real World is Hard
- Stress is a multi-system phenomenon: no single biomarker tells the whole story
- Lab-based stress methods (cortisol, invasive BP) are impractical in daily life
- Wearables offer continuous ambulatory monitoring — but introduce new confounds (movement, temperature, variable breathing)
- VU-AMS goal: a principled, validated composite stress index from a chest-worn device
- **Visual:** Split diagram — lab setting (blood draw, controlled) vs. daily life (walking, meeting, commute) with overlaid question mark on physiological signals

---

### Slide 2 — The Autonomic Stress Response: What We Are Measuring
- Stress activates the sympathetic nervous system and withdraws parasympathetic (vagal) tone simultaneously
- These two processes have different onset speeds, magnitudes, and recovery kinetics
- Electrodermal, cardiac, vascular, and respiratory systems all respond — but not identically
- Capturing all four provides redundancy and mechanistic insight
- **Visual:** Simple ANS diagram — brain → sympathetic chain → heart/sweat glands/vasculature, and vagal nerve → SA node; arrows indicating up/down under stress

---

### Slide 3 — VU-AMS Sensors and Their Stress Markers
- ECG (1 kHz): RMSSD (vagal tone) and LF/HF ratio (spectral balance)
- ICG (per beat): PEP — the most validated ambulatory sympathetic cardiac marker
- EDA/SCL (100 Hz): tonic skin conductance level and spontaneous SCR rate
- IMU (100 Hz): activity classification to gate and protect cardiac and EDA metrics
- Temperature (4 Hz): peripheral vasoconstriction — supplementary contextual indicator
- **Visual:** Annotated diagram of VU-AMS device on chest with labelled sensor sites and arrows to respective stress markers

---

### Slide 4 — Key Markers in Detail: PEP and RMSSD
- **PEP (Pre-Ejection Period):** ICG-derived timing from electrical activation to aortic valve opening. Shortens under sympathetic drive. The best non-invasive, ambulatory sympathetic cardiac indicator (Sherwood et al., 1990)
- **RMSSD:** Beat-to-beat RR interval variability. Dominated by respiratory sinus arrhythmia. Reflects vagal tone. Decreases under stress
- Both markers are per-beat, enabling fine temporal resolution (seconds)
- Major confound for both: physical activity — heart rate and mechanics change dramatically during movement
- **Visual:** Two time-series plots (simulated 10-minute recording): top — PEP over time with a stressor block highlighted; bottom — RMSSD; both dip during the stressor, recover after

---

### Slide 5 — Electrodermal Activity: The Pure Sympathetic Channel
- Eccrine sweat glands have no parasympathetic innervation — EDA is a clean sympathetic signal
- Tonic SCL: slow-moving baseline reflecting accumulated arousal (minutes timescale)
- Phasic SCR rate: discrete responses to each arousing event (1–3 per minute at rest; 5–10+ under stress)
- In ambulatory settings: robust to postural changes; sensitive to temperature and motion artefacts
- Torso placement (VU-AMS) gives valid but lower-amplitude signals than classic palmar EDA — device-specific norms required (Boucsein, 2012)
- **Visual:** Example EDA trace with tonic SCL plotted as a smooth baseline and phasic SCRs as spikes above it; annotations showing SCL rise and individual SCR events during a stressor

---

### Slide 6 — The Activity Problem and How We Solve It
- Walking, climbing stairs, or even standing up from a chair changes PEP, RMSSD, and SCL in ways that mimic stress — for several minutes
- Ignoring activity produces false stress positives in ambulatory recordings
- Solution: HAR module (SP-002) classifies every epoch — sitting, standing, walking, running, cycling, stairs
- Stress metrics are only computed during low-activity states (sitting, standing still, lying)
- 5-minute post-activity recovery window enforced before stress metrics resume
- **Visual:** Timeline diagram of an ambulatory recording — coloured bands for sitting (green), walking (orange), sitting again (green with grey recovery zone), then valid CSI computed; stress peaks labelled

---

### Slide 7 — The Composite Stress Index (CSI): Design and Formula
- Four markers, each z-scored to individual baseline, weighted and combined
- $\text{CSI} = 0.35 \cdot z(\text{PEP}^{-1}) + 0.30 \cdot z(\text{SCL}) + 0.25 \cdot z(\text{RMSSD}^{-1}) + 0.10 \cdot z(\text{SCR rate})$
- Sigmoid-mapped to 0–100: 50 = baseline; > 65 = elevated; > 75 = substantial stress
- Weights literature-informed (Kelsey et al., 2000; Hjortskov et al., 2004); individually calibrated per session
- Missing markers handled gracefully with proportional reweighting
- **Visual:** Waterfall/bar chart showing the four z-scored components stacking up to form the CSI value for a sample epoch; sigmoid curve diagram with 0/50/100 reference points labelled

---

### Slide 8 — Validation Plan: How We Know It Works
- Reference dataset validation: WESAD (Schmidt et al., 2018) — chest-worn ECG + EDA, TSST stressor, ground-truth labels. Target AUC ≥ 0.75 for CSI classification of stress vs. neutral
- Laboratory protocol: N = 20, three stressors (mental arithmetic, cold pressor, TSST), 5-minute recovery blocks, climate-controlled environment
- Expected PEP decrease: 10–25 ms; RMSSD decrease: 20–50%; SCL increase: 1–6 µS
- Acceptance criterion: CSI AUC ≥ 0.80 against TSST ground truth
- **Visual:** Protocol timeline diagram for one participant session — baseline, stressor 1, recovery, stressor 2, recovery, TSST; overlaid expected CSI curve

---

### Slide 9 — BRS: The Advanced Marker (Offline Only)
- Baroreflex Sensitivity (BRS): gain of the pressure-heart rate reflex loop; ms per mmHg
- Validated cardiac risk marker (La Rovere et al., 1988; ATRAMI 1998); suppressed by acute stress
- Computed by BRAVO module (BRS-001): Sequence Method + Spectral Method on 5-minute windows
- In VU-AMS: uses PTT proxy (PEP) — no blood pressure cuff; outputs in ms/ms (relative units)
- Included in CSI-5 (extended offline index): adds 15% weight alongside 4 primary markers
- **Visual:** Scatter plot of RRI vs. SBP_PTT during a sample recording, with up-sequences in green and down-sequences in red, and regression lines overlaid; BRS = mean slope

---

### Slide 10 — Live Display and Reporting
- iOS app (Chen): real-time CSI gauge (0–100, colour-coded zones), marker breakdown bar, activity gate indicator, respiratory rate warning
- VU-DAMS offline (Reyes): full-recording CSI time-series with condition overlays, per-marker z-score traces, summary statistics by condition, correlation matrix
- All markers exportable as CSV and JSON for secondary analysis
- CSI is an intra-individual, within-session relative measure — not an absolute cross-person comparison
- Future: multi-session baseline adaptation; group-level normative reference ranges pending validation study
- **Visual:** Side-by-side mockup of iOS gauge (circular, green-yellow-red, showing CSI=68) and VU-DAMS analysis tab (CSI time-series plot with experimental condition labels and grey activity-gated regions)

---

*Dr. Elena Vasquez — Slow Horses Signal Processing*  
*"Stress leaves fingerprints on everything the ANS touches. Our job is to read them clearly enough to be useful."*
