> **HISTORICAL DESIGN DRAFT — NOT IMPLEMENTED — NOT CURRENT PRODUCT AUTHORITY — PAUSED, TRACKED FUTURE REQUIREMENT**
>
> This document is preserved as a historical design draft. It describes an unimplemented
> battery/protection test plan for the internal-excitation concept described in
> [`internal-excitation-measurement-design.md`](internal-excitation-measurement-design.md).
> No hardware, firmware, or current documentation implements this concept, and no test
> described here has been executed against current VAMeter-Edu hardware. It does not define
> current product behavior or serve as a normative contract.
>
> This is a real, still-requested future requirement (science teachers have asked for an
> internally-powered classroom V-I experiment device), not an abandoned idea. It is paused
> pending the large hardware redesign this concept requires. It is tracked in
> [VAMeter-Edu Issue #11](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/11);
> treat the content below as historical design exploration and reference material for that
> Issue, not as a committed specification, and do not begin executing this test plan
> without an explicit task request.
>
> Original path: `docs/vi-logger/operations/battery_and_protection_test_plan.md`

# 電池・保護試験計画ドラフト

ステータス: ドラフト / 未確定 / 未検証

本文書は、次期 VAMeter-Edu の内部電源、アクティブ出力、保護回路を検証するための運用試験計画ドラフトである。
本文書は試験計画であり、回路方式、部品、ファームウェア、CSVスキーマ、UI仕様を確定しない。

## 1. 目的

内部電源付き V-I 実験器 / V-I ロガー / 発熱実験ロガーは、負荷へ電力を供給できるアクティブ出力を持つ。
そのため、電池、出力、負荷、保護、筐体、ログの安全性を教室使用前に段階的に検証する必要がある。

本計画の目的は次の通りである。

- 電池構成ごとのリスクを確認する
- 短絡、過電流、逆接続、電池逆挿入、低電圧、過熱を検証する
- MCU停止、reset、crash、watchdog、boot中の安全側動作を確認する
- firmware-only保護になっていないことを確認する
- Go / NoGO と rollback条件を記録する

## 2. 対象電池候補

標準電源は未確定である。

| 候補 | 公称電圧 | 試験観点 |
|---|---:|---|
| NiMH 3本 | 3.6 V | 満充電電圧、低電圧、負荷時電圧降下、短絡電流 |
| アルカリ / マンガン 3本 | 4.5 V | 新品電圧、内部抵抗差、マンガン電池での電圧降下 |
| NiMH 4本 | 4.8 V | 標準候補としての連続運用、短絡電流、発熱 |
| アルカリ / マンガン 4本 | 6.0 V | 新品電圧上振れ、豆電球・発熱体定格超過リスク |

電池混在、新旧混在、逆挿入は誤使用条件として扱う。

## 3. 試験対象

- 電池ボックスまたは電池保持構造
- 内部励起出力
- 電圧測定経路
- 電流測定経路
- 短絡保護
- 過電流保護
- 逆接続保護
- 外部電圧印加検出
- 電池逆挿入保護
- 低電圧保護
- 過熱保護
- timeout
- fault clear
- CSV記録候補
- QR/ローカル転送前の出力OFF動作

## 4. 試験前提

試験前に確認する。

- 試験対象ハードウェア版
- ファームウェア版またはcommit
- 電池種類、メーカー、状態、使用本数
- 電池電圧
- 負荷種類と定格
- 測定器
- 保護回路の想定動作
- 試験場所と火傷・発煙・短絡への対処

未確認の場合は `未確認`、未検証の場合は `未検証` と記録する。

## 5. 使用測定器候補

- デジタルマルチメータ
- 電流制限付き安定化電源
- 電流プローブまたは直列電流計
- オシロスコープ
- 温度計または熱電対
- 絶縁された試験リード
- 必要に応じてヒューズ付き試験治具

測定器の校正状態は、分かる範囲で記録する。

## 6. 試験実施者と安全境界

Stage 0〜Stage 5 の危険を伴う試験は、生徒参加で行わない。
開発者、または教員管理下で、試験目的、停止条件、緊急対応を理解した者が実施する。

試験環境の要求:

- 非可燃環境で実施する。
- 必要に応じて保護メガネ、耐熱手袋、絶縁された工具を使う。
- 換気を確保する。
- 緊急遮断できる電源経路を用意する。
- 発煙、発火、電池異常時の消火・隔離・換気手順を準備する。
- 無人通電を禁止する。
- 発熱体、豆電球、抵抗器、電池、配線の周辺に可燃物を置かない。

即停止条件:

- 異臭
- 発煙
- 異常発熱
- 電池膨れ
- 端子変色
- 配線軟化
- 樹脂部品の変形
- 保護部品の変色または破損
- 想定外の出力ON

即停止後は、再通電せず、原因、配線、電池状態、保護回路、負荷、測定器を確認して記録する。
教室使用Goは、本試験計画の完了後に別途判断する。
本試験計画を完了しただけで教室使用Goとはしない。

## 7. 試験段階

### Stage 0: 文書・回路レビュー

目的:

- 試験前に危険な構成を排除する。

確認:

- 保護がfirmware-onlyになっていないか。
- MCU停止中でも出力が安全側へ倒れるか。
- 電池逆挿入時に危険な通電が起きない設計か。
- 外部電圧印加時の電流経路が説明できるか。
- 豆電球、抵抗器、発熱体の定格が分かっているか。

NoGO:

- 短絡保護の説明ができない。
- 過電流保護がfirmwareのみに依存している。
- 電池逆挿入時の挙動が説明できない。
- 外部電圧印加時の経路が不明。

### Stage 1: 無負荷・低リスク通電

目的:

- 電池電圧、起動、出力OFF既定、低電圧検出の基本確認。

確認:

- 電源投入時に出力OFF。
- boot中に出力がONにならない。
- reset後に出力OFF。
- 出力ON操作前に負荷端子へ危険な電圧が出ない。
- 電池電圧が測定・記録候補として取得できるか。

NoGO:

- 電源投入またはboot中に出力がONになる。
- reset後に出力がONのまま。
- 電池逆挿入で発熱、異臭、異常電流がある。

### Stage 2: ダミー負荷試験

目的:

- 抵抗負荷で電圧、電流、保護動作を確認する。

確認:

- 固定抵抗の値と許容電力。
- 測定されるV/I/Pの意味。
- 電流制限の動作。
- 最大時間制限。
- timeout後の出力OFF。
- fault clear後に自動ONしない。

NoGO:

- 抵抗の許容電力を超える。
- fault後に自動で出力が再開する。
- 測定パスを説明できない。

### Stage 3: 短絡・過電流試験

目的:

- 短絡時、過電流時に安全側へ倒れることを確認する。

条件:

- 最初は電流制限付き電源または保護付き治具を使う。
- 乾電池だけでの直接短絡試験は、保護設計と試験治具が確認されるまでNoGO。

確認:

- 短絡時の最大電流。
- 遮断時間。
- 発熱。
- 復帰条件。
- firmware停止中でも保護が働くか。

NoGO:

- 保護がMCU firmwareのみに依存している。
- 配線、端子、電池、保護部品が過熱する。
- fault clearなしで自動復帰する。

### Stage 4: 逆接続・外部電圧印加試験

目的:

- 生徒の誤接続に対する耐性を確認する。

確認:

- 負荷端子の逆接続。
- 外部電源を誤って印加した場合の検出。
- 内部電池と外部電圧の衝突経路。
- 保護回路の温度上昇。
- UI通知とログ候補。

NoGO:

- 外部電圧印加で内部電池へ危険な充電経路ができる。
- MCU停止中に逆接続保護が失われる。
- 逆接続を正常測定として扱う。

### Stage 5: 豆電球・発熱体予備試験

目的:

- 実負荷の定格内での動作を確認する。

確認:

- 豆電球の定格電圧・定格電流。
- 発熱体の抵抗値・定格。
- 起動時突入電流。
- 定常電流。
- 温度上昇。
- 最大時間・最大エネルギー制限。

NoGO:

- 定格が不明な豆電球を使う。
- 定格が不明な発熱体を使う。
- 熱的制限なしで連続通電する。
- 豆電球の見かけの抵抗を固定抵抗値として扱う。

### Stage 6: CSV / 転送連携確認

目的:

- アクティブ出力と記録・転送の安全な連携を確認する。

確認:

- QR/ローカル転送前に出力OFF。
- 出力目標ゼロ。
- CSV close。
- ファイルが空でない。
- 長いCSVでチャンク転送が必要か。
- `REC-*.csv` 互換性が壊れていない。

NoGO:

- QR表示中に出力が継続する。
- CSVがopenのまま転送される。
- `REC-*.csv` が生成またはダウンロードできない。

## 8. 記録項目

各試験で記録する。

- 日付
- 試験者
- 試験実施者の立場
- 管理者または立会者
- branch / commit
- ハードウェア版
- ファームウェア版
- 電池種類・本数・状態
- 電池電圧
- 負荷種類・定格
- 測定器
- 配線図または配線説明
- 期待動作
- 実測結果
- 温度
- fault状態
- CSV有無
- Go / NoGO
- 未確認
- 未検証
- 即停止条件の有無
- rollback条件

## 9. REC CSV再検証Gate

チャンク転送を次期ブランチへ移植する前後に、既存波形CSVを確認する。

- 既存波形 `REC-*.csv` が生成できる
- QR / ローカルダウンロードできる
- ヘッダーとデータが壊れていない
- Excel 等で開ける
- 既存UIからの導線が壊れていない

`MO-*.csv` の確認だけではGoにしない。

## 10. rollback条件

以下の場合は試験を停止し、設計または実装を戻す。

- 電源投入時に出力がONになる。
- MCU停止、reset、crash、watchdog、boot中に安全側へ倒れない。
- 短絡または過電流で危険な発熱がある。
- 電池逆挿入で危険な電流または発熱がある。
- 外部電圧印加時の経路が説明できない。
- fault clearなしで出力が再開する。
- `REC-*.csv` 互換性が壊れる。
- 測定値の意味が説明できない。
- 異臭、発煙、異常発熱、電池膨れ、端子変色、配線軟化がある。
- 無人通電が必要な試験条件になっている。

## 11. Open questions

- 標準電池構成をどれにするか。
- 電池ホルダーの逆挿入対策をどうするか。
- 短絡試験用の安全治具をどう作るか。
- 電流制限値を何Aにするか。
- 最大出力時間と最大エネルギーをどう決めるか。
- 過熱検出をどこで行うか。
- 外部電圧印加検出の閾値をどうするか。
- 発熱体試験の安全な容器と設置方法をどうするか。
- 試験実施者、立会者、停止権限をどう運用するか。
- 生徒向けUI通知をどの段階で設計するか。

## 12. Go / NoGO

Go:

- 保護回路と試験治具を準備して段階的に検証する。
- firmware-only保護を禁止する。
- 未確認・未検証を明記する。
- `REC-*.csv` 互換性Gateを維持する。
- 危険を伴う試験を生徒参加ではなく、開発者または教員管理下で実施する。
- 教室使用Goを本試験完了後の別判断として扱う。

NoGO:

- 乾電池を使った直接短絡から試験を始める。
- 保護回路未定のまま教室使用を想定する。
- 生徒参加でStage 0〜Stage 5の危険を伴う試験を行う。
- 無人通電を行う。
- MCU firmwareだけを保護層にする。
- 定格不明の豆電球・発熱体を使う。
- CSVやUI仕様を本試験計画で確定する。
