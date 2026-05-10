import SwiftUI

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
                    MetricTile(label: "SpO₂",   unit: "%",     value: spo2Text, accent: accentColour, dim: dimColour)
                    MetricTile(label: "SCL",    unit: "µS",    value: sclText,  accent: accentColour, dim: dimColour)
                    MetricTile(label: "Temp",   unit: "°C",    value: tempText, accent: accentColour, dim: dimColour)
                    MetricTile(label: "HRV",    unit: "ms",    value: "—",      accent: accentColour, dim: dimColour)
                    MetricTile(label: "Z₀",     unit: "Ω",     value: z0Text,   accent: accentColour, dim: dimColour)
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
        guard let p = ble.latestPBlock else { return "—" }
        return p.spo2Pct.formatted(.number.precision(.fractionLength(1)))
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

#Preview {
    SignalDashboardView()
        .environment(BLEManager())
}
