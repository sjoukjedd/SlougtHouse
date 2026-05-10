# Electronics Design Brief 001 — Analog Front-End & Electrode Topology

**Issued by:** Jackson Lamb  
**To:** Priya Nair (Electronics)  
**CC:** Dr. Elena Vasquez (Signal Processing), Kai Müller (Firmware)  
**Date:** 2026-05-08  
**Status:** Active

---

## Device placement

The VU-AMS device is worn on the chest, positioned between the collarbones (clavicular / suprasternal region). The device body sits directly on one electrode. That electrode serves a dual purpose:

1. **ECG electrode** — one of the two ECG sense points
2. **ICG upper sense electrode** — the upper voltage sense (Vsense⁺) in the tetrapolar ICG configuration

This is a combined measurement site. The analog front-end must handle both signals simultaneously from this single electrode point.

---

## Signal channels — derived from existing firmware data structures

The existing system defines the following acquired channels. The new hardware must produce all of them.

### A-block (primary physiological, high rate)

| Channel | Type | Unit | Description |
|---------|------|------|-------------|
| ECG1 | Int32 | mV | ECG channel 1 — upper sense electrode (device electrode) |
| ECG2 | Int32 | mV | ECG channel 2 — lower sense electrode |
| Z0 | Int32 | Ω | ICG baseline impedance (DC component of thoracic impedance) |
| V_source | Int32 | mV | ICG current source drive voltage reference |
| Temperature | Double | °C | On-device temperature |

Derived in firmware (software, not hardware):
- `filterdZ0_dZ` — dZ/dt (the ICG waveform, derived by differentiation and filtering of Z0)
- `filterdZ0_Resp` — respiratory component (low-frequency component of Z0)
- `filterdECG1`, `filterdECG2` — filtered ECG signals
- `beat` — R-peak detection flag

### M-block (motion, lower rate)

| Channel | Type | Unit | Description |
|---------|------|------|-------------|
| ax, ay, az | Int16 | g | 3-axis accelerometer |
| φx, φy, φz | Int16 | °/s | 3-axis gyroscope |
| temperatureRaw | Int16 | — | IMU die temperature |

### D-block (barometric, lowest rate)

| Channel | Type | Unit | Description |
|---------|------|------|-------------|
| rawDruk | Int32 | — | Raw pressure ADC |
| rawTemperature | Int32 | — | Raw temperature ADC |

### B-block (battery)

| Channel | Type | Description |
|---------|------|-------------|
| voltageRaw | Int16 | Battery voltage raw ADC |

---

## Electrode topology — what Nair must design around

The device sits on electrode E1 (upper, between collarbones). The full electrode set is as follows:

```
        [DEVICE]
           │
    ┌──────┴──────┐
    │  E1 (upper) │  ← device body sits here
    │             │  ← ECG1 sense point
    └──────┬──────┘  ← ICG Vsense⁺
           │
      (chest, upper)
           │
    E2 (upper current)  ← ICG drive current injection, upper
           │             (on a separate electrode, wired to device)
           │
      (thorax)
           │
    E3 (lower sense)  ← ECG2 sense point + ICG Vsense⁻
           │
    E4 (lower current)  ← ICG drive current injection, lower
```

The GATT profile includes:
- `CBUID_ElectrodeDistance` — electrode distance (RW, string) — confirms the design uses configurable inter-electrode spacing
- `CBUID_electrodeStatus` / `CBUID_LooseElectrodeDetection` — electrode contact quality monitoring is already in the system

**Implication for Nair:** The device connects to E2, E3, E4 via lead wires. The PCB must include:
- Connectors / snap sockets for at least 4 electrode leads (E1 is the device body itself)
- Or: E1 integrated as a conductive patch/pad on the underside of the device housing

---

## ICG analog front-end requirements

ICG uses a tetrapolar configuration:
- **Current injection:** constant-current AC source injected between E2 (upper) and E4 (lower)
- **Voltage sensing:** differential voltage measured between E1 (upper) and E3 (lower)
- **Carrier frequency:** typically 50–100 kHz (sinusoidal, low amplitude)
- **Current amplitude:** typically 1–4 mA RMS (within IEC 60601-1 auxiliary current limits)

The sense voltage contains:
- DC component → Z0 (baseline thoracic impedance, ~20–30 Ω typical)
- AC modulated component (at carrier frequency) → demodulate to get ΔZ (impedance changes due to cardiac and respiratory activity)
- ECG signal (superimposed, must be separated by filtering)

**What Nair must provide:**
- Precision constant-current AC source for ICG drive
- Differential instrumentation amplifier on sense electrodes (high CMRR, >80 dB at carrier frequency)
- Synchronous demodulator (lock-in style) to extract ΔZ from the carrier
- DC path for Z0 measurement
- ECG signal path extracted from same sense electrodes (separate filter branch)

---

## ECG analog front-end requirements

- Two sense points: E1 (upper, device electrode) and E3 (lower sense)
- Standard lead configuration: `ECG1` = E1 referenced, `ECG2` = E3 referenced (or differential between them — Müller to confirm exact ADC input configuration with existing firmware)
- Bandwidth: 0.05–150 Hz (IEC 60601-2-47 diagnostic ECG)
- Protection: defibrillation protection on all patient-connected leads (IEC 60601-1)
- Patient input protection: current-limiting resistors + TVS on all electrode lines

---

## Motion sensor

- IMU: 6-axis (3-axis accelerometer + 3-axis gyroscope)
- Existing data: ±g range and °/s range to be confirmed from firmware configuration
- Interface: SPI or I²C to ESP32 (Müller to advise)
- Placement: on PCB, close to centre of device body for accurate positional reference

---

## Barometric pressure

- Sensor: integrated pressure + temperature (e.g. BMP390 or equivalent)
- Interface: SPI or I²C to ESP32
- Used for altitude tracking and potentially for respiratory artefact correction

---

## Power

- Source: single-cell LiPo battery
- Battery monitoring: raw voltage via ADC to ESP32 (`voltageRaw` B-block channel)
- Requirements: quiet power rails (LDO or charge pump) for analog front-end — switching regulators must not inject noise into ECG/ICG band
- BLE radio and digital section must be decoupled from analog rails

---

## Electrode contact monitoring

The existing GATT profile includes `CBUID_electrodeStatus` / `CBUID_LooseElectrodeDetection`. The hardware must support electrode impedance monitoring or contact detection on all four electrodes, with status bits mapped to the status field in the A-block.

---

## Open questions for Vasquez

Before Nair finalises the analog front-end specification, Vasquez must answer:

1. ICG carrier frequency — what is it in the current hardware? (Müller may know from existing firmware)
2. ICG carrier current amplitude
3. Required ADC resolution and sampling rate for ECG and ICG channels
4. Acceptable noise floor (μV RMS) on ECG input
5. Required dynamic range for Z0 measurement

---

## Next step

Nair: review this brief and raise any questions before beginning schematics. Do not start layout until Vasquez confirms the signal quality requirements and Müller confirms the ADC/interface assumptions.

Müller: review the hardware interface section. Confirm ADC input configuration for ECG1/ECG2 and the existing ICG carrier parameters if known from prior firmware versions.

Vasquez: complete the signal quality requirements section in `intel/interfaces.md` before Nair begins the analog front-end schematic.

---

*Brief 001 — issued.*
