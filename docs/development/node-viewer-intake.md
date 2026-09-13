# Node適用範囲とViewer受入れ

## 目的

Related to #23。VAMeterのFirmware buildへNode依存を追加せず、別repoで生成するViewerの
受入れ条件を明確にする。Firmware/DesktopはC++とPythonの経路で、VAMeter側build/testへ
Node/npm依存を追加しない。ViewerのJavaScriptがassetであることと、Firmware buildにNodeが
必要なことは別である。

Viewerの生成・再現性・browser validationはViewer repository側のauthorityで管理し、
VAMeter-Eduでは最終Viewer assetのexact identity、AssetPool格納契約、FirmwareとAssetPoolの
matched deployment、physical qualificationを別のgateとして扱う。

## Node適用範囲

[Viewer #16](https://github.com/Yuichiroh-Kobayashi/Device-to-Browser-Viewer/issues/16)
の比較結果と [AssetPool統合仕様](../architecture/viewer-assetpool-integration.md) を基準とする。

- Node 24.21.0 / npm 11.19.0: Viewer HOST試験で使用可能。
- Node 24 product builder: 既存builderとの互換性差により **HOLD**。
- qualified Viewer Build Environment V1: product生成authorityとして継続使用。
- VAMeter-Edu Firmware/DesktopへNode runtimeやnpm packageは追加しない。

## Viewer受入れ手順

1. Viewerの最終clean source commit/tree、D2B入力、builder/toolchainを入力authorityとして固定する。
2. qualified build environmentで独立生成し、index、manifest、圧縮CSS/JSのlength/SHA-256とbundle IDを確定する。
3. `app/libs/viewer_asset_contract/` の期待値と `app/assets/web/types.h` の格納slotを照合する。
4. size、identity、route、layoutのいずれかが現行contractと異なる場合はFirmware intakeを停止し、別のreview可能な差分で更新する。
5. FirmwareとAssetPoolはcompatible pairとして生成し、実機書込み・readback・rollbackは別途承認されたphysical gateで行う。

truncate、padding、旧manifest流用、未レビューの再圧縮、integrity check回避、Viewer機能削減による
legacy slotへの押し込みは行わない。

## Stable `v2.0.0` authority

stable `v2.0.0` のViewer / Firmware / AssetPool authorityは本post-v2 intakeで変更しない。

| Item | Stable `v2.0.0` |
| --- | --- |
| Firmware commit | `ee4da1b5e5e238fbc66a9d9a49f4d051c1ca986b` |
| Firmware tree | `f80a0caaa213a965033f6773ea2d3f41af436807` |
| Viewer source | `e1ebdb1cde8585a37447a66f4c8183654f4c3cda` |
| Viewer tree | `8f8426e9af1649f68e66e4f8f432d1b91452e38d` |
| Viewer bundle | `4422530b6e1ba9549dd4bef2e3bb2c183d8fced49ed2d8d695d2a04a4aa7c2af` |
| AssetPool image | `1,655,972` bytes |
| AssetPool SHA-256 | `3a587a04127a5eab4df0d0714e37e214029bcedbbeb6a616e8426c6e9aa1c1fc` |
| AssetPool partition free | `441,180` bytes |
| Dedicated static IRAM | `16,383 / 16,384` bytes |

stable releaseのbyte、過去qualification、release判定は再分類しない。

## Final post-v2 Viewer intake candidate

Viewer #18 / #19 / #22 / #23 をmergeしたactual Viewer `main`から、qualified Viewer Build Environment V1
`sha256:755023019864d9919e890003da7117bfc2803c88c80c2ab001d40cb1f0249b19` を使って
独立したstandalone checkout A/Bから生成したfinal candidateを受入れ入力とする。

```text
Viewer source commit:
d4c0702ca0fb72099260c67b9976ade85bb681d2

Viewer source tree:
c4810727e8b3c17903d586137dae80aa1ef8b992

Viewer bundle:
01e39e5c3230bc2c3a277659014031f6f955864e5a0886c15fa004114b89c973

stored payload:
34774 bytes
```

Independent Build A/Bは4 representationすべてbyte-identicalで、各run内部two-run determinismと
外側A/B比較がPASS。Node 18 HOSTはproduct 120 named + 4 gates、CSV 16、root 30、live 13 PASS。
Node 24は `NODE24_HOST_PASS / NODE24_BUILDER_HOLD`。actual VAMeter physicalはこのViewer build gateでは
NOT RUN。

| Asset | Bytes | SHA-256 | Final served route |
| --- | ---: | --- | --- |
| index | 573 | `2275800d59506344ed693c914fbebe09b701351cdc1c568ef61ce752c1f74781` | `/viewer/` |
| manifest | 1364 | `01e39e5c3230bc2c3a277659014031f6f955864e5a0886c15fa004114b89c973` | `/viewer/asset-manifest.json` |
| CSS gzip | 2669 | `ad1eafe9be7c08ae40198db88c0e892822a0497731644138bfc2e93515f8f015` | `/viewer/assets/app.ad1eafe9be7c08ae40198db88c0e892822a0497731644138bfc2e93515f8f015.css` |
| JS gzip | 30168 | `2c7925c88541d26de3871fcc7362f7f9002164886a6e4666779a5e330fd69253` | `/viewer/assets/app.2c7925c88541d26de3871fcc7362f7f9002164886a6e4666779a5e330fd69253.js` |
| bundle ID + NUL | 65 | bundle ID above | `/viewer/device.json`で公開 |

Final manifestは `viewer_source_commit=d4c0702...`、D2B copied-reference authority
`b30ad676922af73448952d5a9cac312467a944f9` を記録する。

## Final Viewer intake — development source

**IMPLEMENTED IN SOURCE / NOT PHYSICALLY QUALIFIED**。
本PRは merged [PR #25](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/pull/25) の
layout guardを前提に、上記exact Final Viewer assetの受入れを実装する。
base commitは `95d59dd7be33ceb9031ca97ef3013253643d0c36`、treeは
`a0d82b3bd6c78c02a4f7df37df79eeed1bf68ec4`。

| Asset | Layout 1 / stable slot | Final intake layout 2 | Delta |
| --- | ---: | ---: | ---: |
| index | 573 | 573 | 0 |
| manifest | 1364 | 1364 | 0 |
| CSS gzip | 2385 | 2669 | +284 |
| JS gzip | 25809 | 30168 | +4359 |
| bundle ID + NUL | 65 | 65 | 0 |
| stored payload | 30131 | 34774 | +4643 |

以前の `FIXED_SLOT_OVERFLOW / IDENTITY_UPDATE_REQUIRED / FIRMWARE_INTAKE_BLOCKED` は
layout 1に対する判定である。本PRはexact slot resizeとbundle / 4 SHA-256 / CSS・JS route更新により
そのsource blockerを解消する。source実装はexternal review待ちであり、実機受入れやrelease成立を意味しない。
`slot capacity == expected payload length` を維持し、余裕付きslot、truncate、padding、
再圧縮、旧manifest流用、integrity check回避は導入しない。

## Layout version 2

採用構造は **exact-sized fixed slots + partition末尾の固定位置compatibility trailer**。
production offset/capacity authorityはcompile済み `sizeof` / `offsetof` であり、
trailer申告値をread lengthとして信用しない。

| Item | Development layout 2 |
| --- | ---: |
| `sizeof(WebPagePool_t)` | 120,141 |
| `sizeof(StaticAsset_t)` / StaticAsset_t used | 1,660,612 |
| StaticAsset layout version | 2 |
| Viewer sub-layout version | 2 |
| Trailer reserve | 256 |
| Layout growth reserve | 436,284 |
| Partition container bytes | 2,097,152 |

trailerは引き続き `VAMEAPL1`、format version `1`、header size `76`、固定offset `2,096,896`。
field位置、CRC convention、partition tableは変更しない。
`WebPage` baseとfont/image/color/text/syscfg/favicon/index/manifest/CSSの開始offsetは不変。
JSは `1,630,095 -> 1,630,379`、bundle IDは `1,655,904 -> 1,660,547` へ移動する。
payloadは4,643 bytes増え、旧structの末尾padding 3 bytesがなくなるためstruct全体は4,640 bytes増える。
詳しい[trailer byte layout・CRC・failure propagation](../architecture/viewer-assetpool-integration.md#development-layoutcontainer-guard)
を参照する。

full containerはpartition全体をencodeするため、file sizeから単純に「AssetPool free = 0」としない。
StaticAsset_t used、trailer reserve、layout growth reserveを分けて記録する。
stable `v2.0.0` のimage `1,655,972` / partition free `441,180` bytesは歴史値として保持する。

## Guard foundation and failure tiers

PR #25が導入した検証順序を維持する。

1. partition存在・exact sizeを確認してmmapする。
2. fixed tailから256 bytesを読み、magic / format / header size / trailer CRCを検証する。
3. StaticAsset / Viewer layout versionを検証する。
4. static size / WebPage offset / member count / member offset・capacityをcompile authorityと厳密比較する。
5. range / increasing non-overlap / reserved zeroを検証する。
6. compile-known static sizeだけをCRC scanする。
7. 成功時だけinjectして通常setupへ進む。
8. SystemLive開始時にbundle ID + 4 SHA-256を検証し、成功時だけViewer routesを登録する。

**Tier 1**: container/layout不一致ではinjectしない。bool callback / `APP::Setup()` が失敗を伝播し、
HAL・locale・Mooncake初期化前に停止する。AssetPool依存error UI、reboot loop、format、record削除、
未検証poolの継続利用はしない。deviceはreasonをlogして `app_main` からreturnする。
ESP-IDF v5.1.6のmain task削除と現在のwatchdog初期化順序を利用するこの経路は、physicalでは未検証。

**Tier 2**: layout/CRCが正しくてもViewer identityが不一致なら、既存の
`VIEWER_ASSETPOOL_IDENTITY_MISMATCH` でViewer/SystemLive routesだけをfail-closedにする。
device UI/measurement/recorderの動作範囲は維持する。
CRCはcorruption検出であり、authenticationやViewer identity authorityではない。
bundle IDやSHAをtrailerへ重複保存しない。

## Intake and host validation

Final Viewer入力はexact source/tree/bundleで固定したqualified V1 materializationを使用する。
4ファイルそれぞれのlength/SHA、manifestのsource commit・names・routes、manifest SHA == bundleを
生成前に照合する。Viewer source、D2B authority、Node 24 builderは変更しない。

独立したdirectory/processで2 MiB container A/Bを生成し、whole-file SHAと `cmp` を照合する。
独立Python検証はtrailer shape、v2、exact member table、zero reserve、両CRCを確認する。
各containerからindex / manifest / CSS / JS / bundle IDをextractし、exact inputとのlength/SHA/`cmp` を確認する。

`tests/viewer_asset_contract/` はexact sizes/hashes/routes、stable bundle拒否、同一長の改変を検証する。
test-only `--container` seamはproduction layout validatorとViewer identity/route registrationを接続する。
CRCを再計算したViewer改変でもTier 1 PASS / Tier 2 FAILとなることを検証する。
HTTP shimsによるhost証拠であり、actual HTTPD/AP/ブラウザー証拠ではない。

`tests/asset_pool_layout/` はv1/v2混在、malformed sizeとguard-page read bounds、setup failure、
loader拒否、atomic write/rename failure、全体determinismを継続検証する。
PR #25 review M-1に対応し、overlap/out-of-range等のtest名は実際のearly exact mismatch理由を示す。
range/order分岐は内部table整合性の防御であり、そこへ到達させるためにexact equalityを弱めない。

継承された `_copy_fonts / _copy_images / _copy_web_pages` の `_copy_file()` 戻り値未伝播は別follow-up。
本PRでは変更せず、今回の実入力の長さ・hash・生成結果と非Viewer prefix一致を検証記録に残す。

## Mixed-pair compatibility and physical handoff

| Firmware / AssetPool | Source / host判定 |
| --- | --- |
| Final intake / Final layout 2 | Tier 1とexact Viewer identityがPASS |
| Final intake / PR #25 layout 1 | layout version不一致でTier 1拒否 |
| PR #25 / Final layout 2 | layout version不一致でTier 1拒否 |
| Stable v2.0.0 / Final layout 2 | source分析のみ: 非Viewer prefix offsetは不変。旧FWはtrailerを解釈せず、移動したJS/bundle位置とidentity checkによりViewer拒否を期待する |

この表はmixed pairのphysical proofではない。Firmware単独更新と旧pool保持をsupported procedureにしない。
後続physical工程ではreview済みの新Firmware + 新AssetPoolをmatched pairとして固定し、
別途明示承認されたflash/readback、boot CRC timing、actual AP/Viewer、Start/Stop、UI/CSV targeted smokeと
**old stable Firmware + old stable AssetPool**へのmatched rollbackを行う。

## Resource and claim boundary

Dedicated static IRAM remaining **1 byte** はAssetPool容量と独立した制約。
本PRは `IRAM_ATTR` を追加せず、同一ESP-IDF v5.1.6環境のmerged PR #25 baseとcandidateで
ELF/map、application/bootloader/partition、flash/DRAM、dedicated/full IRAM、shared D/IRAMを比較する。
slot growthがAssetPoolに存在することからzero resource deltaを推論しない。

```text
Viewer source candidate: FIXED / BUILD-QUALIFIED (exact authority above)
VAMeter source intake: IMPLEMENTED / EXTERNAL REVIEW PENDING
AssetPool layout: V2 IMPLEMENTED
New development container: DETERMINISTIC GENERATION REQUIRED / NOT RELEASE AUTHORITY
Host tests / ESP-IDF / static resources: candidate-specific evidence required
Runtime heap/stack / boot CRC timing / physical VAMeter: NOT RUN
Release qualification: NOT ESTABLISHED
Stable v2.0.0: UNCHANGED
```
