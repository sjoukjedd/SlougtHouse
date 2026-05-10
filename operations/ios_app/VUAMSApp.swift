import SwiftUI

@main
struct VUAMSApp: App {

    @State private var bleManager = BLEManager()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environment(bleManager)
                .preferredColorScheme(.dark)
        }
    }
}
