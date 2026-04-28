# Minimum Constraints

This file defines the minimum constraints that AI agents must follow in VAMeter-Edu.

## Project boundary

VAMeter-Edu is an educational firmware project for middle-school electricity learning.
It is not a certified laboratory instrument, not a safety-certified protection device, and not a competition motor controller.

## Hard constraints

- Safety, classroom operability, and misoperation tolerance have priority over convenience.
- Do not expose student names, student faces, school-internal information, private network details, or unpublished classroom details.
- School names and classroom photos are prohibited by default. If public use is necessary, confirm purpose and privacy safety first.
- Do not document unimplemented features as available.
- Unknown software behavior must be marked `未確認`.
- Unknown hardware behavior must be marked `未検証`.
- Prefer Reuse → Buy → Make.
- If making or modifying hardware-dependent behavior, define:
  - minimum prototype
  - verification steps
  - rollback condition

## Phase 1 rule

During Phase 1, prioritize code creation rules and verification logs.
User-facing manual generation, GitHub Pages publication, UI image generation, and Codex skills are deferred.
Do not create Phase 2 or later materials during unrelated Phase 1 tasks.
