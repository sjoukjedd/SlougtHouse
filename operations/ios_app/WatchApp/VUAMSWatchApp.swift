import SwiftUI
import WatchKit

@main
struct VUAMSWatchApp: App {

    @WKApplicationDelegateAdaptor(WatchAppDelegate.self) var delegate
    @State private var connectivityReceiver = WatchConnectivityReceiver()

    var body: some Scene {
        WindowGroup {
            WatchContentView()
                .environment(connectivityReceiver)
                .preferredColorScheme(.dark)
        }
    }
}

// MARK: - App Delegate

final class WatchAppDelegate: NSObject, WKApplicationDelegate {

    func applicationDidFinishLaunching() {
        WatchConnectivityReceiver.shared.activateSession()
    }

    func applicationWillEnterForeground() {
        WatchConnectivityReceiver.shared.activateSession()
    }
}
