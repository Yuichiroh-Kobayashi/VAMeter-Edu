# VAMeter-Edu Agent Bootstrap Draft — Phase 1: Code-Centric Rules

This package is a reduced replacement for the previous all-in-one draft.
It is intended for the `edu-dev` branch and ESP-IDF `v5.1.3`.

## Scope

Phase 1 focuses on rules needed before letting AI agents modify firmware:

- classroom safety constraints
- coding and naming rules
- measurement / calibration / CSV rules
- minimal architecture maps
- VAMeter hardware references
- verification log templates
- GitHub Copilot path-specific instructions for firmware-related files

## Not included intentionally

This package does **not** include:

- `AGENTS.md`
- `site/`
- `.github/workflows/`
- `.agents/skills/`
- `docs/manuals/`
- user-facing GitHub Pages content
- UI image generation rules
- MAKER_DRIVE / Mabuchi_Motor documents
- ESP32-S3FN8 low-level datasheet summary

Use the existing repository-root `AGENTS.md` as the primary instruction entry point.

## Why site/workflow are excluded

GitHub Pages will eventually be published by GitHub Actions from a built `site/` artifact.
However, Phase 1 does not add Pages workflow because Markdown-only `site/` content needs a build step such as MkDocs or Jekyll before publishing.

## Copy policy

Do not blindly overwrite existing repository files.
Especially compare the existing `.github/copilot-instructions.md` and `AGENTS.md` before merging.
