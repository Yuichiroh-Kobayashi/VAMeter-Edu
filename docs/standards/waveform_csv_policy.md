# Waveform CSV Policy

## Scope

Applies to waveform recording, CSV export, local download, and student-device compatibility.

## Rules

- Preserve backward compatibility when possible.
- Document column names and units.
- Document header rows and summary rows.
- Verify that student devices can open the file if classroom use is affected.
- If format changes may break existing worksheets or spreadsheet templates, state the impact.
- If CSV format, column meaning, or download behavior changes, update `CHANGELOG.md` or explicitly state why it was not updated.

## Required distinction

Do not confuse:

- displayed value
- sampled value
- filtered value
- CSV-recorded value
- summary value

## Verification

Record validation in `docs/operations/waveform_validation_log.md`.

If validation is not performed, write `未確認` or `未検証` as appropriate.
