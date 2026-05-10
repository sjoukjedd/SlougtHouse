import SwiftUI

struct WatchContentView: View {

    @EnvironmentObject private var receiver: WatchConnectivityReceiver

    var body: some View {
        ScrollView {
            VStack(spacing: 8) {
                connectionBadge

                WatchWaveformView(samples: receiver.ecgBuffer)
                    .frame(height: 60)
                    .padding(.horizontal, 4)

                metricsRow
            }
            .padding(.vertical, 6)
        }
        .navigationTitle("VU-AMS")
        .navigationBarTitleDisplayMode(.inline)
    }

    // MARK: - Subviews

    private var connectionBadge: some View {
        HStack(spacing: 4) {
            Circle()
                .fill(receiver.deviceConnected ? Color.green : Color.orange)
                .frame(width: 7, height: 7)
            Text(receiver.deviceConnected ? "Connected" : "No Signal")
                .font(.system(size: 11, weight: .medium))
                .foregroundStyle(receiver.deviceConnected ? .green : .orange)
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 3)
        .background(.ultraThinMaterial, in: Capsule())
    }

    private var metricsRow: some View {
        HStack(spacing: 0) {
            metricTile(
                value: receiver.heartRate > 0 ? String(format: "%.0f", receiver.heartRate) : "--",
                unit: "bpm",
                label: "HR",
                icon: "heart.fill",
                iconColor: .red
            )

            Divider()
                .frame(height: 40)
                .background(.secondary.opacity(0.4))

            metricTile(
                value: receiver.respirationRate > 0 ? String(format: "%.1f", receiver.respirationRate) : "--",
                unit: "br/min",
                label: "RR",
                icon: "lungs.fill",
                iconColor: .cyan
            )
        }
        .padding(.horizontal, 4)
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
    }

    private func metricTile(value: String,
                             unit: String,
                             label: String,
                             icon: String,
                             iconColor: Color) -> some View {
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
        .environmentObject(receiver)
}
