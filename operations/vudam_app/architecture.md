# VU-DAMS Architecture

**VU-AMS Desktop Analysis & Management System** — offline analysis for `.vua` binary recordings.  
Java 21 · JavaFX 21 · Apache Commons Math 3 · Maven

---

## Module Breakdown

```
nl.vuams.vudam
├── VUDAMSApp.java              — JavaFX Application, stage bootstrap
├── io/
│   └── VUAFileReader.java      — Binary .vua parser (ByteBuffer, little-endian)
├── model/
│   └── DataBlock.java          — Sealed interface + record hierarchy
├── analysis/
│   ├── RPeakDetector.java      — Pan-Tompkins R-peak detection
│   └── HRVAnalyser.java        — Time-domain & frequency-domain HRV metrics
└── ui/
    └── MainController.java     — JavaFX FXML controller, chart wiring, CSV export
```

---

## Data Flow

```
.vua file on disk
      │
      ▼
VUAFileReader.read(Path)
  • reads 16-byte vuams_block_header_t per block
  • dispatches on type byte ('A','I','M','P','S','T')
  • deserialises payload into DataBlock sealed record subtype
      │
      ▼
List<DataBlock>                    (in-memory, full recording)
      │
      ├─── ABlock[] ──► MainController.populateEcgChart()
      │                      JavaFX LineChart (downsampled 4:1 for display)
      │
      ├─── IBlock[] ──► MainController.populateIcgChart()
      │                      SV(t), PEP(t) series
      │
      ├─── MBlock[] ──► MainController.populateImuChart()
      │                      ax/ay/az time series
      │
      └─── ABlock[] ──► RPeakDetector.detect()
                             • band-pass 5–15 Hz (IIR)
                             • 5-point derivative
                             • point-wise squaring
                             • 150 ms moving-window integration
                             • adaptive threshold, 200 ms refractory
                             │
                             ▼
                        List<Integer> peak indices
                             │
                             ▼
                        List<Double> rriMs
                             │
                             ▼
                        HRVAnalyser.compute()
                             • mean RRI, SDNN, RMSSD, pNN50
                             • 4 Hz resample (linear interpolation)
                             • FFT-based PSD (Commons Math FFT)
                             • LF (0.04–0.15 Hz), HF (0.15–0.40 Hz) band power
                             │
                             ▼
                        HRVAnalyser.Metrics record
                             │
                             ▼
                        TableView<HrvRow> + LineChart (RRI tachogram)
```

---

## Signal Sources and Sample Rates

| Block | Type | Rate    | Content                                  |
|-------|------|---------|------------------------------------------|
| A     | ECG  | 1000 Hz | 24-bit ADS1256, 2 channels, 250 samp/blk |
| I     | ICG  | per beat| z0, dZ/dt peak, PEP, LVET, CO, SV        |
| M     | IMU  | 100 Hz  | ICM-20948 9-axis, 10 samp/blk            |
| P     | PPG  | ~100 Hz | MAX30101 red/IR, SpO2, HR                |
| S     | EDA  | ~10 Hz  | AD5933 tonic + phasic SCL                |
| T     | Temp | 4 Hz    | TMP117 skin temperature                  |

---

## Binary File Format

### v2 layout — subject metadata header + data blocks

Files produced with firmware v2+ and a VU-DAMS session setup screen begin with a
variable-length **subject metadata header**, followed by the flat sequence of data blocks.
Legacy files (firmware v1, no session setup) omit this header and start directly with
the first data block. `VUAFileReader` auto-detects the magic bytes and handles both layouts.

#### Subject metadata header (v2+)

```
Offset  Len                Type       Field
0       4                  uint8[4]   magic = { 0x56, 0x55, 0x4D, 0x53 }  ("VUMS")
4       2                  uint16LE   header_len — total byte length of this section
                                       (counts from offset 0, so header content
                                        starts at offset 6)
6       4                  float32LE  electrode_distance_cm
                                       Distance between inner (voltage-sensing) ICG
                                       electrodes in cm. Required for the Kubicek SV
                                       formula (parameter L). Enter at session setup.
                                       NaN = not recorded.
10      2                  uint16LE   subject_id_len
12      subject_id_len     UTF-8      subject_id  (arbitrary string, may be empty)
12+sid  2                  uint16LE   recording_date_len
14+sid  recording_date_len UTF-8      recording_date (ISO-8601, e.g. "2026-05-09")
```

`header_len` = 4 + 2 + 4 + 2 + `subject_id_len` + 2 + `recording_date_len`.

All multi-byte integers are little-endian. The float is IEEE 754 single-precision LE.

**UI requirement (Reyes):** The session setup screen must expose a numeric field
"Electrode distance (cm)" that is mandatory before recording can start. The value must be
validated as a float in [5, 50] cm. It is written into the file header and also surfaced
in the analysis results header so it appears in every exported CSV.

#### Data block header (all versions)

Each block = 16-byte header + `payload_len` bytes of packed payload (little-endian throughout):

```
Offset  Len  Field
0       1    type byte          ('A'=0x41 … 'T'=0x54)
1       1    version            (0x01)
2       2    payload_len        uint16 LE
4       8    timestamp_us       uint64 LE (esp_timer_get_time())
12      4    reserved / padding
16      N    payload            (block-specific, packed, no internal padding)
```

---

## Planned Analysis Algorithms

### ECG / HRV (implemented)
- Pan-Tompkins pipeline: band-pass → derivative → square → MWI → adaptive threshold
- Time domain: mean RRI, SDNN, RMSSD, pNN50
- Frequency domain: uniform resample at 4 Hz, FFT PSD, LF/HF band power

### ICG (current: I-block derived params; waveform pending)
- **Current:** Raw ICG waveform is NOT stored in A-blocks. `task_adc.c` reads
  ADS1256 channels 3/4 (ICG_SENSE / ICG_REF) but writes only ECG ch0/ch1 into
  `a_block_t`. The I-block (`0x49`) carries per-beat firmware estimates:
  `z0`, `dZdt_peak`, `pep_ms`, `lvet_ms`, `co_lpm`, `sv_ml`.
- **Phase 1 (now):** Consume I-block fields directly. Apply Kubicek SV formula
  using `dZdt_peak`, `lvet_ms`, `z0`, and `electrode_distance_cm` from the
  subject metadata header.
- **Future (firmware v2 raw ICG block):** Offline B/X-point detection; Savitzky-Golay
  dZ/dt derivation; beat-by-beat waveform quality checks.

### IMU
- Activity classification (rest / walking / running) via accelerometer magnitude
- Artefact rejection: flag ECG/ICG beats coinciding with high-motion epochs

### PPG
- Cross-validate HR from PPG against ECG-derived HR
- SpO2 trending over recording duration

### EDA / SCL
- Phasic SCR peak detection (moving-baseline subtraction)
- Event-related skin conductance response (ER-SCR) if event markers present

---

## Roadmap

1. **v1.0** — File open, waveform display, ECG R-peak detection, HRV metrics, CSV export
2. **v1.1** — Synchronised zoom/scroll across all waveform panels; artefact flagging
3. **v1.2** — Raw ICG waveform input; offline B-point cursor editing; Kubicek SV
4. **v1.3** — EDA phasic detection; temperature trend overlay
5. **v2.0** — Session comparison; batch processing of multiple .vua files

---

*Slow Horses / VU Amsterdam — 2026*
