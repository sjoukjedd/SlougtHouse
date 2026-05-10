# VU-AMS Demo Video Script
**Title:** VU-AMS Next Generation — Ambulatory Cardiac & Autonomic Monitoring  
**Runtime:** ~2 min 30 sec  
**Format:** Screen-recorded iOS live view + device close-ups + animated graphics; scientific tone; no voice-over narration (text cards + ambient lab sound)

---

## Storyboard

---

### SCENE 1 — OPEN [0:00 – 0:12]

**Visual:**  
Black screen. The VU-AMS logo resolves from centre — `VU` in white, `-AMS` in cyan (#00b6cb). Tagline appears below:

> *"The only wearable measuring cardiac output in the field."*

Background: slow-dissolve to a chest-worn device against a dark navy background.  
A subtle cyan gradient bar sweeps left-to-right across the bottom.

**Text card (white on dark):**
> VU-AMS — Next Generation  
> Vrije Universiteit Amsterdam · 2026

**Audio:** Low ambient tone, clean. No music until scene 3.

---

### SCENE 2 — DEVICE CLOSE-UP [0:12 – 0:28]

**Visual:**  
Slow rotation of the 55 × 55 × 22 mm housing. Electrode cables visible. Camera settles on the front face — smooth matte surface, USB-C port (capped), LED status indicator.

**Callout annotations appear one by one (animated fade-in):**
- `55 × 55 mm housing`
- `USB-C (mechanical safety cap)`
- `Status LED`
- `7-electrode configuration`

**Text card:**
> One 55 × 55 mm device.  
> Seven physiological signals. 28-hour battery.

---

### SCENE 3 — ELECTRODE PLACEMENT [0:28 – 0:52]

**Visual:**  
Diagram animation — body silhouette (front + back). Electrodes appear at their anatomical positions:

| Step | Electrode | Position | Label |
|------|-----------|----------|-------|
| 1 | E1 (clip) | Upper sternum / collarbone | ICG reference |
| 2 | E2 · E3 | Mid-chest, active clips | ICG sense / ECG |
| 3 | E4 | Left shoulder → neck cable | ICG return (neck) |
| 4 | E5 | Right shoulder → lower back | ICG return (back) |
| 5 | E6 · E7 | Left chest, 2 cm apart | SCL / EDA pair |

Cable routing animation: E4 and E5 arc over the shoulders in opposite directions.

**Text card:**
> ICG requires four body-surface electrodes — current injection and voltage sensing, fully separated.  
> No needle, no cuff, no clinic.

**Audio:** Subtle ambient sound. First music element enters — minimal, scientific.

---

### SCENE 4 — iOS LIVE ACQUISITION [0:52 – 1:18]

**Visual:**  
iPhone screen recording. App opens. Live signal traces appear:

- **ECG trace** — cyan, clean R-peaks visible
- **ICG dZ/dt trace** — white, B-C-X-Y-O waveform visible
- **SCL trace** — green, slow tonic baseline with small phasic deflections
- **PPG trace** — orange, pulsatile waveform

Panel in corner shows live values updating every beat:
```
HR      72 bpm
CO      5.1 L/min
SV      71 mL/beat
HRV     48 ms (RMSSD)
SpO₂    98 %
SCL     3.2 µS
T_skin  33.6 °C
```

Zoom in: electrode contact quality indicator — all 7 channels show green checkmarks.

**Text card:**
> Live streaming via BLE 5.0 to iPhone, iPad, Apple Watch.  
> Real-time signal quality monitoring — every electrode, every beat.

---

### SCENE 5 — ICG HAEMODYNAMICS [1:18 – 1:42]

**Visual:**  
Animated anatomy diagram — heart and thorax. ICG excitation current (32 kHz, 1 mA) flows between E1 (neck) and E5 (lower back). Voltage sense path from E2/E3.

Derived parameter flow animation:
```
Raw Z(t)  →  dZ/dt  →  B-point  →  PEP
                   ↘  C-point  →  LVET
                              ↘  SV → CO
```

**Text card (split screen — VU-AMS vs alternatives):**

| Parameter | VU-AMS | ECG-only wearable | PPG wearable |
|-----------|--------|-------------------|--------------|
| Cardiac output | ✓ Direct | ✗ | ✗ |
| Stroke volume | ✓ Direct | ✗ | Estimated |
| Baroreflex sensitivity | ✓ | ✗ | ✗ |
| PEP (sympathetic index) | ✓ | ✗ | ✗ |

**Text card:**
> ICG is the only non-invasive method for ambulatory cardiac output.  
> VU-AMS is the only device that delivers it outside a laboratory.

---

### SCENE 6 — NEW SENSORS HIGHLIGHT [1:42 – 2:00]

**Visual:**  
Four panels animate in from corners, each showing a sensor:

**Top-left — SCL / EDA (green border):**  
Skin conductance trace with SCR peaks highlighted. Label: *"Sympathetic nervous system — tonic arousal + phasic responses"*

**Top-right — PPG / SpO₂ (orange border):**  
Optical waveform, SpO₂ readout. Label: *"Heart rate cross-validation + continuous oxygen saturation"*

**Bottom-left — Body Temperature (yellow border):**  
Temperature strip, slow variation over time. Label: *"Continuous skin temp · TMP117 · ±0.1 °C · thermoregulatory context"*

**Bottom-right — 9-DOF Motion (purple border):**  
Accelerometer trace during steps, orientation visualization. Label: *"Artefact rejection · posture · activity classification · ICM-20948"*

**Central text card:**
> NEW in this generation.  
> Both autonomic branches. One recording session.

---

### SCENE 7 — VU-DAMS OFFLINE ANALYSIS [2:00 – 2:18]

**Visual:**  
Screen recording of VU-DAMS Java desktop application. Timeline view with ECG/ICG ensemble. Artefact marking tool in use. Export dialogue — SPSS / MATLAB / R options visible.

Parameter table shows a subject's session: CO, SV, PEP, HRV, SCL amplitude summary.

**Text card:**
> VU-DAMS offline analysis platform.  
> Artefact rejection · ensemble averaging · publication-ready export.  
> Windows · macOS

---

### SCENE 8 — SPECIFICATIONS SUMMARY [2:18 – 2:28]

**Visual:**  
Dark navy background. Spec tiles appear in a 3 × 2 grid:

| Tile | Value |
|------|-------|
| Battery | 28 h |
| Housing | 55 × 55 × 22 mm |
| ADC | 24-bit · 1 kHz |
| Wireless | BLE 5.0 |
| Safety | IEC 60601-1 Type BF |
| Signals | 7+ physiological |

---

### SCENE 9 — CLOSE [2:28 – 2:35]

**Visual:**  
VU-AMS logo returns centre-screen.

**Text card:**
> VU-AMS — Next Generation  
> **vu-ams.nl**  
>  
> *For research use only. Not intended for clinical diagnosis.*  
> Vrije Universiteit Amsterdam

**Audio:** Music resolves to silence. Fade to black.

---

## Production Notes

| Item | Specification |
|------|---------------|
| Aspect ratio | 16:9 (1920 × 1080) |
| Colour grade | Dark science — deep navy + cyan highlights |
| Font | Inter Bold / Extra Bold for titles; Regular for body |
| Title cards | White text on `#101430`, cyan accent `#00b6cb` |
| Music | Royalty-free: minimal electronic, 80–90 BPM, no lyrics |
| Voice-over | None — text cards only (accessible; works muted) |
| Subtitles | English captions, auto-generated + reviewed |
| Chapters | YouTube chapter markers at each scene |

## Required Assets to Capture

- [ ] Device close-up rotation (real device or CAD render)
- [ ] Electrode placement on torso model or actor
- [ ] iOS app screen recording — live ECG + ICG + SCL + PPG
- [ ] VU-DAMS screen recording — offline processing session
- [ ] Animated diagrams: ICG current path, parameter derivation flow (After Effects / Keynote)
- [ ] Comparative table animation (scenes 5, 6)

## Distribution

| Platform | Format | Spec |
|----------|--------|------|
| Website (vu-ams.nl) | MP4 H.264 | 1080p, autoplay-safe (muted) |
| YouTube | MP4 H.264 | 1080p, chapters, captions |
| LinkedIn | MP4 | ≤ 200 MB, square crop (1:1) for feed |
| Conference kiosk | MP4 loop | 1080p, no audio dependency |
| Email (thumbnail) | JPEG still + link | Scene 4 iOS screenshot |
