# Local CSV download

## Scope

The local download path currently supports only record files matching
`^REC-[0-9]+\.csv$`. The digits are ASCII only. The recorder writes the
timestamped schema described below; the HTTP response remains byte-transparent.

The feature keeps the existing waveform record selection, QR screen, local AP,
AP suffix, and `/download/<record-name>` workflow. It needs neither an internet
connection nor a cloud account.

Record files are also reachable from `Settings → Files → Record files`. If
storage is running low, users can open an unneeded recording there and delete
that recording individually.

The complete deletion path is `Settings → Files → Record files →
<record> → Delete`. Deletion requires confirmation; the firmware neither deletes
old recordings automatically nor formats storage when it becomes full.

After D2B became the primary live path, the educational waveform CSV recorder is
the fallback. It uses manual trigger only and a fixed 5-second recording. Pressing
the encoder starts recording, encoder rotation changes only
the horizontal time-axis zoom, and a short side-button click reaches the
dedicated Help-request hook without changing measurement or recorder state. A long hold exits the waveform screen. While `state_saving` is active, both short click and long hold are ignored: saving neither enters Help nor exits Waveform. The project has no HelpEvent transport sink yet, so the hook currently emits a diagnostic log. Before starting, the recorder requires at least 64 KiB free.
This is conservative headroom for temporary chunks and the final CSV to coexist,
including FAT allocation and directory metadata; it is not the measured size of
one 5-second CSV. If storage information cannot be read or the threshold is not
met, recording is rejected with a warning and the device does not reboot.

The existing recorder checks duration completion before the 5-second chunk
rotation, so the current policy avoids the known second-chunk rotation. Per-sample
synchronous FAT writes remain, Issue #3 stays open, and neither gap-free CSV nor
uniform 25 Hz sampling is claimed.

Current V-I records must match `REC-[0-9]+.csv` using ASCII digits. Legacy
`MO-[0-9]+.csv` files remain visible only as individual cleanup targets: they do
not affect REC numbering, latest-record selection, preview, or QR download.
Numbering uses one more than the maximum valid REC id and does not reuse gaps.

In MSC mode VAMeter appears as USB storage and the serial monitor disconnects as
expected. After file operations, safely eject the drive in the OS, end MSC mode
on VAMeter, and reconnect the monitor if needed. Firmware-side filesystem writes
must not run concurrently with MSC access.

Motor Observe, its `MO-*.csv` files, and all motor control or measurement paths
are outside this implementation.

## Timestamped record schema

New records use this header:

```csv
voltage,current,elapsed_ms
```

The header is followed immediately by samples. There is no summary row and new
records contain neither capacity nor energy columns. The active waveform mode is
passed explicitly to the recorder when its trigger is created:

```csv
<voltage>,,<elapsed_ms>
,<current>,<elapsed_ms>
<voltage>,<current>,<elapsed_ms>
```

These are respectively voltage-only, current-only, and internal both-mode
compatibility rows.

`elapsed_ms` is captured from `esp_timer_get_time()` relative to the instant
immediately before the recording loop. It records scheduler delay and jitter;
it is not synthesized from the sample index. Voltage keeps four decimal places
and current keeps seven. The blank channel is not measured differently or
inferred from its value; the CSV writer uses the explicit recorder channel mode.

The current manual-trigger, nominal 40 ms, no-pretrigger acceptance path requires the first timestamp near zero and strictly increasing millisecond values. The CSV format itself does not promise unique millisecond timestamps for every possible future sampling mode. Faster sampling needs an explicit duplicate policy. If pretrigger is enabled in the future, account for the current implementation's ability to emit multiple pretrigger samples at `0 ms`; do not silently rewrite or interpolate them to hide timing behavior.

Preview reads one logical line at a time and uses only the first two sample
columns. It accepts the legacy `voltage,current,time,capacity,energy` header,
the previous `voltage,current,elapsed_ms,capacity,energy` header, the new
three-column header, legacy two-column samples, previous five-column samples,
new voltage-only/current-only/both samples, and the old summary row. One empty
measurement channel is represented as zero in preview; both channels empty is
not a sample. Empty, malformed, and overlong rows are skipped without leaving
the file position in the middle of a row. The existing maximum preview row count
remains in force.

## Recorder trigger lifetime

After successful creation, the HAL recorder status owns the trigger. Failed
preparation and failed task creation release it immediately. A stop timeout does
not delete the status or trigger and prevents another recorder from being
created. The task marks its resource finished only at its final cleanup point;
a later or repeated destroy call then releases the trigger exactly once.

## Educational waveform staged Auto scale

Voltage-only and current-only waveform views have no manual vertical-scale
selection. Every new view starts at the smallest scale. Auto scale examines the
positive peak across the complete visible ring buffer, not only the newest
sample. It moves up immediately while `positive_peak >= 8 × current_scale`. It
moves down by one step only when `positive_peak < 4 × lower_scale`; equality does
not move down. A motor peak therefore holds the larger scale until that peak has
left the visible buffer. Voltage and current keep independent view-local state.

The educational plot origin is Y=20 and its height is 180 px. Guide spacing is
20 px. The zero reference is Y=200, the 4-division label is Y=120, the
8-division label is Y=40, and the scale readout is centered at Y=220. Thus the
positive plot range is exactly nine divisions: `bottom = 0`,
`top = 9 × value_per_div`. The zero line and `0V`/`0A` label use the active
waveform color. Only the 4- and 8-division positive labels are shown. The old
panel-edge visible-buffer max/min labels, TIME indicator, manual-selection badge,
and manual-trigger M icon are not rendered in the single-channel views; the
centered live value remains.

Voltage scales and labels are:

| V/div | 4 div | 8 div |
|---:|---:|---:|
| 0.1 | 0.4V | 0.8V |
| 0.2 | 0.8V | 1.6V |
| 0.5 | 2.0V | 4.0V |
| 1.0 | 4.0V | 8.0V |
| 2 | 8V | 16V |
| 5 | 20V | 40V |

Current scales are stored internally in amperes:

| Scale | 4 div | 8 div |
|---:|---:|---:|
| 0.1 mA/div | 0.4mA | 0.8mA |
| 0.2 mA/div | 0.8mA | 1.6mA |
| 0.5 mA/div | 2.0mA | 4.0mA |
| 1.0 mA/div | 4.0mA | 8.0mA |
| 2 mA/div | 8mA | 16mA |
| 5 mA/div | 20mA | 40mA |
| 10 mA/div | 40mA | 80mA |
| 20 mA/div | 80mA | 160mA |
| 50 mA/div | 200mA | 400mA |
| 100 mA/div | 400mA | 800mA |
| 0.2 A/div | 0.8A | 1.6A |
| 0.5 A/div | 2.0A | 4.0A |
| 1.0 A/div | 4.0A | 8.0A |

Microamp scale readouts use ASCII `uA`.

Plot clipping does not modify the HAL values received by the UI or the values passed to CSV. Application-level `shuntCurrent` and CSV `current` are in amperes; screen formatting and CSV start from the same processed HAL value. Waveform `_pm_data_a_scale = 1000` is chart-internal numeric scaling only, not calibration or a unit correction.

Processing before the UI is range-specific. The high-current reverse-measurement path can preserve negative current. The low-current path applies its existing current offset and then clamps a negative result to `0 A`. Therefore, do not claim that every range preserves negative sensor-raw current in CSV. Never infer a 1000x correction, INA226 calibration, LC/HC switch, offset, or shunt-coefficient change from UI or CSV behavior.

USB-C Power Monitor → Waveform reaches internal `mode_both`. It records both voltage and current columns, retains the legacy combined chart and each channel's min/mid/max-following labels, and remains outside single-channel staged Auto scale. It has no manual vertical-scale control and no TIME/V/I `/div` readout. Encoder rotation remains horizontal zoom; side short click calls the Help hook and side long hold exits except while saving. Panel-edge max/min-label removal applies only to single-channel views.

## Filename and path handling

The QR entry point and HTTP entry point use the same basename validator. The
HTTP entry point takes the encoded basename after the literal `/download/`
route, performs percent-decoding exactly once, and then validates the decoded
bytes. It does not decode the result a second time.

The validator rejects empty names, names longer than 64 bytes, control bytes
including NUL and DEL, `/`, `\`, `..`, non-ASCII digits, unexpected extensions,
and unexpected prefixes. Only a validated basename is joined to the fixed
record directory; callers cannot select an arbitrary directory.

The selected record name and path are stored as one mutex-protected value.
Starting the server replaces both fields together before starting the AP, so
the complete selection is published before the download endpoint becomes
reachable. Each request takes one consistent snapshot, and stopping the server
clears both fields before stopping the AP. The mutex is released before
validation, file access, and HTTP streaming, so an in-flight request continues
with its initial snapshot if the selection changes.

## Chunked streaming

The HTTP handler opens the selected file in binary mode and transfers it with a
fixed 8 KiB heap buffer. It does not seek for the file size or allocate memory
proportional to that size. Each successful chunk yields to the scheduler.

A zero-byte file is a valid CSV response: no data chunk is emitted and the
chunked response is finished normally. A short `fread` is treated as EOF only
when `ferror` is clear. Read, send, close, and final-chunk failures are not
reported as successful completion. RAII closes the file on every exit after a
successful open.

## Verification status

### Verified by host/build tests

- Host tests: passed. These cover valid and invalid names, decoded URL input,
  length boundaries, zero-byte input, multiple chunks, a short final chunk,
  read failure, send failure, and final-chunk failure.
- Desktop build and all seven host tests: passed with two build jobs. The tests
  include local CSV download, the mode-specific three-column CSV schema and
  legacy parsing, trigger ownership, staged Auto-scale geometry, exact ASCII
  label formatting, hysteresis, and clipping. The changed files emitted no
  warning.
- Merge commit `c9bb8ca310b879ca54e83bcea91ff941463d50d1`
  passed the post-merge clean-build validation.
- The seven dependencies were fetched as real directories and verified
  against `docs/ai/dependency-baseline.md`.
- No cross-worktree dependency symlink was used.
- Desktop configure/build and all seven host tests passed.
- ESP-IDF v5.1.6 application, bootloader, and partition builds passed
  without unintended tracked changes or partition overflow.
- The clean-build artifact is provenance evidence and is not proof of
  the exact binary flashed to the validation device.
- The final Auto-scale/CSV change did not modify localization or static
  assets, so no AssetPool regeneration was required for that change.
- AssetPool generation, provenance, and flashing are documented separately
  in `docs/ai/build-and-validation.md`.

### Verified on physical hardware

- Normal boot and ordinary screen rendering.
- Voltage-only and current-only staged Auto scale.
- Manual-triggered fixed 10-second recording using the current nominal 40 ms, no-pretrigger path.
- Current three-column CSV schema and mode-specific blank columns.
- `elapsed_ms` starts at zero, is strictly increasing for this accepted path, and ends near 10 seconds.
- `REC-000.csv` through `REC-007.csv` were numbered consecutively.
- USB-C `mode_both` CSV filled both voltage and current columns.
- MSC transfer of all eight validation records.
- Settings → Files, individual REC deletion, and individual legacy MO deletion.
- Recording refusal below the 64 KiB margin without a reboot.
- QR/AP download of a small CSV and opening that CSV in Excel.
- Corrected storage warning and delete-confirm displays.

### Not yet verified on physical hardware

- Transfer of a long CSV larger than 8 KiB across multiple chunks.
- Client disconnect during transfer.
- Reconnection and repeated download of the same file.
- AP suffix physical regression verification; the feature exists, but this regression was not executed.
- Intentional mid-stream I/O failure injection.
- Recorder stop-timeout injection.
- Help short-click diagnostic hook; no explicit physical log evidence has been retained.

## Physical-device regression checklist

Do not mark an item PASS without performing it on hardware.

- [x] Boot the device and confirm normal startup.
- [x] Open Voltage Meter and confirm measurement continues normally.
- [x] Open Current Meter and confirm measurement continues normally.
- [x] Open Waveform and record data.
- [x] Confirm a `REC-*.csv` file is generated and selectable.
- [x] Open the existing QR download screen.
- [x] Download the CSV from a client connected to the device AP.
- [x] Download a small CSV.
- [ ] Download a long CSV spanning many chunks.
- [ ] Disconnect the client during a long download.
- [ ] Reconnect and download the same file again.
- [x] Open the downloaded file in Excel.
- [ ] Configure an AP suffix and confirm the advertised SSID still uses it.
- [x] Verify `elapsed_ms` begins near zero, increases monotonically with real jitter,
      and ends near 10000 ms; verify there is no summary row.
- [x] Verify voltage Auto scale starts at 0.1 V/div and follows the staged behavior.
- [x] Verify current Auto scale starts at 0.1 mA/div and follows the staged behavior.
- [x] Confirm voltage-only staged Auto-scale rendering on physical hardware.
- [x] Confirm current-only staged Auto-scale rendering and ASCII `uA` on physical hardware.
- [ ] Exercise a motor peak and confirm the scale stays large while the peak remains visible,
      then decreases without chatter after the peak leaves the buffer.
- [ ] Confirm that single-channel plot clipping does not further modify the processed HAL value written to CSV, while separately accounting for the documented LC/HC range behavior.
- [ ] Confirm a short side click logs a Help request without interrupting measurement or recording.
- [ ] Confirm a side-button long hold exits without also issuing a Help request.
- [ ] Confirm post-recording view recreation restarts at the minimum scale and immediately
      re-evaluates the currently visible buffer.
- [ ] Inject a stop timeout and confirm a second recorder is rejected until task exit.
