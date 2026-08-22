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
  - historical branch that hosted PR #1 plus product, measurement-semantics, protection, and internal-excitation design documents;
  - its recorder/CSV behavior reached `main` independently, through the later `v2.0.0-beta.1` release lineage, not through a formal merge of this branch;
  - the branch itself remains un-merged and is not required for current work; its design-draft documents are preserved as historical drafts under [`../archive/`](../archive/README.md#historical-design-drafts).
- `dev/local-csv-streaming`
  - historical PR #1 head;
  - retained for traceability;
  - its final tree matches the squash merge commit but its ancestry is different.
- Legacy/reference branches are not part of the required standard worktree state unless a task explicitly targets them.

Do not hard-code current branch SHAs into general AI instructions. Verify current refs before any branch operation.

## Device-hosted direct-browser Viewer

The device-hosted browser Viewer is implemented and released as part of `v2.0.0-beta.1`
(2026-08-21), with physical validation on Windows Edge 151 and an iPad 7th generation
running iPadOS 18.7.9 Safari. See the
[device-hosted Viewer contract](../product/device-hosted-viewer-contract.md) for the
current product behavior and the
[`v2.0.0-beta.1` release record](../releases/v2.0.0-beta.1.md) for the release itself. The
architecture is `O1-RX + O3`: bounded pre-parser Origin admission for the D2B WebSocket
combined with a device-hosted same-origin production Viewer and a separately bounded
external-development profile.

The durable boundaries are:

- D2B wire and measurement semantics remain separate from the service-profile split; see
  [measurement-and-presentation semantics](../standards/measurement-and-presentation-semantics.md).
- Origin is an admission-control gate, not authentication. P1 and P2 remain mandatory under the authoritative [Origin admission policy](../architecture/origin-admission-policy.md).
- SystemConfig, SystemLive, and Download remain separated as defined by the [direct-browser service profiles](../architecture/direct-browser-service-profiles.md).
- Static and runtime resource qualification is tracked in the [resource budget](../architecture/resource-budget.md), which now records the actual `v2.0.0-beta.1` application/AssetPool sizes.
- Historical G1 measurement-spike artifacts are evidence about feasibility, size, and diagnostic behavior from before the Viewer was implemented; they are superseded by the current source and the `v2.0.0-beta.1` release evidence above, not current product implementation authority.

Multi-client D2B stream policy beyond the existing one-active-owner safety contract is
under investigation in [VAMeter-Edu Issue #8](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/8).
Analog-meter answer-check display correction is tracked in [VAMeter-Edu Issue #9](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/9).

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

After D2B became the primary live path, device-side CSV recording became the fallback and its current fixed duration changed to 5 seconds. The existing recorder order checks duration completion before the 5-second chunk rotation, so this policy avoids the known second-chunk rotation. Per-sample synchronous FAT writes remain; Issue #3 stays open and neither gap-free output nor uniform 25 Hz sampling is claimed.

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

- GitHub Issue #3: decouple sampling from synchronous FAT writes.
- internal-excitation hardware implementation (paused; [Issue #11](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/11));
- battery/protection hardware validation (paused with the above; see [Issue #11](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/11));
- project-wide HelpEvent transport;
- CI.

GitHub Issue #2 (persistent calibration storage / `cal_store` redesign and reintegration) is closed as not planned; it is not active or upcoming work. Measurement-pipeline calibration changes are not currently planned.

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
