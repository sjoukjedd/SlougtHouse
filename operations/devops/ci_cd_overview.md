# VU-AMS CI/CD Pipeline Overview

## Architecture

Three independent GitHub Actions workflows, each triggered only when its relevant subtree changes. No workflow runs on unrelated commits.

```
.github/workflows/
├── firmware.yml       — ESP32-S3 firmware (ubuntu-latest)
├── ios.yml            — VUAMS iOS app (macos-14)
└── simulator.yml      — VUAMSSim iOS simulator app (macos-14)
```

---

## Workflows

### firmware.yml

**Trigger:** push or PR to `main` touching `operations/firmware/**`

**Runner:** `ubuntu-latest`

**What it does:**
1. Checks out the repo with all submodules (recursive).
2. Restores ESP-IDF v5.3.2 from cache (key: `idf-v5.3.2`). On a cache miss, clones the tagged release and runs `install.sh esp32s3`.
3. Sets the IDF target to `esp32s3`.
4. Runs `idf.py build` in `operations/firmware/`.
5. Uploads `operations/firmware/build/vuams_firmware.bin` as the artifact `vuams-firmware` (retained 30 days).

**Cache:** ESP-IDF install + Python tooling lives in `~/esp/esp-idf` and `~/.espressif`. First run is slow (~10 min); subsequent runs restore in seconds.

---

### ios.yml

**Trigger:** push or PR to `main` touching `operations/ios_app/**`

**Runner:** `macos-14` (Apple Silicon, M1)

**What it does:**
1. Checks out the repo.
2. Restores XcodeGen from the Homebrew cache; installs via `brew install xcodegen` on miss.
3. Runs `xcodegen generate` in `operations/ios_app/` to produce `VUAMS.xcodeproj`.
4. Builds scheme `VUAMS` for `platform=iOS Simulator,name=iPhone 15`, Debug configuration, with `CODE_SIGNING_ALLOWED=NO`.
5. Reports success or failure with a clear message.

**Note:** No signing is performed in CI. The build validates that the project compiles cleanly against the simulator SDK.

---

### simulator.yml

**Trigger:** push or PR to `main` touching `operations/ios_simulator/**`

**Runner:** `macos-14` (Apple Silicon, M1)

**What it does:** Identical flow to `ios.yml`, but targets `operations/ios_simulator/` and scheme `VUAMSSim`.

---

## Adding Secrets

For device builds or TestFlight distribution you will need a signing identity. Add secrets in the GitHub repository under **Settings → Secrets and variables → Actions**.

| Secret name        | Purpose                                             |
|--------------------|-----------------------------------------------------|
| `DEVELOPMENT_TEAM` | Apple Developer Team ID (e.g. `A1B2C3D4E5`). Set this in `xcodebuild` via `DEVELOPMENT_TEAM=${{ secrets.DEVELOPMENT_TEAM }}` and remove `CODE_SIGNING_ALLOWED=NO`. |
| `MATCH_PASSWORD`   | If using Fastlane Match for certificate distribution. |
| `APP_STORE_CONNECT_API_KEY` | JSON key for TestFlight uploads via Fastlane. |

To use `DEVELOPMENT_TEAM` in a workflow step:

```yaml
- name: Build (device signing)
  working-directory: operations/ios_app
  run: |
    xcodebuild \
      -scheme VUAMS \
      -destination "generic/platform=iOS" \
      -configuration Release \
      DEVELOPMENT_TEAM=${{ secrets.DEVELOPMENT_TEAM }} \
      build
```

The `project.yml` files already have `DEVELOPMENT_TEAM: ""` as a placeholder — XcodeGen injects whatever value is supplied at build time.

---

## Local Validation

Before pushing, you can reproduce the CI steps locally:

```bash
# Firmware
cd operations/firmware
source ~/esp/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build

# iOS app
cd operations/ios_app
xcodegen generate
xcodebuild -scheme VUAMS -destination "platform=iOS Simulator,name=iPhone 15" \
  -configuration Debug CODE_SIGNING_ALLOWED=NO build

# iOS simulator app
cd operations/ios_simulator
xcodegen generate
xcodebuild -scheme VUAMSSim -destination "platform=iOS Simulator,name=iPhone 15" \
  -configuration Debug CODE_SIGNING_ALLOWED=NO build
```
