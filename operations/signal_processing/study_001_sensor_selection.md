# Study 001 — Sensor & Signal Selection for VU-AMS Next Generation

**Issued by:** Jackson Lamb  
**To:** Dr. Elena Vasquez (Biomedical Signal Processing)  
**Date:** 2026-05-08  
**Status:** Active — research phase

---

## Objective

Produce a structured evaluation of which physiological and physical signals are worth including in the next-generation VU-AMS device. The device is wearable, chest-worn, battery-powered, connects via BLE. Every sensor added costs space, power, complexity, and money. Every signal excluded may leave a research or clinical question unanswerable.

Vasquez to assess each candidate signal on the dimensions below and produce a recommendation: **Core** (must have), **Nice to have** (include if constraints allow), or **Out of scope** (good reason to exclude).

---

## Existing signals (already in the system — baseline)

| Signal | Block | Notes |
|--------|-------|-------|
| ECG (2 channels) | A-block | ECG1 = upper sense, ECG2 = lower sense |
| ICG — Z0 (baseline impedance) | A-block | DC thoracic impedance |
| ICG — V_source (drive reference) | A-block | Used to compute ΔZ |
| dZ/dt | Derived | Differentiated Z0; cardiac stroke waveform |
| Respiratory (from Z0) | Derived | Low-frequency Z0 modulation |
| 3-axis accelerometer | M-block | ax, ay, az (g) |
| 3-axis gyroscope | M-block | φx, φy, φz (°/s) |
| Barometric pressure + altitude | D-block | Environmental context |
| Temperature (on-device) | A-block | Ambient / skin proximity |
| Battery voltage | B-block | Operational |

---

## Candidate signals to evaluate

For each candidate, Vasquez should assess: scientific value, clinical value, feasibility on the platform, known confounds, and electrode/sensor requirements.

### 1. Skin conductance (EDA / GSR)
- Electrodermal activity — sympathetic nervous system proxy
- Strong stress correlate but highly motion-sensitive
- Requires dedicated electrodes (palmar preferred, wrist acceptable)
- Chest placement is non-standard — assess validity

### 2. Skin temperature
- Peripheral vasoconstriction under stress → finger/wrist temperature drops
- On-device chest temperature already captured — is it useful, or is peripheral temperature needed?
- Low-cost addition (thermistor)

### 3. Photoplethysmography (PPG)
- Optical pulse waveform — heart rate, SpO₂ (if dual-wavelength), pulse transit time
- Chest placement: non-standard, but possible
- Pulse transit time (ECG R-peak to PPG pulse) is a blood pressure proxy — high value
- Assess: chest PPG signal quality vs. wrist/finger

### 4. Continuous blood pressure (cNIBP)
- Gold standard stress measure
- Options: pulse transit time (PTT) via ECG + PPG — non-invasive, no cuff
- PTT requires accurate timing between ECG and PPG — feasible with existing ECG + new PPG
- Assess accuracy limitations of PTT-based cNIBP

### 5. Respiration — additional methods
- Already derived from ICG (Z0 respiratory modulation)
- Additional method: thoracic impedance from dedicated respiratory band (not feasible on this device)
- Nasal thermistor / airflow: not feasible (wearable)
- Assess: is ICG-derived respiration sufficient, or is a second independent channel worth having?

### 6. SpO₂ (blood oxygen saturation)
- Requires red + infrared PPG
- Clinically relevant in stress, exercise, sleep contexts
- Adds optical sensor + at least 2 wavelengths
- Chest placement: assess feasibility

### 7. EMG (electromyography)
- Muscle tension — stress correlate (trapezius is classic)
- Chest placement captures pectoral/intercostal — assess if this adds value over ECG lead
- High bandwidth requirement (20–500 Hz) — conflicts with ECG front-end unless separate channel
- Probably out of scope unless there is a specific research use case

### 8. EEG
- Not feasible on a chest-worn device. Out of scope — document and close.

### 9. Near-field communication / GPS
- Not physiological
- GPS: outdoor location context, high power cost
- Assess whether location context adds value to stress research

### 10. Ambient light sensor
- Circadian rhythm context, light exposure as stressor
- Very low cost and power
- Assess value

### 11. Microphone / acoustic sensor
- Speech activity detection (talking = social stressor context)
- Heart sounds (phonocardiography) — S1/S2 from chest wall
- Privacy implications of microphone in a research device — flag

### 12. Galvanic skin response via chest electrodes
- Some evidence that chest electrodes can detect sympathetic skin response
- Would require dedicated measurement channel separate from ECG/ICG
- Assess literature

---

## Output required from Vasquez

A structured report in `intel/science/study_001_sensor_report.md` covering:

1. **Recommendation table** — Core / Nice to have / Out of scope for each candidate
2. **Rationale** for each recommendation, with literature reference where available
3. **Dependency map** — which sensors require new electrodes, new ICs, extra power
4. **Priority order** for the "nice to have" group — if we can only add two, which two?
5. **Open questions** — what we'd want to know before committing to any candidate

---

## Constraints to factor in

- Device is chest-worn, body-worn surface area is limited
- Battery life is a hard constraint — high-power sensors (GPS, continuous optical) are expensive
- Electrode count is a hard constraint — each extra electrode is patient burden
- The device sits between the collarbones — skin contact quality there is reasonable but not ideal for all sensors
- BLE bandwidth is finite — more channels means larger packets or lower sampling rate

---

## Timeline

Vasquez to deliver initial recommendations before Nair finalises the PCB feature list. Nair cannot commit to sensor footprints or power budget without this input.

---

*Study 001 — commissioned.*
