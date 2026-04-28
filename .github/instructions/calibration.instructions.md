---
applyTo: "app/hal/**,app/apps/app_settings/**,app/apps/app_edu_current/**,app/apps/app_edu_volt/**,docs/operations/calibration_test_log.md,docs/standards/calibration_policy.md,docs/hardware/VAMeter/measurement_path.md"
---

# Calibration instructions

Follow `AGENTS.md` first.

## Phase 1 boundary

During Phase 1, do not create `docs/manuals/`, `site/`, `.github/workflows/`, or `.agents/skills/` unless the task explicitly requests them.

## Scope

These instructions apply to calibration, correction, analog meter support, display scaling, probe mode, and measurement profile changes.

## Core rule

Do not treat calibration constants as magic numbers.

Whenever adding or changing calibration behavior, document:

- target mode
- target hardware or analog meter
- measurement range
- reference instrument or method
- raw value
- corrected value
- display value
- remaining error or uncertainty
- firmware version, if known
- verification date, if known

If the calibration is not verified, write `未検証`.

## Display and measurement separation

Keep these concepts separate:

- measured value
- filtered value
- calibration-adjusted value
- analog-meter-aligned value
- displayed value
- CSV recorded value

Do not change CSV output merely to match a classroom display unless the data format change is explicitly intended and documented.

## Analog meter support

When supporting analog meter reading practice:

- Do not hide the actual measurement basis.
- Make clear whether a value is raw, corrected, or answer-check display.
- Avoid hard-coding values for one school or one instrument without documenting the reason.
- If a setting depends on a specific meter model or range, record it in documentation or operations logs.

## Required documentation

Calibration-related changes should update one or more of:

- `docs/operations/calibration_test_log.md`
- `docs/standards/calibration_policy.md`
- `docs/manuals/user_manual.md`, if user-facing manuals exist and the behavior is user-visible
- `docs/manuals/troubleshooting.md`, if user-facing manuals exist and failure behavior changes

If these files do not exist yet, propose creating them rather than inventing unsupported behavior.
