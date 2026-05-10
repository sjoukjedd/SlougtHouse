# Operations Log

| Date | Agent | Action |
|------|-------|--------|
| 2026-05-08 | Lam | Company stood up. Full team hired. Mission brief and interface register issued. |
2026-05-08 | Nair | Brief 001 issued: analog front-end & electrode topology
2026-05-08 | Lam | Intel from existing aap codebase captured: BLE GATT profile, data block structures (A/M/D/B/C/G/I blocks)
2026-05-08 | Vasquez | Study 001 commissioned: sensor & signal selection for next-gen VU-AMS
2026-05-08 | Lam | ICG specs confirmed by principal: 32kHz carrier, 1mA, 0-32Ω, 1kHz/24bit, back injection / front sense. Addendum A issued to Nair.
2026-05-08 | Lam | Competitor analysis completed and filed. 15 competitors mapped. Key finding: VU-AMS is the only wearable combining ECG+ICG with mobile app.
2026-05-08 | Nair | Block diagram updated to Rev 0.2: PPG AFE (MAX30101), body temp (TMP117), SCL/EDA chain added. ADC ch6/ch7 allocated to SCL. All 8 ADC channels now used.
2026-05-08 | Lam | Brief 001 Addendum B issued to Nair, Vasquez, Müller: PPG, body temp, SCL sensor specs, component candidates, placement validation tasks.
2026-05-08 | Lam | Electrode placement diagram updated to Rev 0.2: E6 and E7 (SCL/EDA, left chest) added. 7-electrode topology confirmed.
2026-05-08 | Nair | Block diagram updated to Rev 0.3: IMU upgraded from 6-axis (ICM-42688) to 9-axis (ICM-20948, adds magnetometer). M_block extended to mx/my/mz.
2026-05-08 | Nair | Block diagram updated to Rev 0.4: ESP32 updated to S3-MINI-1-N8R8 (8MB Flash + 8MB PSRAM, 240MHz, PCB antenna). Rev 0.5: digital LDO replaced by buck converter.
2026-05-08 | Lam | Power budget filed (power_budget_001.md): 72mA @ 3.7V, 2× 1000mAh cells, 28h runtime. Battery geometry confirmed in 55×55×22mm housing.
2026-05-08 | Nair | PCB layout filed (pcb_layout_001.svg Rev 0.2): 2× 52×25mm boards on flex, analog bottom / digital top, batteries left+right. E1 aperture upper portion, E4/E5 exit top (over shoulders).
2026-05-08 | Lam | Marketing set complete: flyer_a4.html, banners.html, email_template.html, congress_poster_a0.html, linkedin_card.html, datasheet.html, video_script.md. All 7 assets filed under operations/marketing/.
2026-05-08 | Standish | Leo Hart hired as Art Director & Web Designer (Hart). Agent file: agents/leo_hart.md. Added to roster.
2026-05-08 | Standish | Oliver Quinn hired as Technical Writer & Documentation Engineer (Quinn). Agent file: agents/oliver_quinn.md. Added to roster.
2026-05-08 | Standish | Marcus Bell hired as QA & Test Engineer (Bell). Agent file: agents/marcus_bell.md. Added to roster.
2026-05-08 | Standish | Dr. Ingrid Wolff hired as Regulatory Affairs & Compliance (Wolff). Agent file: agents/dr_ingrid_wolff.md. Added to roster.
2026-05-08 | Standish | Yusuf Okafor hired as Manufacturing & Production Engineer (Okafor). Agent file: agents/yusuf_okafor.md. Added to roster.
2026-05-08 | Standish | Alex Kim hired as DevOps & Build Engineer (Kim). Agent file: agents/alex_kim.md. Added to roster.
2026-05-09 | Nair | Component selection analog chain filed: component_selection_analog.md. 8 ICs geselecteerd incl. ADS1256, LT3042, AD630, INA333, AD8221.
2026-05-09 | Müller | Firmware architectuur filed: firmware_architecture_001.md. FreeRTOS taken, SPI arbitratie, PSRAM ringbuffer, P/S/T block formaten.
2026-05-09 | Principal | BLE beleid besloten: volledige resolutie naar SD, afgeslankt + delta-gecodeerd via BLE. ~1.6 kB/s BLE budget van 20 kB/s beschikbaar.
2026-05-09 | Principal | 32kHz referentie besloten: dedicated kristaloscillator op onderste PCB (niet ESP32 intern). Nair selecteert component.
2026-05-09 | Principal | Geen tweede ADC. 8-kanaals ADS1256 is definitief. Geen reservecapaciteit voor toekomstige signalen.
2026-05-09 | Principal | Inkoop on hold. Geen onderdelen bestellen tot nader order. Geldt voor Okafor (BOM/leveranciers) en Wolff (batterijkwalificatie).
2026-05-09 | Principal | Voedingsarchitectuur: ±3.3V charge pump (TPS60400). Echte bipolaire rails voor ICG keten.
2026-05-09 | Principal | SCL meting: AD5933 digitaal via I²C. ADS1256 ch6/ch7 ongebruikt.
2026-05-08 | Principal | Mara de Vries aangesteld als Vertrouwenspersoon (De Vries). Agent file: agents/mara_de_vries.md. Rapporteert direct aan principal — buiten managementlijn. Lam heeft geen inzage in vertrouwelijke zaken.
