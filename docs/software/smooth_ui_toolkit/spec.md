# smooth_ui_toolkit Software Notes

## Purpose

Summarize smooth_ui_toolkit constraints relevant to VAMeter-Edu UI and animation behavior.

smooth_ui_toolkit is a C++ UI toolkit used for animation, menu behavior, numerical display, and UI utilities.
This file is not a tutorial and is not an implementation request.

## Source priority

1. smooth_ui_toolkit official GitHub repository
2. VAMeter-Edu source code
3. Local build / UI verification logs
4. Inference, marked as inference

## Role in VAMeter-Edu

smooth_ui_toolkit may be relevant to:

- spring / easing animation
- RGB color transition
- animated menu behavior
- selector/menu abstraction
- numerical display animation
- common utility templates
- UI timing through UI HAL

Do not change smooth_ui_toolkit behavior during unrelated measurement, calibration, CSV, or safety work.

## Officially documented characteristics

The smooth_ui_toolkit README describes:

- Spring and Easing animation interpolation
- RGB color transition interpolation
- LVGL C++ wrapper
- NumberFlow-style components
- animated menu abstractions
- color mixing / conversion methods
- Signal and RingBuffer utilities
- Vector, clamp, map_range, and other math utilities
- UI HAL functions such as `get_tick()` and `delay()`

## VAMeter-Edu rules

- UI animation must not change measurement meaning.
- Display animation must not be the source of CSV values.
- Menu animation must not obscure the selected measurement mode.
- Do not create classroom UI that depends on timing-sensitive animation for safety.
- Do not mix UI animation refactoring with measurement, calibration, CSV, or active-output changes.
- If display text or menu behavior changes, update relevant documentation.

## UI HAL caution

smooth_ui_toolkit can define UI timing through UI HAL functions.

VAMeter-Edu rule:

- Do not change UI tick or delay behavior without checking measurement update timing, waveform recording, and app responsiveness.
- If timing impact is unknown, write `未確認`.

## Number display caution

Animated number display is presentation.

VAMeter-Edu must keep these separate:

- measured value
- filtered value
- calibration-adjusted value
- displayed value
- CSV-recorded value

Do not use animated display state as the source for measurement, calibration, or CSV recording.

## Unverified in VAMeter-Edu

- Exact smooth_ui_toolkit components used by each app.
- Whether NumberFlow-style display is used in VAMeter-Edu.
- Whether UI HAL is overridden in device build.
- Whether animation timing affects waveform display or recorder timing.
- Whether desktop and device builds share the same UI timing behavior.

Mark these as `未確認` until checked in source code.
