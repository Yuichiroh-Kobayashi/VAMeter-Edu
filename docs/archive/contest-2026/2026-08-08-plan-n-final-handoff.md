# VAMeter-Edu Contest Plan N final firmware handoff

## Document status

- Contest: M5Stack Global Innovation Contest 2026
- Feature: VAMeter-Edu Live / Plan N
- Document type: dated handoff / validation evidence / source provenance
- Finalization date: 2026-08-08
- Firmware branch: `contest/plan-n-network-after-waveform`
- Base commit: `e45588da6183f3b8b813a7b0fc1005af58c084b1`
- Exact validated firmware source commit: `913d9011cfd07e74cbc10b61ddc766bf45856f56`
- Production commit message: `feat: add contest live viewer path after waveform`

This handoff is historical evidence, not a standalone normative contract. Current
behavior remains owned by source and tests; physical claims below are owned by the
preserved final validation report.

## Decision

```text
PLAN_N_FINAL_VALIDATION_PASS
PLAN_N_PHYSICAL_PASS
P2_NAVIGATION_RELAY_PASS
P3_NETWORK_D2B_PASS
PLAN_N_WINDOWS_E2E_PASS
LIVE_PHYSICAL_DEMO_PASS
P8_IPAD_LIVE_PHYSICAL_PASS
IPAD_VIEWER_VIA_WINDOWS_PASS
CONTEST_EVIDENCE_CAPTURED
```

The exact validated tracked source tree was committed without changing the bytes of
the three approved production files. The pre-commit working-tree diff and commit diff
were byte-for-byte identical, and all three pre/post file SHA-256 values matched.

```text
VALIDATED_TREE_COMMIT_EQUIVALENCE_PASS
```

## Firmware provenance

Validated physical binary:

- SHA-256: `ddedaafa25a1b2c0a40f28be46181db0d3e5a96dc4934c92777b9e29cbc61743`
- embedded version: `v1.1.1-22-ge45588d-dirty`
- ESP-IDF: `v5.1.6`

The validated BIN was built before the final clean production commit. Its embedded
`git describe` value therefore correctly retains `-dirty`. The validated binary was
built from the exact tracked source tree later committed as
`913d9011cfd07e74cbc10b61ddc766bf45856f56`; the source commit hash and embedded
version are intentionally different identities.

Approved production diff:

```text
app/apps/app_settings/app_settings.h
app/apps/app_waveform/app_waveform.cpp
app/apps/app_waveform/app_waveform.h
3 files changed, 6 insertions(+), 9 deletions(-)
```

Pre-commit and post-commit production-file SHA-256:

```text
bb7533593fae40959eb883caf5d9f576d5632bcfaa6a72735dbc760f27875d51  app/apps/app_settings/app_settings.h
2cc87e04a426b67d5a46918830fee86d28eb66c438e1ecd61bc792263f12ec56  app/apps/app_waveform/app_waveform.cpp
1ff40bbcdfb94aa7c856d881297437fed7159c66db840b00d7590a5788564929  app/apps/app_waveform/app_waveform.h
```

Saved validated diff SHA-256:

```text
7acccc6da871903ecdba692401b351a21d87c195c4d576d4f831f93f06179539  validated-plan-n.diff
7acccc6da871903ecdba692401b351a21d87c195c4d576d4f831f93f06179539  validated-plan-n-binary.diff
```

## Physical evidence authority

Final validation run:

```text
/home/yu-ichirou/LabData/VAMeter-Edu/20260808-094842-plan-n-p2-ipad-final/
```

Authoritative report:

```text
analysis/FINAL_REPORT.md
SHA-256 26bb4eb882d7c5a48b8ab6155f830d438a8e1630397943a1469fa60b5d145214
```

The build artifact and preserved post-flash read-back both rehashed to the validated
BIN SHA above. The preserved image-info reports the embedded version and ESP-IDF
identity above.

Rollback known-good identity:

```text
SHA-256 5d8a32bb9a22e88d654ec063fab1f1179e677850c46421c76a490fba3c7ec14b
embedded v1.1.1-22-ge45588d
commit e45588da6183f3b8b813a7b0fc1005af58c084b1
```

The former `883b0dfaddba791d4707b9b9379dd2eef691c0370fcef2b10a83feeb66f64883`
identity is `STALE_HISTORICAL_IDENTITY` and must not be used for rollback. Existing
historical manifests and evidence were not rewritten during finalization.

## External frozen baselines

- D2B: `5411ba59a12882345d32218eda367bd6ba35ef5d`
  - local detached checkout clean;
  - observed on remote `main` and remote `HEAD` during finalization.
- Viewer: `80a9cd308cb3c6c5a1ccc27241cd645803675921`
  - local detached checkout clean;
  - observed on remote `fix/live-physical-review-gate` during finalization.
- Relay `relay.py` SHA-256:
  `028598690228d5e84e17b8346c809b02867ee9b35cc72a32f4dbc63097a30679`
- Relay test source `tests/test_relay.py` SHA-256:
  `e144cf80b48152810baf038da6ffb7b3885affa2ca8ced4a7eb5740efb2cd062`
- Relay `requirements.txt` SHA-256:
  `d833cdc4cbb985a30cd39bfce04be3e444d8fbf0ea6c59f0badbabab81821099`
- Relay `README.md` SHA-256:
  `ca1bce216eeefc2615c4436dfd3c42845b348f1255a4841225817aeef0776e75`
- Relay `INTEGRATION_STATUS.md` SHA-256:
  `4c1e037bf9e76ad4a16d5d9814483e51080b9fc2bec07bc698c968127d96110b`
- Relay source publication: `PENDING`

No D2B, Viewer, or Relay source was changed, committed, or pushed by firmware
finalization. No Relay repository was created.

## Validated platforms and evidence

### Windows

- OS: Microsoft Windows 11 Pro, version `10.0.26100`, build `26100`
- Browser: Google Chrome `151.0.7922.108` 64-bit
- Result: `PLAN_N_WINDOWS_E2E_PASS` / `LIVE_PHYSICAL_DEMO_PASS`
- Binary frames: `5996`
- Samples: `5995`
- Physical Voltage: `0 V` to `2.4325 V`
- Device producer/output drops: `0 / 0`
- Relay error/overflow/drop/backpressure-timeout: `0 / 0 / 0 / 0`

### iPad

- Model: iPad Pro 11-inch (M1)
- OS: iPadOS `26.5`
- Browser: Safari
- READY: PASS
- STREAMING: PASS
- Physical Voltage: approximately `2.60-2.90 V`
- Result: `P8_IPAD_LIVE_PHYSICAL_PASS` / `IPAD_VIEWER_VIA_WINDOWS_PASS`

The iPad result used the temporary Windows bridge. It is not a claim of direct
iPad-to-device communication.

### Chromebook additional confirmation

- Model: Lenovo `CT-X636F`
- Platform/version: `krane 150.16700.0`
- Browser: Chrome `150.0.7871.222`
- Viewer display/live operation: additionally confirmed good through the bridge

This Chromebook observation is supplemental confirmation only. It is not promoted to
the formal qualification level used for the iPad result.

## Contest demo

- Final edited demo: `MOV_4943.mp4`
- Duration: `53.0986 s`
- SHA-256: `be749ff249685dcd3021083ecc55d5f8a916cb4030c1bb3cfcc5097bee7f1a08`
- Result: `CONTEST_EVIDENCE_CAPTURED`

## Limitations and non-claims

- CSV sanity: `P6_CSV_SANITY_FOLLOWUP_REQUIRED`; optional and non-blocking. HTTP 200
  began, but the Windows curl output file was not created, so existence and header
  were not verified.
- Serial forbidden signatures: not assessable because continuous serial runtime
  evidence was unavailable. No firmware-origin reset was observed during the passed
  mandatory gates.
- The Windows bridge was temporary and was cleaned up after validation.
- No direct iPad/Chromebook-to-device claim is made.
- No production long-duration or soak qualification is claimed.
- No HTTPD 8192 production stack-margin or high-water claim is made.
- No general zero-packet-loss or uniform 25 Hz claim is made.
- The Relay remains a Contest test-only component with publication pending.

## Reproduction

### A. Exact physical evidence reproduction

Use the preserved validated binary whose SHA-256 is
`ddedaafa25a1b2c0a40f28be46181db0d3e5a96dc4934c92777b9e29cbc61743`.
This is the immutable binary bound to the final physical evidence. Verify its hash
before use and follow a separately approved device-operation/flash procedure.

### B. Source reproduction

```bash
git checkout 913d9011cfd07e74cbc10b61ddc766bf45856f56
```

Acquire and verify dependencies according to repository policy, activate ESP-IDF
`v5.1.6`, and build from that commit. A source rebuild can differ byte-for-byte from
the validated BIN because the clean commit changes Git metadata and its embedded
version string. Source-equivalent and binary-identical are distinct claims. Any such
binary is `POST_COMMIT_BUILD_NOT_PHYSICALLY_VALIDATED` until separately tested.

## Finalization mutation boundary

- Production source after the validated diff: no mutation
- Rebuild: not run
- Flash/device operation: not run
- D2B/Viewer/Relay mutation: none
- Pull request: not created
- Merge: not performed
- Tag/release publication: not performed
