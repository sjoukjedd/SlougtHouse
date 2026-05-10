import SwiftUI

/// Root view — three-tab layout: Connect / Live / Dashboard.
struct ContentView: View {

    @EnvironmentObject var ble: BLEManager

    private let accentColour = Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0)

    var body: some View {
        TabView {
            // ── Tab 1: Connect ────────────────────────────────────────────
            ConnectionView()
                .tabItem {
                    Label("Connect", systemImage: "antenna.radiowaves.left.and.right")
                }

            // ── Tab 2: Live Waveforms ─────────────────────────────────────
            LiveWaveformView()
                .tabItem {
                    Label("Live", systemImage: "waveform.path.ecg")
                }

            // ── Tab 3: Dashboard ──────────────────────────────────────────
            SignalDashboardView()
                .tabItem {
                    Label("Dashboard", systemImage: "gauge.with.dots.needle.67percent")
                }
        }
        .tint(accentColour)
    }
}

// MARK: - Live Waveform Stack

/// Stacks four waveforms: ECG1, ICG (dZ/dt), SCL, PPG-IR.
private struct LiveWaveformView: View {

    @EnvironmentObject var ble: BLEManager

    private let background = Color(red: 0x08/255.0, green: 0x0C/255.0, blue: 0x1E/255.0)

    var body: some View {
        ZStack {
            background.ignoresSafeArea()
            VStack(spacing: 2) {
                channelRow(title: "ECG",
                           buffer: ble.ecg1Buffer,
                           color: .green,
                           yRange: -1500...1500)

                channelRow(title: "ICG dZ/dt",
                           buffer: ble.icgBuffer,
                           color: Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0),
                           yRange: -2...2)

                channelRow(title: "SCL",
                           buffer: ble.sclBuffer,
                           color: .yellow,
                           yRange: 0...25)

                channelRow(title: "PPG-IR",
                           buffer: ble.ppgBuffer,
                           color: .red,
                           yRange: 0...1_000_000)
            }
            .padding(.vertical, 8)
        }
    }

    @ViewBuilder
    private func channelRow(title: String,
                            buffer: SignalBuffer,
                            color: Color,
                            yRange: ClosedRange<Float>) -> some View {
        VStack(alignment: .leading, spacing: 0) {
            Text(title)
                .font(.caption2)
                .foregroundStyle(.white.opacity(0.5))
                .padding(.leading, 8)
            WaveformView(buffer: buffer, color: color, yRange: yRange)
                .frame(maxWidth: .infinity)
                .frame(height: 130)
        }
    }
}

#Preview {
    ContentView()
        .environmentObject(BLEManager())
}
