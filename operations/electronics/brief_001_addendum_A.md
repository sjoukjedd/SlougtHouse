# Brief 001 — Addendum A: ICG Specifications & Corrected Electrode Topology

**Issued by:** Jackson Lamb  
**To:** Priya Nair (Electronics)  
**CC:** Kai Müller (Firmware), Dr. Elena Vasquez (Signal Processing)  
**Date:** 2026-05-08  
**Supersedes:** Electrode topology section of Brief 001

---

## ICG — confirmed parameters

| Parameter | Value |
|-----------|-------|
| Carrier frequency | **32 kHz** |
| Injected current amplitude | **1 mA** |
| Measurement range | **0–32 Ω** |
| Output sampling rate | **1 kHz** (demodulated signal) |
| ADC resolution | **24-bit** |
| Calculated LSB | ~1.9 μΩ |

---

## Electrode topology — corrected

Brief 001 assumed a front-only electrode arrangement. That is wrong. The correct configuration is:

```
                     ┌─────────────────────────────┐
                     │         BACK OF BODY         │
                     │                              │
                     │   [E_back_upper]             │
                     │   Neck / nape                │
                     │        │                     │
                     │    Current source ────────── │── (in device, leads to back)
                     │        │                     │
                     │   [E_back_lower]             │
                     │   Lower back                 │
                     └─────────────────────────────┘
                                   │
                          Current flows THROUGH
                             the thorax
                                   │
                     ┌─────────────────────────────┐
                     │        FRONT OF BODY         │
                     │                              │
                     │   [E_front_upper]            │
                     │   Between collarbones        │
                     │   ← DEVICE SITS HERE →      │
                     │   Vsense⁺  │  ECG ref        │
                     │            │                 │
                     │   [E_front_lower]            │
                     │   Solar plexus               │
                     │   Vsense⁻  │  ECG electrode  │
                     └─────────────────────────────┘
```

**Current injection:** Back electrodes only — one at the nape of the neck, one at the lower back.  
**Voltage sensing:** Front electrodes only — device body (collarbone) and solar plexus.  
**ECG:** Measured between the same two front sense electrodes (collarbone ↔ solar plexus). Signal separation from ICG by filtering.

---

## Implications for Nair

### Lead wire count
The device needs connections to **four external electrodes**:

| Lead | Location | Signal |
|------|----------|--------|
| E_front_upper | Device body (integrated) | ICG Vsense⁺, ECG |
| E_front_lower | Solar plexus (front) | ICG Vsense⁻, ECG |
| E_back_upper | Nape of neck (back) | ICG current injection |
| E_back_lower | Lower back (back) | ICG current injection |

The device body is E_front_upper — no separate lead required for that one. Three external lead wires needed minimum: front-lower (solar plexus), back-upper (neck), back-lower (lower back).

### Current source design
- Must drive 1 mA at 32 kHz through the full body impedance path (back → thorax → front)
- Total source impedance seen: electrode contact + thoracic segment ≈ 20–150 Ω typical
- Current source must maintain constant current across this load range
- Galvanic isolation of current source from the sense amplifier is important — the back electrode leads must not inject noise into the front sense inputs
- IEC 60601-1 patient auxiliary current limit at 32 kHz: verify allowable limit (typically 100 μA at DC, but frequency-dependent — 1 mA at 32 kHz may require specific justification under IEC 60601-1 Table 1)

### ICG sense amplifier
- Differential instrumentation amplifier: E_front_upper − E_front_lower
- Input must handle: 32 kHz carrier (amplitude ≈ 1 mA × ~25 Ω ≈ 25 mV) plus DC offset from electrode potentials (can be ±300 mV)
- After amplification: synchronous demodulator at 32 kHz → envelope = Z(t)
- DC component of envelope = Z0 (baseline, 0–32 Ω range)
- AC modulation of envelope = ΔZ (cardiac and respiratory) → dZ/dt by differentiation in firmware

### ECG signal path
- Comes from the same two front electrodes as ICG sense
- The 32 kHz carrier is present on these electrodes — ECG front-end must reject it (notch or high-order low-pass before ECG ADC)
- ECG bandwidth: 0.05–150 Hz — entirely below the 32 kHz carrier, so filtering is straightforward
- Separate ADC input (or separate front-end amplifier chain) from ICG demodulation path

### ADC requirement
- 24-bit ADC for ICG demodulated signal at 1 kHz
- Standard 24-bit sigma-delta ADCs suited for this (e.g. ADS1256, AD7779, or equivalent)
- ECG channel: can share ADC or use separate — coordinate with Müller on ESP32 ADC limitations

### Back lead routing
- Back electrode leads must route from device (chest-front) around or over the body to neck and lower back
- This is a mechanical/wearability question — flag to Voss: electrode harness or garment integration?
- Lead shielding: driven-right-leg circuit or equivalent to reduce common-mode noise on long leads

---

## For Müller

The 24-bit ADC will not be the ESP32's internal ADC (12-bit, noisy). An external sigma-delta ADC is required, communicating with ESP32 via SPI. Nair will specify the part. Müller should confirm available SPI bus capacity and DMA capability for continuous 1 kHz 24-bit streaming.

---

## For Vasquez

Confirm: at 1 mA drive and ~25 Ω thoracic impedance, the sense voltage amplitude at 32 kHz is approximately **25 mV**. Typical ΔZ from cardiac cycle is 0.1–0.5 Ω → ΔV modulation of **0.1–0.5 mV** on the carrier. Confirm these figures are consistent with the planned ICG algorithm inputs. Also confirm the 32 kHz demodulation approach: synchronous (phase-sensitive) demodulation or magnitude-only envelope detection?

---

*Addendum A — issued. Brief 001 + Addendum A together constitute the current electronics design brief.*
