import SwiftUI

// MARK: - ActivityView

/// Live HAR / SAD status view — shows the device's on-board activity
/// recognition and speech detection results, updated every 2 seconds.
struct ActivityView: View {

    @Environment(BLEManager.self) private var ble
    @State private var pulseTrigger = false

    private let background  = Color(red: 0x08/255.0, green: 0x0C/255.0, blue: 0x1E/255.0)
    private let accent      = Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0)

    var body: some View {
        ZStack {
            background.ignoresSafeArea()

            if let activity = ble.currentActivity {
                ActivityContentView(activity: activity, pulseTrigger: pulseTrigger)
                    .onChange(of: activity.seq) {
                        pulseTrigger = true
                    }
            } else {
                ActivityPlaceholderView()
            }
        }
        .navigationTitle("Activiteit")
        // Reset pulseTrigger after the animation completes. The .task modifier
        // is automatically cancelled if the view disappears, avoiding a leaked task.
        .task(id: pulseTrigger) {
            guard pulseTrigger else { return }
            try? await Task.sleep(for: .milliseconds(600))
            pulseTrigger = false
        }
    }
}

// MARK: - ActivityContentView

private struct ActivityContentView: View {

    let activity: XBlock
    let pulseTrigger: Bool

    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    var body: some View {
        ScrollView {
            VStack(spacing: 28) {
                ActivityIconView(
                    activity: activity,
                    pulseTrigger: pulseTrigger,
                    reduceMotion: reduceMotion
                )
                if activity.activityClass == .walking || activity.activityClass == .running {
                    CadenceBadgeView(cadenceSpm: activity.cadenceSpm)
                }
                MotionIntensityView(intensity: activity.motionIntensity)
                SpeakingView(
                    isSpeaking: activity.isSpeaking,
                    speakingFraction: activity.speakingFraction
                )
            }
            .padding(24)
        }
        .scrollIndicators(.hidden)
    }
}

// MARK: - ActivityIconView

private struct ActivityIconView: View {

    let activity: XBlock
    let pulseTrigger: Bool
    let reduceMotion: Bool

    private let accent = Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0)

    var body: some View {
        VStack(spacing: 16) {
            ZStack {
                Circle()
                    .fill(accent.opacity(0.12))
                    .frame(width: 140, height: 140)
                    .scaleEffect(pulseScale)
                    .opacity(pulseOpacity)
                    .animation(
                        reduceMotion
                            ? .none
                            : .easeOut(duration: 0.5),
                        value: pulseTrigger
                    )

                Image(systemName: activity.activityClass.systemImage)
                    .font(.system(size: 64, weight: .light))
                    .foregroundStyle(accent)
                    .accessibilityHidden(true)
            }
            .frame(width: 140, height: 140)

            Text(activity.activityClass.label)
                .font(.title.bold())
                .foregroundStyle(.white)
                .accessibilityLabel("Activiteit: \(activity.activityClass.label)")
        }
    }

    private var pulseScale: CGFloat {
        pulseTrigger ? 1.25 : 1.0
    }

    private var pulseOpacity: Double {
        pulseTrigger ? 0.0 : 0.12
    }
}

// MARK: - CadenceBadgeView

private struct CadenceBadgeView: View {

    let cadenceSpm: Double

    private let accent = Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0)

    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: "shoeprints.fill")
                .foregroundStyle(accent)
                .accessibilityHidden(true)

            Text("\(cadenceSpm, format: .number.precision(.fractionLength(0))) spm")
                .font(.headline)
                .foregroundStyle(.white)
        }
        .padding(.horizontal, 20)
        .padding(.vertical, 10)
        .background(
            Capsule()
                .fill(accent.opacity(0.15))
        )
        .accessibilityElement(children: .combine)
        .accessibilityLabel("Cadans: \(cadenceSpm, format: .number.precision(.fractionLength(0))) stappen per minuut")
    }
}

// MARK: - MotionIntensityView

private struct MotionIntensityView: View {

    let intensity: Double  // 0.0 – 1.0

    private let accent = Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0)

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("Bewegingsintensiteit")
                    .font(.subheadline)
                    .foregroundStyle(.white.opacity(0.6))
                Spacer()
                Text("\(intensity * 100, format: .number.precision(.fractionLength(0)))%")
                    .font(.subheadline.monospacedDigit())
                    .foregroundStyle(.white)
            }

            ZStack(alignment: .leading) {
                RoundedRectangle(cornerRadius: 4)
                    .fill(Color.white.opacity(0.08))
                    .frame(maxWidth: .infinity)

                RoundedRectangle(cornerRadius: 4)
                    .fill(accent)
                    .containerRelativeFrame(.horizontal) { width, _ in width * intensity }
            }
            .frame(height: 8)
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("Bewegingsintensiteit \(intensity * 100, format: .number.precision(.fractionLength(0))) procent")
    }
}

// MARK: - SpeakingView

private struct SpeakingView: View {

    let isSpeaking: Bool
    let speakingFraction: Double  // 0.0 – 1.0

    var body: some View {
        VStack(spacing: 16) {
            SpeakingIndicatorView(isSpeaking: isSpeaking)
            SpeakingFractionView(fraction: speakingFraction)
        }
        .padding(20)
        .background(
            RoundedRectangle(cornerRadius: 16)
                .fill(Color.white.opacity(0.05))
        )
    }
}

// MARK: - SpeakingIndicatorView

private struct SpeakingIndicatorView: View {

    let isSpeaking: Bool

    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: isSpeaking ? "mic.fill" : "mic.slash")
                .font(.title2)
                .foregroundStyle(isSpeaking ? Color.green : Color.white.opacity(0.35))
                .accessibilityHidden(true)

            Text(isSpeaking ? "Spreekt" : "Spreekt niet")
                .font(.headline)
                .foregroundStyle(isSpeaking ? .white : .white.opacity(0.4))

            Spacer()

            if isSpeaking {
                Circle()
                    .fill(Color.green)
                    .frame(width: 10, height: 10)
                    .accessibilityHidden(true)
            }
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel(isSpeaking ? "Spreekt nu" : "Spreekt niet")
    }
}

// MARK: - SpeakingFractionView

private struct SpeakingFractionView: View {

    let fraction: Double  // 0.0 – 1.0

    var body: some View {
        HStack {
            Text("Spreekt \(fraction * 100, format: .number.precision(.fractionLength(0)))% van de tijd")
                .font(.subheadline)
                .foregroundStyle(.white.opacity(0.6))
            Spacer()
        }
        .accessibilityLabel("Spreektijd laatste 30 seconden: \(fraction * 100, format: .number.precision(.fractionLength(0))) procent")
    }
}

// MARK: - ActivityPlaceholderView

private struct ActivityPlaceholderView: View {

    private let accent = Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0)

    var body: some View {
        VStack(spacing: 16) {
            Image(systemName: "sensor.tag.radiowaves.forward")
                .font(.system(size: 48, weight: .light))
                .foregroundStyle(.white.opacity(0.2))
                .accessibilityHidden(true)

            Text("Wacht op activiteitsdata…")
                .font(.body)
                .foregroundStyle(.white.opacity(0.4))
        }
        .accessibilityLabel("Wacht op activiteitsdata van het apparaat")
    }
}

// MARK: - Preview

#Preview {
    let ble = BLEManager()
    ble.currentActivity = XBlock(
        seq: 42,
        timestampUs: 1_000_000,
        activityClass: .walking,
        cadenceSpm: 112.0,
        motionIntensity: 0.61,
        isSpeaking: true,
        speakingFraction: 0.34
    )
    return ActivityView()
        .environment(ble)
}
