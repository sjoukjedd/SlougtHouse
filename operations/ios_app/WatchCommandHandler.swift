import Foundation
import WatchConnectivity
import Observation

// MARK: - WatchCommandHandler
//
// iOS-only. Listens for WatchConnectivity messages from the Apple Watch and
// forwards recording commands to BLEManager.
// Add to the environment via VUAMSApp / AppRootView.

@Observable
@MainActor
final class WatchCommandHandler: NSObject {

    var ble: BLEManager?

    // MARK: Session activation

    func activateSession() {
        guard WCSession.isSupported() else { return }
        let s = WCSession.default
        s.delegate = self
        s.activate()
    }
}

// MARK: - WCSessionDelegate

extension WatchCommandHandler: WCSessionDelegate {

    nonisolated func session(_ session: WCSession,
                             activationDidCompleteWith activationState: WCSessionActivationState,
                             error: Error?) {}

    nonisolated func sessionDidBecomeInactive(_ session: WCSession) {}

    nonisolated func sessionDidDeactivate(_ session: WCSession) {
        session.activate()
    }

    nonisolated func session(_ session: WCSession,
                             didReceiveMessage message: [String: Any]) {
        Task { @MainActor in handleCommand(message) }
    }

    nonisolated func session(_ session: WCSession,
                             didReceiveMessage message: [String: Any],
                             replyHandler: @escaping ([String: Any]) -> Void) {
        Task { @MainActor in handleCommand(message) }
        replyHandler([:])
    }

    // MARK: Private

    private func handleCommand(_ message: [String: Any]) {
        guard let cmd = message["cmd"] as? String else { return }
        switch cmd {
        case "startRecording":
            ble?.startRecording()
            sendRecordingState(true)
        case "stopRecording":
            ble?.stopRecording()
            sendRecordingState(false)
        default:
            break
        }
    }

    private func sendRecordingState(_ isRecording: Bool) {
        let s = WCSession.default
        guard s.activationState == .activated, s.isReachable else { return }
        s.sendMessage(["isRecording": isRecording], replyHandler: nil, errorHandler: nil)
    }
}
