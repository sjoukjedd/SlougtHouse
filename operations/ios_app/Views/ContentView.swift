import SwiftUI

/// Root view — four-tab layout: Connect / Live / Record / Dashboard.
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

            // ── Tab 3: Record ─────────────────────────────────────────────
            RecordingControlView()
                .tabItem {
                    Label("Record", systemImage: "record.circle")
                }

            // ── Tab 4: Dashboard ──────────────────────────────────────────
            SignalDashboardView()
                .tabItem {
                    Label("Dashboard", systemImage: "gauge.with.dots.needle.67percent")
                }
        }
        .tint(accentColour)
    }
}

// MARK: - Recording Control

/// Full-screen recording control: start / stop button + live duration counter.
///
/// When the user taps Start:
///   1. BLEManager sends 0x01 (START_RECORDING).
///   2. BLEManager immediately follows with 0x03 + 8-byte UTC µs timestamp (SYNC_TIME).
///   3. A RecordingSession is created and a 1-second display timer fires.
///
/// When the user taps Stop:
///   1. BLEManager sends 0x02 (STOP_RECORDING).
///   2. The session is discarded (sidecar writing would happen here in a real flow).
private struct RecordingControlView: View {

    @EnvironmentObject var ble: BLEManager

    @State private var session: RecordingSession?
    @State private var elapsed: TimeInterval = 0
    @State private var displayTimer: Timer?

    private let background    = Color(red: 0x08/255.0, green: 0x0C/255.0, blue: 0x1E/255.0)
    private let accentColour  = Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0)
    private let recordRed     = Color(red: 0xE5/255.0, green: 0x1D/255.0, blue: 0x35/255.0)

    private var isRecording: Bool { session != nil }

    private var isConnected: Bool {
        if case .connected = ble.connectionState { return true }
        return false
    }

    var body: some View {
        ZStack {
            background.ignoresSafeArea()

            VStack(spacing: 40) {

                // ── Status pill ───────────────────────────────────────────
                Text(isRecording ? "RECORDING" : (isConnected ? "READY" : "NOT CONNECTED"))
                    .font(.caption)
                    .fontWeight(.semibold)
                    .foregroundStyle(isRecording ? recordRed : (isConnected ? .green : .gray))
                    .padding(.horizontal, 14)
                    .padding(.vertical, 6)
                    .background(
                        Capsule()
                            .fill((isRecording ? recordRed : (isConnected ? Color.green : Color.gray))
                                    .opacity(0.15))
                    )

                // ── Duration counter ──────────────────────────────────────
                Text(formattedDuration(elapsed))
                    .font(.system(size: 64, weight: .thin, design: .monospaced))
                    .foregroundStyle(isRecording ? .white : .white.opacity(0.25))
                    .animation(.none, value: elapsed)

                // ── Start / Stop button ───────────────────────────────────
                Button(action: toggleRecording) {
                    ZStack {
                        Circle()
                            .fill(isRecording ? recordRed : accentColour)
                            .frame(width: 96, height: 96)

                        if isRecording {
                            // Stop: solid square
                            RoundedRectangle(cornerRadius: 4)
                                .fill(Color.white)
                                .frame(width: 28, height: 28)
                        } else {
                            // Start: triangle (play)
                            Image(systemName: "play.fill")
                                .font(.system(size: 32))
                                .foregroundStyle(.white)
                                .offset(x: 3)
                        }
                    }
                }
                .disabled(!isConnected)
                .opacity(isConnected ? 1 : 0.4)
                .accessibilityLabel(isRecording ? "Stop recording" : "Start recording")

                // ── Session metadata ──────────────────────────────────────
                if let session {
                    VStack(spacing: 6) {
                        metaRow(label: "Device",  value: session.deviceName)
                        metaRow(label: "Started", value: formattedDate(session.startTimestamp))
                    }
                    .padding(16)
                    .background(
                        RoundedRectangle(cornerRadius: 12)
                            .fill(Color.white.opacity(0.05))
                    )
                    .padding(.horizontal, 24)
                }
            }
        }
    }

    // MARK: - Actions

    private func toggleRecording() {
        if isRecording {
            stopRecording()
        } else {
            startRecording()
        }
    }

    private func startRecording() {
        guard let newSession = ble.startRecording() else { return }
        session = newSession
        elapsed = 0
        displayTimer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { _ in
            elapsed = Date().timeIntervalSince(newSession.startTimestamp)
        }
    }

    private func stopRecording() {
        ble.stopRecording()
        displayTimer?.invalidate()
        displayTimer = nil
        session = nil
        elapsed = 0
    }

    // MARK: - Formatting helpers

    private func formattedDuration(_ interval: TimeInterval) -> String {
        let totalSeconds = Int(interval)
        let hours   = totalSeconds / 3600
        let minutes = (totalSeconds % 3600) / 60
        let seconds = totalSeconds % 60
        if hours > 0 {
            return String(format: "%02d:%02d:%02d", hours, minutes, seconds)
        }
        return String(format: "%02d:%02d", minutes, seconds)
    }

    private func formattedDate(_ date: Date) -> String {
        let formatter = DateFormatter()
        formatter.dateStyle = .none
        formatter.timeStyle = .medium
        return formatter.string(from: date)
    }

    @ViewBuilder
    private func metaRow(label: String, value: String) -> some View {
        HStack {
            Text(label)
                .font(.caption)
                .foregroundStyle(.white.opacity(0.4))
                .frame(width: 64, alignment: .leading)
            Text(value)
                .font(.caption)
                .foregroundStyle(.white.opacity(0.8))
            Spacer()
        }
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
