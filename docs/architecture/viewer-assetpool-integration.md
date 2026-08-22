# Viewer / AssetPool integration

This describes how the device-hosted Viewer bundle is embedded into the AssetPool image
and served at runtime, implemented in `app/libs/viewer_asset_contract/` and
`app/assets/assets.cpp`. See
[`../product/device-hosted-viewer-contract.md`](../product/device-hosted-viewer-contract.md)
for the served Viewer's product-level behavior and
[`resource-budget.md`](resource-budget.md) for the current size/headroom facts.

## Fixed byte-exact contract

`VIEWER_ASSET_CONTRACT` (`viewer_asset_contract.h`) fixes an exact expected byte length for
each Viewer asset: `kIndexBytes` (573), `kManifestBytes` (1363), `kCssGzipBytes` (761),
`kJsGzipBytes` (22578), for a total `kStoredPayloadBytes` of 25275 bytes, plus a
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

## Runtime integrity check

`IsExpectedBundleId()` compares a candidate bundle-ID buffer (exactly
`kBundleIdCapacity` bytes, NUL-terminated at `kBundleIdCharacters`) against the compiled
`kViewerBundleId` before the stored Viewer bundle is trusted at runtime. This is the same
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
