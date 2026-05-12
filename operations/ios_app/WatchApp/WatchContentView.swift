import SwiftUI

struct WatchContentView: View {

    @Environment(WatchConnectivityReceiver.self) private var receiver

    var body: some View {
        TabView {
            // ── Page 1: Vitals / waveform ─────────────────────────────────
            WatchVitalsPage()

            // ── Page 2: CSI stress ────────────────────────────────────────
            WatchStressView()
        }
        .tabViewStyle(.page)
    }
}

// MARK: - WatchVitalsPage
//
// Original waveform + metrics content, extracted into its own page view.

private struct WatchVitalsPage: View {

    @Environment(WatchConnectivityReceiver.self) private var receiver

    var body: some View {
        ScrollView {
            VStack(spacing: 8) {
                WatchConnectionBadgeView(isConnected: receiver.deviceConnected)

                WatchWaveformView(samples: receiver.ecgBuffer)
                    .frame(height: 60)
                    .padding(.horizontal, 4)

                WatchMetricsRowView(heartRate: receiver.heartRate,
                                    respirationRate: receiver.respirationRate)

                Button {
                    let cmd = receiver.isRecording ? "stopRecording" : "startRecording"
                    receiver.sendRecordingCommand(cmd)
                } label: {
                    Label(
                        receiver.isRecording ? "Stop" : "Start",
                        systemImage: receiver.isRecording ? "stop.circle.fill" : "record.circle"
                    )
                    .font(.system(size: 13, weight: .semibold))
                    .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .tint(receiver.isRecording ? .red : .green)
                .disabled(!receiver.isPhoneReachable)
                .padding(.horizontal, 4)

                if !receiver.isPhoneReachable {
                    Text("Geen verbinding")
                        .font(.system(size: 11))
                        .foregroundStyle(.secondary)
                }
            }
            .padding(.vertical, 6)
        }
        .navigationTitle("VU-AMS")
        .navigationBarTitleDisplayMode(.inline)
    }
}

// MARK: - Connection Badge

private struct WatchConnectionBadgeView: View {
    let isConnected: Bool

    var body: some View {
        HStack(spacing: 4) {
            Circle()
                .fill(isConnected ? Color.green : Color.orange)
                .frame(width: 7, height: 7)
            Text(isConnected ? "Connected" : "No Signal")
                .font(.system(size: 11, weight: .medium))
                .foregroundStyle(isConnected ? .green : .orange)
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 3)
        .background(.ultraThinMaterial, in: Capsule())
    }
}

// MARK: - Metrics Row

private struct WatchMetricsRowView: View {
    let heartRate: Double
    let respirationRate: Double

    var body: some View {
        HStack(spacing: 0) {
            WatchMetricTileView(
                value: heartRate > 0 ? heartRate.formatted(.number.precision(.fractionLength(0))) : "--",
                unit: "bpm",
                label: "HR",
                icon: "heart.fill",
                iconColor: .red
            )

            Divider()
                .frame(height: 40)
                .background(.secondary.opacity(0.4))

            WatchMetricTileView(
                value: respirationRate > 0 ? respirationRate.formatted(.number.precision(.fractionLength(1))) : "--",
                unit: "br/min",
                label: "RR",
                icon: "lungs.fill",
                iconColor: .cyan
            )
        }
        .padding(.horizontal, 4)
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
    }
}

// MARK: - Metric Tile

private struct WatchMetricTileView: View {
    let value: String
    let unit: String
    let label: String
    let icon: String
    let iconColor: Color

    var body: some View {
        VStack(spacing: 2) {
            HStack(spacing: 3) {
                Image(systemName: icon)
                    .font(.system(size: 10))
                    .foregroundStyle(iconColor)
                Text(label)
                    .font(.system(size: 10, weight: .semibold))
                    .foregroundStyle(.secondary)
            }

            HStack(alignment: .lastTextBaseline, spacing: 2) {
                Text(value)
                    .font(.system(size: 22, weight: .bold, design: .rounded))
                    .minimumScaleFactor(0.7)
                    .lineLimit(1)
                Text(unit)
                    .font(.system(size: 9))
                    .foregroundStyle(.secondary)
            }
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 8)
    }
}

#Preview {
    let receiver = WatchConnectivityReceiver()
    return WatchContentView()
        .environment(receiver)
}
