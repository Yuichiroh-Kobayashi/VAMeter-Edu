# Local CSV download

## Scope

The local download path currently supports only record files matching
`^REC-[0-9]+\.csv$`. The digits are ASCII only. No additional CSV prefix or
schema is introduced by this implementation.

The feature keeps the existing waveform record selection, QR screen, local AP,
AP suffix, and `/download/<record-name>` workflow. It needs neither an internet
connection nor a cloud account.

Motor Observe, its `MO-*.csv` files, and all motor control or measurement paths
are outside this implementation.

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

Not executed:

- All physical-device regression checks below.

## Physical-device regression checklist

Do not mark an item PASS without performing it on hardware.

- [ ] Boot the device and confirm normal startup.
- [ ] Open Voltage Meter and confirm measurement continues normally.
- [ ] Open Current Meter and confirm measurement continues normally.
- [ ] Open Waveform and record data.
- [ ] Confirm a `REC-*.csv` file is generated and selectable.
- [ ] Open the existing QR download screen.
- [ ] Download the CSV from an iPad or PC connected to the device AP.
- [ ] Download a small CSV.
- [ ] Download a long CSV spanning many chunks.
- [ ] Disconnect the client during a long download.
- [ ] Reconnect and download the same file again.
- [ ] Open the downloaded file in Excel or Google Sheets.
- [ ] Configure an AP suffix and confirm the advertised SSID still uses it.
- [ ] Confirm the existing UI is unchanged while HelpEvent remains unimplemented.
