# Housing Brief Update 001
## VU-AMS — Behuizingsspecificatie, Revisie 0.1

**Author:** Tom Voss — Industrial/Product Design
**Date:** 2026-05-09
**Ref:** Brief 001, Addendum A, Addendum B, Vasquez Placement Validation 001
**Status:** DRAFT — ter review bij Nair (maatvoering PCB/connector) en Lam

---

## Inleiding

Dit document beschrijft de mechanische en materiaaltechnische specificaties voor de VU-AMS behuizing op basis van de huidige elektronica- en sensorbriefs. De Vasquez-verplichtingen voor het PPG optisch venster (gebogen contactvlak + zwarte siliconen lichtafdichtingsring) zijn hierin verwerkt als harde eisen, geen opties.

Buitenmaten: **55 × 55 × 22 mm** (L × B × H)

---

## Sectie 1 — PPG Optisch Venster

### 1.1 Sensor

**MAX30101**, package OLGA-14: 5,6 × 3,3 mm.

### 1.2 Venstermaat

Opening in behuizingsonderzijde: **7,0 × 5,0 mm** (± 0,1 mm).

Ruimte rondom de sensorpackage: ~0,7 mm per zijde — voldoende voor maattoleranting en lichtafdichtingsring-aansluit, niet groter dan nodig om omgevingslicht via randen te minimaliseren.

### 1.3 Venstermaterialen

Twee opties, in volgorde van voorkeur:

| Optie | Materiaal | Transmissie 660 nm | Transmissie 940 nm | Overweging |
|-------|-----------|-------------------|-------------------|------------|
| A | Optisch glas BK7 | > 92% | > 92% | Laagste kosten, standaard beschikbaar, voldoende voor VU-AMS |
| B | Sapfier (Al₂O₃) | > 85% | > 85% | Hardst, krasvast, geschikt bij frequent harness-contact; hogere kosten |

**Aanbeveling prototype fase:** BK7 optisch glas. Indien veldtests krasbeschadiging tonen bij gebruik met harness, overstap naar sapfier in Rev 2.

Wanddikte venster: **0,8 mm** — dun genoeg om optisch padlengte minimaal te houden, dik genoeg voor hantering en bevestiging.

Bevestiging: ingeperst met UV-uithardende optische lijm (Norland NOA 61 of equivalent), waterdicht naar IP54.

### 1.4 Lichtafdichtingsring

**Verplichting door Vasquez (Placement Validation 001, Sectie 1.1):** zwarte siliconen lichtafdichtingsring is **mandatory** om omgevingslicht-saturatie van de MAX30101 photodiode te voorkomen.

Specificatie:
- Materiaal: zwart siliconen, medisch kwaliteit (ISO 10993-5 biocompatibel)
- Shore hardheid: **A40**
- Ringbreedte: **1,5 mm** (radiaal, rondom vensteropeningperimeter)
- Compressie bij huidcontact: **0,8 mm** (nominale indrukking bij draagdruk ~10–16 g/cm²)
- Functie: optische afdichting tegen omgevingslicht EN zachte overgang huid-venster
- Kleur: zwart (lichtabsorptie, geen reflectie van LED-licht terug naar detector)

De ring is geïntegreerd in een ondiepe sleuf in de behuizingsonderzijde rondom het venster — niet los, niet gelijmd op huid, niet vervangbaar door gebruiker.

### 1.5 Gebogen Contactvlak — Anatomisch Aansluitend

**Verplichting door Vasquez:** vlakke onderzijde is niet acceptabel — resulteert in randcontact en luchtspeet onder sensor.

Geometrie:
- Contactvlak behuizingsonderzijde (centraal gebied rondom PPG venster): **concaaf, radius 150 mm**
- Radius past op suprasternale/claviculaire anatomie (suprasternale inkeping, tuberkel regio)
- Het gebogen vlak behelst een straal van **± 20 mm** rondom het venstermidden — buiten dat gebied is de onderzijde vlak voor elektrode-interface
- De lichtafdichtingsring (Sectie 1.4) loopt in dit gebogen vlak mee — ringcompressie is dus uniform onder draagdruk, ook bij anatomische variatie

Fabricage: CNC-frezen of injectiegietmatrijs met gebogen kernzijde (geen platte vlakke kern op dit lokale gebieden).

---

## Sectie 2 — E1 Elektrode Opening

### 2.1 Geometrie

- Ø **12 mm** opening, bovenzijde behuizing
- Verzonken **0,5 mm** t.o.v. behuizingsoppervlak — elektrode iets onder oppervlakteniveau, beschermt klip-mechanisme tegen stoten

### 2.2 Elektrode-interface

- **RVS klip-interface** (roestvrij staal) — snap-fit of schroefdraad klip (finaal type te bepalen i.s.m. Nair bij connector freeze)
- Klip zit vast aan behuizing, neemt E1 snap-on electrode op
- Elektrisch contact via RVS verend vlak op klipbodem, druk op electrode-gel

### 2.3 Huidafdichting

- **Ring van zacht siliconen** rondom opening, Shore A25
- Breedte: 2,0 mm radiaal, hoogte 1,0 mm (deels gecomprimeerd bij plaatsing)
- Functie: huidcontact en elektrisch-gel insluiting — voorkomt lekkage langs de rand bij zweten
- Materiaal: medisch siliconen, ISO 10993-5

---

## Sectie 3 — Kabeluittredingen

Alle kabeluittredingen gebruiken **siliconen trekontlasting**, kabeldiameter **Ø3 mm**, overmoulded of geclipsed in behuizingswand.

### 3.1 Bovenzijde

| Uittreding | Positie | Hoek | Elektrode(s) |
|------------|---------|------|--------------|
| E4 | Linksboven | 45° richting nek | ECG/ICG lead |
| E5 | Rechtsboven | 45° richting schouder/rug | ECG/ICG lead |
| USB-C | Midden bovenzijde | Loodrecht, mechanische beschermkap | Lading/data |

**USB-C:** zie Sectie 4 voor beschermkap specificatie.

Kabelhoeken (45°) zijn functioneel — reduceren trekbelasting bij beweging van hoofd/schouder.

### 3.2 Onderzijde

| Uittreding | Positie | Elektrode(s) |
|------------|---------|--------------|
| E2 + E3 | Links onderzijde | ICG sense / ECG actief |
| E6 + E7 | Rechts onderzijde | SCL paar (EDA) |

E2/E3 en E6/E7 verlaten elk als dubbelkabel (gemeenschappelijke trekontlasting), tenzij Nair aparte geleiderscreening vereist (coaxiaal per lead) — in dat geval twee separate Ø3 mm uittredingen per paar met 4 mm tussenafstand.

### 3.3 Trekontlasting detail

- Materiaal trekontlasting: zwart siliconen, Shore A50
- Inbeddingsdiepte in behuizingswand: 6 mm
- Buitenste buigradius bij belasting: min. 8 mm (kabelspecificatie-afhankelijk — bevestigen bij kabelkeuze)

---

## Sectie 4 — USB-C Mechanische Veiligheidskap (BF-eis)

**Achtergrond:** De VU-AMS is een medisch apparaat in huidcontact met actieve elektrische circuits. Opladen met patiënt aangesloten is een type-BF toepassing per IEC 60601-1. De beschermkap voorkomt dat de USB-C connector toegankelijk is tijdens meting.

### 4.1 Specificatie

| Parameter | Waarde |
|-----------|--------|
| Materiaal | PA66-GF30 (polyamide, 30% glasvezel) |
| Bevestigingsmethode | Bajonet (quarter-turn) |
| Bevestigingslogica | Vereist intentionele handeling — niet los te koppelen bij normaal gebruik |
| Kleur | **Rood (RAL 3020 Traffic Red)** |
| Opdruk | **"REMOVE BEFORE CHARGING"** (gegraveerd of pad-print, zwart op rood) |
| Minimale letterhoogte opdruk | 2,5 mm |

### 4.2 Bajonet-interface

- Quarter-turn: 90° rotatie vrijgeeft, 90° terugzet vergrendelt
- Vergrendelingshoek: hoorbare click bij vergrendeld (verend RVS element in bajonetsleuf)
- Vervangbaar los onderdeel: de kap is een consumable, reserveonderdelen in pakket meeleverbaar
- Wanneer kap verwijderd: USB-C poort volledig vrij, standaard USB-C kabel past direct

### 4.3 Visuele differentiatie

De rode kleur is functioneel, niet decoratief. Alle overige externe behuizingsonderdelen zijn mat zwart (zie Sectie 5). De rode kap is daardoor onmiddellijk herkenbaar als beveiligingselement, ook bij slecht licht.

---

## Sectie 5 — Materialen

### 5.1 Behuizingskorpus

| Parameter | Specificatie |
|-----------|-------------|
| Materiaal | PC/ABS blend (bijv. Bayblend T65 XF of equivalent) |
| Wanddikte | 1,5 mm nominaal |
| Afwerking exterieur | Mat zwart (VDI 30 textuur, of equivalent spuitgiettextuur) |
| Glansgraad | < 5 GU (60°) |

PC/ABS biedt goede slagvastheid bij lage temperatuur, machinaalbaarheid, en chemische resistentie voor huidsweetzuren.

### 5.2 Huidcontactzijde

| Parameter | Specificatie |
|-----------|-------------|
| Coating/materiaal | Medisch siliconen coating, overmoulded of gegoten laag |
| Shore hardheid | A25 |
| Biocompatibiliteit | ISO 10993-5 gecertificeerd (cytotoxiciteitstest) |
| Dikte | 0,5 mm (huidcontactlaag, buiten gebogen PPG-zone) |
| Kleur | Zwart |

De Shore A25 laag geeft een zachte, huidvriendelijke aansluiting die kleine onregelmatigheden in huidoppervlak overbrugt, relevant voor harness-gedragen gebruik.

### 5.3 Bevestigingsmiddelen

- Behuizingssluiting: **M2 RVS** (DIN 912 cilinderkopschroeven)
- Aantal: 4× (hoeken), eventueel + 1 centraal afhankelijk van PCB-montagestrategie (afstemmen met Nair)
- Schroefinzetten: M2 brass heat-set inserts in PC/ABS wand

---

## Sectie 6 — Maatvoering Verificatie

Bevestiging dat alle componenten passen binnen 55 × 55 × 22 mm (L × B × H).

### 6.1 Breedte (55 mm, X-as)

```
2× 14500 cel (Ø14 mm) + PCB (25 mm breed) = 14 + 25 + 14 = 53 mm
Beschikbaar: 55 mm
Speling: 2 mm (1 mm per zijde) — voldoende voor celhouder/foam
```

**Breedte: PASS ✓**

### 6.2 Hoogte (55 mm, Y-as)

```
14500 cel lengte: 50 mm
Beschikbaar: 55 mm
Speling: 5 mm — ruim voldoende voor celhouder-eindstops en connectordraden
```

**Hoogte: PASS ✓**

### 6.3 Diepte (22 mm, Z-as)

```
PCB stack:
  - Bottom PCB diktte:    1,2 mm
  - Flex tussenruimte:    1,0 mm
  - Top PCB dikte:        1,2 mm
  - Componenten top PCB:  ~10,0 mm (hoogste component, inclusief ESP32 module)
  Subtotaal PCB stack:    13,4 mm

Ruimte voor overige elektronica en connectors:
  22,0 mm − 13,4 mm = 8,6 mm beschikbaar

Verdeling:
  - Huidcontactlaag onderzijde:    0,5 mm
  - Gebogen PPG zone (extra):      0,5 mm (max excursie radius 150 mm over 20 mm straal)
  - Behuizingswand onderzijde:     1,5 mm
  - PCB-montagespeling onderzijde: 1,0 mm
  - [PCB stack 13,4 mm]
  - PCB-montagespeling bovenzijde: 0,8 mm
  - Behuizingswand bovenzijde:     1,5 mm
  - USB-C kap (ingebouwd):         0,9 mm (kap zit binnen behuizingscontour)
  Totaal gebruikt:                 ~20,1 mm
  Speling:                         1,9 mm
```

**Diepte: PASS ✓** — circa 1,9 mm speling, voldoende voor toleranties en veer-/montageonderdelen.

### 6.4 Samenvatting Maatvoering

| As | Beschikbaar | Gebruikt | Speling | Status |
|----|-------------|---------|---------|--------|
| Breedte (X) | 55 mm | 53 mm | 2 mm | PASS |
| Hoogte (Y) | 55 mm | 50 mm | 5 mm | PASS |
| Diepte (Z) | 22 mm | ~20,1 mm | ~1,9 mm | PASS |

Alle drie dimensies bevestigd. Speling in de Z-as is krap maar acceptabel. Voss adviseert Nair om bij eventuele wijzigingen in de component-hoogtekaart (bijv. andere ESP32 module met andere hoogte) onmiddellijk te flaggen — de Z-as heeft de kleinste marge.

---

## Openstaande punten

| Item | Eigenaar | Prioriteit |
|------|----------|------------|
| Bevestigen connector-type E1 klip (snap-fit vs. draad) met Nair | Nair + Voss | Hoog — voor matrijs-freeze |
| Bevestigen maximale component-hoogte top PCB (huidige aanname 10,0 mm) | Nair | Hoog — Z-as marge krap |
| Kabeldiameter E2–E7 bevestigen (aanname Ø3 mm) | Nair | Middel |
| Sapfier venster vs. BK7: finale keuze na prototype veldtest | Voss | Laag — Rev 2 beslissing |
| IP-rating target vaststellen (aanname IP54) | Lam | Middel — voor afdichtingsontwerp |

---

*Filed: `operations/electronics/housing_brief_update_001.md`*
*Voss — 2026-05-09*
