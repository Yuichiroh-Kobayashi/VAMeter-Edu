# PR #1 recorder validation — 2026-07-29

## Scope and evidence

This report extracts the physical validation evidence for PR #1 from the dated [Revision 9 handoff](../../handoffs/2026-07-29-pr1-post-merge-handoff-rev9.md). It intentionally omits the longer Git cleanup and branch-history narrative.

Eight consecutive records, `REC-000.csv` through `REC-007.csv`, were created on physical VAMeter hardware and transferred through USB MSC.

Current schema:

```csv
voltage,current,elapsed_ms
```

There is no summary row. Voltage-only records leave `current` blank, current-only records leave `voltage` blank, and internal USB-C `mode_both` fills both measurement columns.

## Record results

| Record | Physical test | Sample rows | Final elapsed_ms | Voltage range | Current range | Maximum observed gap |
|---|---|---:|---:|---|---|---|
| REC-000 | 130 motor terminal voltage | 211 | 10011 | 0–6.64 V | blank | 608 ms, 5031→5639 |
| REC-001 | 130 motor current, run 1 | 206 | 10025 | blank | 0–0.3018 A | 616 ms, 5035→5651 |
| REC-002 | 130 motor current, run 2 | 206 | 10037 | blank | 0.0471–0.0957 A | 618 ms, 5020→5638 |
| REC-003 | 130 motor current, run 3 | 205 | 10028 | blank | 0.0699–0.0801 A | 632 ms, 5008→5640 |
| REC-004 | 130 motor current, run 4 | 207 | 10009 | blank | 0.0477–0.0969 A | 547 ms, 5020→5567 |
| REC-005 | ThinkPad USB-C, before display correction | 197 | 10030 | 0.0088–20.2763 V | 0–2.9832 A | 613 ms, 5006→5619 |
| REC-006 | 100 ohm resistor current | 206 | 10033 | blank | 0.0041175–0.004125 A | 624 ms, 5021→5645 |
| REC-007 | ThinkPad USB-C, after display correction | 197 | 10030 | 0.0162–20.3 V | 0–2.9847 A | 622 ms, 5003→5625 |

For the accepted manual-trigger, nominal 40 ms, no-pretrigger path, all eight files start with `elapsed_ms = 0`, have strictly increasing timestamps, and finish at approximately 10 seconds. This is an acceptance result for that path, not an unconditional guarantee that every future millisecond timestamp is unique.

## Physical validation status

Verified:

- voltage-only and current-only staged Auto-scale UI;
- fixed 10-second manual recording;
- three-column schema and mode-specific blank columns;
- USB-C `mode_both` recording with both columns populated;
- consecutive REC numbering;
- USB MSC transfer of all eight files;
- Settings → Files;
- individual REC and legacy MO deletion;
- small-file QR/AP download and opening the CSV in Excel;
- insufficient-space rejection without reboot;
- corrected warning and confirmation displays.

Not yet physically verified:

- HTTP transfer of a CSV larger than 8 KiB;
- client disconnect and reconnect/same-file repeat download;
- AP suffix regression, although the feature is implemented;
- intentional mid-stream I/O failure;
- recorder stop-timeout injection;
- Help short-click diagnostic log; no retained physical log evidence was found.

## Known sampling limitation

Ordinary intervals are nominally about 40 ms, but the records contain recurring gaps of about 196–272 ms and a common 5-second chunk-boundary gap of about 547–632 ms. The observed row counts are 197–211 rather than about 251–252. `elapsed_ms` exposes the delay correctly, but no samples exist inside each gap.

The current recorder is suitable for low-speed change and steady-value classroom observation. It must not be described as a guaranteed uniform 25 Hz logger or used for quantitative short-transient analysis until GitHub Issue #3 is resolved. Use `elapsed_ms` as the X value in an XY scatter plot.

## Checksums and provenance

The authoritative per-file SHA256 list is in Section 18 of the [Revision 9 handoff](../../handoffs/2026-07-29-pr1-post-merge-handoff-rev9.md) and in `SHA256SUMS` beside the archived LabData.
