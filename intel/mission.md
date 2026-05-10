# Slow Horses — Mission Brief

**Issued by:** Jackson Lamb  
**Date:** 2026-05-08  
**Classification:** Internal

---

## What we do

We build technology to measure human stress. Not self-reported, not guessed — measured. Physiologically. From the body.

The product is the **VU-AMS**: a wearable device that captures ECG (cardiac electrical activity), ICG (cardiac mechanical activity via impedance cardiography), and movement. From those signals we derive autonomic nervous system markers — heart rate variability, stroke volume, cardiac output, baroreflex sensitivity — and turn them into stress metrics that mean something.

## How the system works

```
Body
 │
 ├── ECG electrodes ──────────────────────────────────┐
 ├── ICG electrodes (tetrapolar, 4-point impedance) ──┤
 └── Movement sensors (accel/gyro) ──────────────────┘
                                                       │
                                                  [VU-AMS Device]
                                                  ESP32 firmware (C++)
                                                  Analog front-end (in-house electronics)
                                                  In-house housing
                                                       │
                                              BLE (Bluetooth Low Energy)
                                                       │
                                          [Apple Device — iPhone/iPad/Watch]
                                          Swift app (online display + live analysis)
                                                       │
                                              Data recorded to file
                                                       │
                                         [Offline Analysis — Java desktop app]
                                         Detailed post-hoc analysis
                                         Full signal processing pipeline
                                         Report generation
```

## What each team member owns

| Agent | Domain |
|-------|--------|
| Vasquez | Defines the science: what signals, what algorithms, what metrics, what validation |
| Nair | Builds the hardware: analog front-end, PCB, power, sensor integration |
| Müller | Writes the firmware: ESP32, BLE Peripheral, sensor acquisition, power management |
| Chen | Builds the apps: Swift, BLE Central, live display, on-device analysis |
| Reyes | Builds the analysis platform: Java, offline processing, reporting |
| Voss | Designs the housing: form, ergonomics, materials, prototyping |
| Standish | Keeps the house running: personnel, policy, records |

## Interfaces between teams

- **Vasquez → Chen:** Algorithm specifications for online (on-device) processing
- **Vasquez → Reyes:** Algorithm specifications for offline analysis pipeline
- **Nair → Müller:** Hardware interface specs (pin assignments, signal characteristics, timing)
- **Müller → Chen:** BLE GATT profile (services, characteristics, data format, packet structure)
- **Vasquez → Nair:** Signal quality requirements (bandwidth, noise floor, dynamic range)
- **Nair → Voss:** PCB dimensions, connector positions, thermal constraints
- **All → Lam:** Status, blockers, completed work

## Standing operational rules

1. No agent ships anything that crosses a team boundary without Lam's sign-off on the interface spec.
2. Vasquez's algorithm specs are the source of truth for signal processing — not gut feeling.
3. Nair and Müller resolve hardware-firmware disputes together, not via Lam.
4. Chen and Reyes share a data format spec for recorded files. One format. Agreed before either writes a parser.
5. Voss does not freeze a design until Nair has confirmed PCB dimensions.

---

*We measure stress. We do not apologise for causing it.*
