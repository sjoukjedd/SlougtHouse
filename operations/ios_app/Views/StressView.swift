import SwiftUI
import SwiftData

// MARK: - StressView
//
// Live Composite Stress Index display.
// Shows a semicircular gauge (0–100), baseline progress, activity gate status,
// and a 2×2 grid of per-marker z-score cards.

struct StressView: View {

    @Environment(BLEManager.self) private var ble
    @State private var baselineResetTrigger = false

    private let background    = Color(red: 0x08/255.0, green: 0x0C/255.0, blue: 0x1E/255.0)
    private let accent        = Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0)
    private let tileBackground = Color(red: 0x10/255.0, green: 0x14/255.0, blue: 0x30/255.0)

    var body: some View {
        let engine = ble.csiEngine
        ZStack {
            background.ignoresSafeArea()
            ScrollView {
                VStack(spacing: 24) {
                    // ── Gauge ─────────────────────────────────────────────
                    CSIGaugeView(score: engine.score, label: engine.csiLabel)
                        .frame(height: 200)
                        .padding(.top, 8)

                    // ── Status row ────────────────────────────────────────
                    StatusRowView(
                        engine: engine,
                        accent: accent,
                        tileBackground: tileBackground
                    )

                    // ── Component cards ───────────────────────────────────
                    if let components = engine.componentScores {
                        ComponentGridView(
                            components: components,
                            accent: accent,
                            tileBackground: tileBackground
                        )
                    } else {
                        ComponentGridPlaceholderView(
                            accent: accent,
                            tileBackground: tileBackground
                        )
                    }

                    // ── Disclaimer ────────────────────────────────────────
                    Text("For research use only. Not intended for clinical diagnosis.")
                        .font(.caption2)
                        .foregroundStyle(.white.opacity(0.3))
                        .multilineTextAlignment(.center)
                        .padding(.horizontal, 24)

                    // ── Reset baseline ────────────────────────────────────
                    Button {
                        engine.resetBaseline()
                        baselineResetTrigger = true
                    } label: {
                        HStack(spacing: 6) {
                            Image(systemName: "arrow.counterclockwise")
                            Text("Reset baseline")
                        }
                        .font(.caption)
                        .foregroundStyle(.white.opacity(0.4))
                    }
                    .buttonStyle(.plain)
                    .padding(.bottom, 16)
                }
                .padding(.horizontal, 16)
            }
        }
        .overlay(alignment: .bottom) {
            if baselineResetTrigger {
                Text("Baseline gereset")
                    .font(.caption)
                    .foregroundStyle(.white)
                    .padding(.horizontal, 16)
                    .padding(.vertical, 8)
                    .background(
                        Capsule()
                            .fill(Color.white.opacity(0.15))
                    )
                    .padding(.bottom, 24)
                    .transition(.opacity.combined(with: .move(edge: .bottom)))
            }
        }
        .animation(.easeInOut(duration: 0.3), value: baselineResetTrigger)
        .task(id: baselineResetTrigger) {
            guard baselineResetTrigger else { return }
            try? await Task.sleep(for: .milliseconds(2000))
            baselineResetTrigger = false
        }
    }
}

// MARK: - CSIGaugeView

private struct CSIGaugeView: View {

    let score: Double?
    var label: String = "CSI"

    // Zone colours per spec
    private let green  = Color(red: 0x29/255.0, green: 0xC6/255.0, blue: 0x6D/255.0)
    private let amber  = Color(red: 0xE0/255.0, green: 0xA8/255.0, blue: 0x1A/255.0)
    private let red    = Color(red: 0xE0/255.0, green: 0x5C/255.0, blue: 0x1A/255.0)

    private let accent = Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0)

    var body: some View {
        GeometryReader { geo in
            let size  = min(geo.size.width, geo.size.height * 2)
            let cx    = geo.size.width / 2
            let cy    = geo.size.height        // bottom half of square
            let r     = size / 2 - 16

            ZStack {
                // ── Track arcs (zones) ────────────────────────────────────
                // Green 0–33
                ArcShape(startAngle: 180, endAngle: 180 + 180 * 0.33)
                    .stroke(green.opacity(0.25), style: StrokeStyle(lineWidth: 18, lineCap: .butt))
                    .frame(width: size, height: size)
                    .position(x: cx, y: cy)

                // Amber 33–66
                ArcShape(startAngle: 180 + 180 * 0.33, endAngle: 180 + 180 * 0.66)
                    .stroke(amber.opacity(0.25), style: StrokeStyle(lineWidth: 18, lineCap: .butt))
                    .frame(width: size, height: size)
                    .position(x: cx, y: cy)

                // Red 66–100
                ArcShape(startAngle: 180 + 180 * 0.66, endAngle: 360)
                    .stroke(red.opacity(0.25), style: StrokeStyle(lineWidth: 18, lineCap: .butt))
                    .frame(width: size, height: size)
                    .position(x: cx, y: cy)

                // ── Filled arc for current score ──────────────────────────
                if let s = score {
                    let frac   = s / 100.0
                    let endDeg = 180 + 180 * frac
                    ArcShape(startAngle: 180, endAngle: endDeg)
                        .stroke(
                            gaugeColor(score: s),
                            style: StrokeStyle(lineWidth: 18, lineCap: .round)
                        )
                        .frame(width: size, height: size)
                        .position(x: cx, y: cy)
                        .animation(.easeInOut(duration: 0.6), value: s)
                }

                // ── Score label ───────────────────────────────────────────
                VStack(spacing: 4) {
                    if let s = score {
                        Text(String(format: "%.0f", s))
                            .font(.system(size: 56, weight: .bold, design: .monospaced))
                            .foregroundStyle(gaugeColor(score: s))
                            .contentTransition(.numericText())
                            .animation(.easeInOut(duration: 0.4), value: s)
                        Text(label)
                            .font(.caption)
                            .foregroundStyle(.white.opacity(0.4))
                    } else {
                        Text("—")
                            .font(.system(size: 56, weight: .thin, design: .monospaced))
                            .foregroundStyle(.white.opacity(0.2))
                        Text(label)
                            .font(.caption)
                            .foregroundStyle(.white.opacity(0.2))
                    }
                }
                .position(x: cx, y: cy - r * 0.25)
            }
        }
    }

    private func gaugeColor(score: Double) -> Color {
        switch score {
        case 0..<34:  return green
        case 34..<67: return amber
        default:      return red
        }
    }
}

// MARK: - ArcShape

private struct ArcShape: Shape {
    var startAngle: Double   // degrees
    var endAngle:   Double   // degrees

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

// MARK: - StatusRowView

private struct StatusRowView: View {

    let engine: CSIEngine
    let accent: Color
    let tileBackground: Color

    private let amber = Color(red: 0xE0/255.0, green: 0xA8/255.0, blue: 0x1A/255.0)

    var body: some View {
        VStack(spacing: 8) {
            if engine.isActivityGated {
                // Activity gated
                HStack(spacing: 8) {
                    Image(systemName: "figure.walk")
                        .foregroundStyle(amber)
                    Text("PAUSED — activity")
                        .font(.subheadline.bold())
                        .foregroundStyle(amber)
                }
                .frame(maxWidth: .infinity)
                .padding(12)
                .background(tileBackground, in: RoundedRectangle(cornerRadius: 10))

            } else if engine.isBaselineComplete {
                // Calibrated
                HStack(spacing: 6) {
                    Image(systemName: "checkmark.circle.fill")
                        .foregroundStyle(accent)
                    Text("Calibrated")
                        .font(.subheadline.bold())
                        .foregroundStyle(accent)
                }
                .frame(maxWidth: .infinity)
                .padding(12)
                .background(tileBackground, in: RoundedRectangle(cornerRadius: 10))

            } else {
                // Baseline in progress
                VStack(alignment: .leading, spacing: 6) {
                    HStack {
                        Text("Baseline")
                            .font(.caption)
                            .foregroundStyle(.white.opacity(0.5))
                        Spacer()
                        Text("\(Int(engine.baselineProgress * 100))%")
                            .font(.caption.monospacedDigit())
                            .foregroundStyle(accent)
                    }
                    ZStack(alignment: .leading) {
                        RoundedRectangle(cornerRadius: 4)
                            .fill(Color.white.opacity(0.08))
                            .frame(maxWidth: .infinity)
                        GeometryReader { geo in
                            RoundedRectangle(cornerRadius: 4)
                                .fill(accent)
                                .frame(width: geo.size.width * engine.baselineProgress)
                                .animation(.easeInOut(duration: 0.4), value: engine.baselineProgress)
                        }
                    }
                    .frame(height: 6)
                }
                .padding(12)
                .background(tileBackground, in: RoundedRectangle(cornerRadius: 10))
            }
        }
    }
}

// MARK: - ComponentGridView

private struct ComponentGridView: View {

    let components: CSIComponents
    let accent: Color
    let tileBackground: Color

    var body: some View {
        LazyVGrid(
            columns: [GridItem(.flexible(), spacing: 12), GridItem(.flexible(), spacing: 12)],
            spacing: 12
        ) {
            ZScoreCard(label: "PEP",  unit: "ms⁻¹·z",      value: components.pepZ,     accent: accent, tileBackground: tileBackground)
            ZScoreCard(label: "RMSSD", unit: "ms⁻¹·z",     value: components.rmssdZ,   accent: accent, tileBackground: tileBackground)
            ZScoreCard(label: "SCL",  unit: "µS·z",         value: components.sclZ,     accent: accent, tileBackground: tileBackground)
            ZScoreCard(label: "SCR",  unit: "evt/min·z",    value: components.scrRateZ, accent: accent, tileBackground: tileBackground)
        }
    }
}

// MARK: - ComponentGridPlaceholderView

private struct ComponentGridPlaceholderView: View {

    let accent: Color
    let tileBackground: Color

    var body: some View {
        LazyVGrid(
            columns: [GridItem(.flexible(), spacing: 12), GridItem(.flexible(), spacing: 12)],
            spacing: 12
        ) {
            ZScoreCard(label: "PEP",   unit: "ms⁻¹·z",   value: nil, accent: accent, tileBackground: tileBackground)
            ZScoreCard(label: "RMSSD", unit: "ms⁻¹·z",   value: nil, accent: accent, tileBackground: tileBackground)
            ZScoreCard(label: "SCL",   unit: "µS·z",      value: nil, accent: accent, tileBackground: tileBackground)
            ZScoreCard(label: "SCR",   unit: "evt/min·z", value: nil, accent: accent, tileBackground: tileBackground)
        }
    }
}

// MARK: - ZScoreCard

private struct ZScoreCard: View {

    let label:          String
    let unit:           String
    let value:          Double?
    let accent:         Color
    let tileBackground: Color

    private let dim = Color.white.opacity(0.35)

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(label)
                .font(.caption)
                .foregroundStyle(dim)
            HStack(alignment: .lastTextBaseline, spacing: 4) {
                Group {
                    if let v = value {
                        Text(String(format: "%+.2f", v))
                            .contentTransition(.numericText(countsDown: v < 0))
                    } else {
                        Text("—")
                    }
                }
                .font(.system(size: 28, weight: .semibold, design: .monospaced))
                .foregroundStyle(accent)
                .lineLimit(1)
                .minimumScaleFactor(0.6)

                Text(unit)
                    .font(.system(size: 9))
                    .foregroundStyle(dim)
                    .lineLimit(1)
                    .minimumScaleFactor(0.7)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(tileBackground, in: RoundedRectangle(cornerRadius: 12))
    }
}

// MARK: - Preview

#Preview {
    let container = try! ModelContainer(for: CSIBaselineRecord.self,
                                        configurations: ModelConfiguration(isStoredInMemoryOnly: true))
    let ble = BLEManager(modelContext: container.mainContext)
    return StressView()
        .environment(ble)
}
