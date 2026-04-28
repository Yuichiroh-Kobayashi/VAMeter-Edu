# Probe Mode Matrix

## Purpose

Keep Normal Probe, Training Probe, measurement display, waveform, and CSV behavior separate.

## Probe / measurement modes

| Mode | Probe | Purpose | UI behavior | Measurement | Status |
|---|---|---|---|---|---|
| Normal Voltage | Normal Probe / commercial test lead | General voltage measurement | beginner-friendly voltage display / waveform | voltage | 実装済み / 要コード確認 |
| Normal Current | Normal Probe / commercial test lead | General current measurement | beginner-friendly current display / waveform | current | 実装済み / 要コード確認 |
| Training Voltage | Training Probe with Combined Terminals | analog voltmeter reading practice | answer-check loop | voltage | 実装済み / 要コード確認 |
| Training Current | Training Probe with Combined Terminals | analog ammeter reading practice | answer-check loop | current | 実装済み / 要コード確認 |

## Data export / classroom workflow

| Function | Applies to | Purpose | UI behavior | Data | Status |
|---|---|---|---|---|---|
| Local CSV Download | Recorded data | student-device data access | local AP + QR | CSV | 実装済み / 要手順確認 |

## Rule

Do not mark a mode as unavailable solely because it still needs code review.
Use `実装済み / 要コード確認` or `実装済み / 要手順確認` when README claims the behavior exists but this file has not verified the code path.

Do not treat data export functions as probe modes.
