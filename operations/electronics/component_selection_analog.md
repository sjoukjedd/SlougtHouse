# VU-AMS — Analog Signal Chain Component Selection
## Lower PCB (52 × 25 mm), Rev 0.3

**Author:** Priya Nair — Electronics  
**Date:** 2026-05-08  
**Ref:** Brief 001 + Addendum A + Addendum B  
**Status:** Draft — pending Vasquez sign-off on ICG demodulation approach and SCL placement validation

---

## Context

The lower PCB hosts the complete analog signal chain. An ESP32-S3-MINI-1-N8R8 on the upper PCB communicates with the external 24-bit ADC on this board via SPI. All 8 ADC channels are allocated (ch0–ch7: ICG Z0, V_source, ECG1, ECG2, Temp legacy, Vbatt, SCL_tonic, SCL_phasic). The design targets IEC 60601-1 Type BF classification (battery-only, floating patient circuit). ICG carrier is 32 kHz at 1 mA, tetrapolar front/back configuration.

---

## Component Selections

---

### InAmp ECG (×2)

**Selected:** INA333 — Texas Instruments  
**Reason:** The INA333 is a precision, zero-drift, 3-op-amp instrumentation amplifier optimised for biopotential acquisition. Its input-referred noise of 7 nV/√Hz and maximum offset voltage of 25 µV are well-suited to ECG amplitudes of 0.5–5 mV. The device operates from a single 1.8–5.5V supply, eliminating the need for a split rail on the lower PCB. Gain is set by a single external resistor. The 32 kHz ICG carrier appearing on the ECG electrodes is attenuated by the INA333's first-stage common-mode rejection and further suppressed by the downstream ECG low-pass filter; no special high-frequency rejection is required from the ECG InAmp itself. Two instances are used: one per ECG channel (ECG1: E_front_upper referenced to RLD; ECG2: E_front_lower referenced to RLD).  
**Critical specs:**
- Input-referred voltage noise: 7 nV/√Hz @ 1 kHz
- CMRR: 100 dB min (G = 100)
- Supply current: 50 µA typical

---

### InAmp ICG Sense

**Selected:** AD8221AR — Analog Devices  
**Reason:** The ICG sense amplifier must reject the 32 kHz common-mode carrier while amplifying the differential modulation signal (~0.1–0.5 mV ΔV on a 25 mV carrier). The AD8221 achieves 80 dB CMRR at 10 kHz and maintains >70 dB CMRR at 32 kHz — sufficient to reduce the 25 mV common-mode carrier to sub-µV levels before the synchronous demodulator. Its wide bandwidth (825 kHz at G = 1, 562 kHz at G = 10) ensures the 32 kHz carrier signal passes through undistorted for the demodulator to process. The AD8221 has laser-trimmed resistors for gain accuracy and a very low input bias current (5 nA) to prevent interaction with the 47 kΩ electrode protection resistors. Available in SOIC-8 (AD8221AR), fitting the constrained PCB area.  
**Critical specs:**
- CMRR: 80 dB min @ 10 kHz; >70 dB @ 32 kHz (G = 10)
- Input bias current: 5 nA max
- Bandwidth: 562 kHz at G = 10

---

### ICG Demodulator (Synchronous Demodulation, 32 kHz Carrier)

**Selected:** AD630ARZ — Analog Devices  
**Reason:** The AD630 is a precision balanced modulator/demodulator IC designed specifically for synchronous (phase-sensitive) detection. It performs true multiplying demodulation: the ICG sense signal at 32 kHz is multiplied by a reference square wave derived from the same oscillator driving the current source, yielding a DC-proportional output equal to Z(t) after low-pass filtering. This suppresses quadrature noise and out-of-band interference in a way that simple envelope detectors cannot. The AD630 offers ±0.01% gain matching between its two amplifier channels and operates to 2 MHz bandwidth, well above the 32 kHz carrier. The reference input accepts CMOS-level logic directly from the ESP32-S3 or a dedicated oscillator. Post-demodulation, a single-pole RC filter (fc ≈ 2 kHz) passes the envelope to ADC ch0 (Z0) while rejecting the 64 kHz demodulation image.  
**Critical specs:**
- Gain matching: ±0.01% (channel A/B)
- Bandwidth: 2 MHz
- THD: 0.001% at 1 kHz output

---

### RLD Amplifier (Driven Right Leg)

**Selected:** OPA333AIDBVR — Texas Instruments  
**Reason:** The driven right leg (DRL) circuit drives a common-mode signal derived from the ECG/ICG sense electrodes back to the patient via a designated electrode, dramatically reducing common-mode interference. The OPA333 is a zero-drift, rail-to-rail output op-amp with 0.1 µV/°C offset drift and 7 nV/√Hz noise — appropriate for a feedback amplifier handling sub-mV common-mode signals. Its low supply current (17 µA) keeps it compatible with the battery budget. The device is unity-gain stable and available in SOT-23-5 (AIDBVR variant), minimising PCB footprint. The RLD loop gain must roll off well before 32 kHz to avoid oscillation; a series feedback resistor (10 kΩ) and compensation capacitor in the feedback path will be added per standard IEC 60601-1 DRL design practice.  
**Critical specs:**
- Offset voltage drift: 0.1 µV/°C max
- Input-referred noise: 7 nV/√Hz
- Supply current: 17 µA typical

---

### 24-bit ADC (SPI, ≥8 Channels, Low Noise)

**Selected:** ADS1256IDBR — Texas Instruments  
**Reason:** The ADS1256 is a 24-bit sigma-delta ADC with 8 single-ended (4 differential) input channels and SPI interface — matching the exact channel count required. It achieves an effective noise floor of 27 nV RMS at 10 SPS and maintains 21 effective bits at 1 kHz data rate, which is the ICG channel target rate. The input MUX allows the ESP32-S3 to cycle through all 8 channels sequentially. The device includes a programmable gain amplifier (PGA, ×1–64) allowing direct connection of the conditioned ECG signals without external gain stages if required. The SPI interface supports continuous conversion with DRDY interrupt to firmware — consistent with Müller's DMA-based acquisition architecture. Powered from the clean analog 3.3V LDO rail, with digital I/O pins tolerant of the ESP32-S3's 3.3V logic.  
**Critical specs:**
- Resolution: 24-bit, effective noise: 27 nV RMS (10 SPS)
- Data rate: programmable up to 30 kSPS (1 kHz ICG target: ample margin for 8-channel scan)
- PGA: ×1 to ×64

---

### ADC Voltage Reference (2.5V, Ultra-Low Noise)

**Selected:** REF5025IDGKT — Texas Instruments  
**Reason:** The REF5025 is a precision 2.5V buried-Zener voltage reference with 3 µV p-p noise (0.1–10 Hz) and a noise spectral density of 7.2 nV/√Hz — among the lowest available in a compact package. For a 24-bit ADC with LSB of ~149 nV (2.5V / 2²⁴), the reference noise must not degrade the ADC's dynamic performance: at 7.2 nV/√Hz, integrated over 1 kHz bandwidth the reference contributes ~230 nV RMS, which limits effective resolution to approximately 20–21 bits — acceptable for the 1 kHz ICG channel. Initial accuracy is ±0.05% with ±3 ppm/°C temperature coefficient, ensuring Z0 absolute accuracy across body temperature excursions. The device includes an output noise-reduction capacitor pin (FILT) to further attenuate broadband noise with an external 10 µF capacitor.  
**Critical specs:**
- Output voltage noise: 3 µV p-p (0.1–10 Hz); 7.2 nV/√Hz broadband
- Temperature coefficient: ±3 ppm/°C max
- Initial accuracy: ±0.05% (±1.25 mV)

---

### Analog LDO (Ultra-Low Noise, 3.3V Rail)

**Selected:** LT3042EDD#TRPBF — Analog Devices / Linear Technology  
**Reason:** The LT3042 is the industry benchmark for low-noise LDO performance: 0.8 µV RMS output noise (10 Hz–100 kHz), which is approximately 40× lower than standard LDOs. This is critical for the analog rail powering the InAmps, reference, and ADC — any LDO noise couples directly into the sub-mV ECG signal chain. The device uses a current-source architecture rather than a voltage-divider feedback network, achieving a power supply rejection ratio (PSRR) of 75 dB at 1 MHz, effectively isolating the analog rail from switching transients on the Li-Po charge path. Output voltage is set by a single resistor from the SET pin. The LT3042 is rated for 200 mA output, more than sufficient for the analog front-end load (estimated 15–30 mA total).  
**Critical specs:**
- Output noise: 0.8 µV RMS (10 Hz–100 kHz, COUT = 10 µF)
- PSRR: 75 dB at 1 MHz
- Output current: 200 mA max

---

### SCL Excitation and Sense (Skin Conductance)

**Selected:** AD5933YRSZ — Analog Devices  
**Reason:** The AD5933 is an impedance measurement system-on-chip integrating a programmable sine wave generator (100 Hz–100 kHz), a transimpedance amplifier, ADC, and on-chip DFT processor — providing a complete synchronous excitation-and-demodulation chain for SCL/EDA in a single 16-lead SSOP package. For SCL, a low-amplitude excitation sine (≤50 mVpp, programmable) is applied between the Ag/AgCl chest electrodes at a frequency in the 500 Hz–1 kHz range (per Addendum B specification). The AD5933 directly computes real and imaginary impedance components, from which skin admittance (conductance, in µS) is derived. The integrated approach eliminates discrete oscillator, TIA, and demodulator components, saving critical PCB area on the 52 × 25 mm board. The DC results are output via I²C to the ESP32-S3; the analog channels SCL_tonic and SCL_phasic are then produced by post-processing in firmware (low-pass and band-pass decimation of successive impedance measurements) and routed to ADC ch6/ch7 via an output DAC or directly as firmware-computed values. Note: if Vasquez confirms that time-domain analog output to the ADC is preferred over digital impedance streaming, the AD5933 can be replaced by a discrete excitation oscillator + TIA + AD8221-based synchronous demodulator; this decision is flagged as pending.  
**Critical specs:**
- Excitation frequency range: 1 kHz–100 kHz (settable); external clock extends below 1 kHz
- Excitation amplitude: programmable 200 mVpp / 400 mVpp (via external voltage divider to ≤50 mVpp at electrode)
- Impedance measurement accuracy: 0.5% (calibrated)

---

### 32kHz ICG Carrier Oscillator

**Geselecteerd:** SG-210STF 32.000KHz — Epson Timing  
**Reden:** De SG-210STF is een temperatuurgecompenseerde kristaloscillator (TCXO-klasse) in CMOS-uitvoering met een gegarandeerde uitgangsfrequentie van 32.000 kHz (niet 32.768 kHz — zie noot hieronder). Jitter bedraagt 0,3 ps RMS typisch, ruim onder de eis van 1 ns RMS. De oscillator integreert een kristal en oscillatorcircuit in één SMD-package (3,2 × 2,5 mm, 4-pad), wat geen externe last-capaciteiten vereist en de footprint op de 52 × 25 mm PCB minimaliseert. Frequentiestabiliteit is ±50 ppm over 0–70°C (ruimschoots voldoende voor 0–40°C skin-worn); het analoge 3,3V-voedingspunt past direct op de LT3042-rail. Beschikbaar bij Mouser (via Epson-distributeur), DigiKey en Farnell.  
**Kritieke specs:**
- Jitter: 0,3 ps RMS typisch (periode-jitter, CMOS uitgang)
- Voeding: 3,3V ±10%, stroom < 1 mA
- Package: SMD 3,2 × 2,5 mm, 4-pad (passend op 52 × 25 mm PCB)
- Stabiliteit: ±50 ppm, 0–70°C (voldoet ruim aan 0–40°C eis)
- Uitgangsfrequentie: 32.000 kHz (niet 32.768 kHz)

**Noot 32.000 vs 32.768 kHz:** 32.768 kHz (= 2¹⁵ Hz) oscillatoren zijn goedkoper en breder leverbaar (horloge-kristallen), maar introduceren een afwijking van +2,4% ten opzichte van 32.000 kHz. Dit versleept de demodulatiefase in de AD630 niet — zolang referentie en carrier uit dezelfde bron komen is synchrone detectie exact — maar het compliceeert Müller's firmware als hij de carrier-frequentie hardcoded op 32.000 kHz heeft staan (bijv. voor bandbreedte of filtercoëfficiënten). Vasquez heeft de ICG-demodulatieparameters op 32.000 kHz gespecificeerd; gekozen wordt voor 32.000 kHz om afwijking en hervalidatie te vermijden.

**Noot voor Müller:** De SG-210STF levert een CMOS-level blokgolf (VOH ≥ 2,4V, VOL ≤ 0,4V bij 3,3V voeding). Deze kan direct worden aangeboden aan de AD630 referentie-ingang (pin 9/10, CMOS-compatibel). Tevens dient dezelfde klokuitgang via een 33Ω serie-weerstand verbonden te worden met één GPIO van de ESP32-S3 (geconfigureerd als PCNT of RMT capture input) zodat de firmware de aanwezigheid en frequentie van de carrier kan monitoren en de demodulatiefase kan vergrendelen. Gebruik GEEN intern LEDC- of RMT-signaal van de ESP32 als primaire carrier — de beslissing van de principal is expliciet: de oscillator op de onderste PCB is leidend.

---

## Notes and Open Items

1. **SCL architecture decision** (Vasquez): Confirm whether SCL tonic/phasic signals are to be delivered as analog voltages to ADC ch6/ch7 (requiring discrete TIA + demodulator per Addendum B block diagram) or as digital impedance values over I²C (AD5933 native mode). If analog delivery is required, the AD5933 must be replaced by a discrete chain and ch6/ch7 remain analog inputs.

2. **ICG demodulation reference** (Müller): The AD630 reference input requires a phase-stable 32 kHz square wave synchronised to the ICG current source oscillator. Müller to confirm whether this reference is generated by the ESP32-S3 (LEDC timer or RMT peripheral) or a dedicated TCXO/crystal oscillator on the lower PCB.

3. **IEC 60601-1 auxiliary current limit at 32 kHz**: Per Table 1, the limit for AC patient auxiliary current is frequency-dependent. At 32 kHz, interpolation between the 1 kHz (100 µA) and 100 kHz (10 mA) limits suggests a limit of approximately 1 mA is permissible, but this requires explicit regulatory review. Flagged for certification phase.

4. **Second ADC**: All 8 channels are allocated. Any future sensor addition requires either a second ADS1256 on SPI CS2, or an analog multiplexer (e.g. ADG1408) expanding channel count. Decision deferred to Rev 0.4 planning per Addendum B standing order.

---

## Preliminary BOM

| # | Part Number | Description | Manufacturer | Supplier (suggested) |
|---|-------------|-------------|--------------|----------------------|
| 1 | INA333AIDGKT | InAmp, ECG (×2 fitted) | Texas Instruments | Mouser / DigiKey |
| 2 | AD8221ARZ | InAmp, ICG sense | Analog Devices | Mouser / DigiKey |
| 3 | AD630ARZ | Balanced modulator/demodulator, ICG synchronous detection | Analog Devices | Mouser / DigiKey |
| 4 | OPA333AIDBVR | Zero-drift op-amp, RLD amplifier | Texas Instruments | Mouser / DigiKey |
| 5 | ADS1256IDBR | 24-bit SPI ADC, 8-ch | Texas Instruments | Mouser / DigiKey |
| 6 | REF5025IDGKT | 2.5V precision voltage reference | Texas Instruments | Mouser / DigiKey |
| 7 | LT3042EDD#TRPBF | Ultra-low-noise LDO, 200 mA, 3.3V | Analog Devices | Mouser / DigiKey |
| 8 | AD5933YRSZ | Impedance analyzer IC, SCL excitation + sense | Analog Devices | Mouser / DigiKey |
| 9 | SG-210STF 32.000KHz | TCXO-class CMOS oscillator, 32.000 kHz, 3,2×2,5 mm SMD, 3.3V | Epson Timing | Mouser / DigiKey / Farnell |

---

*Document filed: `operations/electronics/component_selection_analog.md`*  
*Next action: Vasquez to confirm SCL architecture (analog vs. digital output). Nair to proceed with schematic capture. Open item 2 (32 kHz reference source) gesloten door beslissing principal: dedicated oscillator SG-210STF op onderste PCB. Müller te informeren over GPIO capture-configuratie.*
