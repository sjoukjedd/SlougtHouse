package nl.vuams.vudam.analysis;

import nl.vuams.vudam.model.DataBlock;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;

/**
 * Speech Activity Detection (SAD) from V-block high-ODR accelerometer data (SP-002 §1).
 *
 * <p>Input: List of {@link DataBlock.VBlock} (1 kHz chest accelerometer, 100 samples/block).
 *
 * <p>Processing pipeline per V-block (100 ms window):
 * <ol>
 *   <li>Compute acceleration magnitude: sqrt(ax² + ay² + az²), scaled to g.</li>
 *   <li>Apply a 4th-order Butterworth HPF at 80 Hz / Fs = 1000 Hz, implemented as two
 *       cascaded 2nd-order biquad sections (Direct Form II), maintaining filter state
 *       across block boundaries.</li>
 *   <li>Frame the filtered signal into 20-sample frames with 10-sample hop (STE frames).
 *       Per block: (100 − 20) / 10 + 1 = 9 complete frames, but we carry samples over
 *       block boundaries so frames align across blocks.</li>
 *   <li>Compute Short-Time Energy (STE) per frame: sum of squared filtered samples.</li>
 *   <li>Adaptive threshold: 1000-frame circular buffer of STE values; threshold = mean + 1.5σ.
 *       During the warm-up period (< 1000 frames available), use all available frames.</li>
 *   <li>Binary speech decision: 3+ consecutive frames above threshold.</li>
 *   <li>Post-processing: gap fill (< 20 frames) and minimum segment duration (≥ 30 frames).</li>
 *   <li>Aggregate to 30-second epochs.</li>
 * </ol>
 *
 * <h3>Biquad HPF coefficients (80 Hz, Fs = 1000 Hz, 2nd-order section)</h3>
 * <pre>
 *   b0 = 0.7265,  b1 = -1.4530,  b2 = 0.7265
 *   a1 = -1.3960, a2 = 0.4918
 * </pre>
 * Two identical sections are cascaded to achieve 4th-order response.
 *
 * <h3>ODR requirement</h3>
 * If no V-blocks are present, {@link SadResult#sadAvailable()} will be {@code false}
 * and all signal fields will be {@link Float#NaN}.
 *
 * <p>Thread safety: all mutable state (filter state, circular buffer) is held inside
 * {@link #analyse(List)} as local variables — the class itself has no instance state
 * and the method may be called concurrently.
 */
public final class SadAnalyser {

    // -------------------------------------------------------------------------
    // Constants — scale factor
    // -------------------------------------------------------------------------

    /** ICM-20948 ±2g, 1 LSB = 1/16384 g. */
    private static final double ACC_SCALE_G = 1.0 / 16384.0;

    // -------------------------------------------------------------------------
    // SAD tuning parameters
    // -------------------------------------------------------------------------

    private static final int    FS              = 1000;   // Hz
    private static final int    FRAME_LEN       = 20;     // samples (20 ms)
    private static final int    FRAME_HOP       = 10;     // samples (10 ms)
    private static final int    MIN_CONSECUTIVE = 3;      // frames to confirm speech
    private static final int    ADAPTIVE_WIN    = 1000;   // frames (10 s)
    private static final double THRESHOLD_MULT  = 1.5;    // mean + 1.5 * std
    private static final int    GAP_FILL_FRAMES = 20;     // < 20 silence frames → fill
    private static final int    MIN_SEG_FRAMES  = 30;     // discard segments < 30 frames
    private static final long   EPOCH_MS        = 30_000L; // 30-second epoch

    // -------------------------------------------------------------------------
    // Biquad coefficients — 80 Hz HPF, Fs = 1000 Hz, 2nd-order section
    // Two identical sections cascaded → 4th-order Butterworth HPF
    // -------------------------------------------------------------------------

    private static final double B0 =  0.7265;
    private static final double B1 = -1.4530;
    private static final double B2 =  0.7265;
    private static final double A1 = -1.3960;
    private static final double A2 =  0.4918;

    // -------------------------------------------------------------------------
    // Output record
    // -------------------------------------------------------------------------

    /**
     * Per-30-second-epoch SAD result (SP-002 §1.7).
     *
     * @param epochStartMs         epoch start time from recording start [ms]
     * @param epochEndMs           epoch end time from recording start [ms]
     * @param speakingFraction     fraction of epoch classified as speech (0–1); NaN if not available
     * @param segmentCount         number of distinct speech segments in epoch; -1 if not available
     * @param meanDurationMs       mean speech segment duration [ms]; NaN if no speech or not available
     * @param steBaselineMean      mean STE over epoch [g²]; NaN if not available
     * @param steBaselineStd       std of STE over epoch [g²]; NaN if not available
     * @param ambientNoiseSuspected true if steBaselineStd / steBaselineMean > 0.8
     * @param sadAvailable         false if no V-blocks were present; all NaN fields in that case
     */
    public record SadResult(
            long  epochStartMs,
            long  epochEndMs,
            float speakingFraction,
            int   segmentCount,
            float meanDurationMs,
            float steBaselineMean,
            float steBaselineStd,
            boolean ambientNoiseSuspected,
            boolean sadAvailable
    ) {
        /** Sentinel for a 30-second epoch when SAD data is unavailable. */
        public static SadResult unavailable(long startMs, long endMs) {
            return new SadResult(startMs, endMs,
                    Float.NaN, -1, Float.NaN, Float.NaN, Float.NaN,
                    false, false);
        }
    }

    // -------------------------------------------------------------------------
    // Public API
    // -------------------------------------------------------------------------

    /**
     * Analyses all V-blocks and returns per-epoch SAD results.
     *
     * <p>If {@code vBlocks} is empty, returns a single epoch-spanning result with
     * {@code sadAvailable = false}.
     *
     * @param vBlocks all V-blocks from the recording, in timestamp order
     * @return list of {@link SadResult} records, one per 30-second epoch
     */
    public static List<SadResult> analyse(List<DataBlock.VBlock> vBlocks) {
        if (vBlocks.isEmpty()) {
            // No high-ODR data available — return a single unavailable sentinel
            return List.of(SadResult.unavailable(0L, 0L));
        }

        long firstUs = vBlocks.get(0).timestampUs();

        // ---- Biquad filter state (two cascaded sections) ----
        // Each section: Direct Form II, two delay elements
        double s1w1 = 0.0, s1w2 = 0.0;  // Section 1 state
        double s2w1 = 0.0, s2w2 = 0.0;  // Section 2 state

        // ---- Frame assembly buffer ----
        // We accumulate filtered samples and emit frames as they complete.
        double[] frameBuf = new double[FRAME_LEN * 2]; // generous scratch space
        int frameBufCount = 0;

        // ---- Adaptive threshold circular buffer ----
        double[] steCircBuf = new double[ADAPTIVE_WIN];
        int      circBufHead = 0;
        int      circBufFill = 0;

        // ---- Per-frame accumulators ----
        List<Double>  steValues        = new ArrayList<>();
        List<Boolean> thresholdedFrames = new ArrayList<>();
        List<Long>    frameTsUsList    = new ArrayList<>();

        // Build a contiguous filtered sample stream (with per-sample µs timestamps)
        // Process block by block; carry leftover samples into next block for proper framing
        int carryCount  = 0;
        double[] carrySamples = new double[FRAME_LEN]; // at most FRAME_LEN - 1 carry
        long[]   carryTsUs   = new long[FRAME_LEN];

        for (DataBlock.VBlock vb : vBlocks) {
            long blockTs = vb.timestampUs();
            long stepUs  = 1000L;  // 1 ms = 1000 Hz

            int nNew = 100;
            double[] filtered = new double[carryCount + nNew];
            long[]   tUs      = new long[carryCount + nNew];

            // Copy carry
            System.arraycopy(carrySamples, 0, filtered, 0, carryCount);
            System.arraycopy(carryTsUs,    0, tUs,      0, carryCount);

            // Process new samples through biquad filter
            for (int i = 0; i < nNew; i++) {
                double rawG = (vb.ax()[i] * vb.ax()[i]
                             + vb.ay()[i] * vb.ay()[i]
                             + vb.az()[i] * vb.az()[i]);
                // magnitude in g
                double mag = Math.sqrt(rawG) * ACC_SCALE_G;

                // Biquad section 1 (Direct Form II)
                double w1 = mag  - A1 * s1w1 - A2 * s1w2;
                double y1 = B0 * w1 + B1 * s1w1 + B2 * s1w2;
                s1w2 = s1w1;
                s1w1 = w1;

                // Biquad section 2
                double w2 = y1  - A1 * s2w1 - A2 * s2w2;
                double y2 = B0 * w2 + B1 * s2w1 + B2 * s2w2;
                s2w2 = s2w1;
                s2w1 = w2;

                filtered[carryCount + i] = y2;
                tUs[carryCount + i]      = blockTs + i * stepUs;
            }

            int total = carryCount + nNew;

            // Extract frames with hop
            int pos = 0;
            while (pos + FRAME_LEN <= total) {
                // Compute STE for this frame
                double ste = 0.0;
                for (int j = 0; j < FRAME_LEN; j++) {
                    double v = filtered[pos + j];
                    ste += v * v;
                }

                long frameCentreUs = tUs[pos + FRAME_LEN / 2];
                frameTsUsList.add(frameCentreUs);
                steValues.add(ste);

                // Update circular buffer
                steCircBuf[circBufHead % ADAPTIVE_WIN] = ste;
                circBufHead++;
                if (circBufFill < ADAPTIVE_WIN) circBufFill++;

                // Compute adaptive threshold over available history
                int nHist = Math.min(circBufFill, ADAPTIVE_WIN);
                double mu = 0.0, sigma = 0.0;
                for (int k = 0; k < nHist; k++) {
                    mu += steCircBuf[k];
                }
                mu /= nHist;
                for (int k = 0; k < nHist; k++) {
                    double d = steCircBuf[k] - mu;
                    sigma += d * d;
                }
                sigma = Math.sqrt(sigma / nHist);

                double threshold = mu + THRESHOLD_MULT * sigma;
                thresholdedFrames.add(ste > threshold);

                pos += FRAME_HOP;
            }

            // Carry remaining samples for next block
            carryCount = total - pos;
            System.arraycopy(filtered, pos, carrySamples, 0, carryCount);
            System.arraycopy(tUs,      pos, carryTsUs,    0, carryCount);
        }

        int nFrames = thresholdedFrames.size();
        if (nFrames == 0) {
            return List.of(SadResult.unavailable(0L, 0L));
        }

        // ---- Apply 3-consecutive-frames minimum run rule ----
        boolean[] decision = new boolean[nFrames];
        for (int m = MIN_CONSECUTIVE - 1; m < nFrames; m++) {
            boolean allAbove = true;
            for (int k = 0; k < MIN_CONSECUTIVE; k++) {
                if (!thresholdedFrames.get(m - k)) { allAbove = false; break; }
            }
            if (allAbove) {
                // Mark all three frames as speech
                for (int k = 0; k < MIN_CONSECUTIVE; k++) decision[m - k] = true;
            }
        }

        // ---- Post-processing Rule 1: gap fill (< 20 frames → fill) ----
        boolean[] afterGapFill = gapFill(decision, GAP_FILL_FRAMES);

        // ---- Post-processing Rule 2: discard segments < 30 frames ----
        boolean[] afterMinDuration = minDuration(afterGapFill, MIN_SEG_FRAMES);

        // ---- Aggregate to 30-second epochs ----
        return buildEpochResults(afterMinDuration, frameTsUsList, steValues, firstUs);
    }

    // -------------------------------------------------------------------------
    // Post-processing
    // -------------------------------------------------------------------------

    /** Fills silence gaps shorter than {@code maxGap} frames between speech segments. */
    private static boolean[] gapFill(boolean[] d, int maxGap) {
        boolean[] out = d.clone();
        int n = out.length;
        int silStart = -1;

        for (int i = 0; i <= n; i++) {
            boolean isSpeech = (i < n) && out[i];
            if (!isSpeech && silStart < 0) {
                silStart = i;
            } else if (isSpeech && silStart >= 0) {
                int gapLen = i - silStart;
                // Only fill if gap is flanked by speech on both sides
                if (gapLen < maxGap && silStart > 0) {
                    for (int j = silStart; j < i; j++) out[j] = true;
                }
                silStart = -1;
            } else if (isSpeech) {
                silStart = -1;
            }
        }
        return out;
    }

    /** Discards speech segments shorter than {@code minLen} frames. */
    private static boolean[] minDuration(boolean[] d, int minLen) {
        boolean[] out = d.clone();
        int n = out.length;
        int segStart = -1;

        for (int i = 0; i <= n; i++) {
            boolean isSpeech = (i < n) && out[i];
            if (isSpeech && segStart < 0) {
                segStart = i;
            } else if (!isSpeech && segStart >= 0) {
                int segLen = i - segStart;
                if (segLen < minLen) {
                    for (int j = segStart; j < i; j++) out[j] = false;
                }
                segStart = -1;
            }
        }
        return out;
    }

    // -------------------------------------------------------------------------
    // Epoch aggregation
    // -------------------------------------------------------------------------

    private static List<SadResult> buildEpochResults(boolean[] speech,
                                                      List<Long> frameTsUs,
                                                      List<Double> steValues,
                                                      long recordingStartUs) {
        int nFrames = speech.length;
        List<SadResult> results = new ArrayList<>();

        long firstFrameUs = frameTsUs.get(0);
        long epochStartUs = firstFrameUs;
        long epochEndUs   = epochStartUs + EPOCH_MS * 1000L;

        // Walk through frames epoch by epoch
        int frameIdx = 0;
        while (frameIdx < nFrames) {
            // Collect frames belonging to this epoch
            List<Boolean> epochSpeech = new ArrayList<>();
            List<Double>  epochSte    = new ArrayList<>();

            while (frameIdx < nFrames && frameTsUs.get(frameIdx) < epochEndUs) {
                epochSpeech.add(speech[frameIdx]);
                epochSte.add(steValues.get(frameIdx));
                frameIdx++;
            }

            if (epochSpeech.isEmpty()) {
                epochStartUs = epochEndUs;
                epochEndUs   = epochStartUs + EPOCH_MS * 1000L;
                continue;
            }

            long epochStartMs = (epochStartUs - recordingStartUs) / 1000L;
            long epochEndMs   = epochStartMs + EPOCH_MS;

            // Speaking fraction
            long speakingFrames = epochSpeech.stream().filter(b -> b).count();
            float speakingFraction = (float) ((speakingFrames * FRAME_HOP) / (double) EPOCH_MS);
            speakingFraction = Math.min(1.0f, speakingFraction);

            // Segment count and mean duration
            int segCount = 0;
            long segTotalFrames = 0;
            boolean inSeg = false;
            long segStart = 0;
            for (int i = 0; i <= epochSpeech.size(); i++) {
                boolean s = (i < epochSpeech.size()) && epochSpeech.get(i);
                if (s && !inSeg) {
                    inSeg = true;
                    segStart = i;
                } else if (!s && inSeg) {
                    inSeg = false;
                    segCount++;
                    segTotalFrames += i - segStart;
                }
            }
            float meanDurMs = (segCount > 0)
                    ? (float) ((segTotalFrames * FRAME_HOP) / (double) segCount)
                    : Float.NaN;

            // STE baseline statistics
            double steSum = 0.0;
            for (double v : epochSte) steSum += v;
            double steMean = steSum / epochSte.size();
            double steVar  = 0.0;
            for (double v : epochSte) { double d = v - steMean; steVar += d * d; }
            double steStd = Math.sqrt(steVar / epochSte.size());

            boolean ambientNoise = (steMean > 0.0) && (steStd / steMean > 0.8);

            results.add(new SadResult(
                    epochStartMs,
                    epochEndMs,
                    speakingFraction,
                    segCount,
                    meanDurMs,
                    (float) steMean,
                    (float) steStd,
                    ambientNoise,
                    true
            ));

            epochStartUs = epochEndUs;
            epochEndUs   = epochStartUs + EPOCH_MS * 1000L;
        }

        return results;
    }
}
