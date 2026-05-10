package nl.vuams.vudam.analysis;

import nl.vuams.vudam.model.DataBlock;
import org.apache.commons.math3.complex.Complex;
import org.apache.commons.math3.transform.DftNormalization;
import org.apache.commons.math3.transform.FastFourierTransformer;
import org.apache.commons.math3.transform.TransformType;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Rule-based Human Activity Recognition (HAR) classifier (SP-002 §2).
 *
 * <p>Input: 100 Hz M-blocks (ICM-20948) and optional 25 Hz B-blocks (BMP390).
 *
 * <p>Processing:
 * <ol>
 *   <li>Extract overlapping 2-second windows (200 samples, 100-sample hop) from the
 *       M-block stream.</li>
 *   <li>Compute the feature vector for each window (§2.3).</li>
 *   <li>Apply the priority-ordered rule classifier (§2.4.2) to produce a raw class.</li>
 *   <li>Post-process with a 3-window median filter and a 4-window minimum-duration rule
 *       (§2.4.3).</li>
 *   <li>Aggregate to 30-second epochs and return a {@link List} of {@link HarResult}
 *       records (§2.5).</li>
 * </ol>
 *
 * <h3>Axis conventions and scale factors</h3>
 * <ul>
 *   <li>Accelerometer: raw int16 / 16384.0 → g  (ICM-20948 ±2g default, 1 LSB = 1/16384 g)</li>
 *   <li>Gyroscope:     raw int16 / 131.0 × (π/180) → rad/s  (ICM-20948 ±250 °/s default)</li>
 * </ul>
 *
 * <p>The classifier operates in g for RMS thresholds as defined in SP-002 §2.4.2.
 * All stated threshold values in m/s² in the spec are converted to g (divide by 9.80665)
 * for internal use since that is the natural unit of the calibrated signal.
 *
 * <p>Thread safety: instances are stateless after construction; calling
 * {@link #analyse(List, List)} concurrently from multiple threads is safe.
 */
public final class HarAnalyser {

    // -------------------------------------------------------------------------
    // Constants — scale factors
    // -------------------------------------------------------------------------

    /** ICM-20948 ±2g range: 1 LSB = 1/16384 g. */
    private static final double ACC_SCALE_G = 1.0 / 16384.0;

    /** ICM-20948 ±250°/s range: 1 LSB = 1/131 °/s, converted to rad/s. */
    private static final double GYRO_SCALE_RAD_S = (1.0 / 131.0) * (Math.PI / 180.0);

    /** Standard gravity m/s² — used only for converting spec thresholds stated in m/s². */
    private static final double G_MS2 = 9.80665;

    // -------------------------------------------------------------------------
    // Window / epoch sizing
    // -------------------------------------------------------------------------

    /** Samples per 2-second window at 100 Hz. */
    private static final int WIN_SAMPLES = 200;

    /** Hop between successive windows (1-second hop → 50 % overlap). */
    private static final int WIN_HOP = 100;

    /** Samples in a 30-second epoch at 100 Hz. */
    private static final int EPOCH_SAMPLES = 3000;

    /** Windows per 30-second epoch at 1-second hop. (30-2)/1 + 1 = 29 */
    private static final int WINDOWS_PER_EPOCH = 29;

    /** IMU sample rate Hz. */
    private static final double FS = 100.0;

    // -------------------------------------------------------------------------
    // FFT
    // -------------------------------------------------------------------------

    /** Zero-padded FFT size for dominant-frequency computation. */
    private static final int FFT_SIZE = 256;

    private static final FastFourierTransformer FFT =
            new FastFourierTransformer(DftNormalization.STANDARD);

    // -------------------------------------------------------------------------
    // Classifier thresholds (all in g unless noted)
    // -------------------------------------------------------------------------

    // Rule 1 — Lying
    private static final double R1_AZ_MEAN_MIN_G   = 9.0  / G_MS2;   // |az_mean| > 9.0 m/s²
    private static final double R1_LATERAL_MAX_G   = 1.5  / G_MS2;   // sqrt(ax²+ay²) < 1.5 m/s²
    private static final double R1_RMS_MAX_G        = 1.2;            // RMS_mag < 1.2 g

    // Rule 2 — Sitting
    private static final double R2_AXIS_MIN_G       = 7.0  / G_MS2;   // |ax| or |ay| > 7.0 m/s²
    private static final double R2_RMS_MAX_G        = 1.3;

    // Rule 3 — Standing
    private static final double R3_AZ_MEAN_MIN_G   = 8.5  / G_MS2;
    private static final double R3_RMS_MAX_G        = 1.15;
    private static final double R3_FDOM_MAX_HZ      = 0.5;

    // Rule 4/5 — Stairs
    private static final double R45_BARO_RATE_THR   = 0.5;            // Pa/s
    private static final double R45_FDOM_LO_HZ      = 1.5;
    private static final double R45_FDOM_HI_HZ      = 2.5;

    // Rule 6 — Walking
    private static final double R6_FDOM_LO_HZ       = 1.0;
    private static final double R6_FDOM_HI_HZ       = 2.5;
    private static final double R6_STEP_REG_MIN      = 0.7;
    private static final double R6_RMS_LO_G          = 1.1;
    private static final double R6_RMS_HI_G          = 1.6;

    // Rule 7 — Running
    private static final double R7_FDOM_LO_HZ       = 2.0;
    private static final double R7_FDOM_HI_HZ       = 4.0;
    private static final double R7_RMS_MIN_G         = 1.8;

    // Rule 8 — Cycling
    private static final double R8_FDOM_LO_HZ       = 0.5;
    private static final double R8_FDOM_HI_HZ       = 2.0;
    private static final double R8_GYRO_RMS_MAX      = 0.5;           // rad/s
    private static final double R8_AZ_MEAN_LO_G      = 8.0  / G_MS2;
    private static final double R8_AZ_MEAN_HI_G      = 10.5 / G_MS2;

    // Dominant-frequency noise floor (spectral amplitude in g)
    private static final double FDOM_NOISE_FLOOR_G   = 0.005;

    // Post-processing: minimum class duration in windows (4 s / 1-s hop = 4 windows)
    private static final int MIN_CLASS_WINDOWS = 4;

    // Barometric stair detection: 5-second window for pressure rate = 500 samples at 100 Hz
    private static final long BARO_RATE_WINDOW_US = 5_000_000L;  // 5 seconds in µs

    // -------------------------------------------------------------------------
    // Activity enum
    // -------------------------------------------------------------------------

    /** Activity class codes (SP-002 §2.4.1). */
    public enum Activity {
        LYING,
        SITTING,
        STANDING,
        WALKING,
        RUNNING,
        CYCLING,
        STAIRS_UP,
        STAIRS_DOWN,
        UNKNOWN
    }

    // -------------------------------------------------------------------------
    // Output record
    // -------------------------------------------------------------------------

    /**
     * Per-epoch HAR result (30-second epoch aligned with SP-001).
     *
     * @param windowStartMs   epoch start time from recording start [ms]
     * @param windowEndMs     epoch end time from recording start [ms]
     * @param activityClass   dominant activity class in the epoch
     * @param cadenceSpm      mean cadence during WLK/RUN windows [steps/min]; NaN if not applicable
     * @param motionIntensity normalised motion intensity 0–1: clamp((mean_rms_mag − 1.0) / 2.0, 0, 1)
     * @param altitudeChangeM barometric altitude change over epoch [m]; NaN if no B-blocks
     */
    public record HarResult(
            long windowStartMs,
            long windowEndMs,
            Activity activityClass,
            float cadenceSpm,
            float motionIntensity,
            float altitudeChangeM
    ) {}

    // -------------------------------------------------------------------------
    // Public API
    // -------------------------------------------------------------------------

    /**
     * Analyses a recording and returns per-epoch HAR results.
     *
     * @param mBlocks all M-blocks from the recording, in timestamp order
     * @param bBlocks all B-blocks from the recording, in timestamp order; may be empty
     * @return list of {@link HarResult} records, one per 30-second epoch
     */
    public static List<HarResult> analyse(List<DataBlock.MBlock> mBlocks,
                                          List<DataBlock.BBlock> bBlocks) {
        if (mBlocks.isEmpty()) return List.of();

        // Build flat double[] arrays for each channel at 100 Hz
        int nSamples = mBlocks.size() * 10;  // 10 samples per M-block
        double[] ax = new double[nSamples];
        double[] ay = new double[nSamples];
        double[] az = new double[nSamples];
        double[] gx = new double[nSamples];
        double[] gy = new double[nSamples];
        double[] gz = new double[nSamples];
        long[] tsUs  = new long[nSamples];   // timestamp per sample (µs)

        int idx = 0;
        for (DataBlock.MBlock mb : mBlocks) {
            long blockTs = mb.timestampUs();
            long stepUs  = 10_000L; // 10 ms = 100 Hz
            for (int i = 0; i < 10; i++) {
                ax[idx] = mb.ax()[i] * ACC_SCALE_G;
                ay[idx] = mb.ay()[i] * ACC_SCALE_G;
                az[idx] = mb.az()[i] * ACC_SCALE_G;
                gx[idx] = mb.gx()[i] * GYRO_SCALE_RAD_S;
                gy[idx] = mb.gy()[i] * GYRO_SCALE_RAD_S;
                gz[idx] = mb.gz()[i] * GYRO_SCALE_RAD_S;
                tsUs[idx] = blockTs + i * stepUs;
                idx++;
            }
        }

        // ---- Classify each 2-second window ----
        int nWindows = (nSamples - WIN_SAMPLES) / WIN_HOP + 1;
        Activity[] rawClass = new Activity[nWindows];
        double[]   rmsPerWin = new double[nWindows];   // mean RMS_mag per window for motion_intensity
        double[]   cadencePerWin = new double[nWindows]; // cadence_spm per window (NaN if not walking/running)

        Arrays.fill(cadencePerWin, Float.NaN);

        for (int w = 0; w < nWindows; w++) {
            int start = w * WIN_HOP;
            int end   = start + WIN_SAMPLES; // exclusive

            // Extract window slices
            double[] wAx = Arrays.copyOfRange(ax, start, end);
            double[] wAy = Arrays.copyOfRange(ay, start, end);
            double[] wAz = Arrays.copyOfRange(az, start, end);
            double[] wGx = Arrays.copyOfRange(gx, start, end);
            double[] wGy = Arrays.copyOfRange(gy, start, end);
            double[] wGz = Arrays.copyOfRange(gz, start, end);

            long winCentreUs = tsUs[start + WIN_SAMPLES / 2];

            WindowFeatures f = computeFeatures(wAx, wAy, wAz, wGx, wGy, wGz,
                                               winCentreUs, bBlocks);
            rmsPerWin[w]   = f.rmsMag;
            rawClass[w]    = classify(f, !bBlocks.isEmpty());

            if (rawClass[w] == Activity.WALKING || rawClass[w] == Activity.RUNNING
                    || rawClass[w] == Activity.STAIRS_UP || rawClass[w] == Activity.STAIRS_DOWN) {
                if (f.stepPeriodSamples > 0) {
                    cadencePerWin[w] = (60.0 * FS) / f.stepPeriodSamples;
                }
            }
        }

        // ---- Post-processing: 3-window median filter ----
        Activity[] smoothed = medianFilter3(rawClass);

        // ---- Post-processing: minimum 4-window class duration ----
        Activity[] finalClass = minDurationFilter(smoothed, MIN_CLASS_WINDOWS);

        // ---- Aggregate to 30-second epochs ----
        return aggregateEpochs(finalClass, rmsPerWin, cadencePerWin, tsUs, bBlocks, nSamples);
    }

    // -------------------------------------------------------------------------
    // Feature extraction
    // -------------------------------------------------------------------------

    /** Feature vector for a single 2-second window. */
    private record WindowFeatures(
            double axMean, double ayMean, double azMean,
            double rmsMag,
            double fDomHz,
            double stepRegularity,
            int    stepPeriodSamples,   // 0 if no clear peak
            double gyroRms,
            double baroRatePaS          // NaN if no baro
    ) {}

    private static WindowFeatures computeFeatures(double[] wAx, double[] wAy, double[] wAz,
                                                   double[] wGx, double[] wGy, double[] wGz,
                                                   long centreUs,
                                                   List<DataBlock.BBlock> bBlocks) {
        int n = wAx.length;

        // Per-axis mean
        double axMean = mean(wAx);
        double ayMean = mean(wAy);
        double azMean = mean(wAz);

        // Magnitude array (gravity-inclusive)
        double[] mag = new double[n];
        for (int i = 0; i < n; i++) {
            mag[i] = Math.sqrt(wAx[i] * wAx[i] + wAy[i] * wAy[i] + wAz[i] * wAz[i]);
        }

        // RMS of magnitude
        double rmsMag = rms(mag);

        // Dominant frequency via FFT (zero-pad to FFT_SIZE)
        double fDomHz = dominantFrequency(mag);

        // Autocorrelation for step regularity
        double[] acf = autocorrelation(mag);   // normalised, length n
        // Search peak in [40, 200) samples (0.4 s – 2.0 s at 100 Hz per spec §2.3.5)
        int peakLag = 0;
        double peakVal = -Double.MAX_VALUE;
        int lagMin = 40;
        int lagMax = Math.min(200, acf.length - 1);
        for (int k = lagMin; k <= lagMax; k++) {
            if (acf[k] > peakVal) {
                peakVal = acf[k];
                peakLag = k;
            }
        }
        double stepRegularity = Math.max(0.0, peakVal);
        int stepPeriodSamples = peakLag;

        // Gyro RMS
        double gyroRms = 0.0;
        for (int i = 0; i < n; i++) {
            gyroRms += wGx[i] * wGx[i] + wGy[i] * wGy[i] + wGz[i] * wGz[i];
        }
        gyroRms = Math.sqrt(gyroRms / n);

        // Barometric pressure rate: (latest_pressure - pressure_5s_ago) / 5.0
        double baroRate = Double.NaN;
        if (!bBlocks.isEmpty()) {
            double pNow  = interpolatePressure(bBlocks, centreUs);
            double pPast = interpolatePressure(bBlocks, centreUs - BARO_RATE_WINDOW_US);
            if (!Double.isNaN(pNow) && !Double.isNaN(pPast)) {
                baroRate = (pNow - pPast) / 5.0;
            }
        }

        return new WindowFeatures(axMean, ayMean, azMean, rmsMag, fDomHz,
                                  stepRegularity, stepPeriodSamples, gyroRms, baroRate);
    }

    // -------------------------------------------------------------------------
    // Dominant frequency (FFT, zero-padded to 256, search 0.5–10 Hz)
    // -------------------------------------------------------------------------

    private static double dominantFrequency(double[] mag) {
        double magMean = mean(mag);
        double[] padded = new double[FFT_SIZE];
        for (int i = 0; i < mag.length && i < FFT_SIZE; i++) {
            padded[i] = mag[i] - magMean;   // remove mean
        }

        Complex[] spectrum = FFT.transform(padded, TransformType.FORWARD);

        double freqRes = FS / FFT_SIZE;  // 100/256 ≈ 0.391 Hz
        int kLo = (int) Math.ceil(0.5 / freqRes);   // first bin >= 0.5 Hz
        int kHi = (int) Math.floor(10.0 / freqRes); // last bin <= 10 Hz
        kHi = Math.min(kHi, FFT_SIZE / 2);

        int bestK = -1;
        double bestAmp = 0.0;
        for (int k = kLo; k <= kHi; k++) {
            double amp = spectrum[k].abs();
            if (amp > bestAmp) {
                bestAmp = amp;
                bestK   = k;
            }
        }

        if (bestK < 0 || bestAmp < FDOM_NOISE_FLOOR_G) {
            return 0.0;  // no_dominant_freq
        }
        return bestK * freqRes;
    }

    // -------------------------------------------------------------------------
    // Normalised autocorrelation
    // -------------------------------------------------------------------------

    private static double[] autocorrelation(double[] x) {
        int n = x.length;
        double xMean = mean(x);
        double[] xc = new double[n];
        for (int i = 0; i < n; i++) xc[i] = x[i] - xMean;

        double[] r = new double[n];
        for (int k = 0; k < n; k++) {
            double sum = 0.0;
            for (int i = 0; i < n - k; i++) {
                sum += xc[i] * xc[i + k];
            }
            r[k] = sum;
        }
        // Normalise by r[0]
        if (r[0] > 0.0) {
            for (int k = 0; k < n; k++) r[k] /= r[0];
        }
        return r;
    }

    // -------------------------------------------------------------------------
    // Rule classifier
    // -------------------------------------------------------------------------

    /**
     * Applies the priority-ordered rule set (SP-002 §2.4.2).
     * All RMS / mean thresholds operate in g.
     */
    private static Activity classify(WindowFeatures f, boolean baroAvailable) {
        double absAzMean = Math.abs(f.azMean);
        double lateralMean = Math.sqrt(f.axMean * f.axMean + f.ayMean * f.ayMean);

        // Rule 1 — Lying
        if (absAzMean > R1_AZ_MEAN_MIN_G
                && lateralMean < R1_LATERAL_MAX_G
                && f.rmsMag < R1_RMS_MAX_G) {
            return Activity.LYING;
        }

        // Rule 2 — Sitting
        double absAxMean = Math.abs(f.axMean);
        double absAyMean = Math.abs(f.ayMean);
        if ((absAxMean > R2_AXIS_MIN_G || absAyMean > R2_AXIS_MIN_G)
                && f.rmsMag < R2_RMS_MAX_G) {
            return Activity.SITTING;
        }

        // Rule 3 — Standing
        if (absAzMean > R3_AZ_MEAN_MIN_G
                && f.rmsMag < R3_RMS_MAX_G
                && f.fDomHz < R3_FDOM_MAX_HZ) {
            return Activity.STANDING;
        }

        // Rule 7 — Running (evaluated before Walking to take priority when both match)
        if (f.fDomHz >= R7_FDOM_LO_HZ && f.fDomHz <= R7_FDOM_HI_HZ
                && f.rmsMag > R7_RMS_MIN_G) {
            return Activity.RUNNING;
        }

        // Rule 4 — Stairs up (ascending): pressure decreasing → baro_rate < -0.5
        if (baroAvailable && !Double.isNaN(f.baroRatePaS)
                && f.baroRatePaS < -R45_BARO_RATE_THR
                && f.fDomHz >= R45_FDOM_LO_HZ && f.fDomHz <= R45_FDOM_HI_HZ) {
            return Activity.STAIRS_UP;
        }

        // Rule 5 — Stairs down (descending): pressure increasing → baro_rate > +0.5
        if (baroAvailable && !Double.isNaN(f.baroRatePaS)
                && f.baroRatePaS > R45_BARO_RATE_THR
                && f.fDomHz >= R45_FDOM_LO_HZ && f.fDomHz <= R45_FDOM_HI_HZ) {
            return Activity.STAIRS_DOWN;
        }

        // Rule 6 — Walking
        if (f.fDomHz >= R6_FDOM_LO_HZ && f.fDomHz <= R6_FDOM_HI_HZ
                && f.stepRegularity > R6_STEP_REG_MIN
                && f.rmsMag >= R6_RMS_LO_G && f.rmsMag <= R6_RMS_HI_G) {
            return Activity.WALKING;
        }

        // Rule 8 — Cycling
        if (f.fDomHz >= R8_FDOM_LO_HZ && f.fDomHz <= R8_FDOM_HI_HZ
                && f.gyroRms < R8_GYRO_RMS_MAX
                && absAzMean >= R8_AZ_MEAN_LO_G && absAzMean <= R8_AZ_MEAN_HI_G) {
            return Activity.CYCLING;
        }

        // Rule 9 — Unknown
        return Activity.UNKNOWN;
    }

    // -------------------------------------------------------------------------
    // Post-processing
    // -------------------------------------------------------------------------

    /**
     * 3-window median filter: replaces each window class with the mode of
     * itself and its two neighbours.  Boundary windows use the nearest
     * available neighbour repeated (edge-padding).
     */
    private static Activity[] medianFilter3(Activity[] in) {
        int n = in.length;
        Activity[] out = new Activity[n];
        for (int i = 0; i < n; i++) {
            Activity left   = in[Math.max(0, i - 1)];
            Activity centre = in[i];
            Activity right  = in[Math.min(n - 1, i + 1)];
            out[i] = mode3(left, centre, right);
        }
        return out;
    }

    /** Returns the most frequent of three values; ties broken by centre. */
    private static Activity mode3(Activity a, Activity b, Activity c) {
        if (a == b || a == c) return a;
        if (b == c) return b;
        return b; // no majority: keep centre
    }

    /**
     * Minimum-duration filter: any run of class shorter than {@code minWindows}
     * is replaced by the preceding class (or UNKNOWN at the start).
     */
    private static Activity[] minDurationFilter(Activity[] in, int minWindows) {
        int n = in.length;
        Activity[] out = Arrays.copyOf(in, n);

        int runStart = 0;
        while (runStart < n) {
            Activity cls = out[runStart];
            int runEnd = runStart + 1;
            while (runEnd < n && out[runEnd] == cls) runEnd++;

            int runLen = runEnd - runStart;
            if (runLen < minWindows) {
                Activity replacement = (runStart > 0) ? out[runStart - 1] : Activity.UNKNOWN;
                Arrays.fill(out, runStart, runEnd, replacement);
            }
            runStart = runEnd;
        }
        return out;
    }

    // -------------------------------------------------------------------------
    // Epoch aggregation
    // -------------------------------------------------------------------------

    private static List<HarResult> aggregateEpochs(Activity[] finalClass,
                                                    double[] rmsPerWin,
                                                    double[] cadencePerWin,
                                                    long[] tsUs,
                                                    List<DataBlock.BBlock> bBlocks,
                                                    int nSamples) {
        List<HarResult> results = new ArrayList<>();

        // Windows per epoch based on 1-second hop, 30-second epoch
        // Windows at positions w cover samples [w*100 .. w*100+200)
        // Epoch i covers sample indices [i*3000 .. (i+1)*3000)
        int totalEpochs = (int) Math.ceil((double) nSamples / EPOCH_SAMPLES);

        long firstTs = tsUs[0];

        for (int e = 0; e < totalEpochs; e++) {
            int sampleStart = e * EPOCH_SAMPLES;
            int sampleEnd   = Math.min(sampleStart + EPOCH_SAMPLES, nSamples);

            // Windows whose start sample falls within this epoch
            int wLo = sampleStart / WIN_HOP;
            int wHi = Math.min((sampleEnd - WIN_SAMPLES) / WIN_HOP + 1, finalClass.length);
            wLo = Math.min(wLo, finalClass.length);
            if (wLo >= wHi) continue;

            // Dominant class (mode over epoch windows)
            int[] counts = new int[Activity.values().length];
            double rmsSum = 0.0;
            int rmsCount  = 0;
            double cadenceSum   = 0.0;
            int    cadenceCount = 0;

            for (int w = wLo; w < wHi; w++) {
                counts[finalClass[w].ordinal()]++;
                rmsSum += rmsPerWin[w];
                rmsCount++;
                if (!Double.isNaN(cadencePerWin[w])) {
                    cadenceSum += cadencePerWin[w];
                    cadenceCount++;
                }
            }

            Activity dominant = Activity.UNKNOWN;
            int maxCount = 0;
            for (Activity a : Activity.values()) {
                int c = counts[a.ordinal()];
                if (c > maxCount) {
                    maxCount = c;
                    dominant = a;
                }
            }

            double meanRms = (rmsCount > 0) ? rmsSum / rmsCount : 1.0;
            float motionIntensity = (float) Math.max(0.0, Math.min(1.0, (meanRms - 1.0) / 2.0));
            float cadenceSpm = (cadenceCount > 0) ? (float) (cadenceSum / cadenceCount) : Float.NaN;

            long epochStartMs = (tsUs[sampleStart] - firstTs) / 1000L;
            long epochEndMs   = (tsUs[Math.min(sampleEnd - 1, nSamples - 1)] - firstTs) / 1000L;

            // Barometric altitude change over epoch
            float altChangeM = Float.NaN;
            if (!bBlocks.isEmpty()) {
                double pStart = interpolatePressure(bBlocks, tsUs[sampleStart]);
                double pEnd   = interpolatePressure(bBlocks, tsUs[Math.min(sampleEnd - 1, nSamples - 1)]);
                if (!Double.isNaN(pStart) && !Double.isNaN(pEnd)) {
                    altChangeM = (float) (-(pEnd - pStart) / 12.0);
                }
            }

            results.add(new HarResult(epochStartMs, epochEndMs, dominant,
                                      cadenceSpm, motionIntensity, altChangeM));
        }

        return results;
    }

    // -------------------------------------------------------------------------
    // Barometric interpolation
    // -------------------------------------------------------------------------

    /**
     * Linearly interpolates the barometric pressure from the B-block list at
     * the requested timestamp.  Returns {@code NaN} if the timestamp is outside
     * the covered range.
     */
    private static double interpolatePressure(List<DataBlock.BBlock> bBlocks, long queryUs) {
        if (bBlocks.isEmpty()) return Double.NaN;

        int lo = 0, hi = bBlocks.size() - 1;

        // Clamp check
        if (queryUs <= bBlocks.get(lo).timestampUs()) return bBlocks.get(lo).baroPressurePa();
        if (queryUs >= bBlocks.get(hi).timestampUs()) return bBlocks.get(hi).baroPressurePa();

        // Binary search for surrounding blocks
        while (hi - lo > 1) {
            int mid = (lo + hi) >>> 1;
            if (bBlocks.get(mid).timestampUs() <= queryUs) lo = mid;
            else hi = mid;
        }

        DataBlock.BBlock bLo = bBlocks.get(lo);
        DataBlock.BBlock bHi = bBlocks.get(hi);
        double span = bHi.timestampUs() - bLo.timestampUs();
        if (span == 0.0) return bLo.baroPressurePa();

        double t = (queryUs - bLo.timestampUs()) / span;
        return bLo.baroPressurePa() + t * (bHi.baroPressurePa() - bLo.baroPressurePa());
    }

    // -------------------------------------------------------------------------
    // Statistical helpers
    // -------------------------------------------------------------------------

    private static double mean(double[] x) {
        double s = 0.0;
        for (double v : x) s += v;
        return s / x.length;
    }

    private static double rms(double[] x) {
        double s = 0.0;
        for (double v : x) s += v * v;
        return Math.sqrt(s / x.length);
    }
}
