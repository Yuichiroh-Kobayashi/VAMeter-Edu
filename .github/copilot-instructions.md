# GitHub Copilot instructions for VAMeter-Edu

This repository is an educational firmware project for VAMeter-Edu, a classroom-oriented fork of M5Stack VAMeter firmware.

Always respect `AGENTS.md` as the primary project instruction document.

## Core priorities

- Classroom safety and misoperation tolerance have priority over convenience.
- Do not expose student names, faces, school-internal information, or private network details.
- Do not document unimplemented or unverified features as available.
- Mark unknown software behavior as `未確認`.
- Mark unknown hardware behavior as `未検証`.
- Prefer Reuse → Buy → Make.
- If making or changing hardware-dependent behavior, include minimum prototype, verification steps, and rollback condition.

## User-visible changes

When changing user operation, display text, menu flow, wiring guide, CSV format, local download behavior, settings, calibration, safety behavior, or troubleshooting behavior, update documentation in the same change.

Relevant documents may include:

- `README.md`
- `CHANGELOG.md`
- `docs/manuals/user_manual.md`
- `docs/manuals/quick_start.md`
- `docs/manuals/troubleshooting.md`
- `docs/operations/*_log.md`

## Firmware rules

Keep these domains separated:

- UI / display
- measurement acquisition
- filtering / smoothing
- calibration / correction
- CSV recording / local download
- settings persistence
- hardware control
- safety state management

Use explicit names with physical units, for example:

- `measuredVoltageV`
- `measuredCurrentA`
- `displayedCurrentA`
- `csvSampleIntervalMs`
- `currentLimitA`

Avoid vague names such as `value`, `data`, `tmp`, `current`, and `limit` unless the scope is very small and the meaning is obvious.

## Build assumptions

Use the repository documentation as the source of truth.

Typical commands:

```bash
python ./fetch_repos.py
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py build
```

If a command fails, report the command, result, and likely cause. Do not hide failures.

## Language

User-facing documentation and comments should be Japanese unless a technical identifier or existing English label is required.

Keep classroom-facing wording short and unambiguous.
