# VAMeter Hardware Spec

## Source priority

1. M5Stack official VAMeter product page
2. M5Stack official VAMeter schematic PDFs
3. Firmware source code
4. Local hardware test logs
5. Inference (must be marked as inference)

## Officially documented specs

- Product: M5Stack VAMeter, SKU K136
- Controller: Stamp-S3 / ESP32S3FN8
- CPU: Xtensa 32-bit LX7 dual-core, 240 MHz
- Flash: 8 MB
- Measurement IC: INA226 x2
- INA226 addresses: 0x40 / 0x41
- Display: 1.3 inch, 240 x 240 px
- Display driver: ST7789
- Current range: 0-5 A
- Voltage range: 5-24 V, 1-24 V in isolation mode
- Current resolution: 2.5 µA / 250 µA
- User interface: rotary encoder switch, reset button, boot button, onboard buzzer
- Base relay rating: DC 5-24 V @ 5 A
- Features: USB MSC/OTG, open-source hardware/software, ESP-IDF support, EZData

## VAMeter-Edu use

VAMeter is used as the measurement core for beginner-friendly voltage/current display, waveform recording, local CSV download, and analog meter reading support.

## Interpretation caution

The documented voltage/current range describes the VAMeter measurement and power path capability.

Do not treat it as an automatic permission for classroom loads, internal power-source output, bulb-test output, or motor-drive output.

Future active-output features must define their own power path, protection, verification procedure, and rollback condition.

## Still to verify in VAMeter-Edu firmware

- Which INA226 path is used for each educational mode
- HAL smoothing and displayed-value conversion
- CSV recorded value source
- Relay control behavior in each mode
- Exact behavior of normal probe UI after recent changes

## Classroom boundary

Do not present VAMeter-Edu as a certified safety instrument.
Use low-voltage classroom experiments only unless separately verified.
