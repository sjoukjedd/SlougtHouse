import SwiftUI
import SwiftData

// MARK: - Tab selection

private enum AppTab {
    case connect, live, record, dashboard, activity, stress
}

// MARK: - ContentView

/// Root view — five-tab layout: Connect / Live / Record / Dashboard / Activiteit.
struct ContentView: View {

    @Environment(BLEManager.self) var ble
    @State private var selectedTab: AppTab = .connect
    @State private var showDisconnectBanner: Bool = false

    private let accentColour = Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0)

    private var isConnected: Bool {
        if case .connected = ble.connectionState { return true }
        return false
    }

    var body: some View {
        TabView(selection: $selectedTab) {
            Tab("Connect", systemImage: "antenna.radiowaves.left.and.right", value: AppTab.connect) {
                ConnectionView()
            }
            Tab("Live", systemImage: "waveform.path.ecg", value: AppTab.live) {
                LiveWaveformView()
            }
            Tab("Record", systemImage: "record.circle", value: AppTab.record) {
                RecordingControlView()
            }
            Tab("Dashboard", systemImage: "gauge.with.dots.needle.67percent", value: AppTab.dashboard) {
                SignalDashboardView()
            }
            Tab("Activiteit", systemImage: "figure.walk", value: AppTab.activity) {
                ActivityView()
            }
            Tab("Stress", systemImage: "brain.head.profile", value: AppTab.stress) {
                StressView()
            }
        }
        .tint(accentColour)
        .overlay(alignment: .top) {
            if showDisconnectBanner {
                DisconnectBannerView()
                    .transition(.move(edge: .top).combined(with: .opacity))
                    .zIndex(1)
            }
        }
        .animation(.easeInOut(duration: 0.35), value: showDisconnectBanner)
        .onChange(of: ble.connectionState) { _, newState in
            if case .disconnected = newState {
                // Only show banner when we were previously connected
                // (lastDisconnectReason is set on unexpected disconnects)
                if ble.lastDisconnectReason != nil {
                    showDisconnectBanner = true
                }
            } else if isConnected {
                showDisconnectBanner = false
            }
        }
        .task(id: showDisconnectBanner) {
            guard showDisconnectBanner else { return }
            try? await Task.sleep(for: .seconds(4))
            showDisconnectBanner = false
        }
    }
}

// MARK: - DisconnectBannerView

private struct DisconnectBannerView: View {

    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: "wifi.slash")
                .font(.subheadline.bold())
            Text("Verbinding verloren")
                .font(.subheadline.bold())
            Spacer()
        }
        .foregroundStyle(.white)
        .padding(.horizontal, 16)
        .padding(.vertical, 12)
        .background(
            Color(red: 0xC0/255.0, green: 0x39/255.0, blue: 0x1C/255.0)
                .ignoresSafeArea(edges: .top)
        )
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

    @Environment(BLEManager.self) var ble

    @State private var session: RecordingSession?
    @State private var elapsed: TimeInterval = 0
    @State private var displayTimer: Timer?
    @State private var electrodDistanceText: String = ""

    private let background    = Color(red: 0x08/255.0, green: 0x0C/255.0, blue: 0x1E/255.0)
    private let accentColour  = Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0)
    private let recordRed     = Color(red: 0xE5/255.0, green: 0x1D/255.0, blue: 0x35/255.0)

    private var isRecording: Bool { session != nil }

    /// Parsed electrode distance — nil if empty or out of [5, 50] range.
    private var parsedDistance: Float? {
        guard let v = Float(electrodDistanceText.replacingOccurrences(of: ",", with: ".")) else { return nil }
        return (5...50).contains(v) ? v : nil
    }

    private var distanceOutOfRange: Bool {
        let raw = electrodDistanceText.replacingOccurrences(of: ",", with: ".")
        guard !raw.isEmpty, let v = Float(raw) else { return false }
        return !(5...50).contains(v)
    }

    private var canStart: Bool {
        isConnected && parsedDistance != nil
    }

    private var isConnected: Bool {
        if case .connected = ble.connectionState { return true }
        return false
    }

    var body: some View {
        @Bindable var bleBindable = ble
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

                // ── Subject + electrode inputs (hidden while recording) ───
                if !isRecording {
                    VStack(spacing: 16) {
                        // Subject ID
                        VStack(alignment: .leading, spacing: 4) {
                            TextField("Subject ID", text: $bleBindable.subjectId)
                                .font(.body)
                                .foregroundStyle(.white)
                                .padding(.bottom, 6)
                                .overlay(alignment: .bottom) {
                                    Rectangle()
                                        .fill(accentColour)
                                        .frame(height: 1.5)
                                }
                                .autocorrectionDisabled()
                                .textInputAutocapitalization(.characters)
                        }

                        // Electrode distance
                        VStack(alignment: .leading, spacing: 4) {
                            TextField("Afstand elektroden (cm)", text: $electrodDistanceText)
                                .font(.body)
                                .foregroundStyle(.white)
                                .keyboardType(.decimalPad)
                                .padding(.bottom, 6)
                                .overlay(alignment: .bottom) {
                                    Rectangle()
                                        .fill(distanceOutOfRange ? Color.red : accentColour)
                                        .frame(height: 1.5)
                                }
                                .onChange(of: electrodDistanceText) {
                                    ble.electrodDistanceCm = parsedDistance
                                }
                            if distanceOutOfRange {
                                Text("Waarde moet tussen 5 en 50 cm liggen")
                                    .font(.caption2)
                                    .foregroundStyle(.red)
                            }
                        }
                    }
                    .padding(.horizontal, 32)
                }

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
                .disabled(isRecording ? false : !canStart)
                .opacity((isRecording || canStart) ? 1 : 0.4)
                .accessibilityLabel(isRecording ? "Stop recording" : "Start recording")

                // ── Session metadata ──────────────────────────────────────
                if let session {
                    VStack(spacing: 6) {
                        MetaRowView(label: "Device",   value: session.deviceName)
                        if !session.subjectId.isEmpty {
                            MetaRowView(label: "Subject",  value: session.subjectId)
                        }
                        if let d = session.electrodDistanceCm {
                            MetaRowView(label: "L (cm)",   value: String(format: "%.1f", d))
                        }
                        MetaRowView(label: "Started",  value: formattedDate(session.startTimestamp))
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
        date.formatted(date: .omitted, time: .standard)
    }
}

// MARK: - Meta Row

private struct MetaRowView: View {
    let label: String
    let value: String

    var body: some View {
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

    @Environment(BLEManager.self) var ble

    private let background = Color(red: 0x08/255.0, green: 0x0C/255.0, blue: 0x1E/255.0)

    var body: some View {
        ZStack {
            background.ignoresSafeArea()
            VStack(spacing: 2) {
                ChannelRowView(title: "ECG",
                               buffer: ble.ecg1Buffer,
                               color: .green,
                               yRange: -1500...1500)

                ChannelRowView(title: "ICG dZ/dt",
                               buffer: ble.icgBuffer,
                               color: Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0),
                               yRange: -2...2)

                ChannelRowView(title: "SCL",
                               buffer: ble.sclBuffer,
                               color: .yellow,
                               yRange: 0...25)

                ChannelRowView(title: "PPG-IR",
                               buffer: ble.ppgBuffer,
                               color: .red,
                               yRange: 400_000...600_000)

                if ble.latestRBlock?.rrValid == true {
                    ChannelRowView(title: "RESP",
                                   buffer: ble.respBuffer,
                                   color: Color(red: 0.0, green: 0.80, blue: 0.70))
                }
            }
            .padding(.vertical, 8)
        }
    }
}

// MARK: - Channel Row

private struct ChannelRowView: View {
    let title: String
    let buffer: SignalBuffer
    let color: Color
    var yRange: ClosedRange<Float>? = nil

    var body: some View {
        // Compute the effective y-range once per render, outside the path loop.
        // When no explicit range is provided, derive it from the buffer contents
        // with 10 % padding and a minimum span of ±10 to avoid division by zero.
        let effectiveRange: ClosedRange<Float> = {
            if let r = yRange { return r }
            let samples = buffer.latest(count: buffer.currentCount)
            guard !samples.isEmpty,
                  let lo = samples.min(), let hi = samples.max() else {
                return -10...10
            }
            let span = hi - lo
            let padding = max(span * 0.1, 1.0)   // 10 % of range; at least 1 unit
            let lower = lo - padding
            let upper = hi + padding
            // Ensure minimum total span of 20 (±10 equivalent)
            if upper - lower < 20 {
                let mid = (lower + upper) / 2
                return (mid - 10)...(mid + 10)
            }
            return lower...upper
        }()

        VStack(alignment: .leading, spacing: 0) {
            Text(title)
                .font(.caption2)
                .foregroundStyle(.white.opacity(0.5))
                .padding(.leading, 8)
            WaveformView(buffer: buffer, color: color, yRange: effectiveRange)
                .frame(maxWidth: .infinity)
                .frame(height: 130)
        }
    }
}

#Preview {
    let container = try! ModelContainer(for: CSIBaselineRecord.self,
                                        configurations: ModelConfiguration(isStoredInMemoryOnly: true))
    return ContentView()
        .environment(BLEManager(modelContext: container.mainContext))
}
