package nl.vuams.vudam.analysis;

import java.util.ArrayList;
import java.util.List;

/**
 * Composite Stress Index (CSI-4) analyser.
 *
 * <p>Operates on 30-second epochs assembled by the caller. The first
 * {@value #BASELINE_EPOCHS} valid epochs (120 s) are consumed as baseline;
 * {@link #addEpoch} returns {@code null} during that phase.
 *
 * <h2>Markers</h2>
 * <ol>
 *   <li>PEP⁻¹ = 1 / pepMs  — sympathetic cardiac (IBlock)</li>
 *   <li>SCL (µS tonic EDA) — sympathetic electrodermal (SBlock)</li>
 *   <li>RMSSD⁻¹ = 1 / rmssdMs — vagal cardiac (YBlock)</li>
 *   <li>SCR rate (events/min) — phasic electrodermal (SBlock)</li>
 * </ol>
 *
 * <h2>Algorithm</h2>
 * <pre>
 *   z_i   = clamp((x_i − µ_i) / σ_i, −3, 3)
 *   raw   = Σ(w_i × z_i)   w = [0.35, 0.30, 0.25, 0.10]
 *   CSI   = 100 / (1 + exp(−1.5 × raw))
 * </pre>
 */
public final class CSIAnalyser {

    // -------------------------------------------------------------------------
    // Constants
    // -------------------------------------------------------------------------

    /** Number of epochs that form the baseline (4 × 30 s = 120 s). */
    private static final int BASELINE_EPOCHS = 4;

    /** Marker weights: PEP⁻¹, SCL, RMSSD⁻¹, SCR_rate. */
    private static final double[] WEIGHTS = {0.35, 0.30, 0.25, 0.10};

    /** Logistic steepness constant. */
    private static final double LOGISTIC_K = 1.5;

    /** Z-score clamp bound. */
    private static final double Z_CLAMP = 3.0;

    /** Minimum σ to avoid division by zero (applied before clamping). */
    private static final double MIN_SIGMA = 1e-9;

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    /** Raw baseline samples, one double[] per epoch, index == marker index. */
    private final List<double[]> baselineSamples = new ArrayList<>();

    /** Baseline means, populated once baseline is complete. */
    private double[] baselineMu;

    /** Baseline standard deviations, populated once baseline is complete. */
    private double[] baselineSigma;

    // =========================================================================
    // Public API
    // =========================================================================

    /** Creates a new analyser in the initial (pre-baseline) state. */
    public CSIAnalyser() {}

    /**
     * Submit one 30-second epoch for processing.
     *
     * @param input the four marker values for this epoch
     * @return a {@link CSIResult} once baseline is complete; {@code null} while
     *         the baseline window is still being filled
     * @throws IllegalArgumentException if any marker value is non-finite or
     *         if pepMs / rmssdMs is zero (would produce infinite inverse)
     */
    public CSIResult addEpoch(EpochInput input) {
        validateInput(input);

        double[] markers = toMarkerArray(input);

        // --- Baseline accumulation ---
        if (baselineMu == null) {
            baselineSamples.add(markers);

            if (baselineSamples.size() < BASELINE_EPOCHS) {
                // Still collecting baseline
                return null;
            }

            // Enough samples: compute baseline statistics
            computeBaseline();
            // J1 fix: epoch 4 is part of the baseline, so do not score it.
            // The first score is returned for epoch 5 onwards.
            return null;
        }

        // --- Post-baseline scoring ---
        double progress = 1.0; // baseline is complete
        return score(markers, progress);
    }

    /**
     * Reset the analyser to its initial state, discarding all baseline and
     * accumulated data.
     */
    public void reset() {
        baselineSamples.clear();
        baselineMu    = null;
        baselineSigma = null;
    }

    // =========================================================================
    // Internal helpers
    // =========================================================================

    /**
     * Convert an {@link EpochInput} into the four-element marker array that
     * the scoring logic works with.
     *
     * <p>Order: [PEP⁻¹, SCL, RMSSD⁻¹, SCR_rate]
     */
    private static double[] toMarkerArray(EpochInput in) {
        return new double[]{
                1.0 / in.pepMs,    // PEP⁻¹ — sympathetic cardiac
                in.sclUs,          // SCL tonic µS
                1.0 / in.rmssdMs,  // RMSSD⁻¹ — vagal cardiac
                in.scrRate         // SCR events/min
        };
    }

    /** Compute µ and σ per marker from the accumulated baseline epochs. */
    private void computeBaseline() {
        int n      = baselineSamples.size();
        int nMarks = WEIGHTS.length;

        baselineMu    = new double[nMarks];
        baselineSigma = new double[nMarks];

        // Mean
        for (double[] epoch : baselineSamples) {
            for (int m = 0; m < nMarks; m++) {
                baselineMu[m] += epoch[m];
            }
        }
        for (int m = 0; m < nMarks; m++) {
            baselineMu[m] /= n;
        }

        // Sample standard deviation (Bessel-corrected, divide by n-1)
        for (double[] epoch : baselineSamples) {
            for (int m = 0; m < nMarks; m++) {
                double diff = epoch[m] - baselineMu[m];
                baselineSigma[m] += diff * diff;
            }
        }
        for (int m = 0; m < nMarks; m++) {
            baselineSigma[m] = Math.sqrt(baselineSigma[m] / (n - 1));
            if (baselineSigma[m] < MIN_SIGMA) {
                baselineSigma[m] = MIN_SIGMA;
            }
        }
    }

    /**
     * Compute z-scores, weighted sum, and logistic CSI score for one epoch.
     *
     * @param markers         four-element marker array for this epoch
     * @param baselineProgress always 1.0 when called (baseline is complete)
     */
    private CSIResult score(double[] markers, double baselineProgress) {
        double[] z = new double[WEIGHTS.length];
        double weightedSum = 0.0;

        for (int m = 0; m < WEIGHTS.length; m++) {
            double raw = (markers[m] - baselineMu[m]) / baselineSigma[m];
            z[m]        = clamp(raw, -Z_CLAMP, Z_CLAMP);
            weightedSum += WEIGHTS[m] * z[m];
        }

        double csi = 100.0 / (1.0 + Math.exp(-LOGISTIC_K * weightedSum));

        return new CSIResult(
                csi,
                z[0], z[1], z[2], z[3],
                baselineProgress,
                true
        );
    }

    /** Clamp {@code v} to [min, max]. */
    private static double clamp(double v, double min, double max) {
        return Math.min(max, Math.max(min, v));
    }

    /**
     * Validate that all inputs are finite and that the inverse-marker
     * denominators are non-zero.
     */
    private static void validateInput(EpochInput in) {
        if (!Double.isFinite(in.pepMs) || in.pepMs <= 0.0) {
            throw new IllegalArgumentException(
                    "pepMs must be finite and > 0; got " + in.pepMs);
        }
        if (!Double.isFinite(in.sclUs)) {
            throw new IllegalArgumentException(
                    "sclUs must be finite; got " + in.sclUs);
        }
        if (!Double.isFinite(in.rmssdMs) || in.rmssdMs <= 0.0) {
            throw new IllegalArgumentException(
                    "rmssdMs must be finite and > 0; got " + in.rmssdMs);
        }
        if (!Double.isFinite(in.scrRate) || in.scrRate < 0.0) {
            throw new IllegalArgumentException(
                    "scrRate must be finite and >= 0; got " + in.scrRate);
        }
    }

    // =========================================================================
    // Nested types
    // =========================================================================

    /**
     * Input bundle for a single 30-second epoch.
     */
    public static final class EpochInput {

        /** Pre-ejection period [ms] from IBlock. Must be &gt; 0. */
        public final double pepMs;

        /** Tonic skin conductance level [µS] from SBlock. */
        public final double sclUs;

        /** Root mean square of successive RR differences [ms] from YBlock. Must be &gt; 0. */
        public final double rmssdMs;

        /** Skin conductance response rate [events/min] from SBlock. */
        public final double scrRate;

        /**
         * @param pepMs    PEP [ms] — must be &gt; 0
         * @param sclUs    SCL tonic [µS]
         * @param rmssdMs  RMSSD [ms] — must be &gt; 0
         * @param scrRate  SCR rate [events/min]
         */
        public EpochInput(double pepMs, double sclUs, double rmssdMs, double scrRate) {
            this.pepMs   = pepMs;
            this.sclUs   = sclUs;
            this.rmssdMs = rmssdMs;
            this.scrRate = scrRate;
        }
    }

    /**
     * Result produced for each post-baseline epoch.
     *
     * <p>All z-scores are clamped to [−3, 3].
     */
    public static final class CSIResult {

        /** Composite Stress Index in the range [0, 100]. Higher = more stress. */
        public final double score;

        /** Z-score for PEP⁻¹ (sympathetic cardiac). */
        public final double pepZ;

        /** Z-score for SCL (sympathetic electrodermal). */
        public final double sclZ;

        /** Z-score for RMSSD⁻¹ (vagal cardiac). */
        public final double rmssdZ;

        /** Z-score for SCR rate (phasic electrodermal). */
        public final double scrZ;

        /**
         * Fraction of baseline collection complete [0.0–1.0].
         * Always 1.0 for a returned result (baseline is finished by definition).
         */
        public final double baselineProgress;

        /** {@code true} once the baseline window has been fully populated. */
        public final boolean baselineComplete;

        CSIResult(
                double score,
                double pepZ,
                double sclZ,
                double rmssdZ,
                double scrZ,
                double baselineProgress,
                boolean baselineComplete
        ) {
            this.score            = score;
            this.pepZ             = pepZ;
            this.sclZ             = sclZ;
            this.rmssdZ           = rmssdZ;
            this.scrZ             = scrZ;
            this.baselineProgress = baselineProgress;
            this.baselineComplete = baselineComplete;
        }
    }
}
