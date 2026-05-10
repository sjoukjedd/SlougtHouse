import Foundation
import simd

// MARK: - Parse Error

enum BlockParseError: Error {
    case insufficientData(needed: Int, got: Int)
    case unknownVersion(UInt8)
}

// MARK: - Block Header

struct BlockHeader {
    let type: UInt8
    let version: UInt8
    let payloadLength: UInt16
    let timestampUs: UInt64

    static let wireSize = 12 // 1 + 1 + 2 + 8

    static func parse(from data: Data) throws -> (header: BlockHeader, rest: Data) {
        guard data.count >= wireSize else {
            throw BlockParseError.insufficientData(needed: wireSize, got: data.count)
        }
        let type    = data[0]
        let version = data[1]
        let payloadLength = data.withUnsafeBytes { $0.load(fromByteOffset: 2, as: UInt16.self).littleEndian }
        let timestampUs   = data.withUnsafeBytes { $0.load(fromByteOffset: 4, as: UInt64.self).littleEndian }
        let header = BlockHeader(type: type, version: version,
                                 payloadLength: payloadLength, timestampUs: timestampUs)
        return (header, data.dropFirst(wireSize))
    }
}

// MARK: - A Block (ECG)

struct ABlock {
    let header: BlockHeader
    let ecg1: [Int32]   // 1000 samples/s, lead 1
    let ecg2: [Int32]   // 1000 samples/s, lead 2

    /// Payload layout: sampleCount (UInt16) | ecg1[n] (Int32 each) | ecg2[n] (Int32 each)
    static func parse(from data: Data) throws -> ABlock {
        let (header, rest) = try BlockHeader.parse(from: data)
        guard rest.count >= 2 else {
            throw BlockParseError.insufficientData(needed: 2, got: rest.count)
        }
        let sampleCount = Int(rest.withUnsafeBytes { $0.load(fromByteOffset: 0, as: UInt16.self).littleEndian })
        let needed = 2 + sampleCount * 4 * 2
        guard rest.count >= needed else {
            throw BlockParseError.insufficientData(needed: needed, got: rest.count)
        }
        var ecg1 = [Int32](repeating: 0, count: sampleCount)
        var ecg2 = [Int32](repeating: 0, count: sampleCount)
        rest.withUnsafeBytes { ptr in
            for i in 0..<sampleCount {
                ecg1[i] = ptr.load(fromByteOffset: 2 + i * 4, as: Int32.self).littleEndian
                ecg2[i] = ptr.load(fromByteOffset: 2 + sampleCount * 4 + i * 4, as: Int32.self).littleEndian
            }
        }
        return ABlock(header: header, ecg1: ecg1, ecg2: ecg2)
    }
}

// MARK: - I Block (ICG)

struct IBlock {
    let header: BlockHeader
    let z0: Float       // base impedance (Ω)
    let dZdt: Float     // dZ/dt peak
    let pep: Float      // pre-ejection period (ms)
    let lvet: Float     // left ventricular ejection time (ms)
    let co: Float       // cardiac output (L/min)
    let sv: Float       // stroke volume (mL)

    /// Payload: 6 × Float32 little-endian
    static func parse(from data: Data) throws -> IBlock {
        let (header, rest) = try BlockHeader.parse(from: data)
        let needed = 6 * 4
        guard rest.count >= needed else {
            throw BlockParseError.insufficientData(needed: needed, got: rest.count)
        }
        let floats: [Float] = rest.withUnsafeBytes { ptr in
            (0..<6).map { ptr.load(fromByteOffset: $0 * 4, as: Float.self) }
        }
        return IBlock(header: header,
                      z0: floats[0], dZdt: floats[1],
                      pep: floats[2], lvet: floats[3],
                      co: floats[4], sv: floats[5])
    }
}

// MARK: - M Block (IMU)

struct MBlock {
    let header: BlockHeader
    let accel: SIMD3<Int16>  // raw accelerometer
    let gyro:  SIMD3<Int16>  // raw gyroscope
    let mag:   SIMD3<Int16>  // raw magnetometer

    /// Payload: 9 × Int16 little-endian (ax,ay,az, gx,gy,gz, mx,my,mz)
    static func parse(from data: Data) throws -> MBlock {
        let (header, rest) = try BlockHeader.parse(from: data)
        let needed = 9 * 2
        guard rest.count >= needed else {
            throw BlockParseError.insufficientData(needed: needed, got: rest.count)
        }
        let v: [Int16] = rest.withUnsafeBytes { ptr in
            (0..<9).map { ptr.load(fromByteOffset: $0 * 2, as: Int16.self).littleEndian }
        }
        return MBlock(header: header,
                      accel: SIMD3(v[0], v[1], v[2]),
                      gyro:  SIMD3(v[3], v[4], v[5]),
                      mag:   SIMD3(v[6], v[7], v[8]))
    }
}

// MARK: - P Block (PPG)

struct PBlock {
    let header: BlockHeader
    let ppgRed:  UInt32
    let ppgIr:   UInt32
    let spo2Pct: Float
    let hrPpg:   UInt8

    /// Payload: UInt32, UInt32, Float32, UInt8
    static func parse(from data: Data) throws -> PBlock {
        let (header, rest) = try BlockHeader.parse(from: data)
        let needed = 4 + 4 + 4 + 1
        guard rest.count >= needed else {
            throw BlockParseError.insufficientData(needed: needed, got: rest.count)
        }
        return rest.withUnsafeBytes { ptr in
            let ppgRed  = ptr.load(fromByteOffset: 0, as: UInt32.self).littleEndian
            let ppgIr   = ptr.load(fromByteOffset: 4, as: UInt32.self).littleEndian
            let spo2    = ptr.load(fromByteOffset: 8, as: Float.self)
            let hr      = ptr.load(fromByteOffset: 12, as: UInt8.self)
            return PBlock(header: header, ppgRed: ppgRed, ppgIr: ppgIr,
                          spo2Pct: spo2, hrPpg: hr)
        }
    }
}

// MARK: - S Block (SCL)

struct SBlock {
    let header: BlockHeader
    let sclTonic:   Float
    let sclPhasic:  Float

    /// Payload: 2 × Float32
    static func parse(from data: Data) throws -> SBlock {
        let (header, rest) = try BlockHeader.parse(from: data)
        let needed = 2 * 4
        guard rest.count >= needed else {
            throw BlockParseError.insufficientData(needed: needed, got: rest.count)
        }
        return rest.withUnsafeBytes { ptr in
            SBlock(header: header,
                   sclTonic:  ptr.load(fromByteOffset: 0, as: Float.self),
                   sclPhasic: ptr.load(fromByteOffset: 4, as: Float.self))
        }
    }
}

// MARK: - T Block (Temperature)

struct TBlock {
    let header: BlockHeader
    let skinTempC: Float
    let tempRaw:   Int16

    /// Payload: Float32, Int16
    static func parse(from data: Data) throws -> TBlock {
        let (header, rest) = try BlockHeader.parse(from: data)
        let needed = 4 + 2
        guard rest.count >= needed else {
            throw BlockParseError.insufficientData(needed: needed, got: rest.count)
        }
        return rest.withUnsafeBytes { ptr in
            TBlock(header: header,
                   skinTempC: ptr.load(fromByteOffset: 0, as: Float.self),
                   tempRaw:   ptr.load(fromByteOffset: 4, as: Int16.self).littleEndian)
        }
    }
}
