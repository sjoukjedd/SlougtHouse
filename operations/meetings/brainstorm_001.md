# VU-AMS Team Meeting — New Products Brainstorm
## Meeting 001 | 2026-05-09 | Slough House

**Agenda:** Open brainstorm on new product opportunities leveraging existing technology and expertise.

**Present:** Vasquez, Müller, Chen, Nair, Reyes, Bell, Voss, Hart, Quinn, Kim, Wolff  
**Chair:** Lam  
**Ground rules:** No idea too wild in phase 1. Phase 2: feasibility filter.

---

## Phase 1 — Idea Generation

*Lam: Round table. One or two ideas each. No objections, no death stares. Save it for Phase 2.*

---

### Vasquez (Biomedical Signal Processing)

**1. Baroreflex Sensitivity Monitor (BRS-AMS)**  
We already compute R-to-R intervals and stroke volume on a beat-by-beat basis. Baroreflex sensitivity — the relationship between changes in blood pressure and compensatory changes in heart rate — is one of the most powerful autonomic markers we don't currently surface. We have PEP and SV as proxies for systolic pressure; we have RRI. The Sequence Method for BRS is trivially computed from what we already measure. No new hardware needed — firmware and algorithm extension only. Scientific value is high: BRS is a validated marker of cardiovascular risk, autonomic neuropathy, and acute stress.

**2. Cognitive Load and Mental Fatigue Wearable (NeuroLoad)**  
PEP is a sensitive index of beta-adrenergic sympathetic drive — it shortens reliably under mental effort, even when HR barely changes. Combined with LF/HF ratio and SCL dynamics, we could build a device explicitly validated for cognitive load tracking in operational settings: air traffic control, intensive care, long-haul driving. We have all the signals. What we lack is the validation dataset. That's a research programme, not a hardware problem.

---

### Müller (Firmware)

**1. Ultra-Low-Power Ambulatory Mode**  
The ESP32-S3 is overkill for 24-hour ambulatory recording if we duty-cycle intelligently. An LP-mode firmware variant that stores only I-blocks and compressed RRI (no raw ECG) could extend battery life from 8 hours to 72+ hours on the same cell. Useful for sleep studies, long-duration field research, clinical Holter-replacement applications. New market without new hardware — firmware only.

**2. Multi-Node Body Area Network**  
ESP32 supports ESP-NOW peer-to-peer mesh. A second smaller satellite node — wrist or finger — carrying PPG + skin temperature could stream to the trunk unit and be timestamped-merged into the same recording file. We already have the BLE stack and block format. Adds peripheral vascular tone data (PTT from ECG-to-PPG pulse transit time) at low incremental cost.

---

### Chen (iOS / Apple Platforms)

**1. Apple Watch Standalone Stress Companion App**  
Apple Watch Series 9+ has optical HR, accelerometer, and the new wrist-temperature sensor. It cannot match VU-AMS fidelity, but a Watch app that uses our HRV algorithms (the validated ones, not Apple's black-box ones) as a companion display — synced to the main device — would extend our reach into continuous everyday monitoring. The Watch becomes a qualitative alert layer; the VU-AMS remains the ground truth device.

**2. Live Researcher Dashboard (iPad + BLE)**  
Currently the iOS app is participant-facing: you put it in front of the person being measured. There's a gap for the researcher standing across the room who needs a live metric view — RMSSD trending, PEP trace, SV per beat, motion flags — without touching the participant's device. A separate researcher-mode iPad UI on the same BLE connection (or over local WiFi from the recording device) would be genuinely useful in lab settings.

---

### Nair (Electronics)

**1. Dry-Electrode ECG + ICG Module**  
Gel electrodes are the single biggest friction point for repeated-use clinical and occupational studies. Textile dry electrodes have improved enough that ECG quality is now acceptable for HRV. ICG dry electrodes remain harder — higher impedance, worse SNR — but our precision analog front-end has headroom. A dry-electrode system with active electrode ICs (e.g. TI ADS1299 front-end per electrode) and impedance compensation could make the device genuinely self-applicable. Significant hardware redesign but no new sensing modalities.

**2. Miniaturised Chest Patch (VU-AMS Patch)**  
Current form factor is a belt-clip unit with a cable harness. A rigid-flex redesign that puts everything into a single adhesive chest patch — thin, lightweight, no cables — would open neonatal, paediatric, and post-operative monitoring markets where cable management is a serious safety and comfort issue. We have rigid-flex expertise. This is primarily a packaging and power problem, not a new-signals problem.

---

### Reyes (Java Desktop)

**1. Cloud-Sync Analysis Pipeline**  
The Java desktop app currently requires manual file transfer (SD card out, files in, analyse). A cloud pipeline — device records to SD, iOS app uploads on WiFi, server-side VU-DAMS runs automatically — would transform the product from a lab instrument into a service. Researchers get results in their browser before they've finished the debrief. Infrastructure work, not science work.

**2. Multi-Participant Cohort Analysis Module**  
VU-DAMS processes one file at a time. A cohort mode — batch process a study folder, align all participants on a common event timeline, output group-level HRV and haemodynamic statistics — is something every research user wants and currently does manually in R or SPSS. We have all the parsed data; it's a UI and orchestration problem.

---

### Bell (QA)

**1. Automated Signal Validation Suite**  
We don't have a hardware-in-the-loop test rig. A benchtop signal generator (ECG and ICG waveforms from a validated reference file) fed into the analog front-end, with automated comparison of VU-DAMS output against known ground-truth metrics, would dramatically reduce regression risk on firmware and algorithm updates. Not a product to sell — an internal capability that underpins everything else.

**2. Field Reliability Logging and Telemetry**  
Devices in the field fail silently. A lightweight telemetry block in the firmware (battery voltage, electrode impedance log, error counters) surfaced in VU-DAMS post-hoc would let us see failure patterns across studies before they become support tickets. Pairs with Reyes's cloud pipeline idea.

---

### Voss (Industrial Design)

**1. Modular Electrode System**  
Current electrodes are custom but monolithic. A modular system — standard connector, interchangeable electrode pucks for different body sites, skin types, and use cases — would let users configure the device for chest, abdomen, or limb placement without cable modifications. Opens customisation as a revenue stream and simplifies regulatory submissions for new configurations.

**2. Paediatric and Neonatal Form Factor**  
Scaling the current device to smaller body surfaces is not a simple shrink. Electrode spacing, ICG current path geometry, and adhesive systems all need rethinking for paediatric use. It's a distinct industrial design programme, but the underlying signal chain is the same. High clinical value, high complexity.

---

### Hart (Art / UX / Market)

**1. Occupational Stress Monitoring Service (B2B SaaS)**  
The technology we have is research-grade but the go-to-market isn't. There's a clear B2B opportunity: sell to occupational health consultancies, insurance actuaries, and large employers (transport, healthcare, defence) as a managed stress monitoring service — device loan, protocol design, analysis delivered as a report. Our competitive moat is signal quality no smartwatch can match. Margins are in the service, not the hardware unit.

**2. Athletic Performance and Recovery Monitor**  
HRV monitoring for athletic recovery is a crowded market, but almost entirely PPG-based (unreliable) or single-lead ECG (HRV only, no SV or PEP). A sports-positioned device with our full ICG signal stack — cardiac output during exercise, PEP as sympathetic load, recovery curves post-effort — would be genuinely differentiated in the high-performance sport and military fitness market. It needs a rebranding and a different form factor, but the science is already there.

---

### Quinn (Documentation / User Research)

**1. Guided Protocol Builder**  
Every research group using the VU-AMS reinvents the study protocol. There's no structured tool for specifying task sequences, epoch markers, and analysis windows before a recording starts. A protocol builder — designed with and for researchers — that embeds the event-marker logic into the recording and automatically segments the VU-DAMS output by condition would reduce error and dramatically cut analysis time. Low engineering cost, high usability value.

**2. Participant-Facing Feedback Mode**  
In biofeedback and intervention studies, participants benefit from seeing their own stress signal. An evidence-based, non-alarming real-time feedback display — showing a simple validated stress index, not raw physiology — would open biofeedback therapy and mindfulness research markets. Chen's iOS work is most of what's needed.

---

### Kim (DevOps / Scalability)

**1. Device Management and Firmware OTA Platform**  
As device numbers grow, manual firmware updates via USB are not sustainable. A cloud-based OTA platform — device checks in over WiFi via the iOS app, receives signed firmware bundles, updates itself — with fleet management, version tracking, and rollback capability is infrastructure we'll need regardless of which product direction we take.

**2. Data Lake for Multi-Study Research Consortia**  
A federated data architecture where multiple research groups contribute anonymised VU-AMS recordings to a shared repository — standardised schema, access control, an API for cross-study queries — would make Slow Horses the infrastructure layer for autonomic research, not just a device vendor. Long-term strategic value; short-term it's a significant build.

---

### Wolff (Regulatory)

**1. CE-Marked Clinical Decision Support Tool**  
Our current IEC 60601-1 Type BF compliance covers the device as a measuring instrument. If we move toward clinical decision support — flagging autonomic dysfunction, alerting on arrhythmia, informing treatment decisions — we step into Class IIa medical device territory under MDR 2017/745 in Europe (and De Novo or 510(k) in the US). It's not impossible, but it roughly doubles the documentation burden and requires clinical evidence. Worth planning for if Vasquez's BRS or cognitive load ideas go to market.

**2. Research-Use-Only (RUO) Fast Path**  
Conversely, several of the ideas in this room could be shipped faster under a Research Use Only designation — no diagnostic claims, sold exclusively to academic institutions and research labs. RUO does not exempt us from safety standards (IEC 60601-1 still applies) but removes the clinical evidence requirement. It's a legitimate market position for a company of our size. We should be intentional about which products are RUO and which are clinical, because mixing the two in marketing materials is a regulatory risk.

---

## Phase 2 — Feasibility Filter

*Lam: Right. Enough dreaming. Three filters. Müller, Nair — keep quiet during the regulatory discussion. Wolff, you go last on each.*

---

### Filter 1: Technical Feasibility with Current Expertise

| Idea | Assessment | Effort |
|---|---|---|
| BRS Monitor | Direct extension of existing algorithm pipeline. No new hardware. | S |
| NeuroLoad (Cognitive load) | Signals exist; validation dataset is the real work. Firmware/software only. | M |
| Ultra-Low-Power Ambulatory Mode | Firmware-only. Müller has the stack. Power profiling needed. | S |
| Multi-Node BAN (ESP-NOW + PPG) | Novel but within Müller and Nair's capability. New hardware node required. | M |
| Apple Watch Companion App | Chen has the stack. Watch OS limitations on BLE background modes are a real constraint. | M |
| Live Researcher Dashboard | Straightforward iOS/iPadOS work. Chen could prototype in weeks. | S |
| Dry-Electrode Module | Feasible; significant analog front-end redesign. Nair project. | L |
| Chest Patch (VU-AMS Patch) | Packaging challenge. Rigid-flex expertise exists. Battery constraints severe at this size. | L |
| Cloud Sync Pipeline | Kim + Reyes. Standard infrastructure. No novel engineering. | M |
| Cohort Analysis Module | Reyes. Algorithm work already done; UI and orchestration only. | S |
| Automated Signal Validation Rig | Bell + Nair. High value internally. Benchtop signal generator is commercial off-the-shelf. | S |
| OTA Firmware Platform | Kim. Standard DevOps. Required regardless of product direction. | M |
| Occupational SaaS | Primarily a business model and operations build. Technology exists. | M |
| Athletic Performance Monitor | Firmware and algorithms exist. Form factor redesign by Voss. New BLE profiles. | M |
| Guided Protocol Builder | Quinn + Chen + Reyes. Mostly UX and integration work. | M |
| Participant Feedback Mode | Chen front-end + Vasquez algorithm sign-off on stress index. | S |
| Paediatric Form Factor | L — distinct design programme. Not near-term. | L |
| Data Lake for Consortia | L — significant infrastructure; strategic, not near-term. | L |
| Clinical Decision Support Tool | Technology feasible; regulatory pathway is the constraint. | L (reg) |

---

### Filter 2: Regulatory Pathway

*Wolff:*

Most ideas in this room fall cleanly into one of two buckets. Let me be direct.

**Safe (RUO or non-medical instrument):** BRS Monitor, NeuroLoad, Ultra-Low-Power Mode, Multi-Node BAN, Apple Watch Companion, Researcher Dashboard, Cloud Pipeline, Cohort Analysis, Validation Rig, OTA Platform, Cohort Data Lake, Protocol Builder, Participant Feedback Mode. These are research tools. They measure and record physiological signals; they do not diagnose, treat, or make clinical recommendations. Existing IEC 60601-1 Type BF safety certification covers the hardware. Keep the marketing language research-facing and we stay out of MDR Class IIa territory.

**Elevated risk (requires clinical evidence and MDR/FDA pathway):** Any product making autonomous clinical alerts, informing drug titration, or being marketed for clinical screening — including an arrhythmia-detection feature in the Occupational SaaS, or the BRS monitor if marketed as a cardiovascular risk screening tool. The Clinical Decision Support Tool Wolff proposed is a long-term play: Class IIa in Europe, likely 510(k) in the US, 2–3 years of clinical study minimum.

**Middle risk:** Paediatric and Neonatal Patch. Not inherently Class IIa, but paediatric use requires specific safety evidence for current densities on immature skin, and separate clinical investigations. Manageable but distinct.

**Recommendation:** Any product we ship in the next 18 months should be designed to RUO or non-medical instrument standards. Build clinical evidence in parallel. Do not mix claims.

---

### Filter 3: Scientific and Market Value

*Vasquez (scientific merit):*

Three ideas stand above the others on scientific validity.

First, **BRS Monitor**: baroreflex sensitivity is an established, peer-reviewed marker with decades of validation. The Sequence Method is well-characterised. We can produce numbers that the scientific community trusts immediately. Low implementation risk, high publication value for research users.

Second, **NeuroLoad (PEP-based cognitive load)**: PEP as a sympathetic index is solid — Obrist, Kelsey, and the Leiden group have done the work. The risk is ecological validity: PEP is sensitive but not specific to cognitive load. We need a validation protocol. Worth pursuing as a research platform, not a clinical claim.

Third, **Multi-Node BAN with PTT**: Pulse Transit Time from ECG-to-PPG is a non-invasive blood pressure surrogate with a growing evidence base. If we can get it right, PTT plus our ICG-derived SV/CO gives us a more complete haemodynamic picture than anything else on the wearable market. Technically harder, scientifically exciting.

*Hart (market potential):*

The market question is where the money actually is.

The **Occupational SaaS** model is the largest near-term commercial opportunity. The B2B occupational health market is underserved by research-grade devices — everyone is using smartwatches and getting garbage data. We can offer a managed service at a premium price point. The technology exists; we need a sales motion and a case study.

The **Athletic Performance Monitor** is a real market but crowded at the consumer end. The viable entry point is elite sport and military — customers who will pay for accuracy. The HRV-only smartwatch players cannot touch our ICG-derived cardiac output and PEP data. Rebranding and a purpose-designed form factor from Voss is the main work.

The **Live Researcher Dashboard** and **Cohort Analysis Module** are low-cost, high-satisfaction upgrades for our existing research customers. Not a new market, but they increase retention and reduce churn. Do these.

---

## Phase 3 — Top 3 Concept Cards

*Lam: Voting was quick. Here's where we're pointing the gun.*

---

### Concept Card 1: BRAVO

**Concept Name:** BRAVO — Baroreflex and Autonomic Variability Observer

**Elevator Pitch:**  
An algorithm extension to the existing VU-AMS platform that computes baroreflex sensitivity (BRS) using the Sequence Method, combining validated RRI series and ICG-derived stroke volume data already captured by the device. No new hardware. Delivered as a VU-DAMS module and a live display in the iOS app. Positions the VU-AMS as the only wearable platform with simultaneous BRS, HRV, and haemodynamic output.

**Key Signals / Sensors Needed:**  
- ECG (RRI series) — existing  
- ICG I-block (SV, PEP, LVET per beat) — existing  
- No new sensors required

**Target User:**  
Cardiovascular physiology researchers, autonomic neuroscience labs, clinical researchers in hypertension and heart failure (RUO positioning).

**Development Effort:** S (Small)  
Algorithm: Vasquez leads (Sequence Method BRS, ~4 weeks).  
VU-DAMS module: Reyes (2–3 weeks).  
iOS display: Chen (1–2 weeks for a BRAVO metrics panel).  
Total: ~2 months to functional prototype.

**Regulatory Classification:**  
Research Use Only. No diagnostic claims. Existing IEC 60601-1 Type BF hardware compliance unchanged. Wolff: no additional regulatory pathway required under RUO positioning.

**Next Action:**  
Vasquez to produce BRS algorithm specification (Sequence Method + cross-validation against published reference datasets) within 3 weeks. Reyes to stub the module interface. Chen to add BRAVO panel placeholder in the live display.

---

### Concept Card 2: FIELDWATCH

**Concept Name:** FIELDWATCH — Occupational Stress Monitoring Service

**Elevator Pitch:**  
A B2B managed service built on the VU-AMS hardware stack: employer or occupational health consultancy contracts for a study package — devices shipped, recording protocol pre-configured via Quinn's Protocol Builder, data uploaded via Kim's Cloud Sync Pipeline, analysis delivered as a branded PDF and dashboard report within 24 hours of data receipt. The VU-AMS becomes a service, not just a device. Moat: our ICG-validated stress metrics are measurably more accurate than any competing wearable, and we can prove it.

**Key Signals / Sensors Needed:**  
- Existing VU-AMS signal chain (ECG, ICG, EDA)  
- Protocol Builder (Quinn — new)  
- Cloud Sync Pipeline (Kim + Reyes — new)  
- Report template (Hart + Quinn — new)

**Target User:**  
Occupational health consultancies, large employers in high-stress sectors (emergency services, healthcare, transport, defence), corporate HR departments with occupational risk mandates.

**Development Effort:** M (Medium)  
Cloud pipeline (Kim + Reyes): 2–3 months.  
Protocol Builder UI (Quinn + Chen): 2 months.  
Report template and branding (Hart + Quinn): 4 weeks.  
B2B sales motion (Hart): parallel track.  
Total: ~4 months to first paid pilot.

**Regulatory Classification:**  
Research Use Only, positioned as an occupational physiology assessment tool. No clinical diagnostic claims. Wolff: acceptable under current IEC 60601-1 certification. Any report language suggesting clinical diagnosis requires legal review before shipping.

**Next Action:**  
Hart to identify 2–3 target occupational health consultancies for a pilot conversation within 4 weeks. Kim to produce cloud pipeline architecture proposal. Quinn to begin user research interviews with 3 existing research customers on protocol workflow pain points.

---

### Concept Card 3: VU-AMS PATCH

**Concept Name:** VU-AMS PATCH — Adhesive Chest Patch Form Factor

**Elevator Pitch:**  
A rigid-flex redesign of the VU-AMS electronics and form factor into a single adhesive chest patch: no cables, no harness, self-applicable in under 60 seconds. Same ECG, ICG, EDA, and IMU signal chain as the current device, in a package that opens neonatal, paediatric, post-operative, and continuous ambulatory markets. Paired with Müller's ultra-low-power ambulatory firmware, it enables 72-hour continuous recording without participant management.

**Key Signals / Sensors Needed:**  
- ECG + ICG (dry or hydrogel adhesive electrodes integrated into patch — Nair to evaluate)  
- EDA (optional on patch variant — placement site limits SCL accuracy)  
- IMU (on-board, same ICM-20948 or equivalent)  
- BLE (ESP32 or successor low-power SoC)

**Target User:**  
Clinical researchers (post-operative monitoring, sleep studies, ICU), paediatric physiology labs, ambulatory autonomic studies requiring 24–72 hour recordings.

**Development Effort:** L (Large)  
Hardware redesign (Nair): new rigid-flex PCB, miniaturised analog front-end, battery management — 6–9 months.  
Industrial design (Voss): patch geometry, adhesive system, electrode integration — 4–6 months (parallel).  
Firmware (Müller): LP ambulatory mode, BLE profile update — 2–3 months.  
Regulatory (Wolff): paediatric use requires additional safety evidence for ICG current densities — factor 6 additional months for clinical safety data if targeting neonatal use.  
Total (adult ambulatory, RUO): ~9–12 months. Paediatric: add 6 months.

**Regulatory Classification:**  
Adult ambulatory use: IEC 60601-1 Type BF, RUO. Existing framework applies; new device submission required for the new form factor (notified body review).  
Paediatric / neonatal: elevated regulatory scrutiny. Separate safety and clinical evidence programme required. Wolff recommends scoping adult ambulatory first, neonatal as a phase 2.

**Next Action:**  
Nair to produce a feasibility memo on dry-electrode ICG in patch geometry within 6 weeks — this is the critical technical unknown. Voss to produce concept sketches for patch geometry and electrode layout. Müller to begin LP ambulatory firmware prototyping against existing hardware. All three to report back at Meeting 002.

---

## Summary of Decisions

| Decision | Owner | Deadline |
|---|---|---|
| BRAVO: BRS algorithm spec | Vasquez | Meeting 002 |
| BRAVO: VU-DAMS module stub | Reyes | Meeting 002 |
| BRAVO: iOS panel placeholder | Chen | Meeting 002 |
| FIELDWATCH: B2B pilot targets identified | Hart | 4 weeks |
| FIELDWATCH: Cloud pipeline architecture | Kim | Meeting 002 |
| FIELDWATCH: User research interviews (protocol pain points) | Quinn | Meeting 002 |
| PATCH: Dry-electrode ICG feasibility memo | Nair | 6 weeks |
| PATCH: Patch geometry concept sketches | Voss | Meeting 002 |
| PATCH: LP ambulatory firmware prototype start | Müller | Meeting 002 |

**Meeting 002:** Scheduled in 2 weeks. Agenda: BRAVO algorithm spec review, FIELDWATCH pipeline architecture, PATCH feasibility memo.

---

*"Three targets. Don't miss."*  
*— Lam*
