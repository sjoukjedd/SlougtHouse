import SwiftUI

// MARK: - WatchStressView
//
// Compact CSI stress display for the Apple Watch 46mm face (184×224 pt).
//
// Layout (top → bottom):
//   1. Semicircular arc gauge, ~120 pt diameter, score in centre.
//   2. One-line status: "Calibrating…", "Paused", or empty.
//   3. Row of four coloured component dots (PEP · RMSSD · SCL · SCR).

struct WatchStressView: View {

    @Environment(WatchConnectivityReceiver.self) private var receiver

    private let cyan = Color(red: 0, green: 0xB6/255.0, blue: 0xCB/255.0)

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()

            VStack(spacing: 6) {
                // ── Gauge ─────────────────────────────────────────────────
                WatchCSIGaugeView(score: displayScore)
                    .frame(width: 120, height: 64)   // semicircle uses upper half of square

                // ── Status ────────────────────────────────────────────────
                statusText
                    .font(.system(size: 12, weight: .medium))
                    .frame(height: 16)

                // ── Component dots ────────────────────────────────────────
                WatchComponentDotsView(score: displayScore, cyan: cyan)
            }
            .padding(.vertical, 8)
        }
    }

    // MARK: - Helpers

    /// nil when not yet available (score == -1), otherwise the Double value.
    private var displayScore: Double? {
        receiver.csiScore >= 0 ? receiver.csiScore : nil
    }

    @ViewBuilder
    private var statusText: some View {
        if receiver.csiGated {
            Text("Paused")
                .foregroundStyle(Color(red: 0xE0/255.0, green: 0xA8/255.0, blue: 0x1A/255.0))
        } else if receiver.csiScore < 0 {
            Text("Calibrating…")
                .foregroundStyle(.white.opacity(0.5))
        } else {
            Text("")   // calibrated & running — nothing to say
        }
    }
}

// MARK: - WatchCSIGaugeView

private struct WatchCSIGaugeView: View {

    let score: Double?

    private let zoneGreen  = Color(red: 0x29/255.0, green: 0xC6/255.0, blue: 0x6D/255.0)
    private let zoneAmber  = Color(red: 0xE0/255.0, green: 0xA8/255.0, blue: 0x1A/255.0)
    private let zoneRed    = Color(red: 0xE0/255.0, green: 0x5C/255.0, blue: 0x1A/255.0)

    var body: some View {
        GeometryReader { geo in
            let size = geo.size.width          // 120 pt
            let cx   = size / 2
            let cy   = size / 2                // arc centred at mid; lower half clipped by frame

            ZStack {
                // ── Track zones ───────────────────────────────────────────
                WatchArcShape(startAngle: 180, endAngle: 180 + 180 * 0.33)
                    .stroke(zoneGreen.opacity(0.25),
                            style: StrokeStyle(lineWidth: 12, lineCap: .butt))
                    .frame(width: size, height: size)
                    .position(x: cx, y: cy)

                WatchArcShape(startAngle: 180 + 180 * 0.33, endAngle: 180 + 180 * 0.66)
                    .stroke(zoneAmber.opacity(0.25),
                            style: StrokeStyle(lineWidth: 12, lineCap: .butt))
                    .frame(width: size, height: size)
                    .position(x: cx, y: cy)

                WatchArcShape(startAngle: 180 + 180 * 0.66, endAngle: 360)
                    .stroke(zoneRed.opacity(0.25),
                            style: StrokeStyle(lineWidth: 12, lineCap: .butt))
                    .frame(width: size, height: size)
                    .position(x: cx, y: cy)

                // ── Filled arc ────────────────────────────────────────────
                if let s = score {
                    WatchArcShape(startAngle: 180, endAngle: 180 + 180 * (s / 100.0))
                        .stroke(gaugeColor(s),
                                style: StrokeStyle(lineWidth: 12, lineCap: .round))
                        .frame(width: size, height: size)
                        .position(x: cx, y: cy)
                        .animation(.easeInOut(duration: 0.6), value: s)
                }

                // ── Score label ───────────────────────────────────────────
                Group {
                    if let s = score {
                        Text(String(format: "%.0f", s))
                            .font(.system(size: 36, weight: .bold))
                            .foregroundStyle(gaugeColor(s))
                            .contentTransition(.numericText())
                            .animation(.easeInOut(duration: 0.4), value: s)
                    } else {
                        Text("—")
                            .font(.system(size: 36, weight: .thin))
                            .foregroundStyle(.white.opacity(0.25))
                    }
                }
                // Position label in the centre-bottom quadrant of the semicircle
                .position(x: cx, y: cy - 10)
            }
        }
    }

    private func gaugeColor(_ s: Double) -> Color {
        switch s {
        case 0..<34:  return zoneGreen
        case 34..<67: return zoneAmber
        default:      return zoneRed
        }
    }
}

// MARK: - WatchArcShape

private struct WatchArcShape: Shape {
    var startAngle: Double
    var endAngle:   Double

    func path(in rect: CGRect) -> Path {
        var p = Path()
        p.addArc(center: CGPoint(x: rect.midX, y: rect.midY),
                 radius: rect.width / 2,
                 startAngle: .degrees(startAngle),
                 endAngle:   .degrees(endAngle),
                 clockwise: false)
        return p
    }
}

// MARK: - WatchComponentDotsView
//
// Four coloured dots — one per CSI component.
// Colour is derived from the overall CSI zone (since individual z-scores are
// not transmitted over WatchConnectivity in this revision).
// Future work: transmit per-component z-scores and colour each dot independently.

private struct WatchComponentDotsView: View {

    let score: Double?
    let cyan: Color

    private let labels = ["PEP", "RMSSD", "SCL", "SCR"]

    private let zoneGreen = Color(red: 0x29/255.0, green: 0xC6/255.0, blue: 0x6D/255.0)
    private let zoneAmber = Color(red: 0xE0/255.0, green: 0xA8/255.0, blue: 0x1A/255.0)
    private let zoneRed   = Color(red: 0xE0/255.0, green: 0x5C/255.0, blue: 0x1A/255.0)

    var body: some View {
        HStack(spacing: 14) {
            ForEach(labels, id: \.self) { label in
                VStack(spacing: 3) {
                    Circle()
                        .fill(dotColor)
                        .frame(width: 10, height: 10)
                    Text(label)
                        .font(.system(size: 9, weight: .medium))
                        .foregroundStyle(.white.opacity(0.45))
                }
            }
        }
    }

    private var dotColor: Color {
        guard let s = score else { return .white.opacity(0.2) }
        switch s {
        case 0..<34:  return zoneGreen
        case 34..<67: return zoneAmber
        default:      return zoneRed
        }
    }
}

// MARK: - Preview

#Preview {
    let receiver = WatchConnectivityReceiver()
    return WatchStressView()
        .environment(receiver)
}
