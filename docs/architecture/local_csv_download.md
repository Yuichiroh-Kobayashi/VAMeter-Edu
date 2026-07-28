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

The educational waveform recorder uses manual trigger only and a fixed 10-second
recording. Pressing the encoder starts recording. A short side-button click
cycles the TIME/V-SCALE/I-SCALE control target available for the active channel
mode, while a long hold exits the waveform screen. Before starting, the recorder
requires at least 64 KiB free.
This is conservative headroom for temporary chunks and the final CSV to coexist,
including FAT allocation and directory metadata; it is not the measured size of
one 10-second CSV. If storage information cannot be read or the threshold is not
met, recording is rejected with a warning and the device does not reboot.

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
voltage,current,elapsed_ms,capacity,energy
```

The next row is the summary row:

```csv
,,<recording_duration_ms>,<capacity>,<energy>
```

Each sample is written as five CSV columns, leaving the summary-only fields
empty:

```csv
<voltage>,<current>,<elapsed_ms>,,
```

`elapsed_ms` is captured from `esp_timer_get_time()` relative to the instant
immediately before the recording loop. It records scheduler delay and jitter;
it is not synthesized from the sample index. `recording_duration_ms` is fixed
when the recording loop ends, before chunk close, merge, and final-file save, so
wrap-up time is excluded. Voltage and current precision and the existing
capacity and energy summary values are unchanged.

Preview reads one logical line at a time and uses only the first two sample
columns. It accepts the legacy `voltage,current,time,capacity,energy` header,
legacy two-column samples, and new three- or five-column samples. Empty,
malformed, and overlong rows are skipped without leaving the file position in
the middle of a row. The existing maximum preview row count remains in force.

## Recorder trigger lifetime

After successful creation, the HAL recorder status owns the trigger. Failed
preparation and failed task creation release it immediately. A stop timeout does
not delete the status or trigger and prevents another recorder from being
created. The task marks its resource finished only at its final cleanup point;
a later or repeated destroy call then releases the trigger exactly once.

## Educational waveform scale controls

The initial vertical scale is AUTO. In voltage-only mode, a short side click
toggles TIME and V-SCALE. In current-only mode it toggles TIME and I-SCALE. In
both mode it cycles TIME → V-SCALE → I-SCALE → TIME. Encoder rotation changes
horizontal zoom while TIME is selected and changes only the selected vertical
scale otherwise. Encoder click starts recording only while idle; input is
ignored by the saving screen. Scale settings remain in the app instance when
the waveform view is recreated after a recording, but are not stored in NVS.

Voltage choices are AUTO, 0.1, 0.2, 0.5, 1, 2, and 5 V/div. Current choices,
stored internally in amperes, are AUTO, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50,
100, 200, and 500 mA/div, plus 1 and 2 A/div. The 2 A/div upper choice covers
the existing INA226 high-current physical limit without changing calibration.

The chart keeps its 35 px origin, 170 px height, and 20 px horizontal-guide
spacing. Therefore one division is exactly 20 px and the visible chart spans
8.5 divisions. Manual full range is `value_per_div × 8.5`. Positive voltage is
shown from 0 V upward; a negative observation switches voltage safely to a
zero-centered range. Current is always zero-centered. Out-of-range samples are
clamped only for chart coordinate calculation, leaving measured and CSV values
unchanged. AUTO preserves the existing min/max following and minimum spans;
its displayed per-division value is the actual current full range divided by
8.5. Scale labels use ASCII units including `uA`.

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

Executed locally:

- Host tests: passed. These cover valid and invalid names, decoded URL input,
  length boundaries, zero-byte input, multiple chunks, a short final chunk,
  read failure, send failure, and final-chunk failure.
- Desktop build: passed with two build jobs. The build emitted pre-existing
  deprecated enum-formatting warnings in the Waveform recorder; the changed
  files emitted no warning.
- ESP-IDF v5.1.6 clean firmware build: passed with two CPUs available to the
  build process. The changed files emitted no new diagnostic. Existing project
  and dependency warnings remain.

Executed on physical hardware:

- Normal boot and ordinary screen rendering.
- Voltage and current measurement screens.
- Manual-triggered 10-second recording, REC file generation, and REC numbering.
- Settings → Files, individual REC deletion, and individual legacy MO deletion.
- Recording refusal below the 64 KiB margin without a reboot.
- QR/AP download of a small CSV and opening that CSV in Excel.
- Corrected storage warning and delete-confirm displays.

Not yet executed on physical hardware:

- Transfer of a long CSV larger than 8 KiB across multiple chunks.
- Client disconnect during transfer.
- Reconnection and repeated download of the same file.
- AP suffix verification.
- Intentional mid-stream I/O failure injection.
- Recorder stop-timeout injection.
- Timestamped CSV and vertical-scale controls introduced by this follow-up.

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
- [ ] Verify `elapsed_ms` begins near zero, increases monotonically with real jitter,
      and ends near 10000 ms; verify the summary duration excludes saving time.
- [ ] Verify TIME/V-SCALE/I-SCALE cycling in voltage, current, and both modes.
- [ ] With a 0–5.13 V signal, select 1 V/div and confirm a 0 V baseline and clear trace.
- [ ] With a 0.4275–0.5700 mA signal, select 0.1 then 0.2 mA/div and confirm safe clipping
      followed by an in-range trace.
- [ ] Exercise a motor peak, widen current scale, and confirm the firmware remains stable.
- [ ] Confirm AUTO recovery and persistence after the post-recording view is recreated.
- [ ] Inject a stop timeout and confirm a second recorder is rejected until task exit.
