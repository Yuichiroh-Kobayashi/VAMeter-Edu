# mooncake Software Notes

## Purpose

Summarize mooncake constraints relevant to VAMeter-Edu app structure.

mooncake is the multi-app management and scheduling framework used by the firmware.
This file is not a tutorial and is not an implementation request.

## Source priority

1. mooncake official GitHub repository
2. VAMeter-Edu source code
3. Local build / runtime verification logs
4. Inference, marked as inference

## Role in VAMeter-Edu

mooncake is relevant to:

- application installation
- application opening / closing
- application lifecycle callbacks
- launcher behavior
- app switching
- background or sleeping behavior, if used
- main update loop behavior

Do not change app lifecycle behavior during unrelated UI, measurement, calibration, CSV, or safety work.

## Officially documented characteristics

mooncake is described as a multi-app management and scheduling framework for MCUs.

Key concepts include:

- `installApp()`
- `openApp()`
- `closeApp()`
- `update()`
- `AppAbility`
- lifecycle callbacks such as `onOpen()` and `onRunning()`

mooncake centralizes lifecycle callback execution through its `update()` method.

## Thread-safety caution

The mooncake README states that APIs are not thread-safe.

VAMeter-Edu rule:

- Do not call mooncake app-management APIs from a new task, interrupt, callback, or background context unless synchronization and existing firmware behavior are verified.
- If thread or task interaction is unclear, write `未確認`.

## VAMeter-Edu rules

- Do not bypass the existing launcher/app lifecycle unless explicitly requested.
- Do not make an app start active output merely by opening the app.
- Do not mix app lifecycle changes with measurement logic changes.
- Do not implement Training Probe answer-check behavior in Normal Probe UI unless explicitly requested.
- Do not change app switching behavior without checking user-facing documentation.

## Active-output caution

Future active-output features such as internal power source, bulb test output, or motor observation must not become active merely through `openApp()`.

Opening an app may prepare UI state, but physical output enable must require explicit user action and safety-state verification.

## Unverified in VAMeter-Edu

- Exact lifecycle methods used by each VAMeter-Edu app.
- Whether VAMeter-Edu uses only AppAbility or also extension abilities.
- Whether app close/open behavior resets measurement state.
- Whether local CSV download uses an app, service, or utility path.
- Whether waveform recorder state survives app switching.

Mark these as `未確認` until checked in source code.
