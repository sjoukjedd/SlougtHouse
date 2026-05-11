import Foundation

// MARK: - Signal Generator
// All synthetic physiological signals for the VU-AMS hardware simulator.
// Thread-safe: all state is isolated per instance, call from a single serial queue.

final class SignalGenerator {

    // MARK: Configuration

    struct Config {
        var heartRateBPM: Double = 72.0
        var sclTonic: Double = 3.2          // µS
        var stressMode: Bool = false
    }

    var config: Config {
        didSet { applyConfig() }
    }

    // MARK: - Latest sample (zero-cost read, no time advance)

    private(set) var latest: Sample

    // MARK: - Time tracking

    private var t: Double = 0.0   // master clock in seconds

    // MARK: - ECG state

    // Five-Gaussian P-QRS-T model.
    // Each Gaussian: (time offset relative to R-peak, amplitude mV, width s)
    private struct GaussParam { let dt: Double; let amp: Double; let sigma: Double }
    private var ecgGaussians: [GaussParam] = []
    private var rrInterval: Double = 0.0
    private var lastRPeak: Double = 0.0
    private let ecgNoiseAmplitude: Double = 0.000015   // 15 µV rms
    private var ecgRng = SeededRandom(seed: 1001)
    private var rrRng  = SeededRandom(seed: 2002)

    // MARK: - ICG state

    private var icgRng = SeededRandom(seed: 3003)
    // PEP = 90 ms, LVET = 290 ms (relative to R-peak)
    private let pepOffset: Double  = 0.090
    private let lvetDuration: Double = 0.290

    // MARK: - SCL state

    private var sclPhase: Double = 0.0   // tonic sinus phase
    private var scrEvents: [SCREvent] = []
    private var scrRng = SeededRandom(seed: 4004)
    private var nextScrTime: Double = 0.0

    private struct SCREvent {
        var startTime: Double
        var amplitude: Double   // µS
        var riseTC: Double      // time constant (s)
        var recoveryTC: Double
    }

    // MARK: - PPG state

    private var ppgRng = SeededRandom(seed: 5005)

    // MARK: - IMU state

    private var imuRng = SeededRandom(seed: 6006)

    // MARK: - Temperature state

    private var tempRng = SeededRandom(seed: 7007)

    // MARK: - Init

    init(config: Config = Config()) {
        self.config = config
        latest = Sample(
            time: 0, ecgMV: 0, icgOhmPerS: 0,
            sclTonic: config.sclTonic, sclPhasic: 0,
            ppgAC: 1.0, spo2Pct: 97.5,
            ax: 0, ay: 0, az: 9.81, gx: 0, gy: 0, gz: 0,
            tempC: 33.4, hrBPM: config.heartRateBPM
        )
        applyConfig()
        scheduleNextScr()
    }

    private func applyConfig() {
        let hr = config.stressMode ? 95.0 : config.heartRateBPM
        rrInterval = 60.0 / hr
        // Rebuild Gaussian waveform scaled to amplitude 1.2 mV at R
        ecgGaussians = [
            GaussParam(dt: -0.200, amp:  0.12, sigma: 0.035),  // P
            GaussParam(dt: -0.050, amp: -0.08, sigma: 0.010),  // Q
            GaussParam(dt:  0.000, amp:  1.20, sigma: 0.012),  // R
            GaussParam(dt:  0.040, amp: -0.25, sigma: 0.014),  // S
            GaussParam(dt:  0.180, amp:  0.30, sigma: 0.040),  // T
        ]
    }

    // MARK: - Advance clock

    /// Advance by `dt` seconds. Returns a snapshot of all signals at the new time.
    @discardableResult
    func advance(by dt: Double) -> Sample {
        t += dt
        let s = makeSample()
        latest = s
        return s
    }

    // MARK: - ECG

    private func ecgVoltage() -> Double {
        // Phase within current RR interval
        let phase = (t - lastRPeak).truncatingRemainder(dividingBy: rrInterval)

        // At each new beat boundary, jitter the RR interval
        if phase < 0 || (t - lastRPeak) >= rrInterval {
            let jitter = (rrRng.nextDouble() * 2 - 1) * 0.020   // ±20 ms
            lastRPeak = t - (phase < 0 ? 0 : phase) + jitter
        }

        let phaseInBeat = t - lastRPeak
        var v = 0.0
        for g in ecgGaussians {
            let x = phaseInBeat - (rrInterval * 0.5 + g.dt)  // align R at midpoint
            v += g.amp * exp(-0.5 * (x / g.sigma) * (x / g.sigma))
        }
        v += ecgNoiseAmplitude * ecgRng.nextGaussian()
        return v   // mV
    }

    // MARK: - ICG

    private func icgDZDT() -> Double {
        // Synchronised with ECG R-peak
        let phaseInBeat = (t - lastRPeak).truncatingRemainder(dividingBy: rrInterval)
        let bPoint = pepOffset
        let cPoint = pepOffset + 0.060
        let xPoint = pepOffset + lvetDuration * 0.60
        let yPoint = pepOffset + lvetDuration
        let oPoint = pepOffset + lvetDuration + 0.080

        var dz = 0.0
        // B-C upslope (piecewise sine)
        if phaseInBeat >= bPoint && phaseInBeat < cPoint {
            let frac = (phaseInBeat - bPoint) / (cPoint - bPoint)
            dz = -1.8 * sin(.pi * frac)
        }
        // C-X downslope
        else if phaseInBeat >= cPoint && phaseInBeat < xPoint {
            let frac = (phaseInBeat - cPoint) / (xPoint - cPoint)
            dz = -1.8 * cos(.pi * frac * 0.5)
        }
        // X-Y recovery
        else if phaseInBeat >= xPoint && phaseInBeat < yPoint {
            let frac = (phaseInBeat - xPoint) / (yPoint - xPoint)
            dz = 0.3 * sin(.pi * frac)
        }
        // Y-O notch
        else if phaseInBeat >= yPoint && phaseInBeat < oPoint {
            let frac = (phaseInBeat - yPoint) / (oPoint - yPoint)
            dz = 0.10 * sin(.pi * frac)
        }
        dz += icgRng.nextGaussian() * 0.02
        return dz   // Ω/s  (scaled for ~SV 70 mL, CO ~5 L/min)
    }

    // MARK: - SCL

    private func scheduleNextScr() {
        let lambda = config.stressMode ? 0.15 : 0.05  // events/s
        let interval = -log(1.0 - scrRng.nextDouble()) / lambda
        nextScrTime = t + interval
    }

    private func updateScr() {
        if t >= nextScrTime {
            let amp = 0.1 + scrRng.nextDouble() * (config.stressMode ? 0.8 : 0.5)
            scrEvents.append(SCREvent(
                startTime: t,
                amplitude: amp,
                riseTC: 1.5,
                recoveryTC: 7.0
            ))
            scheduleNextScr()
        }
        // Purge fully-decayed events (5 tau)
        scrEvents.removeAll { t - $0.startTime > $0.recoveryTC * 5 }
    }

    // Returns (tonic µS, phasic µS) as separate components.
    private func sclComponents() -> (tonic: Double, phasic: Double) {
        updateScr()
        let tonicBase = config.stressMode ? 6.0 : config.sclTonic
        let tonic = tonicBase + 0.4 * sin(2 * .pi * t / 120.0)
        var phasic = 0.0
        for ev in scrEvents {
            let elapsed = t - ev.startTime
            let rise = 1.0 - exp(-elapsed / ev.riseTC)
            let decay = exp(-elapsed / ev.recoveryTC)
            phasic += ev.amplitude * rise * decay
        }
        return (tonic, phasic)
    }

    // MARK: - PPG

    private func ppgValue() -> (ac: Double, spo2: Double) {
        let hr = config.stressMode ? 95.0 : config.heartRateBPM
        let cardiac = 0.05 * sin(2 * .pi * (hr / 60.0) * t)
        let resp    = 0.008 * sin(2 * .pi * 0.25 * t)
        let ac = 1.0 + cardiac + resp
        let spo2 = 97.5 + 0.5 * (ppgRng.nextDouble() * 2 - 1)
        return (ac, spo2)
    }

    // MARK: - IMU

    private func imuValues() -> (ax: Double, ay: Double, az: Double,
                                  gx: Double, gy: Double, gz: Double) {
        let ax = 0.02 * sin(2 * .pi * 0.25 * t) + imuRng.nextGaussian() * 0.003
        let ay = imuRng.nextGaussian() * 0.003
        let az = 9.81 + imuRng.nextGaussian() * 0.003
        let gx = imuRng.nextGaussian() * 0.01
        let gy = imuRng.nextGaussian() * 0.01
        let gz = imuRng.nextGaussian() * 0.01
        return (ax, ay, az, gx, gy, gz)
    }

    // MARK: - Temperature

    private func tempValue() -> Double {
        return 33.4 + 0.15 * sin(2 * .pi * t / 600.0) + 0.02 * (tempRng.nextDouble() * 2 - 1)
    }

    // MARK: - Sample assembly

    private func makeSample() -> Sample {
        let ecg = ecgVoltage()
        let icg = icgDZDT()
        let scl = sclComponents()
        let ppg = ppgValue()
        let imu = imuValues()
        let temp = tempValue()
        let hr   = config.stressMode ? 95.0 : config.heartRateBPM

        return Sample(
            time: t,
            ecgMV: ecg,
            icgOhmPerS: icg,
            sclTonic: scl.tonic,
            sclPhasic: scl.phasic,
            ppgAC: ppg.ac,
            spo2Pct: ppg.spo2,
            ax: imu.ax, ay: imu.ay, az: imu.az,
            gx: imu.gx, gy: imu.gy, gz: imu.gz,
            tempC: temp,
            hrBPM: hr
        )
    }

    // MARK: - Sample type

    struct Sample {
        let time: Double
        let ecgMV: Double
        let icgOhmPerS: Double
        let sclTonic: Double    // µS tonic component
        let sclPhasic: Double   // µS phasic (SCR) component
        let ppgAC: Double
        let spo2Pct: Double
        let ax, ay, az: Double
        let gx, gy, gz: Double
        let tempC: Double
        let hrBPM: Double
    }
}

// MARK: - Minimal PRNG (no Foundation randomness on background queues)

private struct SeededRandom {
    private var state: UInt64

    init(seed: UInt64) { state = seed }

    mutating func nextUInt64() -> UInt64 {
        // xorshift64
        state ^= state << 13
        state ^= state >> 7
        state ^= state << 17
        return state
    }

    mutating func nextDouble() -> Double {
        Double(nextUInt64() & 0x001F_FFFF_FFFF_FFFF) / Double(0x001F_FFFF_FFFF_FFFF)
    }

    mutating func nextGaussian() -> Double {
        // Box-Muller
        let u1 = max(nextDouble(), 1e-15)
        let u2 = nextDouble()
        return sqrt(-2.0 * log(u1)) * cos(2 * .pi * u2)
    }
}
