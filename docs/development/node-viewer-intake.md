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
