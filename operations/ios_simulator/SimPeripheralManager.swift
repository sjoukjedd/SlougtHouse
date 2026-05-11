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

    private var timerECG:  DispatchSourceTimer?  // 100 Hz — master clock, advances generator
    private var timerICG:  DispatchSourceTimer?  // 200 Hz — reads latest
    private var timerPPG:  DispatchSourceTimer?  // 50 Hz  — reads latest
    private var timerSCL:  DispatchSourceTimer?  // 10 Hz  — reads latest
    private var timerIMU:  DispatchSourceTimer?  // 25 Hz  — reads latest
    private var timerTemp: DispatchSourceTimer?  // 1 Hz   — reads latest
    private var timerX:    DispatchSourceTimer?  // 0.5 Hz — activity block
    private var timerY:    DispatchSourceTimer?  // every 30 s — HRV summary
    private var timerR:    DispatchSourceTimer?  // every 5 s  — respiratory rate

    // MARK: Private — X block state
    private var xSeq: UInt16 = 0

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
            (VUAMSUUID.xBlock,  notifyProps),
            (VUAMSUUID.yBlock,  notifyProps),
            (VUAMSUUID.rBlock,  notifyProps),
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
        timerECG = makeTimer(interval: .milliseconds(10)) { [weak self] in
            self?.sendECGPacket()
        }
        timerICG = makeTimer(interval: .milliseconds(5)) { [weak self] in
            self?.sendICGPacket()
        }
        timerPPG = makeTimer(interval: .milliseconds(20)) { [weak self] in
            self?.sendPPGPacket()
        }
        timerSCL = makeTimer(interval: .milliseconds(100)) { [weak self] in
            self?.sendSCLPacket()
        }
        timerIMU = makeTimer(interval: .milliseconds(40)) { [weak self] in
            self?.sendIMUPacket()
        }
        timerTemp = makeTimer(interval: .seconds(1)) { [weak self] in
            self?.sendTempPacket()
        }
        timerX = makeTimer(interval: .seconds(2)) { [weak self] in self?.sendXPacket() }
        timerY = makeTimer(interval: .seconds(30)) { [weak self] in self?.sendYPacket() }
        timerR = makeTimer(interval: .seconds(5))  { [weak self] in self?.sendRPacket() }
    }

    private func stopTimers() {
        [timerECG, timerICG, timerPPG, timerSCL, timerIMU, timerTemp, timerX, timerY, timerR]
            .compactMap { $0 }
            .forEach { $0.cancel() }
        timerECG = nil; timerICG = nil; timerPPG = nil
        timerSCL = nil; timerIMU = nil; timerTemp = nil; timerX = nil; timerY = nil; timerR = nil
    }

    private func makeTimer(interval: DispatchTimeInterval, block: @escaping () -> Void) -> DispatchSourceTimer {
        let t = DispatchSource.makeTimerSource(queue: peripheralQueue)
        t.schedule(deadline: .now() + interval, repeating: interval, leeway: .milliseconds(1))
        t.setEventHandler(handler: block)
        t.resume()
        return t
    }

    // MARK: - Block header

    // 12-byte BlockHeader: type UInt8 | version UInt8 | payloadLength UInt16 LE | timestampUs UInt64 LE
    private func makeHeader(type blockType: UInt8, payloadLength: UInt16) -> Data {
        var header = Data(capacity: 12)
        header.append(blockType)
        header.append(0x01)
        withUnsafeBytes(of: payloadLength.littleEndian) { header.append(contentsOf: $0) }
        let ts = UInt64(Date().timeIntervalSince1970 * 1_000_000)
        withUnsafeBytes(of: ts.littleEndian) { header.append(contentsOf: $0) }
        return header
    }

    // MARK: - Packet builders

    /// A block (0x41) — ECG: advance 10 × 1 ms, send n=10 Int32 samples per channel
    private func sendECGPacket() {
        guard hasSubscribers(for: VUAMSUUID.aBlock) else {
            // Still advance time so other blocks stay in sync
            for _ in 0..<10 { generator.advance(by: 0.001) }
            return
        }
        var ecg1: [Int32] = []
        var ecg2: [Int32] = []
        for _ in 0..<10 {
            let s = generator.advance(by: 0.001)
            ecg1.append(Int32(s.ecgMV * 1000.0))
            ecg2.append(Int32(s.ecgMV * 1000.0))  // single-channel device; mirror on ch2
        }
        let n = UInt16(10)
        let payloadLength = UInt16(2 + 10 * 4 + 10 * 4)   // sampleCount + ecg1 + ecg2
        var payload = makeHeader(type: 0x41, payloadLength: payloadLength)
        withUnsafeBytes(of: n.littleEndian) { payload.append(contentsOf: $0) }
        for v in ecg1 { withUnsafeBytes(of: v.littleEndian) { payload.append(contentsOf: $0) } }
        for v in ecg2 { withUnsafeBytes(of: v.littleEndian) { payload.append(contentsOf: $0) } }
        notify(uuid: VUAMSUUID.aBlock, data: payload)
    }

    /// I block (0x49) — ICG: 6 × Float32 = z0, dZdt, pep, lvet, co, sv
    private func sendICGPacket() {
        guard hasSubscribers(for: VUAMSUUID.iBlock) else { return }
        let s = generator.latest
        let sv: Float = 70.0
        let co = sv * Float(s.hrBPM) / 1000.0
        let fields: [Float] = [
            28.0,
            Float(s.icgOhmPerS),
            90.0,
            290.0,
            co,
            sv,
        ]
        let payloadLength = UInt16(fields.count * 4)
        var payload = makeHeader(type: 0x49, payloadLength: payloadLength)
        for f in fields {
            var mutable = f
            withUnsafeBytes(of: &mutable) { payload.append(contentsOf: $0) }
        }
        notify(uuid: VUAMSUUID.iBlock, data: payload)
    }

    /// P block (0x50) — PPG: UInt32 ppgRed | UInt32 ppgIr | Float32 spo2 | UInt8 hr
    private func sendPPGPacket() {
        guard hasSubscribers(for: VUAMSUUID.pBlock) else { return }
        let s = generator.latest
        let ppgIr  = UInt32(max(0, s.ppgAC) * 500_000)
        let ppgRed = UInt32(max(0, s.ppgAC) * 450_000)
        var spo2   = Float(s.spo2Pct)
        let hr     = UInt8(min(255, max(0, Int(s.hrBPM))))
        let payloadLength = UInt16(4 + 4 + 4 + 1)
        var payload = makeHeader(type: 0x50, payloadLength: payloadLength)
        withUnsafeBytes(of: ppgRed.littleEndian) { payload.append(contentsOf: $0) }
        withUnsafeBytes(of: ppgIr.littleEndian)  { payload.append(contentsOf: $0) }
        withUnsafeBytes(of: &spo2) { payload.append(contentsOf: $0) }
        payload.append(hr)
        notify(uuid: VUAMSUUID.pBlock, data: payload)
    }

    /// S block (0x53) — SCL: Float32 sclTonic | Float32 sclPhasic
    private func sendSCLPacket() {
        guard hasSubscribers(for: VUAMSUUID.sBlock) else { return }
        let s = generator.latest
        var tonic  = Float(s.sclTonic)
        var phasic = Float(s.sclPhasic)
        let payloadLength = UInt16(4 + 4)
        var payload = makeHeader(type: 0x53, payloadLength: payloadLength)
        withUnsafeBytes(of: &tonic)  { payload.append(contentsOf: $0) }
        withUnsafeBytes(of: &phasic) { payload.append(contentsOf: $0) }
        notify(uuid: VUAMSUUID.sBlock, data: payload)
    }

    /// M block (0x4D) — IMU: 9 × Int16 LE (ax ay az gx gy gz mx my mz)
    private func sendIMUPacket() {
        guard hasSubscribers(for: VUAMSUUID.mBlock) else { return }
        let s = generator.latest
        let accelScale = 16384.0 / 9.81
        let gyroScale  = 131.0
        let rawAxes: [Int16] = [
            Int16(clamping: Int(s.ax * accelScale)),
            Int16(clamping: Int(s.ay * accelScale)),
            Int16(clamping: Int(s.az * accelScale)),
            Int16(clamping: Int(s.gx * gyroScale)),
            Int16(clamping: Int(s.gy * gyroScale)),
            Int16(clamping: Int(s.gz * gyroScale)),
            100, -50, 200,   // static mag sim values
        ]
        let payloadLength = UInt16(rawAxes.count * 2)
        var payload = makeHeader(type: 0x4D, payloadLength: payloadLength)
        for v in rawAxes { withUnsafeBytes(of: v.littleEndian) { payload.append(contentsOf: $0) } }
        notify(uuid: VUAMSUUID.mBlock, data: payload)
    }

    /// T block (0x54) — Temperature: Float32 skinTempC | Int16 tempRaw LE
    private func sendTempPacket() {
        guard hasSubscribers(for: VUAMSUUID.tBlock) else { return }
        let s = generator.latest
        var tempF  = Float(s.tempC)
        let tempRaw = Int16(s.tempC / 0.0078125)
        let payloadLength = UInt16(4 + 2)
        var payload = makeHeader(type: 0x54, payloadLength: payloadLength)
        withUnsafeBytes(of: &tempF) { payload.append(contentsOf: $0) }
        withUnsafeBytes(of: tempRaw.littleEndian) { payload.append(contentsOf: $0) }
        notify(uuid: VUAMSUUID.tBlock, data: payload)
    }

    /// X block (0x58) — Activity: 13-byte flat format (no BlockHeader).
    private func sendXPacket() {
        guard hasSubscribers(for: VUAMSUUID.xBlock) else { return }
        let s = generator.latest
        var data = Data(capacity: 13)
        data.append(0x58)                                                           // byte[0]: type
        withUnsafeBytes(of: xSeq.littleEndian) { data.append(contentsOf: $0) }     // bytes[1-2]: seq
        xSeq &+= 1
        let tsUs = UInt32(Date().timeIntervalSince1970 * 1_000_000) & 0xFFFFFFFF
        withUnsafeBytes(of: tsUs.littleEndian) { data.append(contentsOf: $0) }     // bytes[3-6]: timestampUs
        data.append(3)                                                              // byte[7]: activityClass = standing
        withUnsafeBytes(of: UInt16(0).littleEndian) { data.append(contentsOf: $0) } // bytes[8-9]: cadenceSpm = 0
        let motionIntensity = UInt8(min(255, Int(abs(s.az - 9.81) * 26)))
        data.append(motionIntensity)                                                // byte[10]: motionIntensity
        data.append(0)                                                              // byte[11]: speaking = 0
        data.append(0)                                                              // byte[12]: speakingFraction = 0
        notify(uuid: VUAMSUUID.xBlock, data: data)
    }

    /// Y block (0x59) — HRV summary: Float32 rmssdMs | Float32 rriLastMs | Float32 hrEcgBpm | UInt8 beatCount | UInt8 reserved
    private func sendYPacket() {
        guard hasSubscribers(for: VUAMSUUID.yBlock) else { return }
        let s = generator.latest
        let hr = s.hrBPM

        // Simulated RMSSD: base 42 ms + stress variation + noise
        let stressVariation: Double = s.hrBPM > 85.0 ? -12.0 : 0.0
        let noise = Double.random(in: -5...5)
        var rmssdMs = Float(42.0 + stressVariation + noise)

        let rriLastMs = Float(60000.0 / hr)
        var hrEcgBpm  = Float(hr)
        let beatCount: UInt8 = 30
        let reserved:  UInt8 = 0

        let payloadLength: UInt16 = 14  // 4+4+4+1+1
        var payload = makeHeader(type: 0x59, payloadLength: payloadLength)
        withUnsafeBytes(of: &rmssdMs)  { payload.append(contentsOf: $0) }
        var rriCopy = rriLastMs
        withUnsafeBytes(of: &rriCopy)  { payload.append(contentsOf: $0) }
        withUnsafeBytes(of: &hrEcgBpm) { payload.append(contentsOf: $0) }
        payload.append(beatCount)
        payload.append(reserved)
        notify(uuid: VUAMSUUID.yBlock, data: payload)
    }

    /// R block (0x52) — Respiratory rate: 10-byte payload every 5 seconds.
    /// Simulates a stable respiratory rate of ~14 br/min with small noise.
    private func sendRPacket() {
        guard hasSubscribers(for: VUAMSUUID.rBlock) else { return }
        let noise = Float.random(in: -0.5...0.5)
        var rrBpm = Float(14.0) + noise
        let payloadLength: UInt16 = 10
        var payload = makeHeader(type: 0x52, payloadLength: payloadLength)
        // rr_bpm: Float32 LE
        withUnsafeBytes(of: &rrBpm) { payload.append(contentsOf: $0) }
        payload.append(0x01)  // rr_valid = valid
        payload.append(75)    // rr_quality = 75
        payload.append(0x00)  // rr_method = PPG-RIIV
        payload.append(0x00)  // rr_conflict = none
        payload.append(0x00)  // rr_confounder = none (14 br/min is within 12–20)
        payload.append(0x00)  // rr_caution = none
        payload.append(0x00)  // _pad[0]
        payload.append(0x00)  // _pad[1]
        notify(uuid: VUAMSUUID.rBlock, data: payload)
    }

    // MARK: - Helpers

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
                handleControlCommand(byte, fullData: req.value ?? Data())
            }
            peripheral.respond(to: req, withResult: .success)
        }
    }

    func peripheralManagerIsReady(toUpdateSubscribers peripheral: CBPeripheralManager) {
        // BLE flow control: manager was congested, now ready. No special handling needed —
        // we simply drop the next timer-driven update naturally.
    }

    // MARK: Control command dispatch

    private func handleControlCommand(_ byte: UInt8, fullData: Data) {
        switch byte {
        case 0x01: break  // START_RECORDING — timers already running
        case 0x02: break  // STOP_RECORDING — keep streaming for live display
        case 0x03: break  // SYNC_TIME — 8-byte timestamp follows, ignored in sim
        default:   break
        }
    }
}
