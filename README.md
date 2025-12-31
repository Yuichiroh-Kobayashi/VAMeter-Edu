# VAMeter-Edu

Educational firmware for [M5Stack VAMeter](https://docs.m5stack.com/en/products/sku/K136), customized for Japanese classroom use.

VAMeter-Edu provides intuitive guidance, flexible configuration, and adaptable modes for hands-on student learning about electricity.

## Features

### Educational Enhancements

- **Simplified Menu**: Voltage Meter → Current Meter → USB-C Power Monitor → Settings
- **Guide Screens**: Connection diagrams showing proper circuit setup for beginners
- **Probe Mode Setting**: Switch between Normal Probe and Training Probe modes
- **Fixed Display Mode**: Locks display to selected measurement (prevents accidental page switching)
- **Japanese Localization**: Full Japanese UI support

### Safety Features

- **OTA Upgrade Disabled**: Prevents accidental firmware changes in classroom
- **Factory Reset Disabled**: Protects device configuration from being reset

### Inherited from VAMeter

- High-precision current detection (2.5 µA / 250 µA resolution)
- Waveform recording and display
- EzData cloud upload support
- USB MSC mode for data export

## Hardware

VAMeter-Edu uses the M5Stack VAMeter hardware with optional custom enclosure:

- **Normal Probe**: Standard test leads via 4mm banana jacks
- **Training Probe**: Custom probe with RCA connectors for analog meter practice

For hardware details, see the [Hackster.io project page](https://www.hackster.io/Yuichiroh-Kobayashi/vameter-edu-easy-tester-for-everyone-learning-electricity-9d06c6).

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

[ESP-IDF v5.1.3](https://docs.espressif.com/projects/esp-idf/en/v5.1.3/esp32s3/index.html)

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
