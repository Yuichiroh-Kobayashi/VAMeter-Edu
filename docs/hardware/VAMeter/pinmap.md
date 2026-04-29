# VAMeter Pin Map

## Official pin map entries

| Function | Signal | ESP32S3FN8 GPIO | Source | Status |
|---|---|---:|---|---|
| I2C SDA | FSDA | GPIO5 | M5Stack official pinmap | 公式確認 |
| I2C SCL | FSCL | GPIO6 | M5Stack official pinmap | 公式確認 |
| INA226 0x40 ALERT | ALERT1 | GPIO41 | M5Stack official pinmap | 公式確認 |
| INA226 0x41 ALERT | ALERT2 | GPIO40 | M5Stack official pinmap | 公式確認 |
| Buzzer PWM | BUZ_PWM | GPIO14 | M5Stack official pinmap | 公式確認 |
| Expansion | EXT_G10 | GPIO10 | M5Stack official pinmap | 公式確認 |
| Expansion | EXT_G9 | GPIO9 | M5Stack official pinmap | 公式確認 |
| Expansion | EXT_G8 | GPIO8 | M5Stack official pinmap | 公式確認 |
| Expansion | EXT_G42 | GPIO42 | M5Stack official pinmap | 公式確認 |
| Base relay | G10_REL | GPIO10 | M5Stack official pinmap / Base | 公式確認 |
| PORT.CUSTOM yellow | G8 | GPIO8 | M5Stack official pinmap | 公式確認 |
| PORT.CUSTOM white | G9 | GPIO9 | M5Stack official pinmap | 公式確認 |

## GPIO10 caution

GPIO10 appears both as `EXT_G10` in the expansion pin map and as `G10_REL` for the VAMeter Base relay.

Do not treat GPIO10 as a free GPIO in VAMeter-Edu without verifying:

1. whether the Base is attached
2. whether relay control is enabled in firmware
3. whether the proposed use conflicts with relay behavior
4. the affected hardware nets

## Implementation rule

Do not introduce new GPIO use without checking:

1. this pin map
2. VAMeter schematic
3. existing firmware source
4. hardware test log

If not verified, mark `未検証`.

## Code-name alignment

Use code-aligned names when discussing Base Grove pins:

| Code name | GPIO | Physical note |
|---|---:|---|
| `HAL_PIN_BASE_GROVE_IOA` | GPIO9 | Port.A C9 / White candidate |
| `HAL_PIN_BASE_GROVE_IOB` | GPIO8 | Port.A C8 / Yellow candidate |
| `HAL_PIN_BASE_RELAY_CTRL` | GPIO10 | Base relay control / G10_REL risk |

Avoid ambiguous names such as `Port.A signal 1` and `Port.A signal 2`.
Use `C9(White)` and `C8(Yellow)` when referring to the observed connector pins.
