# VAMeter-Edu Agent Guide

## Instruction precedence

- Treat this file as the primary repository-wide guidance for AI coding tools.
- Follow the user's explicit request first.
- If a more specific `AGENTS.md` exists under the edited path, follow both files and let the more specific guidance win for that path.
- Tool-specific files such as `CLAUDE.md` and `.github/copilot-instructions.md` are adapters. They must not become independent copies of the project manual.
- Read the relevant documents under `docs/ai/` before changing recorder behavior, measurement semantics, build configuration, dependencies, Git worktrees, or artifacts.

## Project purpose

VAMeter-Edu is educational firmware for the M5Stack VAMeter. It is intended for Japanese middle-school science and technology classes.

The product goal is not only to display voltage and current. It must support a classroom workflow:

1. operate a real circuit or load;
2. observe voltage/current behavior;
3. explicitly record a short interval;
4. export CSV without requiring the internet or a cloud account;
5. graph the measurement against real elapsed time;
6. discuss the relationship between the physical event and measured data.

Prefer classroom clarity, safe failure, data preservation, and explainable measurement semantics over feature count.

## Current development scope

- Stable baseline branch: `main`. Current release: stable [`v2.0.0`](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/releases/tag/v2.0.0) (2026-09-02). The earlier `v2.0.0-beta.1` release remains historical lineage, not current release authority.
- `dev/vi-logger` is a historical branch that hosted PR #1 plus product/design documentation; its recorder/CSV behavior reached `main` through the later `v2.0.0-beta.1` release lineage, not a formal merge of that branch. Current work does not need to start from it.
- High-priority known issue: GitHub Issue #3, recorder sampling stalls during synchronous FAT writes.
- GitHub Issue #2 (persistent calibration storage / `cal_store` reintegration) is closed as not planned. Measurement-pipeline calibration changes are not currently planned work; do not treat Issue #2 as active or upcoming.
- Do not begin Issue #3 implementation unless the task explicitly requests it.
- The device-hosted direct-browser Viewer (`O1-RX + O3` architecture) is implemented and released in stable `v2.0.0`, with physical validation on Windows Edge 151 and an iPad 7th generation running iPadOS 18.7.9 Safari. See `docs/product/device-hosted-viewer-contract.md`. Multi-client policy beyond the existing one-active-owner D2B safety contract (Issue #8) and the analog-meter answer-check display correction (Issue #9) remain open.
- An internal-excitation V-I logger (internal power source for classroom V-I experiments) is a real, still-requested future direction, but is paused pending a hardware redesign. It is tracked in [VAMeter-Edu Issue #11](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/11); historical design drafts are preserved under `docs/archive/design/`. Do not begin implementation without an explicit task request.

Read `docs/ai/project-context.md` for branch roles, scope, and known decisions.

## Direct-browser and physical-work gates

- Before changing direct-browser behavior or Origin handling, read `docs/architecture/origin-admission-policy.md` and `docs/architecture/direct-browser-service-profiles.md`.
- Before any resource-sensitive change, read `docs/architecture/resource-budget.md`.
- Before any physical flash, readback, rollback, or device test, read `docs/ai/physical-validation-and-rollback.md`.
- `O1-RX + O3` is the current direct-browser architecture. Origin is browser-origin admission control, not authentication.
- P1 and P2 are mandatory. Never weaken the production Origin policy to make external development easier.
- Dependency, worktree, build, evidence, physical-safety, and rollback invariants remain mandatory.
- Internal orchestration details, agent runtime metadata, model names, thread identifiers, or reviewer metadata must never become Project or Firmware Gates.

## Environment baseline

- Host development: Ubuntu or WSL Ubuntu.
- Device target: M5Stack VAMeter using ESP32-S3.
- ESP-IDF baseline: v5.1.6.
- The root default is C++11, while a desktop dependency target requires C++17 compile features. Check the edited target and its transitive compile features; do not infer one repository-wide standard or unify the repository on C++17 without an explicit build-system change.
- Follow the repository `.clang-format`.
- Desktop simulation and host tests are useful but do not replace physical-device validation.
- Hardware behavior, storage behavior, waveform rendering, USB MSC, Wi-Fi AP download, and actual sampling timing require a real VAMeter for authoritative validation.

## Main directories

- `app/apps/`: user-facing applications.
  - `app_waveform/`: waveform UI and recorder interaction.
  - `app_files/`: record preview, download, and deletion UI.
  - `app_settings/`: settings entry points, including Files.
- `app/hal/`: platform-independent HAL contracts and shared types.
- `app/libs/record_csv/`: current CSV record handling.
- `app/libs/local_csv_download/`: record-name validation, selection, and streaming support.
- `app/libs/recorder_lifecycle/`: recorder task/resource ownership helpers.
- `app/libs/waveform_scale/`: staged educational Auto-scale logic.
- `app/assets/`: fonts, images, and localization inputs.
- `platforms/desktop/`: desktop simulator implementation.
- `platforms/vameter/`: ESP-IDF device project.
- `platforms/vameter/main/hal_vameter/components/`: VAMeter HAL implementation, including recorder, filesystem, and web server.
- `tests/local_csv_download/`: download/name/selection host tests.
- `tests/recorder_followup/`: recorder lifecycle, CSV, and waveform-scale regression tests.
- `docs/product/`: current, implemented product contracts (device-hosted Viewer, educational recording/local download).
- `docs/standards/`: current, durable normative rules (measurement-and-presentation semantics).
- `docs/architecture/`: current direct-browser architecture, ownership/lifecycle, and resource contracts.
- `docs/operations/`: current operational guidance, such as runtime diagnostics.
- `docs/validation/`: current physical-qualification procedures.
- `docs/releases/`: per-release records.
- `docs/archive/`: dated handoffs, validation evidence, and historical/future design drafts; these are not normative contracts by themselves. Start with `docs/archive/README.md`.
- `docs/ai/`: durable contracts shared by AI tools. Start with `docs/ai/README.md`.

## Pre-work checks

Before editing:

```bash
git rev-parse --show-toplevel
git branch --show-current
git status -sb
git status --porcelain=v1 -uall
git diff --check
git worktree list --porcelain
```

- Confirm the intended repository and worktree.
- The primary checkout should normally remain on `main`.
- Development should occur in an Issue-specific or task-specific linked worktree.
- Do not try to `git switch dev/vi-logger` in the primary checkout when that branch is already used by its linked worktree. Change directory to the linked worktree instead.
- When running branch-changing commands in multi-worktree setups, use `git -C <exact-worktree-path>` and verify the branch before and after the command.
- Stop on an unexpected dirty tree, untracked file, local-only commit, branch divergence, or worktree mapping.

Read `docs/ai/git-worktree-and-artifacts.md` before changing branches, worktrees, generated data, or build outputs.

## Command safety

Do not perform these operations unless the user explicitly authorizes the exact scope:

- `git reset --hard`
- `git clean`
- force checkout or force branch updates
- `git worktree remove --force`
- manual worktree deletion
- branch deletion
- commit, push, merge, or remote mutation
- GitHub Issue/PR mutation
- `sudo` or package installation
- flashing firmware or AssetPool
- formatting or erasing device storage
- broad `rm -rf`, `chmod`, or `chown`

Prefer read-only inspection first. Before any destructive exception, verify clean state, local-only commits, exact refs, worktree mapping, and any required artifact checksums.

## Dependency policy

Dependencies are defined by `repos.json` and fetched per worktree.

From a clean worktree root:

```bash
mkdir -p platforms/vameter/components
python3 ./fetch_repos.py
```

Important limitations of the current script:

- It performs simple `git clone` operations.
- It is not idempotent.
- It does not validate existing repositories.
- Its `subprocess.run()` calls do not use `check=True`; a Python exit status of zero is not sufficient proof that every clone succeeded.

After running it, validate all seven repositories against `repos.json`:

- expected path exists;
- real directory, not a symlink;
- Git-readable;
- expected origin URL;
- expected HEAD and tag/branch;
- clean working tree;
- no cross-worktree path or symlink.

Use `docs/ai/dependency-baseline.md` for the dependency snapshot verified with merge commit `c9bb8ca`. It is evidence, not an immutable lock: if a mutable branch has moved, stop and request review rather than silently accepting or rewinding it.

A fresh worktree may contain an empty `dependencies/LovyanGFX` directory because the path is a tracked gitlink. If it is completely empty and its `.gitmodules`/index/`repos.json` metadata matches, do not delete it; `git clone` can populate the empty directory.

Never use another worktree as a dependency store and never repair a partial fetch with cross-worktree symlinks.

## Non-negotiable recorder and measurement contracts

Read `docs/ai/recorder-and-measurement-contracts.md` before touching recorder, waveform, CSV, storage, Files, or download code.

Key rules:

- Manual trigger only; fixed 5-second fallback recording after D2B became the primary live path.
- `elapsed_ms` is a real relative timestamp, not a synthesized sample index.
- Preserve real delay and jitter; do not regularize or interpolate stored timestamps.
- Current CSV schema is `voltage,current,elapsed_ms`.
- Unused measurement columns are blank.
- No current-record summary row, capacity column, or energy column.
- Plot clipping does not modify the HAL values received by the UI or the values passed to CSV. Do not claim that every measurement range preserves negative sensor-raw current: the existing low-current HAL path clamps negative values after offset handling, while the high-current reverse path can preserve negative current.
- Application-level `shuntCurrent` and CSV `current` use amperes. The waveform `_pm_data_a_scale = 1000` is chart-internal numeric scaling, not measurement calibration or a unit correction.
- Trigger/resource ownership must remain safe across recorder stop timeouts.
- Storage-full and I/O failures must be recoverable: no reboot, format, or automatic record deletion.
- Current records are `REC-[0-9]+.csv`; legacy `MO-*` files must not affect REC numbering, preview, or download.
- Download basename validation and bounded streaming must not be weakened.
- Do not describe the current recorder as a guaranteed uniform 25 Hz logger. Issue #3 documents observed sampling stalls.
- The 5-second policy avoids the known second-chunk rotation because duration completion is checked before rotation, but per-sample synchronous FAT writes remain and Issue #3 stays open.

## Change discipline

- Keep changes small, reviewable, and limited to the requested scope.
- Do not mix calibration, sampling architecture, UI redesign, dependency tooling, and unrelated cleanup in one change.
- Preserve current behavior unless the task explicitly changes the contract.
- Search all relevant call sites and paired interfaces before renaming or changing a shared type.
- Keep desktop and VAMeter HAL behavior aligned where the shared contract requires it.
- Do not reformat unrelated files.
- Do not edit generated dependencies under `dependencies/`, `platforms/vameter/components/`, or `managed_components/`.

## Validation

Choose the smallest relevant validation set, then report what was not run.

Always:

```bash
git diff --check
git status -sb
```

Host tests / desktop build:

```bash
cmake -S . -B build/desktop
cmake --build build/desktop -j 2
ctest --test-dir build/desktop --output-on-failure
```

ESP-IDF device build, only in an ESP-IDF v5.1.6 environment:

```bash
cd platforms/vameter
CMAKE_BUILD_PARALLEL_LEVEL=2 idf.py build
```

Before calling a device build clean and reproducible, verify:

- source worktree is clean;
- expected ESP-IDF version;
- application, bootloader, and partition binaries link successfully;
- no partition overflow;
- `CMakeLists.txt`, `sdkconfig`, and `dependencies.lock` have no unintended tracked changes.

Asset/localization changes require AssetPool regeneration and explicit device flashing validation.

Read `docs/ai/build-and-validation.md` for the validation matrix and hardware boundary.

## Output and reporting

At the end of a task, report:

1. files changed;
2. behavior changed;
3. commands run and their results;
4. hardware checks performed;
5. checks not performed and why;
6. known limitations or follow-up work;
7. whether commit, push, branch, worktree, Issue, or PR state changed.

Never claim hardware validation, clean build provenance, uniform sampling, or device-flashed equivalence without direct evidence.
