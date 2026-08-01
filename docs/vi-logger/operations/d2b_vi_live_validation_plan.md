# D2B V/I live-stream validation plan

## Purpose and boundary

This plan validates the first VAMeter-Edu `d2b-stream/0.1` vertical slice: one
live `vi-measurement` stream containing the existing processed voltage and
current measurements. It does not authorize firmware or AssetPool flashing,
device-storage writes, calibration changes, dependency changes, recorder
changes, or Issue #2/#3 work.

The protocol repository's specification, schemas, golden vectors, Python
validator, and browser reference parser are the oracle. A disagreement is a
product failure until evidence proves an oracle defect. Do not change schemas
or vectors as a workaround.

## Required provenance

Record these before testing:

- VAMeter-Edu worktree, branch, exact commit, and clean `git status`;
- oracle repository path, branch, exact commit, and clean `git status`;
- ESP-IDF version and firmware binary SHA256;
- exact firmware commit represented by the flashed binary;
- device identity and serial port without opening, resetting, or writing it;
- Chrome and Edge full versions, OS, workstation time zone, and test time;
- whether the firmware was flashed during this attended test session and under
  which explicit authorization.

If flashed-firmware provenance cannot be tied to the tested source commit,
report No-Go. A successful build is not evidence that its binary is on-device.

## Preflight gates

1. Confirm the application image uses less than 85% of its partition. Stop at
   90% or any partition overflow. Treat 85% or more as review-required.
2. Confirm no tracked changes to `sdkconfig`, partition definitions,
   `dependencies.lock`, or the seven dependency repositories.
3. Run the oracle Python validator. It must pass all schemas and 95 vectors.
4. Serve the oracle repository on loopback and open
   `http://127.0.0.1:8000/reference/browser/` in both Chrome and Edge. Record a
   screenshot or saved page showing 95/95 golden vectors, all parser-core
   self-tests passing, browser version, and no window errors.
5. Run the VAMeter-Edu host build/CTest and ESP-IDF v5.1.6 build.
6. Run the live-capture validator self-test described in
   `tests/d2b_vi_integration/README.md`.

Do not connect a loopback-hosted page to the device WebSocket. The product
enforces a same-host `Origin`; weakening that check is not a test shortcut.

## Device setup

1. With an operator present, boot the approved firmware normally.
2. Join the VAMeter-Edu access point and open
   `http://<device>/d2b/v0/` in the browser under test.
3. Confirm `/d2b/v0/capabilities` and `/d2b/v0/status` are reachable while
   `/syscfg` remains functional. Confirm no download-server application is
   active at the same time.
4. Keep the serial log visible for watchdog, allocation, send-failure, owner,
   and stack high-water evidence. Do not issue commands that write device
   storage or alter calibration.

## Chrome and Edge live capture

For each browser, paste the complete tracked
`tests/d2b_vi_integration/capture-live.js` into DevTools Console on the device
page. A 30-second capture is the minimum smoke test. The helper:

- fetches capabilities and public status from the same origin;
- opens `/d2b/v0/stream` and records exact control text and binary bytes;
- performs `hello`, `start_stream`, and an orderly `stop_stream`;
- records event order and browser arrival time without treating arrival time as
  a sample timestamp;
- downloads a bounded JSON capture only after `STREAM_END`, `stream_stopped`,
  and a drained idle status.

Validate each JSON file with the oracle-backed command in the integration-test
README. Preserve the capture, validator output, browser screenshot, and serial
log outside Git with SHA256 checksums.

PASS requires:

- exact valid capabilities and control messages;
- one 48-byte, one-record V/I frame per data message;
- `STREAM_START` on the first data frame;
- strictly valid sequence/timestamp continuity under oracle rules;
- canonical positive `0.0f` bits for every invalid channel;
- one final 32-byte `STREAM_END` before `stream_stopped`;
- idle, drained state after orderly stop;
- no owner, send, allocation, watchdog, or task-fault error.

## Actual V/I comparison

Use a stable, current-limited circuit and independently safe reference
instrumentation. Record at least three steady points that exercise voltage and
current, including a near-zero point. For each point record:

- circuit/load description and reference-instrument identity;
- device display voltage/current and observation time;
- decoded D2B voltage/current, `valid_mask`, sequence, and `timestamp_us`;
- the difference and the acceptance criterion agreed before the run.

Compare only valid channels. An invalid channel is absence of a measurement,
not a measured zero. Do not invent a calibration tolerance, reinterpret units,
or modify existing display/CSV numeric processing. The live stream uses volts
and amperes and carries the processed PM-daemon values.

## Intentional gap and backpressure

Induce a bounded receive/network stall while preserving the WebSocket and
stream identity, then restore normal reception. A closed WebSocket is a
reconnect test, not gap evidence. Do not block the PM daemon, add filesystem
I/O, or modify the device to manufacture a gap.

PASS requires a forward sequence gap with `DISCONTINUITY` and the applicable
`PRODUCER_OVERFLOW` and/or `OUTPUT_QUEUE_DROP` flag, matching nondecreasing
public counters. Sequence numbers must not be compressed. If the available
browser/network tooling cannot create a reproducible bounded stall, record the
gap test as not performed; do not infer it from host tests.

## Abrupt disconnect and reconnect

1. Start a stream and close the browser tab or disable the client network
   without sending `stop_stream`.
2. Reconnect during the same device boot and make two normal captures.
3. Validate both files in one validator invocation.

PASS requires stale-owner cleanup, successful new ownership, distinct stream
IDs, no deadlock/watchdog reset, and no binary data before the new
`stream_started`. An abrupt disconnect does not require `STREAM_END` on the
lost connection; the subsequent orderly capture does.

## Soak and resource gates

Run a 30-minute stream in Chrome and Edge separately. The capture helper is
bounded to 100,000 frames and 1,800 seconds. Poll public status without opening
a second WebSocket. After each soak, stop orderly and preserve the serial log.

Check the logged encoder/TX task stack high-water values:

- less than 512 bytes remaining: review-required;
- less than 256 bytes remaining: stop;
- allocation failure: stop.

Record internal free-heap samples at fixed intervals using an already approved
diagnostic method. A continuously decreasing baseline is a leak and a stop
condition. If no approved method exists, heap-trend validation remains blocked;
do not substitute public queue counters for heap evidence.

Stop immediately for a producer blocked by network send, PM-daemon timing that
changes with network conditions, recorder/waveform regression, stale owner,
deadlock, watchdog reset, or task fault.

## Recorder and waveform regression

Without changing their contracts, verify normal waveform operation and one
manual 10-second recording. Confirm the CSV remains
`voltage,current,elapsed_ms`, preserves real elapsed timestamps, uses blank
unused columns, and remains downloadable through Files/AP flow. Do not claim
uniform 25 Hz sampling; Issue #3 remains separate.

## Evidence record and decision

Create a dated validation record under `docs/vi-logger/validation/` only after
the run. For each case record PASS, FAIL, or NOT PERFORMED, exact reproduction,
expected/actual result, affected logs/captures and their SHA256, stack/heap
measurements, and firmware/hardware provenance.

The release decision is No-Go if any mandatory item fails, firmware provenance
is unknown, flashing is still required, or a hardware-only requirement was not
performed. Host tests and synthetic vectors cannot upgrade a physical item to
PASS.
