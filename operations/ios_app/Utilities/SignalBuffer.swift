import Foundation

/// Thread-safe circular ring buffer for Float signal samples.
final class SignalBuffer {

    private var buffer: [Float]
    private let capacity: Int
    private var writeIndex: Int = 0
    private var count: Int = 0

    /// - Parameters:
    ///   - seconds:    Window size in seconds.
    ///   - sampleRate: Expected samples per second.
    init(seconds: Double, sampleRate: Double) {
        capacity = max(1, Int((seconds * sampleRate).rounded(.up)))
        buffer = [Float](repeating: 0, count: capacity)
    }

    // MARK: - Write

    /// Append a batch of samples, overwriting oldest when full.
    func append(_ samples: [Float]) {
        guard !samples.isEmpty else { return }
        for sample in samples {
            buffer[writeIndex] = sample
            writeIndex = (writeIndex + 1) % capacity
            if count < capacity { count += 1 }
        }
    }

    // MARK: - Read

    /// Returns up to `count` most recent samples in chronological order.
    func latest(count requested: Int) -> [Float] {
        let available = min(requested, count)
        guard available > 0 else { return [] }
        var result = [Float](repeating: 0, count: available)
        // writeIndex points to the next-to-write slot (oldest when full)
        let startSlot = ((writeIndex - available) % capacity + capacity) % capacity
        for i in 0..<available {
            result[i] = buffer[(startSlot + i) % capacity]
        }
        return result
    }

    /// Total number of samples currently held.
    var currentCount: Int {
        return count
    }
}
