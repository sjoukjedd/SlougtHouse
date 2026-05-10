import SwiftUI

@main
struct VUAMSApp: App {

    @StateObject private var bleManager = BLEManager()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(bleManager)
                .preferredColorScheme(.dark)
        }
    }
}
