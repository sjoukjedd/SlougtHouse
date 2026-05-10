import Foundation
import CoreBluetooth
import Observation

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
    var latestABlock: ABlock?
    var latestIBlock: IBlock?
    var latestMBlock: MBlock?
    var latestPBlock: PBlock?
    var latestSBlock: SBlock?
    var latestTBlock: TBlock?
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
    @ObservationIgnored let ecg1Buffer  = SignalBuffer(seconds: 5, sampleRate: 1000)
    @ObservationIgnored let ecg2Buffer  = SignalBuffer(seconds: 5, sampleRate: 1000)
    @ObservationIgnored let icgBuffer   = SignalBuffer(seconds: 5, sampleRate: 1000)
    @ObservationIgnored let ppgBuffer   = SignalBuffer(seconds: 5, sampleRate: 100)
    @ObservationIgnored let sclBuffer   = SignalBuffer(seconds: 5, sampleRate: 10)

    // BLE internals
    @ObservationIgnored private var centralManager: CBCentralManager!
    @ObservationIgnored private var connectedPeripheral: CBPeripheral?
    @ObservationIgnored private var characteristics: [CBUUID: CBCharacteristic] = [:]

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: .main)
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
            deviceName: p.name ?? p.identifier.uuidString
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

    // MARK: - Private helpers

    private func resetState() {
        connectedPeripheral = nil
        characteristics.removeAll()
        connectionState = .disconnected
        signalQuality = signalQuality.mapValues { _ in false }
    }
}

// MARK: - CBCentralManagerDelegate
// CBCentralManager is created with queue: .main, so all callbacks arrive on the main thread.
// We use nonisolated + MainActor.assumeIsolated to satisfy the protocol while keeping
// the guarantee that we are always on the main actor when touching state.

extension BLEManager: CBCentralManagerDelegate {

    nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        MainActor.assumeIsolated {
            switch central.state {
            case .poweredOn:
                break // Ready; scanning starts on user request
            default:
                resetState()
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didDiscover peripheral: CBPeripheral,
                                    advertisementData: [String: Any],
                                    rssi RSSI: NSNumber) {
        MainActor.assumeIsolated {
            let id = peripheral.identifier
            if let idx = discoveredPeripherals.firstIndex(where: { $0.id == id }) {
                discoveredPeripherals[idx].rssi = RSSI.intValue
            } else {
                let dp = DiscoveredPeripheral(
                    id: id,
                    peripheral: peripheral,
                    name: peripheral.name ?? id.uuidString,
                    rssi: RSSI.intValue
                )
                discoveredPeripherals.append(dp)
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didConnect peripheral: CBPeripheral) {
        MainActor.assumeIsolated {
            connectionState = .connected(peripheral)
            peripheral.discoverServices([VUAMSUUID.service])
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didFailToConnect peripheral: CBPeripheral,
                                    error: Error?) {
        MainActor.assumeIsolated { resetState() }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didDisconnectPeripheral peripheral: CBPeripheral,
                                    error: Error?) {
        MainActor.assumeIsolated { resetState() }
    }
}

// MARK: - CBPeripheralDelegate

extension BLEManager: CBPeripheralDelegate {

    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didDiscoverServices error: Error?) {
        MainActor.assumeIsolated {
            guard error == nil,
                  let services = peripheral.services else { return }
            for service in services where service.uuid == VUAMSUUID.service {
                peripheral.discoverCharacteristics(VUAMSUUID.allUUIDs, for: service)
            }
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didDiscoverCharacteristicsFor service: CBService,
                                error: Error?) {
        MainActor.assumeIsolated {
            guard error == nil,
                  let chars = service.characteristics else { return }
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
        MainActor.assumeIsolated {
            do {
                if uuid == VUAMSUUID.aBlock {
                    let block = try ABlock.parse(from: data)
                    latestABlock = block
                    ecg1Buffer.append(block.ecg1.map { Float($0) })
                    ecg2Buffer.append(block.ecg2.map { Float($0) })
                    signalQuality["ecg1"] = true
                    signalQuality["ecg2"] = true
                } else if uuid == VUAMSUUID.iBlock {
                    let block = try IBlock.parse(from: data)
                    latestIBlock = block
                    icgBuffer.append([block.dZdt])
                    signalQuality["icg"] = true
                } else if uuid == VUAMSUUID.mBlock {
                    let block = try MBlock.parse(from: data)
                    latestMBlock = block
                } else if uuid == VUAMSUUID.pBlock {
                    let block = try PBlock.parse(from: data)
                    latestPBlock = block
                    ppgBuffer.append([Float(block.ppgIr)])
                    signalQuality["ppg"] = true
                } else if uuid == VUAMSUUID.sBlock {
                    let block = try SBlock.parse(from: data)
                    latestSBlock = block
                    sclBuffer.append([block.sclTonic])
                    signalQuality["scl"] = true
                } else if uuid == VUAMSUUID.tBlock {
                    let block = try TBlock.parse(from: data)
                    latestTBlock = block
                    signalQuality["temperature"] = true
                } else if uuid == VUAMSUUID.xBlock {
                    if let block = XBlock.parse(from: data) {
                        currentActivity = block
                    }
                }
                // status characteristic — parse if needed in future
            } catch {
                // Silently drop malformed packets; production would log to telemetry
            }
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didWriteValueFor characteristic: CBCharacteristic,
                                error: Error?) {
        // ACK for control writes — handle errors if needed
    }
}
