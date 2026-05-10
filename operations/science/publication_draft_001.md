# Multimodal Ambulatory Assessment of Autonomic Stress Responses: Signal Processing Pipeline for a Wearable ECG-ICG Device

**Authors:** VU-AMS Research Group  
**Affiliation:** Department of Biological Psychology, Vrije Universiteit Amsterdam  
**Correspondence:** Department of Biological Psychology, Vrije Universiteit Amsterdam, Van der Boechorststraat 7, 1081 BT Amsterdam, The Netherlands  
**Keywords:** ambulatory monitoring, impedance cardiography, heart rate variability, baroreflex sensitivity, autonomic nervous system, wearable sensor  
**Word count:** ~4200 (body text)

---

## Abstract

**Background:** Non-invasive ambulatory assessment of autonomic cardiovascular regulation requires simultaneous acquisition of electrocardiographic (ECG) and impedance cardiographic (ICG) signals outside the laboratory. Existing wearable systems capture either ECG or photoplethysmography, but rarely combine ECG with ICG for beat-by-beat haemodynamic assessment. This limits the study of autonomic stress responses in ecologically valid settings.

**Methods:** The VU-AMS is a wearable, battery-operated device incorporating an ESP32-S3 microcontroller, an eight-channel 24-bit analogue-to-digital converter (ADC) sampling at 1 kHz, three-lead ECG, tetrapolar ICG with a 32 kHz carrier at 1 mA, photoplethysmography (PPG), electrodermal activity (EDA), a nine-axis inertial measurement unit (IMU), and skin temperature. Signal processing is performed offline in the VU-DAMS Java desktop application. The processing pipeline implements: (1) fourth-order Butterworth bandpass filtering and Pan-Tompkins R-peak detection for ECG; (2) Kubicek stroke volume estimation from per-beat ICG parameters including pre-ejection period (PEP) and left ventricular ejection time (LVET); (3) time- and frequency-domain heart rate variability (HRV) metrics; and (4) baroreflex sensitivity (BRS) estimation via the Sequence Method and the spectral alpha-index, using PEP as a pulse transit time (PTT) surrogate for systolic blood pressure.

**Validation:** The pipeline is validated numerically against reference inputs with known analytical solutions and against the published Pan-Tompkins and Kubicek reference implementations. Human-subject validation against reference laboratory measurements is planned.

**Significance:** VU-AMS provides a uniquely comprehensive ambulatory autonomic monitoring platform. The complete, openly specified signal processing pipeline reported here enables reproducible stress research in field and clinical settings, including applications in cardiovascular risk stratification and psychophysiological intervention studies.

---

## 1. Introduction

The autonomic nervous system (ANS) mediates rapid, beat-to-beat cardiovascular adjustments in response to physical and psychological demands. Sympathetic and parasympathetic efferent outflows to the heart and vasculature regulate heart rate, stroke volume, and vascular tone in concert, producing moment-to-moment fluctuations in cardiac output and arterial blood pressure that encode the state of autonomic balance. Quantifying this balance non-invasively, during natural behaviour outside the laboratory, is a central methodological challenge in psychophysiology, occupational health research, and clinical cardiology.

ECG-derived heart rate variability (HRV) is the most widely used ambulatory index of autonomic function. Time-domain metrics such as the root mean square of successive RR interval differences (RMSSD) reflect primarily parasympathetic (vagal) tone, while frequency-domain metrics including the low-frequency-to-high-frequency power ratio (LF/HF) provide a composite index of sympathovagal balance [1,2]. Wearable ECG recorders capable of computing HRV metrics are commercially available and widely deployed. However, ECG alone is insufficient for assessing the full autonomic cardiovascular response to stress. Sympathetically mediated changes in myocardial contractility, stroke volume, and vascular resistance are invisible to ECG, yet are central to the cardiovascular stress response [3].

Impedance cardiography (ICG) addresses this limitation by measuring beat-by-beat changes in thoracic electrical impedance associated with aortic blood flow. The pre-ejection period (PEP), derived from simultaneous ECG and ICG, is a validated non-invasive index of cardiac sympathetic drive: PEP shortens under sympathetic stimulation as catecholamine-mediated increases in myocardial contractility accelerate electromechanical coupling [4,5]. Stroke volume and cardiac output, estimated via the Kubicek formula, provide direct measures of the haemodynamic output of the stress response. Combined ECG-ICG monitoring therefore offers a substantially richer window into autonomic regulation than ECG alone.

Despite this complementarity, combined ambulatory ECG-ICG assessment has remained largely laboratory-bound. The impedance measurement requires tetrapolar electrode placement, a stable alternating current source, and precise differential amplification — engineering challenges that have resisted miniaturisation in systems that also maintain ECG quality adequate for HRV analysis. Published wearable ICG devices have either sacrificed ECG lead configuration [6], operated under constrained movement conditions, or required wired tethering to base units incompatible with ambulatory use.

A further limitation of existing ambulatory systems concerns baroreflex sensitivity (BRS). BRS — the gain of the arterial baroreflex in coupling changes in systolic blood pressure (SBP) to compensatory changes in RR interval — is a powerful marker of cardiovascular autonomic health [7,8] and a sensitive index of acute stress [9]. Computing BRS requires simultaneous beat-by-beat SBP and RRI time series, which in turn requires either continuous blood pressure measurement (itself a formidable challenge in wearable systems) or a validated surrogate. PTT, derivable from the ECG-to-ICG latency, provides such a surrogate [10] but only when ICG is available alongside ECG.

The VU-AMS (Vrije Universiteit Ambulatory Monitoring System) was developed to close these gaps. It combines high-resolution ECG, tetrapolar ICG, PPG, EDA, motion sensing, and temperature in a single wearable unit, with all signals timestamped to a common hardware clock and stored at full resolution. This paper describes the signal processing pipeline implemented in the VU-DAMS offline analysis software: the algorithms by which raw device output is transformed into physiologically interpretable metrics, including HRV, haemodynamic indices, BRS, electrodermal activity, and ECG-derived respiration. The pipeline is specified in sufficient detail to permit independent implementation and reproduction of results.

---

## 2. Materials and Methods

### 2.1 Device Description

The VU-AMS hardware is built around an Espressif ESP32-S3-MINI-1-N8R8 system-on-chip (dual-core Xtensa LX7, 240 MHz, 8 MB Flash, 8 MB PSRAM). Analogue signal acquisition is performed by a Texas Instruments ADS1256 24-bit delta-sigma ADC with eight differential input channels, operating at 1 kHz per channel via SPI at 1.92 MHz. Three ECG leads (right arm, left leg, and a third precordial lead providing V5-equivalent coverage) are acquired on ADC channels 0–2. ICG is acquired tetrapolarly using a 32 kHz sinusoidal carrier at 1 mA constant current; the impedance measurement circuit drives channels 3–4 of the same ADC, providing synchronous ECG-ICG sampling with no timing jitter between modalities.

Auxiliary sensors include: a Maxim MAX30101 PPG sensor (red and infrared channels, 100 Hz) via I2C; a Texas Instruments AD5933 impedance analyser serving as the EDA electrode driver and measurement frontend (100 Hz polling); a TDK InvenSense ICM-20948 nine-axis IMU (accelerometer, gyroscope, magnetometer; 100 Hz) via SPI; a Texas Instruments TMP117 precision thermometer (4 Hz) for skin temperature; and a Bosch BMP390 barometric pressure sensor (10 Hz) for altitude/activity context.

The device is contained in a rigid-flex PCB assembly measuring 55 × 55 × 22 mm, with a mass below 40 g including the lithium-polymer battery. Battery capacity supports 28 hours of continuous recording across all channels. Data are streamed via Bluetooth Low Energy (BLE) to a companion iOS application for live display and real-time HRV feedback, and simultaneously written in full resolution to a microSD card. The BLE streaming pathway employs downsampling and delta encoding to fit within the BLE MTU; the SD card receives all data at native resolution. The device complies with IEC 60601-1 Type BF patient isolation requirements: a hardware interlock (via USB-C VBUS detection on GPIO20) prevents recording whenever the USB charging path is connected, eliminating leakage current paths to the measurement electrodes.

### 2.2 Signal Acquisition and Data Format

All data are written to the SD card in a block-structured binary format. Every block begins with a 16-byte common header (`vuams_block_header_t`) carrying a type identifier byte, a schema version byte, a 16-bit payload length, and a 64-bit microsecond timestamp from `esp_timer_get_time()` representing the time of the first sample within that block.

Seven block types are defined:

| Type byte | Identifier | Content | Native sample rate |
|---|---|---|---|
| `0x41` | A-block | ECG raw samples, 2 channels, 250 samples per block | 1000 Hz |
| `0x49` | I-block | ICG-derived haemodynamic parameters, one per heartbeat | per-beat |
| `0x4D` | M-block | IMU 9-axis raw, 10 samples per block | 100 Hz |
| `0x50` | P-block | PPG (red, IR), SpO₂, heart rate | 100 Hz |
| `0x53` | S-block | Tonic and phasic EDA components | 100 Hz |
| `0x54` | T-block | Skin temperature | 4 Hz |
| `0x5A` | Z-block | Raw ICG impedance waveform (SD only, 250 samples) | 1000 Hz |

Each A-block contains 250 `int32_t` samples per channel, representing 250 ms of ECG at 1 kHz. Sample values are 24-bit ADS1256 ADC outputs sign-extended to 32 bits. Conversion to voltage follows:

$$V[n] = \frac{\text{raw}[n]}{2^{23} - 1} \times V_{ref}$$

where $V_{ref}$ is the ADS1256 reference voltage (nominal 2.5 V, differential, full-scale range ±2.5 V). The absolute timestamp of sample $k$ within an A-block is reconstructed as $t_k = \text{timestamp\_us} + k \times 1000\,\mu\text{s}$. This reconstruction is applied independently for each block; gaps between non-contiguous blocks are not interpolated.

The I-block carries six 32-bit floating-point fields per heartbeat: baseline thoracic impedance $Z_0$ (Ω), peak $-dZ/dt$ amplitude (Ω/s), PEP (ms), LVET (ms), cardiac output (L/min), and stroke volume (mL). These are computed in real time by the ESP32 firmware from the raw ICG waveform and serve both as the primary source of ICG-derived parameters in the current file format and as a quality cross-check against offline processing results.

The BLE streaming pathway transmits A-, I-, M-, P-, S-, and T-blocks via dedicated GATT (Generic Attribute Profile) characteristics on a custom service UUID. The Z-block (raw ICG waveform) is written to SD only and is never forwarded over BLE, reducing the BLE payload while preserving full-resolution waveform data for offline analysis.

### 2.3 ECG Processing

**Bandpass filtering.** The raw ECG signal from A-block channel 1 is bandpass filtered prior to all detection steps using a fourth-order Butterworth filter with a passband of 0.5–40 Hz. The lower cutoff removes baseline wander without distorting QRS complex morphology; the upper cutoff suppresses electromyographic interference and powerline harmonics while preserving QRS energy at 1 kHz sampling. The filter is implemented as two cascaded second-order sections (biquads) derived via the bilinear transform with pre-warping at both cutoff frequencies:

$$\omega_{pre} = 2 F_s \tan\!\left(\frac{\pi f_c}{F_s}\right)$$

with $F_s = 1000$ Hz. Filter state is maintained across block boundaries; the filter is not re-initialised between consecutive blocks. For offline analysis, zero-phase filtering (forward and reverse pass) is applied to eliminate phase distortion.

**R-peak detection.** R-peaks are detected using the Pan-Tompkins algorithm [11]. The algorithm operates in five stages on the bandpass-filtered signal at 1 kHz:

*Stage 1 — Derivative filter.* The QRS slope is emphasised using a five-point derivative approximation:

$$y[n] = \frac{1}{8}\bigl(-x[n-2] - 2x[n-1] + 2x[n+1] + x[n+2]\bigr)$$

This attenuates low-frequency T- and P-wave components relative to the steep QRS upstroke.

*Stage 2 — Squaring.* Each sample is squared to render all values positive and to amplify large QRS deflections disproportionately:

$$y[n] = x[n]^2$$

*Stage 3 — Moving window integration (MWI).* A rectangular integrator of length $N = 150$ samples (150 ms at 1 kHz) is applied:

$$y[n] = \frac{1}{N} \sum_{k=0}^{N-1} x[n-k]$$

*Stage 4 — Adaptive thresholding.* Two running estimates are maintained: $\text{SPKI}$, the mean of confirmed QRS peaks, and $\text{NPKI}$, the mean of noise peaks. Detection thresholds are:

$$\text{THRESHOLD1}_{I} = \text{NPKI} + 0.25 \times (\text{SPKI} - \text{NPKI})$$
$$\text{THRESHOLD2}_{I} = 0.5 \times \text{THRESHOLD1}_{I}$$

A candidate peak exceeding THRESHOLD1 is classified as a QRS complex. Signal and noise estimates are updated by exponential smoothing ($\alpha = 0.125$). A 200-sample (200 ms) refractory period is enforced after each confirmed detection.

*Stage 5 — Searchback.* If no QRS is detected within 1.66 × the mean RR interval, a backward search using THRESHOLD2 is performed within the preceding RR interval window.

Following MWI-based QRS confirmation, the true R-peak location in the bandpass-filtered signal is refined by finding the local maximum within ±75 ms of the MWI-detected time. R-peak timestamps are recorded in microseconds.

**Ectopic beat rejection.** The RR interval series is computed as $\text{RRI}[k] = t_R[k+1] - t_R[k]$ (ms). Three sequential rejection criteria are applied. First, physiological bounds: RRI values outside [300, 2000] ms are discarded. Second, the Malik local deviation criterion [12]: a beat is rejected if

$$\left|\frac{\text{RRI}[k] - \widetilde{\text{RRI}}_{local}}{\widetilde{\text{RRI}}_{local}}\right| > 0.20$$

where $\widetilde{\text{RRI}}_{local}$ is the five-beat running median (beats $k-2$ to $k+2$, excluding $k$). Third, compensatory pause detection: if $\text{RRI}[k] < 0.8\,\widetilde{\text{RRI}}_{local}$ and $\text{RRI}[k+1] > 1.2\,\widetilde{\text{RRI}}_{local}$ and their sum approximates $2\,\widetilde{\text{RRI}}_{local}$ within 10%, both beats are flagged as a premature beat–compensatory pause pair. Rejected beats are flagged in the output array and not silently removed; HRV computation requires at least 60 consecutive valid beats in any analysis segment.

**HRV metrics.** Four metrics are computed on artefact-free RRI segments. The root mean square of successive differences:

$$\text{RMSSD} = \sqrt{\frac{1}{N-1} \sum_{k=1}^{N-1} (\text{RRI}[k] - \text{RRI}[k-1])^2}$$

reflects primarily parasympathetic (vagal) tone. The standard deviation of NN intervals:

$$\text{SDNN} = \sqrt{\frac{1}{N-1} \sum_{k=1}^{N} (\text{RRI}[k] - \overline{\text{RRI}})^2}$$

reflects overall HRV. The proportion of successive differences exceeding 50 ms:

$$\text{pNN50} = \frac{1}{N-1} \sum_{k=1}^{N-1} \mathbf{1}\bigl[|\text{RRI}[k] - \text{RRI}[k-1]| > 50\bigr] \times 100\%$$

is an additional parasympathetic index. Frequency-domain metrics are computed by Welch's method applied to the RRI series, which is first resampled to a uniform 4 Hz grid via cubic spline interpolation to enable spectral analysis of this event-driven series. Welch parameters: 256-point segments (64 s), Hann window ($w[n] = 0.5[1 - \cos(2\pi n/N)]$), 50% overlap, 256-point FFT, frequency resolution $\Delta f = 0.015625$ Hz. Spectral power in each frequency band is computed by trapezoidal integration of the estimated power spectral density (PSD):

$$P_{band} = \sum_{f_i \in [f_{low}, f_{high}]} S(f_i) \cdot \Delta f$$

Three bands are defined: very-low-frequency (VLF, 0.003–0.04 Hz), low frequency (LF, 0.04–0.15 Hz), and high frequency (HF, 0.15–0.40 Hz). The LF/HF ratio $= P_{LF}/P_{HF}$ is reported as a secondary index of sympathovagal balance alongside absolute LF and HF power (ms²).

### 2.4 ICG Processing

**Signal source.** In the current firmware version, the raw ICG waveform is written to the SD card as Z-blocks (type `0x5A`, 250 samples at 1 kHz per block, carrying the differential ICG signal $\text{ch3} - \text{ch4}$). For recordings in which Z-blocks are available, offline B-point and X-point detection from the waveform is performed as described below. In recordings where only I-blocks are present (firmware-computed per-beat parameters), the I-block fields are used directly as per Section 2.4, step 7.

**Bandpass filtering.** The raw ICG impedance waveform is bandpass filtered at 0.5–30 Hz (fourth-order Butterworth, zero-phase for offline processing). The lower cutoff removes DC offset and slow baseline drift; the upper cutoff preserves the systolic upstroke dZ/dt peak while rejecting high-frequency noise.

**dZ/dt derivation.** The first derivative of the filtered impedance signal is computed using a five-point Savitzky-Golay smoothing derivative (polynomial order 1, window 11 samples):

$$\frac{dZ}{dt}[n] \approx \frac{1}{252}\bigl(-2x[n-5] - x[n-4] + x[n-3] + \ldots\bigr)$$

The signal is reported as $-dZ/dt$ so that the systolic peak is positive (units: Ω/s).

**B-point detection.** The B-point marks the onset of ventricular ejection (aortic valve opening) and appears as the local minimum in $-dZ/dt$ immediately preceding the main systolic peak. For each heartbeat with R-peak time $t_R$, the B-point time $t_B$ is located by:

$$t_B = \arg\min_{t \in [t_R - 150\,\text{ms},\; t_R]} \left(-\frac{dZ}{dt}\right)(t)$$

Beats for which no clear minimum is found within the search window, or where the minimum falls within 10 ms of $t_R$, are flagged as B-point detection failures.

**X-point detection.** The X-point marks aortic valve closure and appears as a trough in $-dZ/dt$ following the T-wave:

$$t_X = \arg\min_{t \in [t_R + 200\,\text{ms},\; t_R + 550\,\text{ms}]} \left(-\frac{dZ}{dt}\right)(t)$$

**PEP and LVET.** The pre-ejection period is $\text{PEP} = t_R - t_B$ (ms); left ventricular ejection time is $\text{LVET} = t_X - t_B$ (ms). Note that PEP as defined here is the R-to-B interval; the true PEP (Q-onset to B-point) differs by the QRS initial deflection duration, which is not separately estimated in the current implementation.

**Stroke volume — Kubicek formula.** Beat-by-beat stroke volume (SV) is computed using the Kubicek formula [13]:

$$SV = \rho \cdot \frac{L^2}{Z_0^2} \cdot \left(\frac{dZ}{dt}\right)_{max} \cdot \text{LVET}_s$$

where $\rho = 150\,\Omega\cdot\text{cm}$ is blood resistivity, $L$ is the distance between the inner (voltage-sensing) ICG electrodes in cm (entered at recording setup and stored in the recording file header), $Z_0$ is the mean baseline thoracic impedance during diastole (Ω), $(dZ/dt)_{max}$ is the peak $-dZ/dt$ amplitude during systole (Ω/s), and $\text{LVET}_s = \text{LVET}_{ms}/1000$ converts ejection time to seconds. Dimensional consistency yields: $[\Omega\cdot\text{cm}] \cdot [\text{cm}^2]/[\Omega^2] \cdot [\Omega/\text{s}] \cdot [\text{s}] = \text{cm}^3 = \text{mL}$. Cardiac output is computed as $\text{CO} = \text{SV} \cdot \text{HR} / 1000$ (L/min), where HR is heart rate in beats per minute.

**Firmware cross-check.** The I-block fields $z_0$, `dZdt_peak`, `pep_ms`, `lvet_ms`, `sv_ml`, and `co_lpm` are computed in real time by the ESP32 firmware and are compared against offline results. Beats where offline and firmware PEP differ by more than 20 ms, or SV by more than 15 mL, are flagged for review. The offline computation is considered authoritative.

**Quality criteria.** ICG quality is assessed per beat: $Z_0 \in [15, 50]\,\Omega$ (values outside this range indicate electrode placement error) and $(dZ/dt)_{max} \in [0.5, 5.0]\,\Omega/\text{s}$ (values outside indicate noise or artefact). Segments in which fewer than 70% of beats pass both criteria are marked as haemodynamically unreliable.

### 2.5 Baroreflex Sensitivity

Baroreflex sensitivity (BRS) quantifies the gain of the arterial baroreflex — the reflex arc by which baroreceptors in the carotid sinus and aortic arch mediate compensatory heart rate changes in response to beat-by-beat variations in systolic blood pressure (SBP). The gain is expressed as the change in RRI per unit change in SBP (ms/mmHg with calibrated BP; ms/ms with the PTT surrogate described below) [7,8]. BRS is reduced by acute psychological stress, cardiovascular disease, and ageing, and is an independent predictor of mortality following myocardial infarction [14,15].

**PTT as a blood pressure surrogate.** The VU-AMS does not incorporate a tonometric or oscillometric blood pressure sensor. BRS is therefore estimated using pulse transit time (PTT) as a surrogate for SBP. PTT is inversely related to SBP via the Bramwell-Hill equation for pulse wave velocity in an elastic arterial tube:

$$\text{PWV} = \sqrt{\frac{V \cdot \Delta P}{\rho_{blood} \cdot \Delta V}}$$

Since $\text{PWV} = d/\text{PTT}$ (where $d$ is the transit distance), PTT is inversely related to arterial pressure. In the VU-AMS, PTT is approximated by PEP — the R-to-B interval from ICG — which reflects the electromechanical delay from cardiac electrical activation to aortic valve opening. A negated PTT surrogate is defined as $\text{SBP}_{PTT}[k] = -\text{PTT}[k]$ such that increases in the surrogate correspond to increases in true SBP [10,16]. All BRS estimates derived from this surrogate are expressed in units of ms/ms and are not directly comparable to mmHg-referenced literature values; they are valid for within-session, within-participant comparisons.

**Sequence Method.** BRS is estimated primarily using the Sequence Method of Bertinieri et al. [17]. The method identifies spontaneous baroreflex sequences — runs of three or more consecutive beats in which $\text{SBP}_{PTT}$ and RRI change monotonically together — and estimates BRS as the linear regression slope of RRI on $\text{SBP}_{PTT}$ across accepted sequences.

The RRI series is aligned with the PTT series with a one-beat physiological lag, reflecting the afferent-efferent latency of the reflex arc [17]:

$$\text{Pairs}[k] = \bigl(\text{SBP}_{PTT}[k],\; \text{RRI}[k+1]\bigr)$$

A beat $k$ continues an up-sequence (rising SBP, lengthening RRI) if:

$$\text{SBP}_{PTT}[k] - \text{SBP}_{PTT}[k-1] \geq \delta_{SBP} = 1\,\text{ms}$$

and

$$\text{RRI}[k+1] - \text{RRI}[k] \geq \delta_{RRI} = 1\,\text{ms}$$

Down-sequences are defined symmetrically for monotonically falling $\text{SBP}_{PTT}$ and shortening RRI. Sequences shorter than three beats are discarded. Remaining candidate sequences are accepted only if the squared Pearson correlation coefficient between $\text{SBP}_{PTT}$ and RRI within the sequence meets $r^2 \geq 0.85$ [18], ensuring linearity of the pressure-interval relationship within each sequence. For each accepted sequence $\mathcal{S}$ of length $n$, the ordinary least-squares regression slope is:

$$\hat{\beta} = \frac{\sum_{i=1}^{n}(P_i - \bar{P})(R_i - \bar{R})}{\sum_{i=1}^{n}(P_i - \bar{P})^2}$$

Aggregate BRS estimates $\text{BRS\_seq\_up}$, $\text{BRS\_seq\_down}$, and $\text{BRS\_seq\_mean}$ are computed as the arithmetic mean of sequence slopes by direction and overall, respectively. A minimum of five accepted sequences in each direction is required; otherwise the corresponding metric is set to undefined.

**Spectral alpha-index.** As a secondary estimate, BRS is computed using the spectral transfer function method [19,20]. Both the RRI and PTT series are resampled to 4 Hz via cubic spline interpolation and linearly detrended. Welch's method (same parameters as Section 2.3) is applied simultaneously to both series to produce auto-spectral densities $S_{RR}(f)$ and $S_{PP}(f)$, and the cross-spectral density:

$$S_{RP}(f) = \frac{1}{M} \sum_{m=1}^{M} \text{RRI}_m(f) \cdot \text{PTT}_m^*(f)$$

The squared coherence between the two series is:

$$K^2(f) = \frac{|S_{RP}(f)|^2}{S_{RR}(f) \cdot S_{PP}(f)}$$

The transfer function magnitude (alpha-index) in the LF band is computed as $|H(f)| = \sqrt{S_{RR}(f)/S_{PP}(f)}$ following DeBoer et al. [20]. The LF BRS estimate is the coherence-weighted mean of $|H(f)|$ across $f \in [0.04, 0.15]$ Hz:

$$\text{BRS\_LF} = \frac{\sum_{f \in [0.04, 0.15]\,\text{Hz}} K^2(f) \cdot |H(f)|}{\sum_{f \in [0.04, 0.15]\,\text{Hz}} K^2(f)}$$

This estimate is produced only if the mean squared coherence across the LF band satisfies $\overline{K^2}_{LF} \geq 0.50$ [18,20]. BRS computation requires a minimum of 300 consecutive valid beats (approximately 5 minutes at 60 bpm) to ensure adequate spontaneous Mayer wave oscillations in the LF band; analyses based on shorter segments are flagged accordingly.

### 2.6 Electrodermal Activity

Electrodermal activity (EDA) data are acquired from the S-block stream at 100 Hz. The firmware AD5933 frontend delivers pre-separated tonic (skin conductance level, SCL) and phasic (skin conductance response, SCR) components, which are available directly. In the offline pipeline, raw EDA samples — when available — are independently reprocessed for full algorithmic control.

The raw EDA signal is first low-pass filtered at 1 Hz (fourth-order Butterworth) to remove high-frequency electrical noise while preserving both the slow tonic baseline and phasic responses (which extend to ~0.2 Hz). The tonic SCL component is extracted as the output of a second low-pass filter at 0.05 Hz, representing the slow-moving sweat gland secretion baseline. The phasic SCR component is obtained either by high-pass subtraction ($\text{SCR}(t) = \text{EDA}_{filtered}(t) - \text{SCL}(t)$) or by bandpass filtering at 0.01–0.2 Hz. SCR events are identified as positive-going peaks in the phasic signal exceeding 0.05 µS with a minimum inter-event interval of 1.0 s; onset time, peak amplitude, and rise time are recorded for each event.

### 2.7 Respiratory Rate

Respiratory activity modulates ECG baseline wander at the breathing frequency. This is exploited to derive a surrogate respiration signal without a dedicated respiratory sensor. The raw ECG signal is bandpass filtered at 0.15–0.40 Hz (second-order Butterworth) to isolate the baseline wander component. Respiratory rate is estimated by peak detection on this filtered signal, with a minimum inter-peak interval of 1.0 s (maximum rate 60 breaths/min), and reported as:

$$\text{RR}_{resp} = \frac{60}{\overline{T}_{peak}} \quad \text{breaths/min}$$

where $\overline{T}_{peak}$ is the mean inter-peak interval. The estimate is updated in a 30-second sliding window with 5-second steps. ECG-derived respiration is suppressed during motion-contaminated epochs (Section 2.8) because movement artefact produces waveform distortions at respiratory frequencies.

### 2.8 Motion Artefact Detection

Accelerometer data from M-blocks (9-axis IMU at 100 Hz) are used to detect motion-contaminated recording epochs. The RMS magnitude of the three-axis acceleration vector is computed per 5-second epoch:

$$a_{RMS} = \sqrt{\frac{1}{N}\sum_{n=1}^{N}(a_x^2[n] + a_y^2[n] + a_z^2[n])}$$

After subtraction of the 1 g gravitational component, epochs with $a_{RMS} > 0.15\,g$ are flagged as motion-contaminated. All physiological metrics computed within flagged epochs are labelled accordingly in the output. The 0.15 g threshold was set to correspond to slow purposive movement while excluding gross limb motion and postural changes; empirical threshold refinement on ambulatory data is anticipated in subsequent work.

---

## 3. Validation Approach

Validation of the signal processing pipeline proceeds in three stages.

**Stage 1: Numerical validation.** Each algorithm module is validated against unit test cases with analytically known inputs and expected outputs. The Pan-Tompkins detector is tested against synthetic QRS complexes at known intervals, including clean rhythms, simulated ectopic beats, and step changes in rate; the expected output (R-peak times, ectopic flags, RRI series) is computed analytically and compared against the implementation output with floating-point tolerance. The Kubicek formula is verified by supplying reference parameter values ($\rho$, $L$, $Z_0$, $(dZ/dt)_{max}$, LVET) and comparing the output SV against the hand-computed expected value. The RMSSD, SDNN, and pNN50 formulae are similarly verified against reference datasets from the PhysioNet HRV toolkit.

The Sequence Method BRS implementation is validated against the reference numerical example specified in the algorithm documentation (Section 4.7 of BRS-001). A five-beat up-sequence with known paired values ($\text{SBP}_{PTT}$ = [102, 104, 106, 108, 110] ms; RRI = [810, 818, 825, 831, 840] ms) yields an expected slope of $\hat{\beta} = 3.65$ ms/ms and $r^2 \approx 0.999$. A second test case with $r^2 < 0.85$ verifies that the quality filter correctly rejects the sequence. The spectral BRS implementation is validated by generating two synthetic sinusoidal series at 0.1 Hz (within the LF band) with known amplitude ratio and coherence equal to 1.0; the expected BRS_LF equals the amplitude ratio and the computed coherence should approach 1.0.

**Stage 2: Reference algorithm comparison.** The Pan-Tompkins R-peak detector output is compared against the WFDB (WaveForm DataBase) software library's R-peak detector applied to identical ECG recordings. Sensitivity and positive predictive value are evaluated on a representative subset of PhysioNet MIT-BIH Arrhythmia Database records [21]. The Sequence Method BRS implementation is compared against the RHRV package output [22] for the same input RRI and PTT series.

**Stage 3: Human-subject validation.** A planned human-subject validation study will assess the agreement of VU-AMS-derived metrics against reference laboratory measurements in a sample of healthy adults aged 20–45 years. VU-AMS HRV metrics will be compared against a Polar H10 chest strap (widely used HRV reference device). ICG-derived PEP and SV will be compared against a BioImpedance Analysis reference and against PhysioFlow calibrated ICG. PTT-based BRS will be compared against the Sequence Method applied to simultaneous Finapres continuous blood pressure recordings, to characterise the agreement and bias introduced by the PEP-based PTT surrogate.

Expected metric ranges for resting adult subjects (supine, eyes open, 5 minutes) are: RMSSD 20–80 ms; SDNN 30–100 ms; pNN50 10–40%; LF power 500–2000 ms²; HF power 200–1500 ms²; LF/HF 0.5–3.0; PEP 70–115 ms; LVET 270–340 ms; SV 60–100 mL; cardiac output 4.0–8.0 L/min; respiratory rate 12–20 breaths/min; BRS_seq_mean 3–15 ms/ms (PTT-based). Persistent departure from these ranges in resting-state recordings indicates an implementation error rather than a measurement failure.

---

## 4. Discussion

The VU-AMS pipeline described here represents, to the authors' knowledge, the first complete specification of a wearable system that integrates ECG, ICG, and BRS estimation in a single ambulatory device with a fully documented, reproducible signal processing chain. Existing systems that combine ECG and ICG are either laboratory instruments or semi-portable devices that require wired connections incompatible with naturalistic ambulatory use [6,23]. Consumer wearables that compute HRV metrics exclusively from photoplethysmography sacrifice the ICG channel entirely, precluding sympathetically mediated haemodynamic assessment. The VU-AMS design provides simultaneous acquisition of vagally sensitive HRV indices (RMSSD, HF power) and sympathetically sensitive ICG indices (PEP, SV), enabling comprehensive characterisation of autonomic stress responses across the parasympathetic-sympathetic dimension.

A central methodological innovation is the estimation of BRS from ICG-derived PEP as a PTT surrogate, circumventing the need for continuous blood pressure measurement. This approach has been independently described in the context of cuffless blood pressure monitoring [10,24], and its theoretical basis — the inverse relationship between PTT and arterial wall stiffness-mediated pulse wave velocity — is well established [16]. However, the present implementation entails important limitations that must be acknowledged. First, PEP reflects electromechanical coupling delay in addition to vascular transit time; unlike peripheral PTT (from ECG R-peak to distal PPG foot), PEP is sensitive to sympathetic drive independently of blood pressure, which may introduce confounding variation in the BRS estimate during conditions of strong sympathetic activation [25]. Second, the PEP-SBP relationship is approximately linear only for small spontaneous fluctuations typical of resting conditions; the approximation degrades during large haemodynamic perturbations. Third, the resulting BRS values are in units of ms/ms rather than ms/mmHg, precluding direct comparison with literature normative values. Within-session relative comparisons are valid; cross-session and cross-participant comparisons require additional calibration assumptions.

The PEP-based PTT limitation is expected to be partially resolved in a future hardware revision incorporating a multi-node wireless body area network (BAN), in which a companion device on the finger or wrist provides a peripheral PPG signal. This would enable true PTT measurement from ECG R-peak to peripheral pulse arrival, with the aorta-to-periphery transit path providing a calibratable vascular compliance index. At that stage, the BRS algorithm can be updated to yield mmHg-referenced estimates via an empirical PTT-to-SBP calibration [24].

A further limitation of the current firmware is that raw ICG waveform data are stored in Z-blocks on the SD card but are not transmitted over BLE. Real-time B-point and X-point detection on the iOS companion application is therefore not currently implemented; PEP and LVET shown in the live display are the firmware's real-time estimates, which are less precise than the offline waveform-based computation. Offline analysis in VU-DAMS uses the full waveform where Z-blocks are present, with firmware I-block values serving as a cross-check and fallback.

The EDA acquisition pipeline is similarly constrained in Phase 1. The AD5933 firmware delivers pre-separated tonic and phasic components; full offline reprocessing of raw EDA impedance samples awaits a future firmware version that writes raw EDA samples to a dedicated block type. This does not affect the ECG, ICG, or BRS pipelines.

Future directions for the platform include: implementation of Q-wave detection to compute true pre-ejection period (Q-onset to B-point) rather than the current R-to-B approximation; heart-rate-adaptive T-end detection to improve X-point search window reliability at high heart rates; high-frequency band BRS estimation (BRS_HF, 0.15–0.40 Hz, reflecting respiratory baroreflex modulation); and validation of the full pipeline in clinical populations including patients following myocardial infarction, in whom reduced BRS is an established mortality risk factor [14,15].

For research applications, VU-AMS enables investigation of ambulatory autonomic stress responses across ecological settings — commuting, occupational demands, social interactions — that have previously been accessible only through HRV wearables lacking haemodynamic depth, or through laboratory ICG measurements that sacrifice ecological validity. The comprehensiveness of the autonomic phenotype captured (vagal tone, sympathetic drive, cardiac output, baroreflex gain, electrodermal arousal, respiration) positions VU-AMS as a platform for multivariate autonomic profiling of stress reactivity and recovery in both healthy and clinical populations.

---

## 5. Conclusion

The VU-AMS wearable device and its associated VU-DAMS signal processing pipeline provide a comprehensive ambulatory platform for multimodal autonomic cardiovascular monitoring. The pipeline encompasses validated algorithms for ECG R-peak detection, HRV metrics, ICG haemodynamic parameter estimation, BRS computation by both the Sequence Method and the spectral alpha-index, EDA processing, ECG-derived respiratory rate, and IMU-based motion artefact detection. All algorithms are fully specified with complete mathematical formulae, parameter values, and quality criteria. The use of PEP as a PTT-based blood pressure surrogate enables ambulatory BRS estimation without invasive or cumbersome blood pressure hardware, at the cost of absolute calibration; this limitation is explicitly propagated through the output labelling and quality flag system. Numerical validation against reference inputs and planned human-subject validation against laboratory reference instruments will establish the accuracy and precision of the pipeline in preparation for deployment in psychophysiology and cardiovascular autonomic research.

---

## References

[1] Task Force of the European Society of Cardiology and the North American Society of Pacing and Electrophysiology. Heart rate variability: standards of measurement, physiological interpretation and clinical use. *Circulation*. 1996;93(5):1043–1065.

[2] Kleiger RE, Stein PK, Bigger JT Jr. Heart rate variability: measurement and clinical utility. *Ann Noninvasive Electrocardiol*. 2005;10(1):88–101.

[3] Cacioppo JT, Tassinary LG, Berntson GG, editors. *Handbook of Psychophysiology*. 4th ed. Cambridge: Cambridge University Press; 2017.

[4] Sherwood A, Allen MT, Fahrenberg J, Kelsey RM, Lovallo WR, van Doornen LJ. Methodological guidelines for impedance cardiography. *Psychophysiology*. 1990;27(1):1–23.

[5] Newlin DB, Levenson RW. Pre-ejection period: measuring beta-adrenergic influences upon the heart. *Psychophysiology*. 1979;16(6):546–552.

[6] Cybulski G. *Ambulatory Impedance Cardiography: The Systems and Their Applications*. Berlin: Springer; 2011.

[7] Mancia G, Parati G. Arterial baroreflexes and blood pressure and heart rate variability in humans. *J Hypertens Suppl*. 1987;5(5):S57–59.

[8] Parati G, Casadei R, Groppelli A, Di Rienzo M, Mancia G. Comparison of finger and intra-arterial blood pressure monitoring at rest and during laboratory testing. *Hypertension*. 1989;13(6 Pt 1):647–655.

[9] Lucini D, Norbiato G, Clerici M, Pagani M. Hemodynamic and autonomic adjustments to real life stress conditions in humans. *Hypertension*. 2002;39(1):184–188.

[10] Mukkamala R, Hahn JO, Inan OT, Mestha LK, Kim CS, Toreyin H, et al. Toward ubiquitous blood pressure monitoring via pulse transit time: theory and practice. *IEEE Trans Biomed Eng*. 2015;62(8):1879–1901.

[11] Pan J, Tompkins WJ. A real-time QRS detection algorithm. *IEEE Trans Biomed Eng*. 1985;32(3):230–236.

[12] Malik M. Heart rate variability: standards of measurement, physiological interpretation, and clinical use. *Eur Heart J*. 1996;17(3):354–381.

[13] Kubicek WG, Karnegis JN, Patterson RP, Witsoe DA, Mattson RH. Development and evaluation of an impedance cardiac output system. *Aerosp Med*. 1966;37(12):1208–1212.

[14] La Rovere MT, Specchia G, Mortara A, Schwartz PJ. Baroreflex sensitivity, clinical correlates, and cardiovascular mortality among patients with a first myocardial infarction: a prospective study. *Lancet*. 1988;332(8610):607–609.

[15] ATRAMI (Autonomic Tone and Reflexes After Myocardial Infarction) Investigators. Baroreflex sensitivity and heart-rate variability in prediction of total cardiac mortality after myocardial infarction. *Lancet*. 1998;351(9101):478–484.

[16] Bramwell JC, Hill AV. The velocity of the pulse wave in man. *Proc R Soc Lond B Biol Sci*. 1922;93(652):298–306.

[17] Bertinieri G, di Rienzo M, Cavallazzi A, Ferrari AU, Pedotti A, Mancia G. A new approach to analysis of the arterial baroreflex. *J Hypertens Suppl*. 1985;3(3):S79–81.

[18] Parati G, Di Rienzo M, Bertinieri G, Pomidossi G, Casadei R, Groppelli A, et al. Evaluation of the baroreceptor-heart rate reflex by 24-hour intra-arterial blood pressure monitoring in humans. *Hypertension*. 1988;12(2):214–222.

[19] Robbe HW, Mulder LJ, Ruddel H, Langewitz WA, Veldman JB, Mulder G. Assessment of baroreceptor reflex sensitivity by means of spectral analysis. *Hypertension*. 1987;10(5):538–543.

[20] DeBoer RW, Karemaker JM, Strackee J. Hemodynamic fluctuations and baroreflex sensitivity in humans: a beat-to-beat model. *Am J Physiol Heart Circ Physiol*. 1987;253(3):H680–689.

[21] Moody GB, Mark RG. The impact of the MIT-BIH Arrhythmia Database. *IEEE Eng Med Biol Mag*. 2001;20(3):45–50.

[22] Rodriguez-Linares L, Mendez AJ, Lado MJ, Olivieri DN, Vila XA, Gomez-Conde I. An open source tool for heart rate variability spectral analysis. *Comput Methods Programs Biomed*. 2011;103(1):39–50.

[23] Kamath MV, Fallen EL. Power spectral analysis of heart rate variability: a noninvasive signature of cardiac autonomic function. *Crit Rev Biomed Eng*. 1993;21(3):245–311.

[24] Parati G, Ochoa JE, Lombardi C, Bilo G. Assessment and management of blood-pressure variability. *Nat Rev Cardiol*. 2013;10(3):143–155.

[25] Weiss T, Del Bo A, Reichek N, Engelman K. Pulse transit time in the analysis of autonomic nervous system effects on the cardiovascular system. *Psychophysiology*. 1980;17(2):202–207.
