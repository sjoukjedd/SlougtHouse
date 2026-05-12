import Foundation
import CoreBluetooth
import Observation
import SwiftData

// MARK: - Connection State

enum ConnectionState: Equatable {
    case disconnected
    case scanning
    case connecting(CBPeripheral)
    case connected(CBPeripheral)

    static func == (lhs: ConnectionState, rhs: ConnectionState) -> Bool {
        switch (lhs, rhs) {
        case (.disconnected, .disconnected): return true
        case (.scanning, .scanning): return true
        case (.connecting(let a), .connecting(let b)): return a.identifier == b.identifier
        case (.connected(let a), .connected(let b)): return a.identifier == b.identifier
        default: return false
        }
    }

    var displayName: String {
        switch self {
        case .disconnected:      return "Disconnected"
        case .scanning:          return "Scanning…"
        case .connecting(let p): return "Connecting to \(p.name ?? p.identifier.uuidString)"
        case .connected(let p):  return "Connected to \(p.name ?? p.identifier.uuidString)"
        }
    }
}

// MARK: - Discovered Peripheral

struct DiscoveredPeripheral: Identifiable, Equatable {
    let id: UUID
    let peripheral: CBPeripheral
    let name: String
    var rssi: Int

    static func == (lhs: DiscoveredPeripheral, rhs: DiscoveredPeripheral) -> Bool {
        lhs.id == rhs.id
    }
}

// MARK: - BLEManager

@Observable
@MainActor
final class BLEManager: NSObject {

    // Tracked state
    var connectionState: ConnectionState = .disconnected
    var discoveredPeripherals: [DiscoveredPeripheral] = []
    var subjectId: String = ""
    var electrodDistanceCm: Float? = nil
    var lastDisconnectReason: String? = nil
    // High-frequency blocks not consumed by any view — suppress observation tracking.
    // nonisolated(unsafe): written only from bleQueue, never concurrently.
    @ObservationIgnored nonisolated(unsafe) var latestABlock: ABlock?
    var latestIBlock: IBlock?
    @ObservationIgnored nonisolated(unsafe) var latestMBlock: MBlock?
    var latestPBlock: PBlock?           // throttled to 5 Hz by ppgDisplayCounter
    @ObservationIgnored nonisolated(unsafe) var latestSBlock: SBlock?
    var latestTBlock: TBlock?
    @ObservationIgnored nonisolated(unsafe) var latestYBlock: YBlock?
    @ObservationIgnored nonisolated(unsafe) var latestRBlock: RBlock?
    var currentActivity: XBlock?
    var signalQuality: [String: Bool] = [
        "ecg1":       false,
        "ecg2":       false,
        "icg":        false,
        "ppg":        false,
        "scl":        false,
        "temperature": false
    ]

    // Signal buffers (5 s at relevant sample rates)
    // @ObservationIgnored: buffers are mutated internally and polled by a timer,
    // not observed directly by SwiftUI.
    @ObservationIgnored let ecg1Buffer      = SignalBuffer(seconds: 5,  sampleRate: 1000)
    @ObservationIgnored let ecg2Buffer      = SignalBuffer(seconds: 5,  sampleRate: 1000)
    @ObservationIgnored let icgBuffer       = SignalBuffer(seconds: 5,  sampleRate: 200)
    @ObservationIgnored let ppgBuffer       = SignalBuffer(seconds: 5,  sampleRate: 50)
    @ObservationIgnored let sclBuffer       = SignalBuffer(seconds: 5,  sampleRate: 10)
    @ObservationIgnored let respBuffer      = SignalBuffer(seconds: 30, sampleRate: 10)
    // 30-second phasic EDA buffer for SCR peak detection (10 Hz)
    @ObservationIgnored let sclPhasicBuffer = SignalBuffer(seconds: 30, sampleRate: 10)

    // SCR detection — published state consumed by SignalDashboardView
    var scrRatePerMin: Double = 0.0
    var scrActive: Bool = false
    // Internal SCR detector state (not observed by SwiftUI)
    @ObservationIgnored private var scrLastEventTime: Date? = nil
    @ObservationIgnored private var scrEventTimestamps: [Date] = []

    // CSI engine — owns the R-peak detector and 30-second epoch timer.
    // Declared as a stored property (not lazy) so the ModelContext can be
    // injected at init time.
    @ObservationIgnored var csiEngine: CSIEngine!

    // RESP waveform downsampling state — written only from bleQueue.
    @ObservationIgnored nonisolated(unsafe) private var ppgDecimateCounter: Int = 0
    @ObservationIgnored nonisolated(unsafe) private var respMean: Double = 0
    // Throttle latestPBlock SwiftUI updates to 5 Hz (every 10th packet at 50 Hz).
    @ObservationIgnored nonisolated(unsafe) private var ppgDisplayCounter: Int = 0

    // BLE internals
    @ObservationIgnored private var centralManager: CBCentralManager!
    @ObservationIgnored private var connectedPeripheral: CBPeripheral?
    @ObservationIgnored private var characteristics: [CBUUID: CBCharacteristic] = [:]

    // Dedicated queue for all BLE callbacks — keeps high-frequency ECG parsing
    // off the main thread entirely.
    @ObservationIgnored private let bleQueue = DispatchQueue(
        label: "ble.callback",
        qos: .userInitiated
    )

    init(modelContext: ModelContext) {
        super.init()
        csiEngine = CSIEngine(ble: self, modelContext: modelContext)
        centralManager = CBCentralManager(delegate: self, queue: bleQueue)
    }

    // MARK: - Public API

    func startScanning() {
        guard centralManager.state == .poweredOn else { return }
        discoveredPeripherals.removeAll()
        connectionState = .scanning
        centralManager.scanForPeripherals(
            withServices: [VUAMSUUID.service],
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]
        )
    }

    func stopScanning() {
        centralManager.stopScan()
        if case .scanning = connectionState {
            connectionState = .disconnected
        }
    }

    func connect(_ peripheral: CBPeripheral) {
        centralManager.stopScan()
        connectionState = .connecting(peripheral)
        connectedPeripheral = peripheral
        peripheral.delegate = self
        centralManager.connect(peripheral, options: nil)
    }

    func disconnect() {
        guard let p = connectedPeripheral else { return }
        centralManager.cancelPeripheralConnection(p)
    }

    // MARK: - Command helpers

    func sendCommand(_ byte: UInt8) {
        guard let p = connectedPeripheral,
              let ch = characteristics[VUAMSUUID.control] else { return }
        p.writeValue(Data([byte]), for: ch, type: .withResponse)
    }

    /// Sends 0x01 (START_RECORDING) followed immediately by a SYNC_TIME packet.
    /// Returns the RecordingSession that was created so the caller can own it.
    @discardableResult
    func startRecording() -> RecordingSession? {
        guard let p = connectedPeripheral,
              let ch = characteristics[VUAMSUUID.control] else { return nil }
        // 1. START_RECORDING
        p.writeValue(Data([0x01]), for: ch, type: .withResponse)
        // 2. SYNC_TIME — capture wall-clock time as close to the START write as possible
        let session = RecordingSession(
            deviceName: p.name ?? p.identifier.uuidString,
            subjectId: subjectId,
            electrodDistanceCm: electrodDistanceCm
        )
        sendSyncTime(to: p, characteristic: ch, at: session.startTimestamp)
        return session
    }

    /// Sends 0x02 (STOP_RECORDING) to the device.
    func stopRecording() {
        sendCommand(0x02)
    }

    /// Writes command 0x03 + 8-byte little-endian Unix microsecond timestamp.
    func sendSyncTime() {
        guard let p = connectedPeripheral,
              let ch = characteristics[VUAMSUUID.control] else { return }
        sendSyncTime(to: p, characteristic: ch, at: Date())
    }

    // Internal variant that takes an explicit timestamp so startRecording() and
    // sendSyncTime() both anchor to the same moment.
    private func sendSyncTime(to peripheral: CBPeripheral,
                              characteristic: CBCharacteristic,
                              at date: Date) {
        let microseconds = UInt64(date.timeIntervalSince1970 * 1_000_000)
        var payload = Data([0x03])           // SYNC_TIME opcode
        withUnsafeBytes(of: microseconds.littleEndian) { payload.append(contentsOf: $0) }
        peripheral.writeValue(payload, for: characteristic, type: .withResponse)
    }

    // MARK: - SCR detection

    /// Called on every new S-block (10 Hz). Detects upward threshold crossings
    /// with a 5-second refractory period, then recomputes rolling rate over 60 s.
    /// Must be called on MainActor — reads and writes @MainActor-isolated SCR state.
    private func updateSCR(phasic: Float) {
        let threshold: Float = 0.05
        let refractorySeconds: TimeInterval = 5.0
        let rateWindowSeconds: TimeInterval = 60.0
        let now = Date()

        // Fetch the two most recent phasic samples to detect upward crossing
        let recent = sclPhasicBuffer.latest(count: 2)
        if recent.count == 2 {
            let prev = recent[0]
            let curr = recent[1]
            // Upward crossing: prev was below threshold, curr is at or above
            let refractory = scrLastEventTime.map { now.timeIntervalSince($0) < refractorySeconds } ?? false
            if prev < threshold && curr >= threshold && !refractory {
                scrLastEventTime = now
                scrEventTimestamps.append(now)
            }
        }

        // Prune events outside the 60-second rolling window
        scrEventTimestamps = scrEventTimestamps.filter {
            now.timeIntervalSince($0) <= rateWindowSeconds
        }

        // Rolling rate: events in last 60 s, scaled to per-minute
        scrRatePerMin = Double(scrEventTimestamps.count)
            * (60.0 / rateWindowSeconds)

        // scrActive: any event within the last 5 seconds
        scrActive = scrEventTimestamps.contains { now.timeIntervalSince($0) <= 5.0 }
    }

    // MARK: - Private helpers

    private func resetState() {
        connectedPeripheral = nil
        characteristics.removeAll()
        connectionState = .disconnected
        signalQuality = signalQuality.mapValues { _ in false }
        // Reset SCR detector
        scrRatePerMin = 0.0
        scrActive = false
        scrLastEventTime = nil
        scrEventTimestamps.removeAll()
    }
}

// MARK: - CBCentralManagerDelegate
// CBCentralManager is created with bleQueue, so all callbacks arrive on that background queue.
// Delegate methods are nonisolated; they do background-safe work directly and dispatch
// observable state updates to MainActor via Task { @MainActor in }.

extension BLEManager: CBCentralManagerDelegate {

    nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            break // Ready; scanning starts on user request
        default:
            Task { @MainActor [weak self] in
                self?.resetState()
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didDiscover peripheral: CBPeripheral,
                                    advertisementData: [String: Any],
                                    rssi RSSI: NSNumber) {
        let id = peripheral.identifier
        let rssiValue = RSSI.intValue
        Task { @MainActor [weak self] in
            guard let self else { return }
            if let idx = discoveredPeripherals.firstIndex(where: { $0.id == id }) {
                discoveredPeripherals[idx].rssi = rssiValue
            } else {
                let dp = DiscoveredPeripheral(
                    id: id,
                    peripheral: peripheral,
                    name: peripheral.name ?? id.uuidString,
                    rssi: rssiValue
                )
                discoveredPeripherals.append(dp)
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didConnect peripheral: CBPeripheral) {
        Task { @MainActor [weak self] in
            guard let self else { return }
            connectionState = .connected(peripheral)
            lastDisconnectReason = nil
            peripheral.discoverServices([VUAMSUUID.service])
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didFailToConnect peripheral: CBPeripheral,
                                    error: Error?) {
        Task { @MainActor [weak self] in
            self?.resetState()
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didDisconnectPeripheral peripheral: CBPeripheral,
                                    error: Error?) {
        let reason = error?.localizedDescription
        Task { @MainActor [weak self] in
            guard let self else { return }
            if let reason {
                lastDisconnectReason = reason
            }
            resetState()
        }
    }
}

// MARK: - CBPeripheralDelegate

extension BLEManager: CBPeripheralDelegate {

    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didDiscoverServices error: Error?) {
        guard error == nil, let services = peripheral.services else { return }
        Task { @MainActor [weak self] in
            guard let self else { return }
            for service in services where service.uuid == VUAMSUUID.service {
                peripheral.discoverCharacteristics(VUAMSUUID.allUUIDs, for: service)
            }
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didDiscoverCharacteristicsFor service: CBService,
                                error: Error?) {
        guard error == nil, let chars = service.characteristics else { return }
        Task { @MainActor [weak self] in
            guard let self else { return }
            for ch in chars {
                characteristics[ch.uuid] = ch
                if VUAMSUUID.allBlockUUIDs.contains(ch.uuid) ||
                   ch.uuid == VUAMSUUID.status {
                    peripheral.setNotifyValue(true, for: ch)
                }
            }
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didUpdateValueFor characteristic: CBCharacteristic,
                                error: Error?) {
        guard error == nil, let data = characteristic.value else { return }
        let uuid = characteristic.uuid

        do {
            if uuid == VUAMSUUID.aBlock {
                let block = try ABlock.parse(from: data)
                // Buffer appends are thread-safe (SignalBuffer has NSLock)
                ecg1Buffer.append(block.ecg1.map { Float($0) })
                ecg2Buffer.append(block.ecg2.map { Float($0) })
                // Store @ObservationIgnored property — written only from bleQueue, safe
                latestABlock = block
                // Observable UI properties → MainActor
                Task { @MainActor [weak self] in
                    guard let self else { return }
                    signalQuality["ecg1"] = true
                    signalQuality["ecg2"] = true
                }
            } else if uuid == VUAMSUUID.iBlock {
                let block = try IBlock.parse(from: data)
                icgBuffer.append([block.dZdt])
                Task { @MainActor [weak self] in
                    guard let self else { return }
                    latestIBlock = block
                    signalQuality["icg"] = true
                }
            } else if uuid == VUAMSUUID.mBlock {
                let block = try MBlock.parse(from: data)
                // @ObservationIgnored — safe to write on bleQueue
                latestMBlock = block
            } else if uuid == VUAMSUUID.pBlock {
                let block = try PBlock.parse(from: data)
                ppgBuffer.append([Float(block.ppgIr)])
                // Counters are @ObservationIgnored and only touched from bleQueue
                ppgDecimateCounter += 1
                let doResp = ppgDecimateCounter >= 5
                if doResp { ppgDecimateCounter = 0 }
                ppgDisplayCounter += 1
                let doDisplay = ppgDisplayCounter >= 10
                if doDisplay { ppgDisplayCounter = 0 }

                // IIR DC removal for RESP — computed on bleQueue before handing off
                let irValue = Double(block.ppgIr)
                if doResp {
                    respMean = 0.99 * respMean + 0.01 * irValue
                    let respSample = Float(irValue - respMean)
                    respBuffer.append([respSample])
                }

                Task { @MainActor [weak self] in
                    guard let self else { return }
                    signalQuality["ppg"] = true
                    if doDisplay {
                        latestPBlock = block
                    }
                }
            } else if uuid == VUAMSUUID.sBlock {
                let block = try SBlock.parse(from: data)
                // @ObservationIgnored — safe on bleQueue
                latestSBlock = block
                sclBuffer.append([block.sclTonic])
                sclPhasicBuffer.append([block.sclPhasic])
                // updateSCR reads sclPhasicBuffer (thread-safe) and writes
                // @MainActor SCR state — must run on MainActor
                Task { @MainActor [weak self] in
                    guard let self else { return }
                    signalQuality["scl"] = true
                    updateSCR(phasic: block.sclPhasic)
                }
            } else if uuid == VUAMSUUID.tBlock {
                let block = try TBlock.parse(from: data)
                Task { @MainActor [weak self] in
                    guard let self else { return }
                    latestTBlock = block
                    signalQuality["temperature"] = true
                }
            } else if uuid == VUAMSUUID.xBlock {
                if let block = XBlock.parse(from: data) {
                    Task { @MainActor [weak self] in
                        self?.currentActivity = block
                    }
                }
            } else if uuid == VUAMSUUID.yBlock {
                let block = try YBlock.parse(from: data)
                // @ObservationIgnored — safe on bleQueue
                latestYBlock = block
                // csiEngine is @MainActor — dispatch
                Task { @MainActor [weak self] in
                    self?.csiEngine.updateFromYBlock(block)
                }
            } else if uuid == VUAMSUUID.rBlock {
                if let block = RBlock.parse(from: data) {
                    // @ObservationIgnored — safe on bleQueue
                    latestRBlock = block
                    // csiEngine is @MainActor — dispatch
                    Task { @MainActor [weak self] in
                        self?.csiEngine.updateFromRBlock(block)
                    }
                }
            }
            // status characteristic — parse if needed in future
        } catch {
            // Silently drop malformed packets; production would log to telemetry
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didWriteValueFor characteristic: CBCharacteristic,
                                error: Error?) {
        // ACK for control writes — handle errors if needed
    }
}
