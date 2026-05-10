import Foundation

/// Decodes delta-compressed sample streams as used in VU-AMS firmware.
///
/// The firmware transmits a Float anchor (first sample absolute value) followed
/// by packed signed deltas.  The decoder reconstructs the original float sequence.
struct DeltaDecoder {

    // MARK: - 10-bit delta decoding

    /// Decode a stream of 10-bit signed deltas (packed two-per-3-bytes, big-endian nibble layout).
    ///
    /// Wire format:  anchor (Float32 LE) | packed 10-bit deltas
    ///
    /// - Parameters:
    ///   - data:   Raw characteristic payload.
    ///   - anchor: Absolute value of the first sample (from header or first field).
    ///   - scale:  LSB scale factor (delta_counts → physical unit).
    /// - Returns:  Decoded samples in physical units.
    static func decode10bit(data: Data, anchor: Float, scale: Float) -> [Float] {
        guard data.count >= 3 else { return [] }
        // Each pair of 10-bit values occupies 3 bytes: [AAAAAAAA | AABBBBBB | BBBBBBXX]
        // We iterate in 3-byte chunks.
        let chunkCount = data.count / 3
        var samples = [Float]()
        samples.reserveCapacity(chunkCount * 2 + 1)
        var accumulator = anchor

        for chunk in 0..<chunkCount {
            let base = chunk * 3
            let b0 = UInt32(data[base])
            let b1 = UInt32(data[base + 1])
            let b2 = UInt32(data[base + 2])

            // Extract two 10-bit values (big-endian bit order)
            let raw0 = (b0 << 2) | (b1 >> 6)          // bits [9:0] of first delta
            let raw1 = ((b1 & 0x3F) << 4) | (b2 >> 4) // bits [9:0] of second delta

            let delta0 = signExtend(raw0, bits: 10)
            let delta1 = signExtend(raw1, bits: 10)

            accumulator += Float(delta0) * scale
            samples.append(accumulator)
            accumulator += Float(delta1) * scale
            samples.append(accumulator)
        }
        return samples
    }

    // MARK: - 8-bit delta decoding

    /// Decode a stream of 8-bit signed deltas (one per byte).
    ///
    /// Wire format: anchor (Float32 LE) | Int8 deltas
    ///
    /// - Parameters:
    ///   - data:   Raw characteristic payload (after stripping the header).
    ///   - anchor: Absolute value of the first sample.
    ///   - scale:  LSB scale factor.
    /// - Returns:  Decoded samples in physical units.
    static func decode8bit(data: Data, anchor: Float, scale: Float) -> [Float] {
        guard !data.isEmpty else { return [] }
        var samples = [Float]()
        samples.reserveCapacity(data.count)
        var accumulator = anchor
        for byte in data {
            let delta = Int8(bitPattern: byte)
            accumulator += Float(delta) * scale
            samples.append(accumulator)
        }
        return samples
    }

    // MARK: - Helpers

    /// Sign-extend a raw unsigned value from `bits` width to a full Int32.
    private static func signExtend(_ value: UInt32, bits: Int) -> Int32 {
        let shift = 32 - bits
        return Int32(bitPattern: value << shift) >> shift
    }
}
