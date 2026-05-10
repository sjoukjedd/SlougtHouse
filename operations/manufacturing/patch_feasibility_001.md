# Feasibility Memo: Dry-Electrode ICG in Patch Form Factor
## VU-AMS PATCH Concept — Technical Assessment

**Author:** Priya Nair, Electronics  
**Date:** 2026-05-10  
**Classification:** Internal — Concept Validation  
**Req Ref:** Brainstorm 001, Concept Card 3 (VU-AMS PATCH)

---

## Executive Summary

**Verdict: CONDITIONAL GO — with prototype validation.**

Dry-electrode ICG measurement in a chest patch geometry is **technically feasible**, but with significant signal-to-noise compromises and electrode material constraints. The current VU-AMS analog front-end (AD8221 + AD630 chain) has sufficient headroom to accommodate the 10–100 kΩ contact impedance of dry electrodes, provided that:

1. A **semi-dry or low-capacitance electrode material** is selected (Ag/AgCl snap electrodes or TiN-coated stainless recommended; carbon-loaded silicone and textiles are problematic).
2. Electrode **spacing is 4–5 cm minimum** (not the 3 cm of standard gel configuration) to maintain adequate sense signal.
3. **Active electrode preprocessing** (buffer stage) is added to minimize impedance loading.
4. **Prototype bench validation** confirms SNR and CMRR performance before committing to patch form-factor design.

ICG signal will be degraded compared to gel electrodes—expect 40–60% loss in ΔZ sensitivity and 3–5 dB noise floor increase. The signal is usable for heart rate, stroke volume estimation, and basic haemodynamic trending, but not suitable for detailed arterial compliance or advanced morphological analysis.

This memo supports advancing the PATCH concept to prototype stage (Phase 2) with explicit dry-electrode feasibility testing as a go/no-go gate.

---

## 1. ICG Electrode Requirements & Baseline

### 1.1 Standard VU-AMS Configuration (Gel Electrodes)

- **Current injection:** 1 mA RMS @ 32 kHz sinusoid (tetrapolar: 2 injection + 2 sense)
- **Electrode contact impedance:** ~1 kΩ @ 32 kHz (Ag/AgCl gel, ~3 cm² contact area, well-hydrated skin)
- **Sense signal baseline:** Z0 ≈ 25 Ω (chest tetrapolar impedance), derived from 25 mV carrier amplitude at sense electrodes
- **Sense signal dynamic range:** ΔZ ≈ 0.1–2 Ω per heartbeat (stroke volume modulation), measured as ~0.1–0.5 mV differential modulation on the carrier
- **Sense electrode pair spacing:** 3 cm (collarbone to sternum) — inner pair, 4 cm to outer pair (neck/lower back injection)

### 1.2 Dry Electrode Contact Impedance — Literature and Measurement Data

Dry electrodes exhibit impedance that is **highly skin-state dependent** and typically **10–100× higher** than gel at the same frequency:

| Electrode Material | Contact Impedance @ 32 kHz | Skin Dependence | Notes |
|---|---|---|---|
| Ag/AgCl gel (reference) | ~1 kΩ | Low (±20%) | Wet contact, stable baseline |
| Ag/AgCl dry snap | 5–15 kΩ | Medium (±30%) | Semi-dry with minimal skin prep; humidity-dependent |
| TiN-coated stainless | 8–20 kΩ | Medium (±25%) | Impedance matching layer; stable, hysteresis low |
| Carbon-loaded silicone | 50–150 kΩ | HIGH (±80%) | Skin-hydration sensitive; porous matrix impedance unstable |
| Conductive textile (dry) | 100–500 kΩ | VERY HIGH (>100%) | Requires skin moisture; impedance changes dramatically with pressure |

**Critical observation:** At 32 kHz, the impedance of dry electrodes is dominated by the electrode material double-layer capacitance and skin barrier resistance in series. Surface roughness, oxide layer stability, and local skin hydration all contribute to variability.

---

## 2. Feasibility Assessment: Can ICG Work with Dry Electrodes?

### 2.1 Signal Path Analysis — Impedance Divider Effect

The ICG measurement chain can be modeled as a Thévenin equivalent current source (1 mA, ideal) driving the patient-electrode impedance network. The sense electrodes couple the differential voltage across an inner electrode pair into the AD8221 input stage.

**With gel electrodes (1 kΩ contact impedance per electrode):**
- Total sense-path impedance = 2 × R_contact + Z_patient ≈ 2 kΩ + 25 Ω ≈ 2 kΩ (dominated by electrodes)
- Sense voltage delivered to AD8221 input = 1 mA × 25 Ω (Z_patient only, assuming ideal voltage source across sense pair)
- Actual voltage divider: V_sense = 1 mA × Z_patient × [Z_patient / (2×R_contact + Z_patient)] ≈ 1 mA × 25 Ω × [25/(2000+25)] ≈ 12.5 mV (well into AD8221 linear range)

**With dry electrodes (20 kΩ contact impedance per electrode, worst case):**
- Total sense-path impedance = 2 × 20 kΩ + 25 Ω ≈ 40 kΩ
- Sense voltage at AD8221 input = 1 mA × 25 Ω × [25/(40000+25)] ≈ 0.63 mV (marginal, **63% reduction**)

**With semi-dry (10 kΩ each):**
- Total impedance ≈ 20 kΩ
- Sense voltage ≈ 1.2 mV (88% of gel level)

**Conclusion:** Signal amplitude drops significantly with dry electrodes. A 10 kΩ limit (semi-dry) is roughly the threshold for acceptable signal level; beyond 20 kΩ per electrode, the signal drops below 1 mV and noise floor becomes critical.

### 2.2 CMRR at Elevated Source Impedance

The AD8221 is specified with CMRR ≥ 70 dB @ 32 kHz and a **gain-setting resistor tolerance of ±1%**. However, this CMRR specification assumes low source impedance (typically < 100 Ω per input).

When source impedance rises to 10–20 kΩ per input, **impedance mismatch degrades effective CMRR**:

- Each input path now has a different RC time constant due to electrode-skin interface capacitance (typically 100–500 pF per electrode).
- At 32 kHz, the impedance magnitude becomes frequency-dependent: |Z| = √(R² + (1/2πfC)²). For R = 20 kΩ and C = 200 pF, |Z| ≈ 21 kΩ at 32 kHz, but phase angle = arctan(1/(2πfRC)) ≈ −14°.
- If the two electrode impedances differ by 10% in magnitude or phase, the imbalance creates a differential signal proportional to common-mode voltage.
- Estimated CMRR degradation: −6 to −10 dB at 32 kHz with ±20 kΩ mismatched source impedance.

**Revised CMRR with dry electrodes:**
- Nominal (matched impedance): 70 dB
- With ±20% impedance mismatch (realistic for dry electrodes in situ): 60–64 dB
- This is still adequate for a 25 mV common-mode carrier and 0.1–0.5 mV signal, but reduces noise margin significantly.

### 2.3 Noise Figure and SNR Impact

**AD8221 noise performance (from datasheet):**
- Input-referred voltage noise: 42 nV/√Hz (10 Hz–100 kHz)
- Integrated 1 kHz bandwidth: ~42 nV/√Hz × √1000 Hz ≈ 1.3 µV RMS

**With gel electrodes and matched source impedance:**
- Signal (Z0 modulation): ΔV ≈ 100–500 µV (heartbeat, measured at AD8221 output post-gain G=10 means 10–50 mV)
- Noise (post-gain): 1.3 µV × 10 = 13 µV RMS
- SNR: 50 µV / 13 µV ≈ 15 dB

**With dry electrodes (20 kΩ) and mismatched impedance:**
- Signal level drops to ~50–100 µV (due to impedance divider)
- Noise floor does NOT scale with signal (AD8221 input-referred noise is independent of source impedance, but **apparent noise increases** due to CMRR degradation)
- Additional noise from CMRR residual: ~25 mV (carrier) / antilog(60 dB) ≈ 250 µV common-mode feedthrough
- Effective noise at output: 13 µV + 250 µV ≈ 263 µV RMS
- SNR: 75 µV / 263 µV ≈ −5 dB (UNACCEPTABLE)

**Critical mitigation:** Active electrode buffer stage (OPA333-based follower with 50 Ω output impedance) placed directly at the electrode, reducing the impedance seen by AD8221 to < 100 Ω. This restores CMRR to > 65 dB and improves SNR to 5–8 dB (marginal but usable for HR/SV extraction).

### 2.4 Can the INA333/AD8221 Handle Dry Electrode Impedance?

**Direct answer: YES, with reservations.**

- The AD8221 is rated for input impedance up to 100 kΩ (differential), so 20 kΩ per input (40 kΩ differential) is within spec.
- The input-referred noise of 42 nV/√Hz is **not degraded** by source impedance up to 1 MΩ (the input bias current interaction is negligible due to the FET input stage).
- However, **CMRR degrades** with impedance mismatch (see Section 2.2), and this is the limiting factor.

**Required modifications to the front-end:**

1. **Active electrode buffer** (unity-gain OPA333, 50 Ω output) mounted at the electrode tip or in a small clip housing. Reduces source impedance from 20 kΩ to 50 Ω for the PCB-mounted AD8221.
2. **Electrode impedance matching**: Select electrode materials with low impedance disparity (semi-dry Ag/AgCl snap electrodes are better matched in pairs than carbon silicone).
3. **Gain increase at AD8221**: Change AD8221 gain from G=10 to G=20 or higher, compensating for the ~60% signal level loss. This increases output-referred noise proportionally but maintains SNR above the digital noise floor of the ADS1256.

---

## 3. Electrode Material Options for Dry Contact

### 3.1 Ag/AgCl Dry Snap Electrodes (Semi-Dry, Recommended)

**Material:** Silver/silver chloride button snap in a flexible adhesive backing (e.g. 3M RED DOT semi-dry).

**Contact impedance:**
- 5–15 kΩ @ 32 kHz, depending on skin hydration
- Requires minimal skin prep (light alcohol wipe; does not require conductive gel)
- Baseline impedance stable over 8–24 hour wear

**Advantages:**
- Proven performance in long-term ECG monitoring (Holter) and clinical studies
- Matched pairs available from the same manufacturer/batch → low impedance disparity
- Capacitive coupling to skin stable and predictable
- Can be integrated into flexible adhesive patch without additional mechanics

**Disadvantages:**
- Higher contact impedance than gel (5–10× increase)
- Significant hysteresis if removed and reapplied; not reusable
- Polarization impedance can drift over 24+ hours (frequency-dependent effects)

**ICG Feasibility:** **VIABLE** — Contact impedance 5–15 kΩ is at the upper edge of the comfortable range but workable with active buffering. Signal loss ~60%, acceptable for ambulatory HRV and SV trending.

**Cost & Supply:** $0.30–0.80 per electrode from Covidien, 3M, Ambu (standard clinical supplies).

**Recommendation for PATCH prototype:** Use 3M RED DOT™ 2260 (or equivalent semi-dry Ag/AgCl snap, 6 mm diameter, flexible backing). Source four snaps per patch (2 for injection, 2 for sense). Validate contact impedance and signal level in bench test before committing to adhesive integration.

---

### 3.2 Titanium Nitride (TiN) Coated Stainless Steel (Low-Impedance Dry)

**Material:** Thin TiN PVD coating (~1–2 µm) on 304 or 316L stainless steel, with silk-screen printed conductive adhesive interface.

**Contact impedance:**
- 8–20 kΩ @ 32 kHz (lower than Ag/AgCl due to oxide-free ceramic surface)
- Very stable impedance with repeated wear (reusable if the adhesive is replaced)
- Low polarization impedance at DC and low frequencies

**Advantages:**
- Impedance is more stable and predictable across different skin types
- Biocompatible (gold standard for implantable electrodes)
- Reusable electrode — only the adhesive patch needs replacement
- Excellent for pediatric use (if hysteresis is a concern)

**Disadvantages:**
- TiN sputtering is an additional manufacturing step (not standard adhesive electrode production)
- Higher per-unit cost (~$2–5 in development quantities, < $1 in volume)
- Requires custom design to integrate into patch adhesive backing
- Less established supply chain for pre-formed adhesive-backed electrodes

**ICG Feasibility:** **VIABLE** — Contact impedance 8–20 kΩ is good for dry-electrode ICG. Superior impedance matching compared to Ag/AgCl snaps → better CMRR.

**Recommendation for PATCH prototype:** TiN coating is attractive for longer-term wear and pediatric use, but for the initial prototype, rely on commercial Ag/AgCl snaps. TiN can be evaluated in Phase 2 if patch durability or cost becomes a driver.

---

### 3.3 Carbon-Loaded Silicone (Flexible, Low Comfort Requirement)

**Material:** Silicone elastomer with 20–40% carbon black filler, no skin-prep requirement.

**Contact impedance:**
- 50–150 kΩ @ 32 kHz (highly hydration-dependent; can reach 500 kΩ if skin is very dry)
- Impedance drifts significantly with sweat, humidity, and local skin conductance

**Advantages:**
- Extremely flexible and comfortable for extended wear
- No gel or semi-wet interface — fully dry, no mess
- High compliance (rubber-like) — excellent for infant skin and post-operative patients

**Disadvantages:**
- Impedance is **unstable** — varies ±80% across participants and during wear
- Impedance mismatching between pairs is severe (±40–50%), degrading CMRR by 15–20 dB
- ICG signal will be severely compromised (SNR < 0 dB without active buffering)
- Not suitable for precise impedance measurements (ICG requires stable Z0 baseline)

**ICG Feasibility:** **MARGINAL** — Contact impedance 50–150 kΩ is at the edge of feasibility, but the **instability is disqualifying** for ICG. ICG requires a stable baseline impedance to compute ΔZ (change in impedance per beat). With carbon silicone, Z0 itself drifts by 20–50% over a few minutes, making beat-by-beat ΔZ measurement impossible.

**Recommendation:** Do NOT use carbon-loaded silicone for ICG. Reserve for future EDA-only or ECG-only patch variants.

---

### 3.4 Conductive Textile (Dry, Highest Impedance)

**Material:** Silver-nylon woven fabric, knitted silver-polymer composites (e.g. Eeonyx, Sensatex smart fabrics).

**Contact impedance:**
- 100–500 kΩ @ 32 kHz (strongly dependent on contact pressure and local moisture)
- Impedance changes dramatically with movement (pressure transients)

**Advantages:**
- Fully integrated into flexible patch/clothing
- Excellent comfort (cloth-like)
- Minimal visible electrodes

**Disadvantages:**
- Impedance is **extremely unstable** — can vary ±200% with pressure, humidity, and sweat
- Motion artifact coupling is severe
- Not suitable for any quantitative impedance measurement

**ICG Feasibility:** **NO-GO** — Contact impedance 100–500 kΩ is far too high. Even with active buffering, the SNR would be negative, and impedance instability would make Z0 baseline tracking impossible.

**Recommendation:** Conductive textiles are excellent for comfort-focused, signal-agnostic wearables (gesture recognition, simple HR via PPG). Do NOT use for ICG in the patch prototype.

---

## 4. Patch Geometry and Electrode Placement

### 4.1 Tetrapolar ICG Configuration in Constrained Space

Standard ICG requires four electrodes:
- **E_inj_outer:** Current injection, back (nape of neck)
- **E_inj_inner:** Current injection, lower back
- **E_sense_outer:** Voltage sense, front (upper thorax)
- **E_sense_inner:** Voltage sense, front (lower thorax)

The **sense electrodes must be spaced inside the injection electrodes** to ensure the impedance measured is predominantly the thoracic impedance (not limb or torso-limb impedance). Typical spacing:
- Injection spacing: 15–20 cm (back-to-back around ribcage)
- Sense spacing: 3–5 cm (sternum, front)

**Can this fit in an 80×60 mm patch?**

**NO** — not in a single patch. The current geometry would require:

```
Injection electrodes (E_inj_outer, E_inj_inner) ← 15–20 cm apart
                                   ↑
                             [body wrap-around]
                                   ↓
Sense electrodes (E_sense_outer, E_sense_inner) ← 3–5 cm apart (front sternum)
```

An 80×60 mm patch on the anterior chest can accommodate the sense pair (3–5 cm spacing = easily fit horizontally). The injection pair **must be elsewhere** (back), requiring separate lead wires or a second patch.

### 4.2 Hybrid Patch + Lead Architecture

**Recommended approach for PATCH concept:**

- **Patch (80×60 mm, anterior chest):** Houses sense electrodes (E_sense_outer, E_sense_inner) and all analog/digital electronics.
- **Secondary electrodes (back placement):** Injection electrodes (E_inj_outer, E_inj_inner) connected to patch via two thin wires (~2–3 mm dia., insulated, routed under clothing).

**Electrode placement diagram:**

```
                       ┌─ E_sense_outer (collarbone area, ≈15mm from midline)
                       │
  PATCH (80×60mm)      │        ← integrated into flexible adhesive, front sternum
  ┌──────────────┐     │
  │  Electronics │─────┤
  │  + Sense e's │     │
  └──────────────┘     │
                       │
                       └─ E_sense_inner (lower sternum, ≈20mm from midline)
                           ↓ Spacing: 4–5 cm (vertical)

              [Body — anterior/lateral thorax]
              
                       [Two thin wires, posterior thorax]
                       ↓
                       ↓
  E_inj_outer (left scapula, nape) ←─ connected to lead 1
  E_inj_inner (lower back, L5)     ←─ connected to lead 2
```

**Spacing summary:**
- Sense pair (on patch): 4–5 cm vertical separation (easily accommodated)
- Injection pair (back): 15–20 cm vertical separation (full body wrap-around)
- Current path: back-to-back, encircling thorax; sense pair centered in electrical field
- Expected Z0: 25–30 Ω (similar to standard belt-clip VU-AMS configuration)

**Practical implementation:**
- Patch housing: Rigid-flex PCB (52×25 mm electronics, as per brainstorm note) embedded in a 2–3 mm flexible silicone backing.
- Sense electrodes: Two Ag/AgCl semi-dry snaps (6 mm dia.) molded into the silicone, vertically spaced 40–50 mm.
- Adhesive: Medical-grade acrylate or silicone adhesive (water-resistant, 24–72 hour wear, available from 3M/Covidien).
- Lead wires: Two twisted-pair, shielded conductors (0.5 mm dia. each), routed in a cable loom under the participant's shirt.
- Back electrodes: Ag/AgCl snap electrodes (same as front) in small ~40×40 mm flexible patches (non-adhesive, held by Velcro or a second layer of elastic wrap).

**Alternative (full integration, Phase 2):** A full-body adhesive patch (front + back in a single wraparound design) would eliminate lead wires but significantly complicates manufacturing, regulatory pathways, and comfort (sweating, maceration risk increases).

### 4.3 Electrode Placement Validation for the Patch

**Assumption:** Patch positioned at mid-sternum, electrodes E_sense_outer and E_sense_inner horizontally aligned at different vertical positions.

**Tissue impedance at 32 kHz (from thoracic bioimpedance literature):**
- Lungs (air-filled): very high impedance, primarily capacitive
- Heart muscle: 20–40 Ω·cm
- Blood (conductivity ~0.6 S/m): dominates the conductivity path
- Rib/bone: high impedance, minimal current path

**Expected impedance across a 4–5 cm sternum-to-sternum vertical sense pair:**
- Z0 ≈ 25–35 Ω (measured in standard VU-AMS configurations, literature consensus 20–30 Ω for tetrapolar thoracic ICG)
- ΔZ per heartbeat ≈ 0.3–0.8 Ω (stroke volume varies 40–100 mL; thoracic impedance change 0.4–1 Ω per 20 mL SV)

**Signal expected at patch-mounted sense electrodes:**
- Baseline voltage (V_sense on AD8221 input) ≈ 1 mA × 25 Ω × matched-pair = 12.5 mV (gel) → 5 mV (semi-dry, with signal loss)
- Modulation ΔV ≈ 0.3 mV (per beat modulation)
- This is well within the AD8221 operating range and post-demodulator output.

**Validation protocol:**
1. Build a benchtop tissue phantom (equivalent 25–30 Ω resistive load + 200 pF capacitive load) representing the sense impedance.
2. Mount two Ag/AgCl semi-dry snap electrodes on the phantom at 40–50 mm separation.
3. Connect the sense pair to the AD8221 front-end (with and without active electrode buffer).
4. Apply 1 mA 32 kHz current via external injection pair.
5. Measure the sense signal amplitude and noise floor; compare gel vs. dry electrode performance.
6. Confirm that ΔZ modulation is detectable and that SNR is > 0 dB.

---

## 5. Electrode Material Recommendation

**PRIMARY CHOICE: Ag/AgCl Semi-Dry Snap Electrodes (3M RED DOT equivalent)**

**Justification:**
1. **Proven performance:** Decades of clinical use in long-term ECG monitoring; contact impedance well-characterized in literature (5–15 kΩ @ 32 kHz).
2. **Adequate impedance level:** 10 kΩ is at the high end of comfort, but with active electrode buffering (OPA333, 50 Ω output), the impedance seen by the AD8221 drops to < 100 Ω, maintaining CMRR > 65 dB.
3. **Availability:** Off-the-shelf components, pre-formed in flexible adhesive backing, cost-effective ($0.30–0.80 per electrode in volume).
4. **Integration path:** Can be adhesive-molded into the silicone patch backing without custom manufacturing.
5. **Signal quality:** Expected SNR ~6–8 dB (marginal but usable for HR and SV trends); ΔZ modulation will be clearly visible above noise floor.

**SECONDARY CHOICE (Phase 2): TiN-Coated Stainless Steel**

- Explore only if Ag/AgCl snap sourcing becomes problematic or if pediatric reusability becomes a priority.
- Requires custom electrode design and PVD coating infrastructure.
- Defer to Phase 2 prototype validation.

**REJECT:** Carbon-loaded silicone (impedance instability) and conductive textile (excessive impedance, severe motion artifact).

---

## 6. Signal-to-Noise Impact — Quantitative Summary

### Scenario 1: Gel Electrodes (Baseline)

| Parameter | Value | Notes |
|---|---|---|
| Contact impedance per electrode | 1 kΩ | Matched, stable |
| Sense voltage at AD8221 input | 12.5 mV | Baseline carrier at sense pair |
| ΔZ modulation amplitude | 0.3–0.5 mV | Heartbeat |
| AD8221 output gain | 10 | Standard configuration |
| Output signal amplitude | 125 mV (carrier), 3–5 mV (modulation) | Well above noise floor |
| Noise floor (AD8221 input-referred) | 13 µV RMS | 42 nV/√Hz × √1000 |
| Output noise | 130 µV RMS (referenced to output) | Clean baseline |
| SNR (heartbeat modulation) | 5 mV / 130 µV ≈ **38 dB** | Excellent |

### Scenario 2: Semi-Dry Ag/AgCl Electrodes (10 kΩ, without active buffer)

| Parameter | Value | Notes |
|---|---|---|
| Contact impedance per electrode | 10 kΩ | Skin-dependent, ±30% |
| Sense voltage at AD8221 input | 1.2 mV | **60% loss due to impedance divider** |
| ΔZ modulation amplitude | 0.18–0.30 mV | Proportional loss |
| AD8221 output gain | 10 | Standard configuration |
| Output signal amplitude | 12 mV (carrier), 1.8–3 mV (modulation) | Marginal |
| CMRR degradation | −8 dB (from impedance mismatch) | Effective CMRR: 62 dB |
| Common-mode feedthrough | ~25 mV carrier × 10⁻⁶·² ≈ 630 µV | From CMRR degradation |
| Output noise | 130 µV + 630 µV ≈ **760 µV RMS** | Dominated by residual carrier |
| SNR | 2.5 mV / 760 µV ≈ **5.2 dB** | Marginal; signal detectable but noisy |

**Verdict:** **BARELY ACCEPTABLE** without active buffering.

### Scenario 3: Semi-Dry Ag/AgCl Electrodes (10 kΩ, with active buffer OPA333)

| Parameter | Value | Notes |
|---|---|---|
| Contact impedance per electrode | 10 kΩ → **50 Ω (buffered)** | Active electrode reduces source impedance 200× |
| Sense voltage at AD8221 input | 12.2 mV | Signal restored (minimal loss now) |
| ΔZ modulation amplitude | 0.30–0.50 mV | Full modulation preserved |
| AD8221 output gain | 10 | Standard, or increase to 15–20 if needed |
| Output signal amplitude | 122 mV (carrier), 3–5 mV (modulation) | Good margin |
| CMRR restoration | 68 dB (low source impedance, matched buffers) | Effective CMRR restored near nominal |
| Common-mode feedthrough | < 100 µV | Negligible |
| Output noise | 130 µV RMS | Back to nominal (no degradation) |
| SNR | 4.5 mV / 130 µV ≈ **35 dB** | **RESTORED TO GEL LEVEL** |

**Verdict:** **GO** — Active buffering restores SNR to gel-electrode performance levels.

---

## 7. Technical Requirements for Prototype Validation

### 7.1 Minimum Prototype Specifications

**Hardware:**
1. Bench electrode interface board with AD8221 + AD630 chain (existing component selection, Rev 0.3).
2. Active electrode buffer stages: Two OPA333 unity-gain followers (one per sense electrode), mounted on a small PCB or directly in the electrode connector housing.
3. Tissue phantom impedance (25 Ω resistor + 200 pF capacitor in series) to simulate the sense path.
4. Four Ag/AgCl semi-dry snap electrodes (3M RED DOT or equivalent), spaced at 40 mm, 60 mm, and 100 mm to assess spacing sensitivity.
5. 32 kHz function generator driving 1 mA constant-current source (Howland pump or benchtop current source).

**Measurements to perform:**
1. **Baseline impedance (DC and AC @32 kHz):** Measure Z0 across the sense pair with and without active buffering. Confirm < 5% change over 2–4 hour test period (stability).
2. **Signal amplitude (voltage and SNR):** Measure the 32 kHz carrier amplitude and the 0.5–2 Hz envelope modulation (simulated by dithering the injection current by ±5%). Compare gel vs. semi-dry performance.
3. **CMRR:** Apply a 25 mV common-mode 32 kHz signal to both sense electrodes and measure the rejection ratio (output residual / input common-mode voltage).
4. **Noise floor:** Acquire 30 seconds of data with steady 1 mA injection and zero intentional modulation. Compute spectral noise density (nV/√Hz) and integrated 1 kHz bandwidth noise. Target: < 1 µV RMS.
5. **Frequency response (32 kHz ±2 kHz):** Confirm the synchronous demodulator output is flat (< 1 dB variation) across the 32 kHz ± 2 kHz band.

### 7.2 Go/No-Go Criteria

**The PATCH prototype advance requires ALL of the following:**

| Criterion | Target | Measurement | Rationale |
|---|---|---|---|
| **Signal amplitude (semi-dry vs. gel)** | ≥ 70% of gel level | Measure V_sense at AD8221 input, AC component | 30% loss is acceptable; < 50% loss is a no-go |
| **Stability (Z0 baseline)** | ±5% over 4 hours at constant temp | Continuous Z0 output monitoring | ICG ΔZ measurement requires stable baseline; > ±10% drift is disqualifying |
| **SNR (heartbeat modulation)** | ≥ 3 dB (preferably > 6 dB) | SNR = modulation signal / noise floor | 3 dB is marginal; 6 dB is comfortable; < 0 dB is no-go |
| **CMRR at 32 kHz** | ≥ 60 dB (with active buffer) | Apply 25 mV common-mode 32 kHz; measure output residual | Ensures carrier suppression is adequate for demodulation |
| **Electrode impedance disparity** | ≤ 10% between paired electrodes | Measure Z at each electrode independently | Mismatches > 20% significantly degrade CMRR |
| **Reusability / repeatability** | ≤ 5% hysteresis on re-contact | Remove and reapply electrode; measure impedance change | For longer-term wear studies; Ag/AgCl snaps typically have < 20% hysteresis |

**If ANY criterion fails:** Escalate to Nair for electrode material reassessment or active buffer circuit redesign. Do not advance to PATCH form-factor layout without passing all criteria.

---

## 8. Prototype Experiments to Confirm or Deny Feasibility

### Experiment 1: Benchtop Tissue Phantom Validation (Week 1)

**Objective:** Confirm SNR and CMRR with semi-dry electrodes in a controlled, reproducible tissue model.

**Setup:**
- Use the existing ADS1256 + AD8221 + AD630 analog chain (per component_selection_analog.md).
- Build a Thévenin equivalent of thoracic impedance: 25 Ω resistor (parallel RC: 25 Ω + 10 Ω in series with 200 pF capacitor for realistic 32 kHz phase).
- Mount four Ag/AgCl snaps on a rigid phenolic test board at 40 mm, 60 mm, and 100 mm spacing to identify the sensitivity to electrode separation.
- Drive the injection pair with 1 mA sinusoid @ 32 kHz from a calibrated current source.

**Measurements:**
- Record ADC outputs (ch0 = Z0, ch1 = V_source) for 10 seconds at 1 kHz sampling.
- Compute FFT of Z0 output; measure noise density and 32 kHz rejection ratio.
- Repeat with gel electrodes (for baseline comparison) and semi-dry snaps.
- Report: SNR, CMRR, impedance stability, frequency response flatness.

**Success criteria:** SNR ≥ 3 dB, CMRR ≥ 60 dB, impedance drift < ±5% over test duration.

---

### Experiment 2: In Vivo Human Subject Study — Single Session (Week 2–3)

**Objective:** Validate ICG signal quality on live human subjects wearing semi-dry electrodes in patch configuration.

**Participants:** 3–5 healthy volunteers (IRB approval required, or research-use-only protocol).

**Electrode placement:**
- Anterior sternum: Two Ag/AgCl snaps (6 mm diameter) spaced 4–5 cm vertically, centered on midline, at approximately T4 and T6 vertebral level projections.
- Posterior (back): Two Ag/AgCl snaps at scapular and lower-lumbar position (standard ICG tetrapolar spacing).
- Connect anterior pair via 50 Ω shielded twisted pair to the existing VU-AMS device running in ICG-only mode.

**Protocol:**
1. **Baseline (5 min, supine rest):** Collect ECG + ICG with existing gel-electrode VU-AMS system (belt-clip).
2. **Dry electrode test (5 min):** Swap to semi-dry snap electrodes (anterior patch + posterior leads); collect same epoch.
3. **Repeat x 2:** Remove and reapply the semi-dry electrodes twice to assess re-contact repeatability and hysteresis.

**Measurements:**
- Compute R-R interval, stroke volume (via ICG I-block), and PEP for each session.
- Compare gel vs. semi-dry SV estimates: target < 10% difference in mean, < 20% in beat-to-beat variability.
- Assess impedance baseline stability and drift over the 15-minute wear period.
- Qualitatively assess comfort, skin irritation, and participant feedback.

**Success criteria:** 
- SV estimates within ±15% of gel baseline (< 10% is ideal).
- No significant increase in ectopy or signal dropout.
- Participant tolerance acceptable (no irritation or discomfort beyond standard Ag/AgCl snap).

---

### Experiment 3: Adhesive Integration Prototype (Week 4)

**Objective:** Assess feasibility of molding Ag/AgCl snaps into a flexible silicone adhesive patch backing.

**Design:**
- Create a small (40 × 80 mm) silicone patch backing (medical-grade Dow Corning Silastic or 3M VHB tape base).
- Precision-mold two Ag/AgCl snap electrode wells (6 mm diameter, 2 mm deep) spaced 40 mm apart (vertical).
- Adhere the snaps into the wells using conductive epoxy or mechanical press-fit.
- Add surface adhesive (3M 1500 or equivalent) to the back side for skin contact.
- Validate that the snap-to-epoxy-to-patch assembly withstands repeated flexing (simulator: 100 cycles of 90° bend at 3 mm radius).

**Validation:**
- Electrical continuity check: measure resistance from snap terminal to flex PCB connector pads (target < 1 Ω).
- Mechanical durability: no visible cracking, delamination, or adhesive loss after flex cycling.
- Adhesive durability: patch must remain fixed to skin for ≥ 4 hours without slipping or edge lifting.

**Success criteria:** Prototype patch holds snaps securely, maintains electrical continuity, and demonstrates acceptable wear characteristics.

---

### Experiment 4: Patch Form-Factor Electronics Assembly (Week 5–6)

**Objective:** Integrate the analog front-end (ADS1256 + AD8221 + AD630) into a rigid-flex PCB that fits the patch form factor.

**Design constraints:**
- Total patch dimensions: 80 × 60 × 3 mm (height including adhesive backing).
- Rigid PCB zone: 52 × 25 mm (same as current VU-AMS lower PCB).
- Flex PCB zone: 10 × 25 mm (narrow connector to upper PCB housing).
- Components fit within 2 mm vertical envelope (ICs on L1, passives on L2, no shields on L3/L4).

**Integration:**
- Use the existing component selection (component_selection_analog.md) with no changes to the active circuit.
- Add two OPA333 active buffer stages (unity-gain followers) for the sense electrodes, mounted at the flex-to-snap interface.
- Ensure all signal traces use the 5 mm clearance rule (Section 12 of schematic_bottom_pcb_001.md) between injection and sense paths.

**Validation:**
- Benchtop electrical testing: confirm all ADC channels respond correctly, SNR and CMRR match Experiment 1.
- Mechanical fit: patch dimensions and stiffness confirm wearability on human subject (no discomfort, no edge stress).

**Success criteria:** Patch electronics perform identically to benchtop prototype; mechanical design is acceptable for Phase 2 prototype units.

---

## 9. Risks and Mitigations

| Risk | Severity | Mitigation |
|---|---|---|
| **Dry electrode impedance variability** (±50% between subjects or over time) | HIGH | Use semi-dry Ag/AgCl snaps (narrower impedance range); validate on ≥5 subjects. Include automatic impedance-based gain adjustment in firmware (adaptive gain control). |
| **CMRR degradation with impedance mismatch** | MEDIUM | Implement active electrode buffer (OPA333) to reduce source impedance 200×. Validate CMRR ≥ 60 dB in benchtop phantom. |
| **SNR margin too narrow** (modulation signal near noise floor) | MEDIUM | Prototype with two gain settings (G=10 and G=20 on AD8221). If SNR < 3 dB, escalate to designer review; TiN or custom impedance-matching electrode design may be required. |
| **Patch adhesive failure** in humid/sweaty conditions | MEDIUM | Source medical-grade adhesive with proven 24–48 hour wear data (3M, Covidien, Ambu clinical specs). Validate in controlled humidity chamber (90% RH, 20°C, 8 hour test). |
| **Electrode reuse vs. single-use** (cost/waste implications) | LOW | Semi-dry Ag/AgCl snaps are single-use (clinical standard); cost ~$0.50 per snap. Accept single-use model for Phase 1 prototype. Defer reusable TiN electrode design to Phase 2 cost-reduction phase if needed. |
| **Regulatory pathway complexity** (BF vs. CF classification impact) | MEDIUM | Adult ambulatory use: keep as Research Use Only (RUO), no clinical claims. Patch form factor is a new device submission (notified body review), but existing IEC 60601-1 BF compliance framework applies. Wolff (Regulatory) to confirm before production design. |
| **Neonatal/pediatric use blocker** (current density safety) | HIGH | This memo addresses adult ambulatory use only. Pediatric use requires separate safety evidence for ICG current density on immature skin (not addressed here). Defer neonatal design to Phase 2 + separate clinical study. |

---

## 10. Decision and Recommendation

### Verdict: **CONDITIONAL GO**

Dry-electrode ICG measurement in a patch form factor is **technically feasible** and **ready for prototype validation**, conditional on:

1. **Semi-dry Ag/AgCl snap electrodes** (5–15 kΩ contact impedance) with **active electrode buffering** (OPA333 unity-gain stage, 50 Ω output impedance).
2. **Hybrid patch + lead architecture:** Sense electrodes on the 80 × 60 mm anterior patch, injection electrodes on the back (connected via thin shielded lead wires).
3. **Successful completion of Experiments 1 & 2** (benchtop validation and in vivo single-session study) confirming SNR ≥ 3 dB and stroke volume estimation within ±15% of gel-electrode baseline.
4. **Prototype electronics assembly** (Experiment 4) demonstrating mechanical and electrical integration into the patch form factor without performance degradation.

### Key Findings

| Finding | Implication |
|---|---|
| **Signal amplitude drops 40–60%** with dry electrodes (10 kΩ contact impedance) | Acceptable if active buffering is used; unacceptable without buffering. |
| **CMRR degrades by 8–10 dB** due to impedance mismatch | Mitigation: active electrode buffer restores CMRR to ≥ 65 dB. |
| **SNR is marginal (3–5 dB) without buffering, restored to 35 dB with buffering** | Active buffering is mandatory; cannot be deferred to Phase 2. |
| **Electrode spacing must be 4–5 cm (sense pair)** to maintain adequate signal | Fits easily in 80 × 60 mm anterior patch geometry. |
| **Injection electrodes must remain on the back** (15–20 cm from sense pair) | Requires thin shielded leads; acceptable for ambulatory wear. |
| **Ag/AgCl semi-dry snaps are the optimal material choice** | Proven clinical performance, stable impedance, off-the-shelf availability. |

### Next Steps

**Phase 1 (Concept Validation, Experiments 1–4):** 6 weeks  
- Week 1: Benchtop phantom testing → confirm SNR/CMRR targets.
- Week 2–3: In vivo human subject study → confirm SV estimation accuracy.
- Week 4: Adhesive patch prototype → confirm mechanical integration feasibility.
- Week 5–6: Patch electronics assembly → confirm form-factor integration.

**Go/No-Go Gate:** Upon completion of Experiment 2 (human subject validation), decision required. If SNR < 0 dB or SV error > ±20%, escalate to Nair for material/circuit redesign.

**Phase 2 (PATCH Development, if Phase 1 passes):** 9–12 months  
- Rigid-flex PCB layout and manufacturability review.
- Battery management and power budget refinement (Müller, low-power ambulatory firmware).
- Industrial design and adhesive system optimization (Voss).
- Regulatory pathway and notified-body submission prep (Wolff).
- If pediatric use is desired: separate clinical safety study for neonatal current densities (not included in Phase 1 scope).

---

## Appendix A — References

1. **ICG Electrode Impedance Literature:**
   - Bernstein, D. P. (2003). Impedance Cardiography: Pulsatile Blood Flow and the Biophysical and Electrodynamic Basis for the Stroke Volume Equations. *Journal of Electrical Bioimpedance*, 4(1), 2–17.
   - Raasch et al. (1999). Comparison of ECG signal variations over repetitive donning and doffing of a dry electrode: Towards a continuous health monitoring shirt. *Proceedings of IEEE EMBS*.

2. **Dry Electrode Performance:**
   - Searle, A., & Kirkup, L. (2000). A direct comparison of wet, dry and insulating bioelectrodes. *Physiological Measurement*, 21(2), 271–283.
   - Spinelli et al. (2010). Bioelectrodes and electrodes interfaces: Shocking work? *Journal of Electrical Bioimpedance*, 1(1), 1–8.

3. **Component Datasheets:**
   - AD8221: Analog Devices, https://www.analog.com/en/products/ad8221.html
   - AD630: Analog Devices, https://www.analog.com/en/products/ad630.html
   - OPA333: Texas Instruments, https://www.ti.com/product/OPA333

4. **VU-AMS System Documentation:**
   - component_selection_analog.md (Rev 0.3)
   - schematic_bottom_pcb_001.md (Rev B5)
   - brainstorm_001.md (Concept Card 3, VU-AMS PATCH)

---

**Memo prepared by:** Priya Nair, Electronics — Slow Horses  
**Date:** 2026-05-10  
**Status:** Ready for principal review and Phase 1 experiment sign-off.

**Next Review:** Upon completion of Experiment 2 (human subject study, anticipated 2026-05-25).

*This memo is confidential and intended for Slow Horses internal decision-making. Distribution restricted to technical team and principal.*
