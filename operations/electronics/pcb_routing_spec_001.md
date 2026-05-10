# PCB Routing Specification and Design Rules
## VU-AMS Bottom PCB — Analog Front-End Rev 0.1
### Document: pcb_routing_spec_001.md

**Author:** Priya Nair — Electronics  
**Date:** 2026-05-09  
**Ref:** schematic_bottom_pcb_001.md · netlist_bottom_pcb_001.net · component_selection_analog.md · vasquez_placement_validation_001.md  
**Status:** ISSUED — for KiCad layout entry and fab preparation

---

## 1. Board Specification

### 1.1 Physical Dimensions and Outline

| Parameter | Value | Justification |
|-----------|-------|---------------|
| Board dimensions | 52.0 × 25.0 mm | Constrained by housing internal cavity; confirmed in pcb_layout_001.svg |
| Board outline tolerance | ±0.1 mm | Standard PCB fab; housing snap-fit alignment requires tight tolerance |
| Corner radius | 1.0 mm | Housing recess radius; prevents corner crack propagation |
| Minimum component keep-in | 0.5 mm from board edge | Mechanical clearance from housing walls |
| E1 aperture | Ø 12 mm circular cutout, centred at 12.5 mm from top edge, 26 mm from left edge | Matches housing spring-contact position; see pcb_layout_001.svg |

### 1.2 Layer Stackup — 4 Layers

| Layer | Designation | Function | Copper weight |
|-------|-------------|----------|---------------|
| TOP (L1) | Signal + Power | Component placement, digital and mixed-signal routing, SPI traces, I²C, power distribution traces | 1 oz (35 µm) |
| L2 | GND | Solid ground pour — continuous, unbroken under entire analog section | 1 oz (35 µm) |
| L3 | Power Planes | VBAT, AVDD, AVSS polygon pours; isolated regions per supply domain | 1 oz (35 µm) |
| BOTTOM (L4) | Signal + Analog | Analog signal traces (electrode paths, ICG sense, ECG), analog return paths; no digital traces | 1 oz (35 µm) |

**Rationale for layer assignment:**  
Placing the solid GND plane on L2 (immediately below the component layer) provides the lowest-impedance return path for all signal traces on TOP. The analog signal traces on BOTTOM are referenced to the GND plane on L2 through the board substrate, minimising loop area for sensitive biopotential signals. Power planes on L3 are shielded from both signal layers by the L2 ground.

### 1.3 Rigid-Flex Construction

This design is a **single rigid-flex PCB** — not two separate boards connected by a cable. It consists of two 52×25mm rigid zones joined by a single flex strip.

#### Mechanical zones

| Zone | Dimensions | Material | Notes |
|------|------------|----------|-------|
| Rigid zone 1 (bottom PCB, analog front-end) | 52.0 × 25.0 mm | FR4, Tg ≥ 130°C, 1.6 mm | Component-bearing; all analog and power ICs |
| Rigid zone 2 (top PCB, digital/ESP32) | 52.0 × 25.0 mm | FR4, Tg ≥ 130°C, 1.6 mm | ESP32-S3, BLE, battery management |
| Flex strip | 10.0 × 20.0 mm (nominal) | Polyimide, 0.1 mm total thickness | 25 µm copper each side; 2-layer |

#### Flex zone stackup

| Layer | Designation | Function | Copper |
|-------|-------------|----------|--------|
| F1 (flex top) | Signal | Power and digital signal routes between rigid zones | 25 µm (0.5 oz) |
| F2 (flex bottom) | GND return | Continuous GND return plane on flex; keeps loop area small across the bend | 25 µm (0.5 oz) |

- **Coverlay:** Polyimide coverlay applied both sides of flex zone; adhesive opening only at pad locations where flex traces terminate into rigid zone pad patterns.
- **Stiffener:** FR4 stiffener (0.2 mm) bonded at both rigid-to-flex transitions to prevent delamination and constrain the bend to the free-flex zone.
- **Vias in flex zone:** Prohibited. All layer transitions (F1/F2 to rigid stackup layers L1–L4) occur within the rigid zones, ≥ 1.5 mm from the rigid-flex boundary.
- **Bend radius:** Minimum 3.0 mm, one-time static bend to fold the assembly into the housing. Do not design for repeated dynamic flexing. The flex strip is folded once during assembly and held by housing geometry — not designed as a repeated-flex zone.
- **Signal routing on flex:** Maximum 8 nets on F1 across the flex strip. Routed parallel, 0.20 mm width, 0.20 mm spacing. All high-speed digital (SPI) and power (VBAT, AVDD) pass through the flex. No analog biopotential signal traces cross the flex zone.
- **Copper fill on F2:** Solid GND return pour across the full flex zone width. Connected to L2 GND plane at both rigid zone entry points via stitching vias (0.3 mm drill) placed within 1.5 mm of the flex boundary.
- **Fab note to KiCad DRC:** Define flex zone as a separate board region with relaxed trace-width minimum (0.15 mm) and no via placement. Mark rigid-flex boundaries explicitly in Edge.Cuts or User.Drawings layer.

---

### 1.4 Material and Finish — Rigid Zones

| Parameter | Value | Justification |
|-----------|-------|---------------|
| Substrate | FR4, Tg ≥ 130°C | Standard medical-grade PCB material; sufficient for body-worn thermal environment (0–40°C) |
| Board thickness | 1.6 mm ± 0.1 mm | Housing pocket depth defined; 1.6 mm is the constrained maximum |
| Surface finish | ENIG (Electroless Nickel Immersion Gold) | Required for fine-pitch pads: ADS1256 SSOP-28 (0.65 mm pitch), LT3042 DFN-8 (0.5 mm pitch), TMP117 DSBGA-8. ENIG provides flat, solderable surface; prevents oxidation on shelf; compatible with Ag/AgCl electrode connector contacts. HASL not acceptable for sub-0.65 mm pitch. |
| Solder mask | LPI both sides, green or white | White preferred on skin-contact side for visual inspection of solder joints |
| Silkscreen | Top side only | Component reference designators and pin 1 markers |
| IPC class | IPC Class 2 | Standard for medical wearables at this complexity level |

### 1.5 Via Specification

| Via type | Drill diameter | Pad diameter | Justification |
|----------|---------------|--------------|---------------|
| Signal via (standard) | 0.3 mm | 0.6 mm | Minimum at standard PCB fabs; used for signal layer transitions |
| Power via | 0.4 mm | 0.8 mm | Reduced resistance for AVDD and VBAT distribution vias |
| Thermal via (LT3042 GND pad) | 0.3 mm | 0.6 mm | Array of 4 vias under exposed pad; ENIG finish for solderability |
| Buried/blind vias | Not used | — | Adds cost and complexity; not required at this pin density |

**Via-in-pad:** Permitted under LT3042 DFN-8 exposed thermal pad only. Vias must be tented (solder mask filled) to prevent solder wicking during reflow.

### 1.6 Trace Width and Clearance Rules

| Net class | Min trace width | Recommended width | Justification |
|-----------|----------------|-------------------|---------------|
| Signal — digital (SPI, I²C, GPIO) | 0.10 mm | 0.15 mm | Low current; width set by impedance control and DRC clearance |
| Signal — analog low-level (ECG, ICG sense, REF) | 0.10 mm | 0.12 mm | Minimal copper area to reduce parasitic capacitance and stray pickup; matched routing required |
| Power — AVDD, AVSS, DVDD | 0.30 mm | 0.40 mm | 30 mA max AVDD load; 0.30 mm gives < 100 mΩ/cm resistance in 1 oz copper |
| Power — VBAT input | 0.50 mm | 0.60 mm | 100 mA worst-case LT3042 input current; 0.50 mm gives ~35 mΩ/cm |
| Analog electrode traces | 0.50 mm | 0.50 mm | Patient protection traces; wider to reduce series resistance in safety path |
| I²C pull-up resistor traces | 0.10 mm | 0.15 mm | Low current signal lines |

**Minimum clearance (all layers):** 0.15 mm signal-to-signal. 0.20 mm signal-to-copper pour. 0.30 mm analog-to-digital zone boundary. 3.00 mm electrode trace to digital trace (IEC 60601-1 BF creepage/clearance consideration on patient-circuit conductors).

---

## 2. Critical Routing Rules Per Circuit Block

### 2.1 ICG Carrier — 32 kHz Oscillator and Current Source Chain

**Signal chain:** SG-210STF (U10) → OSC_REF → AD630 pin 9 (SELECT/REF+)  
**Secondary branch:** U10 pin 3 → R_OSC (33 Ω) → J1 pin 15 (OSC_MON to ESP32)

| Rule | Value | Justification |
|------|-------|---------------|
| OSC_REF trace maximum length | 20 mm | At 32 kHz the quarter-wavelength is ~2.3 km; length constraint is for minimising EMI radiation and capacitive pickup, not transmission-line effects. 20 mm is achievable within the ICG zone without any vias. |
| OSC_REF trace width | 0.15 mm | Low current CMOS signal; narrow to reduce parasitic capacitance (AD630 SELECT pin input impedance is high) |
| OSC_REF routing layer | BOTTOM (L4) | Away from power planes on L3; referenced to GND plane on L2 |
| GND guard trace on OSC_REF | Required both sides of trace | 0.15 mm guard traces connected to AGND every 5 mm via short stitch; prevents capacitive coupling from adjacent charge-pump switching noise |
| Characteristic impedance of OSC_REF line | 50 Ω (target) | Achieved by setting trace width to 0.15 mm on 1.6 mm FR4 with L2 GND plane at ~0.36 mm below BOTTOM layer. Verify with PCB impedance calculator during fab review. |
| OSC_REF routing | No 90° bends; 45° or curved bends only | Reduces discontinuities; no functional impact at 32 kHz but establishes consistent practice |
| U10 supply decoupling | 100 nF within 1 mm of U10 VDD pin | Prevents oscillator supply noise from coupling into carrier frequency; essential for phase stability |
| U10 placement constraint | Minimum 10 mm from TPS60400 (U11) | Charge pump switching noise (nominally ~1 MHz) must not couple into oscillator power or output pin |
| OSC_MON branch series resistor R_OSC | 33 Ω, within 2 mm of J1 pin 15 | Isolates ESP32 GPIO capacitance from oscillator output; prevents loading effect on phase reference |

**ICG current source zone:**  
The ICG VCCS components (discrete op-amp, matched resistor array, R_mon) form an electrically isolated zone in the top-left of the board (see Section 4, Step 3). ICG injection traces to J_E4 and J_E5 must run on BOTTOM (L4) directly to the connector pads. These traces must not pass within 3 mm of AD8221 (U3) input traces (NET_ICG_SENSE, NET_E2_PHR).

### 2.2 ADS1256 SPI Interface

**Nets:** NET_ADC_MOSI (J1 pin 8 → U6 pin 21), NET_ADC_MISO (U6 pin 19 → J1 pin 9), NET_ADC_CLK (J1 pin 7 → U6 pin 20), NET_ADC_CS (J1 pin 10 → U6 pin 18), NET_ADC_DRDY (U6 pin 17 → J1 pin 11)

| Rule | Value | Justification |
|------|-------|---------------|
| Length matching (MOSI, MISO, CLK) | ±0.5 mm between the three traces | Prevents setup/hold timing skew at SPI clock rates up to 10 MHz (ADS1256 max SCLK); at 10 MHz, 0.5 mm corresponds to ~3 ps propagation skew — well within timing margins |
| Routing layer | TOP (L1) | Digital interface; keep on component-side signal layer, referenced to L2 GND plane |
| No 90° bends | 45° or curved bends only | Reduces impedance discontinuities; prevents acid traps in etching |
| GND guard traces | Single GND trace on each side of MISO | MISO is the most sensitive (weakly driven by ADS1256); flanking GND guard reduces inductive coupling from adjacent MCU-driven lines |
| Minimum clearance SPI to analog traces | 3.0 mm | Prevents coupling of 10 MHz SPI harmonics into sub-mV ECG/ICG signal traces on BOTTOM layer |
| CS and DRDY trace routing | Keep parallel to SPI bus, same layer, within 1 mm lateral distance | Simplifies bundle routing to J1; no matching requirement on control lines |
| Decoupling at U6 DVDD (pin 22) | 100 nF within 1 mm | Isolates digital supply switching from AVDD via shared PCB copper; mandatory for 24-bit ADC noise floor |
| ADS1256 CLKIN (pin 23) | 10 kΩ to AGND within 5 mm of pin | Selects internal oscillator; short trace to AGND resistor prevents floating pin pickup |

### 2.3 Electrode Input Traces — Patient Protection Chain

**Nets:** NET_E1_PAD through NET_E7_PAD (connector to TVS), NET_Ex_TVS (TVS to safety resistor), and post-resistor nets to analog front-end.

| Rule | Value | Justification |
|------|-------|---------------|
| Routing layer | BOTTOM (L4) only | Analog patient-circuit conductors must not share a layer with digital traces. BOTTOM layer with L2 GND plane above provides shielding from L1 digital activity. |
| Minimum clearance from digital traces | 3.0 mm | IEC 60601-1 creepage/clearance for BF-type patient circuits at 250 V working voltage in PCB material CTI group II. 3 mm on a PCB surface exceeds the 2.5 mm minimum for pollution degree 2; conservative margin taken. |
| TVS placement | Within 2 mm of electrode connector pad | TVS clamping is only effective if placed before any PCB trace can act as an antenna. TVS cathode GND connection must be via a direct, wide via (0.4 mm drill) to L2 GND plane — zero via daisy-chain. |
| Safety resistor (R1–R7) placement | Within 3 mm of TVS output pad | Keeps the protected (low-impedance) segment of the trace short; the 10 kΩ resistor limits fault current from the TVS output to the InAmp input. |
| Trace width (electrode pad to TVS) | 0.50 mm | This segment is part of the patient circuit; wider trace reduces series resistance below the clinically relevant level and provides robustness against mechanical flex at connector edge |
| Trace width (TVS to safety resistor) | 0.30 mm | Post-clamping, reduced width is acceptable; 30 mA clamp current capability |
| Trace width (safety resistor to InAmp input) | 0.10 mm | Minimum; this is the high-impedance biopotential node — narrow trace reduces parasitic capacitance and stray pickup |
| ICG injection traces (E4, E5) | Route along board top edge directly to J_E4, J_E5 | ICG injection must not pass adjacent to ICG sense traces (E2, E3); minimum 5 mm separation or GND trace between them |
| Phantom power traces (E2, E3) | AVDD → R_PH1/R_PH2 on TOP (L1); capacitor C_BLK1/C_BLK2 on BOTTOM (L4) | Phantom DC on TOP layer; AC signal path after DC block transitions to BOTTOM analog layer at the blocking capacitor pad |

### 2.4 TPS60400 Charge Pump (U11) — AVSS Generation

**Nets:** AVDD (input), NET_CP_PLUS (pin 2 to C_P1), NET_CP_MINUS (pin 3 to C_P1), NET_AVSS_RAW (pin 5 to L1), AVSS (L1 output to AD630)

| Rule | Value | Justification |
|------|-------|---------------|
| Flying capacitor C_P1 trace length | < 3 mm from U11 pin 2 and pin 3 | TPS60400 switches C_P1 at nominally ~1 MHz; long traces add series inductance that reduces pumping efficiency and increases switching noise. Datasheet requires minimum flying cap trace length. |
| C_P1 trace width | 0.40 mm | Carries peak charging currents > 50 mA during switching transitions; 0.40 mm in 1 oz copper gives ~25 mΩ/cm, adequate for brief transient. |
| Input decoupling caps (C_IN_CP) placement | Within 1 mm of U11 pin 1 (V+) | Absorbs switching current spikes from AVDD rail; prevents charge pump noise coupling back to AVDD and thence to LT3042-supplied analog circuits |
| Output decoupling caps (C_OUT_CP) placement | Within 1 mm of U11 pin 5 (VOUT) | Pre-filter capacitor; must be present before LC filter inductor L1 |
| LC filter L1 placement | Within 5 mm of U11 VOUT pad | Short pre-filter trace minimises the length of unfiltered AVSS_RAW; switching noise radiates from this segment |
| No signal traces under TPS60400 | Keep-out on BOTTOM (L4) under entire U11 footprint and 2 mm margin | Charge pump switching creates electric field; sensitive analog nodes on BOTTOM layer must not be routed under or within 2 mm of U11 |
| AVSS_RAW trace routing | Not parallel to any ICG or ECG signal trace for > 5 mm | After LC filter, AVSS is quiet; before the filter, AVSS_RAW switching noise must be isolated from biopotential signal paths |
| GND connection U11 pin 4 | Direct wide via (0.4 mm drill) to L2 GND plane | Low-impedance ground return for charge pump switching current; ring-ground topology unacceptable |

### 2.5 LT3042 LDO (U8) — AVDD Generation

**Nets:** VBAT (input), AVDD (output), R_SET to AGND

| Rule | Value | Justification |
|------|-------|---------------|
| Input capacitor C_IN_LT (10 µF) | Within 1.0 mm of U8 VIN pin | LT3042 datasheet requires minimum capacitance at VIN; close placement ensures low ESR path from capacitor to pin; >1 mm trace adds series inductance that can reduce LDO stability |
| Output capacitor C_OUT_LT (10 µF + 100 nF) | Within 1.0 mm of U8 VOUT pin | LT3042 output noise performance (0.8 µV RMS) is specified with 10 µF at VOUT; any additional series inductance between COUT and pin degrades the LDO noise transfer function. This is the single most critical layout requirement for the AVDD rail quality. |
| C_SET capacitor (10 nF) | Within 1.0 mm of U8 SET pin | Internal current-source noise filtering; long trace adds parasitic capacitance that can alter the effective SET capacitance and slightly increase output noise |
| Exposed thermal pad via array | 4 × 0.3 mm vias to L2 GND plane | DFN-8 exposed pad is the GND connection; multiple vias provide both thermal path and low-impedance ground. Vias must be tented/filled. |
| AVDD output trace | Never route parallel to VBAT trace | LT3042 PSRR is 75 dB at 1 MHz; parallel routing capacitively couples VBAT switching noise back onto AVDD. Maintain ≥ 3 mm separation or orthogonal routing. |
| AVDD trace from LT3042 | Star topology from LT3042 VOUT pad to each functional block | See Section 3. The LT3042 output is the star centre; each branch (ICG, ECG, ADC, SCL, PPG) has its own trace. No daisy-chain. |
| AVDD trace width | 0.40 mm to each branch | 30 mA total AVDD load; 0.40 mm per branch gives adequate headroom with < 50 mΩ total trace resistance at branch lengths < 20 mm |

### 2.6 Analog Shielding

All high-impedance analog signal nodes — ADS1256 input traces, INA333 (U1, U2) inputs, AD8221 (U3) inputs, electrode sense traces (NET_E1_FILT through NET_E5_FILT after the safety resistors), and the VREF trace — must be enclosed by active shielding and grounded copper on all layers. The following rules apply.

#### 2.6.1 Ground pour enclosure — analog zone

- **L2 (GND plane):** Solid, unbroken copper pour under the entire analog signal zone: from U3 (AD8221) through U1/U2 (INA333) through U6 (ADS1256) through U7 (REF5025). No slots or trace-forced gaps under any of these ICs. The L2 pour is the primary shield against L1 digital activity and L3 power-plane switching.
- **L1 (TOP layer) copper pour:** AGND flood fill over all unrouted areas within the analog zone. Stitched to L2 via a grid of 0.3 mm vias at ≤ 4 mm pitch across the entire analog zone.
- **L4 (BOTTOM layer) copper pour:** AGND flood fill over all unrouted areas within the analog zone, stitched to L2 via the same grid. This pour forms the reference plane immediately adjacent to the analog signal traces on L4.
- **L3 (Power planes layer):** AVDD polygon occupies the analog zone; where no power polygon exists, AGND fill is added and stitched to L2. No ungrounded copper islands on L3.

The combined effect is a 4-layer cage: analog signal traces on L4 are sandwiched between the L2 GND plane (above) and the L4 AGND pour (surrounding the traces on the same layer). Traces on L1 in the analog zone are likewise enclosed by L1 pour below and L2 GND plane immediately beneath.

#### 2.6.2 Guard rings on high-impedance electrode input traces

Guard rings are required on all high-impedance electrode input nodes from the safety resistor output to the InAmp input pins. These are the highest-sensitivity nodes on the board.

| Node / trace | Guard type | Connection | Remarks |
|---|---|---|---|
| NET_E1_FILT to AD8221 IN+ | Passive GND guard | AGND pour | 0.15 mm gap between signal trace and guard ring; guard stitched to L2 every 3 mm |
| NET_E2_FILT to AD8221 IN− | Passive GND guard | AGND pour | Same as above |
| NET_E3_FILT to INA333 inputs | Passive GND guard | AGND pour | ECG sense path |
| NET_E1_FILT to INA333 inputs | Passive GND guard | AGND pour | ECG reference path |
| VREF trace (REF5025 OUT → ADS1256 VREFP) | Passive GND guard both sides | AGND pour | 0.15 mm guard, stitched to L2 every 3 mm; this trace must not be within 1 mm of any switching net |

**Driven guard (active shield):** For the ICG sense inputs (NET_E2_FILT and NET_E1_FILT as they run from the safety resistor to the AD8221 pin), a driven guard is required in addition to the passive outer guard:
- A unity-gain buffer (OPA333 in unity-gain configuration, dedicated instance or shared with available OPA333 bandwidth — see schematic Section 4.3) drives an inner guard ring that tracks the signal potential.
- The inner guard is at signal potential; the outer guard (beyond it) is at AGND.
- This two-ring structure eliminates leakage and cable-capacitance errors at the AD8221 input.
- Inner guard ring width: 0.20 mm. Gap between inner guard and signal trace: 0.15 mm. Outer AGND ring: 0.20 mm, gap from inner: 0.20 mm.

#### 2.6.3 ICG 32 kHz carrier trace — coplanar waveguide

The OSC_REF trace and the ICG injection traces (ICG_SRC_P to J_E4/J_E5) carry the 32 kHz carrier and must not radiate into the sense amplifier inputs.

- **OSC_REF (U10 pin 3 → U4 AD630 pin 9):** Routed as coplanar waveguide on L4. GND strips of 0.20 mm width on each side, gap 0.20 mm from trace edge. GND strips stitched to L2 via 0.3 mm vias at every 5 mm. This rule supersedes the 0.15 mm guard requirement in Section 2.1 — 0.20 mm ground strips provide lower impedance shielding.
- **ICG injection traces (ICG_SRC_P to J_E4/J_E5):** Same coplanar waveguide treatment on L4.

#### 2.6.4 Mechanical shield can

An SMD shield can (tin-plated steel or stainless, PCB-footprint with perimeter solder fence and lid) is recommended over the entire analog signal zone. Dimensions: to suit KiCad layout, nominally 30 × 22 mm covering U3 (AD8221), U1, U2 (INA333), U6 (ADS1256), U7 (REF5025), and all electrode sense traces. The fence pads solder to the L1 AGND pour. The lid provides a direct mechanical Faraday enclosure for the most sensitive nodes.

- Candidate part: Harwin S02-20150300 (adjustable frame) or equivalent SMD shield fence.
- Height must not conflict with housing lid clearance (confirm with housing model before ordering).
- If total height budget prevents a full-height can, a low-profile shield fence (fence only, no lid) is acceptable as a second choice — it still provides trace-level shielding from side-entry fields.

#### 2.6.5 ADS1256 analog ground plane

The L2 GND pour under the ADS1256 (U6) footprint must be continuous and unbroken across the entire IC footprint plus a 2 mm margin on all sides. No power polygon, trace, or via clearance hole may fragment this area. The ADS1256 AGND pin (pin 6) connects via a direct 0.4 mm drill via to this pour. The VREFN pin (pin 16) connects via a separate, dedicated 0.4 mm via to the same pour — sharing a via with any other return current path is prohibited.

---

### 2.7 ICG Current Source / Input Signal Separation

The VCCS (voltage-controlled current source, carrying ~1 mA at 32 kHz) and the ICG sense input path (differential signal of 0.1–0.5 mV on a 25 mV carrier into AD8221/INA333) must be treated as incompatible signal domains. Any capacitive or inductive coupling from the injection path into the sense path creates a correlated interference term at exactly the carrier frequency — it cannot be rejected by CMRR or filtering. The following rules are mandatory.

#### 2.7.1 Physical routing separation

- **Current source path:** The VCCS output traces (ICG_SRC_P, nets to J_E4 and J_E5) run along the **top board edge** of the analog rigid zone (y = 0 to y = 5 mm from top edge). They are routed on L4 and exit directly to the electrode connector pads at the top edge.
- **Sense path:** The ICG sense traces (NET_E2_FILT, NET_E1_FILT from safety resistors to AD8221 pins 2 and 3) run along the **bottom board edge** of the analog rigid zone (y = 20 to y = 25 mm from top edge). They are routed on L4 and enter the AD8221 package from the bottom.
- **Minimum clearance between injection and sense trace bundles:** 5.0 mm at all points across the entire board.
- **No parallel routing:** Injection and sense traces must not run parallel to each other for a continuous length exceeding 2.0 mm. If the layout requires them to approach each other (e.g., near the electrode connector cluster), they must cross at 90° or be separated by an intervening GND trace of ≥ 0.20 mm width stitched to L2.

#### 2.7.2 VCCS zone isolation

- The VCCS components (op-amp, matched resistor network R8–R11, R_mon) occupy a dedicated sub-zone in the top-left of the board as defined in Section 4, Step 3.
- A GND trace of 0.30 mm width, stitched to L2 via 0.3 mm vias every 3 mm, forms a continuous barrier between the VCCS sub-zone boundary and the AD8221 (U3) placement zone.
- No non-VCCS signal trace may enter the VCCS sub-zone. Power traces (AVDD, AVSS) to the VCCS op-amp enter from the power supply zone at the top-left and do not cross the sense zone.
- Minimum spacing between the VCCS op-amp output pin trace and the nearest AD8221 input trace: 5.0 mm.

#### 2.7.3 Driven shield on ICG sense electrode leads

The coaxial cable carrying the ICG sense signal from the skin electrode to the PCB (E2 and E3 electrode cables) uses a driven shield to eliminate cable capacitance as a crosstalk path.

- **Cable configuration:** 2-conductor: signal core + shield. At the electrode end, the shield is driven by the LMP7721 buffer unity-gain output (see schematic — Bijlage B4). At the PCB end, the shield connects via R_SHD2/R_SHD3 (100 Ω) to NET_E2_FILT/NET_E3_FILT respectively.
- **Board-side shield buffer:** An additional unity-gain buffer instance (OPA333, SOT-23-5, same part as RLD amplifier U4) may be placed between NET_E2_FILT and the shield return at the PCB-side connector if cable length or layout requires additional drive capability. Input to this buffer: NET_E2_FILT (post C_BLK2); output: shield conductor at PCB connector pin.
- **Purpose:** Equalises the potential of the cable shield to the cable signal core. The capacitance between core and shield carries no charging current because both ends are at the same potential. This eliminates the 32 kHz injection-path field from coupling into the sense cable capacitance.
- **Layout requirement:** The driven-shield buffer (if placed on the PCB) must be located within 5 mm of the E2/E3 electrode connector pads, within the sense path zone (bottom edge), and fully separated from the VCCS zone.

#### 2.7.4 Split GND plane rule — proximity contingency

If the final layout cannot achieve the 5.0 mm injection-to-sense clearance (for example, due to electrode connector positioning constraints), the following contingency applies:

- Split the L2 GND plane between the injection-side and sense-side zones with a 0.3 mm slot running parallel to the board's long axis between the two trace bundles.
- Connect the two GND half-planes at exactly **one point** — a 0.5 mm copper bridge at the top-left corner of the board (near the power supply zone), remote from any analog signal trace.
- This prevents a ground loop that would allow injection-path return currents to flow under the sense traces.
- This contingency must be reviewed by Vasquez before implementation — splitting the GND plane has consequences for L2 shield effectiveness over the ADC and reference zones. The preferred solution remains full 5.0 mm clearance.

---

## 3. Power Distribution

### 3.1 VBAT Plane

- **Layer:** L3 polygon pour, covering the top-left quadrant of the board (LT3042 input zone, approximately 15 × 10 mm region)
- **Entry point:** J1 pins 1 and 2 (VBAT, dual-pin current sharing). Both flex connector pads must connect to the L3 VBAT polygon via 0.6 mm traces individually, then merged at the polygon entry.
- **Fused entry:** A 500 mA PTC resettable fuse (or fixed 0402 fuse, 250 mA fast-blow) must be placed in series with VBAT between J1 pad and L3 polygon. This is the single fused entry point; all VBAT distribution is downstream of this fuse.
- **Polygon isolation:** VBAT polygon must be isolated from AVDD polygon by ≥ 0.5 mm gap on L3. No copper bridge between the two polygons other than the LT3042 pass element connection on TOP layer.
- **Vbatt monitor tap:** R_VBAT_A (100 kΩ) connects from the VBAT polygon to NET_VBAT_MON node. This tap must be at the output side of the PTC fuse to measure post-fuse voltage.

### 3.2 AVDD Plane (+3.3 V Analog)

- **Layer:** L3 polygon pour, separate from VBAT. Covers the central and right regions of the board where analog consumers are located.
- **Polygon entry:** Single connection from LT3042 VOUT (U8 pin 2) on TOP layer via 0.4 mm trace down to L3 AVDD polygon via a 0.4 mm via. This via is the star centre for AVDD distribution.
- **Star topology from L3 AVDD:** Individual branches rise from L3 to TOP (L1) at each functional block via a single 0.4 mm via per block:
  - Branch 1: ICG chain — U10 (oscillator), U3 (AD8221), U4 (AD630)
  - Branch 2: ECG chain — U1 (INA333 ch1), U2 (INA333 ch2), U5 (OPA333 RLD)
  - Branch 3: ADC block — U6 (ADS1256 AVDD), U7 (REF5025)
  - Branch 4: SCL block — U9 (AD5933)
  - Branch 5: PPG/thermal — U12 (MAX30101 VLED, AVDD), U13 (TMP117)
  - Branch 6: Charge pump — U11 (TPS60400 V+)
- **No daisy-chain:** No branch may feed a second consumer from the first consumer's power pin. Each IC power pin traces back to the L3 AVDD polygon independently (via its local 100 nF decoupling cap).
- **Local decoupling:** Every AVDD consumer IC must have a 100 nF 0402 X7R capacitor from its AVDD pin to AGND within 0.5 mm of the power pin. This is in addition to shared bulk capacitors (10 µF per functional block, placed centrally within each block zone, within 3 mm of the functional block centroid).

### 3.3 AVSS Plane (−3.3 V Negative Analog Rail)

- **Layer:** L3 polygon pour, isolated from VBAT and AVDD polygons by ≥ 0.5 mm gap. Located in the ICG/demodulator zone (top-left to centre-left of board).
- **Source:** TPS60400 (U11) VOUT → LC filter (L1 + C_AVSS) → AVSS polygon. The LC filter output node is the polygon entry point; the TPS60400 output (AVSS_RAW, before L1) is NOT connected to the L3 polygon — it is a separate short trace on TOP layer only.
- **Consumers:** AD630 −VS (U4 pin 8) only in Rev 0.1. ICG VCCS op-amp −VS if populated. No other components.
- **Isolation from digital ground:** AVSS polygon must not share any copper with the DGND net. AVSS is a negative supply rail, not a return path. The single-point connection between AGND and AVSS occurs at the AD630 −VS node (see Section 3.4).
- **AVSS trace to AD630:** Single 0.30 mm trace from L3 AVSS polygon to U4 pin 8 via dedicated via. 100 nF X7R decoupling from this pin to AGND within 1 mm.

### 3.4 Analog GND vs. Digital GND — Star Connection

**This is the most critical grounding decision on the board.**

- **AGND (analog ground):** Solid copper pour on L2 under the entire analog section (approximately right 75% of board). All analog IC GND pins connect directly to this pour via the nearest via or direct pad-to-pour connection.
- **DGND (digital ground):** J1 pins 3 and 4. On the bottom PCB, DGND exists only at the flex connector and in a small local pour around the ADS1256 DVDD / DGND pins (U6 pin 22, decoupling cap to DGND). This pour is electrically separate from AGND on the bottom PCB.
- **Single-point star connection:** AGND and DGND are connected at exactly one point on the bottom PCB: at the ADS1256 AGND pin (U6 pin 6), which is where the digital interface transitions to the analog core. The connection is a short 0.5 mm × 0.15 mm copper bridge between the AGND pour and the DGND island, placed at U6 pin 6. This bridge must not be wider than 0.5 mm — a large copper bridge between AGND and DGND defeats the star topology.
- **AGND star to flex:** AGND returns to J1 pins 5 and 23. These are the primary and secondary return paths for AGND to the top PCB; both must connect to the L2 GND pour via 0.4 mm vias at the J1 footprint.
- **Electrode TVS AGND connections:** All D1–D7 TVS cathode GND pins connect to the L2 AGND pour directly via 0.4 mm vias immediately adjacent to each TVS pad. No daisy-chain of TVS grounds.
- **VREF return:** REF5025 (U7) AGND pin and ADS1256 VREFN (pin 16) both connect to the AGND pour at the nearest point to U6. These are the most noise-sensitive nodes in the system; their GND return must not share a via with any digital or switching current path.

---

## 4. Component Placement Priorities

Placement must follow this sequence. Each step defines mandatory placement before proceeding to the next.

### Step 1 — ADS1256 (U6): Center of Analog Section, Bottom-Right

**Position:** 36–48 mm from left edge, 6–18 mm from bottom edge.  
**Rationale:** The ADS1256 SSOP-28 (6.5 × 10.2 mm body) is the largest analog IC and sets the grid for the analog input traces. All ADC channel inputs route to this IC; placing it centrally in the analog section minimises average trace length to the signal conditioning blocks. Bottom-right placement keeps SPI digital interface pins (pins 17–22, right side of package) close to the J1 flex connector on the top edge.

**Orientation:** Pin 1 towards bottom edge. AVDD (pin 4) and SPI pins (17–22) face top/right; analog inputs (pins 1–3, 24–28) face left and bottom.

**Decoupling cap placement:**
- C_U6_AVDD (100 nF): within 0.5 mm of pin 4
- C_U6_DVDD (100 nF): within 0.5 mm of pin 22
- C_U6_bulk (10 µF 0603): within 3 mm of pin 4
- C_U6_VREF (100 nF): within 1 mm of pin 15 (VREFP)
- CLKIN resistor to AGND: within 2 mm of pin 23

**Keep-out:** No via placement in the 1 mm clearance zone around the SSOP-28 land pattern.

---

### Step 2 — Input Protection and Electrode Connectors (J_E1 through J_E7, D1–D7, R1–R7)

**Position:** All electrode connectors along the bottom edge (J_E2, J_E3, J_E6, J_E7 at 0–10 mm from bottom edge) and top edge (J_E4, J_E5 at 0–10 mm from top edge). J_E1 centred around the E1 aperture cutout.

**Placement rule:** TVS diodes (D1–D7) must be placed between each connector pad and the safety resistor, with no PCB trace between the connector and TVS input. Maximum distance from connector pad to TVS input: 2 mm.

**Safety resistors (R1–R7):** Within 3 mm of corresponding TVS output. Oriented perpendicular to the signal flow direction to minimise footprint along the signal path.

**Routing within this step:**
- All electrode pad-to-TVS traces on BOTTOM (L4), 0.50 mm wide
- TVS cathode to AGND via on L2: 0.4 mm drill, within 0.5 mm of TVS GND pad
- Post-resistor signal traces on BOTTOM (L4), 0.10–0.12 mm wide, transitioning to the InAmp inputs

**Active electrode phantom power (E2, E3):**
- R_PH1, R_PH2 (560 Ω) on TOP (L1), within 3 mm of J_E2 and J_E3 connector pads respectively
- C_BLK1, C_BLK2 (10 µF 0805) on BOTTOM (L4), within 3 mm of corresponding phantom resistor return pad
- R_SHD1, R_SHD2 (100 Ω) on BOTTOM (L4), in the shield return path, within 2 mm of NET_E2_FILT and NET_E3_FILT nodes

---

### Step 3 — ICG Signal Chain (U10, U3, U4, and VCCS components)

**Zone:** Left-centre of board, 2–26 mm from left edge, 10–35 mm from top edge.

**Sub-placement order within this step:**

1. **U10 (SG-210STF oscillator):** Place first within the ICG zone, minimum 10 mm from TPS60400 (U11). Orient with output pin (pin 3) facing U4 AD630 to minimise OSC_REF trace length.

2. **U4 (AD630 SOIC-16):** Place with pin 9 (SELECT/REF+) facing U10 output. OSC_REF trace from U10 to U4 pin 9 must be routed on BOTTOM (L4) with GND guard traces, total length ≤ 20 mm.

3. **U3 (AD8221 MSOP-8):** Place with differential inputs (pins 2, 3) facing the electrode connector side (bottom edge) and output (pin 6) facing U4 pin 1 (CHA IN+). Gain resistor R_G3 (5.49 kΩ) between pins 1 and 8, placed within 1 mm of both RG pins.

4. **ICG VCCS components (discrete op-amp + resistors):** In a dedicated sub-zone physically separated by ≥ 3 mm and a GND trace from the ICG sense zone (U3 inputs). VCCS output traces to J_E4 and J_E5 run directly along the board edge.

5. **R_G3 gain resistor:** Within 1 mm of U3 RG pins. Orientation: long axis parallel to the pin-to-pin direction.

**Critical ICG trace rules:**
- NET_ICG_INAMP_OUT (U3 pin 6 → U4 pin 1): maximum 10 mm, on BOTTOM (L4), GND guard both sides
- NET_OSC_REF (U10 pin 3 → U4 pin 9): maximum 20 mm, on BOTTOM (L4), GND guard both sides, 50 Ω characteristic impedance
- The 32 kHz carrier traces on the same layer must not pass within 3 mm of ECG InAmp (U1, U2) inputs

---

### Step 4 — Power Supply ICs (U8 LT3042, U11 TPS60400): Top-Left

**Zone:** 0–20 mm from left edge, 0–15 mm from top edge.

**U8 (LT3042 DFN-8):** Top-left corner, within 5 mm of J1 VBAT pins. VIN pin facing J1 flex connector; VOUT pin facing board centre (AVDD distribution). Exposed GND pad with 4-via array to L2. C_IN_LT (10 µF) within 1 mm of VIN; C_OUT_LT (10 µF + 100 nF) within 1 mm of VOUT; C_SET (10 nF) within 1 mm of SET pin; R_SET (33.2 kΩ) within 3 mm of SET pin, with return to AGND pour.

**U11 (TPS60400 SOT-23-5):** Within 5 mm of U8, but separated by ≥ 5 mm from U10 oscillator. Flying capacitor C_P1 (100 nF) placed between U11 pins 2 and 3, within 2 mm of each pin. C_IN_CP and C_OUT_CP within 1 mm of respective V+ and VOUT pins. LC filter inductor L1 within 5 mm of U11 VOUT, with C_AVSS on AVSS (filtered) side.

**VBAT trace:** Runs from J1 pins 1 and 2 directly to U8 VIN via the PTC fuse. Width: 0.60 mm on L1. No other components or traces tapped from this net between the fuse and U8 VIN.

---

### Step 5 — Decoupling Capacitors

All local 100 nF decoupling capacitors are placed in this step, after all ICs are positioned.

**Rule:** Every AVDD power pin must have its 100 nF within 0.5 mm of the IC pad, on the same layer as the IC, between the power pin and the nearest AGND via. The capacitor is placed on the direct line between the power pin and the via — not adjacent to the IC but with a detour.

**Placement priority:** ADS1256 (U6) AVDD first (most sensitive), then AD8221 (U3), then AD630 (U4), then LT3042 (U8), then remaining ICs.

**Bulk capacitors (10 µF per block):** Placed in the geometric centre of each functional block zone after local decoupling is confirmed.

**REF5025 filter capacitor:** C_FILT_REF (10 µF) at pin 5 (FILT) must be within 2 mm of the pin — this is mandatory for achieving the REF5025's low-noise performance specification. The C_OUT_REF (10 µF + 100 nF) within 1 mm of pin 3 (OUT/VREF).

**MAX30101 VLED decoupling (Vasquez mandatory condition):** C_LED_BULK (10 µF 0603) within 2 mm of MAX30101 pin 3 (VLED). The 100 nF X7R within 0.5 mm of pin 3. These capacitors absorb the 10–50 mA LED pulse currents and prevent them from coupling through the AVDD rail into ICG or ECG chains.

---

### Step 6 — ESP32 Flex Connector J1 and SPI/I²C Pads: Top Edge, Centre

**J1 (Hirose FH12-24S-0.5SH, 24-pin FFC/FPC):** Placed along the top edge of the board, centred (approximately 11–18 mm from left edge). Connector body extends off the top edge; mating direction is from the top.

**Pin assignment routing from J1:**

| Pin group | Routing notes |
|-----------|---------------|
| VBAT (pins 1, 2) | 0.60 mm traces directly to PTC fuse and LT3042 VIN; shortest possible path |
| DGND (pins 3, 4) | To DGND island near ADS1256 DVDD area |
| AGND (pin 5) | To L2 AGND pour via 0.4 mm via at J1 footprint edge |
| DVDD (pin 6) | To ADS1256 pin 22 via 0.15 mm trace; short, direct |
| SPI bundle (pins 7–11) | Length-matched group (±0.5 mm), routed on TOP (L1), direct to U6 SPI pins; no vias in the SPI traces if avoidable |
| I²C bus (pins 12, 13) | 0.15 mm traces to pull-up resistors (R_SDA, R_SCL to AVDD) then to U9, U12, U13 in daisy-chain sequence |
| PPG_INT (pin 14) | 0.10 mm to U12 pin 9 |
| OSC_MON (pin 15) | Via R_OSC (33 Ω, within 2 mm of pin 15) to U10 output |

**Mechanical strain relief keep-out zone:** A 3 mm × 25 mm keep-out zone for all components (not just connectors) is defined along the top edge of the board (from y = 0 to y = 3 mm, across the full 25 mm board width). Only the J1 connector body and its landing pads are permitted in this zone. No vias within 1 mm of the J1 footprint edge clips.

---

## 5. Flex Cable Interface

### 5.1 Connector Specification

| Parameter | Value |
|-----------|-------|
| Connector type | Hirose FH12-24S-0.5SH (or equivalent ZIF 24-pin 0.5 mm pitch FFC) |
| Pitch | 0.50 mm |
| Number of contacts | 24 |
| Contact pad width | 0.30 mm |
| Contact pad length | 1.50 mm |
| Pad pitch | 0.50 mm (0.30 mm pad + 0.20 mm gap) |
| Connector body width (along pad row) | 12.65 mm |
| Connector body depth | 5.75 mm |
| Mating direction | Top edge of PCB, cable folded toward board back |
| Locking | ZIF (zero insertion force) actuator |
| Contact plating on PCB pads | ENIG (mandatory — matches ENIG board finish; pure tin HASL not acceptable at 0.5 mm pitch) |

### 5.2 Flex Cable Parameters

| Parameter | Value | Justification |
|-----------|-------|---------------|
| Cable type | Type A FFC (conductors on same side), 24-pin | Type A allows direct top-to-top connection between the two PCBs when the boards are face-to-face in the stacked assembly |
| Cable conductor pitch | 0.50 mm | Matches J1 and corresponding J1 on top PCB |
| Conductor width | 0.30 mm | Standard for 0.5 mm pitch FFC |
| Number of conductors | 24 | Full pinout per Section 9 of schematic_bottom_pcb_001.md |
| Cable thickness | 0.20 mm | Standard FFC; fits within 0.3 mm flex PCB slot in housing stack |
| Maximum cable length | 15 mm | Housing stack depth (see pcb_layout_001.svg) is approximately 6 mm between PCB surfaces; 15 mm allows for cable loop and alignment tolerance |
| Insulation material | Polyimide (Kapton) | Required for 60°C+ thermal capability in body-worn device; PET acceptable at lower cost if operating temperature confirmed < 50°C |

### 5.3 Mechanical Strain Relief and Keep-Out Zones

**Keep-out zone — no components:** 3 mm × 25 mm zone along the top PCB edge (from board edge to y = 3 mm, full 25 mm width). Reason: The FFC connector actuator and cable exit zone must remain clear of tall components to allow cable routing and ZIF actuator operation.

**Keep-out zone — no vias:** 1 mm × 14 mm zone centred on the J1 land pattern (x = 5.7 mm to x = 19.4 mm, y = 0 to y = 1 mm). Reason: Vias under or immediately adjacent to FFC pads can create solder bridges or undermine pad adhesion.

**Cable bend radius:** Minimum bend radius of FFC cable ≥ 1.5 mm (3× cable thickness). The housing stack geometry provides approximately 2 mm radius at the cable exit point — acceptable.

**PCB anchor pads:** J1 connector has two mechanical anchor pads (1.5 × 2.0 mm, at each end of the connector body). These must be soldered to the PCB and provide pull-out strength. Anchor pad copper must connect to L2 GND pour for EMI shielding at the cable entry point.

---

## 6. DRC Checklist — Pre-Fabrication Verification

The following 10-point checklist must be completed and signed off before Gerber files are submitted to the fabrication house.

---

**Item 1 — Minimum clearance violations**  
Verify: No trace-to-trace clearance < 0.15 mm on any layer. No trace-to-copper-pour clearance < 0.20 mm. No analog-to-digital zone boundary violation < 0.30 mm. Check: Run DRC in KiCad with custom design rules file. Expected DRC error count: zero.

---

**Item 2 — Electrode trace isolation (IEC 60601-1 BF)**  
Verify: All patient-circuit conductors (NET_E1_PAD through NET_E7_PAD and their post-TVS nets) maintain ≥ 3.0 mm clearance from any digital trace or digital copper pour on all layers. Measure creepage path in KiCad 3D viewer for the board edge path between electrode connectors and the nearest digital component. Expected minimum: 3.0 mm creepage along board surface.

---

**Item 3 — OSC_REF trace length and shielding**  
Verify: NET_OSC_REF total length from U10 pin 3 to U4 pin 9 ≤ 20 mm. Guard traces present on both sides of the full OSC_REF trace length. GND stitch vias on guard traces at ≤ 5 mm intervals. Layer: BOTTOM (L4) only, no vias in the trace itself. Verify characteristic impedance is approximately 50 Ω by inspecting trace width (0.15 mm) and dielectric stack (1.6 mm FR4, L2 GND at ~0.36 mm distance from BOTTOM layer).

---

**Item 4 — ADS1256 SPI trace length matching**  
Verify: Lengths of NET_ADC_MOSI, NET_ADC_MISO, NET_ADC_CLK (from J1 land pattern to U6 land pattern) match within ±0.5 mm. Measure using KiCad length tuning tool or trace length report. Document the three measured lengths in the fab package.

---

**Item 5 — Decoupling capacitor placement**  
Verify: Every AVDD power pin on U1–U13 has a 100 nF capacitor within 0.5 mm of the pad. Check the following critical ones individually: U6 AVDD (pin 4), U6 DVDD (pin 22), U3 AVDD (pin 7), U8 VOUT (LT3042, 10 µF within 1 mm), U7 VREF output (10 µF within 1 mm of REF5025 FILT pin), U12 VLED (10 µF within 2 mm per Vasquez mandatory condition). Any missing or misplaced decoupling cap is a blocking issue.

---

**Item 6 — TPS60400 flying capacitor trace integrity**  
Verify: C_P1 is within 2 mm of U11 pins 2 and 3. Trace width from pins 2 and 3 to C_P1 is ≥ 0.40 mm. No signal traces route under or within 2 mm of U11 footprint on BOTTOM (L4). Verify no AVSS_RAW trace runs parallel to any ICG or ECG signal trace for more than 5 mm.

---

**Item 7 — GND plane continuity**  
Verify: L2 GND pour is a single continuous polygon with no slots or splits under the analog signal section (ICG, ECG, ADC zones). Inspect the GND pour in KiCad copper layer view. Acceptable: small clearance holes for vias. Unacceptable: any trace-forced slot that cuts across the GND pour under U3, U4, U6, or U7. The only intentional split is the narrow bridge at the AGND/DGND star point at U6 pin 6.

---

**Item 8 — Via drill and annular ring specification**  
Verify: All signal vias have drill ≥ 0.3 mm and pad ≥ 0.6 mm (annular ring ≥ 0.15 mm per IPC-2221 Class 2). All power vias have drill ≥ 0.4 mm and pad ≥ 0.8 mm. LT3042 thermal pad vias: 4 vias × 0.3 mm drill, all marked tented in Gerber (solder mask defined, filled). Confirm via-in-pad marking is correct in fabrication notes.

---

**Item 9 — Flex connector pad and mechanical keep-out**  
Verify: J1 24-pin land pattern matches Hirose FH12-24S-0.5SH datasheet footprint (pitch 0.50 mm, pad 0.30 × 1.50 mm). Both mechanical anchor pads are present and connected to L2 GND pour. 3 mm × 25 mm component keep-out zone on top edge is free of all components. 1 mm × 14 mm via keep-out under J1 pads is free of all vias. Confirm ENIG is specified on the fab order — no HASL.

---

**Item 10 — Analog/digital ground star point and VREF routing**  
Verify: There is exactly one copper bridge between the AGND pour and DGND island; this bridge is at U6 pin 6, width ≤ 0.5 mm. Confirm by visual inspection of L2 copper layer in KiCad. Verify: NET_VREF (REF5025 output to ADS1256 VREFP) is routed with GND guard traces on both sides for its entire length. No switching or high-current trace is within 1 mm of the VREF trace. Confirm VREFN (U6 pin 16) connects directly to the AGND pour with no shared via with any digital return current path.

---

**Item 11 — ICG injection-to-sense separation**  
Verify: The minimum clearance between any injection-path trace (ICG_SRC_P, nets to J_E4/J_E5) and any sense-path trace (NET_E2_FILT, NET_E1_FILT post-safety-resistor, NET_E3_FILT) is ≥ 5.0 mm at all points. Verify: No parallel run between injection and sense traces exceeds 2.0 mm continuous length. Verify: VCCS sub-zone boundary GND trace is present and stitched to L2 at ≤ 3 mm intervals. Verify: Driven-shield buffer placement is within the sense zone (bottom edge), ≥ 5 mm from any injection-path trace. If split-GND contingency is used, verify: exactly one copper bridge between GND half-planes, bridge width ≤ 0.5 mm, at top-left corner location.

---

**Item 12 — Rigid-flex zone compliance**  
Verify: No vias placed within the flex zone (between the two rigid zone boundaries). Verify: All layer transitions (rigid to flex copper connection) occur within the rigid zones, ≥ 1.5 mm from each rigid-flex boundary. Verify: Stiffener regions are defined in the fab drawing at both transition edges. Verify: No analog biopotential signal traces are routed on the flex zone (F1/F2 layers). Verify: Flex F2 GND return pour is continuous across the full flex zone width; GND return stitching vias to L2 are placed ≤ 1.5 mm from each flex boundary. Verify: Minimum bend radius of the flex strip is 3.0 mm in the assembled housing geometry (check against housing 3D model).

---

## Revision History

| Rev | Date | Author | Changes |
|-----|------|--------|---------|
| 0.1 | 2026-05-09 | Priya Nair | Initial issue — based on schematic Rev 0.1, netlist Rev 0.1, Vasquez placement validation, component selection Rev 0.3 |
| 0.2 | 2026-05-09 | Priya Nair | Principal design change: two 52×25mm boards are ONE rigid-flex PCB (Sec 1.3). Added analog shielding rules (Sec 2.6): GND pour enclosure, guard rings, coplanar waveguide on 32kHz traces, shield can recommendation, unbroken ADS1256 GND plane. Added ICG injection/sense separation rules (Sec 2.7): 5mm clearance, no parallel runs >2mm, VCCS zone isolation, driven-shield buffer on sense leads, split-GND contingency. Added DRC items 11 (ICG separation) and 12 (rigid-flex compliance). |

---

*Document: `operations/electronics/pcb_routing_spec_001.md`*  
*Author: Nair — Electronics — Rev 0.2 — 2026-05-09*  
*Next actions: Vasquez to confirm Section 2.3 electrode clearance value is consistent with BF certification path. Vasquez to review split-GND contingency (Sec 2.7.4) before implementation if clearance cannot be met. Müller to review J1 SPI pin assignment against ESP32-S3 GPIO allocation. Housing team to confirm flex strip bend radius ≥ 3.0 mm in assembled geometry.*
