import SwiftUI
import CoreBluetooth

// MARK: - App ViewModel

@MainActor
final class SimViewModel: ObservableObject, SimPeripheralManagerDelegate {

    // MARK: Published state

    @Published var isAdvertising: Bool = false
    @Published var bleState: CBManagerState = .unknown
    @Published var connectedCentrals: Int = 0
    @Published var stressMode: Bool = false {
        didSet { peripheralManager.config.stressMode = stressMode }
    }

    // Live display values (updated on main)
    @Published var displayHR: Double    = 72.0
    @Published var displaySpO2: Double  = 97.5
    @Published var displaySCL: Double   = 3.2
    @Published var displayTemp: Double  = 33.4

    // ECG waveform ring buffer (3 seconds × 250 Hz = 750 samples)
    @Published var ecgTrace: [Double] = Array(repeating: 0.0, count: 750)

    // MARK: Private

    private let peripheralManager = SimPeripheralManager()
    private var displayTimer: Timer?
    private var localGenerator = SignalGenerator()  // for display-only reads

    // MARK: Init

    init() {
        peripheralManager.delegate = self
        startDisplayTimer()
    }

    // MARK: - Actions

    func toggleAdvertising() {
        if isAdvertising {
            peripheralManager.stopAdvertising()
            isAdvertising = false
        } else {
            peripheralManager.startAdvertising()
        }
    }

    // MARK: - Display timer (independent of BLE, always runs)

    private func startDisplayTimer() {
        // 30 Hz UI refresh
        displayTimer = Timer.scheduledTimer(withTimeInterval: 1.0 / 30.0, repeats: true) { [weak self] _ in
            Task { @MainActor [weak self] in self?.tickDisplay() }
        }
    }

    private func tickDisplay() {
        localGenerator.config.stressMode = stressMode
        let dt = 1.0 / 30.0
        let s = localGenerator.advance(by: dt)

        displayHR   = s.hrBPM
        displaySpO2 = s.spo2Pct
        displaySCL  = s.sclUS
        displayTemp = s.tempC

        // Feed ECG ring buffer at display rate (sub-sampled for Canvas)
        ecgTrace.removeFirst()
        ecgTrace.append(s.ecgMV)
    }

    // MARK: - SimPeripheralManagerDelegate

    nonisolated func peripheralManagerDidUpdateState(_ state: CBManagerState) {
        Task { @MainActor [weak self] in self?.bleState = state }
    }

    nonisolated func peripheralManagerDidStartAdvertising(error: Error?) {
        Task { @MainActor [weak self] in
            self?.isAdvertising = (error == nil)
        }
    }

    nonisolated func peripheralManagerDidConnect(centralCount: Int) {
        Task { @MainActor [weak self] in self?.connectedCentrals = centralCount }
    }

    nonisolated func peripheralManagerDidDisconnect(centralCount: Int) {
        Task { @MainActor [weak self] in self?.connectedCentrals = centralCount }
    }
}

// MARK: - Content View

struct ContentView: View {

    @StateObject private var vm = SimViewModel()

    // Brand colours
    private let bgDeep   = Color(red: 0x10/255.0, green: 0x14/255.0, blue: 0x30/255.0)
    private let accent   = Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0)
    private let cardBG   = Color(red: 0x18/255.0, green: 0x1E/255.0, blue: 0x40/255.0)

    var body: some View {
        ZStack {
            bgDeep.ignoresSafeArea()

            ScrollView {
                VStack(spacing: 20) {
                    headerView
                    bleStatusBanner
                    waveformCard
                    vitalsGrid
                    stressToggleCard
                    advertiseButton
                    Spacer(minLength: 32)
                }
                .padding(.horizontal, 20)
                .padding(.top, 16)
            }
        }
        .preferredColorScheme(.dark)
    }

    // MARK: - Header

    private var headerView: some View {
        VStack(spacing: 4) {
            Text("VU-AMS SIM")
                .font(.system(size: 28, weight: .bold, design: .monospaced))
                .foregroundColor(accent)
            Text("Hardware Simulator · BLE Peripheral")
                .font(.caption)
                .foregroundColor(.white.opacity(0.5))
        }
        .frame(maxWidth: .infinity)
        .padding(.top, 8)
    }

    // MARK: - BLE status

    private var bleStatusBanner: some View {
        HStack(spacing: 10) {
            Circle()
                .fill(bleStateColor)
                .frame(width: 10, height: 10)
            Text(bleStateText)
                .font(.caption)
                .foregroundColor(.white.opacity(0.7))
            Spacer()
            if vm.connectedCentrals > 0 {
                Label("\(vm.connectedCentrals) central\(vm.connectedCentrals == 1 ? "" : "s")",
                      systemImage: "antenna.radiowaves.left.and.right")
                    .font(.caption)
                    .foregroundColor(accent)
            }
        }
        .padding(12)
        .background(cardBG)
        .cornerRadius(12)
    }

    private var bleStateColor: Color {
        switch vm.bleState {
        case .poweredOn:  return vm.isAdvertising ? accent : .green
        case .poweredOff: return .red
        default:          return .orange
        }
    }

    private var bleStateText: String {
        switch vm.bleState {
        case .poweredOn:
            return vm.isAdvertising ? "Advertising as VU-AMS-SIM" : "BLE Ready"
        case .poweredOff:   return "Bluetooth Off"
        case .unauthorized: return "BLE Unauthorized"
        case .unsupported:  return "BLE Unsupported"
        default:            return "Waiting for BLE..."
        }
    }

    // MARK: - ECG Waveform

    private var waveformCard: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("ECG")
                    .font(.caption.weight(.semibold))
                    .foregroundColor(accent)
                Text("3 s · 250 Hz")
                    .font(.caption2)
                    .foregroundColor(.white.opacity(0.4))
            }

            Canvas { context, size in
                let samples = vm.ecgTrace
                guard samples.count > 1 else { return }

                let minV = -0.4, maxV = 1.4
                let range = maxV - minV

                var path = Path()
                for (i, val) in samples.enumerated() {
                    let x = CGFloat(i) / CGFloat(samples.count - 1) * size.width
                    let normalised = (val - minV) / range
                    let y = size.height * (1.0 - CGFloat(normalised))
                    if i == 0 { path.move(to: CGPoint(x: x, y: y)) }
                    else       { path.addLine(to: CGPoint(x: x, y: y)) }
                }

                context.stroke(path,
                               with: .color(accent),
                               style: StrokeStyle(lineWidth: 1.5, lineCap: .round, lineJoin: .round))

                // Zero-line
                let zeroY = size.height * (1.0 - CGFloat((0 - minV) / range))
                var zeroPath = Path()
                zeroPath.move(to: CGPoint(x: 0, y: zeroY))
                zeroPath.addLine(to: CGPoint(x: size.width, y: zeroY))
                context.stroke(zeroPath, with: .color(.white.opacity(0.1)),
                               style: StrokeStyle(lineWidth: 0.5, dash: [4, 4]))
            }
            .frame(height: 120)
            .background(Color.black.opacity(0.3))
            .cornerRadius(8)
        }
        .padding(16)
        .background(cardBG)
        .cornerRadius(16)
    }

    // MARK: - Vitals grid

    private var vitalsGrid: some View {
        LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 12) {
            VitalCard(label: "Heart Rate",
                      value: String(format: "%.0f", vm.displayHR),
                      unit: "bpm",
                      icon: "heart.fill",
                      accent: accent,
                      cardBG: cardBG)
            VitalCard(label: "SpO₂",
                      value: String(format: "%.1f", vm.displaySpO2),
                      unit: "%",
                      icon: "lungs.fill",
                      accent: accent,
                      cardBG: cardBG)
            VitalCard(label: "SCL Tonic",
                      value: String(format: "%.2f", vm.displaySCL),
                      unit: "µS",
                      icon: "waveform.path.ecg",
                      accent: accent,
                      cardBG: cardBG)
            VitalCard(label: "Skin Temp",
                      value: String(format: "%.1f", vm.displayTemp),
                      unit: "°C",
                      icon: "thermometer.medium",
                      accent: accent,
                      cardBG: cardBG)
        }
    }

    // MARK: - Stress toggle

    private var stressToggleCard: some View {
        HStack {
            VStack(alignment: .leading, spacing: 4) {
                Text("Stress Scenario")
                    .font(.subheadline.weight(.semibold))
                    .foregroundColor(.white)
                Text("HR 95 bpm · SCL 6 µS · SCR ×3")
                    .font(.caption)
                    .foregroundColor(.white.opacity(0.5))
            }
            Spacer()
            Toggle("", isOn: $vm.stressMode)
                .labelsHidden()
                .tint(accent)
        }
        .padding(16)
        .background(vm.stressMode ? accent.opacity(0.15) : cardBG)
        .cornerRadius(16)
        .overlay(
            RoundedRectangle(cornerRadius: 16)
                .stroke(vm.stressMode ? accent.opacity(0.5) : Color.clear, lineWidth: 1)
        )
        .animation(.easeInOut(duration: 0.2), value: vm.stressMode)
    }

    // MARK: - Advertise button

    private var advertiseButton: some View {
        Button(action: { vm.toggleAdvertising() }) {
            HStack(spacing: 10) {
                Image(systemName: vm.isAdvertising ? "stop.fill" : "antenna.radiowaves.left.and.right")
                Text(vm.isAdvertising ? "Stop Advertising" : "Start Advertising")
                    .fontWeight(.semibold)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 16)
            .background(vm.isAdvertising ? Color.red.opacity(0.8) : accent)
            .foregroundColor(vm.isAdvertising ? .white : bgDeep)
            .cornerRadius(16)
        }
        .disabled(vm.bleState != .poweredOn)
        .opacity(vm.bleState == .poweredOn ? 1.0 : 0.4)
        .animation(.easeInOut(duration: 0.2), value: vm.isAdvertising)
    }
}

// MARK: - Vital Card

private struct VitalCard: View {
    let label: String
    let value: String
    let unit: String
    let icon: String
    let accent: Color
    let cardBG: Color

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Image(systemName: icon)
                    .foregroundColor(accent)
                    .font(.caption)
                Spacer()
            }
            HStack(alignment: .lastTextBaseline, spacing: 4) {
                Text(value)
                    .font(.system(size: 28, weight: .bold, design: .monospaced))
                    .foregroundColor(.white)
                Text(unit)
                    .font(.caption)
                    .foregroundColor(.white.opacity(0.5))
            }
            Text(label)
                .font(.caption2)
                .foregroundColor(.white.opacity(0.4))
        }
        .padding(14)
        .background(cardBG)
        .cornerRadius(14)
    }
}

// MARK: - Preview

#Preview {
    ContentView()
}
