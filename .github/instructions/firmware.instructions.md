---
applyTo: "app/**,platforms/**"
---

# Firmware instructions

Follow `AGENTS.md` first.

Before editing firmware, read these files if they exist:

1. `AGENTS.md`
2. `docs/canon/minimum_constraints.md`
3. `docs/standards/measurement_safety_policy.md`
4. `docs/standards/coding_standard.md`
5. `docs/standards/naming_units.md`
6. `docs/architecture/app_map.md`
7. `docs/architecture/measurement_state_machine.md`
8. `docs/architecture/probe_mode_matrix.md`
9. `docs/hardware/VAMeter/pinmap.md`

If a referenced document does not exist, do not invent its contents. Write `未作成` or propose creating it as part of the current documentation task.

If the task changes app lifecycle, display rendering, UI animation, or embedded UI dependency behavior, also read the relevant file under `docs/software/`.

## Scope

These instructions apply to firmware source code, platform code, HAL code, app code, and device build files.

## Safety

- Preserve safe power-on behavior.
- Do not introduce active output behavior without a safety state model.
- If active output behavior is added later, the default state must be output disabled.
- Fault state must force output off.
- Fault clear must require explicit user operation.
- Do not mix safety-critical output changes with unrelated UI cleanup.
- Do not mix UI cleanup, measurement logic, calibration, and safety behavior in one change.

## Architecture

Keep these domains separated:

1. UI / display
2. measurement acquisition
3. filtering / smoothing
4. calibration / correction
5. CSV recording / local download
6. settings persistence
7. hardware control
8. safety state management

Do not add measurement logic directly into view rendering code unless the existing architecture requires it. If you must do so, document the reason.

Do not scatter hardware constants. Put hardware-specific constants in HAL, board, config, or a clearly named central definition.

## Naming

Use names that include physical quantity and unit where possible.

Good:

```cpp
float measuredVoltageV;
float measuredCurrentA;
float displayedVoltageV;
float currentLimitA;
uint32_t sampleIntervalMs;
```

Bad:

```cpp
float value;
float data;
float current;
float limit;
uint32_t interval;
```

## Measurement values

When changing measurement code, distinguish:

- raw measured value
- filtered value
- calibration-adjusted value
- displayed value
- CSV recorded value

Do not silently change the meaning of an existing variable, CSV column, or display field.

CSV values must not silently change when display formatting changes.

If a value source is unknown, write `未確認`.

If hardware behavior is not verified, write `未検証`.

## Classroom workflow

Preserve existing classroom workflows unless the task explicitly changes them.

Pay particular attention to:

- menu flow
- voltage/current fixed display modes
- guide screens
- waveform recording
- local CSV download
- disabled OTA Upgrade
- disabled Factory Reset

When a user-visible behavior changes, update documentation in the same change or state clearly why documentation was not changed.

## Build and verification

After firmware changes, run or document why you could not run:

```bash
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py build
```

For changes affecting assets or desktop simulator behavior, also consider:

```bash
mkdir build && cd build
cmake .. && make -j8
```

If a command fails, report:

- command
- result
- likely cause
- next action

If verification is not performed, write `未検証` or `未確認` explicitly.

## Change scope
- Keep changes small and scoped.
- Avoid unrelated refactors.
- Do not mix generated asset changes with logic changes unless necessary.
- Do not implement future work unless explicitly requested.
- Do not document future work as implemented.

## Motor Observe active-output rule

If a task touches Motor Observe, PWM, GPIO8/GPIO9/GPIO10, Port.A, MAKER-DRIVE, motor output, or bring-up UI:

Read first:

- `docs/architecture/motor_observe_pwm_backend_design.md`
- `docs/hardware/MAKER_DRIVE/spec.md`
- `docs/hardware/MAKER_DRIVE/interface_port_a_1ch.md`
- `docs/hardware/VAMeter/pinmap.md`
- `docs/hardware/VAMeter_Base/relay_control.md`
- `docs/operations/port_a_verification_plan.md`
- `docs/operations/safety_test_log.md`

Hard rules:

- Do not bypass `SafetyController` or `BackendController`.
- Do not use GPIO10 for Motor Observe PWM.
- Do not use Base relay as a safety disconnect.
- Do not implement High/High coast without a separate review.
- Do not implement PWM/High or High/PWM.
- Do not connect MAKER-DRIVE or motor in code assumptions.
- UI work must run host-side test, desktop build, and device build.
