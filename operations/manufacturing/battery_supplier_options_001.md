# VU-AMS — Batterijleverancier Evaluatie
## BATT-001, Rev 0.1

**Auteur:** Yusuf Okafor — Manufacturing & Production  
**Datum:** 2026-05-09  
**Status:** Draft — voor review door Wolff (Regulatory) en Lamb  
**Context:** Cel-selectie voor het VU-AMS wearable device (2× 14500 in serie of parallel, afhankelijk van pack-architectuur). Het apparaat is gecertificeerd conform IEC 60601-1 BF-type (skin-worn).

---

## 1. Achtergrond en eisen

Het VU-AMS maakt gebruik van twee 14500-cellen. De cel-eisen zijn:

| Parameter | Eis |
|-----------|-----|
| Formaat | 14500 (Ø14 mm × 50 mm nominaal) |
| Capaciteit | ≥ 1000 mAh per cel |
| Chemie | Li-ion of LiPo |
| Nominaalspanning | 3,6–3,7 V per cel |
| Temperatuurbereik | 0–40 °C (skin-worn gebruik) |
| Verplichte certificeringen | IEC 62133-2, UN38.3 |
| Medische context | Cel in IEC 60601-1 BF-gecertificeerd apparaat |
| Voorkeur documentatie | Testrapporten, MSDS/SDS, UN38.3 Summary beschikbaar |

**Opmerking regulatoir kader:** IEC 60601-1 clausule 15.4.3.4 vereist dat lithium-accumulatoren in een gecertificeerd medisch apparaat voldoen aan IEC 62133. UN38.3 is verplicht voor transport (lucht, zee, weg). Vanaf 2026 geldt in de EU aanvullend de EU Battery Passport-verplichting voor cellen in gereguleerde producten. Leveranciers met ISO 13485-gecertificeerde productieprocessen verdienen de voorkeur vanwege traceerbaarheid en kwaliteitsborging.

---

## 2. Overzicht geëvalueerde opties

### Optie A — Keeppower P1450C2

**Leverancier:** Keeppower (Shenzhen Keppower Technology Co., Ltd.), Shenzhen, China  
**Cel model / Partnummer:** P1450C2  
**Website:** [keeppower.com](https://keeppower.com/product/keeppower-14500-1000mah-protected-li-ion-rechargeable-battery-p1450c2/)

| Parameter | Waarde |
|-----------|--------|
| Capaciteit (nominaal) | 1000 mAh |
| Capaciteit (minimaal) | 950 mAh |
| Nominaalspanning | 3,6 / 3,7 V |
| Maximale spanning (laadafsluit) | 4,20 V ± 0,05 V |
| Ontlaadafsluitspanning | 2,75 V |
| Maximale ontlaadstroom | 1,9 A (continu) |
| Laadstroom | 500 mA |
| Afmetingen | Ø14,3 × 52,0 mm |
| Bescherming | Ingebouwde PCM (overcharge, overdischarge, overcurrent, short-circuit; IC van Japanse makelij) |
| Temperatuurbereik gebruik | 0–40 °C (skin-worn conform) |
| Chemie | Li-ion |

**Certificeringen:**  
- UN38.3 (bevestigd)  
- CE  
- RoHS  
- MSDS beschikbaar  
- CCC  
- EU Batterijverordening 2023/1542  
- IEC 62133: niet expliciet vermeld in retaildocumentatie — **directe verificatie bij Keeppower vereist**

**Beschikbare documentatie:**  
- MSDS: ja  
- UN38.3 Summary: ja (op aanvraag)  
- IEC 62133 testrapport: onbekend — opvragen  

**Levertijd:** Retail: directe beschikbaarheid (o.a. batterijservice.nl, NL). B2B/OEM: directe offerte Keeppower Shenzhen.  
**MOQ:** Retail: 1 stuk. OEM-directe inkoop: vermoedelijk 50–200 stuks (op te vragen).  
**Prijs prototype-aantallen:** ~€4,99/cel (batterijservice.nl, NL, retail). OEM-prijs lager bij directe bestelling.

**Geschiktheid medisch device:**  
Matig-goed. Keeppower levert degelijke beschermde cellen met bewezen performance (uitgebreid onafhankelijk getest, zie lygte-info.dk). Zwakheid: IEC 62133 formele certificering niet direct aangetoond in publieke documentatie. Beschermd formaat (PCM ingebouwd) is relevant voor IEC 60601-1 veiligheidsanalyse. Nader onderzoek vereist voor volledige medische dossieropbouw.

---

### Optie B — Topwell Power ICR14500 (1000 mAh)

**Leverancier:** Yichun Topwell Power Co., Ltd., Yichun, Jiangxi, China  
**Cel model / Partnummer:** ICR14500-1000 (productlijn; precieze P/N op te vragen)  
**Website:** [topwellpower.com](https://www.topwellpower.com/collections/14500-li-ion-battery)

| Parameter | Waarde |
|-----------|--------|
| Capaciteit | 1000 mAh (vermeld; verificatie bij bestelling vereist) |
| Nominaalspanning | 3,7 V |
| Afmetingen | 14,5 × 50,5 mm |
| Chemie | Li-ion |
| Max ontlaadstroom | op aanvraag (OEM-spec) |
| Temperatuurbereik | 0–45 °C ontladen (opgave fabrikant) |

**Certificeringen:**  
- IEC 62133 (bevestigd, incl. IEC 62133-2 voor Li-ion)  
- UN38.3 (bevestigd)  
- CE  
- RoHS / REACH  
- CB  
- UL  
- MSDS beschikbaar  

**Beschikbare documentatie:**  
- IEC 62133 testrapport: beschikbaar op aanvraag  
- UN38.3 Summary: beschikbaar  
- MSDS/SDS: beschikbaar  
- CB-certificaat: beschikbaar  

**Levertijd:** Samples 5–7 werkdagen; productie 15–25 werkdagen (industriestandaard).  
**MOQ:** Samples mogelijk laag (10–50 stuks); serieproductie 500+ stuks gebruikelijk. Op te vragen.  
**Prijs prototype-aantallen:** Niet publiek — directe offerte-aanvraag vereist. Verwacht €2–4/cel bij kleine aantallen, minder bij volume.

**Geschiktheid medisch device:**  
Goed. Topwell heeft ook een specifieke 14500 900 mAh variant gelabeld "for medical device", wat duidt op ervaring in deze sector. IEC 62133 én UN38.3 zijn beide aanwezig. Documentatiepakket compleet voor initiële regulatory dossieropbouw. ISO 13485 niet bevestigd — nadrukkelijk opvragen.

---

### Optie C — BENZO Energy 14500 (OEM/medisch)

**Leverancier:** Shenzhen Benzo Energy Technology Co., Ltd., Shenzhen, China  
**Cel model / Partnummer:** OEM 14500 Li-ion 1000 mAh (custom configuratie mogelijk; 2S2P-pack beschikbaar)  
**Website:** [benzoenergy.com](https://www.benzoenergy.com/lithium-ion-cell-manufacturer-customized-14500-2s2p-7-4v-1600mah-cylindrical-battery-pack-for-medical-equipment.html)

| Parameter | Waarde |
|-----------|--------|
| Capaciteit (als cel) | 1000 mAh (OEM-specificatie; ook pack-configuratie 2S2P 1600 mAh beschikbaar) |
| Nominaalspanning | 3,7 V (cel); 7,4 V (2S-pack) |
| Chemie | Li-ion |
| Max laadstroom | 1C |
| Cyclusleven | ≥ 500 cycli |
| Afmetingen cel | 14 × 50 mm nominaal |

**Certificeringen:**  
- IEC 62133 (bevestigd; UN38.3 Testing report beschikbaar)  
- UN38.3 (bevestigd)  
- CE / RoHS  
- MSDS beschikbaar  
- KC, UL, CB (productlijnbreed)  
- ISO 9001  
- ISO 13485: niet expliciet bevestigd voor dit product — opvragen

**Beschikbare documentatie:**  
- IEC 62133 testrapport: op aanvraag  
- UN38.3 Summary: beschikbaar  
- MSDS/SDS: beschikbaar  
- Certificaatpagina op website: [benzoenergy.com/lipo-battery-certificate](https://www.benzoenergy.com/lipo-battery-certificate)

**Levertijd:** Samples: 5–10 werkdagen; productie 3–5 weken.  
**MOQ:** Samples mogelijk laag (10–50 stuks). Serieproductie op aanvraag.  
**Prijs prototype-aantallen:** Niet publiek — offerte vereist. Verwacht vergelijkbaar met Topwell.

**Geschiktheid medisch device:**  
Goed tot zeer goed. BENZO heeft aantoonbare ervaring met 14500-packs specifiek voor medische apparatuur. De 2S2P-packoptie is relevant als Nair/Vasquez een hogere spanning of ingebouwde BMS-oplossing wil. Certificatiepagina biedt transparantie. ISO 13485 is het enige open punt.

---

### Optie D — Large Power (large.net / large-battery.com)

**Leverancier:** Large Power Co., Ltd. (Shenzhen Large Electronics Co., Ltd.), Shenzhen, China  
**Cel model / Partnummer:** Custom 14500 Li-ion 1000 mAh (OEM maatwerk)  
**Website:** [large.net/lithium-ion-battery](https://www.large.net/lithium-ion-battery/list-140/) | [large-battery.com](https://www.large-battery.com/medical-battery-solution/)

| Parameter | Waarde |
|-----------|--------|
| Capaciteit | ≥ 1000 mAh (OEM-specificeerbaar) |
| Nominaalspanning | 3,6–3,7 V |
| Ontlaadtemperatuur | −21 °C tot +60 °C (celchemie); skin-worn 0–40 °C ruim gehaald |
| Chemie | Li-ion |
| Maatwerk | Ja — volledig OEM inclusief BMS en kabelharnas |

**Certificeringen:**  
- IEC 62133 (bevestigd)  
- UN38.3 (bevestigd)  
- ISO 13485 (ISO 13485-gecertificeerde productielijn — sterkste punt)  
- ISO 9001  
- UL / CE / RoHS / REACH  
- FDA-gerelateerde certificeringen (voor US-markt)  
- IEC 61960  

**Beschikbare documentatie:**  
- Volledig medisch compliance-dossier op aanvraag  
- IEC 62133 testrapport: beschikbaar  
- UN38.3 Summary: beschikbaar  
- MSDS/SDS: beschikbaar  
- ISO 13485-certificaat: beschikbaar  

**Levertijd:** Samples 7–14 werkdagen; productie 4–6 weken (maatwerkontwerp inbegrepen).  
**MOQ:** Samples op aanvraag (typisch 5–20 stuks voor engineering samples); serieproductie 200–500 stuks.  
**Prijs prototype-aantallen:** Hogere instapprijs dan consumentenmerken; verwacht €4–7/cel voor kleine engineering-batches. Volume verlaagt sterk.

**Geschiktheid medisch device:**  
Zeer goed. Large Power is de enige leverancier in deze lijst met bevestigde ISO 13485-productiecertificering. Dit is een significante troef voor het medische technische dossier en de DHF (Device History File). Aanbevolen als primaire leverancier voor het certificeringstraject. Hogere prijs en langere doorlooptijd zijn de trade-offs.

---

## 3. Vergelijkingstabel

| # | Leverancier | Model | Capaciteit | IEC 62133 | UN38.3 | ISO 13485 | Documentatie | MOQ (indicatief) | Prijs/cel proto | Medisch geschikt |
|---|-------------|-------|------------|-----------|--------|-----------|--------------|------------------|-----------------|------------------|
| A | Keeppower | P1450C2 | 1000 mAh | Onbekend* | Ja | Nee | MSDS, UN38.3 | 1–50 stuks | ~€5,00 | Matig-goed |
| B | Topwell Power | ICR14500-1000 | 1000 mAh | Ja | Ja | Onbekend* | IEC 62133, UN38.3, MSDS, CB | 10–50 stuks | ~€2–4 | Goed |
| C | BENZO Energy | OEM 14500 | 1000 mAh | Ja | Ja | Onbekend* | IEC 62133, UN38.3, MSDS | 10–50 stuks | ~€3–5 | Goed |
| D | Large Power | Custom 14500 | ≥1000 mAh | Ja | Ja | **Ja** | Volledig medisch dossier | 5–20 samples | ~€4–7 | Zeer goed |

*= directe verificatie bij leverancier vereist vóór kwalificatie

---

## 4. Aanbeveling

**Primaire aanbeveling: Optie D — Large Power**

Large Power is de enige leverancier met een bevestigde ISO 13485-gecertificeerde productieomgeving. In de context van een IEC 60601-1 BF-gecertificeerd medisch apparaat is dit de sterkste positie voor het technisch dossier. De volledige certificatieset (IEC 62133, UN38.3, ISO 13485, CE, FDA) maakt de regulatory dossieropbouw het meest robuust. Aanbevolen voor het definitieve productietraject.

**Back-up / prototype-aanbeveling: Optie C — BENZO Energy**

Voor de eerste prototype-iteraties (batch ≤ 10 stuks conform BOM-001) is BENZO Energy een pragmatische keuze: bewezen 14500-ervaring voor medische apparatuur, IEC 62133 aanwezig, snelle levering van kleine aantallen, en een transparant certificatieprofiel. Kan worden ingezet voor functionele tests terwijl het Large Power-kwalificatietraject loopt.

**Optie B — Topwell Power** is aanvaardbaar als alternatieve leverancier of bij leveringsproblemen, maar vraagt aanvullende verificatie op ISO 13485.

**Optie A — Keeppower** wordt niet aanbevolen voor het medische dossier zolang IEC 62133-certificering niet formeel is aangetoond. Bruikbaar voor interne testbenches en laboratoriumprototypes buiten het officiële certificeringstraject.

---

## 5. Acties en volgende stap voor Wolff (Regulatory)

**Voor het certificeringstraject te starten zijn de volgende acties vereist:**

1. **Leveranciersselectie bevestigen** — Lamb/Okafor stellen vast welke leverancier(s) in aanmerking komen voor het officiële DHF. Aanbeveling: Large Power als primair, BENZO als back-up.

2. **Aanvragen bij Large Power (en BENZO als back-up):**
   - ISO 13485-certificaat (scope en uitgiftedatum)
   - IEC 62133-2 testrapport (cel-niveau, niet pakconfiguratie)
   - UN38.3 Summary Report
   - MSDS / SDS (Safety Data Sheet)
   - Celspecificatie-datasheet (formeel, met revisienummer)
   - EU Battery Passport-status (relevant per 2026)

3. **Leverancierskwalificatie** — Wolff initieert de Approved Supplier Procedure:
   - Verstuur leveranciersvragenlijst (QMS, productielocatie, traceerbaarheid)
   - Beoordeel ISO 13485-scope op toepasselijkheid voor celproductie
   - Documenteer in DHF als "Battery Cell Supplier Qualification Record"

4. **Regulatory mapping door Wolff:**
   - Verifieer dat cel-certificeringen (IEC 62133-2, UN38.3) aansluiten op de IEC 60601-1 veiligheidsanalyse en risicodossier (ISO 14971)
   - Controleer of UN38.3 Summary voldoet aan IATA DGR-eisen voor luchttransport van prototypes
   - Noteer EU Battery Passport-deadline 2026 als compliance-verplichting

5. **Engineering samples bestellen** — Na Wolff-fiat: order 10 engineering samples bij geselecteerde leverancier(s) voor celcharacterisatie door Vasquez en integratieverificatie door Nair.

6. **Elektrisch testplan** — Vasquez/Nair stellen testprotocol op voor cel-acceptatie (capaciteitsmeting, interne weerstand, thermisch gedrag bij huidtemperatuur 0–40 °C).

---

*Rapport opgesteld door Yusuf Okafor, Manufacturing & Production, Slow Horses.*  
*Verdere vragen: via Lamb.*
