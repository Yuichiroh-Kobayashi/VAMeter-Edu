# VAMeter Measurement Path

## Purpose

Prevent AI agents from confusing sensor values, display values, calibration-adjusted values, and CSV values.

## Known hardware

- INA226 x2 are used for different resolutions and ranges.
- INA226 addresses are 0x40 and 0x41.
- VAMeter official documentation describes current detection resolutions of 2.5 µA and 250 µA.

## To verify in firmware

| Item | Status |
|---|---|
| INA226 address used for low-current path | 未確認 |
| INA226 address used for high-current path | 未確認 |
| HICUR behavior and threshold | 未確認 |
| raw-to-filtered conversion | 未確認 |
| filtered-to-displayed conversion | 未確認 |
| displayed-to-CSV relationship | 未確認 |
| calibration-adjusted value source | 未確認 |

## Rules

- Do not infer sensor path from UI color or app name.
- Do not use displayed strings as calculation sources.
- Do not change CSV source without updating waveform CSV policy and validation log.
- Do not treat INA226 ALERT pins as measurement-value sources unless firmware behavior is verified.
