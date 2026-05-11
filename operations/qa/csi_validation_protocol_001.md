# VU-AMS Composite Stress Index — Laboratory Validation Protocol

**Document ID:** CSI-VAL-001  
**Author:** Marcus Bell — QA & Test Engineering  
**Date:** 2026-05-11  
**Status:** DRAFT v1.0  
**Audience:** Vasquez (validation lead), Reyes (offline analysis), Chen (iOS live display), Hart (field coordination)  

---

## 1. Objective

Validate that the VU-AMS Composite Stress Index (CSI) — a 4-marker composite of pre-ejection period (PEP), root mean square of successive RR intervals (RMSSD), tonic skin conductance level (SCL), and skin conductance response (SCR) rate — **correctly discriminates between baseline rest and standardised psychological stressors** in a laboratory setting. This protocol operationalises the validation plan outlined in CSI-001 Section 6.2.

**Acceptance criteria:** CSI-001 Section 6.3.

---

## 2. Study Design

### 2.1 Participants

- **N = 20** healthy adults (power buffer: target efficacy requires N ≥ 15 for α = 0.05, power = 0.80, expected d = 0.8; N = 20 adds 33% buffer for signal quality exclusions and attrition)
- **Age:** 18–45 years
- **Inclusion criteria:**
  - No reported history of cardiovascular disease, diabetes, or neurological condition
  - No psychiatric diagnosis (GAF score > 60, or negative screening on PHQ-9 score < 5 and GAD-7 score < 5)
  - Resting heart rate 50–100 bpm at screening
  - Resting systolic BP < 140 mmHg, diastolic < 90 mmHg
- **Exclusion criteria:**
  - Current medication: beta-blockers, anticholinergic agents, stimulants (methylphenidate, amphetamines), or any autonomic-active drug
  - Smoking (current or within 6 months)
  - BMI > 35 kg/m²
  - Caffeine consumption > 300 mg within 2 hours of session start
  - Sleep deprivation (self-reported < 6 hours previous night)
  - Pregnancy or breastfeeding (self-reported)

**Recruitment:** University or community volunteer database with informed consent obtained for all procedures per institutional ethics approval (IRB reference: [pending]).

### 2.2 Study Design

**Within-subjects, repeated-measures design.** Each participant undergoes a single session containing:

- 1 × baseline rest condition (5 min)
- 3 × stressor conditions in counterbalanced order (5 min mental arithmetic, 3–5 min cold pressor, 15 min TSST)
- Recovery periods between conditions (10 min)
- Total session duration: ~90 minutes

**Counterbalancing:** Stressor order randomised across participants using Latin square design (6 possible orderings) to control for fatigue and order effects.

### 2.3 Environment

- **Laboratory setting:** Climate-controlled room maintained at 22 ± 1°C and 40–60% relative humidity
- **Ambient lighting:** Indirect, non-flickering (≥ 500 lux; fluorescent or LED preferred to avoid 60 Hz contamination of ECG)
- **Acoustic environment:** Quiet (< 60 dB ambient noise); door closed, external noise minimised
- **Subject positioning:** Seated in comfortable chair with arm support throughout (except during cold pressor, where arm is extended for ice water bath)
- **Observation setup (TSST):** Two experimenters seated ~1.5 m in front of subject with neutral expression

---

## 3. Experimental Conditions

### 3.1 Baseline Rest (5 minutes)

**Objective:** Establish individual within-session baseline for z-score normalisation (CSI-001 Section 4.3).

**Protocol:**
1. Subject seated, eyes open, in relaxed posture
2. Experimenter silent; no interaction or tasks
3. Subject instructed: *"Please sit quietly and relax. You will hear no more instructions. Try to keep your eyes open and remain as still as possible."*
4. Duration: exactly 5 minutes (300 s)
5. Data: all A-, I-, S-, T-blocks recorded; no ECG/EDA motion artefacts expected (low activity, IMU `a_RMS < 0.1g`)

**Signal expectations:** 
- PEP: ~90–110 ms (individual baseline)
- RMSSD: ~40–80 ms (resting parasympathetic tone)
- SCL: 1–5 µS (resting electrodermal level)
- SCR rate: 0–2 events/min (spontaneous fluctuations only)

### 3.2 Stressor A — Mental Arithmetic (5 minutes)

**Objective:** Mild-to-moderate cognitive stress, sympathetic cardiac and electrodermal activation.

**Reference:** Hjemdahl et al. (1984); Sherwood et al. (1990).

**Protocol:**
1. Subject remains seated
2. Experimenter presents a 4-digit starting number (e.g., 1022, 3847)
3. Subject instructed: *"Starting from [number], please subtract [7 or 13] out loud as quickly and accurately as possible. I will correct errors and prompt you to continue."*
4. Subtraction interval: 7s or 13s (alternating to increase difficulty; e.g., first 30 s by 7, next 30 s by 13, repeat)
5. Experimenter corrects errors immediately: *"That's not correct, try again"* (neutral tone, no emotional inflection)
6. If subject falls silent for > 5 s, experimenter prompts: *"Please continue."*
7. Duration: exactly 5 minutes (300 s); no break
8. Data: all blocks recorded

**Signal expectations:**
- PEP: decrease of 10–20 ms from baseline (sympathetic cardiac acceleration)
- RMSSD: decrease of 20–40% from baseline (vagal withdrawal)
- SCL: increase of 1–3 µS from baseline (mild sudomotor activation)
- SCR rate: increase to 2–5 events/min

**Task difficulty calibration:** Actual subtraction accuracy is not graded; the cognitive load and social pressure (experimenter present, watching) drive the stress response.

### 3.3 Stressor B — Cold Pressor Test (CPT; up to 5 minutes, subject-terminated)

**Objective:** Acute pain and sympathetic stress; reference stressor for validation against classical psychophysiology literature.

**Reference:** Kövári et al. (2015); Hines-Brown technique.

**Protocol:**
1. Subject remains seated; experimenter brings a thermally insulated container filled with ice water (4°C ± 0.5°C) to the subject
2. Subject's non-dominant hand is submerged wrist-deep into the container
3. Subject instructed: *"Please put your hand in the water. Try to keep it in as long as you comfortably can, up to 3 minutes. You can withdraw your hand at any time if it becomes intolerable."*
4. Experimenter monitors hand position and cold tolerance; notes withdrawal time (if < 180 s)
5. If subject does not withdraw by 180 s, experimenter states: *"You may remove your hand now,"* and assists with withdrawal
6. Subject's hand dried with towel; subject seated with arm resting on chair support
7. Duration: Up to 300 s maximum (or subject-terminated earlier)
8. Data: all blocks recorded during submersion and 60 s recovery post-submersion

**Signal expectations:**
- PEP: decrease of 5–15 ms from baseline (acute sympathetic drive)
- RMSSD: decrease of 15–30% from baseline (reflex vagal withdrawal + pain)
- SCL: increase of 2–5 µS from baseline (strong sympathetic response to pain)
- SCR rate: increase to 3–8 events/min (phasic responses to pain pulses)
- Skin temperature: initial decrease during immersion (vasoconstriction); rebound increase post-withdrawal

**Cold tolerance note:** Withdrawal time is recorded for secondary analysis (correlate cold tolerance with individual symptom severity or trait neuroticism from exit questionnaire); do not use as criterion for data inclusion/exclusion.

### 3.4 Stressor C — Trier Social Stress Test (TSST, abbreviated form; 15 minutes total)

**Objective:** Ecologically valid, multi-component stressor combining anticipatory anxiety, cognitive load, and social evaluation. Standard protocol; primary validation condition.

**Reference:** Kirschbaum et al. (1993); Kudielka et al. (2007).

**Protocol overview:**

| Phase | Duration | Instructions & Content |
|-------|----------|------------------------|
| **Preparation** | 5 min | Subject sits alone in preparation room; told they will give a public speech (topic: "Why are you a good job candidate?") in front of two evaluators. Subject may prepare notes but will not be allowed to use them during speech. Evaluators will use standardised assessment forms. |
| **Speech (free talk)** | 5 min | Subject enters evaluation room with two experimenters seated behind a desk. Brief statement of task (< 30 s). Subject then gives uninterrupted 5-minute speech on the assigned topic. Experimenters maintain neutral expression (no nodding, smiling, or verbal feedback). |
| **Mental arithmetic** | 5 min | After speech, subject performs rapid serial subtraction (by 13 from 2083, spoken aloud) in front of evaluators. Experimenter corrects errors immediately: "That is incorrect." No verbal encouragement. |

**Setup:**
- Two trained experimenters (neutral gender pairing; both present throughout speech and arithmetic phases)
- Video camera (visible to subject) recording the session to emphasize evaluation
- Subject stands throughout the speech and arithmetic phases (though may sit during preparation)
- Experimenters use standardised assessment rubric (Trier TSST form V2.0 or equivalent) to evaluate content and delivery; form visible to subject if possible

**Signal expectations:**
- PEP: decrease of 15–25 ms from baseline (strongest sympathetic response of all three stressors)
- RMSSD: decrease of 25–50% from baseline (sustained vagal withdrawal + anticipatory anxiety)
- SCL: increase of 2–6 µS from baseline (persistent elevated tonic level)
- SCR rate: increase to 3–10 events/min (frequent phasic responses)
- Heart rate: increase of 20–40 bpm from baseline

---

## 4. Recovery Periods

**Between each condition (Stressor A → Recovery 1 → Stressor B → Recovery 2 → Stressor C):**

- Duration: 10 minutes
- Subject remains seated, silent, at rest
- Experimenter leaves the room (no interaction)
- Instruction (single statement at start of recovery): *"Please remain seated and relax. I will return in 10 minutes."*
- Data: all blocks recorded; used to assess return-to-baseline kinetics (secondary outcome)

**Post-session recovery (after Stressor C):**

- Final 5 minutes: subject remains in lab, seated at rest
- Allows recovery to baseline to confirm data integrity
- Used to verify no sustained physiological elevation or equipment failure

---

## 5. Equipment Checklist

### 5.1 Hardware

| Item | Specification | Qty | Rationale |
|------|---------------|-----|-----------|
| **VU-AMS Device** | ESP32-S3-MINI-1-N8R8, latest firmware revision (v2.x+) | 1 | Primary recording device |
| **iPhone / iPad** | iOS 17+ (iPhone 15 or later preferred for BLE 5.3) | 1 | Live CSI display + BLE relay |
| **Chest Electrodes** | VU-AMS standard 7-electrode set (Ag/AgCl, pre-gelled, 35 mm diameter; standard or paediatric based on torso size) | 1 per participant | ECG and ICG acquisition |
| **Hand Electrodes** | EDA (SCL) electrode pair: Ag/AgCl, 8 mm diameter, placed on thenar and hypothenar eminence of non-dominant hand | 1 per participant | Tonic and phasic EDA |
| **Ice Bath Container** | Thermally insulated (foam or double-walled stainless steel), capacity ≥ 10 L | 1 | Cold pressor test; contains 4°C ice water |
| **Thermometer (Water)** | Digital or glass mercury, range 0–100°C, ± 0.1°C accuracy | 1 | Monitor ice water temperature (target 4 ± 0.5°C) |
| **Thermometer/Hygrometer (Room)** | Digital, 0–50°C and 0–100% RH, display within view of experimenter | 1 | Log ambient conditions every 10 minutes |
| **Digital Timer** | Visible to experimenter; alarm function | 1 | Precisely time all 5-minute and 10-minute blocks |
| **Video Camera** | Consumer or lab-grade (1080p minimum, 30 fps); tripod mounted; visible to subject during TSST | 1 | Record TSST speech for post-hoc analysis and experimenter fidelity checks |
| **Polar H10 Chest Strap** | Bluetooth HR/HRV sensor; ECG contact electrodes | 1 | Gold-standard HR/HRV reference for validation against CSI markers |
| **Shimmer3 GSR+ Sensor** | Wearable EDA sensor (forehead or wrist placement); synced via Bluetooth to data logging PC | 1 | Gold-standard EDA (SCL/SCR) reference |
| **Pulse Oximeter (optional)** | Finger clip, SpO₂ and pulse rate readout | 1 | Monitor for hypoxia during cold pressor (safety) |

### 5.2 Software & Data Management

| Item | Purpose |
|------|---------|
| **VU-DAMS (offline analysis)** | Post-session block parsing, signal processing (SP-001, BRS-001), CSI computation |
| **iOS VU-AMS app** | Live CSI display + BLE block relay; in-app data logging of received block timestamps |
| **Polar H10 app** (or generic BLE central) | Paired with H10 chest strap; exports RRI/RMSSD reference data |
| **Shimmer3 Consensys** (or generic BLE central) | Paired with Shimmer3 GSR+; exports EDA reference data |
| **Video recording software** (optional) | Capture TSST speech on lab PC; time-sync with physiological data via LED/beep cue at session start |
| **Lab data logger spreadsheet** (Excel/CSV) | Record participant ID, date, stressor order, electrode impedances, observer notes, ambient conditions at 10-min intervals, cold pressor withdrawal time, exit questionnaire scores |

---

## 6. Electrode Placement Protocol

Electrode placement directly affects signal quality. Follow the VU-AMS 7-electrode topology exactly.

### 6.1 ECG/ICG Electrode Positions (Chest)

**Prerequisite:** Subject shirtless; chest area cleaned with alcohol wipe and allowed to air dry (~30 s) to remove oils and improve adhesion.

| Electrode | Anatomical Landmark | Position | Purpose |
|-----------|-------------------|----------|---------|
| **E1** | Midline, manubrium | At the manubrium (upper sternum, level of 2nd rib) | ECG reference (white wire) |
| **E2** | Midline, 2nd ICS | At the 2nd intercostal space (just below 2nd rib), midline | ECG positive (red wire) |
| **E3** | Anterior axillary line, 4th ICS | 4th intercostal space, at the anterior axillary line, right side | ICG outer electrode (blue wire — axial) |
| **E4** | Midline, xiphoid process level | At or just above the xiphoid process (~T9 level) | ICG outer electrode (green wire — axial) |
| **E5** | Anterior axillary line, 5th ICS | 5th intercostal space, anterior axillary line, right side | ICG inner electrode (yellow wire — radial, closer to heart) |
| **E6** | Midline, xiphoid-to-umbilicus | Midway between xiphoid and umbilicus (~T10 level) | ICG inner electrode (orange wire — radial, closer to heart) |
| **E7** | Left ankle/lateral calf | Medial malleolus or lateral calf, left side | ECG ground reference (black wire) |

**Attachment method:**
1. Peel backing from pre-gelled electrode; press firmly onto skin with thumb (uniform pressure, no air bubbles)
2. Verify electrode is in full contact; repeat for all 7 electrodes
3. Attach lead wires: match colour to each electrode (red to E2, black to E7, etc.)
4. Secure lead wires with athletic tape (non-adhesive tape over wire bundle to chest, away from electrodes) to prevent accidental pull

**Quality check:** Impedance measurement performed by VU-AMS firmware upon boot or on demand:
- Target: ICG impedance Z0 = 20–150 Ω (reported in I-block)
- If Z0 out of range: re-clean electrode sites; may indicate dry skin or poor adhesion
- ECG impedance not directly reported but expected < 5 kΩ per electrode pair

### 6.2 EDA Electrodes (Hand)

**Prerequisite:** Non-dominant hand; subject's fingers relaxed, palm facing up.

| Electrode | Position | Purpose |
|-----------|----------|---------|
| **EDA1** | Thenar eminence (ball of thumb) | EDA electrode (positive) |
| **EDA2** | Hypothenar eminence (ball of little finger) | EDA electrode (negative/reference) |

**Attachment method:**
1. Clean thenar and hypothenar surfaces with alcohol wipe; air dry
2. Apply small amount of electrode gel to each 8 mm pre-gelled electrode
3. Press gently but firmly onto each eminence
4. Attach lead wires (typically white and black); secure with athletic tape around wrist to prevent accidental pull

**Stability:** Ask subject to rest non-dominant arm on armrest during stressors; prevents hand movement during cold pressor (subject uses dominant hand for ice water bath).

---

## 7. Data Collection Procedure

### 7.1 Pre-Session (15 minutes before start)

1. **Consent & screening:** Review informed consent; confirm exclusion criteria (medications, caffeine, sleep, BMI) verbally
2. **Vital signs:** Measure resting heart rate (1 minute, seated), systolic/diastolic BP (cuff, non-dominant arm, seated)
   - If HR > 100 bpm or BP > 140/90 mmHg: note in lab log; proceed unless clinically contraindicated
3. **Questionnaires:** Administer baseline trait measures:
   - PHQ-9 (depression screening)
   - GAD-7 (anxiety screening)
   - State-Trait Anxiety Inventory (STAI) — state form only
4. **Randomise stressor order:** Draw sealed envelope containing counterbalanced stressor order (Latin square)
5. **Sensor attachment:** Apply all 7 chest electrodes + 2 hand EDA electrodes; verify impedance

### 7.2 Session Start (T = 0 min)

1. **Position subject:** Seated in comfortable chair, feet flat on floor, arms resting on armrests
2. **Start VU-AMS recording:** Send CMD_RECORDING_START; confirm LED indication and iOS app connection
3. **Start reference devices:**
   - Polar H10: confirm pairing and data logging on iOS app or desktop
   - Shimmer3 GSR+: confirm pairing and data logging on desktop BLE central
   - Video camera: start recording (if TSST is included); note start time on metadata
4. **Synchronisation pulse:** Optional but recommended: trigger a bright LED flash at t=0 visible to both VU-AMS and video camera (establishes frame sync for post-hoc analysis)
5. **Note session start time** on lab sheet

### 7.3 Baseline Rest (T = 0 to 5 min)

1. **Experimenter instruction (read once, at T=0):** *"Please sit quietly and relax. You will hear no more instructions until we proceed to the next task. Try to keep your eyes open and remain as still as possible."*
2. **Experimenter action:** Leave room; subject alone
3. **Monitor:** Watch video monitor (optional) for gross motion; log any artifacts (coughing, movement, etc.)
4. **At T=5 min:** Bell/chime signal to experimenter; proceed to next phase

### 7.4 Stressor A — Mental Arithmetic (T = 5 to 10 min)

1. **Experimenter returns; reads task instruction (< 30 s):**  
   *"I would like you to perform a mental arithmetic task. Starting from the number [e.g., 1022], please subtract [7 or 13] out loud as quickly and accurately as possible. I will correct errors and prompt you to continue. Ready? Begin."*
2. **Present starting number** (written on card, shown to subject)
3. **Conduct subtraction:** Monitor compliance; correct errors with neutral tone (*"That's not correct, try again"*); prompt after 5-second silence (*"Please continue"*)
4. **At T=10 min:** End task; *"Thank you. The task is now complete."*

### 7.5 Recovery 1 (T = 10 to 20 min)

1. **Experimenter instruction:** *"Please remain seated and relax. I will return in 10 minutes."*
2. **Experimenter leaves room**
3. **Monitor:** As in baseline
4. **At T=20 min:** Proceed to next stressor

### 7.6 Stressor B — Cold Pressor Test (T = 20 to 25+ min)

1. **Experimenter returns with ice bath container**
2. **Read task instruction (< 30 s):** *"For this next task, I would like you to submerge your [left/right] hand into this cold water. Try to keep it in as long as you comfortably can, up to 3 minutes. You can withdraw your hand at any time if it becomes intolerable. Are you ready?"*
3. **Ensure water temperature is 4 ± 0.5°C** (check with thermometer immediately before task)
4. **Subject submerges hand** (wrist-deep)
5. **Monitor hand position and withdrawal time:**
   - If subject withdraws before 180 s: note time (e.g., "withdrew at 132 s")
   - If subject tolerates 180 s: at T = +180 s, state: *"You may remove your hand now."*
6. **Post-withdrawal:** Dry subject's hand with clean towel; subject rests with arm on armrest
7. **Continue recording** for 60 seconds post-withdrawal (recovery kinetics)
8. **At T = 25 min (or withdrawal time + 60 s):** Proceed to Recovery 2

### 7.7 Recovery 2 (T = 25 to 35 min)

1. **Experimenter instruction & action:** As in Recovery 1
2. **At T=35 min:** Proceed to Stressor C (TSST)

### 7.8 Stressor C — Trier Social Stress Test (T = 35 to 65 min)

#### Preparation Phase (T = 35 to 40 min)

1. **Experimenter escorts subject to separate "preparation room"** (different room or screened-off area)
2. **Read instruction (read verbatim from script):**
   *"You will be asked to give a speech about why you are a good job candidate. The topic is: 'Why would you be a good job candidate?' You will give this speech in front of two evaluators in the next room. You have 5 minutes to prepare. You may use a pen and paper to make notes, but you will NOT be allowed to use your notes during the speech. The evaluators will use standardised assessment forms to evaluate your speech. They will judge the content of your speech and your delivery. Try to be as convincing as possible."*
3. **Provide paper and pen** (optional; subject may not use notes during speech)
4. **Leave subject alone for exactly 5 minutes**
5. **Monitor time precisely** on visible timer

#### Speech Phase (T = 40 to 45 min)

1. **Experimenter escorts subject back to main lab** where two evaluators are seated behind a desk
2. **Brief statement (< 30 s):** *"We are now ready for your speech. You have 5 minutes to speak about why you would be a good job candidate. You may not use your notes. Begin when you are ready."*
3. **Subject stands (or sits, depending on lab setup) and delivers uninterrupted 5-minute speech**
4. **Evaluators maintain neutral expression:** No nodding, smiling, or verbal feedback; visible note-taking on assessment form
5. **Experimenter(s) remain silent and neutral**
6. **Video camera records subject's face and upper body** (optional but recommended for fidelity checks)
7. **At T = 45 min:** *"Thank you. That completes the speech portion."*

#### Arithmetic Phase (T = 45 to 50 min)

1. **State task (< 30 s):** *"For the next 5 minutes, I would like you to perform a rapid serial subtraction task. Starting from 2083, please subtract 13 and continue out loud as quickly and accurately as possible. If you make an error, I will tell you and you must try again. Ready? Begin."*
2. **Conduct subtraction:** As in Stressor A (Mental Arithmetic), but now in front of evaluators
3. **Corrective feedback (neutral tone, no encouragement):** *"That is not correct."*
4. **At T=50 min:** *"Thank you. The task is now complete."*

#### TSST Debrief (T = 50 to 55 min)

1. **Subject returns to preparation room (or separate area)**
2. **Experimenter provides structured debrief:** Reassure subject that this was a laboratory stressor and not a real evaluation; thank subject for participation
3. **Provide time for questions**
4. **Administer post-TSST questionnaire:** STAI state form (post-stressor anxiety) + custom 5-item stress perception rating (0–10 Likert scale for mental, physical, and social stress during each component)

### 7.9 Recovery 3 (T = 55 to 65 min)

1. **Experimenter instruction:** *"Please return to the main room and remain seated and relax for the final 10 minutes of the session."*
2. **Subject seated at rest; experimenter leaves room**
3. **Continue recording**

### 7.10 Session End (T = 65 min)

1. **Send CMD_RECORDING_STOP** to VU-AMS
2. **Stop reference devices:** Polar H10 and Shimmer3 GSR+ logging
3. **Stop video camera**
4. **Remove all electrodes carefully**; inspect skin for any irritation
5. **Administer exit questionnaire:**
   - PHQ-9 and GAD-7 (if baseline was significantly elevated, re-check post-session)
   - Cold tolerance scale (0–10: how tolerable was the CPT?)
   - Perceived stress during each stressor (0–10 Likert scale, repeated for mental arithmetic, CPT, TSST)
   - Cardiac symptom awareness (did you notice your heart racing? 0–10)
   - Open-ended feedback: "Which stressor felt most stressful? Why?"
6. **Vitals (repeat):** HR and BP, seated (confirm return toward baseline)
7. **Thank subject; provide honorarium** (if applicable)

---

## 8. CSI Scoring and Analysis

### 8.1 Data Export and Preprocessing

**On the VU-AMS device:**
- Power off after CMD_RECORDING_STOP completes
- Remove SD card; mount on lab PC

**Block parsing:**
- Run VU-DAMS parser on the SD card recording file
- Verify all blocks parse without CRC errors; reject file if CRC error rate > 0.1%
- Export per-epoch summary CSV with all A-, I-, S-, T-block fields (timestamps, ECG, ICG, EDA, temperature, IMU)

**Reference data:**
- Export Polar H10 RRI series and RMSSD (30-second epoch averages) from H10 app or BLE central log
- Export Shimmer3 GSR+ tonic SCL and SCR events from Consensys or custom BLE central script

### 8.2 Baseline Period Identification and Z-Score Computation

1. **Identify baseline rest epoch:** T = 0 to 5 min (first 300 s of recording)
2. **Require all 300 s to be low-activity (HAR state = SITTING or STANDING_STILL; IMU `a_RMS < 0.1g`) and electrode contact good** (EDA `electrode_contact == 1`, ICG Z0 in range)
3. **If baseline period corrupted (motion, electrode loss):** Use entire first 10 minutes of low-activity data as fallback baseline; flag as `baseline_inferred = true`
4. **Compute per-marker means and SDs over baseline period:**
   - $\mu_{\text{PEP}} = \text{mean}(\text{PEP per beat, baseline period})$
   - $\sigma_{\text{PEP}} = \text{std}(\text{PEP per beat, baseline period})$
   - $\mu_{\text{RMSSD}} = \text{mean}(\text{RMSSD per 30-s epoch, baseline period})$
   - $\sigma_{\text{RMSSD}} = \text{std}(\text{RMSSD per 30-s epoch, baseline period})$
   - $\mu_{\text{SCL}} = \text{mean}(\text{SCL per 30-s epoch, baseline period})$
   - $\sigma_{\text{SCL}} = \text{std}(\text{SCL per 30-s epoch, baseline period})$
   - $\mu_{\text{SCR\_rate}} = \text{mean}(\text{SCR rate per 60-s epoch, baseline period})$
   - $\sigma_{\text{SCR\_rate}} = \text{std}(\text{SCR rate per 60-s epoch, baseline period})$

### 8.3 Per-Epoch CSI Computation

**For each 30-second analysis window across the entire recording:**

1. **Compute per-marker values:**
   - $\text{PEP}_t = $ mean of all PEP values (per-beat) in the 30-s window
   - $\text{RMSSD}_t = $ RMSSD computed over the 30-s window (beat-to-beat variability)
   - $\text{SCL}_t = $ mean of S-block `scl_tonic_uS` field in the 30-s window
   - $\text{SCR\_rate}_t = $ count of SCR events in a 60-s window centred on the 30-s epoch, divided by 1 minute

2. **Invert markers with negative stress direction (PEP, RMSSD):**
   - $\text{PEP}^{-1}_t = 1000 / \text{PEP}_t$ (dimensionless, scaled × 1000)
   - $\text{RMSSD}^{-1}_t = 1000 / \text{RMSSD}_t$

3. **Compute z-scores:**
   - $z_{\text{PEP}}(t) = \frac{\text{PEP}^{-1}_t - \mu_{\text{PEP}^{-1}}}{\sigma_{\text{PEP}^{-1}}}$
   - $z_{\text{RMSSD}}(t) = \frac{\text{RMSSD}^{-1}_t - \mu_{\text{RMSSD}^{-1}}}{\sigma_{\text{RMSSD}^{-1}}}$
   - $z_{\text{SCL}}(t) = \frac{\text{SCL}_t - \mu_{\text{SCL}}}{\sigma_{\text{SCL}}}$
   - $z_{\text{SCR\_rate}}(t) = \frac{\text{SCR\_rate}_t - \mu_{\text{SCR\_rate}}}{\sigma_{\text{SCR\_rate}}}$

4. **Clamp z-scores to [−3, +3]:**
   - $z_i^{\text{clamp}}(t) = \max(-3, \min(3, z_i(t)))$ for each marker $i$

5. **Check activity gating:** If HAR state indicates walking/running/cycling or IMU `a_RMS > 0.15g`, set `csi_activity_gated = true` and do NOT compute CSI (record NaN)

6. **Check post-activity recovery window:** If current epoch is within 5 min after any high-activity epoch, set `post_activity_recovery = true` and do NOT compute CSI (record NaN)

7. **Compute raw CSI:**
   $$\text{CSI}_{\text{raw}}(t) = 0.35 \cdot z_{\text{PEP}}^{\text{clamp}}(t) + 0.30 \cdot z_{\text{SCL}}^{\text{clamp}}(t) + 0.25 \cdot z_{\text{RMSSD}}^{\text{clamp}}(t) + 0.10 \cdot z_{\text{SCR\_rate}}^{\text{clamp}}(t)$$

8. **Handle missing markers:** If any marker is unavailable in an epoch (e.g., EDA electrode contact lost), reweight remaining markers:
   $$w_i' = \frac{w_i \cdot \mathbf{1}[\text{marker } i \text{ available}]}{\sum_j w_j \cdot \mathbf{1}[\text{marker } j \text{ available}]}$$
   Recompute CSI with $w'$ weights. Flag with `n_markers_available` (0–4).

9. **Map to [0, 100] via sigmoid:**
   $$\text{CSI}(t) = \frac{100}{1 + e^{-1.5 \cdot \text{CSI}_{\text{raw}}(t)}}$$

10. **Export all per-epoch values:** Write CSV with columns:
    - `timestamp_s` (seconds from recording start)
    - `epoch` (running count)
    - `stressor_phase` (BASELINE, MATH, RECOVERY1, CPT, RECOVERY2, TSST, RECOVERY3)
    - `csi` (final 0–100 index)
    - `pep_inv_z`, `scl_z`, `rmssd_inv_z`, `scr_rate_z` (individual marker z-scores)
    - `n_markers_available` (0–4)
    - `csi_activity_gated` (true/false)
    - `post_activity_recovery` (true/false)
    - `resp_rate_flag` (true if respiratory rate outside 8–22 breaths/min)
    - `baseline_inferred` (true if baseline was inferred)

### 8.4 Per-Marker Analysis

**Primary stress response metrics (each stressor vs. baseline):**

For each stressor (MATH, CPT, TSST) and recovery period:

1. **Extract valid 30-second epochs** (exclude activity-gated and post-activity-recovery epochs; require ≥ 2 markers available)

2. **Paired analysis (stressor vs. baseline):**
   - **PEP:** Compute mean PEP during baseline and during stressor; paired t-test on per-beat values (if N per-beat values within stressor window: test mean baseline vs. mean stressor PEP using two-sample t-test)
   - **RMSSD:** Compute per-epoch RMSSD; paired t-test across 30-s epochs
   - **SCL:** Compute per-epoch mean; paired t-test across 30-s epochs
   - **SCR rate:** Compute per-60-s-epoch rates; paired t-test

3. **Effect size:** Compute Cohen's d for each marker in each stressor vs. baseline

4. **Expected direction:** PEP and RMSSD should DECREASE; SCL and SCR rate should INCREASE

**Report template:**
| Marker | Baseline Mean | Stressor Mean | Δ | p-value | Cohen's d | Direction |
|--------|---------------|---------------|---|---------|-----------|-----------|
| PEP | XX ms | XX ms | ±X ms | X.XXX | X.XX | Expected: ↓ |
| RMSSD | XX ms | XX ms | ±X% | X.XXX | X.XX | Expected: ↓ |
| SCL | X.X µS | X.X µS | +X.X µS | X.XXX | X.XX | Expected: ↑ |
| SCR rate | X.X /min | X.X /min | +X.X /min | X.XXX | X.XX | Expected: ↑ |

### 8.5 CSI Discriminant Validity (ROC Analysis)

**Objective:** Assess whether CSI correctly discriminates stress epochs from rest epochs.

**Procedure:**

1. **Define epoch labels:**
   - **Rest:** All epochs from BASELINE and RECOVERY1/RECOVERY2/RECOVERY3 phases (label = 0)
   - **Stress:** All epochs from MATH, CPT, and TSST phases (label = 1)
   - Exclude: activity-gated and post-activity-recovery epochs

2. **Compute ROC curve:** Vary decision threshold across all CSI values; compute sensitivity (true positive rate) and specificity (true negative rate) at each threshold

3. **Compute Area Under the ROC Curve (AUC):**
   - AUC = 0.5: random classification
   - AUC = 1.0: perfect classification
   - **Target per CSI-001:** AUC ≥ 0.80 for the 4-marker CSI on the VU-AMS validation cohort

4. **Report:** Per-stressor AUC:
   | Stressor | AUC | 95% CI | N epochs (stress) | N epochs (rest) |
   |----------|-----|--------|-------------------|-----------------|
   | MATH | X.XX | [X.XX, X.XX] | NNN | NNN |
   | CPT | X.XX | [X.XX, X.XX] | NNN | NNN |
   | TSST | X.XX | [X.XX, X.XX] | NNN | NNN |
   | **Combined** | X.XX | [X.XX, X.XX] | NNN | NNN |

5. **Optimal CSI threshold:** Identify threshold that maximizes Youden index (sensitivity + specificity − 1) on the combined data

### 8.6 Secondary Outcomes: Recovery Kinetics

For each recovery period (RECOVERY1, RECOVERY2, RECOVERY3):

1. **Identify return-to-baseline time:** Time from end of stressor until CSI returns to within ±5 points of baseline CSI (CSI ≈ 50 ± 5)

2. **Report:** 
   | Recovery | Stressor Ending Time | Return-to-Baseline Time | Time to ±5 CSI | Mean Recovery CSI | Status |
   |----------|----------------------|------------------------|-----------------|-------------------|--------|
   | RECOVERY1 | TT:MM:SS | MM:SS | Expected: < 10 min | | Pass/Fail |

3. **Interpret:** Rapid recovery (< 5 min) suggests effective parasympathetic reactivation; sustained elevation suggests impaired recovery

---

## 9. Acceptance Criteria Table

Extract from CSI-001 Section 6.3, formatted as a clear pass/fail checklist. **To be completed during analysis.**

| Criterion | Threshold | Stressor(s) | Details | Pass / Fail |
|-----------|-----------|-------------|---------|------------|
| **P1 — Critical** | | | | |
| PEP stressor response | Significant decrease, p < 0.05 (paired t-test, N ≥ 15) | MATH | Minimum effect size: d ≥ 0.5 | — |
| | | CPT | Minimum effect size: d ≥ 0.3 | — |
| | | TSST | Minimum effect size: d ≥ 0.5 | — |
| RMSSD stressor response | Significant decrease, p < 0.05 | ≥ 2 of 3 stressors | Minimum effect size: d ≥ 0.3 per stressor | — |
| CSI AUC (ROC, stress vs. rest) | AUC ≥ 0.75 | All stressors combined | Binary classification of stress vs. rest epochs | — |
| Activity gating specificity | 0 false CSI values during known motion epochs | All data | No stress values computed during walking, running, climbing | — |
| CSI < 2 markers | Correctly suppressed | All data | CSI flagged as unreliable if n_markers_available < 2 | — |
| **P2 — Target** | | | | |
| CSI AUC (extended 5-marker, BRS) | AUC ≥ 0.80 | All stressors combined | Offline-only, requires 5-min BRS windows | — |
| PEP stressor magnitude | Within literature ranges | MATH | −10 to −20 ms change expected | — |
| | | CPT | −5 to −15 ms change expected | — |
| | | TSST | −15 to −25 ms change expected | — |
| RMSSD stressor magnitude | Within literature ranges | MATH | −20 to −40% change expected | — |
| | | CPT | −15 to −30% change expected | — |
| | | TSST | −25 to −50% change expected | — |
| SCL stressor magnitude | Within literature ranges | MATH | +1 to +3 µS change expected | — |
| | | CPT | +2 to +5 µS change expected | — |
| | | TSST | +2 to +6 µS change expected | — |
| Recovery kinetics | CSI return to baseline ± 5 within 10 min | All stressors | Rapid recovery indicates intact parasympathetic reactivation | — |
| **P3 — Informational** | | | | |
| Inter-marker correlation | Pearson r > 0.3 among PEP, RMSSD, SCL during stress | All stressors | Check that markers track together (expected) or diverge (confound?) | — |
| Respiratory rate confound | Respiratory rate < 8 or > 22 breaths/min in < 10% of epochs | All data | Hyperventilation or irregular breathing confounds RMSSD; flag epochs | — |

---

## 10. Known Confounds and Controls

### 10.1 Physical Activity

**Risk:** Walking to/from lab, fidgeting, or weight shifting alters PEP, RMSSD, and SCL independent of psychological stress.

**Control:**
- **Activity gating (mandatory):** CSI not computed during epochs with HAR state = WALKING, RUNNING, CYCLING, STAIR_CLIMBING, or IMU `a_RMS > 0.15g`
- **Seated posture throughout:** All stressors conducted with subject seated except TSST speech phase (standing, but brief and controlled)
- **Post-activity recovery window (5 min):** No CSI computed for 5 minutes after any high-activity epoch
- **Verify in data:** Check that all stressor phases (MATH, CPT speech, CPT recovery, TSST) show low activity; if activity flags appear during stressor, investigate and flag as confound

### 10.2 Respiratory Rate

**Risk:** Slow deep breathing (paced respiration) inflates RMSSD artificially; fast breathing suppresses it independent of stress state.

**Control:**
- **Do NOT instruct paced breathing:** Allow subject to breathe naturally throughout session
- **Monitor respiratory rate:** Compute from ECG baseline wander (SP-001 Section 4) or PPG (if available)
- **Flag epochs:** If respiratory rate departs from normal range (12–20 breaths/min), set `resp_rate_flag = true`
- **Report alongside RMSSD:** In summary analysis, always tabulate mean respiratory rate for each condition alongside RMSSD

### 10.3 Electrode Contact

**Risk:** Electrode dry-out, loss of contact, or movement artefacts during stressor reduce signal amplitude and introduce spurious SCL changes.

**Control:**
- **Check before session:** Verify impedance for all 7 chest electrodes (target ICG Z0 = 20–150 Ω) and EDA electrodes
- **Monitor during session:** VU-AMS firmware reports `electrode_contact` flag (0 = lost, 1 = good) in every S-block
- **Exclude contaminated epochs:** Do not compute CSI if `electrode_contact == 0` during an epoch
- **Post-session inspection:** After electrode removal, visually inspect skin for irritation or signs of poor adhesion

### 10.4 Ambient Temperature

**Risk:** Room temperature > 28°C increases SCL via thermoregulatory sweating, confounding stress-induced SCL.

**Control:**
- **Climate control:** Maintain lab at 22 ± 1°C throughout (confirmed by thermometer reading every 10 minutes during session)
- **Log temperature:** Record ambient temperature and humidity at T = 0, 10, 20, 30, 40, 50, 60 minutes
- **Reject session if:** Temperature varies > ±2°C during session or drifts above 25°C (flag as confounded)

### 10.5 Electrode Placement Variability

**Risk:** Different electrode positions across participants or sessions affect absolute PEP and SCL amplitudes.

**Control:**
- **Standardised anatomical landmarks:** Follow Section 6.1 exactly for all participants
- **Impedance-based verification:** Z0 impedance indicates sensor contact quality; within-range Z0 (20–150 Ω) acceptable
- **Use z-score normalisation:** CSI is computed relative to individual within-session baseline, not cross-individual comparison; placement variability cancels out

### 10.6 Ectopic Beats and Artefacts

**Risk:** Premature ventricular contractions (PVCs) or atrial fibrillation create anomalous RR intervals, inflating RMSSD spuriously.

**Control:**
- **Ectopic rejection:** SP-001 Section 2.3 defines ectopic detection (RRI outliers > 2 SD from rolling median); exclude ectopic beats from RMSSD computation
- **ECG quality monitoring:** VU-AMS firmware reports ECG signal quality in A-block; if quality poor, mark epoch as unreliable
- **Manual inspection:** For each participant, visually inspect ECG trace during baseline and stressor periods; flag any obvious arrhythmias

---

## 11. Reporting Template

The validation report will be filed after testing is complete. This section outlines the structure.

### 11.1 Report Header

- **Title:** "VU-AMS Composite Stress Index Validation Study — Final Report"
- **Study ID:** CSI-VAL-001
- **Date:** [Session date range]
- **Lead Investigator:** Marcus Bell (QA & Test Engineering)
- **Scientific Lead:** Dr. Elena Vasquez (Biomedical Signal Processing)
- **Participants:** N = 20 (or final count if < 20 due to exclusions)

### 11.2 Executive Summary (1 page)

State primary findings: CSI AUC, pass/fail on acceptance criteria (P1 and P2), any critical deviations from expected stressor responses.

### 11.3 Methods

- Study design (within-subjects, three stressors, N participants)
- Participant demographics (age, sex, BMI) — table
- Equipment list (VU-AMS firmware version, iOS app version, reference devices)
- Stressor protocols (mental arithmetic script, CPT temperature, TSST evaluation form)
- Data analysis methods (z-score normalisation, CSI formula, statistical tests)

### 11.4 Results

#### 11.4.1 Participant Characteristics & Attrition

- Table: demographics by study arm (all stressor orders)
- Attrition log: number of participants excluded pre-session (failing inclusion/exclusion criteria), during-session (severe adverse event), post-hoc (signal quality failure, electrode contact loss > 50% of session, etc.)
- Final N used in analysis

#### 11.4.2 Stressor Response: Per-Marker Analysis

- **Table 1:** Baseline vs. stressor means for PEP, RMSSD, SCL, SCR rate (one table per stressor)
- **Figure 1:** Time-series plots of raw and z-scored markers for representative participant (show baseline, stressor, recovery)
- **Table 2:** Paired t-test results (p-values, Cohen's d) for each marker × each stressor
- **Text:** Summary of effect sizes vs. literature expectations (Section 8.4)

#### 11.4.3 CSI Performance

- **Figure 2:** CSI time-series for representative participant with stressor phases colour-coded
- **Table 3:** Mean CSI ± SD per phase (baseline, MATH, CPT, TSST, each recovery)
- **Table 4:** CSI ROC results — per-stressor and combined AUC with 95% CI
- **Figure 3:** ROC curves (sensitivity vs. 1-specificity) for each stressor; combined curve
- **Table 5:** Optimal CSI thresholds (Youden-maximised, for future clinical use)

#### 11.4.4 Recovery Kinetics

- **Table 6:** Return-to-baseline times for each recovery period; comparison across stressors
- **Figure 4:** Recovery CSI trajectories (smoothed) for all participants, averaged by stressor

#### 11.4.5 Confound & Signal Quality Analysis

- **Table 7:** Percentage of epochs excluded due to activity gating, post-activity recovery, low electrode contact, respiratory rate flag, per stressor
- **Table 8:** Respiratory rate statistics (mean, SD) per phase
- **Figure 5:** Correlation matrix of all 4 markers during stress periods (check inter-marker consistency)

#### 11.4.6 Acceptance Criteria Results

- **Table 9 (repeats Section 9):** Full checklist with Pass/Fail and supporting statistics
- **Text:** Summary statement: "CSI passed [X/Y] P1 criteria and [X/Y] P2 targets."

### 11.5 Discussion

- **Summary of findings:** Did CSI discriminate stress vs. rest as expected? Were stressor responses in line with literature?
- **Deviation explanations:** If any criterion failed, explain (e.g., "PEP response to CPT smaller than expected because X")
- **Implications for algorithm:** Does the CSI formula need weight adjustment? Should baseline period be longer?
- **Limitations:** Sample size, participant demographics, single-site study, reference device availability, stressor order effects
- **Future work:** Extended CSI (BRS) validation, multi-site normative study, clinical applications

### 11.6 Appendices

- **A:** Detailed stressor scripts (mental arithmetic, TSST evaluation form)
- **B:** TSST video still frames (demonstrate standardised protocol adherence)
- **C:** Per-participant summary statistics (baseline means, stressor responses, AUC; one row per participant)
- **D:** Raw per-epoch CSV export (sample from one participant, first 30 minutes)
- **E:** Data quality log (electrode contact, activity gates, ectopic beats excluded, reference device drift)

---

## 12. Safety & Adverse Events

### 12.1 Cold Pressor Safety

The cold pressor test is a validated research stressor but carries discomfort risk. **Informed consent explicitly mentions the cold water immersion.**

**Contraindications (absolute):**
- Raynaud's syndrome or vasospastic disorders
- Open cuts or wounds on non-dominant hand
- Severe cold intolerance (self-reported)

**Monitoring during CPT:**
- Experimenter remains in view of subject at all times
- Subject informed they may withdraw at any time (no pressure to reach 3-minute target)
- If subject shows signs of severe distress (uncontrollable shivering, cyanosis, extreme pain), experimenter may ask subject to withdraw immediately
- Pulse oximetry (optional) to monitor for hypoxia

**Post-CPT:**
- Inspect hand for signs of cold injury (excessive redness, blistering, pain)
- If concerning findings: apply warm compress; advise follow-up with occupational medicine or primary care

### 12.2 TSST Safety

The TSST is a standardised psychological stressor with minimal physical risk. **Debriefing mandatory** after completion.

**Monitoring:**
- If subject requests to withdraw from speech portion mid-stream, experimenter permits withdrawal and ends task immediately
- Experimenter observes for signs of acute psychological distress (panic attack symptoms, severe trembling); if observed, pause and debrief

**Post-TSST:**
- Structured debrief: experimenter explains the stressor is a laboratory protocol, not a real job interview; reassures that performance is not evaluated
- Allow time for questions and concerns
- Provide contact information for researcher (Marcus Bell) in case post-session anxiety persists

### 12.3 Incident Reporting

Any adverse event (electrode burn, syncope, panic attack, uncontrolled hypertension, etc.) is recorded in a project adverse event log with:
- Participant ID (anonymised)
- Date and time
- Event description (objective, clinical assessment)
- Action taken
- Outcome / follow-up

**Serious adverse events** (hospitalisations, injuries requiring medical attention) reported to institutional safety officer and IRB within 24 hours.

---

## 13. Data Management & Ethics

### 13.1 Participant Anonymisation

- Participants assigned a study ID (CSI-VAL-###) at enrolment
- All data files, logs, and reports use study ID only (no names)
- Master key (linking study ID to participant name/contact) stored in locked file cabinet with access restricted to principal investigator (Marcus Bell) and scientific lead (Vasquez)

### 13.2 Data Security

- VU-AMS SD card recordings de-identified before analysis (rename file to CSI-VAL-### format)
- Exported CSV and analysis files stored in encrypted folder on lab PC
- Backup copies stored on institutional encrypted cloud storage (e.g., Institutional Box account, not commercial cloud)
- Video recordings of TSST speech stored separately in locked cabinet; accessible only to Vasquez and Hart for fidelity checks; discarded after validation report is finalised

### 13.3 Retention & Disposal

- Raw VU-AMS SD card recordings: retained for 5 years (allows post-hoc reanalysis if algorithm changes)
- Video recordings: discarded within 3 months of report completion
- Demographic questionnaires: discarded after 1 year
- De-identified analysis CSV files: retained indefinitely for publication and audit trail

### 13.4 Publication & Intellectual Property

- Publication of validation results follows institutional policy and Slow Horses company guidelines
- All algorithms and analysis code (Vasquez CSI formula, Reyes offline CSI implementation) remain proprietary; not disclosed in published papers beyond the algorithm description already in CSI-001
- Validation report filed as technical document within Slow Horses; availability restricted to project team unless otherwise approved

---

## 14. Known Limitations & Future Iterations

### 14.1 Limitations

1. **Single-site study:** N = 20 at one laboratory with one set of experimenters; may not generalise across sites
2. **Healthy volunteers:** Results may not apply to populations with autonomic dysfunction, psychiatric conditions, or cardiovascular disease
3. **Laboratory stressors:** TSST, mental arithmetic, and CPT are standardised but not ecologically representative of real-world stress (work meetings, family conflict)
4. **Single session:** No within-participant test-retest reliability data; unknown if CSI weights are stable across sessions
5. **Reference device availability:** Polar H10 and Shimmer3 GSR+ ideal but not always available; validation relies on VU-AMS internal signal processing alone in deployed settings

### 14.2 Future Work

1. **Extended CSI validation:** Repeat protocol with BRS added (CSI-5) in VU-DAMS offline mode; target AUC ≥ 0.80
2. **Multi-site replication:** Conduct validation at ≥ 2 additional sites (different experimenters, equipment) to confirm generalisability
3. **Ambulatory validation:** Recruit participants to wear VU-AMS + reference devices (Polar H10, Shimmer3) during daily activities (work, commute, social situations); assess CSI discrimination of self-reported stress vs. baseline in naturalistic settings
4. **Patient population validation:** Validate CSI in populations with known autonomic dysfunction (diabetes, POTS, panic disorder) to assess sensitivity and specificity in clinical populations
5. **Weight optimisation:** Run regression analyses on validation data to derive empirical weights (currently literature-informed; CSI-001 Section 4.3 leaves weights as open item)
6. **Longitudinal stability:** Multi-session study (same participants, 1-week intervals) to assess within-subject CSI stability and sensitivity to actual life stress changes

---

## 15. Approvals & Sign-Offs

**Prepared by:**  
Marcus Bell  
QA & Test Engineering, Slow Horses  
Date: 2026-05-11

**Reviewed by:**  
Dr. Elena Vasquez  
Biomedical Signal Processing, Slow Horses  
Date: [pending]  
Signature: \_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_

**Approved by:**  
Jackson Lamb (Lam)  
Head of Operations, Slow Horses  
Date: [pending]  
Signature: \_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_

**Institutional Ethics (IRB):**  
Protocol Reference: [pending]  
Approval Date: [pending]

---

## 16. References

All citations from CSI-001 Section 7, plus:

- Hjemdahl P et al. (1984). Comparison of plasma catecholamine and cardiovascular responses to mental stress. *Psychosom Med*, 46(5):429–439.
- Kövári E et al. (2015). Cold pressor response and autonomic function in healthy subjects. *Clin Auton Res*, 25(6):377–384.
- Kudielka BM et al. (2007). HPA axis and SES: A systematic review and meta-analysis of studies using the TSST. *Psychoneuroendocrinology*, 32(6):603–615.
- Munoz ML et al. (2015). Validity of (ultra-)short recordings for heart rate variability measurements. *PLOS ONE*, 10(9):e0138921.

---

*CSI-VAL-001 — Marcus Bell, QA & Test Engineering, Slow Horses — 2026-05-11*
