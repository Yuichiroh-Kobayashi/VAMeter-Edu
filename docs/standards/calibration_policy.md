# Calibration Policy

## Scope

Applies to analog-meter alignment, displayed-value correction, CSV-recorded values, and any future automatic calibration.

## Rules

- Keep calibration separate from measurement acquisition.
- Record whether calibration affects display only, CSV only, or both.
- Do not hard-code a classroom-specific correction without a log.
- If calibration is not verified on hardware, write `未検証`.

## Required log fields

- date
- branch / commit
- firmware version
- target mode
- target hardware
- reference instrument or method
- measurement range
- affected value path: display only / CSV only / both / other
- raw measured value, if available
- filtered value, if available
- calibration-adjusted value, if available
- displayed value, if available
- CSV-recorded value, if available
- before behavior
- after behavior
- remaining error or uncertainty
- judgment / next action
