import Foundation

// MARK: - RPeakDetector
//
// Stateful R-peak detector operating on a 1000 Hz ECG signal.
// Feed one sample at a time; returns the RR interval (ms) when a beat is detected.
//
// Algorithm:
//   • Adaptive amplitude threshold — initialised to half the peak of the first second,
//     then updated after each detection: threshold = 0.875·old + 0.125·newPeak
//   • 250-sample blanking period after each detection (250 ms at 1000 Hz)
//   • RRI validity window: [300, 2000] ms (30–200 bpm)
//   • Last 32 valid RRIs stored in a fixed-size circular buffer
//   • rmssd computed over the last 30 valid RRIs (nil if fewer than 2)

struct RPeakDetector {

    // MARK: - Constants

    private static let blankingSamples = 250       // 250 ms at 1000 Hz
    private static let rriMin: Float   = 300        // ms (200 bpm)
    private static let rriMax: Float   = 2000       // ms (30 bpm)
    private static let rriBufferSize   = 32
    private static let rmssdWindowSize = 30         // RRIs used for RMSSD

    // MARK: - Initialisation state

    private var initBuffer: [Float] = []            // collects first 1000 samples
    private var isInitialised = false

    // MARK: - Detection state

    private var threshold: Float = 0
    private var blankingCountdown = 0               // samples remaining in blanking period
    private var samplesSinceLast = 0                // distance to last detected peak
    private var peakInWindow: Float = 0             // running max since last detection

    // MARK: - RRI circular buffer

    private var rriBuffer = [Float](repeating: 0, count: RPeakDetector.rriBufferSize)
    private var rriWriteIndex = 0
    private var rriCount = 0

    // MARK: - Public API

    /// Feed a single ECG sample. Returns RRI (ms) if a beat is detected, else nil.
    mutating func feed(_ sample: Float) -> Float? {

        // ── Initialisation phase: accumulate first second ─────────────────
        if !isInitialised {
            initBuffer.append(sample)
            if initBuffer.count >= 1000 {
                let peak = initBuffer.max() ?? 1.0
                threshold = peak * 0.5
                isInitialised = true
                initBuffer.removeAll(keepingCapacity: false)
                samplesSinceLast = 0
                blankingCountdown = 0
                peakInWindow = 0
            }
            return nil
        }

        samplesSinceLast += 1

        // ── Blanking period ───────────────────────────────────────────────
        if blankingCountdown > 0 {
            blankingCountdown -= 1
            return nil
        }

        // Track running peak for threshold update after detection
        if sample > peakInWindow {
            peakInWindow = sample
        }

        // ── Threshold crossing detection ──────────────────────────────────
        guard sample > threshold else { return nil }

        // Candidate beat detected — compute RRI
        let rriMs = Float(samplesSinceLast)          // 1 sample = 1 ms at 1000 Hz

        // Update adaptive threshold
        threshold = 0.875 * threshold + 0.125 * peakInWindow

        // Reset for next beat
        samplesSinceLast = 0
        blankingCountdown = RPeakDetector.blankingSamples
        peakInWindow = 0

        // ── RRI validity gating ───────────────────────────────────────────
        guard rriMs >= RPeakDetector.rriMin, rriMs <= RPeakDetector.rriMax else {
            return nil
        }

        // Store valid RRI
        storeRRI(rriMs)
        return rriMs
    }

    // MARK: - RMSSD

    /// RMSSD over the last 30 valid RRIs. Nil if fewer than 2 values are available.
    var rmssd: Float? {
        let n = min(rriCount, RPeakDetector.rmssdWindowSize)
        guard n >= 2 else { return nil }

        // Extract the n most recent RRIs in chronological order
        var values = [Float](repeating: 0, count: n)
        for i in 0..<n {
            let slot = ((rriWriteIndex - n + i) % RPeakDetector.rriBufferSize
                        + RPeakDetector.rriBufferSize) % RPeakDetector.rriBufferSize
            values[i] = rriBuffer[slot]
        }

        // Compute RMSSD: sqrt( mean of squared successive differences )
        var sumSq: Float = 0
        for i in 1..<n {
            let diff = values[i] - values[i - 1]
            sumSq += diff * diff
        }
        return sqrt(sumSq / Float(n - 1))
    }

    // MARK: - Private helpers

    private mutating func storeRRI(_ rri: Float) {
        rriBuffer[rriWriteIndex] = rri
        rriWriteIndex = (rriWriteIndex + 1) % RPeakDetector.rriBufferSize
        if rriCount < RPeakDetector.rriBufferSize { rriCount += 1 }
    }
}
