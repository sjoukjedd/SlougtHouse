import SwiftUI
import CoreBluetooth

/// "Connect" tab — scan, device list, status indicator.
struct ConnectionView: View {

    @EnvironmentObject var ble: BLEManager

    private let background   = Color(red: 0x08/255.0, green: 0x0C/255.0, blue: 0x1E/255.0)
    private let accentColour = Color(red: 0x00/255.0, green: 0xB6/255.0, blue: 0xCB/255.0)

    var body: some View {
        NavigationStack {
            ZStack {
                background.ignoresSafeArea()
                VStack(spacing: 0) {
                    statusBanner
                    deviceList
                }
            }
            .navigationTitle("VU-AMS Connect")
            .navigationBarTitleDisplayMode(.inline)
            .toolbarColorScheme(.dark, for: .navigationBar)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    scanButton
                }
            }
        }
    }

    // MARK: - Sub-views

    private var statusBanner: some View {
        HStack {
            Circle()
                .fill(statusColour)
                .frame(width: 10, height: 10)
            Text(ble.connectionState.displayName)
                .font(.subheadline)
                .foregroundStyle(.white)
            Spacer()
            if case .connected = ble.connectionState {
                Button("Disconnect") {
                    ble.disconnect()
                }
                .font(.subheadline)
                .foregroundStyle(.red)
            }
        }
        .padding()
        .background(Color.white.opacity(0.05))
    }

    private var scanButton: some View {
        Group {
            if case .scanning = ble.connectionState {
                Button("Stop") { ble.stopScanning() }
                    .foregroundStyle(.red)
            } else {
                Button("Scan") { ble.startScanning() }
                    .foregroundStyle(accentColour)
            }
        }
    }

    private var deviceList: some View {
        List {
            if ble.discoveredPeripherals.isEmpty {
                Text("No devices found — tap Scan to search.")
                    .foregroundStyle(.secondary)
                    .listRowBackground(Color.clear)
            } else {
                ForEach(ble.discoveredPeripherals) { device in
                    DeviceRow(device: device, accentColour: accentColour)
                        .onTapGesture { ble.connect(device.peripheral) }
                        .listRowBackground(
                            Color(red: 0x10/255.0, green: 0x14/255.0, blue: 0x30/255.0)
                        )
                }
            }
        }
        .scrollContentBackground(.hidden)
    }

    private var statusColour: Color {
        switch ble.connectionState {
        case .disconnected:  return .gray
        case .scanning:      return .yellow
        case .connecting:    return .orange
        case .connected:     return .green
        }
    }
}

// MARK: - Device Row

private struct DeviceRow: View {
    let device: DiscoveredPeripheral
    let accentColour: Color

    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text(device.name)
                    .foregroundStyle(.white)
                    .font(.headline)
                Text(device.id.uuidString)
                    .foregroundStyle(.secondary)
                    .font(.caption2)
            }
            Spacer()
            Text("\(device.rssi) dBm")
                .font(.caption)
                .foregroundStyle(accentColour)
        }
        .padding(.vertical, 4)
    }
}

#Preview {
    ConnectionView()
        .environmentObject(BLEManager())
}
