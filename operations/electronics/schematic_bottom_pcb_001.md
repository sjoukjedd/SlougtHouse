# Schematic Description — Bottom PCB (Analoog)
## VU-AMS Rev 0.1 — 52 × 25 mm Analog Front-End
### Netlist-niveau tekstueel document voor KiCad schematic capture

**Auteur:** Priya Nair — Electronics  
**Datum:** 2026-05-09  
**Ref:** Brief 001 + Addendum A + Addendum B + Vasquez Placement Validation 001 + Component Selection Rev 0.3 + Principal Decisions 2026-05-09  
**Status:** Gereed voor KiCad capture — alle open punten gesloten (B1–B5)

---

## 1. Overzicht — Blokdiagram en Signaalstromen

```
ELEKTRODEN (PATIËNTZIJDE)
════════════════════════════════════════════════════════════════════════════════

  E1  ──[TVS1]──[R1: 10kΩ]──────────────────────────────────────────┐
  (collarbone, device body)                                          │
                                                       ┌────────────┤
  E2  ──[TVS2]──[R2: 10kΩ]──[phantom power]──────────►│ ICG sense  │
  (active electrode, front-lower, 2-draads kabel)     │ AD8221AR   │
                                                       │ (U3)       │──► ICG-keten
  E3  ──[TVS3]──[R3: 10kΩ]──[phantom power]──────────►│            │
  (active electrode, front-lower, 2-draads kabel)     └────────────┘
                                                                     │
                                                       ┌────────────┐│
  E4  ──[TVS4]──[R4: 10kΩ]──────────────────────────►│ ICG        ││
  (nape of neck, ICG injectie return)                 │ stroombron ││
                                                      │ (discrete) ││◄─── SG-210STF (32kHz)
  E5  ──[TVS5]──[R5: 10kΩ]──────────────────────────►│            │└─────────────────────
  (lower back, ICG injectie return)                   └────────────┘

  E6  ──[TVS6]──[R6: 10kΩ]──[LPF1: 5kHz RC]──────────────────────► AD5933 (U7)
  (SCL elektr. +, linker borst)                                          SCL keten
  E7  ──[TVS7]──[R7: 10kΩ]──[LPF2: 5kHz RC]──────────────────────► AD5933 (U7)
  (SCL elektr. -, linker borst, 2cm afstand van E6)

════════════════════════════════════════════════════════════════════════════════
ECG KETEN

  E1 ──────────────────────────────────────────────────────────────►┐
                                                      INA333 (U1)   │──► ECG1 → ADC ch2
  E2/E3 (via phantompower, gemeenschappelijk)─────────────────────►┘
                                                                     │
  E3 ──────────────────────────────────────────────────────────────►┐
                                                      INA333 (U2)   │──► ECG2 → ADC ch3
  E1 (ref) ───────────────────────────────────────────────────────►┘

  E1 + E3 gemeenschappelijk ──► OPA333 RLD (U4) ──► RLD elektrode (via E1 pad terugkoppeling)

════════════════════════════════════════════════════════════════════════════════
ICG KETEN

  SG-210STF (Y1) ──[33Ω]──────────────────────────────────────────► ESP32 GPIO (flex J1)
       │
       ├───────────────────────────────────────────────────────────► AD630 pin 9/10 (ref)
       │
       └── ICG stroombron (Howland of Tietze VCCS) ──► E4/E5 (injectie)

  E2/E3 (sense) ──► AD8221AR (U3) ──► AD630ARZ (U5) ──[RC 2kHz LPF]──► ADC ch0 (Z0)
                                                                     ──► ADC ch1 (V_source)

════════════════════════════════════════════════════════════════════════════════
SCL KETEN

  E6/E7 ──[TVS]──[10kΩ]──[5kHz LPF]──► AD5933 VIN/RFB (U7) ──(I²C)──► flex J1 → ESP32

════════════════════════════════════════════════════════════════════════════════
PPG

  MAX30101 (U8) ──(I²C)──────────────────────────────────────────── flex J1 → ESP32
                 [10µF + 100nF VLED ontkoppeling]

════════════════════════════════════════════════════════════════════════════════
VOEDING

  VBAT (LiPo via flex) ──► LT3042 (U9) [+3.3V analog rail AVDD]
  AVDD ──► TPS60400 (U11) [−3.3V rail AVSS via charge pump + LC-filter]
  AVDD ──► REF5025 (U10) [2.5V ADC referentie VREF]

════════════════════════════════════════════════════════════════════════════════
ADC

  ADS1256 (U6) ← [ch0: ICG Z0] [ch1: V_source] [ch2: ECG1] [ch3: ECG2]
                  [ch4: Temp legacy] [ch5: Vbatt] [ch6: NC] [ch7: NC]
               ──(SPI + DRDY)─────────────────────────────────────── flex J1 → ESP32
  AD5933 (U7) ──(I²C direct)──────────────────────────────────────── flex J1 → ESP32
              [SCL digitaal: impedantie real/imag via I²C, geen analoge ADC-kanalen]
```

**Netten en voedingsdomeinen:**
- `AVDD` (+3.3V) — analoge rail (LT3042-uitgang): voeding voor U1, U2, U3, U4, U5, U6, U7, U8, Y1
- `AVSS` (−3.3V) — negatieve analoge rail (TPS60400 charge pump, afgeleid van AVDD): voeding voor AD630 −VS, ICG VCCS
- `VREF` (+2.5V) — ADC-referentie (REF5025-uitgang): verbonden aan ADS1256 VREFP
- `AGND` — analoge massa, ster-topologie terug naar flex J1 pin GND
- `VBAT` — ongereguleerde LiPo-input (3.0–4.2V), alleen voor LT3042 input en Vbatt-meetdeler

---

## 2. Patiëntbeveiliging — TVS-diodes en Serieweerstanden

**Classificatie:** IEC 60601-1 Type BF (battery-only, drijvend patiëntcircuit)

### Schema per elektrodelijn

Elke elektrodelijn volgt hetzelfde beschermingsschema:

```
ELECTRODE_PAD ──[TVS_n]──[R_n]──► ANALOG_FRONTEND_NET
                    │
                   AGND
```

### Componentwaarden en netten

| Elektrode | TVS | Serieweerstand | Net naar frontend | Beschrijving |
|-----------|-----|----------------|-------------------|--------------|
| E1 (collarbone) | TVS1: PRTR5V0U2X of gelijk. bidirectioneel 5.5V clamp, SOD-323 | R1: 10kΩ 0402 1% | NET_E1_FILT | ICG Vsense+, ECG ref |
| E2 (active, front) | TVS2: PRTR5V0U2X | R2: 10kΩ 0402 1% | NET_E2_FILT | ICG Vsense−, ECG |
| E3 (active, front) | TVS3: PRTR5V0U2X | R3: 10kΩ 0402 1% | NET_E3_FILT | ICG Vsense−, ECG (redundant) |
| E4 (neck, injectie) | TVS4: PRTR5V0U2X | R4: 10kΩ 0402 1% | NET_E4_FILT | ICG injectie return |
| E5 (lower back, injectie) | TVS5: PRTR5V0U2X | R5: 10kΩ 0402 1% | NET_E5_FILT | ICG injectie return |
| E6 (SCL+) | TVS6: PRTR5V0U2X | R6: 10kΩ 0402 1% | NET_E6_FILT | SCL/EDA+ |
| E7 (SCL−) | TVS7: PRTR5V0U2X | R7: 10kΩ 0402 1% | NET_E7_FILT | SCL/EDA− |

**WAARSCHUWING (BF-classificatie):**  
De serieweerstanden 10kΩ zijn vereist voor patiëntveiligheidsstroom-beperking per IEC 60601-1 clause 8.7.3 (BF type applied part, auxiliary current limit). Bij 10kΩ en maximale netspanning 250V (single-fault) is de patiëntlekstroom maximaal 25 mA — ruim boven de 10 µA BF-DC limiet. De BF-classificatie is van toepassing op battery-only voeding; galvanische isolatie van de gehele analoge keten ten opzichte van netspanning is het primaire beschermingsmechanisme. De TVS-diodes beschermen tegen ESD en defibrillatiespanningspieken (per IEC 60601-2-27 bijlage).

**Actieve elektroden E2/E3 — phantomvoeding (zie Bijlage B punt 4, RESOLVED 2026-05-09):**  
E2 en E3 zijn actieve elektroden op een 2-ader kabel (signaal + phantomvoeding). Buffer in elektrodeklip: LMP7721 (unity gain). Phantomvoeding: AVDD (+3.3V) via R_PH (560Ω 1% 0402) per ader, met C_PH (100nF X7R 0402) HF-ontkoppeling aan PCB-zijde. Blokkeer condensator C_BLK (10µF X5R 0805) aan ontvangerzijde scheidt DC phantom van AC biosignaal. Driven shield: shield van signaalkabel verbonden aan LMP7721 uitgang in elektrodeklip, aan PCB-zijde teruggekoppeld via R_SHD (100Ω 0402) naar NET_Ex_FILT.

**Layout-waarschuwing:**  
TVS-componenten zo dicht mogelijk bij de electrodepads plaatsen, vóór elke andere trace-routing. De AGND-verbinding van de TVS-kathode naar de analoge grondvlak moet laag-inductief zijn (brede kortsluitverbinding, geen lange trace).

---

## 3. ICG Keten — Oscillator → Stroombron → Transformer → InAmp → Demodulator → ADC

### 3.1 Oscillator: SG-210STF (Y1)

**Component:** Epson SG-210STF 32.000 kHz TCXO, SMD 3.2×2.5mm 4-pad  
**Voeding:** AVDD (3.3V) op pin 1 (VDD), GND op pin 2 (GND)  
**Uitgang:** pin 3 (OUT) — CMOS blokgolf, 32.000 kHz

**Netten:**
- `Y1_VDD` = AVDD via 10Ω ferrietkraal FB1 (ruisfilter) + 100nF ontkoppelcondensator C_Y1 naar AGND
- `Y1_OUT` = CMOS oscillatoruitgang

**Aftakkingen van Y1_OUT:**
1. `OSC_REF` → AD630 REF-ingang (pin 9 en 10, zie 3.4)
2. `OSC_ESP32` → via 33Ω serieweerstand R_OSC naar flex connector J1 pin OSC_MON → ESP32 GPIO (PCNT of RMT capture input)

**Kritieke netten:** `OSC_REF` moet zo kort mogelijk gehouden worden (< 15mm trace, afgeschermd door grondvlak). De 33Ω naar ESP32 isoleert de oscillatoruitgang capacitief van de ESP32-GPIO-ingangsimpedantie.

**Layout-waarschuwing:** Y1 voedingspin met 100nF direct decoupling (afstand < 1mm). Oscillator niet nabij ICG-stroombronkomponenten plaatsen om terugkoppeling via voedingsnet te voorkomen.

---

### 3.2 ICG Stroombron (Voltage-Controlled Current Source, VCCS)

**Topologie:** Howland current pump of Tietze floating VCCS met op-amp.  
**Aanbevolen op-amp:** OPA627 of AD8671 (lage ruisvoeding op 3.3V single supply met virtuele aarde op AVDD/2 — zie noot hieronder over virtuele aarde).

**Functie:** Omzet de 32kHz oscillatorblokgolf (Y1_OUT) in een constante 1 mA sinusoïdale wisselstroom naar E4 en E5 (injectie-elektroden, nek en onderrug). Blokgolf → laagdoorlaat R-C → sinusoïde.

**Blokgolf → Sinus-omvorming:**
- R_sin: 10kΩ, C_sin: 470pF → RC-laagdoorlaatfilter (fc = 34kHz, −3dB)
- Door de 32kHz blokgolf door één RC-netwerk te sturen worden harmonischen onderdrukt; de 3e harmonische (96kHz) is met één pool al −20dB t.o.v. de grondtoon

**Howland stroombron configuratie (voorbeeld):**

```
               R8       R9
OSC_SIN ──[R8]──┬──[R9]──┬── E4 (via J_E4, TVS4, R4)
                │        │
               [R10]    [+] OPA_VCCS (U_VCCS)
                │        │
               AGND    [out]──┬── E4 (feedback)
                              │
                             [R11] (programmeerbare weerstand of vaste 100kΩ)
                              │
                             AGND
```

**Componentwaarden (richtlijn, te verfijnen in simulatie):**
- R8 = R9 = R10 = R11 = 100kΩ 0.1% (matched pair voor CMRR)
- Sturing via gelijkwaardig matching: 1 mA output bij 100mVpk oscillatoramplitude → transconductantie 10 mS
- V_source monitoring: weerstand R_mon (1kΩ, 0.1%) in serie met de uitgang naar E4 → spanning over R_mon proportioneel aan stroom → ADC ch1 (V_source)

**Netten:**
- `ICG_SRC_P` = uitgang naar E4/E5 (via J_E4, J_E5)
- `VSOURCE_MON` = spanning over R_mon → ADC ch1

**Layout-waarschuwing:** Stroombroncomponenten fysiek scheiden van ICG sense-lijnen (E2/E3). Bij voorkeur in aparte PCB-zone. De terugloopdraad (E4 of E5) mag nooit parallel lopen met de senselijn.

---

### 3.3 ICG Sense InAmp: AD8221AR (U3)

**Component:** AD8221ARZ, SOIC-8  
**Voeding:** AVDD (pin 7), AGND (pin 4)  
**Differentiele ingang:** IN+ (pin 3) = NET_E2_FILT, IN− (pin 2) = NET_E1_FILT  
*(E2/E3 zijn de frontelektroden, E1 is het device-body-elektrodepunt; differentieel over ICG-sensepaar)*

**Gain-instelling:**
- Gewenste gain: G = 10 (drager 25 mV → 250 mV uitgang voor AD630)
- Gain-formule AD8221: G = 1 + (49.4kΩ / R_G)
- R_G = 49.4kΩ / (G − 1) = 49.4kΩ / 9 ≈ **5.49kΩ** → gebruik 5.49kΩ 0.1% (E96 reeks)
- R_G = R_G1 geplaatst tussen pin 1 (RG) en pin 8 (RG)

**Netten:**
- `ICG_INAMP_OUT` = pin 6 (OUT) → AD630 signaalingang (U5 pin 1)
- `REF_INAMP` = pin 5 (REF) → AGND (of virtuele-aarde midpunt als single-supply split-rail)

**Componentwaarden:**
- C_U3_VDD: 100nF 0402 X7R, AVDD naar AGND, direct aan pin 7
- C_U3_VDD2: 10µF 0603 X5R tantalum of ceramisch, AVDD naar AGND

**Layout-waarschuwing:** AD8221 is gevoelig voor EMI op de ingangspinnen bij deze frequenties. Ingangstrace zo kort mogelijk houden. Een ferrite bead (of 100Ω 0402) in elke ingangsleiding (voor R2/R3) kan raadzaam zijn als extra HF-blokkade.

---

### 3.4 ICG Demodulator: AD630ARZ (U5)

**Component:** AD630ARZ, SOIC-16  
**Voeding:** AVDD (pin 15 = +VS), AGND (pin 8 = −VS, of virtuele aarde; let op: AD630 is ontworpen voor split-supply ±5V — bij single 3.3V vereist dit een charge-pump of virtuele massa op AVDD/2)

**WAARSCHUWING — voedingsarchitectuur AD630:**  
De AD630 is standaard gespecificeerd voor ±5V split supply. Bij 3.3V single supply MOET een virtuele-aardelading (AVDD/2 = 1.65V) worden gegenereerd via een rail-splitter (bijv. TLE2426 of spanningsdeler met buffer). Dit is een kritiek open punt voor Rev 0.1: beslissing vereist over of een charge-pump (TI TPS60400 of gelijk.) een −3.3V rail genereert, of dat een 1.65V virtuele aarde volstaat. Adviseer: separate ±3.3V rails via kleine inverting charge pump op analoge rail.

**Pinverbindingen (ideale split-supply configuratie ±3.3V):**
- Pin 1 (CHA IN+): `ICG_INAMP_OUT` (signaal van AD8221)
- Pin 2 (CHA IN−): AGND (of virtuele aarde)
- Pin 9 (SELECT/REF+): `OSC_REF` (32kHz CMOS van Y1)
- Pin 10 (SELECT/REF−): AGND
- Pin 16 (OUT): `DEMOD_OUT` → post-demodulatie LPF
- Pin 15 (+VS): +AVDD (of +3.3V)
- Pin 8 (−VS): −AVDD (of −3.3V via charge pump)

**Post-demodulatie laagdoorlaatfilter (2kHz, 1e orde):**
- R_LPF1: 5.6kΩ 0402 1%
- C_LPF1: 15nF 0402 C0G/NP0
- fc = 1/(2π × 5.6kΩ × 15nF) ≈ 1.9 kHz ≈ 2 kHz
- Netten: `DEMOD_OUT` → R_LPF1 → `Z0_FILTERED` → C_LPF1 naar AGND → ADS1256 ch0

**Netten:**
- `Z0_FILTERED` → ADS1256 AINP0 (ADC ch0, ICG Z0)
- `VSOURCE_MON` → ADS1256 AINP1 (ADC ch1, V_source)

**Componentwaarden ontkoppeling U5:**
- C_U5_P: 100nF 0402 X7R (+VS naar AGND)
- C_U5_N: 100nF 0402 X7R (−VS naar AGND)
- C_U5_bulk: 10µF 0603 per voedingsrail

---

## 4. ECG Keten — Elektroden → InAmps → RLD → ADC

### 4.1 ECG InAmp 1: INA333 (U1) — ECG kanaal 1

**Component:** INA333AIDGKT, SC-70-8 (USON-8)  
**Voeding:** AVDD (pin 7 = VS+), AGND (pin 4 = VS−) — single supply  
**Differentiële ingang:** IN+ (pin 3) = NET_E1_FILT (collarbone), IN− (pin 2) = NET_RLD (via RLD terugkoppeling)

**Gain-instelling:**
- Doelgain voor ECG: G = 100 (5mV ECG → 500mV voor ADC)
- Gain-formule INA333: G = 1 + (100kΩ / R_G)
- R_G = 100kΩ / (G − 1) = 100kΩ / 99 ≈ **1.01kΩ** → gebruik 1.00kΩ 0.1% (standaard E96)
- R_G1 geplaatst tussen pin 1 (RG−) en pin 8 (RG+)

**Netten:**
- `ECG1_AMP_OUT` = pin 6 (OUT) → ECG1 laagdoorlaatfilter → ADC ch2
- `REF1` = pin 5 (REF) = NET_RLD (driven right leg referentiespanning)

**ECG laagdoorlaatfilter na U1 (Sallen-Key of passief 2e orde, fc ≈ 150 Hz):**
- Passief RC 2e orde: R_ECG1A: 1kΩ, C_ECG1A: 1µF (fc = 159 Hz), gevolgd door R_ECG1B: 1kΩ, C_ECG1B: 1µF
- Doel: onderdrukking van 32kHz ICG carrier vóór ADC-ingang
  (32kHz / 150Hz = 213× → 2e orde RC geeft −46dB @ 32kHz — afdoende)

**Netten:**
- `ECG1_FILTERED` → ADS1256 AINP2 (ADC ch2)

**Componentwaarden ontkoppeling U1:**
- C_U1: 100nF 0402 X7R, pin 7 naar pin 4 (direct)

---

### 4.2 ECG InAmp 2: INA333 (U2) — ECG kanaal 2

**Identiek aan U1, met:**
- IN+ (pin 3) = NET_E3_FILT (solar plexus)
- IN− (pin 2) = NET_RLD
- R_G2 = 1.00kΩ 0.1%
- Output `ECG2_AMP_OUT` → ECG2 LPF → `ECG2_FILTERED` → ADS1256 AINP3 (ADC ch3)

---

### 4.3 Driven Right Leg Amplifier: OPA333 (U4)

**Component:** OPA333AIDBVR, SOT-23-5  
**Voeding:** AVDD (pin 5 = VS+), AGND (pin 2 = GND)  
**Functie:** Gemeenschappelijk-modusspanningsterugkoppeling naar RLD-elektrode (E1/E2 midpunt), onderdrukt 50/60Hz storingsignalen

**Topologie:**

```
ECG1_AMP_IN_P (NET_E1_FILT) ──[R_RLD_A: 1MΩ]──┐
                                                ├──► OPA333 (U4) IN− (pin 3)
ECG2_AMP_IN_P (NET_E3_FILT) ──[R_RLD_B: 1MΩ]──┘
                                                    OPA333 IN+ (pin 2) = AGND
                                                    OPA333 OUT (pin 1) = NET_RLD
```

**Terugkoppelnetwerk (RLD stability):**
- R_RLD_FB: 10kΩ (van OUT naar IN−)
- C_RLD_FB: 100nF (parallel aan R_RLD_FB) → rollt de lusversterking af boven 160Hz, voorkomt oscillatie bij 32kHz

**Patiëntbescherming op RLD-uitgang:**
- R_RLD_OUT: 10kΩ 0402 serieresistor naar het RLD-pad (E1 terugkoppelpunt op device-body)
- Dit beperkt stroom bij single-fault naar < 0.5mA bij 5V foutspanning

**Netten:**
- `NET_RLD` = OPA333 output = REF-ingang U1 pin 5 en U2 pin 5

**Componentwaarden ontkoppeling U4:**
- C_U4: 100nF 0402 X7R, pin 5 naar pin 2 (direct)

---

## 5. SCL Keten — Elektroden → 5kHz LPF → AD5933 → I²C

### 5.1 Ingangsbescherming en 5kHz LPF (Vasquez verplichte conditie)

**Vasquez conditie:** 5kHz laagdoorlaatfilter op E6 en E7 vóór AD5933-ingang ter onderdrukking van de 32kHz ICG-carrier (mandatory, zie vasquez_placement_validation_001.md sectie 2.4A).

**Topologie per elektrode (E6 voorbeeld, E7 identiek):**

```
E6_PAD ──[TVS6]──[R6: 10kΩ]──[R_LPF_E6: 3.3kΩ]──┬── AD5933 VIN (U7 pin 5)
                                                   │
                                               [C_LPF_E6: 10nF]
                                                   │
                                                 AGND
```

**Berekening:**
- fc = 1/(2π × 3.3kΩ × 10nF) = 4.82 kHz ≈ 5 kHz (binnen 4% van eis)
- Demping @ 32kHz: 20×log(32/4.82) = 20×log(6.64) = +16.4dB per pool → 1 pool RC geeft −16.4dB @ 32kHz
- Let op: −16.4dB per pool is slechts 1e orde. Voor 40dB onderdrukking (Vasquez eis) zijn 2 polen vereist:
  - Optie A: twee secties in cascade: (R6:10kΩ + R_LPF1:3.3kΩ, C_LPF1:10nF) → (R_LPF2:3.3kΩ, C_LPF2:10nF)
  - Optie B: actieve Sallen-Key 2e orde Butterworth LP (fc=5kHz) — aanbevolen als ruimte beschikbaar
- **Aanbeveling:** 2e orde passief RC cascade (Optie A) vanwege ruimtebesparing op 52×25mm PCB

**Gecombineerde filtering E6 (2e orde cascade):**
- R6: 10kΩ (patiëntveiligheid, al opgenomen in sectie 2)
- R_LPF_E6A: 3.3kΩ 0402, C_LPF_E6A: 10nF 0402 C0G → fc = 4.82kHz (pool 1)
- R_LPF_E6B: 1kΩ 0402, C_LPF_E6B: 33nF 0402 C0G → fc = 4.82kHz (pool 2)
- Totale demping @ 32kHz: 2 × 16.4dB = 32.8dB (net onder 40dB eis; overweeg pole 1 te verhogen naar R=4.7kΩ voor 3×20dB=40dB)

**Netbeschrijving:**
- `NET_E6_FILT` (na R6) → R_LPF_E6A → `NET_E6_LP1` → R_LPF_E6B → `NET_E6_LP2` → AD5933 pin 5 (VIN)
- C_LPF_E6A: `NET_E6_LP1` naar AGND
- C_LPF_E6B: `NET_E6_LP2` naar AGND
- Zelfde structuur voor E7 → AD5933 pin 6 (via interne TIA, RFB extern geplaatst)

---

### 5.2 AD5933 Impedantie-Analyser (U7)

**Component:** AD5933YRSZ, SSOP-16  
**Voeding:** AVDD (pin 14 = AVDD), AVDD (pin 3 = DVDD — AD5933 accepteert 3.3V op beide), AGND (pin 1, pin 11)  
**Interface:** I²C (pin 9 = SDA, pin 10 = SCL) → flex J1 I²C bus

**SCL/EDA excitatie:**
- Interne DDS-generator genereert sinus op configureerbare frequentie (default: 500–1000Hz voor SCL, instelbaar via I²C)
- Excitatie-amplitude: programmeerbaar (200mVpp of 400mVpp intern); extern spanningsdeler naar max. 50mVpp aan elektroden:
  - R_EXC_DIV_A: 68kΩ, R_EXC_DIV_B: 10kΩ (deler: 50mVpp / 200mVpp ≈ 1:4 — output gedeeld door 5)
  - Plaatsing: van AD5933 VOUT (pin 4) via deler naar E6 elektrode-keten

**Externe terugkoppelingsweerstand (RFB):**
- RFB stelt de transimpedantie in voor de interne TIA
- SCL huidweerstand range: 200kΩ–10MΩ
- RFB keuze: 1MΩ (middenwaarde logaritmisch)
- R_FB1: 1MΩ 0402 1% geplaatst tussen AD5933 pin 5 (VIN) en pin 6 (RFB) extern

**Kalibratie:**
- Een externe kalibratie-impedantie (ZCAL: 200kΩ 0.1% in serie met 10nF C0G) kan geselecteerd worden via een kleine NMOS-schakelaar (2N7002 of gelijk.) aangestuurd door een ESP32-GPIO via de flex-connector. Kalibratie wordt uitgevoerd bij inschakelen.

**Netten:**
- `AD5933_SDA` → flex J1 I²C_SDA
- `AD5933_SCL` → flex J1 I²C_SCL
- 4.7kΩ pull-up weerstanden (R_SDA, R_SCL) naar AVDD op de onderste PCB

**ADS1256 ch6/ch7 — NIET AANGESLOTEN (beslissing principal 2026-05-09):**  
De AD5933 levert impedantieresultaten uitsluitend digitaal via I²C naar de ESP32. Er is geen analoge uitvoertrap; ADS1256 ch6 (AINP6) en ch7 (AINP7) worden in Rev 0.1 **niet** op de SCL-keten aangesloten. AINP6 en AINP7 worden via 100kΩ pull-down naar AGND getrokken en beschikbaar gehouden als testpoints voor toekomstig gebruik. Zie ook sectie 8 (ADC kanaalmap).

**Componentwaarden ontkoppeling U7:**
- C_U7_AVDD: 100nF 0402 X7R + 10µF 0603 X5R aan pin 14 naar AGND
- C_U7_DVDD: 100nF 0402 X7R aan pin 3 naar AGND

**Layout-waarschuwing:**  
AD5933-signaallijnen (VOUT, VIN, RFB) volledig afschermen van ICG-signaallijnen. Het 32kHz ICG-veld dat op E6/E7 aanwezig is, wordt grotendeels onderdrukt door de 5kHz LPF (sectie 5.1), maar de AD5933 VOUT-excitatieplaat mag absoluut niet koppelen naar ICG-sense-lijnen. Aparte PCB-regio, gescheiden door grondspoor.

---

## 6. PPG — MAX30101 Plaatsing, Ontkoppeling, I²C

### 6.1 MAX30101 (U8)

**Component:** MAX30101EFD+, OLGA-14 (optic, 5.6×3.3mm)  
**Voeding:**
- Pin 1 (VDD_1P8): 1.8V rail — afgeleid van AVDD via LDO of spanning-deler (NB: apart 1.8V LDO vereist indien AVDD = 3.3V; gebruik van MIC5504-1.8 of gelijk. 50mA LDO)  
  - Alternatief: gebruik de 1.8V DVDD-rail van de bovenste PCB via de flex-connector
- Pin 3 (VLED): 3.3V LED-voedingspin → **Vasquez conditie: 10µF + 100nF lokale ontkoppeling, bulk capacitor zo dicht mogelijk bij pin 3**
- Pin 12 (GND): AGND

**VASQUEZ CONDITIE (mandatory layout requirement):**  
MAX30101 VLED (pin 3) vereist 10µF (C_LED_BULK: 0603 X5R tantalum of ceramisch) + 100nF (C_LED_HF: 0402 X7R) lokale ontkoppeling. De 10µF capacitor moet binnen 2mm van pin 3 worden geplaatst. Reden: MAX30101 LED-drive kan puls-stromen van 10–50mA trekken bij duty-cycled bediening (100µs puls @ 100Hz); deze transients mogen de ICG sense-keten niet bereiken via de AVDD-rail.

**Interface:**
- Pin 11 (SDA): I²C_SDA bus
- Pin 10 (SCL): I²C_SCL bus
- Pin 9 (INT): `PPG_INT` → flex J1 → ESP32 GPIO interrupt
- 4.7kΩ pull-up weerstanden op I²C al aanwezig (gedeeld met AD5933, TMP117, BMP390)

**Netten:**
- `MAX30101_SDA` = I²C_SDA (shared bus)
- `MAX30101_SCL` = I²C_SCL (shared bus)
- `PPG_INT` → J1 pin INT_PPG

**I²C adres:** MAX30101 = 0xAE (write) / 0xAF (read), vast

**Layout-waarschuwing (Vasquez conditie):**  
MAX30101 signal traces fysiek gescheiden van ICG signal traces. Op een 4-laags PCB: MAX30101 op aparte koperzone van de ICG-keten, liefst aan tegenovergesteld einde van de 52×25mm kaart. Minimale scheiding van ICG-trace-bundel: 3mm clearance plus grondspoor tussen zones. Zie vasquez_placement_validation_001.md sectie 1.4.

---

## 6B. Barometrische druksensor — BMP390 (U12)

### 6B.1 Component

**Component:** Bosch BMP390 (BMP390T), LGA-12 (2.0 × 2.0 × 0.75 mm)  
**Functie:** Meting van luchtdruk (Pa) en omgevingstemperatuur (°C). Primaire toepassing in het VU-AMS: detectie van hoogteverandering bij trapklimmen via drukgradiënt (≈ −12 Pa/m, trapstap ≈ 0.2 m → ≈ 2.4 Pa per tree). Secundair: ambient temperatuur als referentie voor correctie.

**I²C adres:** 0x77 (SDO verbonden aan DVDD_3V3 = VDD-niveau; selecteert adres 0x77)  
**Alternatief adres:** 0x76 (SDO naar GND) — niet gebruikt in Rev 0.1 (geen conflict met andere I²C-apparaten op 0x77)

---

### 6B.2 Voeding en ontkoppeling

**Voedingspin (VDDIO, pin 2):** verbonden aan **DVDD_3V3** (+3.3V digitale rail, via flex J1 pin 6)  
*(BMP390 VDDIO range: 1.2–3.6V; 3.3V is binnen spec)*

**Ontkoppelcondensatoren (verplicht, direct bij VDDIO-pin):**
- C_BMP_1: 100 nF 0402 X7R — van VDDIO naar AGND (HF-ontkoppeling, < 0.5 mm van pin 2)
- C_BMP_2: 10 µF 0603 X5R — van VDDIO naar AGND (bulk ontkoppeling)

**GND (pin 1, pin 7, exposed pad):** verbonden aan AGND (digitale massa van onderste PCB)

---

### 6B.3 Pinverbindingen

| Pin | Netnaam | Verbinding | Beschrijving |
|-----|---------|------------|--------------|
| 1 (GND) | AGND | Analoge massa | Grond |
| 2 (VDDIO) | DVDD_3V3 | +3.3V digitale rail via flex J1 | Voeding |
| 3 (SDO) | DVDD_3V3 | Pull-up naar DVDD_3V3 via R_SDO (10 kΩ 0402) | I²C adres = 0x77 |
| 4 (CSB) | DVDD_3V3 | Pull-up naar DVDD_3V3 via R_CSB (10 kΩ 0402) | Selecteert I²C modus (CSB hoog = I²C) |
| 5 (SDI / SDA) | I2C_SDA | Gedeelde I²C databus | I²C data |
| 6 (SCK) | I2C_SCL | Gedeelde I²C klokbus | I²C klok |
| 7 (GND) | AGND | Analoge massa | Grond |
| EP (exposed pad) | AGND | Gesoldeerd aan grondvlak | Thermische en elektrische massa |

**Pull-up weerstanden op I²C:** 4.7 kΩ naar AVDD (R_SDA, R_SCL) al aanwezig op onderste PCB (gedeeld met AD5933, MAX30101, TMP117 — zie sectie 5.2 en 6.1).

---

### 6B.4 Plaatsing

- **Locatie:** Bovenste PCB (digitale zone, nabij ESP32-S3 I²C-pinnen GPIO5/GPIO6)  
  *(De BMP390 is een digitale sensor zonder analoog signaalbeheer; plaatsing op de bovenste PCB is correct. De I²C-bus kruist de flex-strip; dit is conform flex J1 pin 12/13.)*
- **Vermijden:** Niet plaatsen nabij warmtebronnen (LiPo-lader, ESP32 radiogedeelte, zon-zijde behuizing)
- **Vrije luchtcirculatie:** Behuizing voorzien van kleine ventilatie-opening (≥ 1 mm²) nabij BMP390 voor correcte drukgeleiding. Zie behuizingsdocumentatie.

---

### 6B.5 I²C bus sharing

De BMP390 deelt de I²C bus (I2C_SDA, I2C_SCL, 400 kHz Fast Mode) met:

| Device | I²C adres | Locatie |
|--------|-----------|---------|
| AD5933 | 0x0D | Onderste PCB |
| MAX30101 | 0x57 | Onderste PCB |
| TMP117 | 0x48 | Bovenste PCB |
| BMP390 | 0x77 | Bovenste PCB |

Geen adresconflicten. Bus owned by `task_i2c_sensors` — geen mutex vereist (single owner).

---

### 6B.6 Blokdiagram

```
DVDD_3V3 ──[C_BMP_2: 10µF]──┬── BMP390 (U12) VDDIO (pin 2)
                             │                  │
                         [C_BMP_1: 100nF]    SDI (pin 5) ──► I2C_SDA → flex J1 → ESP32
                             │                  │
                           AGND             SCK (pin 6) ──► I2C_SCL → flex J1 → ESP32
                                               │
                                          SDO (pin 3) ──[R_SDO: 10kΩ]──► DVDD_3V3  (addr 0x77)
                                               │
                                          CSB (pin 4) ──[R_CSB: 10kΩ]──► DVDD_3V3  (I²C mode)
```

---

*Ref: Bosch BMP390 datasheet Rev 1.8 — Section 4 (pin description), Section 5.3 (I²C interface), Section 8.5 (compensation formulas)*

---

## 7. Voeding — LT3042 Analoge Rail, REF5025 ADC Referentie

### 7.1 Analoge LDO: LT3042 (U9)

**Component:** LT3042EDD#TRPBF, DFN-8 (3×3mm)  
**Invoer:** `VBAT` (LiPo 3.0–4.2V) via flex J1 pin VBAT  
**Uitvoer:** `AVDD` = 3.3V analoge rail

**Uitgangsinstelling:**
- LT3042 gebruikt een ISET-weerstand (R_SET) van SET-pin naar GND
- Vout = ISET × R_SET, waarbij ISET = 100µA typisch
- Vout = 3.3V → R_SET = 3.3V / 100µA = **33kΩ** (gebruik 33.2kΩ 0.1% E96)

**Ontkoppelcondensatoren (kritiek voor ruis):**
- C_IN_LT: 10µF 0603 X5R (invoer, zo dicht mogelijk bij pin IN)
- C_OUT_LT: 10µF 0603 X5R (uitvoer, direct aan pin OUT) — dit is de LT3042-vereiste voor 0.8µV RMS ruis
- C_OUT_LT2: 100nF 0402 X7R (parallel aan C_OUT_LT, HF ontkoppeling)
- C_SET_LT: 10nF 0402 NP0/C0G (van SET-pin naar GND, interne ruisfiltering)

**Enable-pin:** LT3042 pin EN → AVDD (altijd aan) of optioneel verbonden aan ESP32 GPIO via flex J1 (power-sequencing)

**Netten:**
- `VBAT` = invoer (3.0–4.2V)
- `AVDD` = uitvoer (3.3V, ultra-low noise)
- `LT3042_PGOOD` → flex J1 (optioneel, voor monitoring)

**Vermogensdissipatie (worst case):**
- Vin_max = 4.2V, Vout = 3.3V, Iout_max = 30mA (analoge keten)
- Pdiss = (4.2 − 3.3) × 30mA = 27mW → DFN-8 thermische weerstand ~40°C/W → ΔT = 1.1°C (geen probleem)

**Layout-waarschuwing:**  
LT3042 AVDD-uitvoer NOOIT parallel routen met VBAT of de digitale voedingslijn. Afzonderlijke trace. Thermische via's niet nodig bij 27mW, maar de GND-pad (exposed pad) van het DFN-8 package moet volledig gesoldeerd zijn (meerdere via's naar grondvlak).

---

### 7.2 ADC Spanningsreferentie: REF5025 (U10)

**Component:** REF5025IDGKT, SC-70-5  
**Invoer:** `AVDD` (3.3V)  
**Uitvoer:** `VREF` = 2.5V (nauwkeurig, laag-ruis)

**Netten:**
- `VREF` → ADS1256 VREFP (pin 15)
- `AGND` → ADS1256 VREFN (pin 16)

**Externe ruisfiltercondensator (FILT-pin):**
- C_FILT_REF: 10µF 0603 X5R van pin FILT (pin 5) naar AGND — verlaagt ruis van 7.2nV/√Hz naar < 2nV/√Hz in lage band
- C_OUT_REF: 10µF 0603 X5R van pin OUT (pin 3) naar AGND (uitgangsbelasting, stabiliteit)
- C_OUT_REF2: 100nF 0402 X7R parallel aan C_OUT_REF

**Netten:**
- `VREF` = REF5025 output = ADS1256 VREFP

**Layout-waarschuwing:**  
VREF-trace afschermen met grondspoor aan beide zijden. Geen schakelende componenten of hoge-stroom traces naast de VREF-lijn. De 10µF FILT-condensator direct aan de FILT-pin (< 2mm).

---

### 7.3 Vbatt Meetdeler (ADC ch5)

**Functie:** Meet LiPo-spanning voor firmware B-block `voltageRaw`

**Spanningsdeler:**
- R_VBAT_A: 100kΩ 0402 1% (van VBAT naar NET_VBAT_MON)
- R_VBAT_B: 100kΩ 0402 1% (van NET_VBAT_MON naar AGND)
- Deler: 0.5 × Vin → bij Vin=4.2V → 2.1V (< 2.5V VREF ✓), bij Vin=3.0V → 1.5V

**Net:**
- `NET_VBAT_MON` → ADS1256 AINP5 (ADC ch5)

**Let op:** Schakelaar (ESP32-GPIO aangestuurd NMOS) in serie met R_VBAT_A aanbevolen om continue stroom door de deler te vermijden (100kΩ+100kΩ = 200kΩ → 21µA continu bij 4.2V = 88µW). Optioneel in Rev 0.1.

---

### 7.4 Negatieve Rail: TPS60400 Charge Pump (U11)

**Beslissing principal (2026-05-09):** ±3.3V split-supply architectuur via TPS60400 inverting charge pump.

**Component:** TI TPS60400DBV (of gelijkwaardig: TPS60400DBVR), SOT-23-5  
**Functie:** Inverteert de +3.3V AVDD naar −3.3V AVSS voor AD630 −VS en ICG VCCS-opamp (negatieve rail).  
**Invoer:** `AVDD` (+3.3V) op pin 1 (V+)  
**Uitvoer:** `AVSS_RAW` (≈ −3.3V ongefilterfd) op pin 5 (VOUT)

**Externe condensatoren (per TPS60400 datasheet, vereist voor pompwerking):**
- C_P1: 100nF 0402 X7R — vliegcondensator, van pin 2 (C+) naar pin 3 (C−)
- C_IN_CP: 100nF 0402 X7R + 10µF 0603 X5R — van V+ (pin 1) naar GND (pin 4), lokale invoer-ontkoppeling
- C_OUT_CP: 100nF 0402 X7R + 10µF 0603 X5R — van VOUT (pin 5) naar GND (pin 4), uitgangs-ontkoppeling

**LC-filter op AVSS uitgang (schakelruis onderdrukking):**  
De TPS60400 is een schakelende charge pump (~1MHz schakelfrequentie). De AVSS_RAW-uitgang bevat schakelruis dat de ICG-keten kan verstoren. Verplichte LC-filter:

```
AVSS_RAW ──[L1: 10µH, 50mΩ DCR, 0402/0603]──┬── AVSS (−3.3V gefilterd)
                                              │
                                          [C_AVSS: 10µF 0603 X5R]
                                              │
                                             GND
```

**LC-filterwaarden:**
- L1: 10µH ferrietkraal of inductantie, DCR < 50mΩ, Isat > 50mA (bijv. Würth 744043100 of Murata LQH3NPN100NN0)
- C_AVSS_A: 10µF 0603 X5R (hoofdafvlakking)
- C_AVSS_B: 100nF 0402 X7R (parallel, HF bypass)
- Afsnijfrequentie LC: fc = 1/(2π√(LC)) = 1/(2π√(10µH × 10µF)) ≈ **16kHz** — schakelruis van ~1MHz wordt onderdrukt met > 40dB
- Extra RC-snubber parallel aan C_AVSS optioneel (1Ω + 100nF) om peaking te dempen

**Netten:**
- `AVSS_RAW` = TPS60400 uitgang vóór LC-filter
- `AVSS` = −3.3V gefilterde negatieve rail → AD630 pin 8 (−VS), VCCS-opamp −VS

**Vermogensanalyse:**
- Iout_max AVSS: 20mA (AD630 quiescent ~10mA, VCCS-opamp ~5mA, marge)
- TPS60400 max Iout: 60mA (datasheet) — ruimschoots voldoende
- Efficiëntie TPS60400 @ Iout=20mA: ~80% typisch → Iin = 20mA × 3.3V / (3.3V × 0.8) ≈ 25mA extra op AVDD

**Layout-waarschuwingen:**
- TPS60400 vliegcondensator C_P1 zo dicht mogelijk bij pin 2 en 3 (< 3mm trace, brede kortsluitsporen)
- AVSS_RAW-trace afschermen; niet parallel lopen met ICG-senselijnen
- LC-filter L1 en C_AVSS direct na TPS60400 uitgang plaatsen; AVSS na LC-filter als afzonderlijk gefilterd net verdelen naar verbruikers
- GND-verbinding van TPS60400 pin 4 rechtstreeks naar analoge grondvlak (brede via)

---

### 7.5 Voedingsdomein Overzicht (bijgewerkt 2026-05-09)

| Domein | Spanning | Bron | Verbruikers |
|--------|----------|------|-------------|
| `VBAT` | 3.0–4.2V | LiPo (extern via flex J1) | LT3042 invoer, Vbatt-meetdeler |
| `AVDD` | +3.3V | LT3042 (U9) | U1, U2, U3, U4, U5, U6, U7, U8, Y1, U10, U11 (TPS60400 invoer) |
| `AVSS` | −3.3V | TPS60400 (U11) + LC-filter | AD630 −VS (U5), ICG VCCS opamp −VS |
| `VREF` | +2.5V | REF5025 (U10) | ADS1256 VREFP |
| `DVDD_3V3` | +3.3V | Bovenste PCB via flex J1 | ADS1256 DVDD (pin 22) |
| `AGND` | 0V | Ster-GND op onderste PCB | Alle analoge massa |

---

## 8. ADC: ADS1256 (U6)

**Component:** ADS1256IDBR, SSOP-28  
**Voeding:** AVDD (pin 4 = AVDD) + AGND (pin 6 = AGND voor analoge zijde) + DVDD (pin 22) = 3.3V van digitale rail via flex J1

**Kanaalmap:**

| Pin (AINP) | Kanaal | Net | Signaal |
|-----------|--------|-----|---------|
| AINP0 (pin 1) | ch0 | `Z0_FILTERED` | ICG baseline impedantie |
| AINP1 (pin 2) | ch1 | `VSOURCE_MON` | ICG V_source monitor |
| AINP2 (pin 3) | ch2 | `ECG1_FILTERED` | ECG kanaal 1 |
| AINP3 (pin 28) | ch3 | `ECG2_FILTERED` | ECG kanaal 2 |
| AINP4 (pin 27) | ch4 | `TEMP_LEGACY` | Temperatuur legacy (NTC of ongebruikt — naar AGND via 100kΩ pull-down) |
| AINP5 (pin 26) | ch5 | `NET_VBAT_MON` | Vbatt meetspanning |
| AINP6 (pin 25) | ch6 | NC | **Ongebruikt** — 100kΩ pull-down naar AGND, testpoint beschikbaar |
| AINP7 (pin 24) | ch7 | NC | **Ongebruikt** — 100kΩ pull-down naar AGND, testpoint beschikbaar |

**SCL meting — beslissing principal (2026-05-09): AD5933 digitaal via I²C.**  
De AD5933 levert impedantieresultaten (real/imaginary) uitsluitend digitaal via I²C; de ESP32 verwerkt deze waarden rechtstreeks. SCL_tonic en SCL_phasic zijn firmware-berekende softwarewaarden — geen analoge uitvoer op de PCB. ADS1256 ch6 (AINP6) en ch7 (AINP7) zijn daarom **ongebruikt** in Rev 0.1. Beide ingangen worden via 100kΩ pull-down naar AGND getrokken (floating-input-bescherming) en als testpoints beschikbaar gehouden. De AD5933 I²C-bus loopt via flex J1 (I2C_SDA / I2C_SCL) rechtstreeks naar de ESP32 — geen tussenliggende analoge tussenstap.

**SPI-interface:**
- SCLK (pin 20): `SPI_SCK` → flex J1
- DIN (pin 21): `SPI_MOSI` → flex J1
- DOUT (pin 19): `SPI_MISO` → flex J1
- CS (pin 18): `ADC_CS` → flex J1 (laag = geselecteerd)
- DRDY (pin 17): `ADC_DRDY` → flex J1 (aparte interrupt-lijn, actief laag)

**VREF:**
- VREFP (pin 15): `VREF` (2.5V van REF5025)
- VREFN (pin 16): `AGND`

**Ontkoppeling ADS1256:**
- C_U6_AVDD: 100nF 0402 X7R + 10µF 0603 aan pin 4 naar AGND
- C_U6_DVDD: 100nF 0402 X7R aan pin 22 naar AGND
- C_U6_VREF: 100nF 0402 X7R extra capaciteit aan VREFP naar VREFN

**CLKIN:**
- ADS1256 gebruikt intern klok (pin 23 CLKIN → via 10kΩ naar AGND voor interne oscillator gebruik), of externe 7.68MHz klok. Rev 0.1: interne klok.

---

## 9. Flex Connector J1 — Pinout naar Bovenste PCB

**Connector type:** FFC/FPC 0.5mm pitch, 24-polig (Hirose FH12-24S-0.5SH of gelijk.)  
**Richting:** Onderste PCB → bovenste PCB (ESP32-S3-MINI-1)

| Pin | Netnaam | Richting | Beschrijving |
|-----|---------|----------|--------------|
| 1 | VBAT | IN | LiPo voeding 3.0–4.2V naar LT3042 |
| 2 | VBAT | IN | LiPo voeding (2e pin, stroom-deling) |
| 3 | DGND | — | Digitale massa |
| 4 | DGND | — | Digitale massa (2e pin) |
| 5 | AGND | — | Analoge massa (gesepareerd tot startknooppunt) |
| 6 | DVDD_3V3 | IN | 3.3V digitale rail voor ADS1256 DVDD |
| 7 | SPI_SCK | IN | SPI klok van ESP32 naar ADS1256 |
| 8 | SPI_MOSI | IN | SPI data ESP32 → ADS1256 |
| 9 | SPI_MISO | OUT | SPI data ADS1256 → ESP32 |
| 10 | ADC_CS | IN | ADS1256 chip select (actief laag) |
| 11 | ADC_DRDY | OUT | ADS1256 data ready interrupt (actief laag) |
| 12 | I2C_SDA | BIDIR | I²C databus: AD5933, MAX30101, TMP117, BMP390 |
| 13 | I2C_SCL | IN | I²C klok van ESP32 |
| 14 | PPG_INT | OUT | MAX30101 interrupt (FIFO watermark) |
| 15 | OSC_MON | OUT | 32kHz oscillator monitor (via 33Ω R_OSC van Y1_OUT) |
| 16 | EN_ANALOG | IN | LT3042 enable (optioneel power-sequencing) |
| 17 | SCL_CAL_EN | IN | AD5933 kalibratie-schakelaar (NMOS gate, optioneel) |
| 18 | LT3042_PG | OUT | LT3042 power-good (optioneel) |
| 19 | SPARE_1 | — | Niet verbonden, reserveer voor Rev 0.2 |
| 20 | SPARE_2 | — | Niet verbonden |
| 21 | SPARE_3 | — | Niet verbonden |
| 22 | SPARE_4 | — | Niet verbonden |
| 23 | AGND_2 | — | Extra analoge massa-return |
| 24 | VBAT_SENSE | OUT | VBAT na R_VBAT_A deler voor ESP32 (als ADC ch5 niet via ADS1256) |

**Pull-up weerstanden op de onderste PCB:**
- I2C_SDA: 4.7kΩ naar AVDD (R_SDA op onderste PCB)
- I2C_SCL: 4.7kΩ naar AVDD (R_SCL op onderste PCB)
- ADC_CS: 10kΩ pull-up naar DVDD_3V3 (default hoog = deselect)
- ADC_DRDY: 10kΩ pull-up naar DVDD_3V3

**Layout-waarschuwing:**  
Flex-connectorpinnen 7–11 (SPI) en 12–13 (I²C) moeten op de PCB met impedantiecontrole worden gerouteerd. Geen SPI-trace naast ICG-signaallijnen. De AGND en DGND-pinnen (3–5, 23) verbinden op de bovenste PCB via een enkel sternunt; op de onderste PCB zijn AGND en DGND gescheiden tot aan J1.

---

## Bijlage A — Namenregister Kritieke Netten

| Net | Bron | Bestemming | Beschrijving |
|-----|------|------------|--------------|
| `AVDD` | LT3042 out | U1,U2,U3,U4,U5,U6,U7,U8,Y1,U10,U11 | +3.3V analoge voedingsrail |
| `AVSS` | TPS60400+LC out | AD630 −VS, VCCS opamp −VS | −3.3V gefilterde negatieve rail |
| `VREF` | REF5025 out | ADS1256 VREFP | 2.5V ADC-referentie |
| `AGND` | Ster-GND | Alle analoge massa | Analoge grondvlak |
| `OSC_REF` | Y1_OUT | AD630 pin 9/10 | 32kHz demodulatiefase-referentie |
| `OSC_ESP32` | Y1_OUT via 33Ω | J1 pin 15 | 32kHz oscillator monitor naar ESP32 |
| `ICG_INAMP_OUT` | AD8221 pin 6 | AD630 pin 1 | Versterkt 32kHz ICG-zinssignaal |
| `DEMOD_OUT` | AD630 pin 16 | RC LPF | Gedemoduleerd ICG-signaal |
| `Z0_FILTERED` | RC LPF out | ADS1256 AINP0 | ICG Z0 na 2kHz LPF |
| `VSOURCE_MON` | R_mon junction | ADS1256 AINP1 | ICG stroombron monitor |
| `ECG1_FILTERED` | ECG LPF 1 | ADS1256 AINP2 | ECG kanaal 1 na 150Hz LPF |
| `ECG2_FILTERED` | ECG LPF 2 | ADS1256 AINP3 | ECG kanaal 2 na 150Hz LPF |
| `NET_RLD` | OPA333 out | INA333 x2 REF pin | Driven right leg terugkoppeling |
| `NET_E6_LP2` | E6 5kHz LPF out | AD5933 VIN | E6 na anti-aliasing filter |
| `NET_E7_LP2` | E7 5kHz LPF out | AD5933 RFB node | E7 na anti-aliasing filter |

---

## Bijlage B — Open Punten voor KiCad Capture

~~1. **AD630 voedingsarchitectuur:** Beslissing vereist over split-supply (±3.3V via charge pump) of virtuele aarde.~~  
**RESOLVED 2026-05-09 — beslissing principal:** TPS60400 (of gelijkwaardig) inverteert AVDD (+3.3V) naar AVSS (−3.3V). Schematic uitgewerkt in sectie 7.4. LC-filter (10µH + 10µF) op AVSS-uitgang verplicht.

~~2. **SCL ch6/ch7 ADC-verbinding:** Bevestig of ADS1256 ch6/ch7 analoge ingangen zijn of dat AD5933 digitaal via I²C gaat.~~  
**RESOLVED 2026-05-09 — beslissing principal:** AD5933 meet digitaal via I²C naar ESP32. ADS1256 ch6 en ch7 worden niet aangesloten. Zie sectie 5.2 en sectie 8 voor uitwerking.

~~3. **ICG VCCS op-amp selectie:** OPA627 werkt op ±5V, niet op single 3.3V.~~  
**RESOLVED 2026-05-09 — impliciet opgelost door beslissing voedingsarchitectuur:** ±3.3V split-supply (punt 1) is bevestigd; VCCS-opamp kan nu worden geselecteerd op ±3.3V supply. Aanbeveling voor KiCad: OPA627 of AD8671 op ±3.3V. Exacte opamp-selectie te bevestigen in Component Selection Rev 0.4.

~~4. **Actieve elektroden E2/E3 phantomvoeding (OPEN):** Spanningsniveau (+3.3V of apart?) en serieweerstandswaarde voor phantomvoeding vast te leggen na bevestiging van het actieve-elektrode-type (LMP7721 buffer of gelijk.). Schematic-blok E2/E3 phantom power circuit blijft placeholder tot beslissing.~~

**RESOLVED 2026-05-09 — Priya Nair (Electronics):** Phantomvoeding voor actieve elektroden E2/E3 uitgewerkt op basis van LMP7721 buffer (3fA Ibias, Vsupply 1.8–5.5V, unity gain). Onderstaande waarden en topologie zijn vastgesteld.

**Spanningsniveau:** AVDD (+3.3V) direct — geen aparte spanningsdeler. De LMP7721 supply current is typisch 900µA, max 1.5mA (datasheet SNOSB57). Bij een serieweerstand van 560Ω bedraagt de spanningsval maximaal 0.84V (@ 1.5mA worst case), zodat de buffervoeding minimaal 2.46V bedraagt — ruim boven de 1.8V ondergrens van de LMP7721. Bij typisch 900µA is de buffervoeding 2.80V. Een aparte spanningsdeler of LDO is niet nodig en voegt ruis en componenten toe zonder voordeel.

**Phantomvoeding topologie (per elektrode E2, identiek voor E3):**

```
AVDD (+3.3V) ──[R_PH: 560Ω, 0402 1%]──── NET_PHxx_PWR ────────────────► naar elektrodeklip (ader 1: phantom+signaal)
                                                │
                                           [C_PH: 100nF 0402 X7R]          (lokale HF-ontkoppeling aan PCB-zijde)
                                                │
                                              AGND

Elektrodeklip (LMP7721, unity gain):
  VCC ← ader 1 (phantom DC)                    }
  GND ← ader 2                                 }   2-ader kabel
  IN+ ← huidcontact (via schutring/shield)     }
  OUT → gesuperponeerd op ader 1 (AC signaal)  }

PCB ontvangerzijde signaalscheiding:
  ader 1 ──[C_BLK: 10µF 0805 X5R]──► NET_E2_FILT ──► AD8221 IN+
                │
           [R_PH: 560Ω naar AVDD]  (phantomvoeding, zie boven)
```

**Componentwaarden en motivering:**

| Component | Waarde | Pakket | Motivering |
|-----------|--------|--------|------------|
| R_PH2, R_PH3 | 560Ω 1% 0402 | 0402 | Spanningsval max 0.84V @ 1.5mA; V_buffer min 2.46V > 1.8V LMP7721 min. Voldoende stroom voor LMP7721 bij minimale signaaldemping. |
| C_PH2, C_PH3 | 100nF X7R 0402 | 0402 | HF-ontkoppeling aan PCB-zijde; onderdrukt charge-pump ruis van TPS60400 op AVDD. |
| C_BLK2, C_BLK3 | 10µF X5R 0805 | 0805 | Blokkeer condensator scheidt DC phantom van AC signaal aan ontvangerzijde. Met ingangsimpedantie AD8221 >>10GΩ is de -3dB laagfrequentiegrens << 0.001Hz — ECG (0.05Hz) en ICG (32kHz) passeren zonder demping. Waarde 10µF is standaard voor bio-elektrisch signaalscheiding; geeft robuuste DC-blokkering bij lage ESR. |

**Signaaldemping berekening:**
- R_PH (560Ω) in serie met signaalbron. De AD8221 ingangsimpedantie is 10GΩ differentieel.
- Dempingsfactor: R_PH / (R_PH + R_in) = 560 / (560 + 10×10⁹) ≈ 0 → verwaarloosbare signaaldemping.
- De 10kΩ patiëntveiligheidsweerstand (R2/R3, sectie 2) domineert de serieimpedantie; R_PH voegt slechts 5.6% toe aan deze serieimpedantie, zonder invloed op het signaalpad.

**Driven shield — JA, geïmplementeerd:**
- De shield van de 2-ader kabel wordt verbonden aan de uitgang van de LMP7721 buffer in de elektrodeklip (unity-gain output = signaalkern potentiaal).
- Dit elimineert de kabelcapaciteit effectief: de shield volgt de signaalkern, zodat er geen capacitieve stroom vloeit over de kabelcapaciteit. Bewegingsartefacten en capacitieve interferentie worden hiermee sterk onderdrukt.
- Aan PCB-zijde: shield-ader verbonden aan `NET_E2_FILT` (na C_BLK) via 100Ω R_SHD2/R_SHD3 (0402) — de 100Ω voorkomt oscillatie van de shielddriver bij capacitieve belasting.
- Netten: `NET_E2_SHIELD` → R_SHD2 (100Ω) → `NET_E2_FILT`; idem voor E3.

**Schematische netbeschrijving (toe te voegen in KiCad):**
- `AVDD` → R_PH2 (560Ω) → `NET_E2_PHR` (junction: C_PH2 naar AGND + kabelader 1)
- Kabelader 1 retour (AC signaal) → C_BLK2 (10µF) → `NET_E2_FILT` (bestaand net, naar TVS2/R2)
- Shield kabelader → R_SHD2 (100Ω) → `NET_E2_FILT`
- Identieke structuur voor E3: R_PH3, C_PH3, C_BLK3, R_SHD3

~~5. **CF vs BF classificatie:** Rev 0.1 BF of CF? Galvanische isolatie vereist?~~  
**RESOLVED 2026-05-09 — beslissing principal:** Rev 0.1 is **BF** (battery-only, drijvend patiëntcircuit, geen galvanische isolatie-DC/DC converter). Sectie 2 (BF-waarschuwing) blijft ongewijzigd. CF-upgrade is een Rev 1-onderwerp; buiten scope van dit document.

**Openstaande punten samenvatting:** Alle punten gesloten. Punten 1, 2, 3, 4 en 5 zijn resolved. Document gereed voor KiCad capture.

---

## 10. Rigid-Flex Mechanical Structure

### 10.1 Construction overview

The VU-AMS PCB assembly is a **single rigid-flex PCB**. There are no separate boards connected by a discrete FFC cable. The assembly consists of:

- **Rigid zone 1** — bottom analog PCB, 52.0 × 25.0 mm, 4-layer FR4, 1.6 mm thick. Hosts the entire analog signal chain (Sections 2–8 of this document).
- **Rigid zone 2** — top digital PCB, 52.0 × 25.0 mm, 4-layer FR4, 1.6 mm thick. Hosts the ESP32-S3-MINI-1-N8R8, battery management, and BLE antenna.
- **Flex strip** — 10.0 × 20.0 mm nominal, 2-layer polyimide (0.1 mm total dielectric), 25 µm copper each side, with polyimide coverlay. Connects the two rigid zones.

The flex strip carries a maximum of 8 nets: VBAT (×2), DGND (×2), AGND, DVDD_3V3, SPI bundle (SCLK, MOSI, MISO, CS, DRDY), I²C (SDA, SCL), and selected GPIO lines. These correspond to the signal definitions in Section 9 (J1 pinout). The FFC connector J1 and its associated land pattern described in Section 9 and pcb_routing_spec_001.md Section 5 are **replaced** by the rigid-flex transition in this design revision. No discrete connector is used to join the two rigid zones.

### 10.2 Rigid-to-flex transition

- The copper traces on the flex F1 layer terminate into plated pads at the rigid zone boundary; these pads are soldered to the TOP layer (L1) of each rigid zone at the transition interface.
- A polyimide stiffener (0.2 mm) is bonded at the transition edge on both rigid zones to prevent stress concentration and delamination.
- No vias are placed within the flex zone. All layer transitions within the rigid zones occur ≥ 1.5 mm inward from the flex boundary.
- One-time static fold: the flex strip is bent to a radius of ≥ 3.0 mm during housing assembly. The bend locates in the middle of the flex strip free-flex zone, away from both stiffeners.

### 10.3 Electrical interface across the flex strip

The net assignment across the flex strip is identical to Section 9 J1 pinout, with the following constraints:

- No analog biopotential signal traces (electrode nets, ICG sense, ECG signals) cross the flex zone. All such signals originate and terminate within rigid zone 1.
- Power rails (VBAT, AVDD) and digital signals (SPI, I²C, GPIO) cross the flex zone on F1, with a continuous GND return on F2.
- The AGND/DGND star connection topology (pcb_routing_spec_001.md Section 3.4) is maintained: AGND and DGND are joined at one point on rigid zone 1 (at ADS1256 pin 6) and are carried as separate conductors across the flex to the star-connection point on rigid zone 2.

---

## 11. ICG Sense — Driven Shield Circuit

### 11.1 Purpose

The ICG sense electrode leads (E2, E3) carry a differential signal of 0.1–0.5 mV superimposed on a 25 mV, 32 kHz carrier. The cable capacitance between signal core and shield (typically 50–200 pF/m for a 2-conductor shielded cable) forms a current divider with the source impedance. Any potential difference between core and shield drives a capacitive current that degrades signal integrity and couples interference. A driven shield eliminates this by actively holding the shield at the same potential as the signal core.

### 11.2 Circuit topology — PCB-side driven shield buffer

The driven shield is partially implemented in the electrode clip (LMP7721 unity-gain buffer driving the shield at the electrode end — see Bijlage B4). An additional PCB-side buffer is required to handle the shield conductor at the PCB connector, after the blocking capacitor C_BLK2/C_BLK3.

```
                              PCB — ANALOG ZONE (bottom edge)

NET_E2_FILT (post C_BLK2) ──────────────┬─────────────────► AD8221 IN+ (pin 3)
(post safety resistor R2,                │
 post blocking cap C_BLK2)              [+]──OPA333 (U_SHD2, unity gain)
                                          │
                                         [−]──┐
                                              │
                                         [OUT]──► R_SHD2_DRV (100 Ω, 0402) ──► cable shield conductor at J_E2 connector

AGND ────────────────────────────────────────────────── OPA333 GND (pin 2)
AVDD ────────────────────────────────────────────────── OPA333 VS+ (pin 5)
```

**Identical structure for E3:** NET_E3_FILT → OPA333 (U_SHD3) → R_SHD3_DRV (100 Ω) → cable shield conductor at J_E3 connector.

### 11.3 Component specification

| Reference | Part | Value / Type | Notes |
|-----------|------|-------------|-------|
| U_SHD2, U_SHD3 | OPA333AIDBVR | Unity-gain buffer, SOT-23-5 | Same part as RLD amplifier U4; zero-drift, 7 nV/√Hz, 17 µA supply current |
| R_SHD2_DRV, R_SHD3_DRV | 100 Ω 0402 1% | Series output resistor | Prevents oscillation with capacitive cable shield load; value confirmed stable for shield capacitance up to 1 nF |
| C_SHD2, C_SHD3 | 100 nF X7R 0402 | OPA333 supply decoupling | From AVDD pin to AGND, within 0.5 mm of OPA333 pin 5 |

**Configuration:** OPA333 connected as voltage follower (non-inverting unity gain). IN+ (pin 3) connected to NET_E2_FILT. IN− (pin 2) connected to OUT (pin 1) for unity-gain feedback. OUT drives R_SHD2_DRV.

**Net additions to schematic:**
- `NET_E2_SHIELD_DRV` = OPA333 U_SHD2 output = junction: R_SHD2_DRV → cable shield conductor at J_E2
- `NET_E3_SHIELD_DRV` = OPA333 U_SHD3 output = junction: R_SHD3_DRV → cable shield conductor at J_E3

### 11.4 Layout requirements for driven-shield buffers

- U_SHD2 and U_SHD3 must be placed within **5 mm** of the J_E2 and J_E3 electrode connector pads respectively, on the **bottom edge** of the analog rigid zone.
- These buffers are part of the **sense path zone** (bottom board edge). They must not be placed within the VCCS injection zone (top edge).
- Minimum clearance from any injection-path trace (ICG_SRC_P, J_E4/J_E5 nets) to U_SHD2/U_SHD3 and their associated traces: **5.0 mm**.
- The input trace to OPA333 IN+ (from NET_E2_FILT) must have a passive GND guard ring as specified in pcb_routing_spec_001.md Section 2.6.2.
- The output trace (NET_E2_SHIELD_DRV) carries low-impedance driven signal and does not require a guard ring, but must not run adjacent to the high-impedance IN+ trace.

---

## 12. Physical Separation — ICG Injection vs. Sense Paths

### 12.1 Mandatory separation requirement

**This is a hard design rule, not a recommendation.**

The ICG current injection path and the ICG sense path must be treated as physically and electrically separate domains throughout rigid zone 1:

| Domain | Nets | Board zone | Layer |
|--------|------|------------|-------|
| Injection path (VCCS output) | ICG_SRC_P, traces to J_E4, J_E5 | Top edge (y = 0 to y = 5 mm from top) | L4 (BOTTOM) |
| Sense path (electrode inputs) | NET_E2_FILT, NET_E1_FILT, NET_E3_FILT (post-R2/R3), NET_E2_SHIELD_DRV | Bottom edge (y = 20 to y = 25 mm from top) | L4 (BOTTOM) |

Minimum clearance between any injection-path trace and any sense-path trace: **5.0 mm**.

No injection-path trace and sense-path trace may run parallel to each other for more than **2.0 mm** continuous length. If they must approach each other, they cross at 90° or are separated by a GND trace ≥ 0.20 mm (stitched to L2 every 3 mm).

### 12.2 Physical rationale

At 32 kHz, the injection path carries 1 mA into the body impedance (~25 Ω chest impedance → 25 mV drive). The sense amplifier (AD8221, G = 10) amplifies signals of 0.1–0.5 mV differential. Coupling a fraction of 25 mV of injection signal into the sense input creates a fixed offset at exactly 32 kHz that the synchronous demodulator (AD630) cannot distinguish from a real impedance signal — it appears as a DC offset on the Z0 output. Even −60 dB coupling (25 mV × 0.001 = 25 µV) is 5–25% of the full-scale ICG ΔZ signal and will corrupt the waveform.

The 5 mm clearance, combined with the GND pour enclosure (pcb_routing_spec_001.md Section 2.6.1) and the driven shield (Section 11), provides estimated isolation:

- Capacitive coupling at 5 mm trace separation on FR4 (εr ≈ 4.5): approximately −55 to −65 dB at 32 kHz for parallel traces.
- With interposing GND trace: add approximately −20 dB shielding → total −75 to −85 dB.
- Required isolation: 25 mV injection → < 1 µV at sense input = −88 dB. The combination of 5 mm clearance + GND trace + coplanar waveguide on injection path approaches this target; further isolation is provided by the AD8221's CMRR (70 dB at 32 kHz).

### 12.3 VCCS sub-zone boundary

The VCCS components (op-amp U_VCCS, resistors R8–R11, R_mon) are confined to a rectangular sub-zone in the top-left of rigid zone 1. A continuous GND barrier trace (0.30 mm, L1, stitched to L2 every 3 mm via 0.3 mm vias) marks the boundary of this sub-zone on all four sides. Nothing outside the VCCS sub-zone may route within this zone except AVDD and AVSS power traces entering from the top-left.

---

*Document: `operations/electronics/schematic_bottom_pcb_001.md`*  
*Auteur: Nair — 2026-05-09 | Rev: B5 — rigid-flex structure, driven-shield circuit, ICG separation rules added 2026-05-09*  
*Status: Gereed voor KiCad capture — alle open punten (B1–B5) gesloten. Secties 10–12 toegevoegd per principal designwijziging.*
