# VAMeter-Edu

**Designed for real classrooms.**  
Educational firmware for [M5Stack VAMeter](https://docs.m5stack.com/en/products/sku/K136), customized for hands-on electricity learning in Japanese middle-school classrooms. VAMeter-Edu connects real circuit operation with digital readings, waveform observation, short explicit recording, and local CSV graphing.

> **日本語版:** [README_ja.md](README_ja.md)

> **Latest stable release:** [`v2.0.0`](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/releases/tag/v2.0.0) (2026-09-02)

> **M5Stack Global Innovation Contest 2026 — Special Mentions - Educational Impact Award**
>
> VAMeter-Edu was recognized for its educational impact in the [M5Stack Global Innovation Contest 2026](https://m5stack.com/global-innovation-contest-2026/results). The contest build demonstrated real-time voltage/current observation in a browser. VAMeter-Edu has since developed from that demonstration into a stable release designed for real classroom use, with direct device-hosted viewing, more predictable connection behavior, reproducible validation, and classroom-oriented workflows.

## Features

### Educational Enhancements

- **Simplified Menu**: Voltage Meter → Current Meter → USB-C Power Monitor → Settings
- **Guide Screens**: Connection diagrams showing proper circuit setup for beginners
- **Fixed Display Mode**: Locks display to selected measurement (prevents accidental page switching)
- **Japanese Localization**: Full Japanese UI support

### Device-hosted Viewer (`v2.0.0`)

VAMeter now provides its own Wi-Fi access point and same-origin Web Viewer. A student or teacher connects directly to the device and opens the Viewer without an internet connection or cloud account.

- **Student / Professional** display modes
- **Voltage / Current / Both** measurement profiles
- **10 / 30 / 60 s** display windows
- Device-timestamp waveform rendering without filling gaps or turning invalid samples into zero
- System-aware **Light / Dark** theme with a page-lifetime manual override
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

### v2.0.0 (2026-09-02)

[`v2.0.0`](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/releases/tag/v2.0.0) is the stable classroom-oriented release of the device-hosted Viewer and five-second educational CSV workflow described above.

The application and AssetPool binaries are a matched release pair. **Do not mix binaries from different VAMeter-Edu versions.** Download the files from the stable Release and verify them with the published `SHA256SUMS` before writing them.

| Release identity | Value |
|---|---|
| Release URL | [`v2.0.0`](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/releases/tag/v2.0.0) |
| Firmware tag | `v2.0.0` |
| Annotated tag object | `f862754cadd3ddf485b0ed5b9312507c7df15853` |
| Firmware commit | `ee4da1b5e5e238fbc66a9d9a49f4d051c1ca986b` |
| Firmware tree | `f80a0caaa213a965033f6773ea2d3f41af436807` |
| Viewer source commit | `e1ebdb1cde8585a37447a66f4c8183654f4c3cda` |
| Viewer source tree | `8f8426e9af1649f68e66e4f8f432d1b91452e38d` |
| D2B repository reference commit | `b30ad676922af73448952d5a9cac312467a944f9` |
| D2B repository commit tree | `22d644546a8ae76559bb7f1ec01fa737c7160886` |
| Viewer copied `reference/browser/src` subtree | `6e5b4844548c1355dea7e5cbbcb1200c9d2335fd` |
| Viewer bundle | `4422530b6e1ba9549dd4bef2e3bb2c183d8fced49ed2d8d695d2a04a4aa7c2af` |
| app SHA-256 | `4a54b7addb69c89497de585a200de5c3976b21c2a0a2e50ee4fb0d6d0e51198a` |
| AssetPool SHA-256 | `3a587a04127a5eab4df0d0714e37e214029bcedbbeb6a616e8426c6e9aa1c1fc` |
| `SHA256SUMS` SHA-256 | `40558a943ef194dd7931cc5fe8fe8a9517928dcf09c9443af5d05bbf4abf84e0` |

Current limitations:

- [Issue #3](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3) remains open; neither gap-free CSV nor uniform sampling is guaranteed.
- Static IRAM usage is `16383 / 16384` bytes. Resource headroom is extremely limited and remains technical debt.
- Multi-client product policy remains future work in [Issue #8](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/8).
- Browser and streaming validation is not an electrical calibration certificate.

### v2.0.0-beta.1 (2026-08-21; historical prerelease)

This historical beta introduced the device-hosted same-origin Viewer, the Student / Professional and Voltage / Current / Both views, selectable 10 / 30 / 60 second display windows, and the five-second educational CSV workflow.

The published application and AssetPool binaries are a matched release pair. **Do not mix either binary with another version.** Verify the downloaded files before writing them:

| Release identity | Value |
|---|---|
| Firmware released commit | `6f769485f5f1de119d7b7edcc38f724620dc2ac7` |
| Viewer commit | `105bca2616ef372fe23ac0797f58b5c7383ee20c` |
| D2B commit | `5411ba59a12882345d32218eda367bd6ba35ef5d` |
| Viewer bundle | `cbcbd7eab111b49c0c6119b22a7f50ae55981933fd799abfd98d92d0dc5d96e5` |
| app SHA-256 | `9b872ea5cc483b361bba9550e1878b9036d205ee830fd84a0e321d3f3a732423` |
| AssetPool SHA-256 | `1df4b81fba8b3f16baf1331f015cdb1fdc7214d66a215657ef752673b43c1c41` |

Historical notes:

- [Issue #3](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3) is open; neither gap-free CSV nor uniform sampling is guaranteed.
- Static IRAM usage is `16383 / 16384` bytes; resource headroom is extremely limited and must be reviewed before adding firmware features.
- This beta does not represent the stable `v2.0.0` release.

### v1.1.0 (2026-01-01)

- **Local CSV download**: Replaced cloud upload with QR-code based local download
- **AP suffix setting**: Device identification for multi-unit classroom use (01–40)
- **Bug fix**: Fixed an issue where waveform recording did not start after returning from Voltage/Current meter screens

See [CHANGELOG.md](CHANGELOG.md) for details.

## Roadmap

- [Reduce recorder sampling stalls during synchronous FAT writes](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3)
- [Add reverse-current indication and Training-mode protection](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/15)
- [Multi-client product policy](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/8)
- [Analog-meter answer-check display correction](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/9)
- [Internally powered V-I logger research (paused pending hardware redesign)](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/11)

## Contributing

Bug reports, documentation improvements, and focused pull requests are welcome.

Please see [CONTRIBUTING.md](CONTRIBUTING.md) before opening an Issue or PR. If you are unsure whether a problem belongs to the firmware, the browser Viewer, or the generic D2B protocol, describe where you encountered it and maintainers can help route or cross-link it.

日本語: [CONTRIBUTING_ja.md](CONTRIBUTING_ja.md)

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
