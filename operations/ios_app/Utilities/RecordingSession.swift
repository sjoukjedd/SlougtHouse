import Foundation

// MARK: - RecordingSession

/// Tracks a single VU-AMS recording session and handles wall-clock anchoring.
///
/// The firmware timestamps every data block in microseconds-since-boot
/// (`esp_timer_get_time()`).  Once the first block arrives, `deviceBootOffset`
/// is derived from the difference between the SYNC_TIME wall-clock stamp and
/// the first block's device uptime, allowing every subsequent uptime value to
/// be converted to an absolute `Date`.
final class RecordingSession {

    // MARK: - Public properties

    /// Wall-clock time at which START_RECORDING + SYNC_TIME were sent.
    let startTimestamp: Date

    /// BLE peripheral name (used in the sidecar and for file naming).
    let deviceName: String

    /// Subject identifier entered by the researcher before recording starts.
    let subjectId: String

    /// Distance between the inner ICG electrodes in centimetres (Kubicek SV formula).
    let electrodDistanceCm: Float?

    /// Difference between wall-clock time and device uptime, set once the
    /// first data block is received.  `nil` until that moment.
    private(set) var deviceBootOffset: TimeInterval?

    // MARK: - Init

    init(deviceName: String,
         subjectId: String = "",
         electrodDistanceCm: Float? = nil,
         startTimestamp: Date = Date()) {
        self.deviceName = deviceName
        self.subjectId = subjectId
        self.electrodDistanceCm = electrodDistanceCm
        self.startTimestamp = startTimestamp
    }

    // MARK: - Boot-offset calibration

    /// Call this with the `timestampUs` from the very first data block that
    /// arrives after recording starts.  Subsequent calls are no-ops so the
    /// offset stays anchored to the first block.
    ///
    /// - Parameter firstBlockUptimeUs: `BlockHeader.timestampUs` from the
    ///   first received block (device microseconds since boot).
    func calibrate(firstBlockUptimeUs: UInt64) {
        guard deviceBootOffset == nil else { return }
        let deviceUptimeSeconds = TimeInterval(firstBlockUptimeUs) / 1_000_000
        // bootTime = wallClock - deviceUptime
        deviceBootOffset = startTimestamp.timeIntervalSince1970 - deviceUptimeSeconds
    }

    // MARK: - Time conversion

    /// Converts a device uptime (µs since boot) to an absolute wall-clock `Date`.
    ///
    /// Falls back to `startTimestamp` offset by the uptime if calibration has
    /// not yet occurred, so callers always receive a valid date.
    ///
    /// - Parameter uptime: Device uptime in microseconds (`BlockHeader.timestampUs`).
    func absoluteTime(forDeviceUptime uptime: UInt32) -> Date {
        let deviceSeconds = TimeInterval(uptime) / 1_000_000
        if let offset = deviceBootOffset {
            return Date(timeIntervalSince1970: offset + deviceSeconds)
        }
        // Pre-calibration fallback: treat startTimestamp as t=0 of the device
        return startTimestamp.addingTimeInterval(deviceSeconds)
    }

    // MARK: - Sidecar

    /// Writes a JSON metadata sidecar alongside the SD data file.
    ///
    /// The sidecar URL is derived from `url` by replacing its path extension
    /// with `.meta.json`.  The caller is responsible for choosing a sensible
    /// base URL (e.g. the path where the SD file will be stored or cached).
    ///
    /// - Parameters:
    ///   - url: Base URL — the `.meta.json` file is written next to this path.
    ///   - electrodeDistanceCm: Thorax electrode separation in centimetres,
    ///     read from user settings and required for ICG stroke volume scaling.
    func saveMetadataSidecar(to url: URL, electrodeDistanceCm: Double = 0) throws {
        let sidecarURL = url.deletingPathExtension().appendingPathExtension("meta.json")

        let iso8601 = ISO8601DateFormatter()
        iso8601.formatOptions = [.withInternetDateTime, .withFractionalSeconds]

        let bundle = Bundle.main
        let appVersion = (bundle.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String)
            ?? "unknown"
        let buildNumber = (bundle.object(forInfoDictionaryKey: "CFBundleVersion") as? String)
            ?? "0"

        let payload: [String: Any] = [
            "start_time":             iso8601.string(from: startTimestamp),
            "device_id":              deviceName,
            "app_version":            "\(appVersion) (\(buildNumber))",
            "electrode_distance_cm":  electrodeDistanceCm,
            "sync_time_us":           UInt64(startTimestamp.timeIntervalSince1970 * 1_000_000),
        ]

        let data = try JSONSerialization.data(withJSONObject: payload,
                                             options: [.prettyPrinted, .sortedKeys])
        try data.write(to: sidecarURL, options: .atomicWrite)
    }
}
