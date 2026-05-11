import SwiftUI

/// Scrolling waveform view — draws the last 5 seconds of signal from a SignalBuffer.
struct WaveformView: View {

    let buffer: SignalBuffer
    let color: Color
    let yRange: ClosedRange<Float>

    // Layout constants
    private let backgroundColour = Color(red: 0x10/255.0, green: 0x14/255.0, blue: 0x30/255.0)
    private let gridColour       = Color.white.opacity(0.12)
    private let displaySeconds: Int = 5

    var body: some View {
        TimelineView(.animation(minimumInterval: 1.0 / 30.0)) { _ in
            Canvas { context, size in
                context.fill(Path(CGRect(origin: .zero, size: size)), with: .color(backgroundColour))
                drawGrid(context: context, size: size)
                drawWaveform(context: context, size: size)
            }
        }
    }

    // MARK: - Grid

    private func drawGrid(context: GraphicsContext, size: CGSize) {
        let secondWidth = size.width / CGFloat(displaySeconds)
        for s in 1..<displaySeconds {
            let x = CGFloat(s) * secondWidth
            var line = Path()
            line.move(to: CGPoint(x: x, y: 0))
            line.addLine(to: CGPoint(x: x, y: size.height))
            context.stroke(line, with: .color(gridColour), lineWidth: 1)
        }
    }

    // MARK: - Waveform

    private func drawWaveform(context: GraphicsContext, size: CGSize) {
        let samples = buffer.latest(count: Int(size.width))
        guard samples.count >= 2 else { return }

        let yMin = CGFloat(yRange.lowerBound)
        let yMax = CGFloat(yRange.upperBound)
        let ySpan = yMax - yMin

        func yPos(_ v: Float) -> CGFloat {
            let clamped = max(yMin, min(yMax, CGFloat(v)))
            return size.height * (1 - (clamped - yMin) / ySpan)
        }

        let xStep = size.width / CGFloat(samples.count - 1)
        var path = Path()
        path.move(to: CGPoint(x: 0, y: yPos(samples[0])))
        for (i, sample) in samples.dropFirst().enumerated() {
            let x = CGFloat(i + 1) * xStep
            path.addLine(to: CGPoint(x: x, y: yPos(sample)))
        }
        context.stroke(path, with: .color(color), lineWidth: 1.5)
    }
}

#Preview {
    let buf = SignalBuffer(seconds: 5, sampleRate: 1000)
    buf.append((0..<5000).map { Float(sin(Double($0) * .pi / 100)) * 500 })
    return WaveformView(buffer: buf,
                        color: .cyan,
                        yRange: -600...600)
        .frame(height: 120)
        .background(Color.black)
}
