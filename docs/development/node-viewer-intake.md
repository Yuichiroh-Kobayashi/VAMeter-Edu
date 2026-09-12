# Node適用範囲とViewer受入れ

## 目的

Related to #23。VAMeterのFirmware buildへNode依存を追加せず、別repoで生成するViewerの
受入れ条件を明確にする。2026-09-11にsource `4a674f0f3499cb02980964ee24d30ba1dc024d45`
のCMake、app、platforms、tests、開発手順をread-onlyで確認した。
Firmware/DesktopはC++とPythonの経路で、調べたbuild/test呼出しにNode/npm依存はなかった。
ViewerのJavaScriptがassetであることと、Firmware buildにNodeが必要なことは別である。
既存Windows/WSL観測guideの全面改訂は本変更に含めない。

## 準備

[Viewer #16](https://github.com/Yuichiroh-Kobayashi/Device-to-Browser-Viewer/issues/16)
の比較結果と [AssetPool統合仕様](../architecture/viewer-assetpool-integration.md) を読む。
共通候補Node 24.21.0 / npm 11.19.0はViewer HOST試験に採用可能だが、既存builderとの
互換性エラーで製品生成はHOLD。VAMeterにruntime設定やnpm packageは追加しない。

## 操作

1. Viewerの最終clean source commit/tree、D2B入力、builder/toolchainを入力manifestで特定する。
2. 同じ環境の独立2生成からindex、manifest、圧縮CSS/JSの長さ/SHAとbundle IDを受け取る。
3. `app/libs/viewer_asset_contract/` の期待値と `app/assets/web/types.h` のslotを照合する。
4. 採用する場合は期待SHA、route、bundleと必要なlayout変更を別のレビュー可能な差分にする。
   Firmwareと新規AssetPoolを対応づけて生成する。実機への書込みは変更レビュー後の別途承認で行う。

## 確認結果

| 格納対象 | 現行固定slot / bytes | 比較基準生成 / bytes |
|---|---:|---:|
| index | 573 | 573 |
| manifest | 1364 | 1364 |
| CSS gzip | 2385 | 2385 |
| JS gzip | 25809 | 25809 |
| 合計payload | 30131 | 30131 |
| bundle ID（NUL含む） | 65 | 65 |

Viewer比較source `81226e7b39410ac673c1b46a9b76eab6084a4f19` の基準生成bundleは
`be563812df74534c09d15bd67c5015519da3eca551e9e8812f6096a3343a58cb`。
現行Firmwareの期待bundleは
`4422530b6e1ba9549dd4bef2e3bb2c183d8fced49ed2d8d695d2a04a4aa7c2af`。
index/CSS/JSのSHAは一致したが、manifest/bundleは一致しない。slotに収まるだけでは受入れ不可。
今回asset、固定値、Firmware、AssetPool、配布物は更新していない。

## 失敗時の行動

builder失敗、source不一致、長さ/SHA不一致をHOLDとして保存する。丸め、padding、
旧manifest流用やruntime整合性チェックの回避は行わない。
`_copy_viewer_assets()` は正確な長さを要求し、HTTP route登録時にも4assetのSHAとbundleを検証する。
[既存のWindows/WSL観測](wsl-windows-serial-observation.md) で採録するclient runtimeは、
今回のHOST用Node採用とは分ける。Windows版Nodeを使う場合はWindows-native保存先で確認する。

## 終了と保存

取込み元manifest、候補ごとの判定と理由を保存する。実機、AP、serial、機器列挙は未実施。
今回の文書整備は無reset、binary readback、実機配信を証明しない。
後続では最終成果物をレビューし、必要な実機承認と新規入力manifestを準備する。

## Post-v2 provisional candidates — fixed-slot gate

2026-09-13のclassroom UI整理・10秒窓tick・CSV列順更新後のViewer sourceをViewer Build Environment V1で生成した比較。
[Viewer Draft PR #22](https://github.com/Yuichiroh-Kobayashi/Device-to-Browser-Viewer/pull/22)
と [stacked Draft PR #23](https://github.com/Yuichiroh-Kobayashi/Device-to-Browser-Viewer/pull/23)
はいずれも **PROVISIONAL DEVELOPMENT BUILD**。final Firmware intake identityではない。
各clean committed standalone checkoutから同じqualified V1 imageで独立2生成し、
index/manifest/CSS gzip/JS gzipの全bytes一致、各run内two-run determinismもPASS。
Node 24 builderはHOLDのまま。以下は今回の新生成値であり、旧#22/#23候補値は流用しない。

- Viewer #22: source `f3f59e735f5f1b71012b8c3fbc4dcb93da612d59` / tree `450e687aa627716dee92b197d6c1eceed44fa847`。
  bundle `e3b49a78ef1a3e7219f470e18d2123fd381d4463bcde653d011224f8b3dddef8`、stored payload **33717 bytes**。
- Viewer #23: source `8b0d9f7108b990a40bfb93dea402e49c84df169c` / tree `5a3c21bef9ddff3a0844930ac28fe71776a85c15`。
  bundle `0c4d0f38237776274a143a5f3b193adbf3105e88b214b40d4ff195065d420f38`、stored payload **34774 bytes**。

比較slot authorityはVAMeter source `20587054769b4327c037854dee7a75e899e84d0a` の
[`WebPagePool_t`](../../app/assets/web/types.h)、
[expected length/SHA contract](../../app/libs/viewer_asset_contract/viewer_asset_contract.h)、
[exact copyとstatic_assert](../../app/assets/assets.cpp)をread-onlyで再確認した。

| Asset | Current slot | #22 candidate | Delta | Result | #23 candidate | Delta | Result |
| --- | ---: | ---: | ---: | --- | ---: | ---: | --- |
| index | 573 | 573 | +0 | SIZE_FITS | 573 | +0 | SIZE_FITS |
| manifest | 1364 | 1364 | +0 | SIZE_FITS | 1364 | +0 | SIZE_FITS |
| CSS gzip | 2385 | 2669 | +284 | FIXED_SLOT_OVERFLOW | 2669 | +284 | FIXED_SLOT_OVERFLOW |
| JS gzip | 25809 | 29111 | +3302 | FIXED_SLOT_OVERFLOW | 30168 | +4359 | FIXED_SLOT_OVERFLOW |
| bundle ID (NUL含む) | 65 | 65 | +0 | SIZE_FITS | 65 | +0 | SIZE_FITS |

`SIZE_FITS`は長さだけの判定で、identity一致や受入れPASSではない。
両候補のintake分類は **FIRMWARE_INTAKE_BLOCKED**。理由を分離する。

- **FIXED_SLOT_OVERFLOW**: CSS/JS gzipが現行の各fixed arrayを超過する。
  Viewer機能削減、truncate、padding、旧manifest流用で収めない。
- **IDENTITY_UPDATE_REQUIRED**: index/manifestが同じ長さでもSHA・bundleは現行期待値と異なる。
  新候補を取り込むには期待length/SHA、content-hashed routes、bundleと必要なlayoutの
  対応更新が別レビューで必要。現行runtime integrity checkは回避しない。

[partition map](../../platforms/vameter/partitions.csv)のAssetPool領域は2 MiB。
[stable resource記録](../architecture/resource-budget.md)のAssetPool空き441,180 bytesは
partition全体の値であり、上のper-member fixed slotの空きではない。
数KBの増分がpartitionに収まり得ても現行配列に収まらなければintakeはBLOCKED。
新AssetPool全体のサイズ・alignment・offsetは今回生成/検証していない。

## Future layout compatibility and rollback requirement

Before physical deployment of an AssetPool layout change, Firmware and AssetPool
MUST have an explicit fail-closed layout/version/size compatibility guard.

これは将来のlayout remediationに必須の要件であり、今回の実装済み機能ではない。
候補はmagic/version、layout version、exact structure size、または同等のfail-closed
mechanism。方式の決定と実装は次の設計レビューに残す。Viewer asset SHA照合だけで
AssetPool全体の異なる構造offset/sizeに対する互換性が保証されたとは扱わない。

将来のFirmwareとAssetPoolはcompatible pairとして生成・照合・配布する。
rollbackも **old Firmware + old AssetPool** の対応pairへ戻す。片側だけのrollbackを
既定手順にしない。[既存AssetPool統合仕様](../architecture/viewer-assetpool-integration.md)
のmatched deployment境界と、別途実機承認を維持する。

Dedicated static IRAM remaining **1 byte** は別のtechnical debtである。
AssetPool flash layout/fixed-slot超過と混同せず、flash空きをIRAM余裕とみなさない。
今回のViewer開発候補だけでstable v2.0.0 release撤回とは判定しない。

この修正は文書のみ。Firmware source、WebPagePool_t、期待値、route、partition、
AssetPool layout/bytesは変更していない。Firmware/Desktop/AssetPool build、
実機・USB/serial・AP・flash/reset/readback/OTAはNOT RUN。
