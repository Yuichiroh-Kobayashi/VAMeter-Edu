# LovyanGFX Software Notes

## Purpose

Summarize LovyanGFX constraints relevant to VAMeter-Edu firmware work.

LovyanGFX is the graphics foundation used for embedded display rendering.
This file is not a tutorial and is not an implementation request.

## Source priority

1. LovyanGFX official GitHub repository
2. VAMeter-Edu source code
3. Local build / display verification logs
4. Inference, marked as inference

## Role in VAMeter-Edu

LovyanGFX is related to:

- LCD drawing
- sprite / off-screen drawing
- image and asset rendering
- display driver configuration
- desktop/device rendering differences, if used by the project

Do not change LovyanGFX usage while making unrelated measurement, calibration, CSV, or safety changes.

## Officially documented characteristics

LovyanGFX is a display graphics library for:

- ESP32 with SPI / I2C / 8-bit parallel display connections
- ESP-IDF
- Arduino ESP32
- multiple display controllers including ST7789
- sprite / off-screen buffer drawing
- DMA-assisted drawing on supported platforms

## VAMeter-Edu rules

- Do not place measurement logic inside drawing code.
- Do not derive internal values from formatted display strings.
- Do not mix display refactoring with measurement, calibration, CSV, or safety changes.
- Do not change display driver or panel configuration without hardware verification.
- If display behavior changes, verify both device build and, if applicable, desktop simulator behavior.
- If generated assets are changed, record source and verification status.

## Values to keep separate

- measured value
- filtered value
- calibration-adjusted value
- displayed value
- CSV-recorded value

LovyanGFX should only receive values prepared for display.

## Font and glyph coverage rule

VAMeter-Edu display text depends on registered fonts and glyph subsets.

Rules:

- Do not assume Japanese text will render correctly.
- If UI text includes Japanese, verify glyph coverage in desktop build and device build.
- Bring-up UI should prefer ASCII / English text unless Japanese font coverage is explicitly verified.
- If tofu boxes such as `□` appear, treat it as a font coverage issue, not a logic error.
- Do not expand font subsets casually; font changes affect binary size and must be reviewed separately.

## Unverified in VAMeter-Edu

- Exact LovyanGFX configuration used by VAMeter-Edu.
- Whether all rendering paths are shared between desktop and device builds.
- Whether sprite memory usage affects waveform or local download behavior.
- Whether display timing affects measurement update timing.

Mark these as `未確認` or `未検証` until checked.
