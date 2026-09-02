# Device-hosted Viewer contract

This describes only current, implemented behavior of the VAMeter-Edu device-hosted
browser Viewer, released in stable
[`v2.0.0`](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/releases/tag/v2.0.0)
on 2026-09-02. See
[`../architecture/systemlive-lifecycle-and-ownership.md`](../architecture/systemlive-lifecycle-and-ownership.md)
for the underlying route/session architecture.

The stable release authority is:

| Release identity | Value |
|---|---|
| Firmware release commit | `ee4da1b5e5e238fbc66a9d9a49f4d051c1ca986b` |
| Viewer release source | `e1ebdb1cde8585a37447a66f4c8183654f4c3cda` |
| Viewer release tree | `8f8426e9af1649f68e66e4f8f432d1b91452e38d` |
| Viewer bundle | `4422530b6e1ba9549dd4bef2e3bb2c183d8fced49ed2d8d695d2a04a4aa7c2af` |

Before the stable release, the Viewer Public Status Standard R1 work had an
earlier targeted physical-validation stage. Its exact tested Firmware source was
`8ac66922735b9634f7a440dbf0ae1dff0784789f`, its exact Viewer source was
`84136a22ed6ea00f428f8c1c430dc76ec615caf4`, and its bundle ID was
`6fe4991f3dcea5793b4b19736e4ab9c3ca39869c59e789776abae5a5d84733ca`.
That candidate application and AssetPool were written and independently read back with
exact identity, then targeted device-hosted integration was manually passed on Windows
Edge and iPad Safari, including start/stop behavior, reconnect cleanup, and rollback.
That historical result was **PASS WITH EVIDENCE GAPS**, not full browser qualification:
oracle-backed
browser capture, screenshots/DevTools evidence, long soak, gap/backpressure, and other
full-qualification cases were not performed. The session ended after restoring the
then-known-good `v2.0.0-beta.1` application + AssetPool pair, so that PR #14 candidate
was not deployed at the end of that validation. This is historical chronology, not the
current product authority; the stable release authority above now identifies the
device-served product. See the
[targeted physical validation record](../archive/validation/d2b/2026-08-22-public-status-r1-viewer-targeted-physical-validation.md).

## What it is

VAMeter now hosts the Viewer itself: the device serves the Viewer's HTML/CSS/JS bundle and
the D2B live-measurement stream from the same origin, so a browser on the classroom Wi-Fi
network can open the device's address and see live voltage/current without any app
install, cloud account, or internet access. The Viewer is a separate frontend codebase
(`Yuichiroh-Kobayashi/Device-to-Browser-Viewer`); this repository serves the built,
integrity-checked bundle and provides the D2B stream and `device.json` it consumes.

## SystemLive

SystemLive is the route/security profile that serves the Viewer, the D2B stream, and
related read-only D2B endpoints together, kept separate from device configuration
(SystemConfig) and CSV download (Download). See
[`../architecture/direct-browser-service-profiles.md`](../architecture/direct-browser-service-profiles.md)
for the full profile boundary.

## Student and Professional modes

The Viewer supports two display modes: **Student** and **Professional**. There is no
third "Presentation" mode. Mode selection is a Viewer (browser-side) concern; the device
does not gate stream access by mode.

## Voltage / Current / Both

Before SystemLive starts, AppWaveform on the device selects one bounded display profile —
`mode_volt_only`, `mode_current_only`, or `mode_both` — which maps exactly to `Voltage`,
`Current`, or `Both` in `/viewer/device.json`. The selected value is immutable for that
active SystemLive lifecycle: it is fixed when SystemLive starts and does not change until
SystemLive is restarted with a new selection.

- **Student** mode filters the graph to the channel(s) implied by the active display
  profile.
- **Professional** mode shows both voltage and current graphs regardless of display
  profile, when the underlying stream data is available.

## `device.json.display_name`

`GET /viewer/device.json` returns a fixed JSON object built from
`VIEWER_ASSET_CONTRACT::BuildDeviceJson()`:

```json
{"schema_version":1,"viewer_bundle_id":"<sha>","d2b_protocol":"d2b-stream/0.1","d2b_stream":"live-vi","display_name":"Voltage"}
```

`display_name` is one of `"Voltage"`, `"Current"`, or `"Both"`, matching the active
display profile above. `schema_version` is currently always `1`.

## Fail-closed missing/unknown profile

`DisplayProfileForWaveformModeCode()` maps only the three known waveform mode codes to
`Voltage`/`Current`/`Both`; any other code maps to `DisplayProfile::Invalid`. An `Invalid`
profile has no display name (`DisplayName()` returns `nullptr`), and
`BuildDeviceJson()`/`DisplayProfileSession::begin()` both fail rather than substituting a
default. An unknown or missing display profile fails closed — it does not silently become
`Both` or any other default.

## One-active-D2B-stream-owner safety contract

There is one authority for D2B runtime/session/WebSocket lifecycle. The existing
one-active-owner D2B safety contract is unchanged by SystemLive: only one D2B stream owner
is active at a time. A second client observing/joining an active D2B stream while another
client already owns it is not yet a defined product policy; multi-client behavior is under
investigation in
[VAMeter-Edu Issue #8](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/8), which
must preserve this existing one-active-owner safety contract while it is resolved.

## The browser does not control relay/mode/range

The browser Viewer is a read-only measurement client. It never controls Relay state,
measurement mode, or measurement range; those remain device-side decisions made through
the device's own UI. Origin admission on `/d2b/v0/stream` is a browser-origin admission
control, not a command channel — see
[`../architecture/origin-admission-policy.md`](../architecture/origin-admission-policy.md).

## Production excludes synthetic/capture/replay/arbitrary endpoints

SystemLive's route set is the fixed set of Viewer/D2B routes defined by
`VIEWER_ASSET_CONTRACT::ViewerRoutes()` plus the exact D2B endpoints
(`/d2b/v0/`, `/d2b/v0/capabilities`, `/d2b/v0/status`, `/d2b/v0/stream`). Production admits
no synthetic-measurement injection endpoint, no capture-replay endpoint, and no arbitrary
path beyond this fixed set; WebSocket admission on `/d2b/v0/stream` accepts only the exact
canonical upgrade request described in
[`../architecture/origin-admission-policy.md`](../architecture/origin-admission-policy.md).
The same-origin browser capture helper under `tests/d2b_vi_integration/` is a development
and validation tool that connects to a real device stream; it is not a production endpoint.

## Stable structural controls / incremental updates

The Viewer bundle (HTML, CSS, JS, and the asset manifest) is served with content-hash
routes and immutable cache headers, and its integrity is checked against a fixed bundle
ID (`IsExpectedBundleId()`) before being trusted. Viewer UI controls (mode, channel
selection, display window) are structural, browser-side state; the device does not push
UI layout changes. Live measurement data arrives incrementally over the D2B WebSocket
stream rather than by polling or reloading the page.

## 10 / 30 / 60 second display windows

The stable [`v2.0.0` release](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/releases/tag/v2.0.0)
supports 10, 30, and 60 second display windows. Their original beta.1 introduction is
also preserved in the historical [CHANGELOG](../../CHANGELOG.md) entry. Window selection is a
Viewer-side (browser) presentation concern over the same live D2B stream; it does not
change what the device measures or streams.

## Roadmap / Related Issues

- Simplifying Student mode to a single Start/Stop control is tracked in
  [Device-to-Browser-Viewer Issue #1](https://github.com/Yuichiroh-Kobayashi/Device-to-Browser-Viewer/issues/1)
  (external repository). It is not implemented; do not describe a one-button Student
  control as current behavior.
- Multi-client D2B stream policy is tracked in
  [VAMeter-Edu Issue #8](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/8).
- Analog-meter answer-check display correction is tracked in
  [VAMeter-Edu Issue #9](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/9); see
  [`../standards/measurement-and-presentation-semantics.md`](../standards/measurement-and-presentation-semantics.md#analog-pointer-matching-current-boundary)
  for the current boundary.
