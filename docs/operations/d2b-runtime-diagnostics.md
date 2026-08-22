# D2B runtime diagnostics

This describes the current `D2B_DIAG` diagnostic-only breadcrumb log
(`app/libs/runtime_evidence/`) used for reading device logs before and during physical
D2B/SystemLive testing. Do not claim measurement accuracy, a uniform sampling rate, or
heap/stack safety margins from these log lines alone: heap and stack values are
instrumented, not independently measured or load-tested by this logging layer itself.

This layer does not change the D2B `d2b-stream/0.1` protocol, its 32-byte envelope,
sequence/timestamp semantics, backpressure behavior, public HTTP endpoints, on-device
measurement display, CSV recording, or UI navigation. It only emits log lines.

## Line format

Every line begins with `D2B_DIAG` and carries these fixed keys in order:

```text
D2B_DIAG event=<token> reason=<token> result=<token> owner=<token> generation=<n> server_generation=<n> socket=<n> stream_id=<n> heap_free=<n> heap_min=<n> heap_largest=<n> encoder_stack_min=<n> tx_stack_min=<n> acquisition_depth=<n> output_depth=<n> producer_drops=<n> output_drops=<n>
```

A resource field that does not apply to a given line is `0`; `socket` uses `-1` when not
applicable. A boot line appends `reset_reason_code`, `reset_reason_raw`, `boot_identity`,
and `rtc_boot_counter` after the fixed resource fields.

No SSID, password, IP address, MAC address, payload bytes, measurement values, or
personal information is recorded. The logged structure has no string/text field capable
of carrying such data — only the fixed enum tokens and numeric counters below are emitted.

## `event` tokens

`boot`, `server_request`, `server_start`, `server_stop`, `websocket_connect`,
`websocket_disconnect`, `stream_start`, `stream_stop`, `stream_abrupt`, `queue_snapshot`,
`send_failure`, `heap_trend`, `stack_trend`, `pump`, `unknown`.

## `reason` tokens

`boot`, `network_settings_start`, `network_settings_intentional_stop`, `download_start`,
`download_intentional_stop`, `owner_acquire`, `owner_release`, `websocket_accepted`,
`websocket_rejected`, `stream_accepted`, `stream_orderly_stop_accepted`,
`stream_orderly_stop_completed`, `disconnect`, `server_stop`, `send_failure`,
`queue_drop`, `producer_drop`, `output_drop`, `internal_failure`, `origin_rejected`,
`protocol_violation`, `server_start_failed`, `server_stop_failed`, `active_stream_trend`,
`pump_schedule_accepted`, `pump_schedule_coalesced`, `pump_queue_rejected`,
`pump_callback_begin`, `pump_callback_end`, `pump_stale`, `unknown`.

## `result` tokens

`requested`, `succeeded`, `failed`, `rejected`, `accepted`, `completed`, `abrupt`,
`dropped`, `sent`, `observed`, `unknown`.

## `owner` tokens

`none`, `system`, `download` — the SystemLive and Download service profiles (see
[`../architecture/direct-browser-service-profiles.md`](../architecture/direct-browser-service-profiles.md))
are distinguished as separate D2B owners.

## What is observed

- **Boot/reset:** `event=boot`. The RTC no-init region is used only as a magic-guarded
  volatile counter (`rtc_boot_counter`).
- **Network Settings origin:** `network_settings_start` and
  `network_settings_intentional_stop` are recorded as the reason for server lifecycle
  events originating from Network Settings; `download_start` /
  `download_intentional_stop` are the equivalent reasons for the Download profile.
- **Server owner:** only a successful acquire advances a non-zero `server_generation`. A
  failed acquire leaves `server_generation` unchanged; after release, the last generation
  is retained for correlation.
- **WebSocket owner:** `generation` is an independent non-zero owner key that advances per
  accepted connection, used to prevent acting on a stale socket/action; it is not a
  substitute for the server-lifecycle `server_generation`.
- **Server lifecycle:** `server_start` / `server_stop` requests and results are each
  recorded, both request and result carrying a heap snapshot.
- **WebSocket:** accepted connects and ordinary disconnects are recorded. An intentional
  server stop is recorded as `event=websocket_disconnect reason=server_stop`, distinct
  from a peer-initiated disconnect; an active stream at that point is additionally
  recorded as `stream_abrupt` with `reason=server_stop`.
- **Send failure:** the existing TX task records an `event=send_failure
  reason=send_failure` snapshot, followed by `event=stream_abrupt
  reason=send_failure`. An internal task failure records at least
  `event=stream_abrupt reason=internal_failure`.
- **HTTPD pump:** `event=pump` records `pump_schedule_accepted`,
  `pump_schedule_coalesced`, `pump_queue_rejected`, `pump_callback_begin`,
  `pump_callback_end`, and `pump_stale`. These mark only scheduling/callback boundaries,
  not a per-frame log; each reason has its own independent rate limiter of at least
  5,000,000 µs (see below).
- **Stream:** accepted start, orderly stop accepted/completed, and abrupt termination are
  recorded.
- **Resource trend:** internal heap free/minimum-ever/largest-block, and D2B
  encoder/TX-task stack high-water in bytes.
- **Queue/drop:** the existing TX task snapshots producer/output drop-counter changes; the
  acquisition/encoder hot path does not log.

## Rate limiting and correlation

An active stream's trend line is not re-emitted more often than every 5,000,000 µs
(`kMinimumRateIntervalUs`); the first sample is emitted immediately, and if the clock
moves backward the gate recovers relative to the new time. During physical testing,
correlate boot, Network Settings start/intentional-stop, server stop,
WebSocket connect/disconnect, stream start/stop, and drop/send-failure breadcrumbs in
time order using the same WebSocket `generation` and `stream_id`, cross-checking against
`server_generation` where relevant.

HTTPD pump breadcrumbs confirm scheduling state (accepted/coalesced/begin/end/rejected/
stale) only; they are not substitute evidence for queue-work retry success or actual
WebSocket frame transmission. `pump_queue_rejected` and `pump_stale` remain rate-limited
even under repeated 100 ms TX wake attempts, and this layer does not change the output
ring's depth of 32, drop-oldest policy, or sequence/timestamp/discontinuity metadata.
