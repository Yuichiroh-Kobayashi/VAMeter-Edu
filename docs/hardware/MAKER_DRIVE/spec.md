# Cytron MAKER-DRIVE Hardware Spec

## Purpose

This file summarizes the Cytron MAKER-DRIVE hardware constraints for future VAMeter-Edu Motor Observe Mode.

MAKER-DRIVE is treated as a beginner-friendly brushed DC motor driver for small classroom motors.

This file is not an implementation request.

## Source priority

1. Cytron MAKER-DRIVE official datasheet
2. Cytron official product page
3. CytronMotorDriver official library source and examples
4. Local hardware verification logs
5. Inference, marked as inference

## Role in VAMeter-Edu

Future use case:

- Drive one small brushed DC motor through MAKER-DRIVE.
- Use only one MAKER-DRIVE channel.
- Use VAMeter measurement path to observe motor voltage, current, and electrical power.
- Visualize the relationship between PWM command and motor load.
- Do not claim torque control unless closed-loop current control and torque relation are separately verified.

## VAMeter-Edu project boundary

Although MAKER-DRIVE is a 2-channel motor driver, VAMeter-Edu will initially use only one channel.

Reason:

- VAMeter-Edu drive signals are assumed to come from VAMeter Base Port.A.
- VAMeter Base Port.A provides only the drive-signal capacity required for one PWM_PWM motor channel.
- Head-only control is out of scope.
- Dual-motor control is out of scope.

## Official / conservative specs

Use conservative values for classroom planning.

| Item | Value | Status |
|---|---:|---|
| Motor type | Brushed DC motor | official |
| Channels | 2 | official |
| VAMeter-Edu initial use | 1 channel only | project constraint |
| Power input voltage | 2.5-9.5 VDC | official / conservative |
| Maximum motor current, continuous | 1 A per channel | conservative |
| Maximum motor current, peak | 1.5 A per channel, < 5 s | conservative |
| Logic input Low | 0-0.5 V | datasheet |
| Logic input High | 1.7-6 V | datasheet |
| PWM frequency | DC-20 kHz | datasheet |
| 5V output | Max 200 mA | datasheet |
| Protection | reverse polarity, overtemperature | official |

## Product-page conflict note

Some product-page or FAQ text may describe different current values from the datasheet.

For VAMeter-Edu safety planning, use the conservative datasheet-side value:

- 1 A continuous per channel
- 1.5 A peak per channel for less than 5 seconds

If a later official revision is used, record the source and revision in `docs/operations/safety_test_log.md`.

## Board functions

| Function | Description | VAMeter-Edu use |
|---|---|---|
| VB+ / VB- | Motor power input | Motor power path; must be verified |
| Motor output terminal | Connects to DC motor | One motor only in initial plan |
| M1A / M1B | PWM input pair for motor 1 | Preferred initial channel |
| M2A / M2B | PWM input pair for motor 2 | Not used in initial plan |
| 5VO | 5V output, max 200mA | Do not power VAMeter-Edu from this unless verified |
| Test buttons | Run motor at full speed | Do not use in classroom workflow unless separately tested |
| Motor status LEDs | Show motor direction | Visual aid only; not a measurement source |

## Initial classroom boundary

Initial Motor Observe Mode should be limited to:

- one MAKER-DRIVE channel
- one small brushed DC motor
- VAMeter Base Port.A drive output
- current below 1 A continuous
- current-limited power source during bring-up
- explicit safe state before enabling motor output
- no hand-stall tests
- no claim of torque control

## VAMeter-Edu initial PWM policy

For VAMeter-Edu Motor Observe initial implementation:

| State / target | M1A candidate | M1B candidate | Notes |
|---|---|---|---|
| SafeDisabled | Low | Low | Brake-side safe state |
| OutputArmed | Low | Low | No PWM output |
| Fault / timeout / leaveMode | Low | Low | Best-effort safe output |
| target = 0 | Low | Low | Brake-side stop |
| target > 0 | PWM | Low | Forward candidate |
| target < 0 | Low | PWM | Reverse candidate |

Do not use:

- High/High coast
- PWM/High
- High/PWM

`PWM/High` and `High/PWM` are not equivalent to normal forward/reverse PWM. They can alternate between coast and the opposite drive state depending on the PWM phase. Do not implement them without a separate design review.

## Unverified

- Exact VAMeter Base Port.A signal names and GPIO mapping for Motor Observe Mode.
- MAKER-DRIVE behavior with VAMeter-Edu output pins.
- Actual PWM frequency in the VAMeter-Edu firmware environment.
- Whether VAMeter current sampling is fast enough for feedback control.
- Whether motor current is measured at motor driver input, motor output, or another selected path.
- Effect of braking or coast behavior on VAMeter measurement path.
- Classroom-safe load fixture for motor observation.

Mark these as `未検証` until tested.
