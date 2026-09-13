# Viewer / AssetPool integration

This describes how the device-hosted Viewer bundle is embedded into the AssetPool image
and served at runtime, implemented in `app/libs/viewer_asset_contract/` and
`app/assets/assets.cpp`. See
[`../product/device-hosted-viewer-contract.md`](../product/device-hosted-viewer-contract.md)
for the served Viewer's product-level behavior and
[`resource-budget.md`](resource-budget.md) for the recorded size/headroom claim
boundaries.

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
require each expected payload length to fit its fixed slot. The PR-A development generator value-initializes the complete object before applying
its default member initializers, then explicitly clears these Viewer slots before exact copies.

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

## PR-A development layout/container guard

PR-A implements the guard foundation from [the accepted intake design](../development/node-viewer-intake.md).
It keeps the exact stable `StaticAsset_t` layout, Viewer slots, bundle ID, hashes, and routes.
This is development behavior, **not a release or physical qualification**. PR-B performs the
later Final Viewer intake and layout version change.

The unchanged `StaticAsset_t` declaration is shared through `app/assets/static_asset_types.h`.
`app/libs/asset_pool_layout/` uses that declaration for C++11 `sizeof`/`offsetof` authority.
It allocates no heap for validation, adds no `IRAM_ATTR`, and uses no CRC lookup table.

### Full-partition byte format

| Region | Offset | Bytes | Content |
| --- | ---: | ---: | --- |
| StaticAsset_t used | 0 | 1,655,972 | Current struct object representation |
| Layout growth reserve | 1,655,972 | 440,924 | All zero |
| Trailer reserve | 2,096,896 | 256 | Header below, followed by zero reserved bytes |
| Full container | 0 | 2,097,152 | Exact partition-sized file |

The stable release's historical image size `1,655,972` and partition free `441,180`
remain unchanged historical facts. The new full container uses the separate terms above.

All integers in the following trailer are unsigned, explicitly encoded little-endian.
The on-flash trailer is never serialized by casting a C++ struct.

| Relative offset | Bytes | Field / PR-A value |
| --- | ---: | --- |
| 0 | 8 | ASCII `VAMEAPL1` |
| 8 | 2 | Trailer format version `1` |
| 10 | 2 | Header size `76` |
| 12 | 2 | StaticAsset layout version `1` |
| 14 | 2 | Viewer sub-layout version `1` |
| 16 | 4 | `sizeof(StaticAsset_t)` = `1655972` |
| 20 | 4 | `offsetof(StaticAsset_t, WebPage)` = `1540471` |
| 24 | 4 | Viewer member count `5` |
| 28 / 32 | 4 / 4 | Index offset / capacity: `1625773 / 573` |
| 36 / 40 | 4 / 4 | Manifest offset / capacity: `1626346 / 1364` |
| 44 / 48 | 4 / 4 | CSS gzip offset / capacity: `1627710 / 2385` |
| 52 / 56 | 4 / 4 | JS gzip offset / capacity: `1630095 / 25809` |
| 60 / 64 | 4 / 4 | Bundle ID offset / capacity: `1655904 / 65` |
| 68 | 4 | Static asset CRC32 |
| 72 | 4 | Trailer CRC32 |
| 76 | 180 | Reserved, all zero |

Member offsets in production are calculated from the two `offsetof` expressions, not
these historical numbers. Compile-time assertions cover standard layout, trivial byte
copying, the version-1 sizes, slot equality, bounds, and partition fit. Runtime validation
still checks every declaration for exact agreement, safe ranges, increasing non-overlap,
and zero reserved bytes. A malformed range can be rejected earlier by exact offset or
capacity comparison.

Both CRCs use table-less CRC-32/IEEE 802.3: reflected polynomial `0xEDB88320`, initial
value `0xFFFFFFFF`, final xor `0xFFFFFFFF`. The static asset CRC covers exactly the
Firmware compile-known `sizeof(StaticAsset_t)` bytes at offset zero, including padding.
The declared size must match before that read; it never controls the CRC read length.
The trailer CRC covers bytes `[0,72)` only. Reserved bytes `[76,256)` are checked separately.
CRC detects corruption/completeness errors; it is neither authentication nor Viewer
identity authority. The existing bundle ID and four SHA-256 constants retain that role.

### Generation and desktop loading

`CreateStaticAsset()` now uses `new StaticAsset_t()`; the implicit default constructor
preserves the Color/Text default member initializers after zero-initialization. It does
not clear a constructed object wholesale. Independent generation tests use different
allocator poison patterns and compare the complete 2 MiB images; language reasoning alone
is not the determinism acceptance criterion.

The writer retains `<final>.tmp -> write -> flush -> close -> rename`. It emits the full
zero growth reserve and fixed-tail trailer, checks the final length, and removes failed
temporary output on stream/close/rename errors. A partial container is never published.

`GetStaticAssetFromBin()` requires a readable regular file of exactly 2 MiB, reads and
validates the fixed-tail trailer before allocating/loading the struct, and verifies the
static CRC before returning it. It returns `nullptr` and an `ASSETPOOL_CONTAINER_*`
reason marker on rejection. Its optional `missing` output distinguishes absence from an
invalid existing container. POSIX symlinks, including dangling symlinks, are rejected.
Desktop setup generates only when the file is absent; an
invalid existing file stops startup and is preserved.

### Tier 1 startup stop and Tier 2 Viewer rejection

Device startup finds the partition, requires exactly 2 MiB, maps that range, copies the
fixed 256-byte trailer, validates the complete shape, and scans the compile-known static
bytes in place. Only success reaches `InjectStaticAsset()`, whose font/text use therefore
occurs after validation. A rejected mapping is unmapped.

The `SetupCallback_t::AssetPoolInjection` callback and `APP::Setup()` now return `bool`.
Both device and desktop call sites handle failure. A missing/false callback returns before
HAL, locale, Mooncake, or ordinary UI initialization. Device logs
`ASSETPOOL_LAYOUT_REJECTED reason=...` and `ASSETPOOL_TIER1_FAIL_STOP`, then returns from
`app_main`. ESP-IDF v5.1.6's `components/freertos/app_startup.c` deletes its main task on
return. The configured automatic task watchdog initialization is disabled; HAL registers
the watchdog only after successful AssetPool injection. This stop path uses no AssetPool
error UI, busy loop, reboot, format, record deletion, or automatic AssetPool rewrite.
Physical boot timing and watchdog behavior remain **NOT RUN** in PR-A.

A container with correct layout/CRC but incorrect Viewer identity still reaches the
existing Tier 2 `VIEWER_ASSETPOOL_IDENTITY_MISMATCH` check. Viewer/SystemLive routes fail
closed while core UI, measurement, and recorder remain available. PR-A does not move
Viewer identity validation into Tier 1 or change `viewer_http_routes.cpp`.

### Matched-pair deployment and validation boundary

| Firmware / AssetPool pair | Source/host expectation |
| --- | --- |
| New PR-A / new PR-A with stable Viewer | Layout gate passes; unchanged stable Viewer identity checks apply |
| New PR-A / old stable pool | Tier 1 fail-stop; desktop rejects the short file, device rejects the legacy tail |
| Old stable / new PR-A | Struct and Viewer bytes remain compatible; old Firmware ignores the tail |

The third row is source/host compatibility evidence, not physical proof. **PR-A Firmware
requires a regenerated trailer-bearing PR-A AssetPool. Firmware-only flash while retaining
the old stable AssetPool is unsupported and intentionally reaches Tier 1 fail-stop.**
PR-A is not released alone: PR-A review/merge -> PR-B review/merge -> one combined physical
gate. No physical write/readback/reset/qualification is part of this implementation task.

`tests/asset_pool_layout/` covers CRC vectors, negative metadata, mixed layout, guard-page
read bounds, real `APP::Setup()` failure propagation, independent full-container generation,
regular/size/CRC loader rejection, and atomic write/rename failure. The pure library test
uses C++11 with warnings as errors; real desktop integration tests inherit the existing
LovyanGFX C++17 compile feature. All root host tests and the ESP-IDF v5.1.6 build/resource
comparison remain required. Static measurements do not establish the approximately
1.66 MiB boot CRC's actual device timing, runtime heap, or stack high-water.

## Stable `v2.0.0` Viewer intake

Stable [VAMeter-Edu `v2.0.0`](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/releases/tag/v2.0.0),
published on 2026-09-02 from Firmware commit
`ee4da1b5e5e238fbc66a9d9a49f4d051c1ca986b`, embeds final Viewer PR #12 bundle
`4422530b6e1ba9549dd4bef2e3bb2c183d8fced49ed2d8d695d2a04a4aa7c2af`. Its product-byte
authority is Viewer source commit `e1ebdb1cde8585a37447a66f4c8183654f4c3cda`, tree
`8f8426e9af1649f68e66e4f8f432d1b91452e38d`; a later merge commit is not substituted for
that authority. The exact bundle was byte-identical across two runs under Viewer Build
Environment V1 and has separate Viewer-side browser qualification. Later Viewer source
changes do not alter this published bundle; they require another reviewed Firmware
Viewer/AssetPool intake and a later VAMeter-Edu release to become device-served.

Before release publication, this Firmware intake inherited only that exact Viewer-side
qualification and did not by itself establish that the bytes had been written to or
served by an actual VAMeter. That limitation is retained as pre-release chronology; the
stable release above is now the current product authority. The
previous PR #20 `fbe7f2...` intake remains superseded historical chronology, and the
earlier `6fe499...`, `4789...`, and released beta.1 identities retain only their own
recorded evidence; none substitutes for the stable release record.
Firmware and its newly generated AssetPool must be treated as a matched deployment;
mixed old/new firmware and AssetPool layouts are not supported. This source intake also
does not replace the historical physical qualification of the released `v2.0.0-beta.1`
bundle `cbcbd7eab111b49c0c6119b22a7f50ae55981933fd799abfd98d92d0dc5d96e5`.
