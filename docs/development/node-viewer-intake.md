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

## Fixed-slot gate

現行stable slotとfinal candidateを照合する。

| Asset | Current slot | Final candidate | Delta | Result |
| --- | ---: | ---: | ---: | --- |
| index | 573 | 573 | 0 | SIZE_FITS |
| manifest | 1364 | 1364 | 0 | SIZE_FITS |
| CSS gzip | 2385 | 2669 | +284 | **FIXED_SLOT_OVERFLOW** |
| JS gzip | 25809 | 30168 | +4359 | **FIXED_SLOT_OVERFLOW** |
| bundle ID + NUL | 65 | 65 | 0 | SIZE_FITS |
| stored payload | 30131 | 34774 | +4643 | — |

`SIZE_FITS`は長さだけの判定で、identity一致やFirmware intake PASSを意味しない。
Final candidateの分類は次のとおり。

```text
FINAL_VIEWER_V1_INTAKE_CANDIDATE_READY
FIXED_SLOT_OVERFLOW
IDENTITY_UPDATE_REQUIRED
FIRMWARE_INTAKE_BLOCKED
```

AssetPool partitionは2 MiBで、stable実測では全体に441,180 bytes残っている。しかしこの余りは
`StaticAsset_t`末尾より後ろにあり、CSS/JSのper-member fixed array容量ではない。したがって
partition全体に数KBの余裕があることはfixed-slot overflowの解消を意味しない。

## Layout remediation analysis

read-only source分析とcompiler `sizeof` / `offsetof` probeにより、現行raw layoutは次の事実で確定した。
probeの `sizeof(StaticAsset_t)=1,655,972` はstable resource実測と完全一致した。

- `sizeof(WebPagePool_t) = 115,498`
- `sizeof(StaticAsset_t) = 1,655,972`
- `WebPagePool_t` は `StaticAsset_t` の最終member。
- `viewer_bundle_id`後には現行structのtrailing padding 3 bytesだけが存在する。
- Final candidateにexact slot resizeした場合、font/image/color/text/syscfg/favicon/index/manifest/CSSの開始offsetは不変。
- `viewer_js_gzip`だけが +284 bytes、`viewer_bundle_id`だけが +4,643 bytes移動する。
- exact slot resize後の `sizeof(StaticAsset_t) = 1,660,612`。
- 2 MiB partition内にはlayout metadata用の固定tailを確保しても十分な容量がある。

このため問題はpartition容量不足ではなく、raw C++ struct ABIのlayout compatibilityである。
Viewer bundle ID + 4 asset SHA-256の現行runtime checkはmixed Viewer pairを結果としてfail-closedにするが、
layout/version/sizeを明示するcontractではない。またboot時はViewer identity checkより前にAssetPoolの
font/text byteを使用するため、truncated/corrupt poolをViewer hashだけで安全に扱えるとはしない。

## Accepted layout remediation design — not yet implemented

設計レビュー結果は次のとおり。

```text
LAYOUT_REMEDIATION_DESIGN_READY
LAYOUT_REMEDIATION_DESIGN_ACCEPTED_WITH_REFINEMENTS
```

採用方針は **exact-sized fixed slots + assetpool partition末尾の固定位置 compatibility trailer**。
余裕付きfixed slot（A2）やdescriptor containerへの全面移行は今回採用しない。

### Exact Viewer slots

Final Viewer asset長にslotを正確に一致させ、現行の `slot capacity == expected payload length` 不変条件を維持する。

```text
index      573
manifest  1364
CSS       2669
JS       30168
bundle ID   65
```

余裕付きslotには将来raw ABIを据え置ける利点があるが、今回は`MatchesSha256()`と既存host testの
byte-exact invariantを維持して実装riskを最小化することを優先する。

### Partition-tail compatibility trailer

layoutを知らない段階でも安全に読めるよう、compatibility metadataは`StaticAsset_t`直後ではなく
**assetpool partition末尾の固定offset**に置く。`StaticAsset_t`自身の後ろへ相対配置すると、その位置を
知るために検証対象のlayoutを先に信用する循環になるため採用しない。partition先頭へheaderを置く方式も
old Firmwareが既存font byteとして解釈するため採用しない。

新container designは概念的に次の構造とする。

```text
assetpool partition: 2,097,152 bytes
  [0 .. sizeof(StaticAsset_t)-1]                  StaticAsset_t
  [sizeof(StaticAsset_t) .. partition-257]        zero-filled layout growth reserve
  [partition-256 .. partition-1]                  compatibility trailer reserve
```

Option Aのfinal slot resize後は:

```text
StaticAsset_t:                    1,660,612 bytes
compatibility trailer reserve:          256 bytes
layout growth reserve:              436,284 bytes
partition container/image:        2,097,152 bytes
```

このfull-partition container採用後は、従来の`partition size - image file size`を単純に
「AssetPool free」と呼ばない。`StaticAsset_t used`、`trailer reserve`、`layout growth reserve`を
分けてresource reportに記録する。stable `v2.0.0`の1,655,972 / 441,180 bytesは歴史値として変更しない。

### Trailer contract

trailerは少なくとも次を明示する。

- magic
- trailer/header format version
- header size
- `StaticAsset_t` layout version
- Viewer sub-layout version
- `static_asset_size`
- `offsetof(StaticAsset_t, WebPage)`
- Viewer member count
- Viewer 5 memberのoffset + capacity
- `static_asset_crc32`
- trailer/header CRC32
- reserved bytesはzero固定

Viewer bundle IDはtrailerへ重複保存せず、現行`WebPage.viewer_bundle_id`とFirmware compile constantを
identity authorityとして維持する。Viewer 4 assetのSHA-256も現行Firmware compile constantsを
権威とし、trailer metadataをViewer integrity authorityへ昇格させない。

`static_asset_crc32`はsecurity/authenticityではなく、Inject前のflash corruption / partial pool検出用。
実装後にboot時間、flash/IRAM/DRAM/stack差分を実測し、採否を最終確認する。

### Validation order and failure tiers

新FirmwareはAssetPoolを`StaticAsset_t*`としてinjectする前に、partitionとtrailerのshapeを検証する。
少なくとも以下の順序を守る。

1. assetpool partition存在確認
2. partition size確認
3. partition mmap
4. 固定tail位置からtrailer読出し
5. magic / header size / trailer CRC
6. layout version / Viewer layout version
7. `static_asset_size`がFirmware compile済み`sizeof(StaticAsset_t)`と厳密一致し、`WebPage` offsetもcompile済み`offsetof(StaticAsset_t, WebPage)`と厳密一致することを確認。不一致はTier 1とする。以降のCRC/read/bounds検証の長さ・上限はFirmware側compile定数を使い、trailer申告値をread lengthとして信用しない。
8. Viewer member count / exact offset / exact capacity / bounds / non-overlap
9. `static_asset_crc32`
10. すべてPASSした場合だけ `AssetPool::InjectStaticAsset()`
11. SystemLive start時に現行bundle ID + 4 asset SHA-256を検証
12. Viewer routes登録

Failure policyは二段に分ける。

**Tier 1 — layout/container incompatibility**

- AssetPoolをinjectしない。
- 通常`APP::Setup()`へ進まない。
- serialに具体的なreason markerを残す。
- AssetPool依存font/imageを使ったerror UIを出さない。
- reboot loopにはしない。
- 停止はtask watchdogを踏まない方式（yieldするidle、`app_main`からのreturn、または同等の非再起動経路）とし、WDT resetによる再起動反復を作らない。
- logだけ出して未検証poolをinjectし続ける方式は採用しない。

現在`AssetPoolInjection` callbackは失敗を`APP::Setup()`へ返せないため、PR-Aでは成功/失敗を
明示的に伝播できるAPI（例: callback / setupのbool化、または同等のfail-stop経路）が必要。
callbackだけ`return`して`APP::Setup()`を継続する実装は禁止する。現行`app/app.cpp`ではcallback後に
`AssetPool::SetLocaleCode(locale_code_jp)`が`getStaticAsset()->Text`へ進むため、未injectのまま継続すると
null pointer dereferenceになり得ることをこの禁止の根拠とする。

**Tier 2 — layoutは正当だがViewer identity不一致**

- 現行`VIEWER_ASSETPOOL_IDENTITY_MISMATCH`挙動を維持する。
- device本体UI/measurement/recorderを壊さず、Viewer/SystemLive routeだけfail-closedにする。
- reboot、format、record削除は行わない。

## Implementation sequencing

実装は2 PRに分け、physical gateは結合後に1回行う。

### PR-A — layout/version compatibility guard foundation

Viewer payload、Viewer slot容量、Viewer identityを変更しない。

想定scope:

- compatibility trailer encode/decode/validationのhost-testable library
- partition size / layout / bounds / CRC guard
- AssetPool full-partition deterministic generation
- struct paddingの決定化
- Inject前failure propagation / Tier 1 fail-stop
- desktop loaderの短いbin / trailer検証のfail-closed化
- host negative/mixed-layout tests
- `sizeof(StaticAsset_t) + trailer reserve <= partition size`のcompile-time assertion

PR-Aのlayout versionはstable slot shapeを表すversion 1。**PR-A単独をreleaseしない**。
PR-A以降のFirmwareは必ずtrailer付きAssetPoolを再生成・再書き込みしたmatched pairとして扱う。
旧AssetPoolのままPR-A Firmwareだけを書き込むとTier 1 fail-stopとなり通常起動しないため、Firmware単独flashを
supported development procedureにしない。PR-AとPR-Bの両方をreview済みにしてから短い間隔で順にmergeし、
unsupported development stateを長期間作らない。

### PR-B — Final Viewer exact intake

PR-A merge後にFinal Viewer identityとexact slot resizeを取り込む。

想定scope:

- CSS slot `2385 -> 2669`
- JS slot `25809 -> 30168`
- `kStoredPayloadBytes = 34774`
- bundle `01e39e5c...`
- index / manifest / CSS / JS SHA-256更新
- Final manifestに記録されたcontent-hashed CSS/JS route更新
- layout version `1 -> 2`
- Viewer contract host tests更新
- architecture / resource / validation docs更新

PR-A / PR-B結合後にmatched new Firmware + new AssetPoolをbuildし、独立AssetPool生成、ESP-IDF map、
readback、actual AP/Viewer、Start/Stop、UI/CSV targeted smoke、old Firmware + old AssetPoolへのmatched rollbackを
1つのphysical gateとして実施する。

## Resource boundary

Dedicated static IRAM remaining **1 byte** は独立したtechnical debtであり、AssetPool partition headroomと
相殺できない。PR-A / PR-Bでは`IRAM_ATTR`を追加せず、ESP-IDF mapでdedicated static IRAM、full IRAM、
DRAM `.data/.bss`、flash `.text/.rodata`、application image、stack/heap影響を実測する。

CRCやlayout validationを設計しただけではresource PASSを主張しない。実装後のmap差分がgateである。

## Claim boundary

この文書更新で確定するのは受入れ入力と実装前design decisionだけである。

```text
Viewer candidate: final / build-qualified
Viewer source: d4c0702ca0fb72099260c67b9976ade85bb681d2
Viewer tree: c4810727e8b3c17903d586137dae80aa1ef8b992
Viewer bundle: 01e39e5c3230bc2c3a277659014031f6f955864e5a0886c15fa004114b89c973
Firmware intake: BLOCKED
Layout remediation design: ACCEPTED / NOT IMPLEMENTED
Firmware/AssetPool implementation: NOT STARTED
New authoritative AssetPool binary: NOT GENERATED
Actual VAMeter physical qualification: NOT RUN
Stable v2.0.0: unchanged
```

本PRは文書のみ。Firmware source、`WebPagePool_t`、Viewer contract constants/routes、partition、
AssetPool binary、release/tag、device stateは変更しない。