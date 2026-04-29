---
applyTo: "docs/operations/**,docs/templates/**"
---

# Operations log instructions

Follow `AGENTS.md` first.

## Scope

These instructions apply to verification logs, classroom rollout notes, calibration records, safety checks, and operation log templates.

During Phase 1, do not create `docs/manuals/` or public `site/` content unless explicitly requested.

## Operations logs

Operations logs are evidence, not advertising.

They should record facts, not impressions.

Include:

- date, if known
- firmware version or commit, if known
- hardware used
- environment, if relevant
- test condition
- command, if relevant
- procedure
- result
- failure, if any
- remaining unknowns
- next action
- rollback condition, if applicable

Use `未確認` for unknown software behavior.

Use `未検証` for unknown hardware behavior.

Do not replace failed verification with optimistic language.

## Classroom rollout

For classroom use, check:

- Can the teacher explain the operation in one minute?
- Can students recover from common mistakes?
- Does the UI prevent mode confusion?
- Is the CSV workflow reproducible on student devices?
- Are OTA Upgrade and Factory Reset protected from accidental use?
- Are connection diagrams consistent with actual wiring?
- Are personal information and school-internal information absent?

## Calibration and measurement logs

When recording calibration or measurement verification, include:

- target mode
- target hardware or analog meter
- measurement range
- reference instrument or method
- raw value, if available
- filtered value, if available
- calibration-adjusted value, if available
- displayed value, if available
- CSV recorded value, if available
- remaining error or uncertainty
- verification status

If a value source is unknown, write `未確認`.

If hardware behavior is not verified, write `未検証`.

## Build and flash logs

When recording build or flash verification, include the commands used.

Typical build command:

```bash
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py build
```

Typical flash command:

```bash
idf.py -p /dev/<YourPort> flash -b 1500000
```

Example:

```bash
idf.py -p /dev/ttyACM0 flash -b 1500000
```

If AssetPool is flashed, record the exact command and input file path.

## WSL2 / usbipd / ESP-IDF flash rule

When writing or changing VAMeter-Edu flash instructions for Windows + WSL2 + usbipd, refer to:

- `docs/operations/wsl_usbipd_flash_guide.md`

Rules:

- In WSL, specify serial ports as `/dev/ttyACM0` or `/dev/ttyUSB0`.
- Do not write `ttyACM0` without `/dev/`.
- Windows COM numbers and WSL device paths are different.
- BUSID may change after reconnecting USB.
- If `Could not open ttyACM0` appears, first check whether `/dev/` is missing.
- For Motor Observe bring-up, confirm MAKER-DRIVE and motor are not connected before flashing.
- Do not enable `MOTOR_OBSERVE_BRINGUP_AUTOSTART=1` for classroom release builds.

## Prohibited content

- Do not include student names, faces, school names, internal network names, or private details.
- Do not describe unimplemented future work as available.
- Do not claim safety performance that has not been verified.
- Do not treat a failed or skipped verification as passed.

## Output rule

When updating an operation log, keep the entry concise and reproducible.

A future reader should be able to answer:

- What was tested?
- With what hardware and firmware?
- By what procedure?
- What happened?
- What remains unknown?
- What is the next action or rollback condition?
