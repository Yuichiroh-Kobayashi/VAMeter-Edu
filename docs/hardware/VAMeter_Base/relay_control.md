# VAMeter Base Relay Control

## Role

The VAMeter Base relay controls the Base-side output path.
Do not assume it disconnects every measurement path or USB-C power path.

Do not use the Base relay as evidence that USB-C input/output current is cut off unless verified on the actual hardware path.

Exact affected nets must be verified from:

- VAMeter Base schematic
- VAMeter schematic
- firmware relay control code
- hardware test log

## Officially documented item

- Base relay rating: DC 5-24 V @ 5 A
- Relay control pin: G10_REL / GPIO10

## Current rule

The relay must not be used as a safety claim until the exact affected nets and fault behavior are verified.

GPIO10 is also listed as `EXT_G10` in the VAMeter expansion pin map.
Do not assign GPIO10 to another purpose unless the Base relay interaction has been checked and recorded.

## Motor Observe caution

Motor Observe must not use the Base relay as a safety disconnect.

Rules:

- Do not use `HAL::SetBaseRelay()` for Motor Observe safety.
- Do not assume GPIO10 disconnects GPIO8/GPIO9, USB-C, or all measurement paths.
- GPIO10 / G10_REL / EXT_G10 must be treated as relay-related and excluded from Motor Observe PWM candidates.
- If a future design requires relay interaction, create a separate design review before implementation.

## Future current-limit cutoff

If current-limit cutoff is implemented later:

1. define the monitored current value source
2. define threshold and hysteresis
3. define relay OFF behavior
4. verify affected nets
5. record test result in `docs/operations/safety_test_log.md`

If any item is not verified, write `未検証`.
