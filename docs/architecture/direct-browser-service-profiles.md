# Direct-browser service profiles

SystemConfig, SystemLive, and Download are mutually separated route and security profiles. A route belonging to one profile must not become reachable merely because another profile is active.

## SystemConfig

- configuration pages and configuration APIs only;
- Viewer absent;
- D2B absent.

SystemConfig authority must not be carried into SystemLive as a write surface.

## SystemLive

- device-hosted Viewer;
- D2B index, capability, status, and measurement stream;
- no configuration write APIs;
- no `/download/*`;
- valid measurement runtime/model/session state before stream admission;
- O1-RX admission under the canonical [Origin policy](origin-admission-policy.md).

Only `/d2b/v0/stream` WebSocket admission requires Origin. Ordinary Viewer and D2B HTTP GETs do not.

## Download

- selected download only;
- SystemLive, Viewer, D2B, configuration pages, and configuration APIs absent, as already frozen.

Download selection and basename/streaming protections remain separate from direct-browser viewing.

## O3 topology

Production serves the Viewer from the device as a same-origin application. External development uses a separately bounded development-only profile and explicit Origin authority. Production admission must not be widened for development convenience.

The production transport is currently HTTP on port 80. No hostname or mDNS alias is promised by this profile.

## Runtime and product invariants

- There is one authority for runtime/model/session/WebSocket lifecycle.
- Supported Viewer modes remain Student and Professional only; there is no Presentation mode.
- D2B envelope, stream, measurement, and unit semantics are unchanged by the service-profile split.
- The browser never controls Relay state, measurement mode, or measurement range.
- `C_HYBRID` remains retained under the frozen `O1-RX + O3` direction.

Resource qualification for these profiles is governed by
[`resource-budget.md`](resource-budget.md).
