# AGENTS.md

## Project role

This repository is an educational firmware project for **VAMeter-Edu**, a classroom-oriented fork of M5Stack VAMeter firmware.

VAMeter-Edu is used in Japanese junior high school Technology and Science lessons to help beginners visualize electricity through:

- Voltage / current digital display
- Waveform display and CSV recording
- Analog meter reading support
- Local CSV download for student devices
- Japanese classroom-oriented UI and guide screens

This is not a general-purpose laboratory instrument, not a safety-certified protection device, and not a competition motor controller.

## Must read first

Before changing code or documentation, read these files if they exist.

1. `README.md`
2. `CHANGELOG.md`
3. `docs/canon/minimum_constraints.md`
4. `docs/standards/measurement_safety_policy.md`
5. `docs/standards/coding_standard.md`
6. `docs/architecture/app_map.md`
7. `docs/architecture/measurement_state_machine.md`
8. `docs/architecture/probe_mode_matrix.md`

If a referenced document does not exist yet, do not invent its contents.
Write `未作成` or propose creating it as part of the agent-governance task.

During Phase 1, `docs/manuals/` and `docs/standards/user_manual_policy.md` are intentionally deferred.
Do not create user-facing manuals unless the task explicitly requests them.

## Hard rules

- Safety, classroom operability, and misoperation tolerance have priority over convenience.
- Do not expose student names, faces, school-internal information, or private network details.
- Do not make COI-risky proposals casually.
- Do not replace safety with effort, intention, or spirit.
- Prefer **Reuse → Buy → Make**.
- If making or modifying hardware-dependent behavior, define:
  - minimum prototype
  - verification steps
  - rollback condition
- Unknown hardware behavior must be marked `未検証`.
- Unknown software behavior must be marked `未確認`.
- Do not document unimplemented features as available.
- Do not mix safety-critical output changes with unrelated UI cleanup in the same change.

## Current classroom product constraints

VAMeter-Edu currently emphasizes:

- simplified menu flow
- voltage/current fixed display modes
- guide screens for correct wiring
- Japanese localization
- waveform recording
- local CSV download via device AP and QR code
- disabled OTA Upgrade
- disabled Factory Reset

When changing any of these user-visible behaviors, update the relevant user documentation in the same change.

## Build and verification

### Fetch dependencies

```bash
python ./fetch_repos.py
```

### Desktop simulator build

```bash
sudo apt install build-essential cmake libsdl2-dev
mkdir build && cd build
cmake .. && make -j8
cd desktop && ./app_desktop_build
```

### Device build

Use ESP-IDF v5.1.3 unless the repository documentation is updated.

```bash
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py build
```

### Device flash

```bash
idf.py -p <YourPort> flash -b 1500000
```

### AssetPool flash

```bash
parttool.py --port <YourPort> write_partition --partition-name=assetpool --input "path/to/AssetPool-VAMeter.bin"
```

If you run the desktop build first, AssetPool-VAMeter.bin may be generated under:

```text
build/desktop/AssetPool-VAMeter.bin
```

If a command fails, do not hide the failure. Report the command, result, and likely cause.

## Documentation update rule

When changing code that affects any of the following, update documentation in the same change:

- user operation
- display text
- menu flow
- connection guide
- supported probe mode
- measurement mode
- CSV format
- local download behavior
- safety behavior
- settings behavior
- calibration behavior
- troubleshooting behavior

Required documents, when present:

- `README.md`
- `CHANGELOG.md`
- `docs/manuals/user_manual.md`
- `docs/manuals/quick_start.md`
- `docs/manuals/troubleshooting.md`
- `docs/operations/*_log.md` when verification or calibration is involved

Do not write that a feature is usable unless it is implemented and verified.

## Measurement and safety policy

Measurement-related changes must distinguish:

- raw measured value
- filtered value
- displayed value
- recorded CSV value
- calibration-adjusted value

Use explicit names with physical units.

Good examples:

```cpp
float measuredVoltageV;
float measuredCurrentA;
float displayedCurrentA;
float csvSampleIntervalMs;
float currentLimitA;
```

Bad examples:

```cpp
float value;
float current;
float data;
float limit;
```

If an active output feature is added later, such as an internal power source, bulb test output, or motor observation mode, it must follow a safety state model:

- power-on state is output disabled
- output target starts at zero
- output can only be enabled by explicit user action
- fault state forces output off
- fault clear requires explicit operation
- timeout returns output to disabled state

Do not add active output behavior without verification and rollback conditions.

## Architecture rules

Keep these domains separated:

1. UI / display
2. measurement acquisition
3. filtering / smoothing
4. calibration / correction
5. CSV recording / local download
6. settings persistence
7. hardware control
8. safety state management

Do not mix measurement logic and screen drawing in the same change unless the existing architecture requires it and the reason is documented.

Do not place hardware-specific constants directly throughout the code.
Use central configuration or HAL-level definitions.

## Calibration rules

Calibration changes must record:

- target mode
- target hardware or analog meter
- measurement range
- reference instrument or method
- before/after behavior
- remaining error or uncertainty
- date and firmware version, if known

If calibration is not verified, write 未検証.

Do not hard-code a classroom-specific calibration value without documenting why it is acceptable.

## CSV and data export rules

When changing waveform recording or CSV output:

- preserve backward compatibility when possible
- document column names and units
- document header rows and summary rows
- update sample data or operations log when available
- verify that student devices can open the file

If a CSV format change may break existing classroom workflows, explicitly state the impact.

## Localization and UI text

- User-facing text should be Japanese unless the repository intentionally keeps a specific English label.
- Use short, classroom-friendly wording.
- Avoid ambiguous menu labels.
- Keep guide images, localization keys, and app names consistent.
- Do not expose school names, student names, or internal network information in UI text, screenshots, logs, or documentation.

## Git and change management

Before starting work, inspect:

```bash
git status
```

For each task:

- keep the change small
- avoid unrelated refactors
- do not mix generated asset changes with logic changes unless necessary
- summarize verification performed
- list unverified items explicitly

Recommended branch names:

```text
chore/agent-governance-vameter-edu
docs/manual-update-<topic>
fix/<short-bug-name>
feat/<short-feature-name>
```

## Review checklist

Before finishing, check:

- [ ] Does the change preserve classroom safety?
- [ ] Does power-on behavior remain safe?
- [ ] Are user-visible changes documented?
- [ ] Are unimplemented features not advertised?
- [ ] Are unknown items marked 未確認 or 未検証?
- [ ] Are physical quantities named with units?
- [ ] Does the build command pass, or is the failure documented?
- [ ] Are calibration or measurement changes recorded in operations docs?
- [ ] Are student/school/private details absent?
- [ ] Is the diff limited to the requested task?

## Relationship with `.github` instructions

This repository may also contain:

- `.github/copilot-instructions.md`
- `.github/instructions/*.instructions.md`

Those files are primarily for GitHub Copilot and path-specific IDE assistance.

When using Codex, treat this `AGENTS.md` as the primary instruction entry point.
If `.github/instructions/*.instructions.md` exists and matches the files being edited, read the relevant file and follow it unless it conflicts with this `AGENTS.md` or higher-priority user instructions.

## Task skills

If repeated task guides are added later, consult:

```text
.agents/skills/*/SKILL.md
```

These skills are primarily for Codex. Other AI agents may ignore them.
If no matching skill exists, do not invent one during an unrelated task.
