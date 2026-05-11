import SwiftUI
import SwiftData

/// Live numeric dashboard — grid of key physiological values.
struct SignalDashboardView: View {

    @Environment(BLEManager.self) var ble

    private let background   = Color(red: 0x08/255.0, green: 0x0C/255.0, blue: 0x1E/255.0)
    private let accentColour = Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0)
    private let dimColour    = Color.white.opacity(0.35)

    var body: some View {
        ZStack {
            background.ignoresSafeArea()
            ScrollView {
                LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())],
                          spacing: 16) {
                    MetricTile(label: "HR",     unit: "bpm",   value: hrText,   accent: accentColour, dim: dimColour)
                    MetricTile(label: "CO",     unit: "L/min", value: coText,   accent: accentColour, dim: dimColour)
                    MetricTile(label: "SV",     unit: "mL",    value: svText,   accent: accentColour, dim: dimColour)
                    SpO2Tile(value: spo2Text, tileBackground: spo2TileBackground, accent: accentColour, dim: dimColour)
                    MetricTile(label: "SCL",    unit: "µS",    value: sclText,  accent: accentColour, dim: dimColour)
                    MetricTile(label: "Temp",   unit: "°C",    value: tempText, accent: accentColour, dim: dimColour)
                    MetricTile(label: "HRV",    unit: "ms",    value: "—",      accent: accentColour, dim: dimColour)
                    MetricTile(label: "Z₀",     unit: "Ω",     value: z0Text,   accent: accentColour, dim: dimColour)
                    RRMetricTile(value: rrText, tileBackground: rrTileBackground, accent: accentColour, dim: dimColour)
                    SCRTile(value: scrText, scrActive: ble.scrActive, accent: accentColour, dim: dimColour)
                }
                .padding()
            }
        }
    }

    // MARK: - Derived values

    private var hrText: String {
        guard let p = ble.latestPBlock else { return "—" }
        return "\(p.hrPpg)"
    }

    private var coText: String {
        guard let i = ble.latestIBlock else { return "—" }
        return i.co.formatted(.number.precision(.fractionLength(1)))
    }

    private var svText: String {
        guard let i = ble.latestIBlock else { return "—" }
        return i.sv.formatted(.number.precision(.fractionLength(0)))
    }

    private var spo2Text: String {
        guard let p = ble.latestPBlock,
              !p.spo2Pct.isNaN else { return "—" }
        return String(format: "%.1f", p.spo2Pct)
    }

    private var spo2TileBackground: Color {
        guard let p = ble.latestPBlock,
              !p.spo2Pct.isNaN else {
            return Color(red: 0x10/255.0, green: 0x14/255.0, blue: 0x30/255.0) // dim/invalid
        }
        let pct = p.spo2Pct
        if pct >= 95 {
            return Color(red: 0x0A/255.0, green: 0x2E/255.0, blue: 0x1A/255.0) // green tint
        } else if pct >= 90 {
            return Color(red: 0x2E/255.0, green: 0x1E/255.0, blue: 0x04/255.0) // amber tint
        } else {
            return Color(red: 0x2E/255.0, green: 0x08/255.0, blue: 0x08/255.0) // red tint
        }
    }

    private var scrText: String {
        guard ble.sclBuffer.currentCount >= 10 else { return "—" }
        return String(format: "%.1f", ble.scrRatePerMin)
    }

    private var sclText: String {
        guard let s = ble.latestSBlock else { return "—" }
        return String(format: "%.2f", s.sclTonic)
    }

    private var tempText: String {
        guard let t = ble.latestTBlock else { return "—" }
        return t.skinTempC.formatted(.number.precision(.fractionLength(1)))
    }

    private var z0Text: String {
        guard let i = ble.latestIBlock else { return "—" }
        return i.z0.formatted(.number.precision(.fractionLength(1)))
    }

    private var rrText: String {
        guard let r = ble.latestRBlock, r.rrValid, !r.rrBpm.isNaN else { return "—" }
        return String(format: "%.1f", r.rrBpm)
    }

    private var rrTileBackground: Color {
        guard let r = ble.latestRBlock, r.rrValid, !r.rrBpm.isNaN else {
            return Color(red: 0x10/255.0, green: 0x14/255.0, blue: 0x30/255.0) // dim/default
        }
        let bpm = r.rrBpm
        if bpm >= 12 && bpm <= 20 {
            return Color(red: 0x0A/255.0, green: 0x2E/255.0, blue: 0x1A/255.0) // green tint
        } else if bpm >= 4 && bpm <= 40 {
            return Color(red: 0x2E/255.0, green: 0x1E/255.0, blue: 0x04/255.0) // amber tint
        } else {
            return Color(red: 0x10/255.0, green: 0x14/255.0, blue: 0x30/255.0) // dim/default
        }
    }
}

// MARK: - Metric Tile

private struct MetricTile: View {
    let label:  String
    let unit:   String
    let value:  String
    let accent: Color
    let dim:    Color

    private let tileBackground = Color(red: 0x10/255.0, green: 0x14/255.0, blue: 0x30/255.0)

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(label)
                .font(.caption)
                .foregroundStyle(dim)
            HStack(alignment: .lastTextBaseline, spacing: 4) {
                Text(value)
                    .font(.system(size: 36, weight: .semibold, design: .monospaced))
                    .foregroundStyle(accent)
                    .lineLimit(1)
                    .minimumScaleFactor(0.5)
                Text(unit)
                    .font(.caption2)
                    .foregroundStyle(dim)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(tileBackground, in: RoundedRectangle(cornerRadius: 12))
    }
}

// MARK: - RR Metric Tile

/// Respiratory rate tile with dynamic background colour:
/// - Green tint when rr_bpm is in normal range 12–20 br/min
/// - Amber tint when rr_bpm is outside 12–20 but within 4–40 (confounder zone)
/// - Default dim when invalid or unavailable
private struct RRMetricTile: View {
    let value:          String
    let tileBackground: Color
    let accent:         Color
    let dim:            Color

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("Ademhaling")
                .font(.caption)
                .foregroundStyle(dim)
            HStack(alignment: .lastTextBaseline, spacing: 4) {
                Text(value)
                    .font(.system(size: 36, weight: .semibold, design: .monospaced))
                    .foregroundStyle(accent)
                    .lineLimit(1)
                    .minimumScaleFactor(0.5)
                Text("br/min")
                    .font(.caption2)
                    .foregroundStyle(dim)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(tileBackground, in: RoundedRectangle(cornerRadius: 12))
    }
}

// MARK: - SpO2 Tile

/// SpO2 tile with dynamic background colour:
/// - Green tint  ≥ 95 % (normal)
/// - Amber tint  90–94 % (mild hypoxia warning)
/// - Red tint    < 90 % (significant hypoxia)
/// - Default dim when NaN / no data
private struct SpO2Tile: View {
    let value:          String
    let tileBackground: Color
    let accent:         Color
    let dim:            Color

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("SpO2")
                .font(.caption)
                .foregroundStyle(dim)
            HStack(alignment: .lastTextBaseline, spacing: 4) {
                Text(value)
                    .font(.system(size: 36, weight: .semibold, design: .monospaced))
                    .foregroundStyle(accent)
                    .lineLimit(1)
                    .minimumScaleFactor(0.5)
                Text("%")
                    .font(.caption2)
                    .foregroundStyle(dim)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(tileBackground, in: RoundedRectangle(cornerRadius: 12))
    }
}

// MARK: - SCR Tile

/// SCR rate tile with an optional pulsing green dot when a SCR event occurred
/// within the last 5 seconds.
private struct SCRTile: View {
    let value:     String
    let scrActive: Bool
    let accent:    Color
    let dim:       Color

    @State private var pulse: Bool = false

    private let tileBackground = Color(red: 0x10/255.0, green: 0x14/255.0, blue: 0x30/255.0)

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("SCR")
                .font(.caption)
                .foregroundStyle(dim)
            HStack(alignment: .lastTextBaseline, spacing: 4) {
                Text(value)
                    .font(.system(size: 36, weight: .semibold, design: .monospaced))
                    .foregroundStyle(accent)
                    .lineLimit(1)
                    .minimumScaleFactor(0.5)
                Text("events/min")
                    .font(.caption2)
                    .foregroundStyle(dim)
                if scrActive {
                    Circle()
                        .fill(Color.green)
                        .frame(width: 8, height: 8)
                        .opacity(pulse ? 0.3 : 1.0)
                        .animation(
                            .easeInOut(duration: 0.6).repeatForever(autoreverses: true),
                            value: pulse
                        )
                        .onAppear { pulse = true }
                        .onDisappear { pulse = false }
                }
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(tileBackground, in: RoundedRectangle(cornerRadius: 12))
    }
}

#Preview {
    let container = try! ModelContainer(for: CSIBaselineRecord.self,
                                        configurations: ModelConfiguration(isStoredInMemoryOnly: true))
    return SignalDashboardView()
        .environment(BLEManager(modelContext: container.mainContext))
}
