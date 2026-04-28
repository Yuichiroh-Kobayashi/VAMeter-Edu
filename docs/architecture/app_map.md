# App Map

## Purpose

Map VAMeter-Edu application areas so AI agents do not confuse UI, measurement, CSV, calibration, and hardware control.

## Current known areas

| Area | Role | Status |
|---|---|---|
| Simplified menu flow | Beginner-friendly entry points | 実装済み / 要コード確認 |
| Voltage fixed display | Voltage-focused display mode | 実装済み / 要コード確認 |
| Current fixed display | Current-focused display mode | 実装済み / 要コード確認 |
| Guide screens | Wiring support for beginners | 実装済み / 要コード確認 |
| Training UI | Analog meter answer-check loop | 実装済み / 要コード確認 |
| Waveform recording | Record voltage/current waveform | 実装済み / 要手順確認 |
| Local CSV download | Device AP + QR download | 実装済み / 要手順確認 |
| OTA Upgrade | Disabled for classroom firmware | 実装済み / 要コード確認 |
| Factory Reset | Disabled for classroom firmware | 実装済み / 要コード確認 |
| Help button | Future UI guidance | Future / 未実装 |

## Status vocabulary

- `実装済み / 要コード確認`: README or release notes claim the behavior exists, but this document has not verified the exact code path.
- `実装済み / 要手順確認`: Behavior exists, but classroom operation steps still need verification.
- `Future / 未実装`: Future work. Do not document it as usable and do not implement it unless explicitly requested.

## Rule

Do not change multiple areas in one task unless the task explicitly requires it.
