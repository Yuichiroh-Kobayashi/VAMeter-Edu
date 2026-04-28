# MAKER-DRIVE Safety Limits for VAMeter-Edu

## Purpose

Define conservative safety limits for future MAKER-DRIVE use in VAMeter-Edu.

This file exists to prevent AI agents from treating MAKER-DRIVE as a general-purpose motor controller.

## Conservative limits

Use datasheet-side conservative values unless later verification justifies stricter limits.

| Item | Limit | Rule |
|---|---:|---|
| Motor supply voltage | 2.5-9.5 VDC | Do not exceed |
| Continuous current | 1 A per channel | Treat as maximum, not target |
| Peak current | 1.5 A per channel, <5 s | Avoid in classroom operation |
| Logic input High | 1.7-6 V | VAMeter output level must be verified |
| PWM frequency | DC-20 kHz | Actual firmware frequency must be verified |
| 5V output | 200 mA max | Do not power VAMeter-Edu from this unless verified |

## VAMeter-Edu initial restriction

Only one MAKER-DRIVE channel is used.

The reason is not the MAKER-DRIVE capability.
The reason is the VAMeter-Edu project constraint:

- drive signals come from VAMeter Base Port.A
- Port.A is treated as one motor-channel interface
- head-side standalone control is out of scope
- two-channel motor control is out of scope

## Classroom restrictions

- Use only small brushed DC motors at first.
- Do not connect motors expected to exceed 1 A continuous.
- Do not use RS-380 / RS-540 class motors in the initial plan.
- Use a current-limited supply during bring-up.
- Keep the motor mechanically fixed.
- Keep fingers, hair, wires, and loose parts away from the rotating shaft.
- Do not create load by holding the shaft or wheel by hand.
- Use a repeatable load fixture if load observation is required.
- Treat overtemperature cutoff as driver protection, not classroom safety control.

## Test button caution

MAKER-DRIVE test buttons can run the motor at full speed.

For VAMeter-Edu classroom use:

- Do not rely on test buttons for normal operation.
- Do not press test buttons while students handle wiring.
- Document any test-button use in operation logs.

## Motor Observe Mode boundary

Initial goal:

- visualize PWM command
- visualize voltage, current, and power
- observe current increase under load
- avoid claiming torque control

Not initial goal:

- closed-loop current control
- certified overcurrent protection
- torque measurement
- high-current motor control
- two-channel motor control
- competition robot control

## Fault / rollback candidates

Future implementation should define rollback if:

- current exceeds conservative threshold
- supply voltage collapses
- motor does not stop on command
- PWM output appears on wrong Port.A signal
- PWM output appears while disabled
- VAMeter measurement and expected wiring disagree
- driver overheats
- classroom operation cannot be explained clearly

All fault thresholds are `未検証` until tested.
