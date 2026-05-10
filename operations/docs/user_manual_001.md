# VU-AMS User Manual v0.1

**Document number:** VU-AMS-UM-001  
**Revision:** 0.1 — First draft  
**Date:** 2026-05-09  
**Status:** Draft — not for distribution

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Hardware Overview](#2-hardware-overview)
3. [Getting Started](#3-getting-started)
4. [Recording a Session](#4-recording-a-session)
5. [Downloading Data](#5-downloading-data)
6. [Safety and Care](#6-safety-and-care)
7. [Troubleshooting](#7-troubleshooting)

---

## 1. Introduction

### 1.1 What the VU-AMS measures

The VU-AMS (Vrije Universiteit Ambulatory Monitoring System) is a wearable physiological recording device designed for use in clinical research. It continuously measures three classes of signal from the body:

- **ECG (Electrocardiography)** — the electrical activity of the heart, recorded from electrodes placed on the skin surface. From the ECG, the system derives heart rate and heart rate variability (HRV), which reflect how the nervous system regulates cardiac function.

- **ICG (Impedance Cardiography)** — the mechanical activity of the heart, measured by passing a very small, imperceptible electrical current (1 mA at 32 kHz) through the thorax and detecting how the impedance changes with each heartbeat. From the ICG waveform, the system estimates stroke volume (the amount of blood ejected per beat), cardiac output (total blood flow per minute), and the pre-ejection period (a marker of sympathetic nervous system activity).

- **Movement (Inertial Measurement Unit)** — three-axis acceleration, three-axis gyroscope, and three-axis magnetometer data, recorded at 100 Hz. Movement data is used to detect physical activity, correct physiological signals for motion artefacts, and provide context for interpreting stress-related changes.

The device also records supplementary signals: photoplethysmography (PPG, optical pulse detection at the fingertip), skin conductance level (SCL, also known as electrodermal activity or EDA — a sweat-gland response to psychological arousal), skin temperature, and barometric pressure.

### 1.2 Why these signals matter

The autonomic nervous system (ANS) governs the body's involuntary responses to stress, threat, and effort. It has two branches: the sympathetic system (activating, associated with arousal and stress) and the parasympathetic system (calming, associated with rest and recovery). These two branches continuously influence the heart, blood vessels, sweat glands, and other organs.

Measuring ANS activity non-invasively is difficult. The VU-AMS achieves it by combining ECG, ICG, and movement data to compute validated autonomic markers — HRV indices, baroreflex sensitivity, stroke volume variability — that cannot be obtained from any single signal alone.

The result is an objective, continuous, and unobtrusive record of a participant's physiological stress state throughout a naturalistic day or a controlled laboratory protocol.

### 1.3 Who this manual is for

This manual is written for clinical researchers who are deploying the VU-AMS with participants. It assumes no engineering background. You do not need to understand electronics or signal processing to use this device safely and effectively.

If you are configuring analysis pipelines or integrating raw data files, refer to the separate analysis software documentation.

---

## 2. Hardware Overview

### 2.1 The device

The VU-AMS device is a small, lightweight unit worn on the upper chest, clipped to clothing or carried in a purpose-made vest. It contains:

- The main electronics board with the ESP32 microcontroller, analog signal-conditioning circuits, and wireless radio
- A rechargeable lithium-polymer battery
- A microSD card slot for local data storage
- A USB-C port for charging
- Five LED indicators
- A single push-button for power and recording control

The device is connected to the participant by five electrode lead wires (see Section 2.3). The device body itself also serves as one electrode (the collarbone contact, E1).

### 2.2 LED indicators

The device has five LEDs. Their meanings during normal operation are as follows:

| LED | Colour | Meaning |
|-----|--------|---------|
| Power | Green (steady) | Device on, battery sufficient |
| Power | Green (slow blink) | Device on, battery low (below 15%) |
| Power | Red (steady) | Battery critical (below 5%) — save and end session |
| BLE | Blue (blink, 1 Hz) | Advertising — waiting for app connection |
| BLE | Blue (steady) | Connected to app |
| SD | Orange (blink) | Writing data to SD card |
| SD | Red (steady) | SD card error — do not end session until resolved |
| Recording | White (steady) | Recording in progress |

If both the SD LED and Recording LED are lit, data is being captured and saved. If the Recording LED is white but the SD LED is dark, the device is streaming via Bluetooth but the SD card may not be writing — check the app for a warning.

### 2.3 Electrode layout

The VU-AMS uses seven electrodes in total, labelled E1 through E7. E1 is the device body itself; E2–E5 are body-surface snap-electrode lead wires for ECG and ICG; E6 and E7 are a dedicated two-electrode pair on the non-dominant hand for skin conductance (SCL/EDA) measurement.

All seven electrode positions must be correctly occupied for a complete recording. Omitting any electrode will result in one or more signal channels being absent or unreliable.

**Summary table:**

| Electrode | Location | Lead colour | Function |
|-----------|----------|-------------|----------|
| E1 | Device body — upper chest, collarbone area | — (device body) | ICG voltage sense (+), ECG reference |
| E2 | Solar plexus — upper abdomen, midline | Yellow | ICG voltage sense (−), ECG channel |
| E3 | Lower left rib — below left breast | Red | ECG only |
| E4 | Back of neck — base of skull, midline | Black | ICG current injection (+), ECG driven reference |
| E5 | Lower back — lumbar midline | White | ICG current injection (−), ECG driven reference |
| E6 | Palm of non-dominant hand — thenar eminence | Green | SCL active electrode |
| E7 | Dorsum of non-dominant hand — index/middle finger | Blue | SCL reference electrode |

**Electrode diagram:**

```
                          FRONT OF BODY

        E1  [Device / collarbone area]
            Attached directly to device chassis.
            ICG voltage sense (+) · ECG

        E2  [Solar plexus — upper abdomen, midline]
            Yellow lead wire.
            ICG voltage sense (−) · ECG

        E3  [Lower left rib — below left breast]
            Red lead wire.
            ECG only


                          BACK OF BODY

        E4  [Neck — base of skull, midline]
            Black lead wire.
            ICG current injection (+) · ECG driven reference

        E5  [Lower back — lumbar, midline]
            White lead wire.
            ICG current injection (−) · ECG driven reference


                      NON-DOMINANT HAND

        E6  [Palm — thenar eminence]
            Green lead wire.
            SCL active electrode

        E7  [Dorsum — between index and middle finger, proximal phalanx]
            Blue lead wire.
            SCL reference electrode
```

The ECG/ICG path: current is injected from back (E4 to E5) through the thorax; the resulting voltage difference is sensed at the front (E1 to E2). The ECG is derived simultaneously from E1, E2, and E3. E4 and E5 additionally serve as right-leg-drive electrodes for ECG common-mode rejection.

The SCL path: E6 and E7 connect to the AD5933 impedance analyser circuit inside the device via the green and blue lead wires. Skin conductance is measured as the real part of the admittance between these two electrodes.

#### Electrode placement guidance

- All electrode sites should be clean and dry. Remove any body lotion, oil, or sweat with the supplied alcohol wipes before applying electrodes. Allow 30–60 seconds for the skin to dry fully before applying the electrode pad.
- Use the disposable adhesive Ag/AgCl gel electrodes supplied with the device. Do not reuse electrodes across participants or sessions.
- E1 is attached via the device's own snap connector on its rear surface — press the device firmly against the collarbone area and secure with the device clip or vest.
- E2 (solar plexus): find the midpoint between the navel and the lower sternum. Place centrally.
- E3 (lower left rib): approximately one hand-width below the left breast, along the lower rib margin.
- E4 (neck): at the base of the skull, on the midline, in the hollow between the trapezius muscles.
- E5 (lower back): at the lumbar midline, approximately level with the top of the pelvis.
- E6 (palm): on the thenar eminence (the fleshy area at the base of the thumb), non-dominant hand. Avoid placing over tendons or the palm crease.
- E7 (dorsum): on the back of the same hand, on the proximal phalanx between the index and middle fingers.

Ensure each electrode lies flat against the skin with no air pockets under the adhesive. Poor electrode contact is the most common cause of poor signal quality.

---

## 3. Getting Started

### 3.1 What you will need

- VU-AMS device
- Device clip or participant vest
- Electrode kit: seven Ag/AgCl electrodes plus lead wires (E1–E7; E1 is built into the device)
- USB-C charging cable
- iPhone or iPad running the VU-AMS companion app (iOS 16 or later)
- MicroSD card (supplied, formatted and inserted)

### 3.2 Charging the device

1. Connect the supplied USB-C cable to the device's USB-C port (located on the bottom edge).
2. Connect the other end to a USB charger or computer port.
3. The Power LED will glow amber during charging.
4. When charging is complete, the Power LED turns green.
5. A full charge from empty takes approximately 2–3 hours. A fully charged battery supports a minimum of 8 hours of continuous recording.

Do not leave the device charging unattended for extended periods beyond a full charge. Do not charge in temperatures above 40 °C or below 0 °C.

### 3.3 Checking the SD card

Before every recording session, verify that the microSD card is correctly seated:

1. The card slot is on the side of the device (behind a small cover on some units).
2. Push the card gently until it clicks into place.
3. When you power on the device, the SD LED will blink briefly orange as the card is initialised, then go dark. If the SD LED turns red after power-on, the card is not recognised — remove and reinsert it, or replace it.

Always use the supplied microSD card. The device is tested with specific card types. Third-party cards may cause write errors.

### 3.4 Powering on

Press and hold the power button for 2 seconds until the LEDs cycle (a brief flash of all LEDs). The device will initialise over approximately 5 seconds:

1. All LEDs flash once — firmware started.
2. SD LED blinks orange — card being checked.
3. BLE LED begins blinking blue (1 Hz) — device is advertising and ready to pair.

### 3.5 Pairing with the iPhone app

Pairing is required once per device–phone combination. Subsequent sessions connect automatically.

**First-time pairing:**

1. Open the VU-AMS app on the iPhone.
2. Tap **Devices** in the bottom navigation bar.
3. Tap **Add Device**. The app will scan for nearby VU-AMS devices.
4. Your device will appear listed by its device ID (printed on the label on the back of the device). Tap it.
5. The iPhone will prompt you to confirm the Bluetooth pairing — tap **Pair**.
6. The BLE LED on the device will change from blinking to steady blue, confirming connection.
7. The app will display the device status screen showing battery level, SD card status, and sensor readiness.

**Subsequent sessions:**

Open the app and navigate to **Devices**. Your device will appear under **Known Devices**. Tap **Connect**. The device must be powered on and within Bluetooth range (approximately 10 metres, line-of-sight).

### 3.6 Applying the device to a participant

1. Confirm the participant has given informed consent and that there are no contraindications (see Section 6.1).
2. Prepare all seven electrode sites as described in Section 2.3.
3. Apply E4 (neck) and E5 (lower back) first, attaching the lead wires before placing the electrodes on the skin. The back electrodes are the most difficult to reach; doing them first while the participant is accessible is recommended.
4. Apply E2 (solar plexus) and E3 (lower left rib).
5. Apply E6 (palm) and E7 (dorsum) to the non-dominant hand. Route the SCL lead wires under clothing if possible to reduce movement artefact.
6. Attach the device to the participant using the clip or vest, positioning it at the upper chest so E1 is in firm contact with the collarbone area.
7. In the app, navigate to the **Signal Quality** screen. All seven electrode channels should show green status. Yellow indicates marginal contact; red indicates poor or no contact. Do not begin recording until all channels are green.

---

## 4. Recording a Session

### 4.1 Starting a recording

Once the device is connected to the app and all electrode channels show good signal quality:

1. In the VU-AMS app, navigate to the **Record** tab.
2. Enter the participant ID and session label in the provided fields. These will be embedded in the data file.
3. Tap **Start Recording**.
4. The app will confirm the command was received. On the device, the Recording LED (white) will illuminate.
5. The app transitions to the **Live View** screen.

Recording simultaneously stores data to the SD card and streams selected signals to the app via Bluetooth. If the Bluetooth connection is lost during recording, the device continues recording to the SD card without interruption.

### 4.2 What the live view shows

The live view displays the following in real time:

| Panel | Signal | Update Rate |
|-------|--------|-------------|
| ECG trace | Raw ECG waveform, two channels | 1000 samples/s |
| Heart rate | Beats per minute, derived from ECG R-peaks | Updated each beat |
| ICG trace | –dZ/dt waveform | 1000 samples/s |
| Stroke volume | Estimated mL per beat | Updated each beat |
| Cardiac output | Estimated L/min | Updated each beat |
| Movement | Acceleration magnitude | 100 samples/s |
| PPG | Optical pulse waveform | 100 samples/s |
| SpO₂ | Peripheral oxygen saturation (%) | Updated each pulse |
| SCL | Skin conductance level (µS) | ~10 samples/s |
| Skin temperature | °C | ~4 samples/s |

Traces are colour-coded by signal type. ECG is shown in green, ICG in blue, and movement in orange. You can pinch to zoom any trace.

A red banner will appear at the top of the live view if any of the following conditions occur: electrode contact lost, SD card write error, battery critical, or Bluetooth packet drop rate above threshold.

### 4.3 Conducting a protocol

The VU-AMS does not impose any restrictions on participant behaviour during recording. The device is designed for ambulatory use — participants may walk, sit, stand, and carry out normal activities.

For best signal quality during critical measurement periods (such as orthostatic challenges or cognitive stress tasks), ask the participant to remain still and breathe normally. Movement artefacts in the ECG and ICG are automatically flagged in the offline analysis software.

### 4.4 Stopping a recording

1. In the app, tap **Stop Recording** on the Live View screen (or the Record tab).
2. Confirm the prompt.
3. The firmware will close the current data file cleanly on the SD card. This takes 1–3 seconds.
4. The Recording LED will extinguish.
5. The app will display a session summary: duration, estimated data volume, and a quality report (proportion of clean ECG beats detected).

Do not power off the device or remove the SD card immediately after tapping Stop. Wait until the Recording LED has extinguished and the SD LED has finished blinking.

---

## 5. Downloading Data

### 5.1 Data storage on the SD card

All physiological data is written continuously to the microSD card during recording. The SD card stores data even when Bluetooth is unavailable or disconnected. Data on the SD card is the primary record — the app's live view is for monitoring only.

Data is organised in a folder structure on the card:

```
VUAMS/
└── SESSIONS/
    └── <PARTICIPANT_ID>_<SESSION_LABEL>_<TIMESTAMP>/
        ├── session.vuams      — binary data file (all signal blocks)
        └── session_info.json  — session metadata (participant ID, start time, firmware version)
```

### 5.2 Removing the SD card

1. **Stop the recording** (Section 4.4) and wait for the Recording LED to extinguish.
2. Power off the device by pressing and holding the power button for 3 seconds.
3. Wait for all LEDs to extinguish.
4. Open the card slot cover and press the microSD card inward gently — it will spring out.
5. Remove the card and insert it into a standard microSD-to-SD adapter for use with a computer, or connect via a USB card reader.

Never remove the SD card while the Recording or SD LED is lit. Doing so will corrupt the current data file.

### 5.3 File format overview

The `.vuams` file is a binary stream of data blocks. Each block begins with a 16-byte header:

| Byte offset | Field | Size | Description |
|-------------|-------|------|-------------|
| 0 | Block type | 1 byte | `A` (ECG), `I` (ICG derived), `M` (IMU), `P` (PPG), `S` (SCL), `T` (Temperature) |
| 1 | Schema version | 1 byte | Currently `0x01` |
| 2–3 | Payload length | 2 bytes (little-endian) | Number of bytes following this header |
| 4–11 | Timestamp | 8 bytes (uint64, µs) | Device microsecond timer at first sample in block |

The header is followed immediately by the payload, whose structure depends on the block type:

| Block type | Contents | Samples per block | Block interval |
|------------|----------|-------------------|----------------|
| A (ECG) | 2 × 250 int32 samples | 250 per channel | 250 ms |
| I (ICG derived) | z0, dZdt_peak, PEP, LVET, cardiac output, stroke volume (all float32) | 1 (per heartbeat) | Per beat |
| M (IMU) | 9 axes × 10 int16 samples (accel, gyro, magnetometer) | 10 per axis | 100 ms |
| P (PPG) | Red count, IR count, SpO₂ %, HR (bpm), validity flag | 1 | ~10 ms |
| S (SCL) | Tonic SCL (µS), phasic SCL (µS), contact flag | 1 | ~10 ms |
| T (Temperature) | Skin temperature (°C), raw register value | 1 | 250 ms |

All multi-byte integers are little-endian. Floating-point values are 32-bit IEEE 754.

To load data into the analysis desktop application, use **File > Open Session** and select the `.vuams` file. The application will parse and display all channels automatically.

---

## 6. Safety and Care

### 6.1 Intended use and contraindications

The VU-AMS is intended for non-therapeutic physiological monitoring in research participants who are able to give informed consent.

**Do not use the VU-AMS on participants who:**

- Have a cardiac pacemaker or implantable cardioverter-defibrillator (ICD)
- Have any implanted active medical device in the thorax
- Have known skin sensitivity or allergy to Ag/AgCl electrode gel
- Have open wounds, rashes, or skin lesions at any electrode site
- Are under 18 years of age without specific institutional approval and parental consent
- Are pregnant (unless specifically approved in your ethics protocol)

The ICG measurement injects a small current through the body. Although the current (1 mA at 32 kHz) is well below any threshold for physiological stimulation, it is contraindicated in participants with implanted electronic devices. When in doubt, consult the participant's physician before use.

### 6.2 Electrical safety classification

The VU-AMS is designed to comply with **IEC 60601-1 Type BF** applied-part requirements. Type BF signifies:

- **B** (Body): the device is suitable for continuous contact with the body
- **F** (Floating): the applied parts (electrode inputs) are electrically isolated from ground and from all other circuits

This isolation means that leakage currents through the electrodes are kept below the IEC 60601-1 limits for body-applied parts, even in the event of a single fault condition.

The device is not suitable for direct cardiac application (Type CF classification). It must not be connected to or used alongside intravenous catheters or other devices with direct heart access.

### 6.3 Battery care

- Charge the battery before each recording session. Do not begin a long session on a partially charged battery.
- The device will display a low-battery warning (Power LED blinking green) when the battery reaches 15% state of charge. You have approximately 1 hour of recording remaining at this point under normal operating conditions.
- At 5% state of charge, the device will begin a graceful shutdown: it will close the current data file cleanly and power off. Do not attempt to restart a critically low device for recording — charge it first.
- Do not expose the device to temperatures above 45 °C (do not leave in a closed vehicle in hot weather).
- If the device will not be used for more than two weeks, store it at approximately 50% charge in a cool, dry location.
- Do not attempt to replace the battery yourself. Contact the study coordinator for battery service.

### 6.4 Cleaning and electrode care

**After each participant:**

1. Disconnect all electrode lead wires from the disposable electrodes. Discard the electrodes.
2. Wipe the lead wire connectors with a 70% isopropyl alcohol wipe. Allow to dry before reconnecting to new electrodes.
3. Wipe the device chassis with a lightly dampened cloth. Do not submerge the device in liquid and do not spray cleaning agents directly onto the device.
4. Inspect the lead wires for damage (cracked insulation, loose connectors). Replace any damaged leads before next use.

The device is not autoclavable and is not rated for immersion cleaning.

### 6.5 Adverse event procedure

If a participant reports discomfort, tingling, or skin irritation at any electrode site:

1. Immediately stop the recording.
2. Remove the affected electrode.
3. Inspect the skin. If redness is mild and fades within 10 minutes, it is likely a normal reaction to electrode adhesive and does not require further action.
4. If redness persists, the skin is broken, or the participant reports pain beyond mild discomfort, follow your institution's adverse event reporting procedure.
5. Document the incident in the study log.

---

## 7. Troubleshooting

### Problem 1: Device will not power on

**Symptoms:** No LEDs illuminate when the power button is held.

**Likely causes and solutions:**

1. **Battery fully discharged.** Connect the device to a USB-C charger. Leave it for at least 15 minutes before attempting to power on. A fully discharged battery may take several minutes before the charging circuit begins and the amber charging LED appears.
2. **USB-C cable fault.** Try a different USB-C cable and/or charger. Some cables are charge-only and do not support data — any standard USB-C power cable should work for charging.
3. **Firmware fault.** If the device was previously operating normally and refuses to power on after a full charge, contact technical support. Do not attempt to open the device.

---

### Problem 2: One or more electrode channels show red (poor contact) in the app

**Symptoms:** Signal quality screen shows red or yellow for one or more electrodes immediately after application.

**Likely causes and solutions:**

1. **Poor skin preparation.** Remove the electrode, clean the skin site with an alcohol wipe, allow to dry fully (30–60 seconds), and reapply a fresh electrode.
2. **Air pocket under the electrode.** Press the electrode firmly from the centre outward to expel air. The adhesive ring must lie flat against the skin.
3. **Lead wire not clicked in.** Ensure the lead wire snap connector is fully seated on the electrode stud — you should feel a click.
4. **Electrode past expiry.** Check the expiry date on the electrode packet. Expired electrodes lose gel conductivity.
5. **E1 (device contact).** If E1 shows poor contact, the device body is not pressing firmly against the collarbone area. Adjust the clip or vest.

---

### Problem 3: Recording starts but the ECG trace is noisy or flat

**Symptoms:** Live view shows a flat line, large baseline drift, or frequent artefact bursts on the ECG.

**Likely causes and solutions:**

1. **Movement artefact.** Ask the participant to remain still briefly to confirm signal quality. Some motion artefact during walking is expected and will be handled by the analysis software.
2. **E4 or E5 contact lost.** The back electrodes are the most commonly dislodged during movement. Check under the participant's clothing — both electrodes should lie flat. Re-apply if necessary.
3. **Cable routing.** Lead wires should be routed under clothing and not left to dangle freely. Excess cable should be coiled loosely and secured with surgical tape.
4. **50/60 Hz interference.** A regular high-frequency ripple on the ECG baseline may indicate mains interference from nearby equipment. Move the participant away from desktop computers, monitors, or other mains-powered equipment if possible.

---

### Problem 4: SD card error LED (red SD LED) after powering on

**Symptoms:** SD LED turns solid red within 5 seconds of power-on.

**Likely causes and solutions:**

1. **Card not seated.** Power off, remove the card, and reinsert until it clicks.
2. **Card not formatted correctly.** The card must be formatted as FAT32 or exFAT. Use the supplied card. If using a replacement card, format it on a computer before use (Windows: right-click > Format > exFAT; Mac: Disk Utility > Erase > ExFAT).
3. **Card full.** A 32 GB card holds approximately 48 hours of continuous recording. If the card is full, transfer and archive the existing session data and then format the card before reuse.
4. **Card write-protected.** Some microSD cards in adapters have a write-protect switch. Ensure it is not engaged.
5. **Damaged card.** If the same card fails in another device or computer, replace it.

---

### Problem 5: App cannot find the device / BLE connection drops repeatedly

**Symptoms:** The VU-AMS app cannot detect the device during scanning, or the connection establishes but drops every few seconds.

**Likely causes and solutions:**

1. **Bluetooth not enabled on iPhone.** Check that Bluetooth is on in iOS Settings > Bluetooth.
2. **App not granted Bluetooth permission.** Go to iOS Settings > Privacy & Security > Bluetooth and ensure the VU-AMS app is listed and enabled.
3. **Device not advertising.** Confirm the BLE LED is blinking blue. If it is steady or off, the device may already be connected to another phone, or in a fault state. Power cycle the device.
4. **Range or obstruction.** Bluetooth Low Energy has a practical indoor range of 10–15 metres. Metal furniture, thick walls, and other devices operating in the 2.4 GHz band (Wi-Fi, microwave ovens) can reduce range significantly. Move the phone closer to the device.
5. **Another phone is connected.** The VU-AMS connects to one central device at a time. If a colleague's iPhone was previously connected, it may still hold the connection. Ask them to disconnect from their app, or power-cycle the VU-AMS device to clear the existing connection.

---

*For issues not covered in this manual, contact your study coordinator or the technical team.*

---

**Document end — VU-AMS User Manual v0.1**
