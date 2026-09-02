# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2026-09-02

### Added

- Stable device-hosted same-origin Viewer for direct browser voltage/current observation
- Student / Professional display modes and Voltage / Current / Both profiles
- 10 / 30 / 60 second device-time display windows that preserve gaps and do not convert invalid samples to zero
- System-aware Light / Dark theme with a page-lifetime manual override

### Changed

- Developed the browser-based contest demonstration into the stable classroom-oriented v2.0.0 workflow
- Finalized fail-closed Viewer and AssetPool identity admission
- Established stable offline device-hosted operation without internet or cloud services

### Validated

- Current host regression suite passed 26/26 tests for the release source
- Clean release builds completed with ESP-IDF v5.1.6
- Physical VAMeter/browser validation completed on Windows Edge and iPad Safari
- The post-tag build was proven semantically equivalent to the qualified candidate except for release and build metadata

### Known limitations

- Issue #3 remains open; gap-free CSV and uniform sampling are not guaranteed
- Static IRAM usage is `16383 / 16384` bytes; resource headroom is extremely limited and remains technical debt
- Issue #8 remains future work for the multi-client product policy
- Browser and streaming validation is not an electrical calibration certificate

## v2.0.0-beta.1 — 2026-08-21

### Added

- Device-hosted same-origin Viewer with Student / Professional modes, Voltage / Current / Both profiles, and 10 / 30 / 60 second display windows

### Changed

- Fixed the fallback educational recording duration at five seconds; recorded CSV uses the existing local download workflow

### Validated

- Physical Viewer validation on Windows Edge 151 and an iPad 7th generation running iPadOS 18.7.9 Safari; these browser tests did not re-qualify electrical measurement accuracy

### Known limitations

- Application and AssetPool binaries must be used as the matched `v2.0.0-beta.1` pair
- Issue #3 remains open; gap-free CSV and uniform sampling are not claimed
- Static IRAM usage is `16383 / 16384` bytes; resource headroom is extremely limited and must be reviewed before adding firmware features
- This is a beta prerelease, not stable `v2.0.0`

## [1.1.0] - 2026-01-01

### Added

- **Local CSV Download**: Replace cloud upload (EzData) with local download via QR code
  - Device operates as WiFi AP and hosts HTTP server
  - Scan QR code to download CSV files directly to smartphone/PC
  - Available from both File Manager and Waveform app after recording
- **AP Suffix Setting**: Configure WiFi AP name suffix (01-40) for classroom use
  - Multiple devices can be distinguished by unique AP names (e.g., `M5-VAMeter-01`)
  - Setting persists across power cycles (stored in NVS)
  - Accessible via Settings → Network → AP Suffix

### Fixed

- **Waveform Recording Start Issue**: Fixed encoder click not triggering recording after transitioning from Volt/Current meter pages

### Changed

- Removed EzData upload feature (replaced with local download)
- Improved QR download page UI for better usability

### Technical

- Added `NVS_KEY_AP_SUFFIX` key definition in `types.h`
- Created shared component `SYSTEM::UI::CreateDownloadQRPage()` for code reuse

## [1.0.0] - 2025-12-31

### Added

- Initial educational firmware release
- Voltage and Current meter apps with simplified UI
- Guide screens with connection diagrams
- Japanese localization
- Waveform recording and playback
- Probe mode setting (Normal/Training)
