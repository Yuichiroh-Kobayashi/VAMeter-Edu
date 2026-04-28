# Measurement and Safety Policy

## Scope

This policy applies to measurement behavior, relay behavior, current-limit plans, calibration, CSV output, and any future active-output feature.

## Measurement value separation

Firmware and documentation must distinguish:

- raw measured value
- filtered value
- calibration-adjusted value
- displayed value
- CSV-recorded value

Do not use vague variable names such as `value`, `data`, or `current` when physical quantity and unit matter.

## Safe defaults

- Power-on must not enable any active output.
- Fault states must force outputs or relay-controlled outputs to the safest verified state.
- Fault clear must require explicit user action.
- Timeout behavior must return to a safe state when active output is implemented.
- If active output is not implemented, do not invent an active-output state machine during unrelated measurement or UI work.

## Relay caution

Do not assume the VAMeter Base relay disconnects every measurement path or USB-C power path.
Exact affected nets must be verified from:

- VAMeter Base schematic
- VAMeter schematic
- firmware relay control code
- hardware test log

Do not claim current-limit protection by relay unless the relay-controlled path and cutoff behavior are verified.

## Future work status

The following are future or partially verified items unless a later log proves otherwise:

- glass fuse in front of +IN
- current-limit cutoff using relay OFF, only for verified relay-controlled paths
- Help button
- internal power source
- bulb test output
- motor observation output

Do not describe these as usable features unless implemented and verified.
