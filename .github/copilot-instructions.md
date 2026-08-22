# Copilot Instructions for VAMeter-Edu

## Precedence

- Follow the repository root `AGENTS.md` as the primary common guidance.
- Use this file as a lightweight Copilot completion adapter.
- When instructions conflict, prefer the more specific instruction for the edited path.
- Do not reproduce the full project manual in this file.

## Project baseline

- Project name in prose: `VAMeter-Edu`.
- Target: M5Stack VAMeter / ESP32-S3.
- Device framework baseline: ESP-IDF v5.1.6.
- The root default is C++11, but a desktop dependency target requests C++17 compile features. Check the edited target and transitive requirements; do not infer or impose one repository-wide standard.
- Follow `.clang-format` and existing local style.

## Completion guardrails

- Preserve classroom-first behavior: explicit controls, fixed 5-second fallback recording, safe recoverable errors, and simple CSV.
- Keep shared HAL declarations and desktop/VAMeter implementations consistent.
- When changing measurement or recorder code, preserve the contracts in `docs/ai/recorder-and-measurement-contracts.md`.
- Do not synthesize `elapsed_ms` from row number or force samples onto a uniform timeline.
- Keep current CSV schema as `voltage,current,elapsed_ms`, with unused measurement columns blank.
- Preserve trigger ownership across stop timeout and reject overlapping recorder instances.
- Do not introduce automatic record deletion, storage formatting, or reboot as an error-recovery path.
- Preserve strict `REC-[0-9]+.csv` validation and separation from legacy `MO-*` records.
- Preserve single percent-decode, basename validation, fixed record directory, and bounded streaming for downloads.
- Do not describe or implement the current recorder as guaranteed 25 Hz without addressing Issue #3 and its acceptance criteria.

## CMake, dependencies, and generated code

- Follow the existing CMake style in the edited target.
- Do not edit fetched/generated code under `dependencies/`, `platforms/vameter/components/`, or `managed_components/`.
- Dependencies are defined in `repos.json`; do not silently change versions or substitute symlinks.
- `fetch_repos.py` is non-idempotent and does not reliably propagate individual clone failures; validation is required after running it.
- Avoid generating tracked changes to `sdkconfig` or `dependencies.lock` unless explicitly required.

## High-caution areas

Do not generate broad or speculative edits to these areas without an explicit task and validation plan:

- `platforms/vameter/main/hal_vameter/components/hal_va_recorder.cpp`
- filesystem/storage and incomplete-file cleanup
- HTTP filename validation and CSV streaming
- trigger/task/resource lifetime
- measurement semantics and calibration
- ESP-IDF component search paths
- AssetPool/localization
- Git worktree and dependency setup

## Validation hints

Relevant candidates include:

```bash
git diff --check
cmake -S . -B build/desktop
cmake --build build/desktop -j 2
ctest --test-dir build/desktop --output-on-failure
```

Device builds require an ESP-IDF v5.1.6 environment. Hardware-dependent behavior still requires physical VAMeter validation.
