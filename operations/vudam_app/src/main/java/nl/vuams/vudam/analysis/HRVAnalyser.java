package nl.vuams.vudam.analysis;

import org.apache.commons.math3.complex.Complex;
import org.apache.commons.math3.transform.DftNormalization;
import org.apache.commons.math3.transform.FastFourierTransformer;
import org.apache.commons.math3.transform.TransformType;

import java.util.List;

/**
 * Computes time-domain and frequency-domain HRV metrics from a list of
 * R-R intervals.
 *
 * <h2>Time-domain</h2>
 * <ul>
 *   <li>Mean RRI</li>
 *   <li>SDNN — standard deviation of all normal RR intervals</li>
 *   <li>RMSSD — root mean square of successive differences</li>
 *   <li>pNN50 — proportion of successive differences &gt; 50 ms</li>
 * </ul>
 *
 * <h2>Frequency-domain</h2>
 * Welch-style single-window periodogram on the uniformly resampled RRI series.
 * <ul>
 *   <li>LF power (0.04–0.15 Hz)</li>
 *   <li>HF power (0.15–0.40 Hz)</li>
 *   <li>LF/HF ratio</li>
 * </ul>
 */
public final class HRVAnalyser {

    /** Target resample rate for frequency-domain analysis [Hz]. */
    private static final double RESAMPLE_HZ = 4.0;

    private HRVAnalyser() {}

    /**
     * Compute all HRV metrics from a list of RR intervals.
     *
     * @param rriMs RR intervals in milliseconds, sequential, from RPeakDetector
     * @return a {@link Metrics} record containing all computed values
     * @throws IllegalArgumentException if fewer than 4 intervals are supplied
     */
    public static Metrics compute(List<Double> rriMs) {
        if (rriMs.size() < 4) {
            throw new IllegalArgumentException(
                    "Need at least 4 RR intervals; got " + rriMs.size());
        }

        double[] rri = rriMs.stream().mapToDouble(Double::doubleValue).toArray();

        // --- Time domain ---
        double mean  = mean(rri);
        double sdnn  = sdnn(rri, mean);
        double rmssd = rmssd(rri);
        double pnn50 = pnn50(rri);

        // --- Frequency domain ---
        double[] resampled = resample(rri, RESAMPLE_HZ);
        double[] padded    = zeroPadToPow2(resampled);
        double[] psd       = powerSpectralDensity(padded, RESAMPLE_HZ);

        double freqResHz = RESAMPLE_HZ / padded.length;
        double lfPower   = bandPower(psd, freqResHz, 0.04, 0.15);
        double hfPower   = bandPower(psd, freqResHz, 0.15, 0.40);
        double lfHf      = hfPower > 0 ? lfPower / hfPower : Double.NaN;

        return new Metrics(mean, sdnn, rmssd, pnn50, lfPower, hfPower, lfHf);
    }

    // -------------------------------------------------------------------------
    // Time-domain helpers
    // -------------------------------------------------------------------------

    private static double mean(double[] x) {
        double s = 0;
        for (double v : x) s += v;
        return s / x.length;
    }

    private static double sdnn(double[] x, double mean) {
        double sumSq = 0;
        for (double v : x) {
            double d = v - mean;
            sumSq += d * d;
        }
        return Math.sqrt(sumSq / x.length);
    }

    private static double rmssd(double[] x) {
        double sumSq = 0;
        for (int i = 1; i < x.length; i++) {
            double d = x[i] - x[i - 1];
            sumSq += d * d;
        }
        return Math.sqrt(sumSq / (x.length - 1));
    }

    private static double pnn50(double[] x) {
        int count = 0;
        for (int i = 1; i < x.length; i++) {
            if (Math.abs(x[i] - x[i - 1]) > 50.0) count++;
        }
        return 100.0 * count / (x.length - 1);
    }

    // -------------------------------------------------------------------------
    // Frequency-domain helpers
    // -------------------------------------------------------------------------

    /**
     * Uniformly resample the RRI series by linear interpolation.
     * Input RRI values define the instantaneous "time" axis when accumulated.
     *
     * @param rri       RR intervals [ms]
     * @param targetHz  target sample rate [Hz]
     * @return resampled tachogram at {@code targetHz}
     */
    private static double[] resample(double[] rri, double targetHz) {
        // Build cumulative time axis in seconds
        double[] tSec = new double[rri.length];
        tSec[0] = rri[0] / 1000.0;
        for (int i = 1; i < rri.length; i++) {
            tSec[i] = tSec[i - 1] + rri[i] / 1000.0;
        }

        double totalTime = tSec[tSec.length - 1];
        int nOut = (int) Math.floor(totalTime * targetHz);
        double[] out = new double[nOut];
        double dt = 1.0 / targetHz;

        int j = 0;
        for (int i = 0; i < nOut; i++) {
            double t = i * dt;
            // Advance j until tSec[j] >= t
            while (j < tSec.length - 2 && tSec[j + 1] < t) j++;
            // Linear interpolation between tSec[j] and tSec[j+1]
            double t0 = tSec[j];
            double t1 = (j + 1 < tSec.length) ? tSec[j + 1] : t0;
            double alpha = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0;
            out[i] = rri[j] + alpha * (((j + 1 < rri.length) ? rri[j + 1] : rri[j]) - rri[j]);
        }
        return out;
    }

    private static double[] zeroPadToPow2(double[] x) {
        int n = 1;
        while (n < x.length) n <<= 1;
        double[] padded = new double[n];
        System.arraycopy(x, 0, padded, 0, x.length);
        return padded;
    }

    /**
     * One-sided power spectral density via FFT (no windowing — Hanning is
     * left as a TODO for the analysis module refinement pass).
     */
    private static double[] powerSpectralDensity(double[] x, double sampleHz) {
        FastFourierTransformer fft = new FastFourierTransformer(DftNormalization.STANDARD);
        Complex[] spectrum = fft.transform(x, TransformType.FORWARD);

        int n = x.length;
        int nPos = n / 2 + 1;
        double[] psd = new double[nPos];
        double norm = 1.0 / (n * sampleHz);

        psd[0] = (spectrum[0].abs() * spectrum[0].abs()) * norm;
        for (int i = 1; i < nPos - 1; i++) {
            psd[i] = 2.0 * (spectrum[i].abs() * spectrum[i].abs()) * norm;
        }
        psd[nPos - 1] = (spectrum[nPos - 1].abs() * spectrum[nPos - 1].abs()) * norm;
        return psd;
    }

    /**
     * Integrate PSD bins within a frequency band (trapezoidal rule).
     */
    private static double bandPower(double[] psd, double freqResHz, double fLow, double fHigh) {
        double power = 0.0;
        for (int i = 0; i < psd.length; i++) {
            double f = i * freqResHz;
            if (f >= fLow && f <= fHigh) {
                power += psd[i] * freqResHz;
            }
        }
        return power;
    }

    // =========================================================================
    // Result record
    // =========================================================================

    /**
     * Computed HRV metrics.
     *
     * @param meanRri   mean RR interval [ms]
     * @param sdnn      SDNN [ms]
     * @param rmssd     RMSSD [ms]
     * @param pnn50     pNN50 [%]
     * @param lfPower   LF band power (0.04–0.15 Hz) [ms²]
     * @param hfPower   HF band power (0.15–0.40 Hz) [ms²]
     * @param lfHfRatio LF/HF ratio
     */
    public record Metrics(
            double meanRri,
            double sdnn,
            double rmssd,
            double pnn50,
            double lfPower,
            double hfPower,
            double lfHfRatio
    ) {}
}
