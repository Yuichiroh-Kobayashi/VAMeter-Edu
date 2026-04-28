# CytronMotorDriver Library Notes

## Purpose

Summarize the CytronMotorDriver API facts relevant to future VAMeter-Edu MAKER-DRIVE support.

This file is a software reference.
Hardware limits belong in `docs/hardware/MAKER_DRIVE/`.

## Source priority

1. CytronMotorDriver official library source
2. CytronMotorDriver official examples
3. Local build / hardware verification logs
4. Inference, marked as inference

## Important distinction

`PWM_PWM_DUAL` is an example name.

The actual enum mode used by `CytronMD` is:

```cpp
PWM_PWM
```

The official header defines:

```cpp
enum MODE {
  PWM_DIR,
  PWM_PWM,
};
```

## Constructor

```cpp
CytronMD(MODE mode, uint8_t pin1, uint8_t pin2);
```

The official implementation stores the mode and pins, sets both pins to OUTPUT, and writes both pins LOW in the constructor.

VAMeter-Edu rule:

- Do not construct a motor driver object in a way that produces unintended output.
- If construction behavior is uncertain in the target framework, verify output pins with a logic analyzer or oscilloscope before connecting a motor.

## setSpeed

```cpp
void setSpeed(int16_t speed);
```

The official implementation clamps speed to:

```text
-255 ... +255
```

For `PWM_PWM` mode:

|    speed | pin1        | pin2         |
| -------: | ----------- | ------------ |
| positive | PWM = speed | 0            |
|     zero | 0           | 0            |
| negative | 0           | PWM = -speed |

## One-channel use in VAMeter-Edu

Although the official PWM_PWM_DUAL example controls two motors with four PWM pins, VAMeter-Edu should use only one channel.

Initial conceptual use:

```cpp
CytronMD motor1(PWM_PWM, portAForwardPwmPin, portABackwardPwmPin);
```

Do not create a second CytronMD instance unless the project explicitly changes the hardware interface beyond VAMeter Base Port.A.

## ESP32 / ESP-IDF note

The official implementation uses `ledcWrite()` when `ARDUINO_ARCH_ESP32` is defined.

VAMeter-Edu currently uses ESP-IDF-based firmware, so direct reuse of the Arduino library is not automatically guaranteed.

If the library is used in an ESP-IDF or compatibility environment, verify:

- build compatibility
- PWM channel allocation
- PWM frequency
- pin output at construction
- `setSpeed(0)` behavior
- behavior in Disabled / Fault states

## VAMeter-Edu wrapper rule

Application code should not call `CytronMD` directly from UI code.

Future design should place motor driver interaction behind a wrapper or HAL boundary.

Suggested conceptual interface:

```cpp
class MotorDriver {
public:
  bool begin();
  void disarm();
  void setTargetPercent(int targetPercent);
  void update();
  bool hasFault() const;
};
```

Rules:

- `begin()` must not produce non-zero motor output.
- `disarm()` must force zero output.
- `setTargetPercent()` must store target only.
- physical output should occur only in `update()` while the safety state permits it.
- Fault state must force output off.

## Unverified

- Compatibility with VAMeter-Edu ESP-IDF build.
- ESP32-S3 PWM behavior with this library in the VAMeter-Edu environment.
- PWM output frequency and LEDC channel allocation.
- Safe behavior of constructor and `setSpeed(0)` on the target board.
- VAMeter Base Port.A pin mapping.
- Integration with VAMeter measurement timing.
