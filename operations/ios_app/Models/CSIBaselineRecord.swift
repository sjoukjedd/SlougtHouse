import SwiftData

// MARK: - CSIBaselineRecord
//
// Persists the per-session CSI baseline statistics so the engine does not need
// to re-calibrate every time the app is launched.  A single record is kept;
// on every baseline update the old record is deleted and a new one inserted
// (upsert-by-replace pattern).

@Model
final class CSIBaselineRecord {

    var updatedAt: Date

    // PEP⁻¹ (ms⁻¹)
    var pepInvMean: Double
    var pepInvStd:  Double

    // SCL tonic (µS)
    var sclMean: Double
    var sclStd:  Double

    // RMSSD⁻¹ (ms⁻¹)
    var rmssdInvMean: Double
    var rmssdInvStd:  Double

    // SCR rate (events/min)
    var scrRateMean: Double
    var scrRateStd:  Double

    /// Number of valid baseline epochs that were used to compute these stats.
    var epochCount: Int

    init(updatedAt:    Date   = .now,
         pepInvMean:   Double,
         pepInvStd:    Double,
         sclMean:      Double,
         sclStd:       Double,
         rmssdInvMean: Double,
         rmssdInvStd:  Double,
         scrRateMean:  Double,
         scrRateStd:   Double,
         epochCount:   Int) {
        self.updatedAt    = updatedAt
        self.pepInvMean   = pepInvMean
        self.pepInvStd    = pepInvStd
        self.sclMean      = sclMean
        self.sclStd       = sclStd
        self.rmssdInvMean = rmssdInvMean
        self.rmssdInvStd  = rmssdInvStd
        self.scrRateMean  = scrRateMean
        self.scrRateStd   = scrRateStd
        self.epochCount   = epochCount
    }
}
