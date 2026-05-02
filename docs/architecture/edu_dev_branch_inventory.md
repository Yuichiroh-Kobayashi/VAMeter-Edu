# edu-dev branch inventory for next VI logger planning

## 1. Purpose

This document inventories the mixed changes currently present on the
`edu-dev` branch and classifies what should be reused, generalized,
removed from the next mainline, or kept only as historical reference for
the next VAMeter-Edu plan.

The next plan is not Motor Observe. The intended direction is an
educational V-I experimenter / VI logger / heat logger with:

- internal excitation for classroom V-I experiments
- resistance measurement
- miniature bulb measurement as a non-linear temperature-dependent load
- power and heat generation logging
- optional Grove-style temperature measurement extension
- CSV recording and QR / local transfer
- Science / Technology mode separation

This inventory is docs-only. It does not delete Motor Observe, change CSV
schemas, change CMake, change `apps.h`, change `local_csv_download`, or
implement any new feature.

## 2. Branch context

Confirmed branch state:

- current branch: `edu-dev`
- tracked working tree diff: none before creating this document
- `origin/main`: currently points at `4492da1` with tag `v1.1.1`
- `v1.1.0..origin/main`: 5 commits
- `v1.1.1..origin/main`: no commits
- `origin/main..origin/edu-dev`: 18 commits

Recent `origin/main` context:

```text
4492da1 (tag: v1.1.1, origin/main, origin/HEAD) refactor: remove unfinished cal_store component from main
ae6a50b (origin/feature/cal-store) fix: update dependencies.lock for ESP-IDF 5.1.x compatibility
6d42879 (main) Revise README for VAMeter-Edu and add v1.1.0 features
2d55326 docs: Fix markdown bold rendering (use half-width parentheses)
2d75a03 docs: Add Japanese README (README_ja.md)
23bb065 (tag: v1.1.0) feat: Add local CSV download and AP suffix setting (v1.1.0)
```

`edu-dev` commits after `origin/main` include three mixed groups:

- AI agent governance documents, instructions, standards, and templates
- Motor Observe / MAKER-DRIVE / VAMeter Base relay / motor backend / motor
  CSV additions
- CSV / QR / local download fixes, including chunked streaming

Untracked files exist and are intentionally not included in this task:

- `docs/hardware/Probe/normal_probe_Current_ measurement.png`
- `docs/hardware/Probe/normal_probe_Voltage_measurement.png`
- `docs/hardware/Probe/training_probe_Current_measurement.png`
- `docs/hardware/Probe/training_probe_Voltage_measurement.png`
- `docs/hardware/VAMeter/Schematic_V1.2.pdf`
- `docs/hardware/VAMeter_Base/Schematic_V1.0.pdf`

The schematic PDFs and Probe images require separate source, license, and
final-version review before they are added to any next branch.

## 3. Thesis constraint

The master's thesis PDF is now confirmed to be text-extractable. This
inventory does not fully incorporate the thesis content, because this
document only classifies `edu-dev` branch assets.

Full thesis review is a gate before creating or finalizing:

- `docs/architecture/vi_logger_product_definition.md`

The product definition must explicitly reflect at least:

- why VAMeter was selected
- support for analog pointer-reading skill
- answer-check UI
- design that does not always show the answer
- classroom misoperation tolerance
- enclosure thinking for wiring simplification and damage prevention

Do not finalize the next product definition before that thesis review is
complete.

## 4. Summary of mixed changes

`edu-dev` is not a clean next-generation development branch. It mixes:

- governance documents that are useful for future AI-assisted development
- Motor Observe bring-up code, tests, hardware notes, and operation logs
- MAKER-DRIVE and motor-output assumptions that are outside the next
  V-I experimenter mainline
- CSV / QR / local download work that is likely reusable after
  generalization
- existing VAMeter-Edu educational UI, Training Probe, REC CSV, and QR
  transfer assets

The next mainline should not treat VAMeter as a motor current meter or
carry `MO-*.csv` assumptions into the VI / heat logger design.

## 5. KEEP

Keep only documents that are directly useful as governance, safety, or
coding policy for the next V-I experimenter.

- `AGENTS.md`
- `.github/copilot-instructions.md`
- `.github/instructions/*`
- `docs/canon/minimum_constraints.md`
- `docs/standards/coding_standard.md`
- `docs/standards/measurement_safety_policy.md`
- `docs/standards/naming_units.md`
- `docs/standards/calibration_policy.md`
- `docs/standards/review_checklist.md`
- `docs/standards/comments_policy.md`

`AGENTS.md` and `.github/instructions/*` are kept as structure, but they
need review before migration to the next branch because some Motor
Observe-specific references may still be present.

## 6. KEEP BUT GENERALIZE

These assets should not be copied as-is. They need to be generalized for
internal excitation, VI logger, and heat logger work.

- `docs/architecture/app_map.md`
  - Generalize from the current app set to Science / Technology / heat
    experiment modes.
- `docs/architecture/data_flow.md`
  - Extend the measurement path discipline to internal excitation,
    resistance, bulb behavior, power, energy, and temperature logging.
- `docs/architecture/measurement_state_machine.md`
  - Separate passive measurement, internal-output experiments, CSV
    logging, and transfer states.
- `docs/architecture/safety_state_machine.md`
  - Reuse the active-output safety model, but redesign it around internal
    power, current limiting, short protection, reverse connection, and
    fault clear behavior instead of motor PWM.
- `docs/architecture/probe_mode_matrix.md`
  - Generalize beyond existing voltage/current probe modes to Science and
    Technology UI behavior.
- `docs/standards/waveform_csv_policy.md`
  - Preserve legacy `REC-*.csv` expectations, but create a broader CSV
    logging policy for VI and heat data.
- `app/libs/local_csv_download/*`
  - Generalize from `REC-*.csv` / `MO-*.csv` assumptions to a record name
    policy before adding future `VI-*.csv` / `HEAT-*.csv` candidates.
- `docs/templates/operation_log_template.md`
  - Keep the Go / NoGO / rollback structure, but add fields for battery,
    protection, internal excitation, bulb, temperature, and heat tests.

From `docs/hardware/VAMeter/measurement_path.md`, extract the discipline
of explicitly naming what a measured value means. The Motor Observe wiring
itself is not the next design, but the semantic caution should move into a
future:

- `docs/standards/measurement_semantics_policy.md`

## 7. REMOVE FROM NEXT MAINLINE

The following are candidates to remove from the next V-I experimenter
mainline. This document does not remove them.

- `app/apps/app_motor_observe_bringup/*`
- `app/apps/apps.h` Motor Observe include / autostart block
- `app/libs/motor_observe_backend/*`
- `app/libs/motor_observe_safety/*` motor-specific implementation
- `app/libs/motor_observe_csv/*`
- `platforms/vameter/main/hal_vameter/components/motor_observe_pwm_backend.*`
- `tests/motor_observe/*`
- GPIO8/GPIO9 motor PWM
- GPIO10 relay motor measurement path
- `MOTOR_OBSERVE_BRINGUP_AUTOSTART`
- using `MO-*.csv` as the basis for the next CSV format

Any future removal must be a separate implementation task with build
impact review. Do not mix it with CSV schema design or UI feature work.

## 8. ARCHIVE / REFERENCE

These assets should be treated as decision history and reference material,
not active next-mainline specification.

- `docs/operations/safety_test_log.md`
- Motor Observe-related parts of `docs/hardware/VAMeter/measurement_path.md`
- `docs/hardware/VAMeter_Base/relay_control.md`
- `docs/hardware/MAKER_DRIVE/*`
- `docs/software/CytronMotorDriver/spec.md`
- `docs/architecture/motor_observe_*`
- `docs/operations/motor_observe_*`

For a clean next branch, create summarized reference documents under
`docs/references/` only if the history is needed. Do not keep Motor
Observe implementation details or MAKER-DRIVE operation specifications as
active docs for the next V-I experimenter.

## 9. UNCLEAR

The following require human decision or additional verification.

- Untracked schematic PDFs and Probe images:
  - source, license, and final-version status are not confirmed
  - do not include them in this inventory commit
- `AGENTS_PATCH.md`, `README_BOOTSTRAP.md`, `MANIFEST.md`:
  - decide whether they are temporary bootstrap artifacts or permanent
    governance assets
- `REC-*.csv` after chunked streaming:
  - actual device verification may still be missing
- next CSV prefixes:
  - `VI-*.csv` and `HEAT-*.csv` are candidates, not decisions
- next branch base:
  - choose after reviewing `origin/main`, `v1.1.0`, `v1.1.1`, and
    `edu-dev` deltas
- product definition before full thesis review:
  - keep as NoGO until thesis content is incorporated

## 10. Motor Observe removal impact

Likely impact areas if Motor Observe is removed in a later task:

- `app/apps/apps.h` currently creates direct registration and autostart
  dependencies.
- `platforms/vameter/main/CMakeLists.txt` uses recursive source collection,
  so source files may remain in the build even if app registration is
  removed.
- `app/apps/app_motor_observe_bringup/*` depends on Motor Observe backend,
  safety state, CSV logger, and HAL behavior.
- `motor_observe_pwm_backend.*` depends on LEDC, GPIO8/GPIO9 motor PWM,
  GPIO10 relay behavior, and VAMeter Base assumptions.
- `tests/motor_observe/*` must be archived or replaced with generalized
  active-output safety / CSV / download tests.
- docs contain links and references to Motor Observe, MAKER-DRIVE,
  measurement path review, and safety logs.
- no dedicated Motor Observe localization or asset set was identified as a
  primary removal blocker, but UI strings and docs still need review.

Removal should happen only after the next branch strategy is chosen.

## 11. CSV / QR reuse plan

Reusable direction:

- preserve existing waveform `REC-*.csv` compatibility
- reuse chunked streaming download after REC verification
- keep QR / local transfer as a required next-product capability
- do not use `MO-*.csv` as the next CSV schema baseline
- consider `VI-*.csv` / `HEAT-*.csv` as future design candidates
- before QR display from any active-output mode, stop output safely, force
  the output target to zero, close the CSV, verify the file is readable,
  then show QR / local download
- generalize `local_csv_download` as record name policy, not just by
  adding prefixes

Future CSV policy must distinguish:

- raw measured values
- filtered values
- calibration-adjusted values
- displayed values
- recorded CSV values
- calculated values such as resistance, power, energy, and temperature
  difference

Miniature bulb resistance must not be treated as equivalent to fixed
resistor resistance. It is a temperature-dependent non-linear load.

## 12. REC CSV verification gate

Before chunked streaming is migrated to the next branch, verify existing
waveform CSV behavior on actual device.

Gate requirements:

- existing waveform `REC-*.csv` can be generated
- QR / local download works
- header and data rows are not corrupted
- the file opens in Excel or equivalent student-device software
- existing UI paths to recording and download are not broken

`MO-*.csv` verification alone is not enough to approve migration.

## 13. AI agent governance reuse plan

Reuse the governance structure, but review it before moving to the next
branch.

Keep the following principles:

- safety, classroom operability, and misoperation tolerance first
- no undocumented active output behavior
- unknown hardware behavior is `未検証`
- unknown software behavior is `未確認`
- docs must be updated with user-visible, CSV, measurement, calibration,
  safety, or settings changes
- active output requires safe default, explicit enable, fault-off,
  explicit clear, timeout, verification, and rollback condition
- measurement code must preserve units and distinguish raw, filtered,
  displayed, calibrated, and recorded values

Review and remove or archive Motor Observe-specific references before the
next branch becomes the active VI logger branch.

## 14. Suggested next branch strategy

Do not decide the branch base in this document.

Review these before choosing:

```bash
git log --oneline --decorate origin/main -20
git log --oneline --decorate v1.1.0..origin/main
git log --oneline --decorate v1.1.1..origin/main
git log --oneline --decorate origin/main..origin/edu-dev
```

Decision candidates:

- if `origin/main` is stable, create the next branch from `main`
- if `v1.1.1` is accepted as the stable release base, create the next
  branch from tag `v1.1.1`
- if `main` is unclear, create the next branch from `v1.1.0` or `v1.1.1`
- migrate only selected assets from `edu-dev`

At this point, do not promote `edu-dev` directly to the next mainline.

## 15. Recommended next Codex task

Suggested next docs-only task:

```text
Create docs/architecture/vi_logger_product_definition.md as a draft only.
Before drafting, read the full master's thesis text and extract the
educational design principles for VAMeter selection, pointer-reading
support, answer-check UI, non-always-visible answers, classroom
misoperation tolerance, wiring simplification, and enclosure protection.

Do not implement code, do not change CSV schema, and do not decide branch
base in that task.
```

Suggested later implementation-preparation tasks:

- draft `docs/standards/measurement_semantics_policy.md`
- draft `docs/standards/csv_logging_policy.md`
- draft `docs/operations/battery_and_protection_test_plan.md`
- draft `docs/operations/r_bulb_measurement_test_plan.md`
- draft `docs/operations/heat_generation_experiment_test_plan.md`

## 16. Go / NoGO

GO for this inventory document when:

- only `docs/architecture/edu_dev_branch_inventory.md` is created
- no implementation files are changed
- the KEEP / KEEP BUT GENERALIZE / REMOVE / ARCHIVE classifications above
  are retained
- branch base remains a human decision
- full thesis review is a product-definition gate
- REC CSV verification gate is explicit
- untracked schematic PDFs and Probe images are not included

NoGO if:

- implementation code is changed
- Motor Observe is removed in this task
- branch strategy is finalized in this document
- product definition is finalized without full thesis review
- chunked streaming migration is approved without REC CSV verification
- untracked PDF or image assets are mixed into this document task
