# Measurement and presentation semantics

This is the current, durable authority for how VAMeter-Edu measurement values relate to
what appears on the device screen, in CSV records, and on the D2B live stream. It describes
only behavior implemented in current source and tests. For the recorder/CSV-specific
interaction contract, see
[`../product/educational-recording-and-local-download.md`](../product/educational-recording-and-local-download.md).
For the Viewer contract, see
[`../product/device-hosted-viewer-contract.md`](../product/device-hosted-viewer-contract.md).

## Value stages

Distinguish at least these stages when describing a measurement value:

| Stage | Meaning |
|---|---|
| Raw | Unprocessed value from the sensor/ADC path before range-specific processing |
| Processed (HAL) | The value after the existing HAL applies range-specific offset/clamp handling |
| Display | What the screen renders, including unit selection (`uA`/`mA`/`A`) and rounding |
| Recorded (CSV) | What the recorder writes to `REC-*.csv` |
| Streamed (D2B) | What the D2B live stream carries to the browser |

A display string is never the source of a CSV or D2B value. Changing display formatting
(rounding, unit prefix, axis, plot clipping) must never implicitly change the recorded or
streamed value.

## Raw/sensor vs. processed measurement

Application-level `POWER_MONITOR::PMData_t::shuntCurrent` is in amperes. CSV `current` and
the D2B live-stream current are the same processed HAL `shuntCurrent`, recorded/streamed
without an additional UI-only or CSV-only correction. Screen formatting and CSV/D2B output
start from the same HAL value; the screen's `uA`/`mA`/`A` unit switch is a display format,
not a different underlying value.

Measurement processing is range-specific before the UI receives the value:

- the high-current reverse-measurement path can preserve negative current;
- the low-current path applies its existing current offset and then clamps a negative
  result to `0 A` in the existing HAL.

Do not describe every measurement range as preserving negative sensor-raw current in CSV
or on the D2B stream. What is recorded/streamed is the HAL-processed value, not an
unprocessed sensor-register raw value.

Waveform `_pm_data_a_scale = 1000` exists only to preserve numeric precision inside the
chart implementation. It is not measurement calibration, a shunt correction, or an
A-to-mA conversion of the underlying value.

Never infer or add a 1000x correction, INA226 calibration change, LC/HC switch, offset
change, or shunt-coefficient change from UI, CSV, or D2B observations. Such measurement
changes require a separate hardware/calibration task and evidence, not inference from
presentation behavior.

## Display formatting

Single-channel waveform plot clipping (the educational zero-baseline clip on negative
values) acts only on what is drawn. It does not modify the HAL value the UI receives, the
value written to CSV, or the value carried on the D2B stream. Axis scale, staged Auto
scale, and rounding are display concerns; they do not redefine or rewrite the measurement
they present.

## CSV value

Current CSV schema is `voltage,current,elapsed_ms`. Unused measurement columns are blank
for the active waveform mode (voltage-only, current-only, or the internal both-mode
compatibility view). There is no summary row, capacity column, or energy column.
`elapsed_ms` is a real relative timestamp captured from a monotonic timer relative to the
start of the recording loop; it includes real scheduler delay and jitter and is not
synthesized from the sample index. See
[`../product/educational-recording-and-local-download.md`](../product/educational-recording-and-local-download.md)
for the full recorder contract.

## D2B value

The D2B `d2b-stream/0.1` live stream carries the same processed voltage/current values in
volts and amperes as the display, one 48-byte, one-record V/I frame per data message, with
sequence number and `timestamp_us`. An invalid channel is represented on the wire as the
canonical positive `0.0f` bit pattern together with `valid_mask` marking that channel
invalid; a canonical positive-zero value with an unset `valid_mask` bit is absence of a
measurement, not a measured zero. Compare and use only channels marked valid by
`valid_mask`.

## Valid / invalid / no-data

- **CSV:** a blank column for the inactive channel of the active waveform mode represents
  no data for that channel in that record; it is not a measured zero. There is no
  additional per-sample valid/invalid flag column in the current CSV schema — do not
  invent one.
- **D2B:** validity is carried explicitly by `valid_mask` alongside the canonical
  positive-`0.0f` placeholder described above.
- **Display:** the screen shows the current display-profile channel(s) only; it does not
  currently render a distinct "invalid" glyph beyond the existing measurement/range
  handling.

## No interpolation, zero-fill, or time compression

Stored and streamed timestamps are not regularized. The recorder does not synthesize
`elapsed_ms` from the sample index, and does not interpolate missing samples into the
stored CSV record. On the D2B stream, sequence numbers are not compressed to hide a gap: a
forward sequence gap is reported as `DISCONTINUITY` together with the applicable
`PRODUCER_OVERFLOW` and/or `OUTPUT_QUEUE_DROP` flag and nondecreasing public counters,
rather than being silently closed up. Real delay and jitter — including the recorder's
known per-sample synchronous FAT-write stalls tracked by
[VAMeter-Edu Issue #3](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3) — are
preserved in the data rather than smoothed away.

## Display rounding, axis, and clipping do not rewrite measurement authority

Merged source and tests are the authority for current implementation behavior. A display
choice (rounding, axis scale, staged Auto scale, or single-channel zero-baseline clipping)
never becomes the authority for what was measured, recorded, or streamed. If display
behavior and a recorded/streamed value appear to disagree, treat the recorded/streamed
value as authoritative and investigate the display path, not the reverse.

## Analog pointer-matching (current boundary)

VAMeter-Edu's Training Probe supports reading an analog meter's pointer and then checking
that reading against the device's digital value. No pointer-matching correction —
adjusting the digital value to compensate for an individual analog meter's pointer
offset — is implemented. This future work is tracked as the analog-meter answer-check
display correction in
[VAMeter-Edu Issue #9](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/9); this
document does not define a schema or behavior for it ahead of that work.

## Related historical draft

An earlier, broader measurement-semantics draft for an unimplemented internal-excitation
V-I logger product is preserved at
[`../archive/design/measurement-semantics-policy-future-draft.md`](../archive/design/measurement-semantics-policy-future-draft.md).
It is historical design exploration, not current authority.
