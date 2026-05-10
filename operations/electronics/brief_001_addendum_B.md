# Brief 001 — Addendum B: New Sensor Additions
## Body Temperature · PPG/SpO₂ · Skin Conductance Level (SCL/EDA)

**From:** Lam  
**To:** Nair (Electronics)  
**CC:** Vasquez (Biomedical Signal Processing), Müller (Firmware)  
**Date:** 2026-05-08  
**Status:** AUTHORISED BY PRINCIPAL

---

## Summary

Three new sensing modalities have been approved for the next-gen VU-AMS hardware. This addendum extends Brief 001 and Addendum A. All three are to be integrated into the existing ESP32-based platform without replacing or modifying the existing ICG/ECG analog frontend.

---

## 1. Body Temperature

### Requirement
Skin-surface temperature, high accuracy, continuous monitoring.

### Recommended component
**Texas Instruments TMP117**
- Interface: I²C (joins existing I²C bus: barometric sensor, fuel gauge)
- Resolution: 0.0078°C (16-bit)
- Accuracy: ±0.1°C (–20 to +50°C range)
- Supply: 1.8–5.5V, 135µA active / 150nA shutdown
- Package: SOT-563 (6-pin)

### Placement
On-skin contact probe — flex PCB or spring-contact mounting on the underside of the housing, contact with chest skin directly. Must maintain skin contact under movement. Thermally isolate from PCB/battery heat sources.

### Data block
New **T_block** in firmware:
- `skinTemp_C` (float32)
- Timestamp

### Open questions for Vasquez
- Required update rate? (1Hz likely sufficient — confirm)
- Acceptable thermal lag from core-to-skin temperature? Vasquez to specify if skin temperature is used for thermoregulatory stress scoring or only for baseline correction of other sensors.

---

## 2. PPG / SpO₂

### Requirement
Photoplethysmography for heart rate, HRV cross-validation, and SpO₂ estimation. Chest-mounted — **placement feasibility not yet confirmed.**

### Recommended component
**Maxim MAX30101** (or equivalent: MAX30102, MAXM86161)
- Interface: I²C
- LEDs: Red 660nm + IR 940nm, integrated photodiode
- Supply: 1.8V digital / 3.3V LED
- Current: LED drive 0–51mA programmable; photodiode 18-bit ADC internal
- Package: OLGA-14 (optical)

### Placement
PCB-integrated, chest-facing side of device housing. Requires optical aperture in housing, light seal around sensor (prevent ambient leakage), and direct skin contact or minimal air gap.

### ADC channel allocation
The MAX30101 has its own internal 18-bit ADC. Data is read from the IC over I²C — **does not consume channels on the external 24-bit ADC.** Good.

### Data block
New **P_block** in firmware:
- `ppg_red` (uint32)
- `ppg_ir` (uint32)
- `spo2_pct` (float32, computed on-device or iOS app)
- `hr_ppg` (uint8, BPM)
- Timestamp

### ⚠ Placement flag — Vasquez must validate
Finger and wrist are standard PPG locations. Chest placement is non-standard. Known issues:
- Motion artefact from respiration (chest wall moves with breathing — directly modulates optical path)
- Signal amplitude lower than fingertip (capillary density difference)
- SpO₂ accuracy on chest is unvalidated versus FDA-cleared finger pulse oximetry

**Vasquez to provide:**
1. Literature on chest-mounted PPG signal quality (SNR, HR accuracy, SpO₂ accuracy)
2. Go/no-go recommendation before Nair commits PCB real estate
3. If no-go: alternative placement (wrist-band addon? separate clip-on sensor?)

---

## 3. Skin Conductance Level (SCL / EDA)

### Requirement
Electrodermal activity (EDA) — both tonic SCL (slow baseline) and phasic SCR (event-driven galvanic skin response). Sympathetic nervous system activity. Complements existing HRV parasympathetic measures. Closes the gap vs Empatica E4.

### Electrode additions: E6 and E7
Two new Ag/AgCl gel electrodes, ~2cm spacing, chest placement.
- 47kΩ series protection resistors on each line (patient protection, IEC 60601-1)
- Electrode location TBD pending Vasquez validation (see below)

### Measurement chain (analog)
```
E6 ──[47kΩ]──┐
              ├── AC excitation ~500Hz–1kHz (low-voltage sine, <50mVpp across skin)
E7 ──[47kΩ]──┘
              └── Transimpedance amplifier → synchronous demodulator
                       ├── Low-pass filter → SCL_tonic → ADC ch6
                       └── Band-pass filter (0.01–5Hz) → SCL_phasic → ADC ch7
```

### ADC channel allocation
SCL_tonic → **ch6** (previously spare)  
SCL_phasic → **ch7** (previously spare)

**This exhausts all 8 channels on the existing 24-bit ADC.**  
Channel map, final:

| Ch | Signal |
|----|--------|
| 0 | ICG Z0 (baseline impedance) |
| 1 | V_source (ICG current monitor) |
| 2 | ECG1 (E1−E2) |
| 3 | ECG2 (E1−E3) |
| 4 | Temperature (legacy, deprecated by TMP117 — keep for board rev continuity) |
| 5 | Vbatt |
| 6 | SCL_tonic |
| 7 | SCL_phasic |

No spare channels. **Future sensor additions require a second ADC or analog MUX.** Nair to flag this in next hardware revision decision.

### Data block
New **S_block** in firmware:
- `scl_tonic` (float32, µS)
- `scl_phasic` (float32, µS, SCR amplitude)
- Timestamp

### ⚠ Placement flag — Vasquez must validate
Palmar sites (medial phalanges, thenar/hypothenar) are the gold standard for EDA — high sweat gland density. Chest EDA is non-standard and signal amplitude will be lower. This has been successfully recorded in published studies (Empatica's own validation work) but SNR is lower.

**Vasquez to provide:**
1. Minimum viable SCL signal amplitude from chest sites (µS)
2. Whether chest SCL is sufficient for phasic SCR detection (the clinically relevant signal) or only tonic baseline
3. Recommended inter-electrode spacing and site on chest
4. If chest is inadequate: propose alternative (e.g., integrated wrist-band, palm clip)

---

## Firmware briefing (Müller)

Three new data blocks required. See block structures above. Integration points:
- TMP117: I²C polling task, 1Hz, → T_block
- MAX30101: I²C interrupt-driven (FIFO watermark), ~100Hz raw PPG, → P_block; HR/SpO₂ computed via onboard algorithm or deferred to iOS
- SCL: ADC ch6/ch7 via existing SPI ADC read task; new decimation filter for SCL_tonic (heavy LP), band-pass for SCL_phasic → S_block

BLE GATT: Three new data characteristics needed, or extend existing data characteristic with block type identifiers P, S, T — coordinate with Chen (iOS Swift).

---

## Action summary

| Action | Owner | Deadline |
|--------|-------|----------|
| PPG chest placement validation (literature + go/no-go) | Vasquez | Before PCB layout |
| SCL chest placement validation (amplitude + SCR feasibility) | Vasquez | Before PCB layout |
| Body temp update rate requirement | Vasquez | Before firmware spec |
| TMP117 layout (thermal isolation, skin contact) | Nair | Rev 0.3 schematic |
| MAX30101 optical aperture + light seal design | Nair | Rev 0.3 schematic |
| SCL excitation circuit design + electrode drive | Nair | Rev 0.3 schematic |
| Second ADC decision (go/no-go for future expansion) | Nair + Lam | Before final BOM |
| T_block / P_block / S_block firmware implementation | Müller | After Vasquez go/no-go |
| BLE GATT extension for new blocks | Chen + Müller | After block structure confirmed |

---

*Addendum B to Brief 001. Filed: `operations/electronics/brief_001_addendum_B.md`*  
*Block diagram updated: `operations/electronics/block_diagram.svg` Rev 0.2*
