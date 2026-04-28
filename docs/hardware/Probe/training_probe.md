# Training Probe

## Purpose

The Training Probe is used for analog meter reading practice.
It supports the Read → Check → Re-read loop.

## Current project status

- Training UI with answer-check behavior: 実装済み / 要コード確認
- Classroom practice: 実践済み in prior research context
- Automatic calibration: future work / 未実装扱い unless verified later

## Learning loop

1. Student reads analog meter while digital value is hidden.
2. Student presses the dial to reveal digital value.
3. Student releases the dial to hide value again.
4. Student re-reads the analog scale.

## Rule

Answer-check behavior is Training Probe specific.
Do not apply it to Normal Probe UI unless explicitly requested.
