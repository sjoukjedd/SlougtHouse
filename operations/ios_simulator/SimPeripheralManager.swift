import Foundation
import CoreBluetooth

// MARK: - Peripheral Manager
// Advertises as "VU-AMS-SIM" and streams synthetic physiological data over BLE.
// All CBPeripheralManagerDelegate calls arrive on peripheralQueue (serial).

protocol SimPeripheralManagerDelegate: AnyObject {
    func peripheralManagerDidUpdateState(_ state: CBManagerState)
    func peripheralManagerDidStartAdvertising(error: Error?)
    func peripheralManagerDidConnect(centralCount: Int)
    func peripheralManagerDidDisconnect(centralCount: Int)
}

final class SimPeripheralManager: NSObject {

    // MARK: Public

    weak var delegate: SimPeripheralManagerDelegate?

    private(set) var isAdvertising: Bool = false
    private(set) var connectedCentralCount: Int = 0

    var config: SignalGenerator.Config {
        get { generator.config }
        set { generator.config = newValue }
    }

    // MARK: Private — BLE

    private var peripheral: CBPeripheralManager!
    private let peripheralQueue = DispatchQueue(label: "com.vuams.sim.peripheral", qos: .userInitiated)

    private var characteristics: [CBUUID: CBMutableCharacteristic] = [:]
    private var subscribedCentrals: [CBUUID: Set<CBCentral>] = [:]

    // MARK: Private — Signal generation

    private var generator = SignalGenerator()

    // MARK: Private — Timers (all fire on peripheralQueue)

    private var timerECG: DispatchSourceTimer?   // 250 Hz, 4 samples/notify → 62.5 Hz notify rate
    private var timerICG: DispatchSourceTimer?   // 200 Hz, 4 samples/notify → 50 Hz
    private var timerPPG: DispatchSourceTimer?   // 50 Hz
    private var timerSCLPhasic: DispatchSourceTimer?  // 16 Hz
    private var timerSCLTonic: DispatchSourceTimer?   // 1 Hz
    private var timerIMU: DispatchSourceTimer?   // 25 Hz

    // Delta encoding state (last transmitted 10-bit / 8-bit value)
    private var lastECGRaw: Int16 = 0
    private var lastICGRaw: Int16 = 0
    private var lastPPGRaw: Int16 = 0
    private var lastIMURaw: (Int8, Int8, Int8) = (0, 0, 0)

    // MARK: Init / deinit

    override init() {
        super.init()
        peripheral = CBPeripheralManager(delegate: self, queue: peripheralQueue)
    }

    deinit {
        stopAdvertising()
    }

    // MARK: - Public API

    func startAdvertising() {
        peripheralQueue.async { [weak self] in
            guard let self else { return }
            guard self.peripheral.state == .poweredOn else { return }
            self.buildGATTServiceIfNeeded()
            let advertisementData: [String: Any] = [
                CBAdvertisementDataLocalNameKey: "VU-AMS-SIM",
                CBAdvertisementDataServiceUUIDsKey: [VUAMSUUID.service]
            ]
            self.peripheral.startAdvertising(advertisementData)
        }
    }

    func stopAdvertising() {
        peripheralQueue.async { [weak self] in
            guard let self else { return }
            self.stopTimers()
            self.peripheral.stopAdvertising()
            self.isAdvertising = false
        }
    }

    // MARK: - GATT Setup

    private var serviceAdded = false

    private func buildGATTServiceIfNeeded() {
        guard !serviceAdded else { return }
        serviceAdded = true

        let notifyProps: CBCharacteristicProperties = [.notify]
        let writeProps: CBCharacteristicProperties  = [.write, .writeWithoutResponse]
        let readProps: CBCharacteristicProperties   = [.read, .notify]
        let permissions: CBAttributePermissions     = [.readable, .writeable]

        let uuids: [(CBUUID, CBCharacteristicProperties)] = [
            (VUAMSUUID.aBlock,  notifyProps),
            (VUAMSUUID.iBlock,  notifyProps),
            (VUAMSUUID.mBlock,  notifyProps),
            (VUAMSUUID.pBlock,  notifyProps),
            (VUAMSUUID.sBlock,  notifyProps),
            (VUAMSUUID.tBlock,  readProps),
            (VUAMSUUID.status,  readProps),
            (VUAMSUUID.control, writeProps),
        ]

        let service = CBMutableService(type: VUAMSUUID.service, primary: true)
        var chars: [CBMutableCharacteristic] = []

        for (uuid, props) in uuids {
            let char = CBMutableCharacteristic(
                type: uuid,
                properties: props,
                value: nil,
                permissions: permissions
            )
            characteristics[uuid] = char
            chars.append(char)
            subscribedCentrals[uuid] = []
        }
        service.characteristics = chars
        peripheral.add(service)
    }

    // MARK: - Timers

    private func startTimers() {
        // ECG 250 Hz — send 4 samples per packet → fire every 16 ms (62.5 Hz)
        timerECG = makeTimer(interval: .milliseconds(16)) { [weak self] in
            self?.sendECGPacket()
        }
        // ICG 200 Hz — send 4 samples per packet → fire every 20 ms
        timerICG = makeTimer(interval: .milliseconds(20)) { [weak self] in
            self?.sendICGPacket()
        }
        // PPG 50 Hz
        timerPPG = makeTimer(interval: .milliseconds(20)) { [weak self] in
            self?.sendPPGSample()
        }
        // SCL phasic 16 Hz
        timerSCLPhasic = makeTimer(interval: .milliseconds(62)) { [weak self] in
            self?.sendSCLPhasicSample()
        }
        // SCL tonic + temp 1 Hz
        timerSCLTonic = makeTimer(interval: .seconds(1)) { [weak self] in
            self?.sendSCLTonicAndTemp()
        }
        // IMU 25 Hz
        timerIMU = makeTimer(interval: .milliseconds(40)) { [weak self] in
            self?.sendIMUSample()
        }
    }

    private func stopTimers() {
        [timerECG, timerICG, timerPPG, timerSCLPhasic, timerSCLTonic, timerIMU]
            .compactMap { $0 }
            .forEach { $0.cancel() }
        timerECG = nil; timerICG = nil; timerPPG = nil
        timerSCLPhasic = nil; timerSCLTonic = nil; timerIMU = nil
    }

    private func makeTimer(interval: DispatchTimeInterval, block: @escaping () -> Void) -> DispatchSourceTimer {
        let t = DispatchSource.makeTimerSource(queue: peripheralQueue)
        t.schedule(deadline: .now() + interval, repeating: interval, leeway: .milliseconds(1))
        t.setEventHandler(handler: block)
        t.resume()
        return t
    }

    // MARK: - Packet builders

    /// ECG: 4 × 10-bit delta-encoded samples packed into 5 bytes + 1-byte header
    private func sendECGPacket() {
        guard hasSubscribers(for: VUAMSUUID.aBlock) else { return }
        var payload = Data(capacity: 9)
        let dt = 1.0 / 250.0
        for _ in 0..<4 {
            let s = generator.advance(by: dt)
            let raw = clamp10(Int(s.ecgMV * 512))     // ±512 → 10-bit signed → map to 0-1023
            let delta = Int16(raw) - lastECGRaw
            lastECGRaw = Int16(raw)
            appendDelta10(&payload, delta: delta)
        }
        notify(uuid: VUAMSUUID.aBlock, data: payload)
    }

    /// ICG: 4 × 10-bit delta-encoded
    private func sendICGPacket() {
        guard hasSubscribers(for: VUAMSUUID.iBlock) else { return }
        var payload = Data(capacity: 9)
        let dt = 1.0 / 200.0
        for _ in 0..<4 {
            let s = generator.advance(by: dt)
            let raw = clamp10(Int(s.icgOhmPerS * 256))
            let delta = Int16(raw) - lastICGRaw
            lastICGRaw = Int16(raw)
            appendDelta10(&payload, delta: delta)
        }
        notify(uuid: VUAMSUUID.iBlock, data: payload)
    }

    /// PPG: 1 sample, 10-bit delta + 1-byte SpO2 (×2 = 0-200)
    private func sendPPGSample() {
        guard hasSubscribers(for: VUAMSUUID.pBlock) else { return }
        let s = generator.advance(by: 1.0 / 50.0)
        var payload = Data(capacity: 4)
        let raw = clamp10(Int(s.ppgAC * 512))
        let delta = Int16(raw) - lastPPGRaw
        lastPPGRaw = Int16(raw)
        appendDelta10(&payload, delta: delta)
        let spo2Byte = UInt8(min(200, max(0, Int(s.spo2Pct * 2))))
        payload.append(spo2Byte)
        notify(uuid: VUAMSUUID.pBlock, data: payload)
    }

    /// SCL phasic: 16-bit fixed point, µS × 1000 → Int16 (range 0..30000)
    private func sendSCLPhasicSample() {
        guard hasSubscribers(for: VUAMSUUID.sBlock) else { return }
        let s = generator.advance(by: 1.0 / 16.0)
        let raw = UInt16(min(30000, max(0, Int(s.sclUS * 1000))))
        var payload = Data(capacity: 2)
        withUnsafeBytes(of: raw.littleEndian) { payload.append(contentsOf: $0) }
        notify(uuid: VUAMSUUID.sBlock, data: payload)
    }

    /// SCL tonic (4-byte float) + temperature (4-byte float) in a single tBlock packet
    private func sendSCLTonicAndTemp() {
        // We don't advance the generator here; just read the last advance's result
        // via a zero-advance snapshot (temp and tonic are slow-changing)
        let s = generator.advance(by: 0.0)
        var payload = Data(capacity: 8)
        var tonic = Float(s.sclUS)
        var temp  = Float(s.tempC)
        withUnsafeBytes(of: &tonic) { payload.append(contentsOf: $0) }
        withUnsafeBytes(of: &temp)  { payload.append(contentsOf: $0) }
        notify(uuid: VUAMSUUID.tBlock, data: payload)
    }

    /// IMU: 6 × 8-bit delta (ax ay az gx gy gz)
    private func sendIMUSample() {
        guard hasSubscribers(for: VUAMSUUID.mBlock) else { return }
        let s = generator.advance(by: 1.0 / 25.0)
        var payload = Data(capacity: 6)
        let axes: [Double] = [s.ax, s.ay, s.az, s.gx, s.gy, s.gz]
        // Scale: accel ±2g → ±127, gyro ±250 dps → ±127
        let scales: [Double] = [63.5, 63.5, 63.5, 0.508, 0.508, 0.508]
        for (val, scale) in zip(axes, scales) {
            let b = Int8(min(127, max(-128, Int(val * scale))))
            payload.append(UInt8(bitPattern: b))
        }
        notify(uuid: VUAMSUUID.mBlock, data: payload)
    }

    // MARK: - Helpers

    private func appendDelta10(_ data: inout Data, delta: Int16) {
        // Pack delta as signed 10-bit clamped to [-511,511], stored as 2 bytes little-endian
        let clamped = Int16(min(511, max(-512, Int(delta))))
        withUnsafeBytes(of: clamped.littleEndian) { data.append(contentsOf: $0) }
    }

    private func clamp10(_ v: Int) -> Int {
        return min(1023, max(0, v + 512))   // shift signed to unsigned 10-bit
    }

    private func hasSubscribers(for uuid: CBUUID) -> Bool {
        return !(subscribedCentrals[uuid]?.isEmpty ?? true)
    }

    private func notify(uuid: CBUUID, data: Data) {
        guard let char = characteristics[uuid] else { return }
        _ = peripheral.updateValue(data, for: char, onSubscribedCentrals: nil)
    }
}

// MARK: - CBPeripheralManagerDelegate

extension SimPeripheralManager: CBPeripheralManagerDelegate {

    func peripheralManagerDidUpdateState(_ peripheral: CBPeripheralManager) {
        DispatchQueue.main.async { [weak self] in
            self?.delegate?.peripheralManagerDidUpdateState(peripheral.state)
        }
    }

    func peripheralManagerDidStartAdvertising(_ peripheral: CBPeripheralManager, error: Error?) {
        isAdvertising = (error == nil)
        if error == nil { startTimers() }
        DispatchQueue.main.async { [weak self] in
            self?.delegate?.peripheralManagerDidStartAdvertising(error: error)
        }
    }

    func peripheralManager(_ peripheral: CBPeripheralManager,
                           didAdd service: CBService, error: Error?) {
        if let error {
            print("[SimPeripheral] addService error: \(error)")
        }
    }

    func peripheralManager(_ peripheral: CBPeripheralManager,
                           central: CBCentral,
                           didSubscribeTo characteristic: CBCharacteristic) {
        subscribedCentrals[characteristic.uuid]?.insert(central)
        // Count unique centrals across all characteristics
        let allCentrals = subscribedCentrals.values.reduce(into: Set<CBCentral>()) { $0.formUnion($1) }
        connectedCentralCount = allCentrals.count
        DispatchQueue.main.async { [weak self] in
            self?.delegate?.peripheralManagerDidConnect(centralCount: allCentrals.count)
        }
    }

    func peripheralManager(_ peripheral: CBPeripheralManager,
                           central: CBCentral,
                           didUnsubscribeFrom characteristic: CBCharacteristic) {
        subscribedCentrals[characteristic.uuid]?.remove(central)
        let allCentrals = subscribedCentrals.values.reduce(into: Set<CBCentral>()) { $0.formUnion($1) }
        connectedCentralCount = allCentrals.count
        DispatchQueue.main.async { [weak self] in
            self?.delegate?.peripheralManagerDidDisconnect(centralCount: allCentrals.count)
        }
    }

    func peripheralManager(_ peripheral: CBPeripheralManager,
                           didReceiveWrite requests: [CBATTRequest]) {
        for req in requests {
            if req.characteristic.uuid == VUAMSUUID.control,
               let data = req.value,
               let byte = data.first {
                handleControlCommand(byte)
            }
            peripheral.respond(to: req, withResult: .success)
        }
    }

    func peripheralManagerIsReady(toUpdateSubscribers peripheral: CBPeripheralManager) {
        // BLE flow control: manager was congested, now ready. No special handling needed —
        // we simply drop the next timer-driven update naturally.
    }

    // MARK: Control command dispatch

    private func handleControlCommand(_ byte: UInt8) {
        switch byte {
        case 0x01: config.stressMode = true
        case 0x00: config.stressMode = false
        default: break
        }
    }
}
