# Issue #23 Node適用範囲とViewer取込み再現性 監査記録

Status: AUDIT / DESIGN REVIEW ONLY
Implementation authority: NO
Node24 product builder adoption: NOT IMPLIED
Physical qualification: NOT RUN BY THIS AUDIT
Base commit: 3348bc86d5d911e131315ad35d6cec022ab01a59

> **Implementation-review note**
>
> 本監査session自体はbuild検証も実機検証も実行していない。本書中の `NOT RUN` は
> **この監査sessionが実行しなかった作業**を指すものであり、「post-merge実機evidenceが
> 存在しない」という主張ではない。
>
> merge後に別途、実機VAMeterへのcandidate書込みとdevice-hosted Viewer配信を含む
> physical smoke evidenceが存在する。formal / release qualificationは専用のvalidation
> evidenceが所有しており、本監査によって成立するものではない。
>
> この注記はimplementation branch側のreview中に追加した。design branch
> `design/issue-23-node-viewer-intake` の原本は変更していない。

本書は [Issue #23](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/23) に対する
read-only監査の記録である。source、build設定、Node/npm環境、Issue状態、外部repositoryの
いずれも変更していない。test/build/実機検証は本監査sessionでは実行していない。

監査対象tree: `b69b3235523e35bf6c6fadfda6e3cad1a70c383d`
(`origin/design/issue-23-node-viewer-intake` と一致、drift検出なし)

## 1. Decision

**Issue #23の実質的内容は、既にsourceとdocumentでほぼ充足している。不足しているのは
「実行」ではなく「集約と用語の固定」である。**

監査で確定した3つの中核事実:

| 問い | 回答 | 根拠 |
| --- | --- | --- |
| production Firmware buildはNodeを要求するか | **しない** | repository全体に `package.json` / `package-lock.json` が存在しない。`CMakeLists.txt` / `*.cmake` / `idf_component.yml` / `*.sh` / `*.yml` のいずれにも `node` / `npm` / `npx` の実行記述がない |
| Desktop buildはNodeを要求するか | **しない** | 同上。root `CMakeLists.txt` はC++のみ。host testは全てC++実行ファイルとPython |
| final Viewer intakeはVAMeter-Edu内部でViewerをbuildするか | **しない** | `app/assets/assets.cpp:227` `_copy_viewer_assets()` は `VAMETER_VIEWER_*_PATH` 環境変数が指す**生成済みファイル**を厳密byte長でcopyするだけである。bundler呼び出しは存在しない |

したがって Issue #23 の「Firmware buildに不要なNode依存を追加しない」は、追加しない
という**設計判断ではなく、現に成立している観測事実**である。Node24をproduct builderへ
採用するか否かは、この結論に影響しない。

推奨: **B（VAMeter側の小さなdocs/config PR一本）**。詳細は第11節。

## 2. Node/npm touchpoint inventory

### 2.1 VAMeter-Edu 全件

検索語 `node` / `npm` / `npx` / `package.json` / `package-lock.json` / `.js` / `.mjs` /
`.cjs` / `capture-live.js` / `Device-to-Browser-Viewer` / `NODE24` / `builder` を
`.git` を除く全treeへ適用した結果。

| # | 対象 | 分類 | 内容 |
| --- | --- | --- | --- |
| 1 | `tests/d2b_vi_integration/capture-live.js` | **E** physical/browser診断helper | 唯一のJavaScriptファイル。`require(` / `process.` / `module.exports` / shebang / `import ... from` のいずれも含まない。`tests/d2b_vi_integration/README.md` は「Chrome or EdgeのDevTools Consoleへ全文を貼る」と指示する。**Node runtimeでは実行されない** |
| 2 | `docs/development/node-viewer-intake.md` | **F** 文書 | `npm` / `NODE24` / `Node 24` / `Node 18` の語を含む唯一のファイル |
| 3 | `README.md:233` | **F** 文書 | 上記文書への1行link |
| 4 | `docs/architecture/viewer-assetpool-integration.md` | **F** 文書 | 語として `node` を含むが、Node.js文脈ではない |
| 5 | `app/libs/d2b_vi/d2b_control.cpp` | **偽陽性** | JSON parserのAST `Node` / `NodeType` / `kMaximumNodes` |
| 6 | `app/assets/web/syscfg.html` | **偽陽性** | build済みVue bundle中の `addedNodes` / `createTextNode` / `parentNode` 等のDOM API名 |
| 7 | `platforms/vameter/sdkconfig:597` | **偽陽性** | `CONFIG_ESP_RMAKER_READ_NODE_ID_FROM_CERT_CN`（未設定） |
| 8 | `app/libs/d2b_vi/d2b_session.*`, `local_fault.h`, `d2b_esp_transport.cpp`, `d2b_vi_transport_test.cpp` | **偽陽性** | 語 `builder` はC++の文字列builder/frame builderであり、Viewer builderではない |
| 9 | `CONTRIBUTING.md` / `CONTRIBUTING_ja.md` | **F** 文書 | repository責任分界の説明 |
| 10 | `.github/` | **G** CI | `.github/copilot-instructions.md` のみ。**workflowファイルは1件も存在しない** |

分類別集計:

- **A. Firmware build dependency**: 0件
- **B. Desktop build dependency**: 0件
- **C. Host test/tooling**: 0件（host tool は `tests/**/*.py` 6件とC++のみ）
- **D. Viewer external build/intake**: 実体0件。取込みは環境変数経由のbyte copyのみ
- **E. Physical/browser diagnostic helper**: 1件（`capture-live.js`、browser貼付け用）
- **F. Documentation/history only**: 4件
- **G. CI**: 0件

**結論: VAMeter-Eduには、Node runtimeによって実行されるファイルが1件も存在しない。**
Issue #23 commentの「CI: NOT RUN（対象workflowなし）」は現在も正しい。

### 2.2 Device-to-Browser-Viewer（外部、read-only）

clone検証: HEAD `d4c0702ca0fb72099260c67b9976ade85bb681d2`、
tree `c4810727e8b3c17903d586137dae80aa1ef8b992`。**Issue task記載のfinal Viewer
source/treeと完全一致**（本監査で直接確認）。

- `package.json`: 存在するが `{name, private, type, description}` のみ。
  `dependencies` / `devDependencies` / `scripts` / `engines` フィールドは**いずれも無い**。
  自己記述は "Dependency-free static d2b-stream/0.1 V/I viewer prototype; no npm dependencies."
- `package-lock.json` / `.nvmrc` / `.node-version`: **存在しない**
- Node/npm候補versionの単一定義元: `tools/build-env/node-toolchain.json`
- CI: `.github/workflows/node-host-tests.yml`（HOST試験）、
  `.github/workflows/reproducible-viewer-build.yml`（builder qualification）

### 2.3 Device-to-Browser-Data-Streaming（外部、read-only）

clone検証: HEAD `5bf62bed455450e0f74b143040edb32c5a2e4dcb`。

- `package.json` / `package-lock.json` / `*.mjs` / `*.cjs` / `.nvmrc`: **0件**
- `.github/`: **存在しない**（CI無し）
- tool: `tools/generate_test_vectors.py`、`tools/validate_test_vectors.py`（Python）
- `reference/browser/src/*.js` 12件はbrowser向けreference実装であり、Node実行対象ではない

## 3. Build/test/intake responsibility matrix

| 経路 | 実行主体 | Node必要性 | 現在のauthority |
| --- | --- | --- | --- |
| VAMeter Firmware build (ESP-IDF v5.1.6) | VAMeter-Edu | **不要** | `docs/ai/build-and-validation.md` |
| VAMeter Desktop build / host test | VAMeter-Edu | **不要** | root `CMakeLists.txt`、`ctest` |
| AssetPool生成 | VAMeter-Edu desktop app | **不要** | `app/assets/assets.cpp` + 環境変数4本 |
| Viewer product bundle生成 | Device-to-Browser-Viewer | **必要（image内蔵Node 18.19.1）** | Build Environment V1 |
| Viewer HOST test | Device-to-Browser-Viewer | **必要（project-local Node）** | `tools/build-env/node.py` |
| D2B protocol validation | Device-to-Browser-Data-Streaming | **不要（Python）** | `tools/validate_test_vectors.py` |
| 実機live capture | browser（device UI上） | **不要** | `capture-live.js` をDevTools貼付け |

### 3.1 「Viewer builderはNodeを使わない」は誤り

重要な訂正点として記録する。qualified Viewer Build Environment V1 は Node を**使う**。

`tools/p2-builder/p2-builder.py` は Python driverだが、内部で `/usr/bin/node` を起動し、
webpack 5.76.1 + terser-webpack-plugin 5.3.7 のNODE_DRIVERを実行する
（`p2-builder.py:158-165` がAPI probe、`:187-194` が `run_webpack()`）。
`tools/build-env/Dockerfile` は `nodejs=18.19.1+dfsg-6ubuntu5` と
`webpack=5.76.1+dfsg1+~cs17.16.16-1` をDebianパッケージとして固定する。

本監査で `sha256sum tools/p2-builder/p2-builder.py` を実行し、
`616e1e4aff16d21b49f4d0b8f3c8bda46a5f47ad09d4a2eb9a0b0227ca06c5aa` を得た。
これは `reproducible-viewer-build.yml` がCIで固定検証する値および
`docs/provenance/build-environment-v1.json` の `recovered_builder_sha256` と一致する。**PASS**（直接確認）。

したがって正しい命題は「Firmware buildにNodeが不要」であり、
「Viewer生成にNodeが不要」ではない。両者を混同しないこと。

## 4. Documentation consistency audit

### 4.1 用語（Node18 / Node24 / qualified builder）

VAMeter-Edu側で `Node 18` / `Node 24` / `NODE24` / `npm` を含む文書は
`docs/development/node-viewer-intake.md` **1件のみ**であり、用語の分散はない。
同文書の3分類（HOST使用可 / product builder HOLD / V1継続使用）は、
Viewer側 `tools/build-env/node-toolchain.json` の
`qualified_uses: ["host-tests"]` / `unqualified_uses: ["product-builder", "physical-client", "windows-native"]`
と整合する。**用語衝突なし。**

### 4.2 stable v2.0.0 immutable authority

`node-viewer-intake.md`「Stable `v2.0.0` authority」節、`resource-budget.md`
「Current stable `v2.0.0` resource facts」節、`viewer-assetpool-integration.md`
「Stable `v2.0.0` Viewer intake」節の3箇所が、いずれも
「stable releaseのbyteを再生成・再分類しない」と明記する。
**releaseバイト再生成を指示する記述は、監査範囲のどの文書にも存在しない。**
Issue #23完了条件「既存リリースを再生成物で上書きしない」は充足。

### 4.3 HOST PASS と BUILDER HOLD の分離

分離自体は明確だが、**証拠のsourceが異なる点が文書上明示されていない**。

`node-viewer-intake.md` は final Viewer intake節（source `d4c0702...`）の中で
「Node 18 HOSTはproduct 120 named + 4 gates、CSV 16、root 30、live 13 PASS。
Node 24は `NODE24_HOST_PASS / NODE24_BUILDER_HOLD`」と続けて書く。

しかし Viewer側の唯一のNode18/Node24比較記録
`docs/development/node-comparison-2026-09-11.json` は、比較source を
`81226e7b39410ac673c1b46a9b76eab6084a4f19` と明記し、
その時点のtest数を `product: 74, protocol: 30, live_gate: 13` と記録する。
`d4c0702` に対するNode24 HOST再実行の記録は、Viewer repositoryのtracked fileには**無い**。

- Node18 HOST（`d4c0702`）: 記載あり、本監査では未実行のため未検証
- Node24 HOST（`d4c0702`）: **証拠が見つからない**。`NODE24_HOST_PASS` は
  source `81226e7b` に対する2026-09-11の結果である

部分的な裏付けとして、`src/product/p2-sp/tests/` の静的構造を確認した:
`test(` 呼び出しを1件も持たないassertion scriptファイルがちょうど**4件**
（`deployment-context` / `lifecycle-ui` / `one-runtime` / `responsive-static`）存在し、
「+ 4 gates」という表現の構造と一致する。`test(` の静的出現総数は117件。
これは構造の傍証であり、**120 named の確認でもPASS判定でもない**（test未実行）。

### 4.4 Viewer intakeのexact frozen byte扱い

`viewer-assetpool-integration.md` の記述どおり、
`_copy_viewer_assets()` は `stat` によるexact byte長一致を要求し、
不一致時は `CreateStaticAsset()` が `nullptr` を返してpool生成を中止する。
size-tolerantな経路は存在しない。**source上PASS**（コード確認済み。本監査は実機確認していない）。

exact equalityは compile時にも担保されている:
`app/libs/asset_pool_layout/asset_pool_layout.cpp:16-24` の `CHECK_MEMBER` が
`sizeof(WebPagePool_t::viewer_*)`（`app/assets/web/types.h` 由来）と
`VIEWER_ASSET_CONTRACT::k*Bytes` の **`==` 一致**を static_assert する。
`app/assets/assets.cpp:30-37` の4件は `<=` だが、上記が厳格側を押さえている。

### 4.5 browser/physical client向けNode と product build向けNode

VAMeter-Edu側では区別が構造的に成立している。`capture-live.js` はbrowser貼付け専用で
Node runtimeを一切参照しない。Viewer側 `node-toolchain.json` も `physical-client` と
`windows-native` を `unqualified_uses` に置く。**混同なし。** Issue #23完了条件
「ビルド用Nodeと実機client用Nodeの確認結果を混同しない」は充足。

### 4.6 重複したhard-coded version / hash

| 値 | VAMeter-Edu内の出現箇所 | 拘束関係 |
| --- | --- | --- |
| 4 SHA-256 + bundle ID | `viewer_asset_contract.cpp`、`tests/viewer_asset_contract/viewer_asset_contract_test.cpp`、`node-viewer-intake.md` | testは意図的な独立検証。docは**非拘束の写し** |
| byte長 573/1364/2669/30168 | `viewer_asset_contract.h`、`app/assets/web/types.h`、`tests/asset_pool_layout/container_test.py`、`node-viewer-intake.md`、`viewer-assetpool-integration.md` | 前2者はstatic_assertで拘束。後3者は**非拘束** |
| 34774 (stored payload) | `node-viewer-intake.md`、`viewer-assetpool-integration.md` | 非拘束 |
| member offset表 (`1625773` 等) | `viewer-assetpool-integration.md`、`container_test.py` | productionは `offsetof` 由来。この2件は**非拘束の写し** |
| Viewer source `d4c0702...` / V1 image digest `755023...` | `node-viewer-intake.md` のみ | 重複なし |

非拘束の写しは **doc 2件 + `container_test.py` 1件**。`container_test.py` は
compile authorityに対する独立検証として意図的であり、除去すべきではない。
実害があるのは**doc 2件の重複**のみで、Issue #23完了条件
「版数・hashを複数helperへ直書きしない」は helper については充足している。

### 4.7 PR #25 / #26 後のstale reference

- `node-viewer-intake.md:92`「**本PR**は merged PR #25 の layout guardを前提に…」
  および `:204`「**本PR**は `IRAM_ATTR` を追加せず…」は、PR #26 merge済みの現在、
  主語が解決しない。同節見出しは「Final Viewer intake — **development source**」、
  状態は「IMPLEMENTED IN SOURCE / NOT PHYSICALLY QUALIFIED」「external review待ち」
  のまま。PR #26は既にmergeされ `main` 系列へ入っている
  （HEAD `3348bc8` = "Merge pull request #26"）
- `viewer-assetpool-integration.md:81-85` も同様に「external review is pending」
- 同文書が引くbase commit `95d59dd7...` / tree `a0d82b3b...` はPR #26 merge前の値
- `docs/development/` が **`docs/README.md` の document roles一覧、`AGENTS.md` の
  Main directories、`docs/ai/README.md` のDocuments一覧のいずれにも登録されていない**。
  Node/Viewer intake authorityを保持するdirectoryが、repository自身の文書役割表に無い
- Issue #23 の唯一のcomment（2026-09-11）は draft PR #24 段階の
  「比較bundleはmanifest不一致のため取込み不可」で停止している。
  PR #24/#25/#26 merge後の状態を反映していない

いずれも**事実の誤りではなく、時制と登録の未更新**である。

## 5. Cross-repo authority findings

### 5.1 本監査で直接検証した一致（PASS）

| 項目 | 期待値 | 観測 | 判定 |
| --- | --- | --- | --- |
| VAMeter base commit | `3348bc86...` | 一致 | **PASS** |
| VAMeter base tree | `b69b3235...` | 一致 | **PASS** |
| branch drift | 無し | `origin/design/issue-23-node-viewer-intake` = 同一 | **PASS** |
| Viewer source commit | `d4c0702c...` | Viewer `main` HEAD と一致 | **PASS** |
| Viewer source tree | `c4810727...` | 一致 | **PASS** |
| p2-builder SHA-256 | `616e1e4a...` | 一致（CI固定値・provenance両方と） | **PASS** |
| D2B copied-reference tree OID | `6e5b4844...` | Viewer `HEAD:src/protocol/d2b-reference` = D2B `HEAD:reference/browser/src` = 同一 | **PASS** |

### 5.2 Viewer側 package.json / scripts / builder path / test path

- `engines`: **未定義**
- `scripts`: **未定義**（CIが直接 `python3 tools/build-env/node.py exec -- node ...` を呼ぶ）
- builder path: `tools/p2-builder/p2-builder.py`（内部で `/usr/bin/node` + webpack）
- current product入口: `tools/product-repro/build-current-product.py`
- historical oracle: `tools/product-repro/verify-beta1-reproduction.py`
- test path: `src/product/p2-sp/tests/*.test.mjs`、`tests/node-self-tests.mjs`、
  `tests/live-gate-regressions.mjs`
- Node version config: `tools/build-env/node-toolchain.json`（GPG keyring SHA と
  archive SHA-256 を含む検証付きinstall）

### 5.3 Qualified Build Environment V1 identity

`docs/provenance/build-environment-v1.json` より:

```text
base image:    ubuntu@sha256:33ceb71981b602c1a7443a53469e4dba065f7503eab3078a2d7a57a2ab987517
image digest:  sha256:755023019864d9919e890003da7117bfc2803c88c80c2ab001d40cb1f0249b19
architecture:  linux/amd64
node:          v18.19.1  (dpkg 18.19.1+dfsg-6ubuntu5)
webpack:       5.76.1    (dpkg 5.76.1+dfsg1+~cs17.16.16-1)
terser-webpack-plugin: 5.3.7 / terser: 5.19.2
python:        3.12.3
```

VAMeter-Edu `node-viewer-intake.md` が引くV1 digest `755023...` と一致。**PASS**。

### 5.4 Node24 builder HOLD の正確な理由

`docs/development/node-comparison-2026-09-11.json`（Viewer側、tracked）:

```text
comparison_source:        81226e7b39410ac673c1b46a9b76eab6084a4f19
baseline_builder_exit:    [0, 0]        (Node 18.19.1)
candidate_builder_exit:   [1, 1]        (Node 24.21.0)
candidate_output:         null
candidate_builder_status: HOLD
failure: enhanced-resolve ResolverFactory.js:235:
         process.config.variables.node_relative_path is undefined
```

HOLDは「Node 24が壊れている」ではなく、**Debianパッケージ版webpackの
`enhanced-resolve` が期待する `process.config.variables.node_relative_path` を、
nodejs.org公式binaryが持たない**という、distro package と official runtime の
組合せ非互換である。Viewer側文書も「Node一般や製品JSの不具合とは断定しない」と明記する。

**この理由が正しいなら、Node24採用の実際の作業はNode版数変更ではなく、
webpack/enhanced-resolveの入手経路（distro package → 固定版npm取得）の変更である。**
それはBuild Environment V1の再qualificationを意味し、Issue #23の範囲を大きく超える。

### 5.5 最重要の発見: final bundle identity が生成側repositoryにtrackedされていない

Viewer repository の `d4c0702`（= final source authority）全treeを検索した結果:

- `01e39e5c3230bc2c3a277659014031f6f955864e5a0886c15fa004114b89c973`: **0件**
- `2c7925c88541...` / `ad1eafe9be7c...` / `2275800d5950...`: **0件**
- `34774` / `30168` / `2669`: **0件**
- 文字列 `d4c0702`: **0件**

`docs/provenance/build-environment-v1.json` が持つのは beta1 / pr11 / pr12 のみ。
`docs/viewer-source-authority.md` が名指すのは stable v2.0.0 の `4422530b...`。

これは事故ではなく、Viewer側の明示的な方針である。
`docs/product/session-history-and-review.md` は
「Candidate identities are recorded externally in the Draft PR only after the final
tracked commit as PROVISIONAL DEVELOPMENT BUILD, not final Firmware intake identity,
avoiding a self-invalidating provenance commit」と述べる。

**帰結: final Viewer bundle `01e39e5c...` の唯一のtracked記録は VAMeter-Edu 側の
`viewer_asset_contract.cpp` / `viewer_asset_contract_test.cpp` /
`node-viewer-intake.md` である。生成側の対応記録はPR本文にしか存在しない。**

この状態は Issue #23 作業項目4「Viewerのsource、build environment、bundle/asset hashを
受入れ記録へ結びつける」に対し、鎖の片端（生成側）がgit外にあることを意味する。
本監査session からは Viewer PR #22/#23 の本文を読めない（GitHub API は
`yuichiroh-kobayashi/vameter-edu` にのみscopeされ、Viewerはanonymous git readのみ）。
**したがって `01e39e5c...` の生成側対応確認は本監査では未実施である。**

## 6. Issue #23 acceptance matrix

| # | 完了条件 | 判定 | 根拠 |
| --- | --- | --- | --- |
| 1 | Nodeの使用経路と共通化対象・対象外が明記されている | **Satisfied but evidence fragmented** | 事実は成立（第2-3節）。ただし「Firmware不要」の根拠がrepository全体の不在証明であり、どこにも1箇所にまとまっていない。`docs/development/` 自体が文書役割表に未登録 |
| 2 | 対象経路のテストと生成物比較が成功し、差分の理由が説明できる | **Satisfied but evidence fragmented** | Node18/Node24比較はViewer側に完全な形で存在（第5.4節）。差分理由も特定済み。ただし比較sourceは `81226e7b` であり final `d4c0702` ではない |
| 3 | 実機clientを移行する場合、WebSocket終了/異常切断/child process終了/Windows-native保存・flushをHOSTで確認 | **Blocked / out of scope（条件不成立）** | 実機clientは移行対象外。`capture-live.js` はbrowser貼付けでNode非依存。`node-toolchain.json` も `physical-client` / `windows-native` を `unqualified_uses` に分類。条件節が発火しない |
| 4 | ビルド用Nodeと実機client用Nodeの確認結果を混同しない | **Already satisfied** | 第4.5節 |
| 5 | 既存リリースを再生成物で上書きしない | **Already satisfied** | 第4.2節。3文書が一致して禁止を明記 |
| 6 | 新Viewer受入れ時、HOSTでのasset整合性・容量確認と未実施の実機確認を区別 | **Already satisfied** | `node-viewer-intake.md` の claim boundary block、および `viewer-assetpool-integration.md` のTier 1節が、host証拠と未実施の実機gateを明示的に分けている |
| 7 | README/開発手順は設定ファイルを参照し、版数・hashを複数helperへ直書きしない | **Docs consolidation needed** | helperへの直書きは無い（`container_test.py` は意図的独立検証）。しかし doc 2件が byte長・offsetを非拘束に重複保持（第4.6節） |

補助的に、Issue本文の作業項目4「Viewer source / build environment / bundle hashを
受入れ記録へ結びつける」については **Viewer-repo change needed**（第5.5節）。
ただしこれは Viewer 側の provenance 方針の問題であり、VAMeter-Edu 側の受入れ記録は既に完備である。

### Issue #23 は Node24 を product builder に採用せずに閉じられるか

**閉じられる。**

理由:

1. Issue titleと本文の目標は「移行**範囲**の確認」と「取込みの**再現性**確認」であり、
   Node24採用そのものではない。Issue本文は「24.21.0を**候補として**必要な比較を行う」
   と書き、末尾で「候補で回帰が出た場合は既存環境を保持し、Node共通化だけを保留する」
   と明示的に保留経路を用意している。
2. その保留条件は既に発火済みである。builder exit 1 × 2回、候補出力null、
   失敗箇所まで特定済み（第5.4節）。これは「未実施」ではなく「実施して不採用」である。
3. 完了条件7項目のうち、Node24採用を前提とするものは0件。条件3は条件節が不成立。
4. Firmware/DesktopへNode依存を追加しないという中核要件は、
   **不在証明として repository 全体で成立済み**である。

残るのは第4.7節の時制更新と第4.6節の重複整理、すなわち文書作業のみである。

## 7. Remaining uncertainty

本監査が**確認できなかった**事項を明示する。

1. **final source `d4c0702` に対するNode24 HOST結果**。Viewer repositoryのtracked file
   に記録が無い。`NODE24_HOST_PASS` は `81226e7b` に対する記録である
2. **`120 named + 4 gates` / `CSV 16` の正確な件数**。test未実行。静的には
   gate scriptがちょうど4件、`test(` 出現117件を観測したのみ
3. **final bundle `01e39e5c...` の生成側対応記録**。Viewer PR #22/#23 の本文を
   本sessionから読めない（GitHub API scope外）
4. **Independent Build A/B の byte一致**。`node-viewer-intake.md` の記載を
   本監査は再現していない
5. **AssetPool layout 2 の実機挙動**。本監査はこれを確認していない。Tier 1 stop path
   （拒否経路）の実機発火、測定されたboot CRC timingの受入れ、watchdog順序の qualification
   は、本監査時点のsource記載どおり NOT RUN であり、後続の実機smokeでも別gateのまま残る。
   一方、通常のboot成功とViewer配信についてはmerge後に別途実機evidenceが存在する。
   本監査はそのevidenceのauthorityではない
6. **D2B authority commit `b30ad676...`** の存在確認。shallow cloneのため
   commit objectを取得できない。ただし D2B HEAD `5bf62bed` の
   `reference/browser/src` tree OID は pinned値 `6e5b4844...` と一致しており、
   **内容authorityは維持されている**
7. **`enhanced-resolve` 非互換の現在時点での再現性**。2026-09-11の記録に依拠する

## 8. Later WSL reproducibility experiment

以下は**設計のみ**。本監査では1件も実行していない。

### 8.1 HOST equivalence（Node18 vs Node24）

目的: HOST試験結果の同値性のみを見る。product byteについて何も主張しない。

前提: Viewer repository の clean checkout（`d4c0702` 固定）、Linux x64、Python 3.12、
`gpgv`。既存runtimeは上書きしない。

```bash
# Viewer repository側で実行
python3 tools/build-env/node.py install
python3 tools/build-env/node.py check
python3 tools/product-repro/materialize-source-export.py

# 候補 Node 24.21.0
python3 tools/build-env/node.py exec -- node --test src/product/p2-sp/tests/*.test.mjs
python3 tools/build-env/node.py exec -- node tests/node-self-tests.mjs
python3 tools/build-env/node.py exec -- node tests/live-gate-regressions.mjs
```

基準側（Node 18.19.1）は Build Environment V1 image 内の `/usr/bin/node` で同一suiteを実行する。

記録項目: 各suiteの named test数、gate script数、pass/fail、stderr全文、exit status。
各2回実行し、run間差異も記録する。

**この試験の出力は product byte について何の主張もしない。**
`d4c0702` に対するNode24 HOST結果が得られれば、第7節の不確実性1が解消する。

### 8.2 Product builder equivalence（qualified V1 vs Node24 candidate）

前提: 独立した2つのstandalone checkout。同一 `d4c0702` / tree `c4810727...`。
`git status --porcelain=v1 -uall` が空であることを両方で確認。
`src/protocol/d2b-reference` tree OID が `6e5b4844...` であることを確認。

```bash
# 基準 A: qualified V1 をそのまま
tools/build-env/run-product-repro.sh \
  viewer-build-env-v1:<tag> /abs/path/checkout-A /abs/path/evidence-A current

# 候補 B: image内 /usr/bin/node のみを公式Node 24 binaryへread-only bindした変種
#   （2026-09-11比較と同じ手法。Python・webpack・terser・圧縮設定は変更しない）
```

比較対象（**すべてbyte単位**）:

- `index.html` の length / SHA-256 / `cmp`
- `asset-manifest.json` の length / SHA-256 / `cmp`
- CSS gzip の length / SHA-256 / `cmp`
- JS gzip の length / SHA-256 / `cmp`
- bundle ID
- gzip determinism（`p2-builder.py:213` の契約: `mtime=0`、filename欠、
  compresslevel=9、FNAME flag非設定）
- 各run内部の two-run determinism
- 外側A/B比較

**差異が出た場合**:

- 差異を characterize する（どのrepresentation、何bytes、どのoffsetから）
- 圧縮設定やPython側を調整して一致させることは**しない**
- builder用途は **HOLD のまま**とし、別途の明示承認なしに解除しない
- 2026-09-11と同じ `enhanced-resolve` 失敗が再現した場合は、
  それが「Node 24の問題」ではなく「distro webpack × official runtime」の問題である
  という第5.4節の分類を維持する

### 8.3 VAMeter intake

Viewer側で新しいbyteを採用しない限り、VAMeter側に新規作業は生じない。
現行byteの再確認だけを行う場合:

```bash
cd /path/to/VAMeter-Edu
cmake -S . -B build/desktop
cmake --build build/desktop -j 2
ctest --test-dir build/desktop --output-on-failure -R 'viewer_asset_contract|asset_pool_layout'
```

- `tests/viewer_asset_contract/` が exact sizes / hashes / routes、
  stable bundle拒否、同一長改変の拒否を検証する
- `tests/asset_pool_layout/` が trailer shape、layout v2、member table、
  zero reserve、両CRC、container determinism を検証する
- AssetPool layout / capacity は `container_test.py` の member表で照合する
- **released artifact（stable v2.0.0 の application binary / AssetPool image）を
  再生成しない。** 既存 `AssetPool-VAMeter.bin` を無条件削除しない
- 実機書込み・readback・rollbackは本実験に含めない

## 9. Single-source-of-truth proposal

mega-configは作らない。toolchain / product asset / physical evidence の
3層を混ぜない。各値はそれを**生成する側**のrepositoryが所有する。

| 値 | 所有すべきrepository / file | 現状 | 必要な変更 |
| --- | --- | --- | --- |
| Node/npm候補version | Viewer `tools/build-env/node-toolchain.json` | **既に単一** | なし |
| qualified builder identity | Viewer `docs/provenance/build-environment-v1.json` | **既に単一** | なし |
| Viewer source commit / tree | Viewer `docs/viewer-source-authority.md` | stable v2.0.0のみ記載。`d4c0702` はtracked記録が無い | Viewer側判断（第5.5節） |
| D2B authority | D2B repository + Viewer の copied tree OID | **tree OID一致で検証可能。既に単一** | なし |
| Viewer bundle ID | **VAMeter `viewer_asset_contract.cpp`**（device配信側の唯一の実効authority） | 実効authorityとして単一。doc重複あり | docは値を持たず `.cpp` を指す |
| 4 asset SHA + route | 同上 | 同上 | 同上 |
| asset byte長 | **`viewer_asset_contract.h`**（`types.h` とstatic_assertで拘束） | 実効authorityとして単一。doc 2件が非拘束の写し | docは値を持たず header を指す |
| member offset表 | **compile済み `offsetof`**（`asset_pool_layout.h`） | production側は既に導出。doc 1件 + `container_test.py` が写し | docは写しをやめる。`container_test.py` は独立検証として**維持** |
| physical qualification authority | `docs/ai/physical-validation-and-rollback.md` + `docs/validation/` + `docs/releases/` | 既に分離 | なし |

原則: **VAMeter-Edu側のdocumentは、受入れの「条件と境界」を述べ、
「値」はsourceを指す。** 値をdocへ書き写すのは、stable v2.0.0 のような
凍結済み歴史記録に限る（それはsourceから消える値だから）。

## 10. Expected file-impact map

第11節の推奨Bを実施する場合に**触れる想定の**ファイル。本監査では変更していない。

| ファイル | 想定変更 | 規模 |
| --- | --- | --- |
| `docs/development/node-viewer-intake.md` | 「本PR」→merged状態への時制更新。byte長・SHA・offset表を `viewer_asset_contract.h` / `.cpp` / `asset_pool_layout.h` への参照へ置換。HOST証拠のsource commit明記（`81226e7b` vs `d4c0702`）。Node適用範囲の3行要約（Firmware不要 / Desktop不要 / Viewer生成は必要）を追記 | 中 |
| `docs/architecture/viewer-assetpool-integration.md` | 同様の時制更新。offset表とbyte長を `offsetof` / header参照へ置換 | 中 |
| `docs/README.md` | document roles に `development/` を追加 | 1-2行 |
| `AGENTS.md` | Main directories に `docs/development/` を追加 | 1行 |
| `docs/ai/README.md` | Documents 一覧に `node-viewer-intake.md` へのlinkを追加 | 1-2行 |
| `docs/development/issue-23-node-viewer-intake-audit.md` | 本書（既に追加済み） | — |

**変更しない**: source、`CMakeLists.txt`、`repos.json`、`sdkconfig`、
`dependencies.lock`、`viewer_asset_contract.*`、`types.h`、`container_test.py`、
`capture-live.js`、Node/npm環境、Viewer/D2B repository。

## 11. Recommended closure/change strategy

**推奨: B — VAMeter側の小さなdocs/config PR一本。その後に #23 を閉じる。**

選択肢の評価:

| 案 | 評価 |
| --- | --- |
| A. 証拠/文書統合の後に #23 を閉じる | 実質Bと同じだが、Bを「統合作業」と呼ぶかの差。Bの完了をもってAを実行する |
| **B. VAMeter側の小さなdocs/config PR一本** | **採用。** 第10節の6ファイル。sourceに触れない。Viewer側の承認状態に依存しない |
| C. Viewer PR + VAMeter docs PR の協調 | 不採用。第5.5節のViewer provenance方針は「self-invalidating provenance commitを避ける」という**意図された設計**であり、Issue #23が変更を要求する筋合いではない。別Issueとして独立に扱うべき |
| D. Node24 builder等価性作業を待って開けておく | 不採用。Issue本文自身が「候補で回帰が出た場合は…Node共通化だけを保留する」と保留経路を定めている。回帰は2026-09-11に観測済み。#23を人質にする理由がない |

Cを不採用とする点の補足: 第5.5節の「final bundleが生成側にtrackedされていない」は
実在する追跡性リスクだが、これは **Viewer repositoryのprovenance方針の論点**であり、
Node共通化範囲の論点ではない。Viewer側Issueとして分離提起し、
#23 の閉鎖条件には含めないことを推奨する。

Node24については **`NODE24_PRODUCT_BUILDER_STATUS = HOLD`** を維持し、
将来の再検討は「Node版数の入替え」ではなく
「webpack/enhanced-resolve入手経路の変更を伴うBuild Environment V2の新規qualification」
として、独立したViewer側Issueで扱うことを推奨する。

**本監査は推奨を実装しない。**

## 12. Cross-Issue conflict assessment

### 12.1 Issue #27（Wi-Fi AP QR / Viewer QR の視覚的区別）

**衝突機序: AssetPool struct layout。** これは軽微ではない。

`app/assets/static_asset_types.h` の `StaticAsset_t` は
`FontPool_t Font; ImagePool_t Image; ColorPool_t Color; TextPool_t Text; WebPagePool_t WebPage;`
であり、`WebPage` は**最後**に位置する。`TextPool_t` / `ImagePool_t` は
固定長配列の構造体である（例: `ImagePool_t::AppEduCurrent_t { uint16_t app_icon[10000]; ... }`）。

したがって #27 が新しいlocalization文字列（`text_pool_{en,cn,jp}.h`）や
新しいicon/border画像（`images/types.h`）を追加すると:

1. `sizeof(TextPool_t)` または `sizeof(ImagePool_t)` が変わる
2. `offsetof(StaticAsset_t, WebPage)` が動く（現在 `1540471`）
3. Viewer 5 memberのoffsetが全て動く（現在 `1625773 / 1626346 / 1627710 / 1630379 / 1660547`）
4. `sizeof(StaticAsset_t)` が変わる（現在 `1660612`）→ layout growth reserveと
   static asset CRC が変わる
5. `asset_pool_layout.cpp:13-14` の `static_assert(kStaticAssetBytes == 1660612U)` と
   `static_assert(sizeof(WebPagePool_t) == 120141U)` は、後者はWebPage内部のみなので
   不変だが、前者は**コンパイルエラーになる**（これは意図された「review and version」gate）
6. `tests/asset_pool_layout/container_test.py:49-52` のhard-coded member表が失効
7. `viewer-assetpool-integration.md` / `node-viewer-intake.md` のoffset表が失効

重なるファイル:

```text
app/assets/localization/text_pool/text_pool_{en,cn,jp}.h
app/assets/localization/text_pool/text_pool_map.h
app/assets/images/types.h
app/assets/static_asset_types.h            （間接: sizeof）
app/libs/asset_pool_layout/asset_pool_layout.cpp   （static_assert 更新必須）
tests/asset_pool_layout/container_test.py
docs/architecture/viewer-assetpool-integration.md
docs/development/node-viewer-intake.md
```

加えてUI側で `app/apps/utils/system/ui/misc/download_qr_page.cpp`、
`app/apps/utils/system/ui/misc/meter_help_qr_view.cpp`、
`app/libs/meter_help_qr/meter_help_qr_urls.cpp`、
`app/apps/app_settings/view/network.cpp` が該当する。

**リスク**: 中〜高。ただし fail-closed 側に倒れる。
`static_assert` と Tier 1 layout guard が先に止めるため、
**silentな不整合にはならない**。危険なのは #27 の実装者が
「static_assertが落ちたので数値を合わせた」だけで済ませ、
layout version と `container_test.py` と doc を更新しない場合である。

**推奨**: 第11節の推奨Bで doc から offset の写しを消すことは、
この衝突面を**先に減らす**。#27 着手前にBを済ませる順序を推奨する。
なお #27 は AssetPool再生成と実機flash・実機表示確認を必須とする（`AGENTS.md`）。

### 12.2 Issue #15（reverse-current indication / latched relay protection）

**#23 との直接衝突は無い。** #15 の中核は
`app/libs/reverse_current_detector/`、`app/libs/current_waveform_clip/`、
`app/apps/app_waveform/view/waveform.cpp`、`app/hal/types.h`、
`platforms/vameter/main/hal_vameter/components/hal_power_monitor.cpp` であり、
Viewer asset / AssetPool layout / Node のいずれにも触れない。

**間接衝突は #27 と同一機序**で存在する。#15 は Issue本文で
`電流の向きが逆です` / `保護のため回路を停止しました。` という新規UI文字列と
「dedicated abnormal screen」を要求する。これらは `text_pool_{en,cn,jp}.h` へ入り、
12.1 の 1〜7 をそのまま発火させる。

重なるファイル（#27 と共通）:

```text
app/assets/localization/text_pool/text_pool_{en,cn,jp}.h
app/assets/localization/text_pool/text_pool_map.h
app/libs/asset_pool_layout/asset_pool_layout.cpp
tests/asset_pool_layout/container_test.py
docs/architecture/viewer-assetpool-integration.md
docs/development/node-viewer-intake.md
```

**リスク**: 低〜中。ただし **#15 と #27 が並行すると両方が `text_pool_*` と
同じ `static_assert` 定数を書き換えるため、AssetPool層でconflictする。**
測定semanticsでは衝突しない。#15 の signed current 保持要件は
`AGENTS.md` の既存contract（plot clippingはHAL値もCSV値も書き換えない）と
同方向であり、#23 とは独立である。

**推奨**: `text_pool_*` と AssetPool layout 定数の更新を、
#15 / #27 のどちらか一方に集約するか、layout更新だけを先行する小PRに切り出す。

## 13. Physical/client work deferred

本監査で**実行していない**もの:

- test実行（host / desktop / ctest / ESP-IDF build）
- Viewer HOST試験（Node18 / Node24 いずれも）
- Viewer product build（V1 / 候補 いずれも）
- AssetPool生成、container A/B生成
- 実機VAMeterへのflash、readback、rollback
- 実機boot、Tier 1 stop path、boot CRC timing、watchdog挙動
- 実機AP / 実ブラウザー / Viewer配信
- Start/Stop、UI/CSV smoke
- Windows-native保存・flush、WebSocket終了・異常切断のHOST確認
- Node/npmのinstall、version変更、PATH変更
- Viewer / D2B repositoryへのpush
- Issue #23 の状態変更、PR作成、merge

上のlistは**本監査sessionの作業範囲**を述べたものであり、projectとしてこれらが
一度も行われていないという主張ではない。実際、merge後に別途、実機VAMeterへの
candidate書込みとdevice-hosted Viewer配信を含むphysical smokeが行われている。
本監査はその記録のauthorityではなく、内容を再現していない。

外部repositoryは anonymous git read による shallow clone のみ行った
（`/home/user/yuichiroh-kobayashi/device-to-browser-viewer`、
`/home/user/yuichiroh-kobayashi/device-to-browser-data-streaming`）。
いずれもcommit/pushしていない。

## 14. Claim boundary

```text
Node touchpoint inventory:        COMPLETE / SOURCE-VERIFIED
Firmware Node dependency:         ABSENT / VERIFIED (build-file不在証明)
Desktop Node dependency:          ABSENT / VERIFIED
Viewer build inside VAMeter-Edu:  ABSENT / VERIFIED (env-var byte copy のみ)
Viewer source d4c0702 / tree:     PASS (本監査で直接一致確認)
p2-builder SHA-256:               PASS (本監査で直接一致確認)
D2B copied-reference tree OID:    PASS (本監査で直接一致確認)
Build Environment V1 digest:      PASS (provenance記載と一致)
NODE24 builder:                   HOLD / NOT RETESTED (2026-09-11記録に依拠)
NODE24 HOST on d4c0702:           NO TRACKED EVIDENCE FOUND
Final bundle 01e39e5c provenance: VAMeter側のみtracked / 生成側未確認
Viewer PR #22/#23 本文:           NOT READABLE FROM THIS SESSION
Host tests / desktop / ESP-IDF:   NOT RUN BY THIS AUDIT
AssetPool generation:             NOT RUN BY THIS AUDIT
Physical VAMeter:                 NOT RUN BY THIS AUDIT
                                  (post-merge physical evidence exists separately;
                                   this audit is not its authority)
Release qualification:            NOT ESTABLISHED
Stable v2.0.0:                    UNCHANGED
Implementation:                   NOT STARTED
```

本書は監査記録であり、product contract でも release authority でもない。
ここに書かれた推奨は、それ自体が実装承認ではない。

ISSUE23_AUDIT_DESIGN_READY_FOR_REVIEW
NODE24_PRODUCT_BUILDER_STATUS = HOLD / NOT_RETESTED
IMPLEMENTATION_NOT_STARTED
