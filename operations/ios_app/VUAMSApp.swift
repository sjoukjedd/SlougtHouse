import SwiftUI
import SwiftData

@main
struct VUAMSApp: App {

    var body: some Scene {
        WindowGroup {
            AppRootView()
                .preferredColorScheme(.dark)
        }
        .modelContainer(for: CSIBaselineRecord.self)
    }
}

// MARK: - AppRootView
//
// Thin wrapper that pulls the SwiftData ModelContext from the environment
// and uses it to construct BLEManager (which passes it down to CSIEngine).
// Keeping this separate from VUAMSApp avoids the @main + @Environment
// interaction restriction.

private struct AppRootView: View {

    @Environment(\.modelContext) private var modelContext

    @State private var bleManager: BLEManager?

    var body: some View {
        Group {
            if let ble = bleManager {
                ContentView()
                    .environment(ble)
            } else {
                // Constructed on first render; should be instantaneous.
                Color.clear
                    .onAppear {
                        bleManager = BLEManager(modelContext: modelContext)
                    }
            }
        }
    }
}
