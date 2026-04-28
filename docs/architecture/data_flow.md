# Data Flow

## Purpose

Define where measurement values come from and where they go.

## Flow

```text
INA226 / hardware measurement
  -> raw measured value
  -> filter / smoothing
  -> branch by intended use

Display path:
  filtered value
  -> optional calibration / analog-meter alignment
  -> display formatting
  -> screen

CSV path:
  filtered value or explicitly selected CSV source
  -> CSV sample writer
  -> local download
```

## Rules

- Display formatting must not be the source of CSV values.
- CSV values must not silently change when display formatting changes.
- CSV values must include documented units.
- CSV summary rows and sample rows must be documented separately when they differ.
- Calibration must state whether it affects display, CSV, or both.
- If the value source is unknown, mark it `未確認`.
