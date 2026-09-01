# Viewer / AssetPool integration

This describes how the device-hosted Viewer bundle is embedded into the AssetPool image
and served at runtime, implemented in `app/libs/viewer_asset_contract/` and
`app/assets/assets.cpp`. See
[`../product/device-hosted-viewer-contract.md`](../product/device-hosted-viewer-contract.md)
for the served Viewer's product-level behavior and
[`resource-budget.md`](resource-budget.md) for the current size/headroom facts.

## Fixed byte-exact contract

`VIEWER_ASSET_CONTRACT` (`viewer_asset_contract.h`) fixes an exact expected byte length for
each Viewer asset: `kIndexBytes` (573), `kManifestBytes` (1364), `kCssGzipBytes` (2385),
`kJsGzipBytes` (25809), for a total `kStoredPayloadBytes` of 30131 bytes, plus a
64-character bundle ID (`kBundleIdCharacters`) stored with its NUL terminator in a
65-byte field (`kBundleIdCapacity`).

## AssetPool generation: byte-exact, fail-closed copy

During AssetPool generation, `AssetPool::CreateStaticAsset()` calls
`_copy_viewer_assets()`, which reads each Viewer file from the path named by its
environment variable (`VAMETER_VIEWER_INDEX_PATH`, `VAMETER_VIEWER_MANIFEST_PATH`,
`VAMETER_VIEWER_CSS_GZIP_PATH`, `VAMETER_VIEWER_JS_GZIP_PATH`) and copies it into the
corresponding `StaticAsset_t::WebPage.viewer_*` field only if:

- the environment variable is set and non-empty;
- the path resolves to a regular file;
- the file's exact byte size matches the corresponding fixed constant above (`stat`-based
  size check, not truncated or padded);
- the read of exactly that many bytes succeeds.

Any of these checks failing logs an error and makes `_copy_viewer_assets()` return
`false`, which makes `CreateStaticAsset()` discard the in-progress asset pool and return
`nullptr` rather than writing a partial or mismatched `AssetPool-VAMeter.bin`. There is no
size-tolerant or best-effort fallback path: a Viewer bundle that does not match the
compiled-in byte-exact contract fails AssetPool generation outright.

The fixed `kViewerBundleId` bytes (not read from a file) are copied directly into
`WebPage.viewer_bundle_id`.

The raw `WebPagePool_t` storage capacities are 573 bytes for index, 1364 for manifest, 2385
for CSS gzip, 25809 for JavaScript gzip, and 65 for the bundle ID. Compile-time assertions
require each expected payload length to fit its fixed slot. Generation clears only these
Viewer slots before exact copies.

## Runtime integrity check

`IsExpectedBundleId()` compares a candidate bundle-ID buffer (exactly
`kBundleIdCapacity` bytes, NUL-terminated at `kBundleIdCharacters`) against the compiled
`kViewerBundleId`. Before any Viewer route is registered, `HasExpectedAssetIdentity()`
also SHA-256 verifies all four fixed-size stored representations against the compiled
contract. A stale ID, malformed ID, short/missing input at generation, or same-size
mutated representation fails closed with no Viewer routes. The bundle ID is the same
identifier surfaced to the browser as `viewer_bundle_id` in `/viewer/device.json` (see the
[Viewer contract](../product/device-hosted-viewer-contract.md#devicejsondisplay_name)).

## Route/content-type/cache contract

`ViewerRoutes()` returns the fixed `kViewerRouteCount` route table consumed by SystemLive:
`/`, `/viewer/`, `/viewer/asset-manifest.json`, the content-hashed CSS and JS routes, and
`/viewer/device.json`. Each route carries a fixed method, MIME type, content-encoding
(`identity` or `gzip`), and cache-control (`no-store` for HTML/manifest/`device.json`,
long-lived `immutable` for the content-hashed CSS/JS routes). This fixed table — not a
generic static-file server — is what SystemLive serves; see
[`direct-browser-service-profiles.md`](direct-browser-service-profiles.md) for how this
fits into the SystemLive/SystemConfig/Download route separation.

## Updating the Viewer bundle

Because the byte lengths and bundle ID are compiled-in constants that must match the
actual Viewer build output exactly, updating the served Viewer bundle requires
regenerating `viewer_asset_contract.h`/`.cpp` (or the equivalent generation step) from the
new Viewer build alongside AssetPool regeneration and flashing — the general AssetPool
regeneration and flashing requirements in
[`../ai/build-and-validation.md`](../ai/build-and-validation.md) apply, and any such change
requires physical-device validation before being treated as deployed.

## Current source intake candidate after `v2.0.0-beta.1`

The current source contract identifies final Viewer PR #12 bundle
`4422530b6e1ba9549dd4bef2e3bb2c183d8fced49ed2d8d695d2a04a4aa7c2af`. Its product-byte
authority is Viewer source commit `e1ebdb1cde8585a37447a66f4c8183654f4c3cda`, tree
`8f8426e9af1649f68e66e4f8f432d1b91452e38d`; a later merge commit is not substituted for
that authority. The exact bundle was byte-identical across two runs under Viewer Build
Environment V1 and has separate Viewer-side browser qualification.

This Firmware intake inherits only that exact Viewer-side qualification. It does not
establish that these bytes have been written to or served by an actual VAMeter. The
previous PR #20 `fbe7f2...` intake remains superseded historical chronology, and the
earlier `6fe499...`, `4789...`, and released beta.1 identities retain only their own
recorded evidence. None is evidence for device-hosted delivery of `4422530b...`.
Firmware and its newly generated AssetPool must be treated as a matched deployment;
mixed old/new firmware and AssetPool layouts are not supported. This source intake also
does not replace the historical physical qualification of the released `v2.0.0-beta.1`
bundle `cbcbd7eab111b49c0c6119b22a7f50ae55981933fd799abfd98d92d0dc5d96e5`.
