import Foundation
import Observation
import SwiftData
import WatchConnectivity

// MARK: - CSIComponents

/// Z-scores for each individual stress marker contributing to the CSI.
struct CSIComponents {
    let pepZ: Double      // z(PEP⁻¹)
    let sclZ: Double      // z(SCL)
    let rmssdZ: Double    // z(RMSSD⁻¹)
    let scrRateZ: Double  // z(SCR_rate)
    var rrZ: Double?      // z(RR) — present only when CSI-5-RR is active
}

// MARK: - CSIEngine
//
// Composite Stress Index engine. Computes a 0–100 stress score from four
// physiological markers every 30 seconds, following Vasquez's CSI-001 spec.
//
// Markers:
//   PEP⁻¹     — sympathetic cardiac (IBlock.pep)
//   SCL        — sympathetic EDA tonic (SBlock.sclTonic)
//   RMSSD⁻¹   — vagal cardiac (ECG R-peak detector)
//   SCR rate   — sympathetic EDA phasic (SBlock.sclPhasic crossings)
//
// Baseline: first 120 s of low-activity epochs (4 × 30 s windows) establish
// per-marker μ and σ. baselineProgress tracks 0.0–1.0 over those 120 s.
//
// Activity gating: epochs from walking/running/cycling/stairs are skipped.
//
// Persistence: the finalised baseline is written to SwiftData via the
// ModelContext supplied at init time, so μ/σ survive app restarts.

@Observable
@MainActor
final class CSIEngine {

    // MARK: - Observable state

    var score: Double?                   // CSI 0–100, nil until baseline complete
    var isBaselineComplete: Bool = false
    var baselineProgress: Double = 0.0   // 0.0–1.0 over 120 s of valid baseline
    var isActivityGated: Bool = false
    var componentScores: CSIComponents?
    private(set) var csiLabel: String = "CSI-4"

    // MARK: - Algorithm constants

    private static let epochSeconds:    Double = 30.0
    private static let baselineEpochs:  Int    = 4       // 4 × 30 s = 120 s
    private static let w1: Double = 0.35  // PEP⁻¹
    private static let w2: Double = 0.30  // SCL
    private static let w3: Double = 0.25  // RMSSD⁻¹
    private static let w4: Double = 0.10  // SCR rate
    private static let sigmoidK: Double = 1.5
    private static let scrThreshold: Float = 0.05  // µS phasic crossing threshold

    // MARK: - Internals (not Observable-tracked)

    private let ble: BLEManager
    private let modelContext: ModelContext

    // R-peak detector — fed from ECG buffer on each epoch
    private var rpeak = RPeakDetector()
    // Track how many ECG samples we have already processed
    private var lastECGSampleCount = 0

    // SBlock phasic history for SCR rate computation (last 60 s of S-blocks)
    // We store (sclPhasic, timestamp) pairs; timestamps are Date() of receipt.
    private var sclPhasicHistory: [(value: Float, date: Date)] = []
    private var lastSCRBlock: SBlock?

    // Epoch accumulation
    private var epochPEPSamples:  [Double] = []
    private var epochSCLSamples:  [Double] = []

    // Baseline storage: arrays of per-epoch marker means
    private var baselinePEPInv:   [Double] = []
    private var baselineSCL:      [Double] = []
    private var baselineRMSSDInv: [Double] = []
    private var baselineSCRRate:  [Double] = []

    // Baseline statistics (μ, σ) computed once baseline is complete
    private var mu  = MarkerStats()
    private var sig = MarkerStats()

    // Firmware-supplied HRV value (from Y-block); preferred over self-computed RMSSD
    // when available and beat count is sufficient.
    private var latestFirmwareRMSSD: Float? = nil

    // R-block — latest respiratory rate measurement from firmware
    private var latestRBlock: RBlock? = nil

    // RR baseline statistics (initial estimates; updated during baseline accumulation)
    private var rrMean: Double = 15.0
    private var rrSd: Double = 3.0
    private var baselineRR: [Double] = []

    // Timer
    private var epochTimer: Timer?

    // MARK: - Init

    init(ble: BLEManager, modelContext: ModelContext) {
        self.ble = ble
        self.modelContext = modelContext
        restoreBaseline()
        scheduleTimer()
    }

    // Timer is invalidated via nonisolated deinit-safe path:
    // We store a reference in a nonisolated wrapper to allow deinit to call invalidate.
    // Since CSIEngine is @MainActor, deinit runs on the main actor — but Swift 5.9
    // still requires nonisolated on deinit. Work around by storing the timer as
    // @ObservationIgnored (already on main actor path) and handling lifecycle in a
    // cancel() method called from BLEManager if ever needed.
    // For now: timer is implicitly invalidated when the object is deallocated since
    // Timer holds a weak target reference (we use a closure-based timer — the closure
    // captures [weak self], so the timer fires but does nothing after dealloc).

    // MARK: - Baseline persistence

    /// Attempt to restore a previously saved baseline from SwiftData.
    private func restoreBaseline() {
        var descriptor = FetchDescriptor<CSIBaselineRecord>(
            sortBy: [SortDescriptor(\.updatedAt, order: .reverse)]
        )
        descriptor.fetchLimit = 1
        guard let record = try? modelContext.fetch(descriptor).first,
              record.epochCount >= CSIEngine.baselineEpochs else { return }

        mu.pepInv   = record.pepInvMean
        mu.scl      = record.sclMean
        mu.rmssdInv = record.rmssdInvMean
        mu.scrRate  = record.scrRateMean

        sig.pepInv   = record.pepInvStd
        sig.scl      = record.sclStd
        sig.rmssdInv = record.rmssdInvStd
        sig.scrRate  = record.scrRateStd

        isBaselineComplete = true
        baselineProgress   = 1.0
    }

    /// Persist the current baseline statistics as a single record (upsert pattern).
    private func persistBaseline(epochCount: Int) {
        // Delete any existing records
        let existing = (try? modelContext.fetch(FetchDescriptor<CSIBaselineRecord>())) ?? []
        for old in existing { modelContext.delete(old) }

        let record = CSIBaselineRecord(
            updatedAt:    .now,
            pepInvMean:   mu.pepInv,
            pepInvStd:    sig.pepInv,
            sclMean:      mu.scl,
            sclStd:       sig.scl,
            rmssdInvMean: mu.rmssdInv,
            rmssdInvStd:  sig.rmssdInv,
            scrRateMean:  mu.scrRate,
            scrRateStd:   sig.scrRate,
            epochCount:   epochCount
        )
        modelContext.insert(record)
        try? modelContext.save()
    }

    // MARK: - Timer

    private func scheduleTimer() {
        epochTimer = Timer.scheduledTimer(
            withTimeInterval: CSIEngine.epochSeconds,
            repeats: true
        ) { [weak self] _ in
            Task { @MainActor [weak self] in
                self?.computeEpoch()
            }
        }
    }

    // MARK: - Y-block integration

    func updateFromYBlock(_ block: YBlock) {
        if block.beatCount >= 10 {
            latestFirmwareRMSSD = block.rmssdMs
        }
    }

    // MARK: - R-block integration

    func updateFromRBlock(_ block: RBlock) {
        latestRBlock = block
    }

    // MARK: - Epoch computation

    private func computeEpoch() {
        // ── Activity gating ───────────────────────────────────────────────
        if let activity = ble.currentActivity {
            switch activity.activityClass {
            case .walking, .running, .cycling, .stairsUp, .stairsDown:
                isActivityGated = true
                sendWatchMessage()
                return
            default:
                break
            }
        }
        isActivityGated = false

        // ── Feed new ECG samples through R-peak detector ──────────────────
        let ecgSampleCount = ble.ecg1Buffer.currentCount
        if ecgSampleCount > lastECGSampleCount {
            let newCount = min(ecgSampleCount - lastECGSampleCount,
                               ble.ecg1Buffer.currentCount)
            if newCount > 0 {
                let samples = ble.ecg1Buffer.latest(count: newCount)
                for s in samples {
                    _ = rpeak.feed(s)
                }
            }
            lastECGSampleCount = ecgSampleCount
        }

        // ── Collect IBlock PEP samples ─────────────────────────────────────
        // IBlock delivers per-beat PEP; we use whatever the BLEManager holds.
        if let iBlock = ble.latestIBlock, iBlock.pep > 0 {
            epochPEPSamples.append(Double(iBlock.pep))
        }

        // ── Collect SCL samples ────────────────────────────────────────────
        if let sBlock = ble.latestSBlock {
            epochSCLSamples.append(Double(sBlock.sclTonic))

            // Track phasic history for SCR rate (keep last 60 s)
            let now = Date()
            sclPhasicHistory.append((value: sBlock.sclPhasic, date: now))
            sclPhasicHistory.removeAll { now.timeIntervalSince($0.date) > 60 }
            lastSCRBlock = sBlock
        }

        // ── Compute per-epoch marker values ───────────────────────────────

        // PEP⁻¹ (ms⁻¹)
        let pepInv: Double?
        if epochPEPSamples.isEmpty {
            pepInv = nil
        } else {
            let meanPEP = epochPEPSamples.reduce(0, +) / Double(epochPEPSamples.count)
            pepInv = meanPEP > 0 ? (1000.0 / meanPEP) : nil
        }
        epochPEPSamples.removeAll(keepingCapacity: true)

        // SCL mean
        let sclMean: Double?
        if epochSCLSamples.isEmpty {
            sclMean = nil
        } else {
            sclMean = epochSCLSamples.reduce(0, +) / Double(epochSCLSamples.count)
        }
        epochSCLSamples.removeAll(keepingCapacity: true)

        // RMSSD⁻¹ — prefer firmware value if available and beat count is sufficient
        let rmssdInv: Double?
        let rmssd = latestFirmwareRMSSD ?? rpeak.rmssd
        if let r = rmssd, r > 0 {
            rmssdInv = 1000.0 / Double(r)
        } else {
            rmssdInv = nil
        }

        // SCR rate (events / min from phasic crossings, 60 s window)
        let scrRate: Double = computeSCRRate()

        // ── Determine if R-block is usable for CSI-5-RR ──────────────────
        let rrBlock = latestRBlock
        let rrIsUsable: Bool
        if let rb = rrBlock, rb.rrValid, rb.rrBpm >= 4, rb.rrBpm <= 40 {
            rrIsUsable = true
        } else {
            rrIsUsable = false
        }

        // ── Baseline accumulation ─────────────────────────────────────────
        if !isBaselineComplete {
            if let p = pepInv   { baselinePEPInv.append(p) }
            if let s = sclMean  { baselineSCL.append(s) }
            if let r = rmssdInv { baselineRMSSDInv.append(r) }
            baselineSCRRate.append(scrRate)
            if rrIsUsable, let rb = rrBlock {
                baselineRR.append(Double(rb.rrBpm))
            }

            // Baseline progress is driven by SCL availability (most reliable indicator)
            let epochsDone = max(baselinePEPInv.count,
                                 baselineSCL.count,
                                 baselineRMSSDInv.count,
                                 baselineSCRRate.count)
            baselineProgress = min(1.0, Double(epochsDone) / Double(CSIEngine.baselineEpochs))

            if epochsDone >= CSIEngine.baselineEpochs {
                finaliseBaseline()
            }
            return
        }

        // ── Z-score and CSI ───────────────────────────────────────────────
        guard isBaselineComplete else { return }

        if rrIsUsable, let rb = rrBlock {
            // CSI-5-RR path
            csiLabel = "CSI-5"
            let rrBpmD = Double(rb.rrBpm)
            let rrZ = clampZ((rrBpmD - rrMean) / safeSD(rrSd))

            // Select weights based on confounder flag
            // [PEP⁻¹, SCL, RMSSD⁻¹, SCR_rate, RR]
            let weights: [Double]
            if rb.rrConfounder {
                weights = [0.39, 0.33, 0.10, 0.08, 0.10]
            } else {
                weights = [0.32, 0.27, 0.22, 0.09, 0.10]
            }

            var availableWeights: [(w: Double, z: Double)] = []
            if let p = pepInv {
                let z = clampZ((p - mu.pepInv) / safeSD(sig.pepInv))
                availableWeights.append((weights[0], z))
            }
            if let s = sclMean {
                let z = clampZ((s - mu.scl) / safeSD(sig.scl))
                availableWeights.append((weights[1], z))
            }
            if let r = rmssdInv {
                let z = clampZ((r - mu.rmssdInv) / safeSD(sig.rmssdInv))
                availableWeights.append((weights[2], z))
            }
            let scrZ = clampZ((scrRate - mu.scrRate) / safeSD(sig.scrRate))
            availableWeights.append((weights[3], scrZ))
            availableWeights.append((weights[4], rrZ))

            guard availableWeights.count >= 2 else {
                score = nil
                sendWatchMessage()
                return
            }

            let totalW = availableWeights.reduce(0) { $0 + $1.w }
            let csiRaw = availableWeights.reduce(0) { $0 + ($1.w / totalW) * $1.z }
            let csiFinal = 100.0 / (1.0 + exp(-CSIEngine.sigmoidK * csiRaw))

            let pepZ   = pepInv.map    { clampZ(($0 - mu.pepInv)   / safeSD(sig.pepInv))   } ?? 0
            let sclZ   = sclMean.map   { clampZ(($0 - mu.scl)      / safeSD(sig.scl))      } ?? 0
            let rmssdZ = rmssdInv.map  { clampZ(($0 - mu.rmssdInv) / safeSD(sig.rmssdInv)) } ?? 0

            score = csiFinal
            componentScores = CSIComponents(
                pepZ:     pepZ,
                sclZ:     sclZ,
                rmssdZ:   rmssdZ,
                scrRateZ: scrZ,
                rrZ:      rrZ
            )
        } else {
            // CSI-4 fallback path
            csiLabel = "CSI-4"

            var availableWeights: [(w: Double, z: Double)] = []

            if let p = pepInv {
                let z = clampZ((p - mu.pepInv) / safeSD(sig.pepInv))
                availableWeights.append((CSIEngine.w1, z))
            }
            if let s = sclMean {
                let z = clampZ((s - mu.scl) / safeSD(sig.scl))
                availableWeights.append((CSIEngine.w2, z))
            }
            if let r = rmssdInv {
                let z = clampZ((r - mu.rmssdInv) / safeSD(sig.rmssdInv))
                availableWeights.append((CSIEngine.w3, z))
            }
            let scrZ = clampZ((scrRate - mu.scrRate) / safeSD(sig.scrRate))
            availableWeights.append((CSIEngine.w4, scrZ))

            guard availableWeights.count >= 2 else {
                score = nil
                sendWatchMessage()
                return
            }

            // Proportional reweighting for missing markers
            let totalW = availableWeights.reduce(0) { $0 + $1.w }
            let csiRaw = availableWeights.reduce(0) { $0 + ($1.w / totalW) * $1.z }

            // Sigmoid to [0, 100]
            let csiFinal = 100.0 / (1.0 + exp(-CSIEngine.sigmoidK * csiRaw))

            // Build component scores (use all four, marking unavailable as 0)
            let pepZ   = pepInv.map    { clampZ(($0 - mu.pepInv)   / safeSD(sig.pepInv))   } ?? 0
            let sclZ   = sclMean.map   { clampZ(($0 - mu.scl)      / safeSD(sig.scl))      } ?? 0
            let rmssdZ = rmssdInv.map  { clampZ(($0 - mu.rmssdInv) / safeSD(sig.rmssdInv)) } ?? 0

            score = csiFinal
            componentScores = CSIComponents(
                pepZ:     pepZ,
                sclZ:     sclZ,
                rmssdZ:   rmssdZ,
                scrRateZ: scrZ,
                rrZ:      nil
            )
        }

        sendWatchMessage()
    }

    // MARK: - Baseline finalisation

    private func finaliseBaseline() {
        mu.pepInv   = mean(baselinePEPInv)
        mu.scl      = mean(baselineSCL)
        mu.rmssdInv = mean(baselineRMSSDInv)
        mu.scrRate  = mean(baselineSCRRate)

        sig.pepInv   = stddev(baselinePEPInv)
        sig.scl      = stddev(baselineSCL)
        sig.rmssdInv = stddev(baselineRMSSDInv)
        sig.scrRate  = stddev(baselineSCRRate)

        // RR baseline — use accumulated values if we have enough, otherwise keep initial estimates
        if baselineRR.count >= 2 {
            rrMean = mean(baselineRR)
            rrSd   = stddev(baselineRR)
        }

        isBaselineComplete = true
        baselineProgress = 1.0

        let epochCount = max(baselinePEPInv.count,
                             baselineSCL.count,
                             baselineRMSSDInv.count,
                             baselineSCRRate.count)
        persistBaseline(epochCount: epochCount)
    }

    // MARK: - WatchConnectivity forwarding

    private func sendWatchMessage() {
        let session = WCSession.default
        guard session.isReachable else { return }
        let scoreValue: Double = score ?? -1.0
        session.sendMessage(
            [
                WatchMessageKey.csiScore: scoreValue,
                WatchMessageKey.csiGated: isActivityGated
            ],
            replyHandler: nil,
            errorHandler: nil
        )
    }

    // MARK: - SCR rate computation

    private func computeSCRRate() -> Double {
        // Count phasic upward crossings of 0.05 µS threshold in the last 60 s
        // Minimum 1 s between events (enforced by only counting rising edges).
        var eventCount = 0
        var wasBelow = true
        var lastEventTime: Date? = nil
        let minInterEventSec: Double = 1.0

        for entry in sclPhasicHistory {
            let isAbove = entry.value >= CSIEngine.scrThreshold
            if isAbove && wasBelow {
                // Rising edge — check inter-event interval
                if let last = lastEventTime,
                   entry.date.timeIntervalSince(last) < minInterEventSec {
                    // Too close — not a new event
                } else {
                    eventCount += 1
                    lastEventTime = entry.date
                }
            }
            wasBelow = !isAbove
        }

        // sclPhasicHistory covers ≤ 60 s, express rate as events/min
        let windowMin = sclPhasicHistory.isEmpty ? 1.0 :
            min(1.0, (sclPhasicHistory.last!.date.timeIntervalSince(
                sclPhasicHistory.first!.date) + 1.0) / 60.0)
        return windowMin > 0 ? Double(eventCount) / windowMin : Double(eventCount)
    }

    // MARK: - Statistics helpers

    private func mean(_ values: [Double]) -> Double {
        guard !values.isEmpty else { return 0 }
        return values.reduce(0, +) / Double(values.count)
    }

    private func stddev(_ values: [Double]) -> Double {
        guard values.count >= 2 else { return 0 }
        let m = mean(values)
        let variance = values.reduce(0) { $0 + ($1 - m) * ($1 - m) } / Double(values.count - 1)
        return sqrt(variance)
    }

    private func safeSD(_ sd: Double) -> Double {
        sd < 1e-6 ? 1.0 : sd   // If σ < 1e-6, treat as 1.0 → z = x - μ (no division by 0)
    }

    private func clampZ(_ z: Double) -> Double {
        max(-3.0, min(3.0, z.isNaN ? 0 : z))
    }
}

// MARK: - MarkerStats helper

private struct MarkerStats {
    var pepInv:   Double = 0
    var scl:      Double = 0
    var rmssdInv: Double = 0
    var scrRate:  Double = 0
}
