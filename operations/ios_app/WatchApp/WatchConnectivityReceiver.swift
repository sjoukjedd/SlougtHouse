import Foundation
import WatchConnectivity
import Observation

// MARK: - Message Keys

enum WatchMessageKey {
    static let heartRate       = "hr"        // Double, bpm
    static let respirationRate = "rr"        // Double, breaths/min
    static let ecgSamples      = "ecg"       // [Double], 1000 Hz lead-1 samples (batch)
    static let deviceConnected = "connected" // Bool
    static let csiScore        = "csi"       // Double, 0–100; -1 = not available
    static let csiGated        = "gated"     // Bool
}

// MARK: - WatchConnectivityReceiver

@Observable
@MainActor
final class WatchConnectivityReceiver: NSObject {

    // MARK: Singleton (used by the app delegate before the SwiftUI @State is live)
    @ObservationIgnored static let shared = WatchConnectivityReceiver()

    // MARK: Tracked state
    private(set) var heartRate: Double = 0          // bpm, 0 = no data
    private(set) var respirationRate: Double = 0    // breaths/min
    private(set) var ecgBuffer: [Double] = []       // ring buffer, last 5 s at 1000 Hz
    private(set) var deviceConnected: Bool = false
    private(set) var csiScore: Double = -1          // 0–100; -1 = not available
    private(set) var csiGated: Bool = false

    // 5 seconds × 1000 samples/s
    @ObservationIgnored private let ecgBufferCapacity = 5_000

    @ObservationIgnored private var session: WCSession?

    // MARK: Session activation

    func activateSession() {
        guard WCSession.isSupported() else { return }
        let s = WCSession.default
        s.delegate = self
        s.activate()
        session = s
    }

    // MARK: Private helpers

    private func appendECG(_ samples: [Double]) {
        var updated = ecgBuffer + samples
        if updated.count > ecgBufferCapacity {
            updated = Array(updated.dropFirst(updated.count - ecgBufferCapacity))
        }
        ecgBuffer = updated
    }

    private func handleMessage(_ message: [String: Any]) {
        if let hr = message[WatchMessageKey.heartRate] as? Double {
            heartRate = hr
        }
        if let rr = message[WatchMessageKey.respirationRate] as? Double {
            respirationRate = rr
        }
        if let samples = message[WatchMessageKey.ecgSamples] as? [Double] {
            appendECG(samples)
        }
        if let connected = message[WatchMessageKey.deviceConnected] as? Bool {
            deviceConnected = connected
        }
        if let csi = message[WatchMessageKey.csiScore] as? Double {
            csiScore = csi
        }
        if let gated = message[WatchMessageKey.csiGated] as? Bool {
            csiGated = gated
        }
    }
}

// MARK: - WCSessionDelegate

extension WatchConnectivityReceiver: WCSessionDelegate {

    nonisolated func session(_ session: WCSession,
                             activationDidCompleteWith activationState: WCSessionActivationState,
                             error: Error?) {
        // No additional setup required on watchOS activation
    }

    // Real-time messages from the iPhone app
    nonisolated func session(_ session: WCSession,
                             didReceiveMessage message: [String: Any]) {
        Task { @MainActor in handleMessage(message) }
    }

    nonisolated func session(_ session: WCSession,
                             didReceiveMessage message: [String: Any],
                             replyHandler: @escaping ([String: Any]) -> Void) {
        Task { @MainActor in handleMessage(message) }
        replyHandler([:])
    }

    // Application context (periodic state sync)
    nonisolated func session(_ session: WCSession,
                             didReceiveApplicationContext applicationContext: [String: Any]) {
        Task { @MainActor in handleMessage(applicationContext) }
    }
}
