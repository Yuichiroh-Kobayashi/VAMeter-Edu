# VI Logger Product Definition Draft

Status: Draft / 未確定

This document defines the draft product direction for the next
VAMeter-Edu generation. It is a planning document only. It does not
implement firmware, CSV schema, UI, local download, CMake, `apps.h`, or
internal power control.

## 1. Purpose

The next VAMeter-Edu should be defined as an educational internal-power
V-I experimenter / VI logger / heat logger for junior high school Science
and Technology lessons.

The product should help learners observe voltage, current, power,
resistance-like behavior, and heat generation while preserving the
educational principle from VAMeter-Edu: the device is a support tool for
thinking and checking, not a tool that simply replaces the learner's
work.

This draft is the upper-level reference for later hardware design,
firmware design, CSV schema design, UI design, transfer design, and
protection circuit design.

## 2. Background

Current VAMeter-Edu is based on M5Stack VAMeter and focuses on classroom
operation:

- simplified Voltage / Current / USB-C / Settings menu
- guide screens for correct wiring
- fixed display modes to avoid accidental page switching
- Japanese classroom UI
- Normal Probe for normal voltage/current measurement
- Training Probe for analog meter practice
- waveform recording
- local CSV download by device AP and QR code
- disabled OTA Upgrade and Factory Reset

The master's thesis, `電流計・電圧計の指針読み取り技能向上を支援する補助教具の開発と授業実践`,
frames VAMeter-Edu as an auxiliary teaching tool. It was not selected only
because it displays accurate values. It was selected because it combines
adequate measurement range and accuracy with an open firmware platform,
display, input controls, and the ability to customize the learning UI.

The next product should extend that educational role from analog meter
reading support into V-I experiments, resistance learning, miniature bulb
behavior, and heat generation logging.

## 3. Branch and reference context

Work branch:

- current branch for this draft: `dev/vi-logger`
- branch base: `origin/main` / `v1.1.1`
- branch switch, merge, cherry-pick, and rebase are out of scope for this
  task

Current branch documents checked:

- `README.md`
- `README_ja.md`
- `CHANGELOG.md`

Current branch documents not present:

- `AGENTS.md`
- `.github/copilot-instructions.md`
- `.github/instructions/*`
- `docs/canon/minimum_constraints.md`
- `docs/standards/*`

Reference branch:

- `origin/edu-dev` was read only through `git show` and `git ls-tree`
- no file was copied from `edu-dev`
- Motor Observe implementation, MAKER-DRIVE documents, motor backend, and
  motor CSV are reference material only

Important reference judgments from `edu-dev`:

- the next plan is not Motor Observe
- do not use VAMeter as a motor current meter
- do not use `MO-*.csv` as the next CSV schema basis
- Motor Observe / MAKER-DRIVE / motor backend are out of next mainline
- CSV recording, QR/local transfer, chunked streaming, measurement
  semantics, and AI governance are reusable after generalization
- keep existing `REC-*.csv` compatibility
- verify existing waveform `REC-*.csv` after any chunked streaming
  migration
- explicitly document the meaning of measured values
- do not use `unknown` values for educational judgment

## 4. Lessons from current VAMeter-Edu and Motor Observe

Reusable lessons from current VAMeter-Edu:

- classroom UI must be short, Japanese, and resistant to accidental
  operation
- guide screens matter because wiring is part of the learning task
- Training Probe should support answer-check behavior without removing
  pointer-reading practice
- Normal Probe should remain available for direct measurement
- local CSV download by QR is useful because it avoids cloud accounts,
  school network dependence, and personal information handling
- `REC-*.csv` workflows are existing classroom assets and must not be
  broken

Reusable lessons from Motor Observe:

- active output must start disabled
- selecting a mode must not energize output
- output enable must require explicit operation
- fault must force output off
- fault clear must require explicit operation
- QR transfer from an active-output mode should first force safe output
  off, close the CSV, and only then show transfer UI
- long CSV download should avoid whole-file allocation and should use
  chunked streaming
- a value is not educationally usable until the measurement path and
  semantics are known

Rejected Motor Observe lessons:

- do not continue MAKER-DRIVE work in the next mainline
- do not measure H-bridge motor terminal voltage with VAMeter
- do not treat driver input current as motor winding current
- do not treat motor PWM percent as force, torque, or student-facing
  physical quantity
- do not carry `MO-*.csv` columns or prefixes into the next schema as a
  baseline

## 5. Educational design principles from the thesis

The thesis was text-extracted and reviewed for the product definition
draft. The following principles must guide the next design.

VAMeter selection:

- VAMeter was adopted because it had suitable measurement range and
  accuracy for the classroom goal, and because its open hardware/software
  nature allowed firmware customization.
- The device value is the integration of accurate measurement and a
  customizable learner-facing UI.

Pointer-reading support:

- The original goal was not to replace analog meters with digital meters.
  The goal was to let students read the analog pointer first and then
  compare their reading with a digital value.
- Digital feedback should help students notice and correct their own
  mistakes.

Answer-check UI:

- Showing the answer all the time can remove the motivation to read the
  pointer.
- The answer-check concept should be preserved: hide or defer some
  calculated answers until the learner performs an intentional check.
- For the next product, Science mode should apply the same principle to
  resistance and power: make students reason from V and I first when that
  is the learning goal.

Classroom tolerance:

- The device must not make circuit work harder for beginners.
- Wiring, visible connection complexity, and operation steps must be
  minimized.
- The enclosure is part of the educational design because it prevents
  switch misoperation, cable pull-out, device damage, and student
  confusion.

Positioning:

- VAMeter-Edu is an auxiliary teaching tool.
- It should support skill acquisition and conceptual thinking, not bypass
  them.

## 6. Product definition

The next VAMeter-Edu is a draft concept for an internal-power V-I
experimenter with logging and transfer functions.

Core capabilities:

- apply a low-voltage internal excitation source to classroom loads
- measure voltage and current in the defined measurement path
- calculate power from measured V and I
- calculate apparent resistance when the measurement meaning is valid
- log time-series V/I/P and related values to CSV
- transfer CSV locally by QR without cloud or school network dependency
- support Science, Technology, and Heat logger modes
- optionally log temperature through Grove-style or equivalent extension

The device is still:

- an educational instrument
- not a certified laboratory instrument
- not a safety-certified protection device
- not a motor controller

The design is not yet approved for hardware implementation. All internal
power behavior is `未検証` until protection circuits, firmware state
behavior, and classroom handling are verified.

## 7. Target classroom use cases

Science:

- observe voltage and current in simple circuits
- infer resistance from V and I
- compare fixed resistor and miniature bulb behavior
- reason from measurement before seeing calculated answers
- observe relationship between power, time, energy, and temperature

Technology:

- check resistor value and tolerance in practical circuits
- check load current and power consumption
- compare fixed resistor, miniature bulb, and heating element behavior
- use calculated R and P as direct design feedback

Heat experiments:

- log electrical power over time
- estimate electrical energy
- compare energy input with temperature change when a temperature sensor is
  available
- export CSV for spreadsheet analysis on student devices

## 8. Non-goals

- Do not continue Motor Observe as the next mainline.
- Do not use MAKER-DRIVE.
- Do not make VAMeter-Edu a motor current meter.
- Do not use `MO-*.csv`.
- Do not treat a miniature bulb as a fixed resistor.
- Do not postpone protection circuit planning.
- Do not require school network access.
- Do not require cloud services.
- Do not handle student personal information.
- Do not finalize CSV schema in this product definition draft.
- Do not finalize the standard battery configuration in this draft.

## 9. Science mode

Purpose:

- let students think from V and I
- avoid showing calculated R continuously at the start of learning
- allow answer-check display of R, P, or other calculated values when the
  lesson design requires it
- support post-experiment analysis with logged V/I/P/time data

Draft behavior:

- primary display: voltage and current
- calculated R: hidden, delayed, or answer-check depending on lesson
  setting
- calculated P and energy: available for analysis, not necessarily always
  visible
- temperature: optional and shown only when the sensor path is configured
  and verified

Science mode must not convert the device into an automatic answer display
that bypasses student reasoning.

## 10. Technology mode

Purpose:

- directly confirm R, P, and power consumption
- support component and circuit design checks
- compare fixed resistors, miniature bulbs, and heating elements

Draft behavior:

- primary display may include R and P when the measurement path is valid
- fixed resistor R may be treated as a component value check
- miniature bulb `R = V / I` must be labeled as apparent resistance under
  the current operating condition
- power and current limits should be visible enough to prevent overload

Technology mode can show calculated values more directly than Science
mode, but it still must identify value meaning and units.

## 11. Heat logger mode

Purpose:

- log electrical power over time
- calculate or estimate electrical energy
- support heat generation experiments
- optionally record temperature and temperature change

Draft behavior:

- record time, voltage, current, power, and energy candidate values
- record temperature only when the sensor type, location, and calibration
  status are known
- allow local CSV transfer after output is safely off and the CSV is
  closed
- include maximum time and maximum energy limits before classroom use

Heat logger mode is not Go until thermal limits, heating element ratings,
temperature sensor handling, and enclosure heat behavior are verified.

## 12. Measurement and output domains

Separate domains:

- SYS: ESP32-S3, display, buttons/encoder, UI state, Wi-Fi AP, settings,
  and app flow
- MEAS: voltage/current acquisition, range selection, raw values,
  filtering, calibration, displayed values, and recorded values
- EXC: internal excitation output, output enable, target setting, output
  cutoff, and output state
- TEMP: temperature sensor interface, Grove-style extension, sensor
  placement, calibration status, and sampled temperature values
- STORAGE / TRANSFER: CSV creation, file close, QR display, local HTTP
  download, and long-file streaming
- PROTECTION: short circuit, overcurrent, reverse connection, external
  voltage application, overheat, low voltage, battery reverse insertion,
  timeout, and fault clear

Rules:

- UI drawing must not define measurement meaning.
- Display formatting must not be the source of CSV values.
- CSV values must not silently change when display formatting changes.
- Any calculated value must identify its source values and unit.
- `unknown` source or semantics values must not be used for student-facing
  judgment.

## 13. Internal battery / power candidates

The standard internal power source is not decided in this draft.

Candidate configurations:

| Candidate | Nominal voltage | Notes and risks |
|---|---:|---|
| 3 x NiMH | 3.6 V | Lower voltage, rechargeable, lower short-circuit voltage than 4 cells, but full-charge voltage and low-voltage behavior need verification. |
| 3 x alkaline / manganese | 4.5 V | Common school battery count, but fresh-cell voltage can be higher than nominal. Manganese cells have higher internal resistance and different load behavior. |
| 4 x NiMH | 4.8 V | Strong candidate for standard operation, but not yet decided. Full-charge voltage, current capability, and heat experiment risk need verification. |
| 4 x alkaline / manganese | 6.0 V | Higher available voltage and higher misuse risk. Fresh alkaline cells can exceed nominal total voltage. Must be checked against bulb and heating element ratings. |

Battery-related design considerations:

- new alkaline cells and fully charged NiMH cells may exceed nominal
  voltage
- internal resistance differs by chemistry, age, temperature, and cell
  condition
- short-circuit current can be high enough to damage wiring, cells,
  contacts, loads, or enclosure
- miniature bulb rated voltage/current must be checked before use
- heating elements can create burn and enclosure heat risks
- mixed old/new cells or mixed chemistries must be treated as misuse
- reverse insertion must not create unsafe output
- low battery must fail safe and should be logged
- battery voltage should be recorded in VI / heat CSV when implemented

No battery candidate is approved for standard classroom operation in this
draft.

## 14. Protection requirements

Internal power makes the device an active-output teaching apparatus.
Protection is a first-class requirement, not a later add-on.

Required safety behavior:

- output OFF by default at power-on
- output ON only by explicit user operation
- short-circuit protection
- overcurrent protection
- reverse connection protection
- external voltage application detection
- battery reverse insertion protection
- low-voltage protection
- overheat protection
- timeout to output OFF
- maximum voltage limit
- maximum current limit
- maximum output time limit
- maximum energy limit
- firmware stop, reset, crash, or watchdog event must move hardware toward
  the safe side
- fault clear must require explicit operation and must not auto-restart
  output

Protection must be verified with hardware before the product is described
as usable. If any protection behavior is unknown, mark it `未検証`.

## 15. CSV and transfer requirements

CSV and transfer are required product capabilities.

Requirements:

- CSV recording must remain available
- QR / local transfer must remain available
- cloud service must not be required
- school network access must not be required
- personal information must not be required
- long CSV transfer should use chunked streaming
- existing waveform `REC-*.csv` compatibility must not be broken
- future prefixes such as `VI-*.csv` and `HEAT-*.csv` are candidates, not
  decisions
- `MO-*.csv` must not be used as the next schema basis
- raw, filtered, calibration-adjusted, displayed, recorded, and calculated
  values must be distinguished
- `unknown` values must not be used for educational judgment
- CSV headers must include units or reference a documented schema

Before QR / local transfer from an active-output mode:

1. force output OFF
2. force output target to zero
3. write the final CSV state if applicable
4. close the CSV file
5. verify the file is not empty and is readable
6. show QR / local transfer UI

REC compatibility gate:

- existing waveform `REC-*.csv` can be generated
- QR / local download works
- header and data rows are not corrupted
- the file opens in Excel or equivalent student-device software
- existing UI paths to recording and download are not broken

## 16. Temperature extension

Temperature measurement is a planned extension, not a confirmed feature.

Candidate direction:

- Grove-style sensor connection or equivalent classroom-safe connector
- temperature logging for heat generation experiments
- CSV fields for sensor status, temperature, and temperature change after
  schema design

Open requirements:

- sensor type
- measurement range
- response time
- waterproof or insulation needs
- placement relative to heating element and water/container
- calibration or comparison method
- connector polarity and misconnection tolerance
- whether TEMP is powered from SYS, EXC, or another protected rail

Do not present temperature logging as implemented until verified.

## 17. Open questions

- school-owned miniature bulb rated voltage and current
- resistor values and allowable power
- heating wire resistance and rating
- temperature sensor candidate
- Grove connection method
- standard internal battery configuration
- terminal shape
- enclosure design
- protection circuit topology
- actual maximum current and energy limits
- how Science mode answer-check should be operated
- how Technology mode should expose direct R/P values without confusing
  Science mode
- whether `VI-*.csv`, `HEAT-*.csv`, or another prefix policy should be
  used
- REC CSV hardware recheck after chunked streaming
- how much of the existing VAMeter hardware can be reused
- scope of any new PCB
- calibration workflow for the next device
- how to document apparent bulb resistance in student-facing wording

## 18. Go / NoGO

Go for this draft:

- docs-only product definition draft
- work remains on `dev/vi-logger`
- `edu-dev` is used as reference only
- Motor Observe is excluded from the next mainline
- Science / Technology / Heat logger modes are separated
- protection requirements are explicit
- CSV / transfer requirements are explicit
- open questions remain open
- no design is finalized beyond draft status

NoGO for implementation:

- active output without verified protection
- internal battery standard selection without hardware risk review
- miniature bulb use without rated voltage/current check
- heat logger classroom use without thermal limits
- `MO-*.csv` as next CSV schema
- chunked streaming migration without REC CSV verification
- product definition treated as final without further review

## 19. Next documents to create

Recommended next documents:

- `docs/standards/measurement_semantics_policy.md`
- `docs/standards/csv_logging_policy.md`
- `docs/architecture/internal_excitation_measurement_design.md`
- `docs/architecture/thermal_energy_logging_plan.md`
- `docs/operations/battery_and_protection_test_plan.md`
- `docs/operations/r_bulb_measurement_test_plan.md`
- `docs/operations/heat_generation_experiment_test_plan.md`

Optional reference documents:

- `docs/references/motor_observe_lessons.md`
- `docs/references/vameter_measurement_path_notes.md`

Reference documents should summarize lessons only. They should not
reintroduce Motor Observe or MAKER-DRIVE as active next-mainline
specifications.
