# VU-AMS — Battery Certification Plan
## REG-BATT-001, Rev 0.1

**Author:** Dr. Ingrid Wolff — Regulatory Affairs, Slow Horses  
**Date:** 2026-05-09  
**Status:** Draft — pending review by Lamb  
**Reference document:** BATT-001 Rev 0.1 (Okafor, Manufacturing & Production)  
**Primary supplier:** Large Power Co., Ltd. (ISO 13485)  
**Backup supplier:** Shenzhen Benzo Energy Technology Co., Ltd.  
**Device context:** VU-AMS wearable, IEC 60601-1 BF-type applied part, 2× 14500 Li-ion cells

---

## 1. IEC 60601-1 Clause 15.4.3.4 — Requirements for Internal Lithium-Ion Cells

### Regulatory basis

IEC 60601-1:2005+AMD1:2012+AMD2:2020, clause 15.4.3.4 governs the use of lithium-ion accumulator cells in medical electrical equipment. The clause imposes the following mandatory requirements on the VU-AMS battery subsystem:

| Requirement | Clause reference | Applicability to VU-AMS |
|-------------|-----------------|------------------------|
| Each Li-ion cell must comply with IEC 62133-2 (Li-ion secondary cells) | 15.4.3.4(a) | Both cells in the 14500 2-cell configuration must carry a valid IEC 62133-2 certificate |
| The equipment must incorporate protection against overcharge, over-discharge, and overcurrent at cell level or system level | 15.4.3.4(b) | BMS/PCM implementation required; must be documented in the risk file |
| Charging voltage and current limits must not exceed the cell manufacturer's maximum ratings | 15.4.3.4(c) | Charging IC parameters to be verified against Large Power cell datasheet |
| Temperature monitoring or protection must be implemented to prevent thermal runaway | 15.4.3.4(d) | NTC or equivalent thermal protection in BMS required; validated by Vasquez |
| The design must demonstrate that a single fault in the protection circuit does not render the device unsafe | 15.4.3.4(e) | Single-fault analysis required in FMEA (ISO 14971 scope) |

### Conformity evidence required for the Technical File

The following evidence must be present in the Device History File (DHF) prior to CE marking under the EU Medical Device Regulation (MDR 2017/745):

1. IEC 62133-2 test report for the specific cell type and lot, issued by an accredited laboratory (ILAC-accredited), from Large Power.
2. UN38.3 Summary Report for the cell as supplied (transport safety — IATA DGR Section II).
3. BMS/PCM design documentation demonstrating compliance with overcharge, over-discharge, overcurrent, and short-circuit protection requirements.
4. Cell datasheet (formal revision-controlled document from Large Power) specifying maximum charge voltage, maximum charge current, maximum continuous discharge current, and temperature ratings.
5. Evidence that the PCB charging circuit (Nair) does not exceed the parameters defined in the cell datasheet.
6. Thermal analysis or test evidence that the skin-worn operating temperature range (0–40 °C) is fully within the rated range of the selected cell.

### Action owner: Wolff (collection and DHF integration), Nair (BMS/PCM design documentation), Vasquez (thermal validation)

---

## 2. ISO 14971 Risk Entries — Battery Subsystem

The following hazard entries are added to the VU-AMS risk management file in accordance with ISO 14971:2019. Each entry covers: hazard, hazardous situation, potential harm, initial risk class (probability × severity, using the 5×5 matrix defined in the VU-AMS Risk Management Plan), control measures, and residual risk class.

---

### 2.1 Overcharge

| Field | Entry |
|-------|-------|
| Hazard | Excessive voltage applied to Li-ion cell during charging |
| Hazardous situation | Cell voltage exceeds maximum charge voltage (>4.25 V); internal pressure and temperature rise |
| Potential harm | Electrolyte venting, fire, explosion; burn injury to patient or user (BF applied part) |
| Initial probability | 3 — Occasional (charging IC fault or design error conceivable) |
| Initial severity | 5 — Catastrophic (fire risk on skin-worn device) |
| **Initial risk class** | **15 — Unacceptable; risk reduction required** |
| Control measures | (1) BMS hardware overcharge protection (primary); (2) Charging IC voltage clamp with tolerance per cell datasheet; (3) Software charge voltage limit (secondary, not sole control); (4) IEC 62133-2 certified cell with integrated CID or PCM |
| Residual probability | 1 — Remote |
| Residual severity | 5 — Catastrophic |
| **Residual risk class** | **5 — ALARP; benefit-risk justification required** |
| Verification | BMS overcharge test per IEC 62133-2 §7.3.1; charging circuit design review |

---

### 2.2 Over-discharge

| Field | Entry |
|-------|-------|
| Hazard | Cell discharged below minimum voltage |
| Hazardous situation | Cell voltage drops below 2.75 V; irreversible capacity loss, copper dissolution, internal short-circuit risk on subsequent recharge |
| Potential harm | Reduced device performance (patient receives incomplete measurement); secondary fire hazard on recharge of damaged cell |
| Initial probability | 3 — Occasional (deep discharge from prolonged storage or BMS fault) |
| Initial severity | 3 — Serious (performance harm primary; fire risk secondary) |
| **Initial risk class** | **9 — Undesirable; risk reduction required** |
| Control measures | (1) BMS hardware under-voltage cutoff (≥2.75 V per cell); (2) Low-battery alert in firmware at defined threshold above cutoff; (3) Storage instructions in IFU (ship/store above 3.0 V) |
| Residual probability | 1 — Remote |
| Residual severity | 3 — Serious |
| **Residual risk class** | **3 — Acceptable** |
| Verification | BMS over-discharge test; firmware low-battery alert functional test |

---

### 2.3 Thermal Runaway

| Field | Entry |
|-------|-------|
| Hazard | Uncontrolled exothermic reaction within Li-ion cell |
| Hazardous situation | Internal short circuit, cell mechanical damage, or overcharge triggers self-sustaining temperature increase; cell venting or rupture |
| Potential harm | Fire, explosion, toxic gas emission (HF); severe burn injury to patient (skin-worn device); secondary fire hazard to surroundings |
| Initial probability | 2 — Unlikely (controlled environment, certified cell) |
| Initial severity | 5 — Catastrophic |
| **Initial risk class** | **10 — Undesirable; risk reduction required** |
| Control measures | (1) IEC 62133-2 certified cell with proven thermal stability; (2) NTC thermistor on cell pack with BMS thermal cutoff; (3) Housing design: vent path to prevent pressure buildup (Okafor/Nair); (4) Over-temperature firmware alert; (5) Combination of controls for overcharge and over-discharge (see 2.1, 2.2) reduces initiating events |
| Residual probability | 1 — Remote |
| Residual severity | 5 — Catastrophic |
| **Residual risk class** | **5 — ALARP; benefit-risk justification required** |
| Verification | Nail penetration / crush test per IEC 62133-2 §8.3–8.4 (supplier level); device-level thermal abuse test; housing venting validation |

---

### 2.4 Short Circuit (External)

| Field | Entry |
|-------|-------|
| Hazard | Unintended conductive path across cell or pack terminals |
| Hazardous situation | External short circuit during assembly, maintenance, or connector fault; high discharge current causes rapid cell heating |
| Potential harm | Fire, burn injury, device destruction |
| Initial probability | 3 — Occasional (connector or assembly event) |
| Initial severity | 4 — Critical |
| **Initial risk class** | **12 — Unacceptable; risk reduction required** |
| Control measures | (1) BMS short-circuit protection with hardware current cutoff (primary); (2) Cell-level PCM on selected cell types; (3) Connector design with shrouded terminals (Nair); (4) Assembly procedure with ESD and short-circuit precautions (Okafor) |
| Residual probability | 1 — Remote |
| Residual severity | 4 — Critical |
| **Residual risk class** | **4 — Acceptable** |
| Verification | External short-circuit test per IEC 62133-2 §7.3.2; BMS protection response time measurement |

---

### 2.5 Mechanical Damage

| Field | Entry |
|-------|-------|
| Hazard | Physical deformation or puncture of Li-ion cell |
| Hazardous situation | Drop, crush, or sharp-object penetration causes internal short circuit or electrolyte leakage |
| Potential harm | Fire, toxic electrolyte exposure (skin/eye irritation), burn injury |
| Initial probability | 3 — Occasional (wearable device, patient handling in clinical setting) |
| Initial severity | 4 — Critical |
| **Initial risk class** | **12 — Unacceptable; risk reduction required** |
| Control measures | (1) Rigid housing with defined impact resistance (IEC 60601-1 clause 15.3 mechanical strength requirements); (2) Cell pack mounted with mechanical isolation (foam/bracket per Okafor housing design); (3) IFU instruction prohibiting device use after visible damage; (4) Drop test at device level per IEC 60601-1-11 (if intended for home use) |
| Residual probability | 2 — Unlikely |
| Residual severity | 4 — Critical |
| **Residual risk class** | **8 — ALARP; benefit-risk justification required** |
| Verification | Drop test per applicable IEC 60601-1 clause; housing crush resistance test; cell pack assembly inspection procedure |

---

## 3. Supplier Qualification Procedure — Large Power (Primary)

### 3.1 Purpose

This procedure defines the documents required from Large Power Co., Ltd. to achieve Approved Supplier status for Li-ion cells used in the VU-AMS device, in accordance with the Slow Horses Supplier Qualification Process and the requirements of ISO 13485:2016 clause 7.4.

### 3.2 Documents to request from Large Power

The following documents must be formally requested by Wolff and retained in the DHF under "Battery Cell Supplier Qualification Record — Large Power":

| # | Document | Required content | Acceptability criteria |
|---|----------|-----------------|----------------------|
| 1 | **ISO 13485 certificate** | Certificate number, scope statement, certification body, expiry date, production site address | Scope must cover manufacture of lithium-ion cells or battery packs for medical devices; not expired; issued by accredited CB |
| 2 | **IEC 62133-2 test report** | Cell model/part number, tested lots, all test clauses (§7–8), pass/fail status, accredited lab identity | All mandatory tests passed; part number matches ordered specification; report dated within 3 years or confirmed as still-valid by supplier |
| 3 | **UN38.3 Summary Report** | Cell model, test series T.1–T.8, test results, identity of test laboratory | All tests passed; report signed by responsible person at Large Power |
| 4 | **MSDS / SDS** | GHS-compliant, dated, covers Li-ion cell composition, first-aid measures, fire-fighting, disposal | Must be in English; must cover the exact cell chemistry supplied |
| 5 | **Cell specification datasheet** | Formal document with revision number; minimum, nominal, and maximum values for: capacity, voltage (charge cutoff, nominal, discharge cutoff), max charge current, max continuous discharge current, max temperature, dimensions, weight | All parameters within VU-AMS design requirements; no uncontrolled photocopies accepted |
| 6 | **EU Battery Passport compliance statement** | Written confirmation of Large Power's roadmap or current status for EU Battery Regulation 2023/1542 Battery Passport (see Section 4) | Supplier acknowledges the obligation and provides a timeline for Digital Product Passport data availability |
| 7 | **Quality questionnaire (Slow Horses form SH-SQ-001)** | Manufacturing site location, QMS scope, production lot traceability system, change notification procedure | ISO 13485-certified site confirmed; lot-level traceability confirmed; change notification clause agreed in writing |
| 8 | **Certificate of Conformance (CoC) template** | Format of the CoC that will accompany each delivery batch | Must reference cell part number, lot/batch number, IEC 62133-2 and UN38.3 compliance, and ISO 13485 certificate number |

### 3.3 Procedure steps

1. **Issue Supplier Request Package** — Wolff sends formal document request to Large Power contact, referencing this plan and form SH-SQ-001. Target: within 5 business days of Lamb confirmation of supplier selection.
2. **Document review** — Wolff reviews all received documents against the criteria in table 3.2. Any gaps or non-conformances are communicated to Large Power with a formal response deadline of 10 business days.
3. **ISO 13485 scope verification** — Wolff confirms that the certificate scope explicitly covers the cell manufacturing process, not only downstream packaging or distribution.
4. **Qualification decision** — Upon complete and acceptable documentation package, Wolff issues a Supplier Qualification Record and adds Large Power to the Approved Supplier List (ASL). This unlocks the engineering sample order by Okafor.
5. **Engineering sample inspection** — First delivery (5–20 cells) is subject to incoming inspection per SH-IQC-001 (to be created by Wolff): dimensional check, visual inspection, and CoC verification.
6. **Annual re-qualification** — ISO 13485 certificate and IEC 62133-2 test report validity are reviewed annually; updated CoC required with each production batch.

---

## 4. EU Battery Regulation 2023/1542 — Battery Passport Obligation

### 4.1 Regulatory background

Regulation (EU) 2023/1542 (the "EU Battery Regulation") entered into force on 17 August 2023 and repeals Directive 2006/66/EC. It introduces a Digital Product Passport (DPP) requirement — referred to as the "Battery Passport" — for industrial batteries and electric vehicle batteries, with a phased extension to other battery categories.

For the VU-AMS device, the relevant category is **portable batteries** as defined in Article 3(14): batteries containing one or more cells specifically designed for use in portable equipment. The VU-AMS 14500 Li-ion cells fall within this category.

### 4.2 Battery Passport applicability and deadlines

| Requirement | Applicable regulation article | Deadline | Status for VU-AMS |
|-------------|-------------------------------|----------|-------------------|
| General labelling and information (QR code, chemistry, capacity, hazardous substances) | Art. 13 | Phased; portable batteries: **18 August 2026** for new product introductions | Must be assessed; VU-AMS launch timing must be confirmed against this date |
| Carbon footprint declaration | Art. 7 | Industrial batteries ≥2 kWh: applicable from 2025; portable batteries: currently not mandated for this threshold | Not yet applicable to VU-AMS cells (below threshold) — monitor for updates |
| Battery Passport (Digital Product Passport) | Art. 77 | Portable batteries with capacity >2 Wh used in products placed on the EU market: delegated acts to define exact date; Commission implementing acts expected 2025–2026 | Mandatory for portable batteries; exact date pending delegated act — **treat 2027 as working deadline, with monitoring** |
| Supply chain due diligence (cobalt, lithium, nickel, natural graphite sourcing) | Art. 48–56 | Operators placing >50 tonnes/year on EU market: from 2025; small operators: from 2026 | Slow Horses volumes are below 50-tonne threshold; monitor for SME exemptions |
| Removal and replaceability (portable batteries) | Art. 11 | 28 June 2027 | VU-AMS battery design must allow end-user removal/replacement — confirm with Nair/Okafor |

### 4.3 Immediate compliance obligations for VU-AMS

1. **Battery labelling**: From 18 August 2026, each cell or battery pack placed on the EU market must bear a QR code linking to the information specified in Annex VI of the Regulation. Wolff to confirm whether this is a supplier obligation (Large Power providing EU-compliant labelling) or a Slow Horses obligation at device level.

2. **Battery Passport data collection**: Wolff to initiate data collection from Large Power for the Battery Passport fields defined in Annex XIII. This includes: cell chemistry, capacity, voltage, cycle life, hazardous substances, carbon footprint (when mandated), and supply chain origin of critical raw materials (cobalt, lithium).

3. **Internal registry**: Establish a Battery Passport Data File in the DHF, to be populated with data from Large Power and updated with each new cell lot.

4. **Legal entity registration**: Slow Horses must register as an economic operator in the EU Battery Regulation framework if not already done. Wolff to advise on applicability given current market scope.

---

## 5. Concrete Next Steps — Responsibilities and Timeline

| # | Action | Owner | Depends on | Deadline |
|---|--------|-------|-----------|----------|
| 1 | Confirm supplier selection (Large Power primary, BENZO backup) | Lamb | BATT-001 review | **2026-05-14** |
| 2 | Create Supplier Questionnaire form SH-SQ-001 | Wolff | — | **2026-05-16** |
| 3 | Send formal document request package to Large Power | Wolff | Action 1, 2 | **2026-05-19** |
| 4 | Send document request package to BENZO Energy (backup qualification in parallel) | Wolff | Action 1, 2 | **2026-05-19** |
| 5 | Add ISO 14971 battery risk entries (Section 2 of this plan) to the VU-AMS risk management file | Wolff | — | **2026-05-21** |
| 6 | Receive and review Large Power documentation package | Wolff | Action 3 | **2026-06-06** (allow 14 business days for supplier response) |
| 7 | Issue Supplier Qualification Record — Large Power; update ASL | Wolff | Action 6 | **2026-06-10** |
| 8 | Confirm VU-AMS charging circuit parameters against Large Power cell datasheet | Nair | Action 6 | **2026-06-10** |
| 9 | Authorise engineering sample order (5–20 cells, Large Power) | Wolff (fiat), Okafor (order) | Action 7 | **2026-06-11** |
| 10 | Receive engineering samples; incoming inspection per SH-IQC-001 | Okafor + Wolff | Action 9 | **2026-06-30** |
| 11 | Cell characterisation (capacity, internal resistance, thermal, 0–40 °C) | Vasquez + Nair | Action 10 | **2026-07-18** |
| 12 | Initiate Battery Passport data collection from Large Power | Wolff | Action 3 | **2026-06-06** (include in document request) |
| 13 | Assess IEC 60601-1 clause 15.4.3.4 compliance gap analysis against received BMS design | Wolff + Nair | Action 8 | **2026-06-17** |
| 14 | Update DHF with complete battery certification package (IEC 62133-2, UN38.3, ISO 13485, risk entries, BMS design review) | Wolff | All above | **2026-07-25** |
| 15 | Review EU Battery Regulation Art. 11 (removability) compliance with Nair/Okafor | Wolff | — | **2026-06-01** |
| 16 | Monitor EU Commission delegated act on Battery Passport for portable batteries; update plan if deadline advances | Wolff | — | Quarterly (next review: **2026-08-01**) |

---

## 6. Open Items and Blockers

| # | Item | Owner | Status |
|---|------|-------|--------|
| OI-1 | BENZO Energy ISO 13485 status unconfirmed — must be clarified before BENZO can be used for any certified batch | Wolff | Open — pending document request |
| OI-2 | BMS/PCM design for 2-cell configuration (series vs. parallel) not yet finalised — risk entries 2.1–2.5 will require update once architecture is locked | Nair | Open — pending Vasquez/Nair design decision |
| OI-3 | Incoming inspection procedure SH-IQC-001 does not yet exist — must be created before first delivery | Wolff | Open |
| OI-4 | EU Battery Regulation Art. 11 removability compliance: VU-AMS housing design must be assessed | Nair + Okafor | Open |
| OI-5 | Battery Passport delegated act for portable batteries not yet published — regulatory position may change; plan to be updated on first publication | Wolff | Monitoring |

---

*Prepared by: Dr. Ingrid Wolff, Regulatory Affairs, Slow Horses*  
*For questions: via Lamb*  
*Next revision due: upon receipt of Large Power documentation or design architecture decision — whichever is earlier*
