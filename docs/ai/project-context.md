# VAMeter-Edu project context

## Educational purpose

VAMeter-Edu adapts the open-source M5Stack VAMeter firmware for middle-school electricity education.

The intended learning sequence is:

1. operate a circuit or load;
2. observe voltage/current;
3. explicitly record a short interval;
4. export CSV locally;
5. graph against actual elapsed time;
6. connect physical behavior with measured data.

Classroom usability changes the engineering priorities. A technically powerful feature is not automatically appropriate if it adds hidden state, difficult controls, destructive recovery, or unexplained data transformation.

## Design priorities

In order:

1. student and teacher safety;
2. data preservation;
3. explainable measurement semantics;
4. simple and explicit operation;
5. compatibility with existing records;
6. bounded resource use;
7. performance and additional features.

## Branch roles

- `main`
  - stable baseline;
  - primary worktree should normally stay here and clean.
- `dev/vi-logger`
  - active integration branch for the educational V-I logger;
  - contains PR #1 plus product, measurement-semantics, protection, and internal-excitation design documents;
  - merging this branch to `main` is broader than merging PR #1 alone.
- `dev/local-csv-streaming`
  - historical PR #1 head;
  - retained for traceability;
  - its final tree matches the squash merge commit but its ancestry is different.
- Legacy/reference branches are not part of the required standard worktree state unless a task explicitly targets them.

Do not hard-code current branch SHAs into general AI instructions. Verify current refs before any branch operation.

## PR #1 accepted scope

PR #1 hardened classroom waveform recording and was accepted into `dev/vi-logger`.

Accepted behavior includes:

- manual recording trigger;
- fixed 10-second record;
- staged educational Auto scale for single-channel views;
- mode-specific timestamped three-column CSV;
- recorder trigger/resource lifetime safety;
- storage preflight and recoverable errors;
- `REC-*` and legacy `MO-*` separation;
- Files app access and validated local AP download;
- legacy CSV preview compatibility;
- host tests and design documentation.

## Deliberate simplifications

A manual vertical-scale prototype was built and reviewed, then removed.

The final UI keeps:

- encoder click to start;
- encoder rotation for horizontal time zoom;
- side short click as a Help-request hook;
- side long hold to exit;
- staged Auto scale;
- centered live value.

In the single-channel views it removes manual V/I scale selection, TIME selection/readout, the manual-trigger `M` badge, and panel-edge max/min labels.

This is a deliberate education-first decision, not an unfinished UI.

## Separate work

Do not mix these into unrelated recorder changes:

- GitHub Issue #2: persistent calibration storage redesign and reintegration.
- GitHub Issue #3: decouple sampling from synchronous FAT writes.
- internal-excitation hardware implementation;
- battery/protection hardware validation;
- project-wide HelpEvent transport;
- CI;
- formal `dev/vi-logger` to `main` integration.

## USB-C `mode_both` compatibility behavior

USB-C Power Monitor opens Waveform in internal `mode_both` compatibility mode. It records both voltage and current columns and retains the legacy combined voltage/current chart with each channel's min/mid/max-following labels. Single-channel staged Auto scale and single-channel panel-edge-label removal do not apply. There is no manual vertical-scale control and no TIME/V/I `/div` readout. Encoder rotation remains horizontal zoom, side short click calls the Help hook, and side long hold exits except while saving.

## Source of truth by claim type

Do not apply one universal precedence list to unlike claims. Use the source that owns the claim:

- current implementation behavior: merged code and tests;
- measurement units, range, and calibration semantics: the canonical standards document together with the HAL implementation;
- physical validation status: the focused validation report and dated handoff evidence;
- open or future work: the open GitHub Issue;
- intended architecture and design rationale: the canonical architecture document;
- historical decision trail: the dated handoff.

A dated handoff is evidence and history, not a standalone normative contract. Conversely, an architecture document must not override later physical-validation evidence about what was actually tested. Resolve contradictions explicitly against the appropriate claim type.
