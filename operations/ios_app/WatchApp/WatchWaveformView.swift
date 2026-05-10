import SwiftUI

/// Renders the last 5 seconds of ECG lead-1 samples as a scrolling waveform.
/// Expects `samples` at 1 000 Hz (up to 5 000 points). Fewer points are drawn
/// proportionally from the left, so the view is always useful even during
/// the initial fill phase.
struct WatchWaveformView: View {

    /// Ring-buffered ECG samples, newest last.
    let samples: [Double]

    // Visual tuning
    private let lineWidth: CGFloat = 1.2
    private let waveColor = Color(red: 0.18, green: 0.85, blue: 0.55) // VU-AMS green

    var body: some View {
        Canvas { context, size in
            guard samples.count >= 2 else {
                drawFlatline(context: context, size: size)
                return
            }

            let pts = downsample(to: Int(size.width * 2))
            let (minV, maxV) = amplitudeRange(pts)
            let span = maxV - minV
            let safeSpan = span > 0 ? span : 1.0

            var path = Path()
            for (i, v) in pts.enumerated() {
                let x = CGFloat(i) / CGFloat(pts.count - 1) * size.width
                let normalised = (v - minV) / safeSpan          // 0…1
                let y = (1.0 - normalised) * size.height * 0.88 + size.height * 0.06

                if i == 0 {
                    path.move(to: CGPoint(x: x, y: y))
                } else {
                    path.addLine(to: CGPoint(x: x, y: y))
                }
            }

            context.stroke(
                path,
                with: .color(waveColor),
                style: StrokeStyle(lineWidth: lineWidth, lineCap: .round, lineJoin: .round)
            )
        }
        .background(Color.black.opacity(0.35), in: RoundedRectangle(cornerRadius: 8))
    }

    // MARK: - Helpers

    /// Draws a centred horizontal line when there are no samples.
    private func drawFlatline(context: GraphicsContext, size: CGSize) {
        var path = Path()
        let midY = size.height / 2
        path.move(to: CGPoint(x: 0, y: midY))
        path.addLine(to: CGPoint(x: size.width, y: midY))
        context.stroke(path,
                       with: .color(waveColor.opacity(0.35)),
                       style: StrokeStyle(lineWidth: lineWidth, dash: [4, 4]))
    }

    /// Reduces `samples` to at most `maxPoints` by averaging buckets.
    /// Preserves peaks (min/max per bucket) to keep R-wave spikes visible.
    private func downsample(to maxPoints: Int) -> [Double] {
        guard samples.count > maxPoints else { return samples }
        let bucketSize = Double(samples.count) / Double(maxPoints)
        return (0..<maxPoints).map { i in
            let start = Int(Double(i) * bucketSize)
            let end   = min(Int(Double(i + 1) * bucketSize), samples.count)
            let slice = samples[start..<end]
            // Return the extremum with the largest absolute value to preserve spikes
            let maxVal = slice.max() ?? 0
            let minVal = slice.min() ?? 0
            return abs(maxVal) >= abs(minVal) ? maxVal : minVal
        }
    }

    /// Returns (min, max) with a small margin so the waveform never touches the edges.
    private func amplitudeRange(_ pts: [Double]) -> (Double, Double) {
        let lo = pts.min() ?? 0
        let hi = pts.max() ?? 0
        let margin = (hi - lo) * 0.1
        return (lo - margin, hi + margin)
    }
}

#Preview {
    let sampleRate = 1000
    let duration   = 5
    let t = (0..<(sampleRate * duration)).map { Double($0) / Double(sampleRate) }
    // Synthetic ECG-like signal: 1 Hz base + narrow R-spike approximation
    let synthetic: [Double] = t.map { time in
        let phase = time.truncatingRemainder(dividingBy: 1.0)
        let rPeak = phase < 0.04 ? 1.5 * exp(-pow((phase - 0.02) / 0.008, 2)) : 0
        let pWave = 0.25 * sin(2 * .pi * (time / 1.0 - 0.18))
        let tWave = 0.35 * (phase > 0.35 && phase < 0.65
            ? sin(.pi * (phase - 0.35) / 0.30) : 0)
        return rPeak + pWave + tWave
    }

    return WatchWaveformView(samples: synthetic)
        .frame(width: 180, height: 60)
        .preferredColorScheme(.dark)
}
