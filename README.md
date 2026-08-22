# VAMeter-Edu

**Designed for real classrooms.**  
Educational firmware for [M5Stack VAMeter](https://docs.m5stack.com/en/products/sku/K136), customized for hands-on electricity learning in Japanese middle-school classrooms. VAMeter-Edu connects real circuit operation with digital readings, waveform observation, short explicit recording, and local CSV graphing.

> **Latest prerelease:** [`v2.0.0-beta.1`](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/releases/tag/v2.0.0-beta.1) (2026-08-21). This is a physically tested beta, not the stable `v2.0.0` release.

## Features

### Educational Enhancements

- **Simplified Menu**: Voltage Meter → Current Meter → USB-C Power Monitor → Settings
- **Guide Screens**: Connection diagrams showing proper circuit setup for beginners
- **Fixed Display Mode**: Locks display to selected measurement (prevents accidental page switching)
- **Japanese Localization**: Full Japanese UI support

### Device-hosted Viewer (`v2.0.0-beta.1`)

VAMeter now provides its own Wi-Fi access point and same-origin Web Viewer. A student or teacher connects directly to the device and opens the Viewer without an internet connection or cloud account.

- **Student / Professional** display modes
- **Voltage / Current / Both** measurement profiles
- **10 / 30 / 60 s** display windows
- Device-timestamp waveform rendering without filling gaps or turning invalid samples into zero
- Physical browser validation on **Microsoft Edge 151 for Windows**
- Physical smoke validation on **iPad (7th generation), iPadOS 18.7.9, Safari**

These browser tests validated the device-hosted streaming and lifecycle behavior. They did not re-qualify electrical measurement accuracy or provide a new calibration certificate.

### Educational Recording and Local Data Download

The current fallback recorder captures a manually triggered **5-second educational record**. Students can scan the on-screen **QR code** and download the CSV directly from the device—**no internet connection or cloud account required**.

- Easier to use under restricted school networks
- CSV can be viewed, processed, and shared on student devices
- **EzData cloud upload was removed in v1.1.0** and replaced by this local download workflow
- CSV sampling can stall during synchronous FAT writes; [Issue #3](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3) remains open and **gap-free CSV is not claimed**

For classrooms using multiple devices, you can set an **AP suffix (01–40)** to help distinguish units.

### Safety Features

- **OTA Upgrade Disabled**: Prevents accidental firmware changes in classroom
- **Factory Reset Disabled**: Protects device configuration from being reset

### Inherited from VAMeter

- High-precision current detection (2.5 µA / 250 µA resolution)
- Waveform recording and display
- USB MSC mode for data export

## Hardware

VAMeter-Edu uses the M5Stack VAMeter hardware with optional custom enclosure:

- **Normal Probe**: Standard test leads via 4mm banana jacks
- **Training Probe**: Custom probe with RCA connectors for analog meter practice

For hardware details, see the [Hackster.io project page](https://www.hackster.io/Yuichiroh-Kobayashi/vameter-edu-easy-tester-for-everyone-learning-electricity-9d06c6).

## Firmware

### v2.0.0-beta.1 (2026-08-21)

This beta adds the device-hosted same-origin Viewer, the Student / Professional and Voltage / Current / Both views, selectable 10 / 30 / 60 second display windows, and the 5-second educational CSV workflow described above.

The published application and AssetPool binaries are a matched release pair. **Do not mix either binary with another version.** Verify the downloaded files before writing them:

| Release identity | Value |
|---|---|
| Firmware released commit | `6f769485f5f1de119d7b7edcc38f724620dc2ac7` |
| Viewer commit | `105bca2616ef372fe23ac0797f58b5c7383ee20c` |
| D2B commit | `5411ba59a12882345d32218eda367bd6ba35ef5d` |
| Viewer bundle | `cbcbd7eab111b49c0c6119b22a7f50ae55981933fd799abfd98d92d0dc5d96e5` |
| app SHA-256 | `9b872ea5cc483b361bba9550e1878b9036d205ee830fd84a0e321d3f3a732423` |
| AssetPool SHA-256 | `1df4b81fba8b3f16baf1331f015cdb1fdc7214d66a215657ef752673b43c1c41` |

Current limitations:

- [Issue #3](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3) is open; neither gap-free CSV nor uniform sampling is guaranteed.
- Static IRAM usage is `16383 / 16384` bytes; resource headroom is extremely limited and must be reviewed before adding firmware features.
- This beta does not represent the stable `v2.0.0` release.

### v1.1.0 (2026-01-01)

- **Local CSV download**: Replaced cloud upload with QR-code based local download
- **AP suffix setting**: Device identification for multi-unit classroom use (01–40)
- **Bug fix**: Fixed an issue where waveform recording did not start after returning from Voltage/Current meter screens

See [CHANGELOG.md](CHANGELOG.md) for details.

## Roadmap

- [Student single Start / Stop control and above-the-fold UX](https://github.com/Yuichiroh-Kobayashi/Device-to-Browser-Viewer/issues/1)
- [Multi-client product policy](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/8)
- [Analog-meter answer-check display correction](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/9)

## Project Structure

```bash
.
├── app
│   ├── apps                    # Applications
│   │   ├── app_edu_current     # Current meter (Edu wrapper)
│   │   ├── app_edu_volt        # Voltage meter (Edu wrapper)
│   │   ├── app_launcher        # Main menu with guide screens
│   │   ├── app_power_monitor   # Measurement display
│   │   ├── app_settings        # Settings (incl. Probe Mode)
│   │   ├── app_waveform        # Waveform recorder
│   │   └── utils
│   ├── assets                  # Fonts, images, localization
│   └── hal                     # Hardware abstraction layer
└── platforms
    ├── desktop                 # Desktop simulator (SDL)
    └── vameter                 # ESP-IDF project for device
```

## Build

### Prerequisites

#### Fetch Dependencies

```bash
python ./fetch_repos.py
```

### Desktop Build (Simulator)

#### Tool Chains

```bash
sudo apt install build-essential cmake libsdl2-dev
```

#### Build & Run

```bash
mkdir build && cd build
cmake .. && make -j8
cd desktop && ./app_desktop_build
```

### Device Build (ESP-IDF)

#### Tool Chains

[ESP-IDF v5.1.6](https://docs.espressif.com/projects/esp-idf/en/v5.1.6/esp32s3/index.html)

#### Build

```bash
cd platforms/vameter
idf.py build
```

#### Flash

```bash
idf.py -p <YourPort> flash -b 1500000
```

##### Flash AssetPool

```bash
parttool.py --port <YourPort> write_partition --partition-name=assetpool --input "path/to/AssetPool-VAMeter.bin"
```

If you run desktop build first, you can find `AssetPool-VAMeter.bin` at `../../build/desktop/AssetPool-VAMeter.bin`.

## Credits

- **Developer**: Yuichiroh-Kobayashi
- **Special Thanks**: [@M5Stack](https://github.com/m5stack) for the original VAMeter firmware

## Related Links

- [Hackster.io Project](https://www.hackster.io/Yuichiroh-Kobayashi/vameter-edu-easy-tester-for-everyone-learning-electricity-9d06c6)
- [Original VAMeter Firmware](https://github.com/m5stack/VAMeter-Firmware)
- [M5Stack VAMeter Product Page](https://docs.m5stack.com/en/products/sku/K136)

## License

MIT License - see [LICENSE](LICENSE) file for details.
