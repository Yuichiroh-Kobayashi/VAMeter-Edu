# SystemLive lifecycle and ownership

This describes the current server-lifecycle and D2B stream-ownership state machines behind
SystemLive, implemented in `app/libs/d2b_vi/` and `app/libs/runtime_evidence/`. See
[`direct-browser-service-profiles.md`](direct-browser-service-profiles.md) for the
route-level profile boundary and
[`../operations/d2b-runtime-diagnostics.md`](../operations/d2b-runtime-diagnostics.md) for
the diagnostic log lines these states emit.

## HTTPD server generation lifecycle

`D2B_HTTPD_LIFECYCLE::State` (`d2b_httpd_lifecycle_state.h`) is allocation-free state for
one HTTPD server generation, serialized under the existing D2B send mutex and pipeline
critical section:

- **Phase:** `Stopped`, `Running`, `PreStopping`, `StopFailed`.
- Every mutating call (`beginRunning`, `accepting`, `beginStop`, `stopFailure`,
  `stopSuccess`) is checked against a `(handleKey, generation)` pair, so a call bound to a
  stale server instance or a superseded generation cannot mutate the current state.
- `beginStop()` returns one of `BeginStopResult::First`, `Retry`, `Repeated`,
  `WrongHandle`, or `Inactive`, distinguishing a first stop request from a retried,
  already-repeated, mismatched-handle, or already-inactive stop attempt.

This state machine is what lets a stop-timeout or retried stop request be handled safely
without silently acting on a server instance that has already been superseded.

## D2B control-session lifecycle

A D2B `Session` (`d2b_session.h`) carries `ControlState state`, `bool ownsStream`, and
`std::uint32_t streamId`. `ControlState` moves through `Connected` → `Ready` →
`Streaming` → `Closed`. `OpenSession()` and `CloseSession()` bound a session's lifetime;
`HandleClientMessageInto()` processes `Hello` / `StartStream` / `StopStream` / `Ping`
client messages against the current session state, and `ValidateClientMessageState()`
checks a requested transition against both the current `ControlState` and whether this
session currently owns the stream (`ownsStream`).

`DecideStreamStartDisposition()` (`d2b_stream_start_disposition.h`) maps whether the
per-client pipeline allocation succeeded to `ContinueConnection` or `CloseConnection` —
a failed pipeline allocation at stream start closes the connection rather than leaving it
in an ambiguous half-started state.

## One-active-D2B-stream-owner contract

Only one session's stream-start request can succeed in acquiring stream ownership at a
time; `ErrorCode::Busy` is the defined response for a control-message state conflict of
this kind. This is the current one-active-owner D2B safety contract referenced by
[`direct-browser-service-profiles.md`](direct-browser-service-profiles.md) and by the
Viewer contract's
[one-active-D2B-stream-owner section](../product/device-hosted-viewer-contract.md#one-active-d2b-stream-owner-safety-contract).
Multi-client policy beyond this single-owner contract is under investigation in
[VAMeter-Edu Issue #8](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/8) and
must preserve this contract while unresolved.

## Two owner identities: server generation vs. WebSocket generation

Runtime diagnostics (see
[`../operations/d2b-runtime-diagnostics.md`](../operations/d2b-runtime-diagnostics.md))
distinguish two separate generation counters that must not be conflated:

- **`server_generation`** advances only on a successful HTTPD server-owner acquire and is
  retained across release for correlation. Its `owner` token is one of `none`, `system`,
  or `download` — the SystemLive and Download service profiles are tracked as distinct
  HTTPD owners, consistent with their mutual exclusion in
  [`direct-browser-service-profiles.md`](direct-browser-service-profiles.md).
- **`generation`** (the WebSocket/stream owner key) advances independently, once per
  accepted D2B WebSocket connection, and exists specifically to prevent a stale
  socket/action from being acted on after a newer connection has taken over.

## Stop-timeout and retry safety

Because `D2B_HTTPD_LIFECYCLE::State` keys every mutation to an exact `(handleKey,
generation)` pair, and because the WebSocket `generation` counter is independent of the
server's `server_generation`, a stop timeout or a retried stop/start request cannot
silently apply to the wrong server instance or the wrong stream owner. This underlies the
general trigger/resource-ownership safety property required across recorder and D2B
lifecycles by
[`../ai/recorder-and-measurement-contracts.md`](../ai/recorder-and-measurement-contracts.md).
