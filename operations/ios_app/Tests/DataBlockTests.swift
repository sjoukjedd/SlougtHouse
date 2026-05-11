import Testing
import Foundation
import simd
import SwiftData
@testable import VUAMS

// MARK: - Header helper (mirrors SimPeripheralManager.makeHeader exactly)

private func makeHeader(type blockType: UInt8, payloadLength: UInt16, timestampUs: UInt64 = 0) -> Data {
    var data = Data(capacity: 12)
    data.append(blockType)           // type
    data.append(0x01)                // version
    withUnsafeBytes(of: payloadLength.littleEndian) { data.append(contentsOf: $0) }
    withUnsafeBytes(of: timestampUs.littleEndian)   { data.append(contentsOf: $0) }
    return data
}

// MARK: - ABlock tests

@Suite("ABlock — ECG round-trip")
struct ABlockTests {

    /// Construct exactly what sendECGPacket() produces, verify the parser extracts it correctly.
    @Test func roundTrip() throws {
        let ecg1Values: [Int32] = [100, -200, 300, -400, 500, -600, 700, -800, 900, -1000]
        let ecg2Values: [Int32] = [-50, 150, -250, 350, -450, 550, -650, 750, -850, 950]

        let sampleCount = UInt16(10)
        let payloadLength = UInt16(2 + 10 * 4 + 10 * 4)
        var packet = makeHeader(type: 0x41, payloadLength: payloadLength, timestampUs: 123_456_789)
        withUnsafeBytes(of: sampleCount.littleEndian) { packet.append(contentsOf: $0) }
        for v in ecg1Values { withUnsafeBytes(of: v.littleEndian) { packet.append(contentsOf: $0) } }
        for v in ecg2Values { withUnsafeBytes(of: v.littleEndian) { packet.append(contentsOf: $0) } }

        let block = try ABlock.parse(from: packet)

        #expect(block.header.type == 0x41)
        #expect(block.header.version == 0x01)
        #expect(block.header.payloadLength == payloadLength)
        #expect(block.header.timestampUs == 123_456_789)
        #expect(block.ecg1.count == 10)
        #expect(block.ecg2.count == 10)
        #expect(block.ecg1[0] == ecg1Values[0])
        #expect(block.ecg1[9] == ecg1Values[9])
        #expect(block.ecg2[0] == ecg2Values[0])
        #expect(block.ecg2[9] == ecg2Values[9])
        // Verify all samples
        for i in 0..<10 {
            #expect(block.ecg1[i] == ecg1Values[i])
            #expect(block.ecg2[i] == ecg2Values[i])
        }
    }

    /// Fewer than 12 bytes must throw insufficientData.
    @Test func tooShortForHeader() throws {
        let short = Data(repeating: 0x00, count: 8)
        #expect(throws: BlockParseError.self) {
            try ABlock.parse(from: short)
        }
    }

    /// Exact 12-byte header but no payload → truncated → throws.
    @Test func truncatedPayload() throws {
        // Header claims 82-byte payload but we provide nothing after it.
        let payloadLength = UInt16(2 + 10 * 4 + 10 * 4)
        let headerOnly = makeHeader(type: 0x41, payloadLength: payloadLength)
        #expect(throws: BlockParseError.self) {
            try ABlock.parse(from: headerOnly)
        }
    }

    /// Header + sampleCount present but ECG data truncated → throws.
    @Test func partialPayload() throws {
        let payloadLength = UInt16(2 + 10 * 4 + 10 * 4)
        var packet = makeHeader(type: 0x41, payloadLength: payloadLength)
        withUnsafeBytes(of: UInt16(10).littleEndian) { packet.append(contentsOf: $0) }
        // Only 4 bytes of ECG data, not the full 80
        packet.append(contentsOf: [0x01, 0x00, 0x00, 0x00])
        #expect(throws: BlockParseError.self) {
            try ABlock.parse(from: packet)
        }
    }
}

// MARK: - IBlock tests

@Suite("IBlock — ICG round-trip")
struct IBlockTests {

    /// Mirror the exact field values sendICGPacket() uses for its static constants.
    @Test func roundTrip() throws {
        // Simulate hrBPM=72, icgOhmPerS=1.5 → co = 70*72/1000 = 5.04
        let hrBPM: Float = 72.0
        let icgOhmPerS: Float = 1.5
        let sv: Float = 70.0
        let co = sv * hrBPM / 1000.0

        let fields: [Float] = [28.0, icgOhmPerS, 90.0, 290.0, co, sv]
        let payloadLength = UInt16(fields.count * 4)
        var packet = makeHeader(type: 0x49, payloadLength: payloadLength)
        for f in fields {
            var mutable = f
            withUnsafeBytes(of: &mutable) { packet.append(contentsOf: $0) }
        }

        let block = try IBlock.parse(from: packet)

        #expect(block.header.type == 0x49)
        #expect(block.z0    == 28.0)
        #expect(block.dZdt  == icgOhmPerS)
        #expect(block.pep   == 90.0)
        #expect(block.lvet  == 290.0)
        #expect(block.co    == co)
        #expect(block.sv    == sv)
    }
}

// MARK: - PBlock tests

@Suite("PBlock — PPG round-trip")
struct PBlockTests {

    @Test func roundTrip() throws {
        let ppgRed:  UInt32 = 450_000
        let ppgIr:   UInt32 = 500_000
        var spo2:    Float  = 97.5
        let hr:      UInt8  = 72

        let payloadLength = UInt16(4 + 4 + 4 + 1)
        var packet = makeHeader(type: 0x50, payloadLength: payloadLength)
        withUnsafeBytes(of: ppgRed.littleEndian) { packet.append(contentsOf: $0) }
        withUnsafeBytes(of: ppgIr.littleEndian)  { packet.append(contentsOf: $0) }
        withUnsafeBytes(of: &spo2)               { packet.append(contentsOf: $0) }
        packet.append(hr)

        let block = try PBlock.parse(from: packet)

        #expect(block.header.type == 0x50)
        #expect(block.ppgRed  == ppgRed)
        #expect(block.ppgIr   == ppgIr)
        #expect(block.spo2Pct == spo2)
        #expect(block.hrPpg   == hr)
    }

    /// Exact header, no payload → throws.
    @Test func truncated() throws {
        let headerOnly = makeHeader(type: 0x50, payloadLength: 13)
        #expect(throws: BlockParseError.self) {
            try PBlock.parse(from: headerOnly)
        }
    }
}

// MARK: - SBlock tests

@Suite("SBlock — SCL round-trip")
struct SBlockTests {

    @Test func roundTrip() throws {
        var tonic:  Float = 5.3
        var phasic: Float = 0.8

        let payloadLength = UInt16(4 + 4)
        var packet = makeHeader(type: 0x53, payloadLength: payloadLength)
        withUnsafeBytes(of: &tonic)  { packet.append(contentsOf: $0) }
        withUnsafeBytes(of: &phasic) { packet.append(contentsOf: $0) }

        let block = try SBlock.parse(from: packet)

        #expect(block.header.type == 0x53)
        #expect(block.sclTonic   == tonic)
        #expect(block.sclPhasic  == phasic)
    }

    @Test func truncated() throws {
        let headerOnly = makeHeader(type: 0x53, payloadLength: 8)
        #expect(throws: BlockParseError.self) {
            try SBlock.parse(from: headerOnly)
        }
    }
}

// MARK: - TBlock tests

@Suite("TBlock — Temperature round-trip")
struct TBlockTests {

    @Test func roundTrip() throws {
        let tempC: Double = 36.8
        var tempF  = Float(tempC)
        let tempRaw = Int16(tempC / 0.0078125)   // same formula as sendTempPacket()

        let payloadLength = UInt16(4 + 2)
        var packet = makeHeader(type: 0x54, payloadLength: payloadLength)
        withUnsafeBytes(of: &tempF) { packet.append(contentsOf: $0) }
        withUnsafeBytes(of: tempRaw.littleEndian) { packet.append(contentsOf: $0) }

        let block = try TBlock.parse(from: packet)

        #expect(block.header.type == 0x54)
        #expect(block.skinTempC == Float(tempC))
        #expect(block.tempRaw   == tempRaw)
    }

    @Test func truncated() throws {
        let headerOnly = makeHeader(type: 0x54, payloadLength: 6)
        #expect(throws: BlockParseError.self) {
            try TBlock.parse(from: headerOnly)
        }
    }
}

// MARK: - MBlock tests

@Suite("MBlock — IMU round-trip")
struct MBlockTests {

    @Test func roundTrip() throws {
        // Mirror sendIMUPacket() values: static mag [100, -50, 200]
        // Accel/gyro at zero gives 0 for all scaled axes.
        let rawAxes: [Int16] = [
            0, 0, Int16(clamping: Int(1.0 * 16384.0 / 9.81)), // az = 1 g
            10, -20, 30,  // gyro
            100, -50, 200 // static mag
        ]
        let payloadLength = UInt16(rawAxes.count * 2)
        var packet = makeHeader(type: 0x4D, payloadLength: payloadLength)
        for v in rawAxes { withUnsafeBytes(of: v.littleEndian) { packet.append(contentsOf: $0) } }

        let block = try MBlock.parse(from: packet)

        #expect(block.header.type == 0x4D)
        #expect(block.accel.x == rawAxes[0])
        #expect(block.accel.y == rawAxes[1])
        #expect(block.accel.z == rawAxes[2])
        #expect(block.gyro.x  == rawAxes[3])
        #expect(block.gyro.y  == rawAxes[4])
        #expect(block.gyro.z  == rawAxes[5])
        #expect(block.mag.x   == rawAxes[6])
        #expect(block.mag.y   == rawAxes[7])
        #expect(block.mag.z   == rawAxes[8])
    }

    /// The static mag values sendIMUPacket() hard-codes: 100, -50, 200.
    @Test func staticMagValues() throws {
        let rawAxes: [Int16] = [0, 0, 0, 0, 0, 0, 100, -50, 200]
        var packet = makeHeader(type: 0x4D, payloadLength: UInt16(rawAxes.count * 2))
        for v in rawAxes { withUnsafeBytes(of: v.littleEndian) { packet.append(contentsOf: $0) } }

        let block = try MBlock.parse(from: packet)
        #expect(block.mag == SIMD3<Int16>(100, -50, 200))
    }

    @Test func truncated() throws {
        let headerOnly = makeHeader(type: 0x4D, payloadLength: 18)
        #expect(throws: BlockParseError.self) {
            try MBlock.parse(from: headerOnly)
        }
    }
}

// MARK: - XBlock tests

@Suite("XBlock — HAR/SAD round-trip")
struct XBlockTests {

    // Helper: build the 13-byte wire frame exactly as the simulator would.
    private func makeXPacket(
        seq: UInt16,
        timestampUs: UInt32,
        activityClass: UInt8,
        cadenceRaw: UInt16,   // ×10 fixed-point
        motionIntensity: UInt8,
        speaking: UInt8,
        speakingFraction: UInt8
    ) -> Data {
        var data = Data(capacity: 13)
        data.append(0x58)
        withUnsafeBytes(of: seq.littleEndian)         { data.append(contentsOf: $0) }
        withUnsafeBytes(of: timestampUs.littleEndian) { data.append(contentsOf: $0) }
        data.append(activityClass)
        withUnsafeBytes(of: cadenceRaw.littleEndian)  { data.append(contentsOf: $0) }
        data.append(motionIntensity)
        data.append(speaking)
        data.append(speakingFraction)
        return data
    }

    @Test func roundTrip() {
        // cadenceRaw=850 → cadenceSpm = 85.0; motionRaw=128 → 128/255; speakingFraction=75 → 0.75
        let packet = makeXPacket(
            seq: 42,
            timestampUs: 9_876_543,
            activityClass: 4,    // .walking
            cadenceRaw: 850,
            motionIntensity: 128,
            speaking: 1,
            speakingFraction: 75
        )

        let block = #require(XBlock.parse(from: packet))

        #expect(block.seq            == 42)
        #expect(block.timestampUs    == 9_876_543)
        #expect(block.activityClass  == .walking)
        #expect(block.cadenceSpm     == 85.0)
        #expect(block.motionIntensity == Double(128) / 255.0)
        #expect(block.isSpeaking     == true)
        #expect(block.speakingFraction == 0.75)
    }

    @Test func speakingFalseWhenZero() {
        let packet = makeXPacket(seq: 0, timestampUs: 0, activityClass: 2,
                                 cadenceRaw: 0, motionIntensity: 0,
                                 speaking: 0, speakingFraction: 0)
        let block = #require(XBlock.parse(from: packet))
        #expect(block.isSpeaking == false)
    }

    @Test func unknownActivityClass() {
        // rawValue 99 has no case → should fall back to .unknown
        let packet = makeXPacket(seq: 0, timestampUs: 0, activityClass: 99,
                                 cadenceRaw: 0, motionIntensity: 0,
                                 speaking: 0, speakingFraction: 0)
        let block = #require(XBlock.parse(from: packet))
        #expect(block.activityClass == .unknown)
    }

    @Test func wrongFirstByteReturnsNil() {
        var packet = makeXPacket(seq: 1, timestampUs: 0, activityClass: 0,
                                 cadenceRaw: 0, motionIntensity: 0,
                                 speaking: 0, speakingFraction: 0)
        packet[0] = 0x41   // wrong magic byte
        #expect(XBlock.parse(from: packet) == nil)
    }

    @Test func tooShortReturnsNil() {
        // 12 bytes — one short of the required 13
        let short = Data(repeating: 0x58, count: 12)
        #expect(XBlock.parse(from: short) == nil)
    }

    @Test func exactlyThirteenBytesAccepted() {
        let packet = makeXPacket(seq: 0, timestampUs: 0, activityClass: 0,
                                 cadenceRaw: 0, motionIntensity: 0,
                                 speaking: 0, speakingFraction: 0)
        #expect(packet.count == 13)
        #expect(XBlock.parse(from: packet) != nil)
    }
}

// MARK: - YBlock tests

@Suite("YBlock — HRV summary round-trip")
struct YBlockTests {

    // Build a 26-byte wire frame: 12-byte header + 14-byte payload.
    private func makeYPacket(
        rmssdMs: Float,
        rriLastMs: Float,
        hrEcgBpm: Float,
        beatCount: UInt8,
        reserved: UInt8 = 0,
        type: UInt8 = 0x59
    ) -> Data {
        var data = makeHeader(type: type, payloadLength: 14, timestampUs: 0)
        var rmssd = rmssdMs;    withUnsafeBytes(of: &rmssd)    { data.append(contentsOf: $0) }
        var rri   = rriLastMs;  withUnsafeBytes(of: &rri)      { data.append(contentsOf: $0) }
        var hr    = hrEcgBpm;   withUnsafeBytes(of: &hr)       { data.append(contentsOf: $0) }
        data.append(beatCount)
        data.append(reserved)
        return data
    }

    @Test func yBlockRoundTrip() throws {
        let packet = makeYPacket(rmssdMs: 42.5, rriLastMs: 810.0, hrEcgBpm: 74.1,
                                 beatCount: 30, reserved: 0)

        let block = try YBlock.parse(from: packet)

        #expect(block.header.type       == 0x59)
        #expect(block.header.version    == 0x01)
        #expect(block.header.payloadLength == 14)
        #expect(block.header.timestampUs   == 0)
        #expect(block.rmssdMs   == 42.5)
        #expect(block.rriLastMs == 810.0)
        #expect(abs(block.hrEcgBpm - 74.1) < 0.001)
        #expect(block.beatCount == 30)
    }

    /// 25-byte total packet (1 byte short of the required 26) must throw.
    @Test func yBlockTooShort() throws {
        // Build a correct packet then lop off the last byte.
        let full = makeYPacket(rmssdMs: 0, rriLastMs: 0, hrEcgBpm: 0, beatCount: 0)
        #expect(full.count == 26)
        let short = full.dropLast(1)
        #expect(throws: BlockParseError.self) {
            try YBlock.parse(from: short)
        }
    }

    /// Correct length but type byte 0x58 — parser must throw wrongType.
    @Test func yBlockWrongType() throws {
        let packet = makeYPacket(rmssdMs: 0, rriLastMs: 0, hrEcgBpm: 0, beatCount: 0,
                                 type: 0x58)
        #expect(throws: BlockParseError.self) {
            try YBlock.parse(from: packet)
        }
    }
}

// MARK: - RBlock tests

@Suite("RBlock — Respiratory rate round-trip")
struct RBlockTests {

    // Build a 24-byte wire frame: 12-byte header + 12-byte payload.
    // Payload layout: Float32 rrBpm | UInt8 rrValid | UInt8 rrQuality |
    //                 UInt8 rrMethod | UInt8 rrConflict | UInt8 rrConfounder |
    //                 UInt8 rrCaution | UInt8 pad | UInt8 pad
    // (parser only requires rest.count >= 10, extra pad bytes are harmless)
    private func makeRPacket(
        rrBpmBits: UInt32,
        rrValid: UInt8,
        rrQuality: UInt8,
        rrMethod: UInt8  = 0x00,
        rrConflict: UInt8 = 0x00,
        rrConfounder: UInt8 = 0x00,
        rrCaution: UInt8   = 0x00,
        type: UInt8 = 0x52
    ) -> Data {
        var data = makeHeader(type: type, payloadLength: 12, timestampUs: 0)
        withUnsafeBytes(of: rrBpmBits.littleEndian) { data.append(contentsOf: $0) }
        data.append(rrValid)
        data.append(rrQuality)
        data.append(rrMethod)
        data.append(rrConflict)
        data.append(rrConfounder)
        data.append(rrCaution)
        data.append(0x00)   // pad
        data.append(0x00)   // pad
        return data
    }

    @Test func rBlockRoundTrip() {
        let bpmBits = Float(14.3).bitPattern
        let packet  = makeRPacket(rrBpmBits: bpmBits, rrValid: 0x01, rrQuality: 75)
        let block   = #require(RBlock.parse(from: packet))

        #expect(block.header.type == 0x52)
        #expect(abs(block.rrBpm - 14.3) < 0.01)
        #expect(block.rrValid    == true)
        #expect(block.rrQuality  == 75)
        #expect(block.rrConfounder == false)
    }

    /// rr_valid=0x00, rr_bpm bits = quiet NaN (0x7FC00000).
    @Test func rBlockInvalid() {
        let nanBits: UInt32 = 0x7FC0_0000
        let packet = makeRPacket(rrBpmBits: nanBits, rrValid: 0x00, rrQuality: 0)
        let block  = #require(RBlock.parse(from: packet))

        #expect(block.rrValid == false)
        #expect(block.rrBpm.isNaN)
    }

    /// 21-byte total (1 byte short of the required 22) must return nil.
    @Test func rBlockTooShort() {
        let full = makeRPacket(rrBpmBits: 0, rrValid: 0, rrQuality: 0)
        #expect(full.count == 24)
        let short = full.dropLast(3)   // 24 - 3 = 21
        #expect(RBlock.parse(from: short) == nil)
    }

    /// Correct length but type byte 0x41 — parser must return nil.
    @Test func rBlockWrongType() {
        let packet = makeRPacket(rrBpmBits: 0, rrValid: 0, rrQuality: 0, type: 0x41)
        #expect(RBlock.parse(from: packet) == nil)
    }
}

// MARK: - CSIEngine tests

@Suite("CSIEngine")
struct CSIEngineTests {

    // Full epoch integration tests require Timer injection — deferred.

    // Shared in-memory SwiftData container for CSIEngine tests.
    @MainActor
    private func makeEngine() throws -> CSIEngine {
        let container = try ModelContainer(
            for: CSIBaselineRecord.self,
            configurations: ModelConfiguration(isStoredInMemoryOnly: true)
        )
        let ctx = container.mainContext
        let ble = BLEManager(modelContext: ctx)
        return CSIEngine(ble: ble, modelContext: ctx)
    }

    @Test @MainActor func initialState() throws {
        let engine = try makeEngine()
        #expect(engine.score == nil)
        #expect(engine.isBaselineComplete == false)
        #expect(engine.baselineProgress == 0.0)
        #expect(engine.csiLabel == "CSI-4")
    }

    /// updateFromYBlock with beatCount >= 10 must not crash and must set internal state
    /// (verified indirectly by confirming beatCount < 10 leaves state unchanged while
    /// beatCount >= 10 runs through the acceptance branch without error).
    @Test @MainActor func yBlockStoredWhenBeatCountSufficient() throws {
        let engine = try makeEngine()

        // Build a minimal YBlock with beatCount < 10 — should not update firmwareRMSSD.
        let lowPacket  = makeHeader(type: 0x59, payloadLength: 14)
        var low = lowPacket
        var f0: Float = 30.0; withUnsafeBytes(of: &f0) { low.append(contentsOf: $0) }
        var f1: Float = 700.0; withUnsafeBytes(of: &f1) { low.append(contentsOf: $0) }
        var f2: Float = 65.0; withUnsafeBytes(of: &f2) { low.append(contentsOf: $0) }
        low.append(UInt8(5))   // beatCount < 10
        low.append(UInt8(0))
        let lowBlock = try YBlock.parse(from: low)
        engine.updateFromYBlock(lowBlock)
        // No assertion on private property; just verify no crash and state unchanged.
        #expect(engine.score == nil)

        // beatCount >= 10 — should store firmwareRMSSD.
        var high = makeHeader(type: 0x59, payloadLength: 14)
        var g0: Float = 42.5; withUnsafeBytes(of: &g0) { high.append(contentsOf: $0) }
        var g1: Float = 810.0; withUnsafeBytes(of: &g1) { high.append(contentsOf: $0) }
        var g2: Float = 74.0; withUnsafeBytes(of: &g2) { high.append(contentsOf: $0) }
        high.append(UInt8(10))  // beatCount == 10 → accepted
        high.append(UInt8(0))
        let highBlock = try YBlock.parse(from: high)
        engine.updateFromYBlock(highBlock)
        // No crash; score still nil (no epoch computed yet).
        #expect(engine.score == nil)
    }

    /// After updateFromRBlock with a valid R-block, csiLabel must still be "CSI-4"
    /// because a score requires a completed baseline, which needs epoch computation.
    @Test @MainActor func rBlockLabelTransition() throws {
        let engine = try makeEngine()

        let bpmBits = Float(14.0).bitPattern
        var packet  = makeHeader(type: 0x52, payloadLength: 12)
        withUnsafeBytes(of: bpmBits.littleEndian) { packet.append(contentsOf: $0) }
        packet.append(0x01)  // rrValid
        packet.append(75)    // rrQuality
        packet.append(0x00)  // rrMethod
        packet.append(0x00)  // rrConflict
        packet.append(0x00)  // rrConfounder
        packet.append(0x00)  // rrCaution
        packet.append(0x00)  // pad
        packet.append(0x00)  // pad

        let rBlock = #require(RBlock.parse(from: packet))
        engine.updateFromRBlock(rBlock)

        // Label must remain "CSI-4" — no epoch has fired yet.
        #expect(engine.csiLabel == "CSI-4")
    }

    /// resetBaseline() on a fresh engine leaves all observable state at defaults.
    @Test @MainActor func resetBaselineClears() throws {
        let engine = try makeEngine()
        engine.resetBaseline()

        #expect(engine.isBaselineComplete == false)
        #expect(engine.baselineProgress   == 0.0)
        #expect(engine.score              == nil)
        #expect(engine.csiLabel           == "CSI-4")
    }

    /// baselineProgress on a fresh engine must be in [0, 1].
    @Test @MainActor func baselineProgressInRange() throws {
        let engine = try makeEngine()
        #expect(engine.baselineProgress >= 0.0)
        #expect(engine.baselineProgress <= 1.0)
    }
}
