# Public Status R1 Viewer targeted physical validation — 2026-08-22

## Authority

- Repository: `Yuichiroh-Kobayashi/VAMeter-Edu`, PR #14, branch
  `build/viewer-public-status-r1-intake`.
- Firmware source physically tested: `8ac66922735b9634f7a440dbf0ae1dff0784789f`.
- Firmware base: `917d94619f9d14fc72f44fdb87ab56629aea5160`.
- Viewer source: `84136a22ed6ea00f428f8c1c430dc76ec615caf4`.
- D2B authority: `b30ad676922af73448952d5a9cac312467a944f9`.
- Candidate Viewer bundle: `6fe4991f3dcea5793b4b19736e4ab9c3ca39869c59e789776abae5a5d84733ca`.
- Candidate application: 1,772,416 bytes,
  SHA-256 `0f3ad99cb3877bd4a3b508403250337a5dfb3787b2f6aa1bf27e5025f319dc48`.
- Candidate AssetPool: 1,651,116 bytes,
  SHA-256 `365e62d58072cde8875b7783230d9f6cef195965f185d4652ac6b1259504dd56`.
- Physical evidence root:
  `/home/yu-ichirou/LabData/VAMeter-Edu/20260822-190526-pr14-public-status-r1-physical/`.

The evidence root contains 35 files. Its `SHA256SUMS` has 34 entries and was
revalidated from the root with `sha256sum -c SHA256SUMS`; every entry returned
`OK`. The root and its contents were not modified for this record.

## Scope and claim boundary

This record covers a targeted integration session after the PR #14 candidate
application and AssetPool were written to the device, independently read back,
manually exercised, and rolled back. It does not replace the current
[browser physical qualification procedure](../../../validation/browser-physical-qualification.md).

The correct decision boundary is:

- PR #14 targeted physical integration: **PASS WITH EVIDENCE GAPS**.
- Full browser physical qualification: **NOT COMPLETE**.
- Release qualification: **NOT CLAIMED**.

This record does not claim that all full-qualification T1–T8 subcriteria were
completed. Raw logs and binary captures remain in the immutable evidence root;
they are not duplicated into Git.

## Physical prestate

Before candidate installation, the complete 2 MiB `ota_1` and `assetpool`
partitions were read back, together with `otadata` and the partition table.
The active OTA entry selected `ota_1`. The `ota_1` payload matched the known-good
`v2.0.0-beta.1` application (`9b872ea5cc483b361bba9550e1878b9036d205ee830fd84a0e321d3f3a732423`),
and the AssetPool payload matched the known-good `v2.0.0-beta.1` AssetPool
(`1df4b81fba8b3f16baf1331f015cdb1fdc7214d66a215657ef752673b43c1c41`, bundle
`cbcbd7eab111b49c0c6119b22a7f50ae55981933fd799abfd98d92d0dc5d96e5`).
Full partition tails were verified as erased `0xFF`, and the readbacks provided
exact-prestate rollback coverage for the candidate write union.

The separate `ota_0` readback did not match either known reference. Its content
remains **UNKNOWN** and was not investigated further; this was outside the
candidate write and rollback scope.

## Candidate installation and readback

With explicit operator authorization, Windows-native `esptool.exe` v5.1.0 wrote
only `ota_1` at `0x00210000` and `assetpool` at `0x00510000`. `otadata`, the
boot slot, `ota_0`, and other partitions were not changed. Independent
post-write readback matched the candidate application and AssetPool hashes above;
the unused tails were all `0xFF`. The boot log identified
`v2.0.0-beta.1-5-g8ac6692`, ESP-IDF v5.1.6, and the candidate ELF hash prefix.

The evidence also records a transport/port finding. Large WSL/usbipd-forwarded
readback attempts failed or stalled, while Windows-native `esptool.exe` against
the live COM10 completed the full large readbacks. The same evidence root records
a transient usbipd COM27 display versus live Windows COM10 discrepancy during
identity checks. Future physical work must freshly revalidate the stable device
identity and port; no port name is carried forward as permanent authority.

## Targeted results

| Case | Result and direct evidence |
|---|---|
| T1 boot/candidate identity | **PASS** — normal boot from `ota_1`; candidate app identity and ELF prefix matched; AssetPool injection completed; no crash, watchdog, reset, or allocation fault was observed. |
| T2 SystemLive route isolation | **PASS** — Viewer, capabilities, and public status routes returned; `/syscfg` returned 404 while SystemLive was active. |
| T3 served Viewer/bundle identity | **PASS** — `viewer_bundle_id`, Viewer source commit, D2B authority commit, index/manifest/CSS/JS sizes and hashes matched the candidate authority. |
| T4 Public Status idle/bootstrap | **PASS** — idle `/d2b/v0/status` returned the expected R1 fields with zero clients, zero drops, and zero queued samples; the device-hosted Viewer path operated. The oracle validator was not run in this session. |
| T5 Edge manual device-hosted Viewer smoke | **PASS, operational/manual scope** — 58 seconds of streaming, waveform update confirmed by the operator, clean Stop/idle return, and no drops or stale owner. Oracle-backed binary capture and screenshot were **NOT PERFORMED**. |
| T6 iPad Safari manual device-hosted Viewer smoke | **PASS, operational/manual scope** — Start, waveform, Stop, idle return, and `display_name: "Both"` were confirmed after the expected single-owner contention was cleared. Screenshot and DevTools evidence were **NOT PERFORMED**. |
| T7 abrupt disconnect/reconnect stale-owner recovery | **PASS, operational/manual scope** — abrupt tab close returned to idle, a subsequent Start acquired ownership, and orderly Stop returned to idle. Oracle-backed reconnect capture was **NOT PERFORMED** and distinct `stream_id` was **NOT VERIFIED**. |
| T8 rollback | **PASS** — the known-good matching beta.1 application and AssetPool pair was restored; independent readback matched the prestate byte-for-byte and live `device.json` served the known-good bundle ID `cbcbd7ea...`. |

T5 and T7 therefore record manual operational PASS separately from their missing
oracle-capture subcriteria. They are not full oracle-validated capture PASS claims.

## Evidence gaps and not performed

- Oracle-backed binary capture for T5: **NOT PERFORMED**.
- Oracle-backed reconnect capture for T7: **NOT PERFORMED**.
- Distinct `stream_id` verification in T7: **NOT VERIFIED**.
- Browser screenshots and DevTools evidence: **NOT PERFORMED**.
- Intentional gap/backpressure case: **NOT PERFORMED**.
- 30-minute per-browser soak: **NOT PERFORMED**.
- Three-point measurement accuracy comparison: **NOT PERFORMED**.
- Recorder five-second physical regression: **NOT PERFORMED**.
- `ota_0` unknown-content investigation: **NOT PERFORMED / OUT OF SCOPE**.

## Rollback and final state

After the forward targeted tests, the exact prestate captures were written back
to `ota_1` and `assetpool` with explicit authorization. Independent readback was
byte-identical to the pre-candidate captures, and the device served the exact
known-good beta.1 Viewer bundle ID. The final device state is therefore:

- known-good `v2.0.0-beta.1` application + known-good `v2.0.0-beta.1` AssetPool;
- PR #14 candidate **NOT currently installed**.

## Decision

PR #14's targeted physical integration evidence is **PASS WITH EVIDENCE GAPS**.
The candidate's exact tested runtime authority remains firmware commit
`8ac66922735b9634f7a440dbf0ae1dff0784789f`; the later docs-only closeout does not
change those runtime bytes. Full browser qualification and release qualification
remain unclaimed.
