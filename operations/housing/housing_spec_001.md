# VU-AMS Housing Design Specification
## Document: housing_spec_001 — Revision 0.1

**Author:** Tom Voss — Industrial & Product Design  
**Date:** 2026-05-09  
**Ref:** housing_brief_update_001.md · pcb_layout_001.svg · brief_001_addendum_A.md · brief_001_addendum_B.md · vasquez_placement_validation_001.md  
**Status:** DRAFT — For review by Nair (PCB/connector interface), Vasquez (electrode geometry), Lam (IP-rating sign-off)

---

## 1. External Dimensions with Tolerances

### 1.1 Nominal Envelope

| Dimension | Nominal | Tolerance | Notes |
|-----------|---------|-----------|-------|
| Length (X) | 55.0 mm | ±0.3 mm | Parallel to electrode cable exits, top-to-bottom |
| Width (Y) | 55.0 mm | ±0.3 mm | Perpendicular, harness attachment axis |
| Depth (Z) | 22.0 mm | ±0.3 mm | Skin-to-back-cover axis; critical — see Section 8 |
| Corner radius | 14.0 mm | ±0.2 mm | All external corners, front and rear faces |

### 1.2 Parting-Line Offset

The housing splits at Z = 10.5 mm from the skin-face. This is not at mid-depth: the front shell (skin-facing) is the shallower half (nominally 10.5 mm interior depth) to facilitate PCB drop-in assembly; the rear cover is the deeper half (nominally 11.5 mm interior depth including the PCB stack accommodation). The offset is intentional — see Section 9.

### 1.3 Functional External Features

| Feature | Location | Nominal dimension |
|---------|----------|-------------------|
| E1 electrode aperture | Upper-centre face (skin side) | Ø 12.0 mm, recessed 0.5 mm |
| PPG optical window | Lower-centre (skin side) | 7.0 × 5.0 mm |
| USB-C port aperture | Top edge, centre | 9.5 × 3.5 mm clear opening |
| E4 cable exit | Top edge, left | Ø 5.5 mm, 45° to vertical |
| E5 cable exit | Top edge, right | Ø 5.5 mm, 45° to vertical |
| E2/E3 cable exit | Bottom edge, left-of-centre | Ø 6.5 mm (dual-cable, common grommet) |
| E6/E7 cable exit | Bottom edge, right-of-centre | Ø 6.5 mm (dual-cable, common grommet) |
| Harness lug, upper-left | Top-left corner, rear face | 6.0 × 4.0 mm slot, centred 8 mm from corner |
| Harness lug, upper-right | Top-right corner, rear face | 6.0 × 4.0 mm slot, centred 8 mm from corner |
| Harness lug, bottom-centre | Bottom edge, rear face | 6.0 × 4.0 mm slot, centred |
| Status LED window | Front face, centre | Ø 4.0 mm, frosted PC insert |

---

## 2. Material Selection

### 2.1 Housing Corpus — Front Shell and Rear Cover

| Property | Specification |
|----------|---------------|
| Base material | PC/ABS blend — Covestro Bayblend T65 XF or equivalent |
| Flame rating | UL 94 V-0 at 1.5 mm wall thickness |
| Colour (moulded) | Black, pigmented through — RAL 9005 Jet Black |
| Nominal wall thickness | 1.5 mm (uniform); 2.0 mm at bosses and lug reinforcements |
| Tensile strength | ≥ 55 MPa (ISO 527) |
| Impact (notched Izod) | ≥ 55 J/m at 23°C (ISO 180) |
| Chemical resistance | Resistant to common skin-contact substances: sweat (NaCl, lactic acid, urea), isopropanol wipe-down, ultrasound gel |
| Biocompatibility | ISO 10993-5 cytotoxicity — confirm with material supplier for skin-contact grade; no free halogens |
| Non-conductive | Bulk resistivity > 10¹³ Ω·cm — IEC 60601-1 Type BF requirement; no metal-filled grades |

**Rationale for PC/ABS over pure ABS:** superior low-temperature impact resistance (relevant for outdoor use), better dimensional stability across temperature, higher HDT. Avoids the brittleness of pure PC at thin walls.

### 2.2 Skin-Contact Overmould Layer

| Property | Specification |
|----------|---------------|
| Material | Medical-grade silicone — LSR (Liquid Silicone Rubber) |
| Shore hardness | A25 |
| Biocompatibility | ISO 10993-5 cytotoxicity tested; ISO 10993-10 sensitisation |
| Thickness | 0.5 mm nominal over flat areas; 0.8 mm at PPG concave zone |
| Colour | Black (reduces optical interference with MAX30101 ambient light path) |
| Coverage | Full skin-contact face (front shell), excluding E1 aperture rim and PPG window seal zone |
| Bond method | Overmoulded in 2-shot injection mould or adhesive-bonded with Dow Corning 3140 primer |

### 2.3 Electrode Contact Material — E1

| Property | Specification |
|----------|---------------|
| Electrode type | Snap-on dry Ag/AgCl disc |
| Electrode diameter | Ø 10.0 mm active area (within Ø 12.0 mm aperture) |
| Substrate | Sintered Ag/AgCl, 0.3 mm thick |
| Biocompatibility | ISO 10993-10 skin sensitisation; latex-free |
| Contact interface | Stainless-spring clip — see Section 2.4 |
| Replaceable by user | Yes — snap-fit, no tools |

### 2.4 Electrode Clip and Strap Attachment Hardware

All metal hardware exposed to environment or skin-adjacent moisture path:

| Component | Material | Standard |
|-----------|----------|----------|
| E1 electrode clip (spring retainer) | 316L stainless steel, electropolished | ASTM A276 |
| Harness lug through-bolts | M3 × 8 mm 316L stainless, hex socket cap | DIN 912 |
| Harness lug loop bar | 316L stainless, Ø 3 mm rod, bent |  |
| PCB standoff screws | M2 × 5 mm 316L stainless, pan head Phillips | DIN 7985 |
| Rear cover screws | M2 × 6 mm 316L stainless, hex socket cap | DIN 912 |
| Heat-set inserts (PCB bosses, cover bosses) | Brass M2 × 4 mm, knurled | standard ultrasonic insert |

316L selected for corrosion resistance in sweat (chloride) environment. All hardware is non-ferrous — no magnetic interference with ICG measurement.

### 2.5 USB-C Protective Cap

| Property | Specification |
|----------|---------------|
| Material | PA66-GF30 (polyamide, 30% glass fibre) |
| Colour | RAL 3020 Traffic Red — distinctive from all other housing elements |
| Marking | "REMOVE BEFORE CHARGING" — engraved, minimum 2.5 mm character height |
| Attachment | Bayonet quarter-turn (90°) with stainless spring detent |
| Sealing | O-ring, NBR, Ø 8.5 mm × 1.5 mm cross-section, seated in cap body |
| User operation | Requires intentional quarter-turn — not removable by accidental pull |

### 2.6 PPG Optical Window

| Property | Specification |
|----------|---------------|
| Material | BK7 optical glass (prototype phase); upgrade to Al₂O₃ sapphire Rev 2 if field wear demands it |
| Thickness | 0.8 mm |
| Transmission at 660 nm | > 92% |
| Transmission at 940 nm | > 92% |
| Bonding | UV-cured optical adhesive — Norland NOA 61 or equivalent |
| Clear aperture | 7.0 × 5.0 mm |

### 2.7 Light-Seal Ring (PPG)

| Property | Specification |
|----------|---------------|
| Material | Medical-grade black silicone, LSR |
| Shore hardness | A40 |
| Ring width (radial) | 1.5 mm |
| Nominal compression | 0.8 mm at design wear pressure (~10–16 g/cm²) |
| Function | Optical seal (ambient light rejection) and skin-contact compliance |
| Mounting | Permanently seated in moulded groove in front shell — not user-replaceable |

---

## 3. PCB Mounting — Standoff Positions, Flex Cable Routing, Strain Relief

### 3.1 PCB Stack Orientation

The two PCBs (each 52 × 25 mm) are stacked in the Z-axis with the flex cable bridge between them:

- **Bottom PCB (Analog):** skin-facing side. Sits closest to front shell inner face.
- **Flex PCB:** 0.3 mm thick, connects the two PCBs via a horizontal bridge. Routed through the centre of the stack, 1.0 mm clearance top and bottom.
- **Top PCB (Digital):** faces rear cover interior.

### 3.2 Standoff Positions

Four primary standoff bosses mount the analog PCB to the front shell:

| Standoff | X position (from left inner wall) | Y position (from top inner wall) | Height (Z) |
|----------|----------------------------------|----------------------------------|------------|
| STD-1 (top-left) | 2.5 mm | 2.5 mm | 1.0 mm |
| STD-2 (top-right) | 22.5 mm | 2.5 mm | 1.0 mm |
| STD-3 (bottom-left) | 2.5 mm | 49.5 mm | 1.0 mm |
| STD-4 (bottom-right) | 22.5 mm | 49.5 mm | 1.0 mm |

Standoffs are moulded bosses in the front shell, 4.0 mm outer diameter, M2 heat-set brass insert, 4.0 mm insert length. PCB screw: M2 × 5 mm 316L stainless pan-head Phillips. Standoff height of 1.0 mm provides the specified 1.0 mm lower PCB clearance gap (see Section 8).

The digital PCB is held by the flex cable tension and a secondary retention bracket — a formed 316L sheet-metal clip (0.3 mm thick) clipped over the digital PCB edge near the USB-C connector, anchoring to the rear cover interior bosses. This prevents relative movement of the digital PCB without adding additional screws through the stack.

### 3.3 Flex Cable Routing Path

```
Analog PCB (skin-side)
  |
  └── Flex connector at Y = 32–40 mm (lower third of PCB, centred at X = 12.5 mm)
        |
        Flex PCB folds 180° around a 4.0 mm radius former — silicone rubber
        Former clips to standoff STD-3/STD-4 bridge post
        |
        └── Connects to digital PCB flex connector, mirrored position
              Y = 32–40 mm, X = 12.5 mm on digital PCB
```

The flex cable routing avoids the E1 electrode aperture zone (upper PCB area) and the battery wells. The 4.0 mm fold radius is the minimum safe bend radius for the flex construction (0.3 mm rigid flex, copper trace width 0.2 mm — verify with Nair at flex spec freeze).

### 3.4 Strain Relief

All external cable exits use moulded silicone grommets:

| Parameter | Specification |
|-----------|---------------|
| Grommet material | Black silicone, Shore A50 |
| Grommet wall embedment depth | 6.0 mm into housing wall |
| Cable diameter accommodated | Ø 3.0 mm (single), Ø 6.5 mm (dual, common grommet) |
| Minimum bend radius at exit | 8.0 mm (verify at cable freeze) |
| Exit angle, E4 and E5 | 45° from vertical, directed toward respective shoulder |
| Exit angle, E2/E3 and E6/E7 | Perpendicular to bottom edge, angled 15° toward skin-face |

Grommets are retained by moulded undercut in housing wall and locked by the outer housing assembly. They are user-replaceable by disassembly (M2 screws accessible from rear cover).

---

## 4. Battery Compartment — Retention, Thermal Pad, Fuse Location

### 4.1 Battery Form Factor

Two 14500 cells: Ø 14.0 mm × 50.0 mm, 3.7 V nominal, 1000 mAh each, parallel configuration = 2000 mAh total.

### 4.2 Battery Well Geometry

Each battery occupies a dedicated well in the front shell:

| Parameter | Left Well | Right Well |
|-----------|-----------|------------|
| Inner width (X) | 15.5 mm (14 + 1.5 mm foam) | 15.5 mm |
| Inner depth (Y) | 52.0 mm | 52.0 mm |
| Z depth | 16.5 mm (battery Ø 14 + 2.5 mm PCB clearance) | 16.5 mm |
| Inner wall to PCB edge | 1.0 mm clearance | 1.0 mm clearance |
| Outer wall thickness | 1.5 mm housing wall | 1.5 mm housing wall |

Cell-to-PCB X clearance (each side): 1.0 mm. This is the design clearance for cell foam padding. Total X stack-up: see Section 8.

### 4.3 Retention Clips

Each battery is retained by two stainless-spring clips per cell:

| Parameter | Specification |
|-----------|---------------|
| Material | 301 stainless spring steel, 0.25 mm thick |
| Clip height | 3.0 mm (grips cell Ø at mid-body) |
| Pre-load force | ~2.5 N radial per clip — sufficient to prevent rattling under 3g shock |
| Number per cell | 2 clips, at Y = 12.5 mm and Y = 37.5 mm from cell top |
| Electrical contact | Positive: spring leaf at cell top, welded tab to PCB rail. Negative: spring plate at cell bottom, welded tab. |
| Insulator | Kapton tape over clips where adjacent to non-battery conductors |

### 4.4 Thermal Management

| Item | Specification |
|------|---------------|
| Thermal pad, cell-to-housing | 0.5 mm silicone thermal pad, λ = 1.0 W/m·K, between cell OD and housing well outer wall |
| Purpose | Conduct waste heat from cells to housing exterior; prevent localised hot spots |
| Pad dimensions | 14.0 mm × 45.0 mm per cell |
| Cell maximum temperature | 60°C per cell manufacturer — housing wall outer surface shall not exceed 43°C at max charge rate (IEC 60601-1 surface temperature limit for applied parts adjacent to skin) |
| Thermal break | 1.0 mm air gap between battery wells and PCB stack — prevents cell heat from coupling to analog front-end components |

### 4.5 Fuse Location

The main battery fuse (polyfuse, rated per Nair's power budget) is located on the digital PCB (top PCB), at the battery positive input, within 5 mm of the positive spring contact pad. This is on the non-skin-facing PCB, accessible only by opening the rear cover. No fuse is accessible to the user without a tool. IEC 60601-1 compliance: fuse replacement is a service action, not a user action.

Additionally, each cell has a PTC fuse built into the spring contact assembly — a standard feature for 14500 cells; confirm with selected cell supplier.

---

## 5. Electrode Aperture Dimensions

### 5.1 E1 — Integrated Dry Electrode (Upper Face, Collarbone Contact)

| Parameter | Dimension |
|-----------|-----------|
| Housing aperture diameter | Ø 12.0 mm |
| Aperture recess depth | 0.5 mm below housing face |
| Electrode contact area | Ø 10.0 mm (Ag/AgCl disc, sintered) |
| Electrode type | Dry Ag/AgCl — reusable, snap-fit |
| Spring contact travel | 2.5 mm (spring leaf in PCB, pre-loaded at assembly) |
| Silicone seal ring O.D. | Ø 16.0 mm |
| Silicone seal ring I.D. | Ø 12.0 mm |
| Seal ring section height | 1.0 mm (nominal), Shore A25 |
| Seal ring compression | ~0.4 mm at typical wear pressure |
| Electrode position on housing | Centre-X = 27.5 mm from left edge; Centre-Y = 17.5 mm from top edge |

E1 is positioned in the upper portion of the housing face consistent with the PCB layout (collarbone contact). Gravity-driven contact: the harness keeps the device pressed upward so E1 contacts the suprasternal/clavicular area.

### 5.2 E4 / E5 — Back Electrode Leads, Shoulder Exit (Top Edge)

These are not apertures — they are cable exit grommets. The electrodes themselves are patch-type and are at the end of the lead wire.

| Parameter | E4 (Left) | E5 (Right) |
|-----------|-----------|------------|
| Grommet aperture in housing wall | Ø 5.5 mm | Ø 5.5 mm |
| Position (from top edge, into top wall) | Centred at 8.0 mm from left inner edge | Centred at 8.0 mm from right inner edge |
| Cable exit angle | 45° toward left shoulder / neck | 45° toward right shoulder / lower back |
| Electrode at lead end | Ag/AgCl gel patch, 30 mm × 22 mm, standard 3.5 mm snap |
| Lead wire length | 400 mm (E4, neck lead); 600 mm (E5, lower back lead) — confirm with Vasquez harness geometry |

### 5.3 E2 / E3 — Front Lower Leads, Solar Plexus (Bottom Edge)

| Parameter | Specification |
|-----------|---------------|
| Grommet aperture | Ø 6.5 mm (dual-cable, common grommet) |
| Position | 13.0 mm from left inner edge, centred on bottom wall |
| Exit angle | 15° angled toward skin face |
| Electrode type at lead end | Active Ag/AgCl clip (LMP7721 buffered, 2-wire phantom-powered) |
| Active electrode clip dimensions | 25 mm × 15 mm contact area |
| Lead wire length | 200 mm — solar plexus to device |

### 5.4 E6 / E7 — SCL/EDA Chest Electrodes (Bottom Edge)

| Parameter | Specification |
|-----------|---------------|
| Grommet aperture | Ø 6.5 mm (dual-cable, common grommet) |
| Position | 13.0 mm from right inner edge, centred on bottom wall |
| Exit angle | 15° angled toward skin face |
| Electrode type at lead end | Ag/AgCl gel patch, 20 mm × 15 mm |
| Inter-electrode spacing (at skin) | 20 mm nominal — confirm final value with Vasquez |
| Lead wire length | 150 mm each — lower chest left placement |

### 5.5 PPG Optical Aperture (Skin Face, Centre-Lower)

| Parameter | Specification |
|-----------|---------------|
| Clear opening | 7.0 mm × 5.0 mm, ±0.1 mm |
| Radial clearance to MAX30101 package | ~0.7 mm per side |
| Window material | BK7 glass, 0.8 mm, UV-bonded |
| Concave housing face radius | 150 mm (anatomical conformance, per Vasquez placement validation) |
| Concave zone radius on housing face | ±20 mm from PPG window centre |
| Light-seal groove depth | 0.8 mm, 1.5 mm wide, continuous perimeter |
| PPG aperture position | Centre-X = 27.5 mm; Centre-Y = 40.0 mm from top edge |

---

## 6. Sealing — USB-C Cap, Parting Line, O-Ring Groove

### 6.1 IP Rating Target

**IP54** (dust-protected, splash-resistant from any direction). This satisfies the IPX4 minimum for a chest-worn device in exercise/perspiration environments. The sealed connectors, grommet cable exits, and USB-C cap together achieve IP54 when all are correctly assembled.

Note: upgrade to IP55 or IP65 is possible in Rev 2 if field feedback requires it; the O-ring groove cross-sections below are designed to accommodate this upgrade with only a sealing-face finish change (not a mould change).

### 6.2 USB-C Cap Design

The protective cap is a functional medical safety element (IEC 60601-1 BF classification: patient must not inadvertently charge the device while wearing it).

**Cap body geometry:**
- Outer diameter: 14.0 mm
- Bayonet lug pitch: 2.5 mm (quarter-turn travel)
- Two lug positions, 180° apart
- Detent spring: 0.2 mm 301 stainless leaf, moulded-in during PA66 overmould step
- Detent click force: 3.0–5.0 N (confirmed audible and tactile)
- O-ring groove: Ø 8.5 mm × 1.5 mm cross-section, 30% squeeze at full engagement
- Colour: RAL 3020 Traffic Red (through-pigmented PA66)
- Marking: "REMOVE BEFORE CHARGING" engraved (0.4 mm depth), pad-printed black fill

**Cap-to-housing interface:**
- Housing bayonet female socket moulded into top-edge wall, PC/ABS
- Socket alignment guide: 0.5 mm lead-in chamfer for blind insertion
- Retention when locked: >15 N axial pull-out force before unlocking

### 6.3 Main Housing Seal — Parting Line and O-Ring Groove

**Parting line location:** The split runs on a plane parallel to the skin face, at Z = 10.5 mm from the outer face of the front shell. The parting line is on the side walls of the housing, running continuously around all four sides at this depth. This positions the split behind the silicone overmould zone on the front shell, making the parting line invisible from the front face and ensuring the critical optical and electrode surfaces are on a single moulded part (no parting-line witness marks on skin-contact faces).

**O-ring groove specification (main housing joint):**

| Parameter | Dimension |
|-----------|-----------|
| O-ring cross-section | 1.5 mm (AS568 series 0) |
| O-ring nominal I.D. | Follows inner perimeter of housing — 49.0 mm × 49.0 mm square loop |
| Groove depth | 1.1 mm (73% fill — standard elastomer groove practice) |
| Groove width | 1.9 mm |
| Groove radius (corners) | 3.0 mm (prevents O-ring cutting at corners) |
| O-ring material | EPDM (resistance to sweat, UV, IPA cleaning) |
| O-ring compression at assembly | 0.4 mm (27% compression — in range for IP54 face seal) |
| Groove location on front shell | Moulded groove at top face of front shell sidewall, 1.0 mm inboard of outer surface |

**Cable exit grommets:** Each grommet provides its own radial seal (interference fit between grommet OD and housing bore, plus the cable OD). No additional sealant required at grommet installation.

### 6.4 Parting Line Cosmetic Treatment

The parting line at Z = 10.5 mm runs around the device side walls. It will have a witness line. Treatment:
- Mould tool: shut-off angle at parting line ≥ 3° to ensure clean separation
- Post-mould: parting line not sanded (risks dimensional distortion); acceptable as a functional line consistent with the matte texture
- The parting line is oriented to face laterally — not visible in frontal or dorsal views of the worn device

---

## 7. Assembly Sequence

Tools required: M2 hex driver (1.5 mm), UV lamp (365 nm, for NOA 61), calibrated torque screwdriver (0.1–0.5 N·m range).

### Step 1 — Front Shell Sub-Assembly Preparation

1a. Inspect front shell moulding: check E1 aperture recess, PPG window recess, cable grommet bores, and O-ring groove for flash, short-shots, or sink marks. Reject if any parting-line flash > 0.2 mm in O-ring groove.

1b. Press M2 brass heat-set inserts into PCB standoff bosses (STD-1 through STD-4) using ultrasonic insertion tool. Verify flush to ±0.1 mm.

1c. Press M2 heat-set inserts into rear cover attachment bosses (4 corners) of front shell. Verify flush.

1d. Bond PPG optical window: apply Norland NOA 61 to window perimeter, position BK7 glass in recess, apply 1.5 N seating force, cure at 365 nm UV, 60 s minimum. Wipe excess adhesive flush.

1e. Seat PPG light-seal silicone ring in moulded groove. Confirm ring is fully seated, no lifted sections.

1f. Install silicone cable grommets (E4, E5, E2/E3, E6/E7) into housing bores. Press to full embedment (6.0 mm). Confirm Ø 3.0 mm or Ø 6.5 mm inner bore is clear.

### Step 2 — Battery Installation

2a. Apply 0.5 mm silicone thermal pad to outer wall of each battery well (left and right), adhesive-backed, cut to 14 mm × 45 mm.

2b. Insert left 14500 cell into left well, positive terminal toward top. Confirm spring retention clips engage at two positions along cell body (click indication). Confirm positive spring-leaf contact is touching cell terminal.

2c. Repeat for right 14500 cell.

2d. Verify cell polarity — measure voltage across positive bus tabs. Expected: 3.6–4.2 V per cell. Do not proceed if voltage < 3.0 V or > 4.25 V.

### Step 3 — Analog PCB Installation

3a. Thread electrode cable leads (E4, E5, E2/E3, E6/E7) through pre-installed grommets from outside. Pull cables through until connector end is accessible inside housing. Confirm grommet inner bore is not split.

3b. Seat analog (bottom) PCB onto standoffs STD-1 through STD-4. Align E1 aperture on PCB with E1 aperture on front shell (through-hole in PCB, Ø 13.0 mm clearance hole). Align PPG sensor window on PCB with optical window in front shell (tolerance: ±0.3 mm in X and Y).

3c. Install 4× M2 × 5 mm screws into standoff inserts. **Torque: 0.08 N·m.** Do not exceed — PC/ABS boss wall is 1.5 mm; over-torque will crack insert surrounds.

3d. Connect electrode cable connectors to analog PCB headers (E4-J1, E5-J2, E2/E3-J3, E6/E7-J4 — connector positions per Nair schematic).

3e. Verify MAX30101 PPG sensor package is correctly aligned over optical window: visual check from outside, sensor centrally positioned over BK7 glass, no tilt.

### Step 4 — Flex Cable and Digital PCB Installation

4a. Route flex PCB through centre of housing. Fold flex to 180° around the 4.0 mm silicone former, which clips to the bridge post between STD-3 and STD-4. Confirm fold radius is not less than 4.0 mm — use the former; do not crease.

4b. Connect flex to analog PCB flex connector (ZIF, 0.3 mm pitch). Actuate ZIF latch.

4c. Lower digital (top) PCB onto flex connector (opposite end). Actuate ZIF latch on digital PCB flex connector.

4d. Install digital PCB retention clip (formed 316L sheet-metal clip) over digital PCB near USB-C end. Clip snaps to rear cover boss stubs. Confirm clip is fully seated.

4e. Connect battery positive/negative tabs to digital PCB battery terminals.

### Step 5 — Pre-Closure Electrical Check

5a. Apply 5V USB-C to USB-C port. Confirm BQ25895 charge LED indication (green or amber per firmware behaviour).

5b. Confirm BLE advertisement visible on nearby smartphone (VU-AMS device name).

5c. Confirm all five electrode channels report valid impedance on test jig (if available at this stage — optional at first article).

5d. Power off. Disconnect USB-C.

### Step 6 — O-Ring and Rear Cover Installation

6a. Seat EPDM O-ring in front shell O-ring groove. Apply thin film of silicone grease (Molykote 111 or equivalent) to O-ring OD — do not over-grease (excess grease collects particulates).

6b. Lower rear cover onto front shell. Confirm O-ring groove alignment (visual check at all four sides — O-ring should not be pinched or twisted).

6c. Start all 4× M2 × 6 mm rear cover screws by hand before torquing. This ensures housing halves are aligned before compression begins.

6d. Torque rear cover screws in cross-diagonal sequence: **0.12 N·m**, first pass all four; then **0.15 N·m** final torque. The cross-diagonal sequence ensures uniform O-ring compression.

6e. Verify O-ring is visible as compressed bead at all four sides. If not visible at any point, back off screws, reseat O-ring, retry.

### Step 7 — USB-C Cap and External Hardware

7a. Install USB-C cap onto bayonet socket. Insert at 0° alignment, rotate 90° clockwise until detent click. Confirm cap does not pull off with 10 N axial force test.

7b. Install harness lug hardware: 316L loop bar through each slot lug, retained by M3 × 8 mm screws into moulded M3 threaded inserts. **Torque: 0.35 N·m.**

7c. Final visual inspection: parting line uniform; O-ring bead visible; cable exits dressed at correct angles; USB-C cap red and marked.

### Step 8 — IP54 Seal Test (Batch Testing, 1-in-10 Minimum)

8a. Water splash test per IEC 60529 IPX4: 10 minutes exposure to water spray from all angles. Inspect interior by opening rear cover immediately after test for moisture ingress.

8b. Dust test per IEC 60529 IP5X: 8-hour talcum powder chamber. Inspect interior.

---

## 8. Critical Tolerance Stack-Up Analysis — 1.9 mm Z Constraint

### 8.1 The Problem

The housing external depth is 22.0 mm. The internal Z budget is consumed by the PCB stack, skin-contact layers, housing walls, and mounting clearances. The remaining margin — 1.9 mm — is small enough that a single design change can violate closure. This section documents the full stack-up so every stakeholder understands where the margin lives.

### 8.2 Z Stack-Up (Skin-Face to Back-Cover, Nominal)

| Layer | Nominal (mm) | Tolerance (mm) | Worst-case (mm) |
|-------|-------------|----------------|-----------------|
| Skin-contact silicone overmould | 0.50 | ±0.10 | 0.60 |
| PPG concave zone additional depth | 0.50 | ±0.10 | 0.60 |
| Front shell wall | 1.50 | ±0.10 | 1.60 |
| Lower PCB clearance (standoff height) | 1.00 | ±0.10 | 1.10 |
| Bottom PCB thickness | 1.20 | ±0.10 | 1.30 |
| Flex clearance (flex + air gaps) | 1.00 | ±0.15 | 1.15 |
| Top PCB thickness | 1.20 | ±0.10 | 1.30 |
| Top PCB components (ESP32 module, tallest) | 10.00 | ±0.30 | 10.30 |
| Upper PCB clearance | 0.80 | ±0.10 | 0.90 |
| Rear cover wall | 1.50 | ±0.10 | 1.60 |
| **Total nominal** | **19.20** | | |
| **Total worst-case (all tolerances add)** | | | **20.55** |
| **Available (housing internal depth nominal)** | **22.00** | | |

**Nominal margin: 22.00 − 19.20 = 2.80 mm**  
**Worst-case margin (all tolerances stack unfavourably): 22.00 − 20.55 = 1.45 mm**

The commonly cited "1.9 mm" is the conservative engineering margin — it represents the margin after applying ±1 sigma manufacturing variation, not full worst-case. This is the target design clearance used for component height sign-off: any component exceeding its nominal height by more than 1.9 mm (at the tallest stack point) breaches closure.

### 8.3 The Critical Constraint

**The tallest single item is the ESP32-S3-MINI-1-N8R8 module at ~10.0 mm (package height above PCB surface).** If Nair selects an alternative ESP32 module with greater height, or adds a component taller than approximately 5 mm above the digital PCB surface elsewhere in the stack, the Z margin is consumed.

**Standing instruction to Nair:** Any component change that increases the height profile of the digital PCB by more than 0.5 mm above the current BOM must be flagged to Voss before commit. No exceptions.

### 8.4 O-Ring Compression Contribution

The O-ring groove is cut into the front shell sidewall, not through the Z-axis face. The O-ring compresses radially on the side-wall mating face, contributing zero to the Z stack-up. This was a deliberate design decision — if the O-ring were a face seal on the Z plane, its 0.4 mm compression would directly consume margin.

### 8.5 Mitigation Options if Z is Violated

Priority order:
1. Reduce upper PCB clearance from 0.80 mm to 0.50 mm (saves 0.30 mm; acceptable if no PCBA warp in that zone)
2. Reduce rear cover wall to 1.20 mm (saves 0.30 mm; requires wall ribbing to maintain stiffness — additional mould cost)
3. Reduce lower PCB clearance from 1.00 mm to 0.75 mm (saves 0.25 mm; confirm no contact with PPG concave zone)
4. Increase housing external depth to 23.0 mm (last resort — changes all external mounting dimensions and harness geometry)

---

## 9. Mold Split Line Recommendation

### 9.1 Split Plane

**Recommended split: side-wall horizontal plane at Z = 10.5 mm from front face outer surface.**

This places the split:
- Behind (inboard of) the 0.5 mm silicone overmould on the front shell
- On the lateral walls only — no split line crosses the skin-facing face or the rear cover face
- In the zone of uniform 1.5 mm wall thickness — no boss interference

### 9.2 Two Halves

**Front Shell (Skin-Side Half):**
- Contains: E1 aperture, PPG window recess, light-seal ring groove, battery wells (full depth), PCB standoff bosses, grommet bores, harness lug slots, O-ring groove, silicone overmould zone
- Depth: 10.5 mm nominal external
- Core pull direction: straight Z-axis pull (no side-actions required except for grommet bores and E1 aperture)
- Side actions required: 2 × top-edge (E4, E5 grommet bores, 45° angle — requires angled core-pulls or hand-loads at prototype stage)

**Rear Cover (Back-Side Half):**
- Contains: rear face (flat with texture), USB-C cap bayonet socket, rear cover screw boss lands, battery retention clip anchor points
- Depth: 11.5 mm nominal external
- Straight Z-axis pull; no side-actions for the rear cover itself

### 9.3 Draft Angles

| Surface | Draft angle |
|---------|-------------|
| External side walls | 1.5° minimum |
| Battery well inner walls | 1.0° (tight, required for cell insertion clearance — reduce further if cell sticks) |
| PCB standoff bores | 0.5° (minimal, depth is short) |
| Grommet bore | 0° (grommet itself is flexible; no draft required) |
| O-ring groove | 0° side walls of groove (groove is perpendicular to pull direction) |

### 9.4 Steel-Safe Design Policy

All critical clearances are designed steel-safe: the housing bores and clearances are at maximum material condition in the tool, allowing steel to be removed (dimension opened) after first-article measurement. No dimension is designed such that adding steel to the tool is the only correction path.

---

## 10. Colour and Surface Finish

### 10.1 Housing Corpus

| Property | Specification |
|----------|---------------|
| Colour | RAL 9005 Jet Black, through-pigmented (not painted) |
| Exterior texture | VDI 3400 Reference 30 (equivalent to SPI D-2, medium bead blast) |
| Gloss level | < 5 GU measured at 60° (Gloss Units, ISO 2813) |
| Texture note | VDI 30 applied to all external surfaces except: (a) O-ring groove sealing faces (polished, Ra ≤ 0.4 µm), (b) PPG window recess (polished, Ra ≤ 0.8 µm), (c) E1 aperture recess (polished, Ra ≤ 0.8 µm) |
| Texture application | Mould-applied EDM texture — not secondary operation; tooling cost is one-time |

### 10.2 Skin-Contact Silicone Overmould

| Property | Specification |
|----------|---------------|
| Colour | Black (light-absorbing — reduces optical artefacts at PPG zone periphery) |
| Surface finish | Matte, smooth (no texture tooling required for LSR — naturally matte from mould release) |
| Ra target | ≤ 1.6 µm |

### 10.3 USB-C Cap

| Property | Specification |
|----------|---------------|
| Colour | RAL 3020 Traffic Red, through-pigmented PA66-GF30 |
| Surface finish | Smooth moulded, light matte (VDI 18 equivalent) |
| Marking contrast | Black pad-print on red ground — minimum contrast ratio 4.5:1 for legibility |

### 10.4 Rear Cover

Identical corpus specification to front shell (Section 10.1). No logo or branding markings on production units without separate sign-off.

### 10.5 Rationale for Matte Black

The VU-AMS is a clinical measurement device. Matte black minimises surface reflections that could interfere with optical sensors (PPG, ambient light from environment), reduces visible soiling from daily use, and presents a professional clinical appearance. Gloss finishes are explicitly excluded.

---

## 11. Open Items and Interface Dependencies

| Item | Owner | Priority | Blocking |
|------|-------|----------|----------|
| Confirm ESP32 module max height above PCB (current assumption 10.0 mm) | Nair | Critical | Section 8 stack-up |
| Confirm E1 clip type: snap-fit vs. threaded (affects aperture rim design) | Nair + Voss | High | Section 5.1 tooling |
| Confirm E2/E3 cable diameter (assumption Ø 3.0 mm per cable) | Nair | Medium | Section 3.4 grommet bore |
| IP-rating final target: IP54 confirmed or upgrade to IP55/IP65? | Lam | Medium | Section 6 sealing design |
| Vasquez sign-off on E6/E7 inter-electrode spacing (20 mm assumed) | Vasquez | Medium | Section 5.4 |
| BK7 vs. sapphire window: deferred to Rev 2 after prototype field test | Voss | Low | Section 2.6 |
| Harness lead lengths E4/E5 (400/600 mm assumed) | Vasquez | Medium | Section 5.2 harness geometry |

---

*Filed: `operations/housing/housing_spec_001.md`*  
*Voss — 2026-05-09*
