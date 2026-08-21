# Recorder and measurement contracts

Read this document before editing waveform UI, recorder lifecycle, CSV, storage, Files, download, or measurement semantics.

## User interaction

Single-channel waveform views:

- manual trigger only;
- fixed 5-second fallback recording after D2B became the primary live path;
- encoder click starts while idle;
- encoder rotation changes horizontal time zoom only;
- side short click calls the Help-request hook and must not mutate recorder or measurement state;
- side long hold exits;
- while `state_saving` is active, both side short click and side long hold are ignored: saving must neither enter Help nor exit Waveform;
- no manual vertical scale;
- staged Auto scale;
- centered live value remains visible.

The recorder checks duration completion before the 5-second chunk rotation, so the current 5-second policy avoids the known second-chunk rotation. Per-sample synchronous FAT writes remain inside measurement, Issue #3 stays open, and neither gap-free CSV nor uniform 25 Hz sampling is guaranteed.

USB-C Power Monitor opens Waveform in internal `mode_both`. This compatibility view:

- records voltage and current together and fills both CSV measurement columns;
- retains the legacy combined voltage/current chart and each channel's min/mid/max-following labels;
- does not use single-channel staged Auto scale;
- has no manual vertical-scale control and no TIME/V/I `/div` readout;
- keeps encoder rotation as horizontal zoom;
- sends side short click to the Help hook and uses side long hold to exit, except that both are ignored while saving.

Panel-edge max/min-label removal is a single-channel rule and must not be generalized to `mode_both`.

## Staged Auto scale

Voltage stages:

```text
0.1, 0.2, 0.5, 1.0, 2, 5 V/div
```

Current stages:

```text
0.1mA, 0.2mA, 0.5mA, 1mA, 2mA, 5mA,
10mA, 20mA, 50mA, 100mA, 0.2A, 0.5A, 1A /div
```

Rules:

- new single-channel views start at the smallest intended stage;
- positive peak is taken from the complete visible buffer;
- scale up at `positive_peak >= 8 * current_scale`;
- scale down only when `positive_peak < 4 * lower_scale`;
- equality does not scale down;
- single-channel plot clipping does not rewrite the HAL values received by the UI or the values passed to CSV;
- only the educational single-channel plot clips negative values to the zero baseline;
- microamp labels use ASCII `uA`.

Do not reintroduce manual scale controls without a new, explicit education/UI decision.

## Current CSV schema

```csv
voltage,current,elapsed_ms
```

Voltage-only:

```csv
<voltage>,,<elapsed_ms>
```

Current-only:

```csv
,<current>,<elapsed_ms>
```

Both:

```csv
<voltage>,<current>,<elapsed_ms>
```

Contracts:

- header is immediately followed by samples;
- no current-record summary row;
- no capacity or energy columns;
- unused measurement columns are blank;
- `elapsed_ms` is sampled from a monotonic timer relative to the recording loop start;
- do not synthesize timestamps from row number;
- do not hide scheduler or storage delay by rewriting timestamps;
- do not interpolate missing samples into the stored record unless a future format explicitly distinguishes measured from interpolated data.

For the current manual-trigger, 40 ms, no-pretrigger acceptance path, timestamps are required to start near zero and be strictly increasing. The CSV format does not unconditionally guarantee unique millisecond values for every future sampling mode. Faster sampling or enabled pretrigger requires an explicit duplicate-timestamp policy; current pretrigger handling can emit multiple `0 ms` samples and must not be ignored when that feature is introduced.

## Current value and measurement-path semantics

- Application-level `POWER_MONITOR::PMData_t::shuntCurrent` is in amperes.
- CSV `current` is also in amperes and records the processed HAL `shuntCurrent` without an additional UI- or CSV-only correction.
- Screen formatting and CSV output start from the same HAL `shuntCurrent`; formatting may select `uA`, `mA`, or `A` without changing the stored unit.
- Waveform `_pm_data_a_scale = 1000` exists only to preserve numeric precision inside the chart implementation. It is not measurement calibration, a shunt correction, or an A-to-mA conversion of the underlying value.
- Measurement processing is range-specific before the UI receives the value. The high-current reverse-measurement path can preserve negative current. The low-current path applies its current offset and then clamps a negative result to `0 A` in the existing HAL.
- Therefore, do not state that negative sensor-raw current is preserved in CSV for every range. The accurate UI contract is that plot clipping itself does not further modify the HAL/CSV value.
- Never infer or add a 1000x correction, INA226 calibration change, LC/HC switch, offset change, or shunt-coefficient change from UI or CSV observations. Such measurement changes require a separate hardware/calibration task and evidence.

## Display regression strings

Preserve these reviewed layouts unless a separately validated localization change is requested:

```text
Rec error
See Files

No space
Delete
in Files

削除できません
```

They must fit the 240x240 display. Do not introduce unsupported glyphs; microamp remains ASCII `uA`. Validate wording changes on physical hardware, not only desktop. Localization or static-asset changes require considering AssetPool regeneration and separate flashing validation.

## Legacy preview compatibility

Preview must continue to handle safely:

- old five-column headers with `time` or `elapsed_ms`;
- new three-column records;
- legacy two-column samples;
- old summary rows;
- voltage-only, current-only, and both rows;
- extra columns;
- malformed, empty, and overlong rows.

Compatibility does not mean legacy `MO-*` records become current `REC-*` records.

## Record naming and selection

Current record name:

```text
REC-[0-9]+.csv
```

Rules:

- ASCII digits only;
- first record is `REC-000.csv`;
- next ID is maximum valid REC ID plus one;
- do not reuse gaps;
- legacy `MO-*` does not affect REC numbering or latest-record selection;
- `MO-*` may remain visible as an individual deletion target;
- `MO-*` is not a current preview/download target.

## Recorder lifecycle

- After successful recorder creation, HAL owns the trigger/resource.
- Preparation/task-creation failure releases ownership immediately.
- A stop timeout must preserve recorder state and resource ownership until the task actually exits.
- Do not release a trigger that the recorder task can still access.
- Reject a second recorder while a timed-out recorder remains active.
- Repeated destroy calls must not double free.
- Stop/finalization order must be explicit and testable.

## Storage and failure behavior

Before recording:

- check FAT storage usage;
- preserve the conservative 64 KiB free-space margin unless a reviewed design changes it.

On failure:

- no automatic record deletion;
- no filesystem format;
- no device reboot as normal recovery;
- expose a recoverable error;
- clean incomplete final output and temporary chunks according to the established policy;
- preserve enough state for diagnosis.

## Local download security and resource bounds

- percent-decode the requested filename once;
- validate the decoded basename strictly;
- join only to the fixed record directory;
- reject traversal and invalid names;
- stream with a fixed bounded buffer;
- do not load the complete CSV into RAM;
- handle zero-byte files explicitly;
- do not report success after read, send, or final-chunk failure.

## Known sampling limitation

GitHub Issue #3 is the source of truth.

Observed with eight 10-second records:

- nominal/median ordinary interval: about 40 ms;
- recurring gaps: about 196–272 ms;
- common 5-second chunk-boundary gap: about 547–632 ms;
- recorded rows: 197–211 instead of about 251–252.

`elapsed_ms` correctly exposes the delay, but samples do not exist inside the gaps.

Therefore:

- low-speed change and steady-value classroom observation: acceptable;
- graph with `elapsed_ms` as X using an XY scatter plot: acceptable;
- guaranteed uniform 25 Hz claim: prohibited;
- quantitative short-transient analysis: not supported until Issue #3 is resolved.

Any Issue #3 implementation must preserve the UI, CSV semantics, trigger lifetime, storage preflight, recoverable errors, and legacy preview behavior above. Buffer overflow or data loss must never be silent.
