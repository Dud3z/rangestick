# RangeStick

A "SG Pulse Pro"-style shooting buddy for the [M5StickC Plus2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2): shot timer, anti-cant level, stability (wobble) tracker, and a combo view — with battery-conscious defaults and self-updating firmware over WiFi/GitHub.

## Features

- **Shot Timer** — random start delay, buzzer beep, then detects shots either by microphone (amplitude + spectral shape) or by recoil (IMU acceleration spike + rise sharpness). Logs split times and includes a **learn mode** that fires a couple of real shots and suggests good threshold/lockout/ratio values automatically instead of trial-and-error.
- **Anti-Cant Level** — digital spirit level for the rifle's cant (roll) angle. A short two-step calibration (hold level, then tilt briefly) figures out which IMU axis is the bore axis, so it works regardless of how the stick is mounted. Three display styles: horizon line, bubble tube, dial.
- **Stability Tracker** — tracks aiming "wobble" in MOA using a low-pass filtered, orientation-independent algorithm. Three display styles: sparkline history, scatter plot, pulsing circle.
- **Combo View** — anti-cant on top, stability as a slim indicator at the bottom, both sharing the same calibration/session state as the standalone modules.
- **Settings menu** — five categories (Display, Shot Timer, Anti-Cant, Stability, System), all values persisted to flash (NVS).
- **Battery optimizations** — configurable CPU clock (80/160/240 MHz), throttled IMU polling, a cached battery-level readout, and the usual dim/sleep timeouts. WiFi/BT stay off except while actively used.
- **OTA firmware updates over GitHub** — a phone-friendly captive portal for WiFi setup, plus a one-button "check for update" that pulls a new build straight from this repo and flashes it.

## Hardware

- M5StickC Plus2 (ESP32, built-in IMU, microphone, speaker, 3 buttons: A, B, Power)
- USB-C cable for the initial flash

## Controls

| Button | Menu / lists | Editing a value |
|---|---|---|
| B | Up | Value + |
| Power (click) | Down | Value − |
| A (short) | Select / confirm | Leave edit & save |
| A (long / hold) | Back one level, then back to main menu | — |

Module-specific controls (e.g. learn mode, display style cycling) are shown on screen.

## Building & flashing

This is a [PlatformIO](https://platformio.org/) project.

```sh
pio run                          # build
pio run -t upload --upload-port COM5   # flash over USB (adjust the port)
```

The firmware uses a custom OTA-capable partition table (`partitions_ota.csv`, two ~2MB app slots) instead of the default single-app layout, so it can self-update later. No extra libraries are needed beyond what's in `platformio.ini` — WiFi, HTTPS, the web server and the OTA `Update` API all ship with the ESP32 Arduino core.

## Setting up OTA updates

The device checks `ota/version.txt` / downloads `ota/firmware.bin` from this repo's `main` branch via `raw.githubusercontent.com`. To use it on your own fork:

1. Edit `src/Version.h` and set `OTA_REPO_OWNER` / `OTA_REPO_NAME` / `OTA_REPO_BRANCH` to your repo.
2. On the device: **Settings → SYSTEM → Set up WiFi**. It opens an access point called `RangeStick-Setup`; connect to it with your phone, open the browser page it shows (or let the captive portal pop up automatically), pick your network and enter the password.
3. **Settings → SYSTEM → Check for update** connects, compares the published version against the firmware's own `FW_VERSION`, and offers to download + flash if there's a newer one.

### Cutting a release

```sh
# 1. bump the version the firmware reports
#    edit src/Version.h -> FW_VERSION = "1.0.1"
git add src/Version.h
git commit -m "Bump version to 1.0.1"

# 2. tag it to match and push
git tag v1.0.1
git push origin main --tags
```

Pushing a `vX.Y.Z` tag triggers `.github/workflows/release.yml`, which builds the firmware with PlatformIO and commits `ota/firmware.bin` + `ota/version.txt` back to `main` — that's what devices in the field will see on their next update check.

> **Note:** `FW_VERSION` in `src/Version.h` and the git tag must match (the workflow strips the leading `v`). If you forget to bump `FW_VERSION` before tagging, devices will keep reporting an update is "available" after flashing it, since the new binary still carries the old version string.

### Security trade-off

The OTA client uses `WiFiClientSecure::setInsecure()` — the download is encrypted but the server certificate isn't validated. This is a deliberate simplification for a private device that only ever talks to its own repository; it avoids the maintenance burden of pinning/rotating root CAs on-device. If that's not an acceptable trade-off for your use case, swap it for a pinned root CA in `src/OtaUpdater.cpp`.

## Project structure

```
src/
  main.cpp              Boot splash, main menu, button/power-state handling
  AppModule.h            Shared interface for all screens/modules
  Settings.cpp/.h         Settings menu (categories, fields, WiFi/OTA sub-flows)
  AppSettings.cpp/.h      Persisted configuration (NVS)
  ShotTimer.cpp/.h        Shot timer (mic + recoil detection, learn mode)
  CantLevel.cpp/.h        Anti-cant level module (UI)
  CantCalculator.cpp/.h   Anti-cant calibration/angle math (no UI)
  StabilityTracker.cpp/.h Stability module (UI)
  StabilityCalculator.cpp/.h  Wobble calculation (no UI)
  ComboView.cpp/.h        Combined anti-cant + stability screen
  CantStyles.cpp/.h       Shared anti-cant drawing styles
  Canvas.cpp/.h, Theme.cpp/.h  Shared sprite/canvas helpers and color theme
  WifiConfig.cpp/.h       Persisted WiFi credentials
  WifiPortal.cpp/.h       Captive-portal WiFi setup flow
  OtaUpdater.cpp/.h       Update check, download and flashing
  Version.h               Firmware version + OTA repo coordinates
.github/workflows/release.yml   Builds and publishes firmware on version tags
partitions_ota.csv       Dual-OTA-partition layout for the 4MB flash
ota/                      Published version.txt / firmware.bin (written by CI)
```
