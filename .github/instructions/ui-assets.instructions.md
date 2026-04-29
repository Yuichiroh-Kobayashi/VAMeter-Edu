---
applyTo: "app/assets/**,app/localization/**,app/**/localization*,app/apps/**/view/**,app/apps/app_launcher/**"
---

# UI, localization, and asset instructions

Follow `AGENTS.md` first.

## Scope

These instructions apply to:

- Japanese UI text
- localization keys
- guide screens
- app icons
- menu labels
- connection diagrams
- classroom-facing display behavior

## Classroom UI rules

- Use short Japanese wording for user-facing text.
- Avoid ambiguous menu labels.
- Prefer labels that match classroom operation.
- Do not expose school names, student names, private network names, or internal information.
- Do not add text that assumes a specific school environment.

## Guide screens

When changing guide screens or connection diagrams:

- Keep the diagram consistent with the selected measurement mode.
- Voltage measurement should not be confused with current measurement.
- Current measurement should emphasize series connection.
- Voltage measurement should emphasize parallel connection.
- If a guide image changes, update documentation that references it.

## Localization

When adding or changing localization keys:

- Keep key names consistent with existing naming style.
- Do not leave unused keys unless the reason is documented.
- Do not change a key meaning without checking all callers.
- Prefer Japanese text for classroom-facing labels.
- Preserve existing English technical identifiers when needed.

## Asset changes

Generated asset changes should not be mixed with unrelated logic changes unless necessary.

During Phase 1, do not create new generated UI assets unless explicitly requested.
Prefer documenting the required UI behavior before adding generated headers or binary assets.

If generated headers or binary assets are updated, summarize:

- source image or source font
- generation method
- affected screen
- verification status

Generated assets must be treated as drafts until human review is recorded.

## Font / localization caution

When changing UI text:

- Do not assume Japanese glyphs are available.
- If Japanese text is added, verify font subset coverage.
- Bring-up UI should use ASCII or English-first text unless font coverage is verified.
- Do not add or regenerate font assets without explicit request.
- Do not add localization entries for development-only bring-up UI unless explicitly requested.
