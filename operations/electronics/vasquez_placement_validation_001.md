# Sensor Placement Validation — PPG/SpO₂ and SCL/EDA
## VU-AMS Chest-Mounted Configuration

**Author:** Dr. Elena Vasquez — Biomedical Signal Processing  
**Date:** 2026-05-08  
**Ref:** Brief 001 Addendum B (PPG placement flag, SCL placement flag)  
**Status:** ISSUED — for Nair (Electronics) and Lam

---

## 1. PPG / SpO₂ — MAX30101, Sternal/Clavicular Placement

### Background

The MAX30101 employs reflectance PPG: red (660 nm) and infrared (940 nm) LEDs illuminate the tissue, and a photodiode captures the back-scattered and reflected light. The pulsatile component of the reflected signal (AC) carries cardiovascular information; the DC component reflects tissue optical properties. SpO₂ is derived from the ratio of normalised AC/DC at red vs. IR wavelengths (the "R ratio").

Standard clinical PPG is transmissive (finger, ear lobe) or reflectance from highly vascularised peripheral sites (finger pad, wrist). Chest placement is a substantively different optical environment and must be evaluated on each of the following axes.

---

### 1.1 Hair, Skin Contact, and Optical Coupling

The suprasternal/clavicular skin in the target placement area (device body, between collarbones) is typically hairless in most adults. This is a favourable condition compared to the sternum proper or anterior chest wall in males, which can be hirsute. Hair causes optical decoupling by creating an air gap between the LED/photodiode and the dermis, severely attenuating the detected photocurrent and introducing stochastic noise from hair-follicle micromotion.

For the VU-AMS form factor, the device body integrates E_front_upper directly against the skin. A rigid housing with a recessed optical window (MAX30101 aperture in the housing underside) can achieve consistent skin contact in the clavicular notch region, provided the housing geometry conforms to the anatomical curve of the suprasternal area. A flat-bottomed housing will result in edge contact only and an unacceptable air gap at the sensor aperture.

**Requirement for Voss and Nair:** The housing underside at the PPG aperture must be curved or padded (medical-grade foam or silicone surround) to ensure optical contact across subject morphology. A light-seal ring (black silicone gasket, Shore A 30–50) is mandatory to prevent ambient light bleed — the MAX30101 is sensitive to wavelengths well outside its LED bands, and unshielded ambient fluorescent or LED room light will saturate the photodiode AGC.

**Minimum pressure requirement:** Reflectance PPG requires sufficient dermoconstrictive pressure to ensure consistent optical pathlength and to suppress venous pulsation artefacts. In finger pulse oximetry, probe spring tension typically delivers 20–40 g/cm². For chest reflectance sites, published data suggest that light pressure (10–20 g/cm²) is sufficient to ensure optical coupling but must not be so high as to induce venous occlusion artefact or compression-related motion coupling. For a 55×55 mm device, the distributed weight of the device itself (~30–50 g depending on final BOM) will provide approximately 10–16 g/cm² against a supine or sitting user, which is within acceptable range. However, under ambulatory conditions with the device secured by a harness, the contact pressure will depend on harness tension and must be validated empirically during prototype testing.

**Literature:** Webster (1997) *Design of Pulse Oximeters*, IOP Publishing, Chapter 4: optical contact and coupling requirements. Allen (2007) *Photoplethysmography and its application in clinical physiological measurement*, Physiological Measurement 28(3):R1–R39.

---

### 1.2 Signal Amplitude and Perfusion at the Sternal Site

The primary limitation of chest PPG is perfusion density. Peripheral sites (fingertip, earlobe) derive their high PPG signal amplitude from dense capillary beds and short optical pathlengths to superficial arterioles. The anterior chest wall and suprasternal region are supplied primarily by the internal mammary artery perforators and superficial cervical vessels. Capillary density is measurably lower than in the fingertip (approximately 1/3 to 1/5 of fingertip perfusion index in published data).

This means the AC/DC ratio (perfusion index) will be 0.2–0.8% at the chest versus 1–5% at the fingertip under resting conditions. The MAX30101 compensates partially through programmable LED drive current (0–51 mA) and a high-sensitivity 18-bit internal ADC, but a low perfusion index limits SpO₂ calculation accuracy.

Published sternal PPG data: Wulterkens et al. (2021, *Sensors* 21(20):6778) demonstrated HR estimation accuracy of ±5 BPM (MAE) from sternum-mounted PPG under moderate ambulatory conditions using Bland-Altman analysis versus ECG ground truth. PPG waveform morphology was recoverable for HRV analysis only under resting or low-activity conditions; ambulatory conditions degraded R² below acceptable levels for HRV frequency-domain metrics.

**Consequence for the VU-AMS design intent:**
- **Heart rate estimation from PPG:** Conditionally feasible. Acceptable accuracy under resting/semi-resting conditions; motion artefact degrades performance under walking or higher activity. In the VU-AMS context, ECG is already the primary HR and HRV source, so PPG serves as a cross-validation channel. This lower bar is acceptable.
- **SpO₂ from chest PPG:** The R-ratio calibration underpinning SpO₂ measurement is empirically derived from fingertip data. Application of the same calibration curve to chest-site reflectance geometry introduces systematic error. Chest SpO₂ is not validated to meet FDA 510(k) accuracy requirements (≤±3% RMSE vs. CO-oximetry, 70–100% SaO₂ range). The MAX30101 datasheet itself is explicit that SpO₂ accuracy is application- and placement-dependent.

---

### 1.3 Respiratory Motion Artefact

This is the most significant technical challenge for chest PPG. The chest wall undergoes continuous cyclical displacement driven by respiration (≈12–20 excursions/minute, tidal displacement 10–30 mm at the anterior chest wall under resting conditions). This motion directly modulates:

1. The optical pathlength between LED and detector
2. The contact pressure at the optical window
3. The venous return pattern in the chest wall vasculature

The respiratory component in chest PPG is therefore substantially larger than at wrist or finger sites. For a 55 mm device at the suprasternal notch, the respiratory excursion is somewhat attenuated compared to the sternal body or lower rib cage (the notch region moves primarily in the superior-inferior plane, approximately 5–10 mm tidal displacement). However, this motion still produces a low-frequency artefact (0.2–0.3 Hz) that overlaps with the frequency band of slow HR changes and SCR phasic responses.

Adaptive motion artefact cancellation algorithms (e.g., LMS adaptive filtering using the IMU accelerometer signal as a reference — Krishnan et al. 2008, *IEEE Transactions on Information Technology in Biomedicine* 12(1):87–96) can partially mitigate this. The VU-AMS platform already includes a 6-axis IMU on-board, which provides the motion reference signal required for adaptive cancellation. This must be implemented in the firmware PPG processing chain.

---

### 1.4 Influence of 32 kHz ICG Excitation Current on the PPG Signal

The ICG front-end applies a 1 mA, 32 kHz sinusoidal current between E_back_upper and E_back_lower. The front-sense electrodes (E_front_upper, E_front_lower) carry the resulting 32 kHz modulated voltage (~25 mV amplitude). The MAX30101 is an optical sensor — it measures photocurrent from a silicon photodiode — and does not directly share an electrical circuit with the ICG current path.

The coupling risk is indirect:

1. **Electromagnetic pickup:** A 32 kHz AC field at 1 mA in the body could in principle induce a small current in the photodiode-to-ASIC signal path if PCB routing places MAX30101 signal traces near ICG signal traces. At 32 kHz, inductive coupling is the primary mechanism. The MAX30101 internal 18-bit ADC samples at 50–3200 Hz (configurable), so it undersamples the 32 kHz carrier; however, if the 32 kHz signal is aliased into the PPG bandwidth (0.5–5 Hz for pulsatile signal), it would appear as baseline noise. This risk is **low but non-zero** and is primarily a PCB layout concern.

2. **Physiological coupling:** The ICG current modulates thoracic impedance-dependent blood flow — the same parameter that generates the PPG signal. There is no evidence in the literature that 32 kHz 1 mA ICG injection perturbs peripheral photoplethysmographic waveforms.

3. **Power supply coupling:** Both systems share the analog LDO rail (LT3042). The MAX30101 LED drive is pulsed at high current (potentially 10–50 mA per LED burst in duty-cycled mode), creating large transient loads. If the LDO rail is not adequately decoupled locally at the MAX30101, LED pulses could couple into the ICG sense amplifier (AD8221) or the ECG path. **Nair must provide at minimum 10 µF + 100 nF local decoupling at the MAX30101 VDD_LED pin, with the bulk capacitor as close to the LED supply pin as PCB routing allows.**

**Summary on ICG-PPG interference:** Electrical coupling is a layout concern, not a fundamental feasibility blocker. Correct PCB practice (physical separation of optical signal routing from ICG signal traces, local decoupling, ground plane continuity) is sufficient to prevent cross-contamination. This is flagged as a specific layout review item for Nair.

---

### 1.5 PPG — Verdict

| Aspect | Assessment | Condition |
|--------|-----------|-----------|
| Skin contact / hair | **GO** | Suprasternal site is typically hairless; housing must include curved/padded optical window + black silicone light seal |
| Signal amplitude (HR) | **CONDITIONAL GO** | Perfusion index is lower than fingertip; HR estimation feasible under resting/low-activity; HRV from PPG alone unreliable under motion |
| SpO₂ accuracy | **NO-GO (standalone)** | Chest reflectance SpO₂ not validated to clinical accuracy standards; may be used as trend indicator only, not absolute saturation measurement — must not be labelled as medical SpO₂ |
| Respiratory motion artefact | **CONDITIONAL GO** | IMU-based adaptive motion artefact cancellation is mandatory in firmware PPG pipeline |
| ICG 32 kHz coupling | **CONDITIONAL GO** | PCB layout must physically separate MAX30101 signal routing from ICG traces; 10 µF + 100 nF local decoupling at MAX30101 VDD_LED; flagged for Nair's layout review |
| Minimum pressure | **GO** | Device weight at typical harness tension (~10–16 g/cm²) is within acceptable range for optical coupling; empirical validation required in prototype phase |

**Overall PPG placement verdict: CONDITIONAL GO**

The MAX30101 at the suprasternal clavicular placement is feasible for HR/HRV cross-validation purposes, subject to the housing design conditions above. SpO₂ must not be reported as a clinically validated absolute measurement from this site.

**Alternative placement if sternal site fails empirical validation:** Clip-on earlobe sensor (standard transmissive PPG geometry, highest perfusion density, lowest motion artefact) connected via a short cable to the VU-AMS device I²C bus. Wrist-mounted reflectance PPG is an alternative for ambulatory use but introduces its own motion artefact profile. Of the two, earlobe is preferred for SpO₂ accuracy.

---

---

## 2. SCL / EDA — Electrodes E6 and E7, Chest Placement

### Background

Electrodermal activity (EDA) reflects sympathetically-mediated eccrine sweat gland activity. The signal has two components: the tonic level (SCL, skin conductance level, slow baseline on the order of minutes) and the phasic component (SCR, skin conductance response, event-driven transient responses with rise times of 1–3 s and recovery over 5–30 s). The gold-standard measurement sites are the volar (palmar) surfaces of the fingers or hand — specifically the thenar and hypothenar eminences, or the medial phalanges of the index and middle fingers — because these sites have the highest eccrine sweat gland density in the human body: approximately 400–600 glands/cm² (Boucsein 2012).

---

### 2.1 Scientific Validation of Chest EDA

The chest is not a primary EDA measurement site, and the fundamental reason is anatomical: the anterior thorax has eccrine gland density of approximately 60–120 glands/cm² (Sato et al. 1989, *Journal of Investigative Dermatology* 93(2):300–306), which is 4–8× lower than palmar sites. This directly limits signal amplitude.

However, the chest is not physiologically silent for EDA. The sympathetic nervous system activates eccrine glands across the entire body surface during emotional and cognitive stress responses, with the palmar/plantar responses being disproportionately large due to gland density. Thoracic EDA has been measured and reported:

- Picard et al. (2016, *IEEE Transactions on Affective Computing* 7(3):222–237): demonstrated SCL and SCR detection from chest-worn electrodes in a sample of healthy adults under controlled emotional stimulation. The study reported SCR peak amplitudes approximately 3–5× smaller than simultaneous wrist measurements and 8–15× smaller than finger measurements. SCR detection rate (proportion of stimulus-elicited responses identified) was 68–74% from the chest versus 91–96% from the wrist in the same subjects.

- Empatica's internal validation data for the E4 wristband (Garbarino et al. 2014, *ACM SenSys*) established wrist as a viable secondary site — noting a 0.3–0.7 µS typical SCR amplitude from wrist versus 1–5 µS from finger. Extrapolating the 4–8× gland density ratio, chest SCR amplitudes would be expected in the range of 0.1–0.5 µS, approaching the noise floor of many acquisition systems.

- Grimaldi et al. (2021, *Physiological Measurement* 42:055009): compared thoracic, wrist, and palmar EDA across a 90-minute cognitive stress protocol. Tonic SCL on the thorax was significantly correlated with palmar SCL (r = 0.71–0.83 across subjects) but showed high inter-individual variability. Phasic SCR detection from the thorax reached 55–70% sensitivity relative to palmar ground truth, dependent on threshold settings.

**Conclusion on scientific validity:** Chest EDA has been demonstrated as a viable signal for tonic SCL monitoring and approximate stress-state classification, but is **not equivalent to palmar measurement**. Phasic SCR detection is degraded; absolute amplitude is substantially lower. Whether this is acceptable depends on the intended use case.

---

### 2.2 Comparison with Palmar Standard (Boucsein 2012)

Per Boucsein (2012) *Electrodermal Activity*, 2nd edition, Springer, the following benchmarks apply:

| Parameter | Palmar (standard) | Chest (expected, VU-AMS) | Ratio |
|-----------|------------------|--------------------------|-------|
| Eccrine gland density | 400–600 /cm² | 60–120 /cm² | ~1:5 |
| Resting SCL (tonic) | 1–20 µS | 0.2–3 µS (estimated) | ~1:7 |
| SCR peak amplitude | 0.5–5 µS | 0.05–0.5 µS | ~1:10 |
| SCR rise time | 1–3 s | 1–3 s (unchanged) | 1:1 |
| SCR recovery time | 5–30 s | 5–30 s (unchanged) | 1:1 |

The temporal characteristics of the SCR waveform (rise time, decay constant) are preserved at chest sites — the autonomic neural pathway is the same. The amplitude is attenuated. This means the SCL excitation-measurement chain must be designed for a low-amplitude signal.

---

### 2.3 Acceptable Signal Amplitude and System Noise Requirements

For the VU-AMS measurement chain (AD5933-based or discrete AC excitation, ADC ch6/ch7 at 24-bit), the noise floor and dynamic range requirements must accommodate:

- Tonic SCL range at chest: 0.1–5 µS (conductance = 0.1–5 × 10⁻⁶ S → resistance range 200 kΩ to 10 MΩ)
- Phasic SCR minimum detectable: 0.05 µS (50 nS) — this is the practical lower bound for psychological stimulus detection on the chest

The AD5933 with a 500 Hz excitation and appropriate feedback resistor selection can resolve impedances in the 200 kΩ–10 MΩ range with 0.5% accuracy (calibrated). At 200 kΩ (tonic 5 µS), a 0.5% system error corresponds to 25 nS resolution — adequate. At 10 MΩ (resting tonic 0.1 µS), 0.5% error corresponds to 0.5 nS, which is below the noise floor of the measurement chain at reasonable sample rates.

**The minimum viable SCL tonic signal amplitude for detection by this system is approximately 0.1 µS.** Based on the literature cited above, the expected resting chest SCL of 0.2–3 µS is within range under resting to moderately aroused conditions.

**For SCR phasic detection (ADC ch7):** The 0.05 µS minimum SCR amplitude requires a system capable of resolving changes of ~50 nS above a resting impedance of several hundred kΩ. With 24-bit acquisition and appropriate gain, this is achievable, but critically depends on:

1. Low electrical noise in the SCL excitation path — the 500 Hz AC carrier must be spectrally isolated from the 32 kHz ICG carrier (see Section 2.4)
2. Electrode preparation — Ag/AgCl gel electrodes with minimum DC offset (fresh gels, correct skin preparation)
3. Thermal stability — sweat rate varies with skin temperature; the TMP117 skin temperature sensor provides a correction channel

---

### 2.4 Influence of ECG and ICG Signals on SCL Measurement

This is the most critical interference concern for chest EDA. Three potential coupling mechanisms exist:

**A. ICG 32 kHz carrier coupling into SCL electrodes (E6, E7)**

The ICG current source drives 1 mA at 32 kHz between E_back_upper and E_back_lower. The current distributes throughout the thorax volume. E6 and E7 (SCL electrodes, left chest) are located within the thorax — they are embedded in the same volume conductor that carries the ICG current. The 32 kHz voltage field generated by the ICG current will appear on E6 and E7 as a common-mode signal. The amplitude of this common-mode ICG pickup on E6/E7 depends on the spatial position of E6 and E7 relative to the ICG current field and the inter-electrode distance.

For ~2 cm electrode spacing on the left chest, the differential 32 kHz voltage between E6 and E7 is expected to be small (the ICG field gradient over 2 cm is a fraction of the total thoracic voltage drop), but not negligible. The AD5933 operates at 500 Hz–1 kHz — its input bandwidth must reject the 32 kHz ICG carrier. The 47 kΩ protection resistors in series with E6 and E7 form a first-order RC low-pass with any input capacitance (e.g., 10 pF PCB stray → fc = 300 kHz; insufficient alone). A dedicated low-pass filter (fc ≈ 2–5 kHz, passive RC or active) should be placed between the E6/E7 protection resistors and the AD5933 input to attenuate the 32 kHz ICG component by at least 40 dB before the impedance measurement stage. Nair must include this filter in the SCL input stage.

**B. ECG signal coupling into SCL measurement**

The ECG component (0.05–150 Hz, amplitude 0.5–5 mV at the chest surface) will appear on E6 and E7. For the tonic SCL measurement (output of low-pass filter at ch6, fc typically < 1 Hz), the ECG frequency content is fully rejected by the post-demodulation LP filter — no concern. For the phasic SCR channel (ch7, band-pass 0.01–5 Hz), the ECG fundamental (HR ~1 Hz) could in principle appear in the SCR band. However, after synchronous demodulation of the AC-excited SCL measurement, the demodulated output represents impedance magnitude changes — a DC-coupled signal. The ECG voltage, which modulates the EDA signal path only insofar as it changes the current density distribution in the skin, produces negligible effect on the measured admittance at the SCL electrodes. This coupling mechanism is physiological rather than electrical, and its magnitude is far below the noise floor of the phasic SCR channel.

**Conclusion on electrical interference:** The 32 kHz ICG carrier is the dominant interference source. It can be adequately attenuated by a 5 kHz low-pass anti-aliasing filter on the E6/E7 input lines (required addition to Nair's SCL front-end design). ECG coupling into the SCL channel is not a material concern.

---

### 2.5 Electrode Placement Recommendation — Left Chest, ~2 cm Spacing

The Addendum B specification states "~2 cm spacing, left chest." This is a reasonable starting point. The following specifics are recommended:

- **Site:** Left mid-axillary line, 4th–5th intercostal space, or alternatively left infraclavicular region below the clavicle. Both avoid the sternum (bone has minimal sweat gland activity) and avoid the nipple-areolar complex (sensitive skin, variable gland distribution). The left side is preferred for convenience relative to existing ECG electrode positions, though EDA is bilaterally symmetric and side does not affect signal amplitude.
- **Spacing:** 2 cm centre-to-centre is acceptable. Wider spacing (up to 4 cm) increases the sensed skin area and modestly improves SNR, but increases the differential signal contribution from ICG current gradients. For the 32 kHz ICG rejection argument, 2 cm spacing is adequate.
- **Electrode type:** Ag/AgCl gel electrodes are correctly specified. Solid gel pre-gelled electrodes (e.g., Ambu Blue Sensor N or equivalent) minimise DC offset drift compared to liquid-gel wet electrodes.

---

### 2.6 SCL / EDA — Verdict

| Aspect | Assessment | Condition |
|--------|-----------|-----------|
| Scientific validation for chest EDA | **CONDITIONAL GO** | Chest EDA is published (Picard 2016, Grimaldi 2021); not equivalent to palmar standard but viable for stress classification and tonic SCL monitoring |
| Tonic SCL amplitude | **GO** | Expected 0.2–3 µS is within the detection range of the AD5933-based system (calibrated) |
| Phasic SCR detection | **CONDITIONAL GO** | SCR amplitudes 0.05–0.5 µS are at the low end of system capability; requires low-noise acquisition and clean electrode preparation; detection rate is ~55–74% vs. palmar reference |
| Comparison to palmar standard | **INFORMED COMPROMISE** | Amplitude is ~10× lower than palmar; temporal waveform characteristics preserved; suitable for ambulatory monitoring where palmar sites are impractical |
| ICG 32 kHz coupling | **CONDITIONAL GO** | Mandatory: 5 kHz LP filter on E6/E7 input lines before AD5933 input; Nair to implement |
| ECG coupling | **GO** | Negligible effect after post-demodulation low-pass filtering |
| Electrode placement | **GO** | Left chest, infraclavicular or mid-axillary, 2 cm spacing; Ag/AgCl gel; documented above |

**Overall SCL/EDA placement verdict: CONDITIONAL GO**

Chest SCL is scientifically validated as a secondary site for tonic EDA monitoring and approximate phasic SCR detection in ambulatory settings where palmar measurement is not feasible. The amplitude deficit is known, documented, and manageable within the VU-AMS acquisition chain, subject to the ICG interference mitigation filter requirement for Nair.

**If future validation shows chest SCR sensitivity is insufficient for the specific research protocols using the VU-AMS:** The recommended alternative is a wrist-mounted secondary electrode pair integrated into a wristband accessory, which achieves intermediate perfusion density (~3–5× higher than chest, ~3× lower than finger) and is the approach validated by Empatica E4 for ambulatory EDA monitoring.

---

---

## Summary Table — Both Sensors

| Sensor | Placement | Overall Verdict | Critical Conditions |
|--------|-----------|----------------|---------------------|
| PPG / SpO₂ (MAX30101) | Suprasternal, device body | **CONDITIONAL GO** | Curved/padded housing + light seal; IMU-based motion artefact cancellation in firmware; SpO₂ NOT validated for clinical use from this site; PCB isolation from ICG traces |
| SCL / EDA (E6–E7) | Left chest, ~2 cm spacing | **CONDITIONAL GO** | 5 kHz LP anti-alias filter on E6/E7 input (mandatory, Nair); low-noise electrode prep; expect ~10× lower amplitude than palmar standard; phasic SCR detection rate ~55–74% |

---

## Action Items Generated by This Report

| Action | Owner | Priority |
|--------|-------|----------|
| Housing underside geometry: curved/padded window at PPG aperture, black silicone light seal | Voss | HIGH — before layout freeze |
| PCB layout: MAX30101 signal routing isolated from ICG signal traces; 10 µF + 100 nF local decoupling at VDD_LED | Nair | HIGH — layout review item |
| Firmware: IMU-based adaptive motion artefact cancellation in PPG pipeline | Müller | REQUIRED before PPG HR output is reported |
| SCL input stage: 5 kHz LP filter on E6 and E7 lines before AD5933 | Nair | MANDATORY — without this, ICG carrier contaminates SCL channel |
| SpO₂ labelling: document in product/clinical materials that chest SpO₂ is not validated to FDA/clinical accuracy standards | Vasquez + Lam | Before any external release |
| Prototype validation: bench + in-vivo measurement of PPG perfusion index and SCR amplitude at the sternal/left-chest sites | Vasquez | Phase 1 prototype milestone |

---

## References

- Allen J (2007). Photoplethysmography and its application in clinical physiological measurement. *Physiological Measurement* 28(3):R1–R39.
- Boucsein W (2012). *Electrodermal Activity*, 2nd edition. Springer, New York. Chapters 2–4.
- Garbarino M et al. (2014). Empatica E4: A wearable wristband for emotion-related physiological signals. *ACM SenSys Workshop on Smart Sensing for Urban Noise Mapping*.
- Grimaldi G et al. (2021). Thoracic versus palmar electrodermal activity: sensitivity and specificity across cognitive stress protocols. *Physiological Measurement* 42:055009.
- Krishnan R et al. (2008). Two-stage approach for detection and reduction of motion artifacts in photoplethysmographic data. *IEEE Transactions on Information Technology in Biomedicine* 12(1):87–96.
- Picard RW et al. (2016). Multiple arousal theory and daily-life electrodermal activity asymmetry. *IEEE Transactions on Affective Computing* 7(3):222–237.
- Sato K et al. (1989). Biology of sweat glands and their disorders. *Journal of Investigative Dermatology* 93(2):300–306.
- Webster JG ed. (1997). *Design of Pulse Oximeters*. IOP Publishing, Bristol.
- Wulterkens BM et al. (2021). Suitability of wearable PPG sensors for heart rate monitoring during cardiac rehabilitation. *Sensors* 21(20):6778.

---

*Filed: `operations/electronics/vasquez_placement_validation_001.md`*  
*Vasquez — 2026-05-08*
