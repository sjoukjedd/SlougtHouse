# Slow Horses — Interface Register

**Owner:** Jackson Lamb  
**Last updated:** 2026-05-08 (ICG specs confirmed by principal)

This document tracks all agreed interfaces between teams. Nothing crosses a boundary until it appears here, agreed by both sides.

---

## BLE GATT Profile (Müller ↔ Chen)

| Field | Status |
|-------|--------|
| Service UUIDs | **Pending** |
| Characteristic definitions | **Pending** |
| Data packet format (ECG, ICG, movement) | **Pending** |
| Sampling rates | **Pending** |
| Connection parameters | **Pending** |

*Müller proposes. Chen reviews. Lam signs off.*

---

## Data File Format (Chen ↔ Reyes)

| Field | Status |
|-------|--------|
| File format (binary / HDF5 / other) | **Pending** |
| Header structure | **Pending** |
| Channel layout | **Pending** |
| Timestamp encoding | **Pending** |
| Metadata fields | **Pending** |

*One format. Agreed before either writes a parser.*

---

## Signal Quality Requirements (Vasquez → Nair)

### ICG — CONFIRMED by principal 2026-05-08

| Parameter | Value | Source |
|-----------|-------|--------|
| Carrier frequency | 32 kHz | Principal |
| Injected current | 1 mA | Principal |
| Current injection sites | Neck (back) + lower back | Principal |
| Sense sites | Device electrode (front, between collarbones) + solar plexus (front) | Principal |
| Measurement range | 0–32 Ω | Principal |
| Sampling rate (demodulated) | 1 kHz | Principal |
| ADC resolution | 24-bit | Principal |
| Derived LSB | ~1.9 μΩ (32 Ω / 2²⁴) | Calculated |

**Electrode topology — confirmed (5 electrodes total):**
```
BACK:   E4 [Neck]  ──── ICG I⁺ / RLD ────┐
                                           │ current + RLD path through thorax
        E5 [Lower back] ── ICG I⁻ / RLD ──┘

FRONT:  E1 [Device / collarbone] ── ICG Vsense⁺ · ECG
        E2 [Solar plexus]        ── ICG Vsense⁻ · ECG
        E3 [Lower left rib]      ── ECG only
```
- Current injection from back (E4, E5). Voltage sense from front (E1, E2).
- ECG uses three front electrodes: E1, E2, E3.
- Back electrodes (E4, E5) serve dual purpose: ICG current injection + ECG RLD (right leg drive / driven reference).
- 4 external lead wires: E2 (solar plexus), E3 (lower left rib), E4 (neck), E5 (lower back). E1 is the device body itself.

### ECG

| Signal | Bandwidth | Noise Floor | Dynamic Range | Status |
|--------|-----------|-------------|---------------|--------|
| ECG (front-front lead) | 0.05–150 Hz | **Pending (Vasquez)** | **Pending** | Partial |

*ECG measured between device electrode (collarbone, front) and solar plexus electrode (front). Same electrodes as ICG sense — signal separation by filtering.*

### Motion / Environmental

| Signal | Bandwidth | Noise Floor | Dynamic Range | Status |
|--------|-----------|-------------|---------------|--------|
| Accelerometer | **Pending** | **Pending** | **Pending** | **Pending** |
| Gyroscope | **Pending** | **Pending** | **Pending** | **Pending** |

*Vasquez to complete remaining rows.*

---

## Hardware-Firmware Interface (Nair → Müller)

| Field | Status |
|-------|--------|
| Pin assignments | **Pending** |
| ADC configuration | **Pending** |
| SPI / I2C peripheral map | **Pending** |
| Power rail sequence | **Pending** |
| Interrupt lines | **Pending** |

*Nair leads. Müller reviews.*

---

## Algorithm Specifications (Vasquez → Chen / Reyes)

| Algorithm | Target | Status |
|-----------|--------|--------|
| ECG R-peak detection | Chen (online) + Reyes (offline) | **Pending** |
| HRV time-domain metrics | Reyes (offline) | **Pending** |
| HRV frequency-domain metrics | Reyes (offline) | **Pending** |
| ICG B/C-point detection | Reyes (offline) | **Pending** |
| Stroke volume / cardiac output | Reyes (offline) | **Pending** |
| Movement artefact rejection | Chen (online) + Reyes (offline) | **Pending** |
| Stress index computation | Reyes (offline) | **Pending** |
| Live display processing | Chen (online) | **Pending** |

---

## PCB Constraints (Nair → Voss)

| Field | Status |
|-------|--------|
| Board dimensions | **Pending** |
| Connector positions | **Pending** |
| Mounting points | **Pending** |
| Thermal dissipation zones | **Pending** |

*Nair leads. Voss confirms fit.*
