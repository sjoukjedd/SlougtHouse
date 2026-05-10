# Power Budget — VU-AMS Rev 0.1
**Datum:** 2026-05-08  
**Auteur:** Nair (Electronics) via Lam  
**Doel:** Accucapaciteit bepalen voor 24–50 uur gebruiksduur

---

## Vermogensbudget per component

| Component | Stroom (mA) | Toelichting |
|-----------|-------------|-------------|
| ESP32-S3-MINI-1 | 30 | 80MHz gemiddeld, BLE actief bij verbinding, SD-logging overig tijd |
| ICG analoge keten | 15 | Oscillator + stroombron + InAmp + demodulator + filters — continu |
| ECG InAmps × 2 + filters | 5 | Continu |
| 24-bit ADC (SPI) | 5 | Continu, 1kHz |
| ICM-20948 (9-DOF IMU) | 3 | Laagvermogenmodus bij stilstand |
| MAX30101 PPG | 2 | LED's duty-cycled: 100µs puls @ 100Hz = 1% duty, gem. LED-stroom ~2mA |
| SCL excitatie + sense amp | 8 | Continu (actieve EDA-meting) |
| TMP117 temperatuur | 0.1 | 1Hz polling, rest in shutdown |
| BMP390 barometrisch | 0.2 | 1Hz modus |
| SPI Flash / SD gemiddeld | 5 | Schrijfcycli gemiddeld over tijd |
| BQ25895 (niet aan het laden) | 0.1 | Standby |
| Actieve elektroden E2, E3 | 0.1 | LMP7721: 25µA per stuk |
| Status LED (duty-cycled) | 0.5 | Hartslag-knippering |
| **Systeemtotaal** | **~74 mA** | **@ 3.3V = ~244 mW** |

---

## Accu-uitvoer vereist

### Architectuurbeslissing: digitale rail via buck-converter

De digitale 3.3V rail (ESP32, IMU, PPG, temp, SD) moet worden gevoed via een **step-down (buck) converter** in plaats van een LDO.

Reden: bij een volle LiPo (4.2V) verspilt een LDO 21% van het digitale vermogen als warmte:
- LDO rendement (4.2V → 3.3V): 79%  
- Buck rendement (4.2V → 3.3V): 92%

De **analoge 3.3V rail** (ICG/ECG/SCL) blijft op een ultra-low-noise LDO — buck-schakelruis mag de microvolt-ECG en ICG-meting niet bereiken.

### Berekening

| | LDO (huidige ontwerp) | Buck (aanbevolen) |
|--|----------------------|-------------------|
| Systeemvermogen | 244 mW | 244 mW |
| Accu-rendement | 82% gemiddeld | 91% gemiddeld |
| Accustroom bij 3.7V | **80 mA** | **72 mA** |

### Capaciteit voor doelduur

| Doelduur | Capaciteit (netto) | Met 20% marge |
|----------|--------------------|----------------|
| 24 uur | 72 mA × 24h = **1728 mAh** | **2100 mAh** |
| 50 uur | 72 mA × 50h = **3600 mAh** | **4300 mAh** |

---

## Fysieke implicaties

| Capaciteit | Formaat (typisch LiPo pouch) | Gewicht |
|------------|------------------------------|---------|
| 2100 mAh | ~50 × 60 × 9 mm | ~45 g |
| 4300 mAh | ~70 × 80 × 11 mm | ~90 g |

Een accu van 4300 mAh in de borstkasbehuizing is te zwaar en te groot voor draagcomfort over 50 uur.

---

## Aanbeveling

**Ontwerp voor 2000–2200 mAh in de device-behuizing** (24–30 uur dekking).

Voor 50-uursstudies: **optioneel extern accupakket via USB-C** (passthrough-laden). Dit is standaardpraktijk bij onderzoeksambulante apparaten (Holter, MindWare). De BQ25895 ondersteunt passthrough-laden natively.

Voordelen:
- Device blijft compact en licht voor het overgrote deel van de studies (24h protocol)
- 50h studies zijn mogelijk zonder permanente compromissen in formaat
- USB-C is al aanwezig op het device

---

## Resterende open punt: BF of CF classificatie

De patiëntveiligheidsclassificatie bepaalt of de analoge voorkant (ICG/ECG-elektroden) volledig geïsoleerd moet zijn via een isolatie-DC/DC converter:
- **CF (cardiac floating)**: vereist galvanische isolatie op alle patiëntlijnen — voegt ~10–15 mA toe aan het verbruik (isolatie-DC/DC)
- **BF (body floating)**: drijvende patiëntaarde, minder strikt

Voor een apparaat dat direct op het hart meet (ICG = thoraximpedantie, ECG): **CF is de juiste classificatie**.  
Gevolg: isolatie-DC/DC converter toevoegen aan analoge patiëntrail → +10 mA budget, totaal ~82–85 mA. Capaciteitsadvies blijft ongewijzigd (marge dekt dit op).

**Actie:** Principal bevestigt CF-classificatie → Nair verwerkt isolatietransformator in schema Rev 1.

---

*Filed: `operations/electronics/power_budget_001.md`*
