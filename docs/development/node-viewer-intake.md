# Node適用範囲とViewer受入れ

## 目的

Related to #23。VAMeter-Edu側のbuild/testへNode/npm依存を追加せず、別repositoryで生成される
Viewerの受入れ条件を明確にする。ViewerのJavaScriptがassetであることと、
VAMeter-Edu build にNodeが必要なことは別である。

Viewerの生成・再現性・browser validationはViewer repository側のauthorityで管理し、
VAMeter-Eduでは最終Viewer assetのexact identity、AssetPool格納契約、FirmwareとAssetPoolの
matched deployment、physical qualificationを別のgateとして扱う。

本書はtoolchain適用範囲と受入れ条件を述べる文書である。現行の可変な数値
（byte長、member offset、struct size、hash、route）のauthorityはsourceにあり、
本書はそれを写さずに参照する。凍結済みのstable release値のみ明示値として残す。

## Node適用範囲

3つの経路を混同しない。

```text
VAMeter-Edu Firmware build (ESP-IDF v5.1.6):
Node/npm dependency = NO

VAMeter-Edu Desktop build / host tests:
Node/npm dependency = NO

Device-to-Browser-Viewer product generation:
Node = YES, within the qualified Viewer build environment
```

- VAMeter-Eduにはrepository全体で `package.json` / `package-lock.json` が存在せず、
  `CMakeLists.txt` / `*.cmake` / `idf_component.yml` / shell script / workflow の
  いずれにも `node` / `npm` / `npx` の実行記述がない。host toolはC++とPythonだけである。
- `tests/d2b_vi_integration/capture-live.js` は唯一のJavaScriptファイルだが、
  browserのDevTools Consoleへ貼り付けて使う診断helperであり、Node runtimeでは実行しない。
  詳細は [`tests/d2b_vi_integration/README.md`](../../tests/d2b_vi_integration/README.md)。
- VAMeter-EduはViewerを内部でbuildしない。`app/assets/assets.cpp` の
  `_copy_viewer_assets()` が `VAMETER_VIEWER_*_PATH` 環境変数の指す**生成済みファイル**を
  exact byte長で読み込むだけで、bundlerを起動しない。
- 一方、Viewerのproduct生成にはNodeが必要である。qualified Viewer Build Environment V1 の
  image内で `/usr/bin/node`（Node 18.19.1）がwebpackを駆動する。
  **「Firmware buildにNodeが不要」を「Viewer生成にNodeが不要」と言い換えないこと。**
- VAMeter-Edu Firmware/DesktopへNode runtimeやnpm packageは追加しない。

### Node候補versionのauthority

Node/npm候補versionの定義元はViewer repositoryの
[`tools/build-env/node-toolchain.json`](https://github.com/Yuichiroh-Kobayashi/Device-to-Browser-Viewer/blob/main/tools/build-env/node-toolchain.json)
である。VAMeter-Edu側へ版数を写して二重管理しない。
同fileは候補を `qualified_uses: host-tests` と
`unqualified_uses: product-builder / physical-client / windows-native` に分類する。

### Node 24 の現在の分類

```text
NODE24_PRODUCT_BUILDER_STATUS = HOLD
```

qualified Viewer Build Environment V1 が現在のproduct生成authorityであり、
その image digest は次のとおり。

```text
Qualified Viewer Build Environment V1
image digest:
sha256:755023019864d9919e890003da7117bfc2803c88c80c2ab001d40cb1f0249b19
```

HOLDの理由は、[Viewer #16](https://github.com/Yuichiroh-Kobayashi/Device-to-Browser-Viewer/issues/16)
の比較で、公式Node 24候補runtimeと、build環境にdistro packageとして入っている
webpack / `enhanced-resolve` の期待とが噛み合わず、builderが出力を生成できなかったことによる
（`process.config.variables.node_relative_path` が公式runtimeでundefined）。
これは**その組合せの非互換**であり、「Node 24が壊れている」でも「製品JSの不具合」でもない。

したがってNode 24をproduct生成へ移すことは、版数の差し替えではなく
**別途レビューされるbuild environmentの再qualification**を要する。
本書の更新はその再qualificationを行わず、HOLDを解除しない。

### Node 24 HOST証拠の帰属

HOST試験の証拠とfinal Viewer sourceは、別のcommitに属する。混同しない。

```text
Node24 HOST comparison evidence:
source = 81226e7b39410ac673c1b46a9b76eab6084a4f19

Final Viewer source:
d4c0702ca0fb72099260c67b9976ade85bb681d2
```

tracked な Node 24 HOST比較は final Viewer source より前のもので、source `81226e7b...`
に対して実施された。final source `d4c0702...` を Node 24 HOST PASS として再分類する
tracked evidence は現時点で確認できていない。本documentation updateはその再分類を行わない。
final sourceに対する判定が必要な場合は、同一suiteを実際に再実行し、その結果を別途記録する。

Node 18 HOSTの結果は下記「Final post-v2 Viewer intake candidate」に記載のとおり。

## Viewer受入れ手順

1. Viewerの最終clean source commit/tree、D2B入力、builder/toolchainを入力authorityとして固定する。
2. qualified build environmentで独立生成し、index、manifest、圧縮CSS/JSのlength/SHA-256とbundle IDを確定する。
3. `app/libs/viewer_asset_contract/` の期待値と `app/assets/web/types.h` の格納slotを照合する。
4. size、identity、route、layoutのいずれかが現行contractと異なる場合はFirmware intakeを停止し、別のreview可能な差分で更新する。
5. FirmwareとAssetPoolはcompatible pairとして生成し、実機書込み・readback・rollbackは別途承認されたphysical gateで行う。

truncate、padding、旧manifest流用、未レビューの再圧縮、integrity check回避、Viewer機能削減による
legacy slotへの押し込みは行わない。

## Stable `v2.0.0` authority

stable `v2.0.0` のViewer / Firmware / AssetPool authorityはpost-v2 intakeで変更しない。
以下は凍結済みの歴史値であり、本表は明示値のまま保持する。

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
post-v2の現行開発intakeがこの歴史的qualificationを遡って変えることはない。

## Final post-v2 Viewer intake candidate

Viewer #18 / #19 / #22 / #23 をmergeしたactual Viewer `main`から、
qualified Viewer Build Environment V1 を使って独立したstandalone checkout A/Bから
生成したcandidateを受入れ入力とした。

受入れたViewer identity（provenance anchor）:

```text
Viewer source commit:
d4c0702ca0fb72099260c67b9976ade85bb681d2

Viewer source tree:
c4810727e8b3c17903d586137dae80aa1ef8b992

Viewer bundle:
01e39e5c3230bc2c3a277659014031f6f955864e5a0886c15fa004114b89c973
```

Final manifestは `viewer_source_commit=d4c0702...`、D2B copied-reference authority
`b30ad676922af73448952d5a9cac312467a944f9` を記録する。

各assetのexact byte長、4つのSHA-256、content-hashed route、bundle IDの
**実効authorityはsource**にある。本書は値を写さない。

| 対象 | Source authority |
| --- | --- |
| byte長、bundle ID capacity、route数 | [`app/libs/viewer_asset_contract/viewer_asset_contract.h`](../../app/libs/viewer_asset_contract/viewer_asset_contract.h) |
| bundle ID、4 SHA-256、CSS/JS route文字列 | [`app/libs/viewer_asset_contract/viewer_asset_contract.cpp`](../../app/libs/viewer_asset_contract/viewer_asset_contract.cpp) |
| AssetPool格納slotの実寸 | [`app/assets/web/types.h`](../../app/assets/web/types.h) |
| member offset / capacity / struct size | [`app/libs/asset_pool_layout/asset_pool_layout.h`](../../app/libs/asset_pool_layout/asset_pool_layout.h)（`offsetof` / `sizeof` 由来） |
| slot == payload長 の強制 | [`app/libs/asset_pool_layout/asset_pool_layout.cpp`](../../app/libs/asset_pool_layout/asset_pool_layout.cpp) の `CHECK_MEMBER` static_assert |

`slot capacity == expected payload length` はcompile時に強制される。
余裕付きslot、truncate、padding、再圧縮、旧manifest流用、integrity check回避は導入しない。

Independent Build A/Bは4 representationすべてbyte-identicalで、各run内部のtwo-run determinismと
外側A/B比較がPASSした。Node 18 HOSTはproduct 120 named + 4 gates、CSV 16、root 30、live 13 PASS。
Node 24の帰属は前掲「Node 24 HOST証拠の帰属」を参照する。
このViewer build gate自体には実機VAMeterでの作業は含まれていない。これはbuild gateの
範囲を述べた歴史的事実であり、VAMeter側の現在のphysical状況を述べたものではない。

VAMeter-Eduは受入れたintake identityを自repositoryのsourceとtestで保持する。
Viewer repository側のprovenance記録方針はViewer側のauthorityであり、本書の範囲外である。
不足する証拠をVAMeter-Edu側で補作しない。

## Final Viewer intake — 現在の状態

Final Viewer assetの受入れは
[PR #26](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/pull/26) としてmergeされ、
[PR #25](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/pull/25) のlayout guardの上に
現在の `main` sourceへ入っている。source実装とhost testは完了している。

```text
VAMeter source intake:               MERGED (PR #25 / PR #26)
AssetPool layout:                    V2 IMPLEMENTED
Post-merge physical evidence:        PARTIAL / SEPARATE EVIDENCE EXISTS
Formal browser/device qualification: NOT COMPLETE
Release qualification:               NOT ESTABLISHED
```

source mergeはそれ自体がphysical受入れでもrelease成立でもない。

merge後に別途、matched post-v2 Firmware / layout-2 AssetPool candidateの実機書込みと、
実機VAMeterによるdevice-hosted Viewer配信が行われている。**本書はその実機記録のauthorityではなく、
内容を複製しない。**実機evidenceは専用のvalidation記録が所有する。

同時に、その実機smokeだけで全体qualificationが成立するわけではない。full AssetPool
post-write readback、exact-prestate rollbackの確立、Tier 1 negative/rejection pathの実機試験、
測定されたboot CRC timingの受入れ、runtime resource qualification、
reconnect/soakを含む完全なbrowser/device qualification、release qualificationは、
それぞれ独自のevidenceによってのみ成立する別gateである。

歴史的経緯として、layout 1に対する以前の判定
`FIXED_SLOT_OVERFLOW / IDENTITY_UPDATE_REQUIRED / FIRMWARE_INTAKE_BLOCKED` は、
PR #26のexact slot resizeとbundle / 4 SHA-256 / CSS・JS route更新により解消済みである。
layout 1からlayout 2への各assetのdelta、およびmerge前のbase commit/treeは、
PR #25 / PR #26 の記録に残る。

## Layout version 2

採用構造は **exact-sized fixed slots + partition末尾の固定位置compatibility trailer**。
production offset/capacity authorityはcompile済み `sizeof` / `offsetof` であり、
trailer申告値をread lengthとして信用しない。

struct size、WebPage base offset、Viewer member offset/capacity、trailer reserve、
layout growth reserve、partition container bytes、trailer byte layout、CRC convention、
failure propagationは
[AssetPool統合仕様](../architecture/viewer-assetpool-integration.md#development-layoutcontainer-guard)
とそこから参照されるsourceがauthorityである。本書は値を複製しない。

layout 2では`WebPage` baseとfont/image/color/text/syscfg/favicon/index/manifest/CSSの
開始offsetは不変で、JSとbundle IDの位置が後方へ移動する。

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
ESP-IDF v5.1.6のmain task削除と現在のwatchdog初期化順序を利用するこの**拒否**経路は、
実機で意図的に発火させる試験をまだ行っていない。通常のboot成功はこの経路を通らないため、
実機bootが観測されたことはこの経路の検証にはならない。

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

`tests/asset_pool_layout/container_test.py` はcompile authorityに対する**意図的に独立した**
検証oracleである。そこに書かれた期待値をsourceからの重複として削除しない。

継承された `_copy_fonts / _copy_images / _copy_web_pages` の `_copy_file()` 戻り値未伝播は別follow-up。
PR #26では変更しておらず、当時の実入力の長さ・hash・生成結果と非Viewer prefix一致を検証記録に残した。

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
現行resource authorityは [`resource-budget.md`](../architecture/resource-budget.md) である。
PR #26は `IRAM_ATTR` を追加していない。
slot growthがAssetPoolに存在することからzero resource deltaを推論しない。

```text
Viewer source candidate:      FIXED / BUILD-QUALIFIED (exact identity above)
VAMeter source intake:        MERGED (PR #25 / PR #26)
AssetPool layout:             V2 IMPLEMENTED
New development container:    DETERMINISTIC GENERATION REQUIRED / NOT RELEASE AUTHORITY
Node 24 product builder:      HOLD
Node 24 HOST on d4c0702:      NOT ESTABLISHED (tracked comparison belongs to 81226e7b)

Post-merge physical VAMeter smoke:   PARTIAL / exists outside this document
Full AssetPool post-write readback:  NOT ESTABLISHED HERE
Exact-prestate rollback:             NOT ESTABLISHED HERE
Tier 1 negative physical path:       NOT ESTABLISHED HERE
Boot CRC timing qualification:       NOT ESTABLISHED HERE
Runtime heap/stack qualification:    NOT ESTABLISHED HERE
Formal browser/device qualification: NOT COMPLETE
Release qualification:               NOT ESTABLISHED
Stable v2.0.0:                       UNCHANGED
```

`NOT ESTABLISHED HERE` は「この文書がそのevidenceを所有していない」という意味であり、
実機作業が一切行われていないという意味ではない。

## 関連文書

- [Issue #23 監査記録](issue-23-node-viewer-intake-audit.md) — Node touchpoint棚卸しと
  cross-repository authorityの確認結果。
- [AssetPool統合仕様](../architecture/viewer-assetpool-integration.md) — layout/container契約。
- [build and validation](../ai/build-and-validation.md) — build/test/AssetPool/hardware境界。
