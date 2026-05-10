import Foundation
import WatchConnectivity
import Combine

// MARK: - Message Keys

enum WatchMessageKey {
    static let heartRate      = "hr"        // Double, bpm
    static let respirationRate = "rr"       // Double, breaths/min
    static let ecgSamples     = "ecg"       // [Double], 1000 Hz lead-1 samples (batch)
    static let deviceConnected = "connected" // Bool
}

// MARK: - WatchConnectivityReceiver

final class WatchConnectivityReceiver: NSObject, ObservableObject {

    // MARK: Singleton (used by the app delegate before the SwiftUI @StateObject is live)
    static let shared = WatchConnectivityReceiver()

    // MARK: Published state
    @Published private(set) var heartRate: Double = 0          // bpm, 0 = no data
    @Published private(set) var respirationRate: Double = 0    // breaths/min
    @Published private(set) var ecgBuffer: [Double] = []       // ring buffer, last 5 s at 1000 Hz
    @Published private(set) var deviceConnected: Bool = false

    // 5 seconds × 1000 samples/s
    private let ecgBufferCapacity = 5_000

    private var session: WCSession?

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
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }

            if let hr = message[WatchMessageKey.heartRate] as? Double {
                self.heartRate = hr
            }
            if let rr = message[WatchMessageKey.respirationRate] as? Double {
                self.respirationRate = rr
            }
            if let samples = message[WatchMessageKey.ecgSamples] as? [Double] {
                self.appendECG(samples)
            }
            if let connected = message[WatchMessageKey.deviceConnected] as? Bool {
                self.deviceConnected = connected
            }
        }
    }
}

// MARK: - WCSessionDelegate

extension WatchConnectivityReceiver: WCSessionDelegate {

    func session(_ session: WCSession,
                 activationDidCompleteWith activationState: WCSessionActivationState,
                 error: Error?) {
        // No additional setup required on watchOS activation
    }

    // Real-time messages from the iPhone app
    func session(_ session: WCSession,
                 didReceiveMessage message: [String: Any]) {
        handleMessage(message)
    }

    func session(_ session: WCSession,
                 didReceiveMessage message: [String: Any],
                 replyHandler: @escaping ([String: Any]) -> Void) {
        handleMessage(message)
        replyHandler([:])
    }

    // Application context (periodic state sync)
    func session(_ session: WCSession,
                 didReceiveApplicationContext applicationContext: [String: Any]) {
        handleMessage(applicationContext)
    }
}
