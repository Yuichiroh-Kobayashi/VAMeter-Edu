# Future Active-Output Safety State Machine

## Scope

This file is for future active-output features only:

- internal power source
- bulb test output
- motor observation output
- relay-based current-limit cutoff for verified relay-controlled paths

These are future or partially verified features unless an operations log proves otherwise.

This file is a design constraint for future work.
It is not a request to implement active-output behavior during unrelated measurement, UI, CSV, or calibration tasks.

## States

| State | Output | Entry condition | Exit condition |
|---|---|---|---|
| SafeDisabled | OFF | power-on / reset / fault / timeout / leaving output mode | explicit prepare operation |
| OutputArmed | OFF | output mode selected, target is zero, no fault, user explicitly prepares output | explicit enable / cancel |
| OutputEnabled | ON | explicit enable operation after OutputArmed | disable / fault / timeout / leaving output mode |
| Fault | OFF | overcurrent / abnormal condition | explicit clear after cause removed |
| FaultCleared | OFF | user clears fault | return to SafeDisabled |

## Rules

- Power-on state is output disabled.
- Output target starts at zero.
- Selecting an output mode must not enable output by itself.
- Leaving an output mode must disable output.
- Output can be enabled only after hardware profile, target, and fault state are verified.
- Fault forces output off.
- Fault clear requires explicit user action.
- Timeout returns output to disabled state.

## Relay caution

The Base relay must not be assumed to disconnect every measurement or USB-C power path.
Affected nets must be verified before relying on relay cutoff as protection.
