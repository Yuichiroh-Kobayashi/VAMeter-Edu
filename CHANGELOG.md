# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
