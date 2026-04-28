# MAKER-DRIVE Port.A 1ch Interface

## Purpose

Define the initial VAMeter-Edu connection policy for driving MAKER-DRIVE from VAMeter Base Port.A.

This file is for future Motor Observe Mode planning.
It is not an implementation request.

## Core project decision

VAMeter-Edu will not use MAKER-DRIVE as a 2-channel motor driver.

Initial policy:

- Use VAMeter Base Port.A only.
- Use one MAKER-DRIVE channel only.
- Do not use head-side standalone control.
- Do not implement dual motor control.
- Do not implement differential drive.

## CytronMotorDriver clarification

`PWM_PWM_DUAL` is the name of the official example.

The actual CytronMotorDriver mode used in code is:

```cpp
PWM_PWM
```

The official example creates two independent motor objects:

```cpp
CytronMD motor1(PWM_PWM, pin1A, pin1B);
CytronMD motor2(PWM_PWM, pin2A, pin2B);
```

For VAMeter-Edu initial use, only one object is conceptually required:

```cpp
CytronMD motor1(PWM_PWM, portAForwardPwmPin, portABackwardPwmPin);
```

This is a conceptual example only. Do not implement it without checking the actual VAMeter-Edu firmware framework.

## Required signals for one channel

A single MAKER-DRIVE channel in PWM_PWM mode requires two logic inputs:

| MAKER-DRIVE signal | Role                                           |
| ------------------ | ---------------------------------------------- |
| M1A                | PWM input for one motor direction              |
| M1B                | PWM input for the opposite motor direction     |
| GND                | Common ground with controller signal reference |

Motor power uses a separate power path:

| MAKER-DRIVE terminal | Role                  |
| -------------------- | --------------------- |
| VB+                  | Motor supply positive |
| VB-                  | Motor supply negative |

## Proposed initial channel

Use MAKER-DRIVE channel 1:

| Function           | MAKER-DRIVE | VAMeter-Edu side                           | Status |
| ------------------ | ----------- | ------------------------------------------ | ------ |
| Forward PWM input  | M1A         | VAMeter Base Port.A signal 1               | 未検証 |
| Backward PWM input | M1B         | VAMeter Base Port.A signal 2               | 未検証 |
| Logic reference    | GND         | VAMeter Base / system GND                  | 未検証 |
| Motor supply +     | VB+         | external or future protected supply        | 未検証 |
| Motor supply -     | VB-         | external or future protected supply return | 未検証 |

Do not assign final GPIO names in this document until the VAMeter Base Port.A schematic and firmware pin assignment are verified.

## PWM_PWM behavior

For one channel:

|         Command | M1A | M1B | Meaning                                            |
| --------------: | --- | --- | -------------------------------------------------- |
| positive target | PWM | Low | Forward, actual direction depends on motor wiring  |
|     zero target | Low | Low | Brake according to MAKER-DRIVE truth table         |
| negative target | Low | PWM | Backward, actual direction depends on motor wiring |

According to MAKER-DRIVE truth table:

| Input A | Input B | Motor state |
| ------- | ------- | ----------- |
| Low     | Low     | Brake       |
| High    | Low     | Forward     |
| Low     | High    | Backward    |
| High    | High    | Coast       |

Avoid commanding High/High unless coast behavior is explicitly designed and verified.

## Measurement wiring concept

Motor Observe Mode must define what VAMeter measures.

Possible measurement targets:

1. motor driver supply input
2. motor current path
3. motor terminal voltage
4. total supply power

Do not assume that measured current equals motor winding current unless the wiring path is verified.

The selected measurement path must be documented in:

- `docs/hardware/VAMeter/measurement_path.md`
- `docs/architecture/data_flow.md`
- `docs/operations/safety_test_log.md`

## Safety state requirements

Motor output must follow the future active-output safety state machine.

Minimum requirements:

- power-on output disabled
- target starts at zero
- selecting Motor Observe Mode does not enable output
- non-zero PWM output requires explicit user operation
- fault forces M1A/M1B to safe output
- leaving Motor Observe Mode disables output
- timeout returns output to disabled state
- Verification before motor connection

Before connecting a motor:

1. Confirm VAMeter Base Port.A signal mapping.
2. Confirm no GPIO conflict with Base relay or existing VAMeter functions.
3. Confirm PWM waveform on the two Port.A signals.
4. Confirm both signals are Low in disabled state.
5. Confirm target zero behavior.
6. Confirm positive and negative target polarity without motor load if possible.
7. Confirm motor supply is current-limited.
8. Confirm VAMeter measurement path.
9. Record results in docs/operations/safety_test_log.md.

If not verified, write `未検証`.
