# VUAMSSim — Device Simulator App

## Xcode project aanmaken

1. Installeer XcodeGen: `brew install xcodegen`
2. Navigeer naar deze map: `cd operations/ios_simulator`
3. Genereer project: `xcodegen generate`
4. Open: `open VUAMSSim.xcodeproj`

Vereisten: Xcode 15+, iOS 17+ device of simulator

> **Let op:** BLE peripheral mode (simulator app) vereist echte hardware — deze app werkt niet in de iOS Simulator.
> Vul `DEVELOPMENT_TEAM` in `project.yml` in met je Apple Developer Team ID voor signing.
