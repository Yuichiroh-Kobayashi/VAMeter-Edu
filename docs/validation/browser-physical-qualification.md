# Browser physical qualification

This is the current procedure for qualifying the device-hosted Viewer and D2B live stream
against a real VAMeter and real browsers. It is adapted from, and supersedes as current
authority, the historical
[D2B V/I live-stream validation plan](../archive/plans/d2b-vi-live-validation-pre-beta1.md),
which was pre-`beta.1` planning material. That archived plan is historical planning
evidence for what was intended before the Viewer/SystemLive implementation existed; it is
not a record of what was actually executed for `v2.0.0-beta.1`. The actual `v2.0.0-beta.1`
physical-validation outcome (Windows Edge 151, iPad 7th generation iPadOS 18.7.9 Safari) is
its own separate evidence, recorded in the
[`v2.0.0-beta.1` CHANGELOG entry](../../CHANGELOG.md) and
[release notes](../releases/v2.0.0-beta.1.md); this procedure does not retroactively claim
to be what was followed to produce that earlier evidence. See
[`tests/d2b_vi_integration/README.md`](../../tests/d2b_vi_integration/README.md) for the
capture tooling this procedure uses.

This procedure does not authorize firmware or AssetPool flashing, device-storage writes,
calibration changes, dependency changes, recorder changes, or Issue #3 work by itself;
those each require their own explicit authorization. Issue #2 (calibration persistence) is
closed as not planned and is not applicable here. The protocol repository's
specification, schemas, golden vectors, Python validator, and browser reference parser
remain the oracle for D2B protocol conformance. A disagreement with the oracle is a
product failure until evidence proves an oracle defect; do not change schemas or vectors
as a workaround.

## Required provenance

Record before testing:

- VAMeter-Edu worktree, branch, exact commit, and clean `git status`;
- oracle repository path, branch, exact commit, and clean `git status`;
- ESP-IDF version and firmware binary SHA256;
- exact firmware commit represented by the flashed binary;
- device identity and serial port, without opening, resetting, or writing it;
- browser name, full version, OS, workstation time zone, and test time;
- whether firmware was flashed during this attended session and under which explicit
  authorization.

If flashed-firmware provenance cannot be tied to the tested source commit, report No-Go. A
successful build is not evidence that its binary is on-device.

## Preflight gates

1. Record the exact application image size and the application partition's capacity and
   free bytes. Any partition overflow is stop. Record the difference against the exact
   approved/released baseline (see
   [`../architecture/resource-budget.md`](../architecture/resource-budget.md#recorded-v200-beta1-resource-facts)
   for the recorded `v2.0.0-beta.1` baseline figures); a resource-sensitive increase is
   review-required under
   [`../architecture/resource-budget.md`](../architecture/resource-budget.md). Do not
   invent a percentage threshold beyond the measured baseline. Static IRAM, the full
   linker segment, flash partition layout, and AssetPool are separate constraints and must
   be evaluated on their own terms, not inferred from the application-partition figure.
2. Confirm no tracked changes to `sdkconfig`, partition definitions,
   `dependencies.lock`, or the dependency repositories.
3. Run the oracle Python validator; it must pass all schemas and vectors.
4. Serve the oracle repository on loopback and open its browser reference page in each
   browser under test. Record a screenshot or saved page showing all golden vectors and
   parser-core self-tests passing, browser version, and no window errors.
5. Run the VAMeter-Edu host build/CTest and the ESP-IDF device build.
6. Run the live-capture validator self-test described in
   [`tests/d2b_vi_integration/README.md`](../../tests/d2b_vi_integration/README.md).

Do not connect a loopback-hosted page to the device WebSocket: the product enforces a
same-origin/expected-Origin check on `/d2b/v0/stream`, and weakening that check is not a
test shortcut — see
[`../architecture/origin-admission-policy.md`](../architecture/origin-admission-policy.md).

## Device setup

1. With an operator present, boot the approved firmware normally.
2. Join the VAMeter-Edu access point and open the device-hosted Viewer in the browser
   under test.
3. Confirm `/d2b/v0/capabilities` and `/d2b/v0/status` are reachable, and confirm
   configuration pages/APIs (e.g. `/syscfg`) and the Download surface are *not* reachable
   while SystemLive is active. `WEB_SERVER_PROFILE::PolicyFor(SystemLive)` sets
   `configurationPages`/`configurationApis`/`downloadRoutesAllowed` to `false` and only
   registers Viewer/D2B routes; observing a reachable `/syscfg` or download route during
   SystemLive is a fail, not an expected condition. See
   [`../architecture/direct-browser-service-profiles.md`](../architecture/direct-browser-service-profiles.md).
4. Keep the serial log visible for watchdog, allocation, send-failure, owner, and
   stack-high-water evidence (see
   [`../operations/d2b-runtime-diagnostics.md`](../operations/d2b-runtime-diagnostics.md)).
   Do not issue commands that write device storage or alter calibration.

## Live capture per browser

For each browser, run the tracked capture helper in
[`tests/d2b_vi_integration/`](../../tests/d2b_vi_integration/README.md) against the
device-hosted page. A 30-second capture is the minimum smoke test. The helper fetches
capabilities and public status from the same origin, opens `/d2b/v0/stream`, performs
`hello`/`start_stream`/an orderly `stop_stream`, records event order and control/binary
bytes without treating browser arrival time as a sample timestamp, and downloads a bounded
JSON capture only after `STREAM_END`, `stream_stopped`, and a drained idle status.

Validate each capture with the oracle-backed validator command in the integration-test
README, and preserve the capture, validator output, browser screenshot, and serial log
outside Git with SHA256 checksums.

PASS requires: exact valid capabilities/control messages; one 48-byte, one-record V/I
frame per data message; `STREAM_START` on the first data frame; strictly valid
sequence/timestamp continuity under oracle rules; canonical positive `0.0f` bits for every
invalid channel; one final 32-byte `STREAM_END` before `stream_stopped`; idle/drained
state after orderly stop; no owner, send, allocation, watchdog, or task-fault error.

## Actual V/I comparison

Use a stable, current-limited circuit and independently safe reference instrumentation.
Record at least three steady points that exercise voltage and current, including a
near-zero point. For each point, record the circuit/load description, reference-instrument
identity, device display voltage/current with observation time, decoded D2B
voltage/current with `valid_mask`/sequence/`timestamp_us`, and the difference against the
acceptance criterion agreed before the run. Compare only valid channels — an invalid
channel is absence of a measurement, not a measured zero (see
[`../standards/measurement-and-presentation-semantics.md`](../standards/measurement-and-presentation-semantics.md#d2b-value)).
Do not invent a calibration tolerance, reinterpret units, or modify existing display/CSV
numeric processing.

## Intentional gap and backpressure

Induce a bounded receive/network stall while preserving the WebSocket and stream identity,
then restore normal reception. A closed WebSocket is a reconnect test, not gap evidence. Do
not block the measurement pipeline, add filesystem I/O, or modify the device to
manufacture a gap. PASS requires a forward sequence gap reported as `DISCONTINUITY` with
the applicable `PRODUCER_OVERFLOW`/`OUTPUT_QUEUE_DROP` flag and matching nondecreasing
public counters; sequence numbers must not be compressed. If available tooling cannot
create a reproducible bounded stall, record the case as not performed rather than
inferring it from host tests.

## Abrupt disconnect and reconnect

1. Start a stream and close the browser tab or disable the client network without sending
   `stop_stream`.
2. Reconnect during the same device boot and make two normal captures.
3. Validate both files in one validator invocation.

PASS requires stale-owner cleanup, successful new ownership, distinct stream IDs, no
deadlock/watchdog reset, and no binary data before the new `stream_started`. An abrupt
disconnect does not require `STREAM_END` on the lost connection; the subsequent orderly
capture does.

## Soak and resource gates

Run a 30-minute stream per browser under test, polling public status without opening a
second WebSocket, and preserve the serial log after each soak. Check logged encoder/TX
task stack high-water: less than 512 bytes remaining is review-required, less than 256
bytes remaining is stop, and any allocation failure is stop. Record internal free-heap
samples at fixed intervals using an already-approved diagnostic method; a continuously
decreasing baseline is a leak and a stop condition. Stop immediately for a producer
blocked by network send, timing that changes with network conditions,
recorder/waveform regression, stale owner, deadlock, watchdog reset, or task fault.

## Recorder and waveform regression

Without changing their contracts, verify normal waveform operation and one manual
5-second fallback recording. Confirm the CSV remains `voltage,current,elapsed_ms`,
preserves real elapsed timestamps, uses blank unused columns, and remains downloadable
through the Files/AP flow — see
[`../product/educational-recording-and-local-download.md`](../product/educational-recording-and-local-download.md).
Do not claim uniform 25 Hz sampling; per-sample synchronous FAT writes remain and
[Issue #3](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3) stays open.

## Evidence record and decision

Create a dated validation record under `docs/archive/validation/` after the run,
following the existing `<date>-<topic>.md` naming used there. For each case, record PASS,
FAIL, or NOT PERFORMED, exact reproduction steps, expected/actual result, affected
logs/captures and their SHA256, stack/heap measurements, and firmware/hardware
provenance.

The release decision is No-Go if any mandatory item fails, firmware provenance is
unknown, flashing is still required, or a hardware-only requirement was not performed.
Host tests and synthetic vectors cannot upgrade a physical item to PASS.
