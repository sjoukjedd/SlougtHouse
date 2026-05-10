package nl.vuams.vudam.analysis;

import org.apache.commons.math3.stat.descriptive.rank.Percentile;

import java.util.ArrayList;
import java.util.List;

/**
 * Pan-Tompkins-inspired R-peak detector for single-lead ECG signals.
 *
 * <p>Pipeline:
 * <ol>
 *   <li>Band-pass filter (5–15 Hz) via cascaded Butterworth stages</li>
 *   <li>Derivative</li>
 *   <li>Squaring</li>
 *   <li>Moving-window integration (150 ms window)</li>
 *   <li>Adaptive threshold with refractory period (200 ms)</li>
 * </ol>
 *
 * <p>This implementation is optimised for clarity and correctness over
 * streaming performance; it processes the entire signal in one pass.
 */
public final class RPeakDetector {

    private RPeakDetector() {}

    /**
     * Detect R-peaks in a raw ECG signal.
     *
     * @param ecg        raw ECG samples (ADC integer counts or float mV — units do not matter)
     * @param sampleRate sampling rate in Hz (nominally 1000 Hz for ADS1256 acquisition)
     * @return list of sample indices where R-peaks occur
     */
    public static List<Integer> detect(double[] ecg, double sampleRate) {
        double[] filtered    = bandpass(ecg, sampleRate);
        double[] derivative  = derivative(filtered);
        double[] squared     = square(derivative);
        double[] integrated  = movingWindowIntegrate(squared, sampleRate);
        return threshold(integrated, sampleRate);
    }

    // -------------------------------------------------------------------------
    // Step 1 — Band-pass 5–15 Hz (difference-equation IIR approximation)
    // -------------------------------------------------------------------------

    /**
     * Simple IIR band-pass 5–15 Hz via a cascade of a high-pass and low-pass
     * first-order difference equations. Not a precise Butterworth, but
     * sufficient for QRS energy extraction at 1 kHz.
     */
    private static double[] bandpass(double[] x, double sampleRate) {
        // High-pass (cut ~5 Hz): y[n] = x[n] – x[n-1] + (1 – 2π·fc/fs)·y[n-1]
        double hpAlpha = 1.0 - 2.0 * Math.PI * 5.0 / sampleRate;
        double[] hp = new double[x.length];
        for (int i = 1; i < x.length; i++) {
            hp[i] = x[i] - x[i - 1] + hpAlpha * hp[i - 1];
        }

        // Low-pass (cut ~15 Hz): y[n] = (1 – alpha)·x[n] + alpha·y[n-1]
        double lpAlpha = Math.exp(-2.0 * Math.PI * 15.0 / sampleRate);
        double[] lp = new double[hp.length];
        for (int i = 1; i < hp.length; i++) {
            lp[i] = (1.0 - lpAlpha) * hp[i] + lpAlpha * lp[i - 1];
        }
        return lp;
    }

    // -------------------------------------------------------------------------
    // Step 2 — 5-point derivative to emphasise QRS slope
    // -------------------------------------------------------------------------

    private static double[] derivative(double[] x) {
        double[] d = new double[x.length];
        for (int i = 2; i < x.length - 2; i++) {
            d[i] = (2 * x[i + 2] + x[i + 1] - x[i - 1] - 2 * x[i - 2]) / 8.0;
        }
        return d;
    }

    // -------------------------------------------------------------------------
    // Step 3 — Point-wise squaring (non-linear amplification)
    // -------------------------------------------------------------------------

    private static double[] square(double[] x) {
        double[] s = new double[x.length];
        for (int i = 0; i < x.length; i++) s[i] = x[i] * x[i];
        return s;
    }

    // -------------------------------------------------------------------------
    // Step 4 — Moving-window integration
    // -------------------------------------------------------------------------

    private static double[] movingWindowIntegrate(double[] x, double sampleRate) {
        int windowSamples = (int) Math.round(0.150 * sampleRate); // 150 ms
        double[] mwi = new double[x.length];
        double sum = 0.0;
        for (int i = 0; i < x.length; i++) {
            sum += x[i];
            if (i >= windowSamples) sum -= x[i - windowSamples];
            mwi[i] = sum / windowSamples;
        }
        return mwi;
    }

    // -------------------------------------------------------------------------
    // Step 5 — Adaptive threshold with refractory period
    // -------------------------------------------------------------------------

    private static List<Integer> threshold(double[] mwi, double sampleRate) {
        int refractory = (int) Math.round(0.200 * sampleRate); // 200 ms

        // Initial threshold: 50th percentile of the first 2 s of signal
        int calibSamples = Math.min(mwi.length, (int) (2.0 * sampleRate));
        double[] calib = new double[calibSamples];
        System.arraycopy(mwi, 0, calib, 0, calibSamples);
        double threshold = new Percentile(50).evaluate(calib);

        List<Integer> peaks = new ArrayList<>();
        int lastPeak = -refractory;

        for (int i = 1; i < mwi.length - 1; i++) {
            if (mwi[i] > threshold
                    && mwi[i] > mwi[i - 1]
                    && mwi[i] >= mwi[i + 1]
                    && (i - lastPeak) >= refractory) {

                peaks.add(i);
                lastPeak = i;
                // Adaptive: nudge threshold toward current peak amplitude
                threshold = 0.875 * threshold + 0.125 * mwi[i];
            }
        }
        return peaks;
    }
}
