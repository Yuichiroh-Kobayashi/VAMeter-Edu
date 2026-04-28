# Measurement State Machine

## Scope

This file describes passive measurement and Training Probe behavior only.
Active output safety belongs in `safety_state_machine.md`.

## Core states

| State | Description | Allowed action |
|---|---|---|
| Boot | Firmware starts and initializes UI / measurement services | move to mode selection or guide |
| ModeSelect | User selects voltage/current and probe-oriented UI | select mode |
| ConnectionGuide | Shows wiring guide | confirm / continue |
| MeasureHidden | Training mode: digital value hidden | read analog meter |
| MeasureDisplayed | Training mode: digital value shown during answer-check action | self-check |
| MeasureContinuous | Normal mode: measurement display / waveform | measure / record |
| RecordWaveform | Waveform capture / CSV write | stop / finish |
| LocalDownload | Local AP / QR download flow | download CSV |
| Error | Measurement or UI error | recover or reset |

## Training loop

Training Probe learning loop:

1. Read analog meter while value is hidden.
2. Perform the implemented answer-check action to reveal digital value.
3. End the answer-check action to hide value again.
4. Re-read analog meter.

## Rule

Do not implement Training Probe answer-check behavior in Normal Probe UI unless explicitly requested.

Do not change the physical answer-check operation, such as short press or long press, without checking the current implementation and updating user-facing documentation.
