# VAMeter-Edu PR #1 マージ後引継ぎ・最終レビュー報告

- 更新日: 2026-07-29
- Revision: 9
- Repository: Yuichiroh-Kobayashi/VAMeter-Edu
- 対象PR: #1 fix(recorder): harden classroom waveform recording
- Merge target: dev/vi-logger
- Merge commit: c9bb8ca310b879ca54e83bcea91ff941463d50d1
- 文書種別: dated handoff／validation evidence
- 本文書の基準時点: PR #1 merge後
- 現在仕様の参照: claim typeに応じてmerged code、tests、`docs/vi-logger/`、open Issuesを優先する

本文書は履歴と検証証拠であり、単独で現在実装を規定しない。current implementation behaviorはmerged code/tests、measurement semanticsはcanonical standardsとHAL実装、physical validation statusはvalidation reportと本文書、future workはopen Issue、design rationaleはcanonical architectureを参照する。

V-I logger文書のcanonical rootは`docs/vi-logger/`である。旧`docs/architecture/`、`docs/standards/`、`docs/operations/`の対象fileは互換stubであり、新規内容を追加しない。

## 1. プロジェクト背景と教育上の目的

### 1.1 VAMeter-Eduの位置付け

VAMeter-Eduは、M5Stackのopen-source VAMeter firmwareを基盤とし、中学校理科・技術の電気単元で使用できる教育用V-I loggerへ改造するprojectである。

単に電圧・電流を表示する測定器を作ることではなく、次の学習過程を一つの教材で支えることを目指す。

1. 回路や負荷を実際に動かす
2. 電圧・電流の変化をその場で観察する
3. 必要な区間を生徒自身が明示的に記録する
4. CSVをPC・iPad等へ取り出す
5. 表計算で時間変化をgraph化する
6. 現象と測定値の関係を考察する

対象例は、抵抗、電球、DC motor、USB-C給電などである。
特にmotorの起動・停止や負荷変化のように、瞬間的変化と定常状態が共存する現象を「画面で見る」「記録する」「後から分析する」教材化が重要な狙いである。

### 1.2 学校現場での前提

学校での使用では、研究用測定器とは異なる制約がある。

- 生徒が短時間で操作を理解できること
- 誤操作してもfile消失、format、rebootへ進まないこと
- 記録開始・終了が明確であること
- 1人1台端末や共用PCへ容易にdataを移せること
- cloud accountや専用applicationだけに依存しないこと
- 過去fileと新fileを混在させても安全に扱えること
- 教員が授業前後に容量・採番・file provenanceを確認できること
- 実測値と時間情報の意味を説明できること

このため、PR #1では高機能化よりも、**明示操作、固定時間、安全な失敗、単純なCSV、既存機能との共存**を優先した。

### 1.3 教育的な成功条件

本機能の成功条件は、displayが動くことやCSVが生成されることだけではない。

- 生徒が「いつからいつまで測ったか」を理解できる
- graphのX軸に実時間を使用できる
- voltage-only／current-only／bothの意味が列構造から分かる
- 記録できなかった場合に理由が分かり、dataを勝手に消さない
- 画面上のplot clipが、UIへ届いたHAL値やCSVへ渡す値を追加加工しない
- 表示上のclipと測定dataの保存を区別できる
- 既知のsampling gapを隠さず、教材利用上の限界を説明できる

したがって、最終版は「完全な一定周期logger」ではなく、**低速変化・定常値の観察に使用可能で、時間間隔の不均一性をCSVで確認できる教育用logger**として位置付ける。

---

## 2. PR #1の目的・scope・非scope

### 2.1 PR #1で解決したかった問題

PR #1の中心課題は、既存Waveform recorderを授業で安全に使える状態へhardeningすることだった。

初期scope:

- manual triggerによる明示的な記録開始
- 10秒固定記録
- `REC-[0-9]+.csv`の安全な生成・採番
- SettingsからFiles appへの導線
- REC fileの個別削除と確認画面
- local AP経由のCSV download
- path traversalを防ぐbasename validation
- CSV全体をRAMへ載せないstreaming
- 容量不足時の安全な記録拒否
- auto-delete／format／rebootを行わない
- recorder errorをrecoverableにする
- legacy `MO-*` fileとの共存
- recorder taskとtriggerのlifetime安全化
- sampleごとの実時間timestamp

### 2.2 PR #1の最終scope

実機試験とUI reviewを経て、最終的に次をmergeした。

- manual trigger・10秒固定
- staged Auto scale
- single-channelの教育用波形表示
- mode別3列CSV
- `elapsed_ms`の実測timestamp
- trigger ownership／stop-timeout safety
- storage preflightとrecoverable error
- REC／MOの役割分離
- Files app／local AP download
- current／legacy CSV preview compatibility
- host testsと設計文書

### 2.3 PR #1で完了していないもの

次はPR #1の完了条件に含めない。

- 一定周期25 Hzのsampling保証
- FAT write latencyの完全分離
- calibrationの永続保存
- `cal_store`再統合
- internal excitation measurementの実装
- battery／protection hardwareの実機検証
- HelpEvent transport本体
- fault injection網羅
- CI構築
- `dev/vi-logger`から`main`への正式統合判断
- Motor Observe／MAKER-DRIVE関連機能の統合

これらをPR #1へ混在させず、Issueまたは別branchで扱う。

### 2.4 Branch scopeの意味

- `main`
  - 安定baseline
  - primary worktree専用
- `dev/vi-logger`
  - V-I logger製品定義、測定意味論、保護・内部励起設計、PR #1を含む開発branch
- `dev/local-csv-streaming`
  - PR #1の履歴参照用branch
  - final treeはsquash merge commit `c9bb8ca`と同一
- `edu-dev`
  - legacy/reference branch
  - 標準worktree構成には含めない
  - hostによってlocal branchやupstream状態が異なる可能性がある
  - 明示的な調査依頼がない限りcheckout・merge・更新しない
  - Motor Observe／MAKER-DRIVE等の旧scopeを現在のPR scopeへ自動的に混入させない

後任は「PR #1がmerge済み」であることと、「`dev/vi-logger`全体を`main`へ採用済み」であることを混同しないこと。

---

## 3. 設計方針と判断基準

### 3.1 Education-first

機能数より、授業中に迷いにくい操作を優先する。

- recordingはencoder clickで開始
- recording時間は10秒固定
- vertical scaleは手動選択ではなくstaged Auto scale
- side short clickは将来のHelp request hook
- side long holdは終了
- `state_saving`中はshort click／long holdを無視し、Helpへ進まずWaveformを終了しない
- TIME／manual `M`表示や複数の補助値を削り、live valueを中心にする

manual scaleは一度実装・検証したが、操作対象切替、表示領域、認知負荷が増えた。
最終的には「生徒がscaleを操作する教材」よりも、「現象を観察し記録に集中する教材」を優先してAuto scaleへ簡略化した。

### 3.2 Measurement semantics first

CSVの値と時刻の意味を曖昧にしない。

- `elapsed_ms`はsample indexから合成しない
- `esp_timer_get_time()`の相対値を保存する
- scheduler／FAT I/Oによるdelayを隠さない
- voltage-only／current-onlyでは非対象列を空欄にする
- both-modeだけ両列を記録する
- plot clipはUIへ届いたHAL値やCSV値を書き換えない。ただしHAL測定pathには後述のLC/HC固有処理がある
- 旧summary row、capacity、energyはcurrent schemaから外す

この方針によりsampling stallが可視化された。
「見栄えのよい等間隔data」へ補間するより、実際に取得できた時刻を保存することを優先する。

### 3.3 Fail-safe and data preservation

error時に自動回復を装ってdataを失わない。

- 64 KiB free-space preflight
- insufficient storageでは記録拒否
- auto-deleteなし
- formatなし
- rebootなし
- file creation／write失敗はrecoverable error
- stop timeout中はtriggerとrecorder statusを保持
- recorder task終了前にresourceを解放しない
- second recorderを拒否
- repeated Destroyでdouble freeしない

「授業を止めない」ことを、rebootや自動削除で実現しない。
状態を残し、教員または生徒がFiles画面から確認できることを優先する。

### 3.4 Compatibility over forced migration

既存fileを一括変換・削除しない。

- current recordは`REC-*`
- legacy Motor Observe recordは`MO-*`
- `MO-*`は削除対象として残すが、REC採番・preview・downloadへ混入させない
- preview parserは旧5列、旧2列、旧summary row、新3列を安全に読む
- malformed／empty／overlong rowはsafe skip

### 3.5 Security and bounded resources

local downloadは小規模教材でも通常のfile securityを維持する。

- percent decodeは1回
- decoded basenameをstrict validation
- fixed record directoryとのみ結合
- path traversalを拒否
- 8 KiB fixed buffer
- file全体をRAMへ読み込まない
- zero-byte fileを扱う
- read／send／final chunk failureを成功扱いしない

### 3.6 Reproducibility and provenance

firmware、AssetPool、CSV、dependencyの由来を分けて管理する。

- sourceはGit
- 測定CSVはLabData
- firmware／AssetPoolはArtifacts
- operation logとpatchはArchive
- binary／CSVをGitへcommitしない
- clean buildとdevice-flashed exact binaryを区別する
- provenance不明binaryは`legacy-unverified`
- AssetPoolはfirmwareと整合するsourceから生成する
- dependencyは各worktreeで正式取得し、他worktreeへsymlinkしない

---

## 4. 開発経緯と仕様変更の理由

### 4.1 初回PR

初回主要commit:

```text
4e09375dd6985d9149362a01433b139f0841e032
fix(recorder): harden classroom waveform recording
```

初期実装は、manual 10秒記録、REC管理、Files app、local download、storage safetyを中心としていた。

### 4.2 第1回reviewで判明した問題

初期差分は次の理由でNoGoとなった。

1. stop timeout時にrecorder taskが参照中のtriggerをview側がdeleteする可能性
2. error文言の文字欠け・画面幅超過
3. 実機検証内容とdocument checklistの不整合

この段階で、UI改善より先にresource ownershipと停止時安全性を確定する方針とした。

### 4.3 Motor実機試験で分かったこと

直流安定化電源と減速機付き130 motorによる試験で、次が明確になった。

- CSVに時刻列がなく、時間軸graphを作れない
- motor起動peakが画面範囲外になる
- 約5 Vの変化が既存AUTO表示では小さく見える
- 表示scaleの意味を生徒へ説明しにくい

これを受け、resource lifetimeとtimestampを先に修正した。

```text
a1f1becb24cf985ad50a6bcf7740cc9cb42b2a6e
fix(recorder): preserve lifetime and add sample timestamps
```

主な変更:

- trigger ownershipをHALへ移譲
- stop timeout時のresource保持
- task終了後の1回だけの解放
- sampleごとの`elapsed_ms`
- legacy preview compatibility
- 短いrecoverable error表示

### 4.4 Manual scale prototype

次に、純正firmwareに近い手動scale操作を試作した。

```text
748a1a72553aedf9e0c06485d8fea2b892f47228
feat(waveform): add educational vertical scale controls
```

prototypeでは次を含んだ。

- TIME／V-SCALE／I-SCALEの操作対象切替
- encoderによるvertical scale選択
- voltage／current／both mode
- AUTO／manual scale
- `value/div`表示
- voltage 0 V基準
- current 0 A中心
- out-of-range clip
- view再生成後の設定保持

### 4.5 Readability review

manual scale prototypeは、次の表示問題により再度NoGoとなった。

- selected文字が白背景に白文字で不可視
- scale readoutが波形に上書きされる
- 非ASCII `μA`が残る
- current source build確認が必要

次のcommitで可読性を改善した。

```text
55b93c6a80266792ab7bf5575071f64c94e218d1
fix(waveform): improve scale control readability
```

### 4.6 最終的な簡略化

その後の実機操作と教育用途の再検討により、manual scale UIを最終仕様には残さない判断をした。

最終PR head:

```text
90f2ac1e08d68c8888a73729fc4dcb8d7494f2bc
refactor(waveform): simplify educational waveform recording
```

最終変更:

- manual V／I scale選択を削除
- staged Auto scaleへ統一
- TIME readoutを削除
- manual trigger `M` iconを削除
- single-channel panel端のbuffer max／min表示を削除
- centered live valueを維持
- side short clickをHelp request hookへ戻す
- CSVを`voltage,current,elapsed_ms`の3列へ簡略化
- capacity／energy／summary rowを削除

これは機能後退ではなく、**授業中の操作負荷を減らし、測定と考察へ集中させるための仕様収束**である。

### 4.7 実機受入・merge

最終版は次で実機受入した。

- 130 motor端子電圧
- motor電流4回
- 100 Ω抵抗電流
- USB-C both-mode 2回
- REC-000～REC-007の連続採番
- MSC取得
- mode別空欄
- 3列schema
- timestampの単調増加
- 約10秒終了
- staged Auto scale画面

PR #1は`dev/vi-logger`へsquash mergeされた。

```text
PR head:
90f2ac1e08d68c8888a73729fc4dcb8d7494f2bc

merge commit:
c9bb8ca310b879ca54e83bcea91ff941463d50d1
```

### 4.8 Merge後に判明した性能課題

8 CSVの解析で、通常40 msに加えて次を確認した。

- 周期的な約196～272 ms gap
- 約5秒地点の547～632 ms gap
- 10秒あたり40～55 sample相当の不足

原因はsample取得と同期FAT write／chunk rotationを同じtaskで行う構造と考えられる。

timestampはdelayを正しく記録しており、data破損ではない。
このためmergeは維持し、Issue #3としてproducer-consumer化またはRAM一括保存を検討する。

## 5. 最終判定

PR #1は`dev/vi-logger`へマージ済みである。

根拠:

1. PR #1はGitHub上でmergedとなり、merge commit `c9bb8ca310b879ca54e83bcea91ff941463d50d1`が`dev/vi-logger`の先頭になった。
2. recorder triggerの寿命、容量不足時の安全動作、REC/MO分離、local downloadのpath validationはレビュー済み。
3. staged Auto scale、単一channel波形UI、3列CSVが実機で動作した。Help hookの診断logは明確な実機証拠が残っていないため未確認とする。
4. 8件の連続REC採番に成功した。
5. 電圧専用・電流専用・USB-C both-modeの列出力が仕様どおりである。
6. 全CSVで`elapsed_ms`が0から始まり、単調増加し、約10秒で終了している。
7. MSC modeからhost PCへ8ファイルを取得できた。
8. ユーザーによる画面遷移・表示・記録の受入判定はGoである。

ただし、CSV解析から**同期FAT書込みに起因すると考えられるsampling stall**が明確になった。
これはPR #1のデータ破損や安全性の問題ではなく、記録時刻が実時間を正しく表しているため今回の開発ブランチ統合を妨げない。
一方、一定周期サンプリングを標榜する前に高優先度のfollow-upとして解消する。

---

## 6. 最終実機試験

### 6.1 試験構成

| REC | 測定画面 | 測定対象 |
|---|---|---|
| REC-000 | 電圧波形 | 130モータ端子間電圧 |
| REC-001 | 電流波形 | 130モータ巻線電流1回目 |
| REC-002 | 電流波形 | 130モータ巻線電流2回目 |
| REC-003 | 電流波形 | 130モータ巻線電流3回目 |
| REC-004 | 電流波形 | 130モータ巻線電流4回目 |
| REC-005 | USB-C電力 | ThinkPad USB-C充電電流・画面修正前 |
| REC-006 | 電流波形 | 100Ω金属皮膜抵抗 |
| REC-007 | USB-C電力 | ThinkPad USB-C充電電流・画面修正後 |

全ファイルはMSC modeでhost PCへ取得した。

### 6.2 CSV構造・測定範囲

全8ファイルのheader:

```csv
voltage,current,elapsed_ms
```

| REC | Mode | Sample rows | End elapsed_ms | Voltage range | Current range | 最大sample間隔 | 40 ms基準の行数不足 |
|---|---|---:|---:|---|---|---|---:|
| REC-000 | 電圧波形 | 211 | 10011 | 0～6.64 V | 空欄 | 608 ms（5031→5639） | 40 |
| REC-001 | 電流波形 | 206 | 10025 | 空欄 | 0～0.3018 A | 616 ms（5035→5651） | 46 |
| REC-002 | 電流波形 | 206 | 10037 | 空欄 | 0.0471～0.0957 A | 618 ms（5020→5638） | 46 |
| REC-003 | 電流波形 | 205 | 10028 | 空欄 | 0.0699～0.0801 A | 632 ms（5008→5640） | 47 |
| REC-004 | 電流波形 | 207 | 10009 | 空欄 | 0.0477～0.0969 A | 547 ms（5020→5567） | 44 |
| REC-005 | USB-C電力 | 197 | 10030 | 0.0088～20.2763 V | 0～2.9832 A | 613 ms（5006→5619） | 55 |
| REC-006 | 電流波形 | 206 | 10033 | 空欄 | 0.0041175～0.004125 A | 624 ms（5021→5645） | 46 |
| REC-007 | USB-C電力 | 197 | 10030 | 0.0162～20.3 V | 0～2.9847 A | 622 ms（5003→5625） | 55 |

### 6.3 仕様適合結果

- REC-000:
  - voltage列のみ入力
  - current列は全行空欄
- REC-001～004、REC-006:
  - current列のみ入力
  - voltage列は全行空欄
- REC-005、REC-007:
  - internal both-modeとしてvoltage/current両列を入力
- 全ファイル:
  - summary rowなし
  - capacity/energy列なし
  - first `elapsed_ms = 0`
  - current manual 40 ms／no-pretrigger受入pathで`elapsed_ms`はstrictly increasing
  - final elapsedは10009～10037 ms
  - filenameはREC-000からREC-007まで連続

判定: **CSV schema、mode別空欄、timestamp、採番、MSC取得はPASS**

### 6.4 測定値の妥当性確認

- 130モータ端子電圧:
  - 0～6.64 V
- 130モータ電流:
  - 1回目peak 0.3018 A
  - 2～4回目は概ね0.047～0.097 A
- 100Ω抵抗:
  - 4.1175～4.1250 mA
  - 安定負荷として再現性が高い
- ThinkPad USB-C:
  - 約20.1～20.3 V
  - 最大約2.98 A
  - both-mode出力を確認

測定対象の違いがCSV値へ合理的に反映されている。

---

## 7. sampling intervalに関する新規知見

### 7.1 nominal interval

`TriggerBase`のdefault sample intervalは40 ms。

全CSVでも通常区間のmedian stepは40 msである。

### 7.2 5秒地点の共通gap

全8ファイルで、約5秒地点に547～632 msのgapがある。

例:

```text
REC-003:
5008 ms → 5640 ms
gap = 632 ms
```

recorder taskはsample loop内部で5秒ごとに次を同期実行している。

```text
fclose(current chunk)
open(next chunk)
```

約5秒地点のgapはchunk rotationの実行位置と整合しており、fclose／fopenのblockingが主因である可能性が高い。
ただし、個別I/O時間の直接traceは未取得である。

### 7.3 その他の周期的gap

各CSVには約196～272 msのgapも周期的に存在する。

40 ms周期なら約251～252行が期待されるところ、実測は197～211行であり、40～55 sample相当が不足する。

直接の計測traceはないが、recorder taskがsample取得と`fprintf`相当のFAT書込みを同じthreadで同期実行しているため、libc/FAT buffer flush時のblockingが主因である可能性が高い。

### 7.4 意味

`elapsed_ms`は実際のdelayを隠さず記録しているため、timestampの実装は正しい。
しかし、timestamp間の測定値は存在しない。

したがって現状は:

- 低速変化・定常値の授業観察: 使用可能
- 一定周期25 Hz loggerとしての保証: 不可
- 短時間transientの定量解析: gap内のeventを取り逃がす可能性あり
- Excel graph:
  - category chartではなく、`elapsed_ms`をX値にしたXY散布図を使用する

---

## 8. 高優先度follow-up Issue

推奨title:

```text
Recorder sampling stalls during synchronous FAT writes
```

### 8.1 問題

- nominal: 40 ms
- recurring stall: 約196～272 ms
- chunk boundary stall: 約547～632 ms
- 10秒あたり40～55行相当不足
- sample取得とFAT書込みが同じtask

### 8.2 推奨設計

producer-consumerへ分離する。

Recorder/sampler task:

- `esp_timer_get_time()`でtimestamp取得
- voltage/currentをRAM ring bufferへ格納
- FAT I/Oを行わない
- overflowを明示的に検出

Writer task:

- RAM bufferからblock単位で読み出す
- FATへまとめてwrite
- close/openやflush latencyをsamplingから分離
- stop時に残bufferをdrainしてfinalize

代替案:

- 10秒分をPSRAM/RAMへ保持し、記録終了後に一括保存
- 10秒・約250 sample・3列なら必要RAMは小さい
- storage不足preflightとstop-timeout安全設計は維持

### 8.3 受入基準案

- 10秒記録で245行以上
- 連続sample gap:
  - 通常80 ms以下
  - 5秒地点に特別なgapを作らない
- current manual 40 ms／no-pretrigger受入pathでtimestamp strictly increasing
- voltage/current/both modeを維持
- storage full・I/O errorでrebootしない
- stop timeout時のtrigger ownershipを維持
- producer buffer overflowをsilent lossにしない

このIssueは**release quality向上のため高優先度**とするが、PR #1の`dev/vi-logger`統合を取り消す理由とはしない。

---

## 9. マージ済み機能

### 9.1 教育用波形画面

- manual triggerのみ
- 10秒固定記録
- encoder clickで記録開始
- encoder rotationはhorizontal time zoom
- side short clickはHelp request hook
- side long holdでexit
- `state_saving`中はside short／longを無視し、Help遷移・exitを行わない
- manual vertical scaleなし
- staged Auto scale
- TIME表示なし
- manual trigger `M` iconなし
- single-channelではpanel左右端のbuffer max/min表示なし
- centered live valueは維持

表示回帰基準:

```text
Rec error
See Files

No space
Delete
in Files

削除できません
```

240×240 display内へ収め、unsupported glyphを追加しない。microampはASCII `uA`を使う。localization／asset変更時はAssetPool再生成の要否を確認し、文言は実機表示で検証する。

### 9.2 Voltage Auto scale

```text
0.1, 0.2, 0.5, 1.0, 2, 5 V/div
```

- start: 0.1 V/div
- positive peakをvisible buffer全体から取得
- scale up: `peak >= 8 × current scale`
- scale down: `peak < 4 × lower scale`
- zero baseline: Y=200
- 4-div label: Y=120
- 8-div label: Y=40
- readout center: Y=220
- single-channel plotのclipはUIへ届いたHAL値やCSV値を書き換えない

### 9.3 Current Auto scale

```text
0.1mA, 0.2mA, 0.5mA, 1mA, 2mA, 5mA,
10mA, 20mA, 50mA, 100mA, 0.2A, 0.5A, 1A /div
```

- start: 100 uA/div
- ASCII `uA`
- positive range優先
- application-level `shuntCurrent`とCSV `current`はA単位
- screenとCSVは同じ処理済みHAL `shuntCurrent`を起点とし、CSVは追加補正しない
- `_pm_data_a_scale = 1000`はchart内部の数値精度用であり、calibration・shunt係数・単位補正ではない
- high-current reverse pathはnegative currentを保持可能
- low-current pathはoffset適用後のnegative valueを既存HALが`0 A`へclamp
- 「全rangeのnegative raw currentがCSVへ残る」とは表現しない
- single-channel plotのclipはUIへ届いたHAL/CSV値を追加変更しない
- 1000倍補正、INA226 calibration、LC/HC切替、offset、shunt係数をUI観察から推測で変更しない

### 9.4 USB-C both-mode

- internal `mode_both`
- voltage/current両列をCSVへ記録
- USB-C Power MonitorからWaveformへ進むとこのmodeになる
- combined screenはlegacy voltage/current chart
- voltage/currentそれぞれのmin/mid/max-following labelsを維持
- single-channel staged Auto-scale受入scope外
- manual vertical-scale controlなし
- TIME/V/I `/div` readoutなし
- encoder rotationはhorizontal zoom
- side short clickはHelp hook、side long holdはexit（saving中は両方無視）
- panel-edge max/min label削除はsingle-channel限定
- REC-005/007でboth-mode記録成功

---

## 10. CSV仕様

### 10.1 Current schema

```csv
voltage,current,elapsed_ms
```

Voltage-only:

```csv
<voltage>,,<elapsed_ms>
```

Current-only:

```csv
,<current>,<elapsed_ms>
```

Both:

```csv
<voltage>,<current>,<elapsed_ms>
```

- summary rowなし
- capacityなし
- energyなし
- elapsedはsample indexから合成しない
- `esp_timer_get_time()`相対値
- scheduler／storage delayを隠す補間・書換えなし
- current manual 40 ms／no-pretrigger受入pathでは先頭0 ms付近かつstrictly increasing
- CSV format全般としてmillisecond値の一意性を無条件保証しない
- faster samplingまたはpretrigger導入時はduplicate timestamp policyを別途設計する
- current pretrigger codeは複数sampleを`0 ms`で出力しうるため、将来有効化時に無視しない

### 10.2 Legacy compatibility

preview parserは次を継続して読む。

- `voltage,current,time,capacity,energy`
- `voltage,current,elapsed_ms,capacity,energy`
- `voltage,current,elapsed_ms`
- legacy 2-column sample
- previous 5-column sample
- old summary row
- voltage-only/current-only/both row
- extra columns
- malformed/empty/overlong rowをsafe skip

---

## 11. Recorder安全設計

- triggerはsuccessful creation後にHALが所有
- stop timeout時にtriggerを解放しない
- recorder task終了前のreleaseを拒否
- timeout中のsecond recorderを拒否
- repeated Destroyでdouble freeしない
- 64 KiB free-space preflight
- auto deleteなし
- formatなし
- insufficient storageでrebootなし
- recoverable error
- final file作成失敗時のcleanup

---

## 12. Files・download

- current record: `REC-[0-9]+.csv`
- ASCII digits only
- numbering: max REC id + 1
- gap reuseなし
- legacy `MO-*`:
  - deleteのみ
  - REC numberingへ影響しない
  - preview/download対象外
- SettingsからFiles appを起動
- individual delete confirmation
- local AP download
- percent decodeは1回
- basename validation
- path traversal防止
- 8 KiB streaming buffer
- zero-byte handling
- read/send/final chunk errorをsuccess扱いしない

---

## 13. Build・test状態

PR #1開発中の検証に加え、merge commit`c9bb8ca310b879ca54e83bcea91ff941463d50d1`からpost-merge clean buildを実施した。

post-merge reference build source:

```text
~/Dev/worktrees/VAMeter-Edu/dev-vi-logger
branch: dev/vi-logger
HEAD: c9bb8ca310b879ca54e83bcea91ff941463d50d1
```

reference build環境での最終確認:

- dependency 7件: expected URL／HEAD／tagまたはbranch／cleanを確認
- exact snapshot: `docs/ai/dependency-baseline.md`
- cross-worktree dependency symlink: 0件
- desktop configure: PASS
- desktop build: PASS
- ctest: 7/7 PASS
- ESP-IDF: v5.1.6
- ESP-IDF clean build: 1632/1632 steps PASS
- firmware link: PASS
- application binary file size: 1,722,672 bytes
- application image size: 1,722,557 bytes
- app partition: `0x200000`
- free: `0x5b6d0` bytes（18%）
- partition overflow: なし
- `platforms/vameter/CMakeLists.txt`: tracked diffなし
- `platforms/vameter/sdkconfig`: tracked diffなし
- `platforms/vameter/dependencies.lock`: tracked diffなし
- repository status: clean

別の開発環境でも、同一branch／SHAへの同期、標準worktree作成、dependency 7件の取得・厳密検証までPASSしている。
その環境ではbuild／test／flashは実施していない。

2026-07-29のPR #1 merge時点ではGitHub Actionsは未構成。

## 14. Artifact・LabData

旧follow-up worktreeは削除済みであり、今後artifact参照にworktree pathを使用しない。

標準保存root:

```text
Artifact:
~/Artifacts/VAMeter-Edu/c9bb8ca310b879ca54e83bcea91ff941463d50d1

LabData:
~/LabData/VAMeter-Edu/2026-07-29_motor-usbc-validation
```

これらはhostごとの`$HOME`配下へ配置する。
複製する場合はchecksumを正とし、Git repositoryへ入れない。

### 14.1 AssetPool

正式保存先:

```text
assetpool/AssetPool-VAMeter.bin
```

- size: 1,625,400 bytes
- SHA256:
  `a21a4e2e1d6ead03de2c483aeb33c6a0e9b3b25b2f7e32ac7b95b76616b3ef83`
- source/destination `cmp`: PASS
- Gitへcommitしない

### 14.2 clean-build

post-merge clean buildを次へ保存した。

```text
clean-build/
metadata/clean-build.md
```

application binary:

```text
clean-build/vameter_user_demo.bin
SHA256:
05ec4f60e3afbef2e96f0a6bc6646ccbf4524910edd2cabd9f83bf85d36ec281
```

これは`c9bb8ca`のclean rebuildであるが、実機へflashしたexact binaryの証明ではない。

### 14.3 legacy-unverified

開発中に生成された`g55b93c6-dirty`由来のbinaryは、clean HEAD buildと認定せず次へ限定保存した。

```text
legacy-unverified/
metadata/legacy-unverified.md
```

回帰確認・来歴参照専用とし、正式release binaryとして扱わない。

### 14.4 Checksums

Artifact rootの`checksums.txt`:

```text
20/20 PASS
```

### 14.5 LabData

保存内容:

- `REC-000.csv`～`REC-007.csv`
- `README.md`
- `SHA256SUMS`

検証:

```text
8/8 PASS
```

測定CSV、AssetPool、firmware binaryはいずれもGit tracked fileではない。

## 15. Git/worktree引継ぎ

### 15.1 全開発環境で使用する標準構成

Primary:

```text
~/Dev/VAMeter-Edu
branch: main
upstream: origin/main
status: clean
ahead/behind: 0/0
HEAD: current origin/mainと一致
```

正式dev worktree:

```text
~/Dev/worktrees/VAMeter-Edu/dev-vi-logger
branch: dev/vi-logger
upstream: origin/dev/vi-logger
status: clean
ahead/behind: 0/0
HEAD: current origin/dev/vi-loggerと一致
```

通常時の登録worktreeは上記2件とする。
新規Issue作業は次の形式で追加する。

```text
~/Dev/worktrees/VAMeter-Edu/issue-<number>-<slug>
```

Primaryではfeature開発を行わず、`main`専用・cleanを維持する。

### 15.2 旧PR branch

旧PR用linked worktreeは不要であり、登録・filesystem pathとも残さない。

branchは履歴参照用として保持する。

```text
local branch:
dev/local-csv-streaming

remote-tracking branch:
origin/dev/local-csv-streaming

HEAD:
90f2ac1e08d68c8888a73729fc4dcb8d7494f2bc

ahead/behind:
0/0

local-only commit:
0
```

`90f2ac1`はsquash merge commit `c9bb8ca`のancestorではないが、complete tree diffは空である。

### 15.3 Dependency

各dev／Issue worktreeのrepository rootで次を実行する。

```bash
mkdir -p platforms/vameter/components
python3 ./fetch_repos.py
```

`repos.json`記載7件を各worktree内のreal directoryとして取得する。

merge commit `c9bb8ca` clean-build時のexact SHA snapshotは`docs/ai/dependency-baseline.md`を参照する。これはimmutable lockではなく検証証拠である。mutable branchがsnapshotから移動した場合は自動採用・自動rewindせずreviewする。

原則:

- 他worktreeをdependency storeとして使用しない
- cross-worktree symlinkを作成しない
- scriptのexit statusだけでなくURL／HEAD／tagまたはbranch／cleanを検証する
- partial取得時にsymlinkや別versionで補完しない
- primaryにあるdependencyをdev worktreeへ流用しない
- dependency setup自体は確立済みだが、`fetch_repos.py`のfailure propagation、existing repository validation、idempotent化、lock化は未完了

## 16. 現在状態の確認コマンド

以下はuser名やhost名に依存しない共通確認手順である。

```bash
PRIMARY="$HOME/Dev/VAMeter-Edu"
DEV="$HOME/Dev/worktrees/VAMeter-Edu/dev-vi-logger"

git -C "$PRIMARY" fetch --prune origin

echo "---- primary ----"
git -C "$PRIMARY" status -sb
git -C "$PRIMARY" rev-parse HEAD
git -C "$PRIMARY" rev-list --left-right --count \
  HEAD...origin/main

echo "---- dev ----"
git -C "$DEV" status -sb
git -C "$DEV" rev-parse HEAD
git -C "$DEV" rev-list --left-right --count \
  HEAD...origin/dev/vi-logger

echo "---- worktrees ----"
git -C "$PRIMARY" worktree list --porcelain
git -C "$PRIMARY" worktree prune --dry-run --verbose

echo "---- cross-worktree symlinks ----"
find "$DEV" -type l \
  -not -path "$DEV/build/*" \
  -not -path "$DEV/platforms/vameter/build/*" \
  -print
```

期待:

```text
primary:
  branch main
  HEAD equals origin/main
  ahead/behind 0 0
  clean

dev:
  branch dev/vi-logger
  HEAD equals origin/dev/vi-logger
  ahead/behind 0 0
  clean

worktrees:
  primaryとdevの2件
  prunable entryなし

cross-worktree symlink:
  なし
```

Artifact／LabDataを配置したhostでは次も確認する。

```bash
LABDATA="$HOME/LabData/VAMeter-Edu/2026-07-29_motor-usbc-validation"
ARTIFACT="$HOME/Artifacts/VAMeter-Edu/c9bb8ca310b879ca54e83bcea91ff941463d50d1"

(
  cd "$LABDATA"
  sha256sum -c SHA256SUMS
)

(
  cd "$ARTIFACT"
  sha256sum -c checksums.txt
)
```

## 17. マージ後の未完了項目

### High priority

1. Issue #3 `perf(recorder): decouple sampling from synchronous FAT writes`
   - <https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3>
2. 実CSVを使ったXY scatter graph教材手順

### Normal priority

1. Issue #2 `feat(calibration): complete and reintegrate persistent cal_store`
   - <https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/2>
2. HelpEvent transport本体
   - short-click diagnostic hookの実機log evidenceも未確認
3. long CSV multi-chunk download
4. client disconnect/reconnect
5. AP suffixの実機回帰確認
6. recorder stop-timeout injection
7. mid-stream I/O fault injection
8. CI導入
9. `fetch_repos.py`の検証・idempotent化

---

## 18. 最終CSV SHA256

- `REC-000.csv`: `6d53a29cb839ea0666eb631970947577a75601e7d7742db74b4b13c6c6a42ef3`
- `REC-001.csv`: `35142a1c22fa06c3ce8d195b908fd953deeeb11ee6fdc6bc039de12d05979b50`
- `REC-002.csv`: `e5fe6ed70f8eacd97519f71e12e80ea3e78cb87c8f181c13bb3587798d8aad3e`
- `REC-003.csv`: `addfcf9229a9d7cf12e55e33df21744fc23a668524391772c1a4e36267c1ae3e`
- `REC-004.csv`: `e7bdd7ff37c6abef31aeb0daa33843cf7d612f866a7bf404b5621cf4f4b6fa81`
- `REC-005.csv`: `3702e24deed0b4aac161d93a2eca593c0825d7b88be7392f712965eabfdd9b47`
- `REC-006.csv`: `bea405cef4a17041349627ea85dee603f6e2787d830b968c13048d24a58b3df3`
- `REC-007.csv`: `deaae9d0e287e5219af51b2334df70245c0109ce14feb58f72c799a5b50517ad`

---

## 19. 次担当者への要点

1. PR #1は`dev/vi-logger`へマージ済み。
2. PR headは90f2ac1、squash merge commitはc9bb8ca。`dev/vi-logger`の現在HEADは、その後の文書commit等により進む可能性がある。
3. 90f2ac1とc9bb8caはGit ancestry上の親子ではないが、complete tree diffは空である。
4. 8件の実機CSVでschema、mode別列、timestamp、採番、MSC取得を確認済み。
5. Auto-scale画面はユーザー受入Go。
6. USB-C both-modeも実記録済み。
7. trigger lifetimeとstorage safetyは解決済み。
8. `elapsed_ms`は正確だが、recording taskにFAT I/O stallがある。
9. 約5秒地点に0.55～0.63秒の共通gapがある。
10. 40 ms周期相当では40～55行不足する。
11. この性能問題はIssue #3として高優先度follow-up。
12. uniform 25 Hz loggerとはまだ表現しない。
13. 授業で時間軸を扱う場合はXY scatterを使う。
14. 各開発環境のprimaryは`main`専用・cleanとする。
15. active developmentは正式`dev/vi-logger` worktreeで行う。
16. 旧PR worktreeは作成しない。`dev/local-csv-streaming` branchだけ保持。
17. dirty CMake、loose handoff patch、cross-worktree dependency symlinkは解消済み。
18. AssetPool・clean build・legacy buildはArtifact rootから参照する。
19. 実機CSVはLabDataから参照する。
20. Artifact／LabDataを別hostへ複製する場合はchecksumを検証する。
21. Issue #2／#3の実装開始は別途明示判断する。

## 20. `main`と`dev/vi-logger`のマージ後差分

### 20.1 Branch heads

2026-07-29のGitHub確認結果:

```text
main:
4492da1fe1278ee30eae5f8194291344287a8cc2
refactor: remove unfinished cal_store component from main

dev/vi-logger:
c9bb8ca310b879ca54e83bcea91ff941463d50d1
fix(recorder): harden classroom waveform recording (#1)

merge base:
4492da1fe1278ee30eae5f8194291344287a8cc2
```

Branch relation:

```text
dev/vi-logger is ahead of main by 6 commits
dev/vi-logger is behind main by 0 commits
status: ahead
```

`main`の先頭そのものがmerge baseであり、`main`側だけに存在するcommitはない。
したがって履歴上は、`main`を`dev/vi-logger`へfast-forward可能な状態である。
ただし、後述のscopeと既知課題を確認せずにmainへ統合しない。

### 20.2 File-level diff

`main...dev/vi-logger`の差分:

```text
42 files changed
4,137 additions
782 deletions
```

この差分は、PR #1だけではない。

#### PR #1由来

```text
35 files
2,859 additions
779 deletions
```

主なscope:

- 教育用波形記録
- staged Auto scale
- mode別3列CSV
- recorder trigger lifetime
- storage safety
- Files app導線
- REC/MO互換
- local AP CSV download
- host tests
- recorder／download設計文書

#### PR #1以前から`dev/vi-logger`に存在する差分

```text
7 files
1,278 additions
3 deletions
```

対象:

以下の旧pathは当時のbranch diffを記録するhistorical evidenceである。現在のcanonical文書は`docs/vi-logger/`にあり、旧path自体は互換stubである。

- `README.md`
- `README_ja.md`
- `docs/architecture/internal_excitation_measurement_design.md`
- `docs/architecture/vi_logger_product_definition.md`
- `docs/operations/battery_and_protection_test_plan.md`
- `docs/standards/measurement_semantics_policy.md`
- `platforms/vameter/CMakeLists.txt`

内容:

- READMEのESP-IDF前提を5.1.6へ更新
- V-I loggerの製品定義
- 内部励起測定の設計
- 電池・保護試験計画
- 測定意味論policy
- ESP-IDF component探索から
  `${CMAKE_SOURCE_DIR}/../../components`
  を削除

したがって、`dev/vi-logger`を`main`へ統合する操作は「PR #1だけをmainへ入れる」のではなく、上記product/design documentationとbuild-path変更も同時にmainへ入れる。

### 20.3 `platforms/vameter/CMakeLists.txt`と依存取得

`dev/vi-logger`では次の探索pathが削除済み。

```cmake
${CMAKE_SOURCE_DIR}/../../components
```

これはrepository root直下の`components/`参照を削除する変更であり、`repos.json`が配置する`platforms/vameter/components/`とは別である。
ESP-IDF project root配下の`components/`は標準探索対象になる。

正式な依存取得手順は、各clean worktreeのrepository rootで次を実行すること。

```bash
python3 ./fetch_repos.py
```

`repos.json`は依存を次へ配置する。

```text
dependencies/
platforms/vameter/components/
```

正式運用:

- 各worktreeで`fetch_repos.py`を使用
- 他worktreeへのdependency symlinkを作らない
- dirty treeをdependency storeとして使わない
- tracked CMakeへ個人PC固有pathを書かない
- dependencyの取得URL・path・requested refはrepos.jsonで管理
- c9bb8ca clean-build時の検証済みexact SHAは`docs/ai/dependency-baseline.md`で記録
- mutable branchはimmutable lockではない
- `dependencies.lock`、`sdkconfig`の意図しない差分をcommitしない

現行`fetch_repos.py`は既存directoryの検証やupdateを行わず単純cloneするため、新規worktreeで一度だけ実行する。idempotent化は今後の改善候補とする。

### 20.4 `main`統合判断

Git履歴上の統合条件:

- conflictなし
- `main`側の独自commitなし
- fast-forward可能

一方、品質・scope上の未決事項:

1. synchronous FAT writeによるsampling stall
2. fetch_repos.pyのfailure propagation、既存directory検証、idempotent化が未完了
3. `dev/vi-logger`に含まれるproduct/design文書4件をmainの正式方針として採用するか
4. `platforms/vameter/CMakeLists.txt`のcomponent探索path削除をmainへ正式採用するか

現時点の推奨:

```text
PR #1 -> dev/vi-logger: merged and accepted
dev/vi-logger -> main: technically fast-forwardable, but decision pending
```

`main`を安定branchとして扱う場合、少なくともsampling stallのIssue化と、検証済みdependency snapshotおよびfetch helperの既知制約を確認してからmain統合を判断する。

---

## 21. PR #1 merge直後のGit・環境snapshot

以下は2026-07-29、docs PR作成前のsnapshotであり、current remote HEADの期待値ではない。

PR #1:

```text
state: closed
merged: true
head: 90f2ac1e08d68c8888a73729fc4dcb8d7494f2bc
merge commit: c9bb8ca310b879ca54e83bcea91ff941463d50d1
merged at: 2026-07-29 10:03 JST
base: dev/vi-logger
```

Remote基準状態:

```text
origin/main:
4492da1fe1278ee30eae5f8194291344287a8cc2

origin/dev/vi-logger:
c9bb8ca310b879ca54e83bcea91ff941463d50d1

origin/dev/local-csv-streaming:
90f2ac1e08d68c8888a73729fc4dcb8d7494f2bc

origin/main...origin/dev/vi-logger:
0 6
```

複数の開発環境で次の標準状態を確認済み。

```text
primary:
  main / 4492da1 / clean / origin同期

dev worktree:
  dev/vi-logger / c9bb8ca / clean / origin同期

retained branch:
  dev/local-csv-streaming / 90f2ac1 / origin同期
```

共通確認結果:

- registered worktree: 2件
- old PR worktree: なし
- prunable entry: なし
- dependency 7件: PASS
- cross-worktree symlink: なし
- local-only commit: 0
- commit／push／branch削除／remote write: なし

一方のreference環境ではdesktop testとESP-IDF clean buildまでPASSした。
別環境ではGit／worktree／dependency同期までPASSし、build／test／flashは実施していない。

## 22. 標準開発環境構成・運用原則

### 22.1 標準directory layout

```text
~/Dev/VAMeter-Edu/                           # main専用primary
~/Dev/worktrees/VAMeter-Edu/dev-vi-logger/   # 現行開発
~/Dev/worktrees/VAMeter-Edu/issue-<n>-<slug>/
~/LabData/VAMeter-Edu/<date>-<purpose>/
~/Artifacts/VAMeter-Edu/<commit-or-release>/
~/Archive/VAMeter-Edu/<date>-<operation>/
```

`~`は各hostのuser homeを表す。
絶対pathやmachine固有user名を引継ぎ条件にしない。

### 22.2 Branch／worktree方針

1. primaryは`main`専用・常時clean
2. `dev/vi-logger`は専用linked worktreeでcheckout
3. feature作業はIssue単位のlinked worktree
4. 同一branchを複数worktreeで同時checkoutしない
5. merge済みfeature worktreeはartifact保全後に削除
6. branch削除は別途明示判断する
7. branch移動やmergeは必ず対象pathを`git -C`で明示する
8. fast-forward対象branchを実行前後に再確認する

### 22.3 Dependency方針

1. dependencyは各worktreeで`python3 ./fetch_repos.py`
2. parent directoryだけを事前作成してよい
3. tracked gitlinkの空directoryは削除せず整合を確認する
4. 他worktreeへのcomponent symlinkは禁止
5. URL／HEAD／tagまたはbranch／cleanを全件検証
6. partial cloneを自動削除・別versionで補完しない

### 22.4 Data／Artifact方針

1. 測定CSVは`~/LabData/`
2. build／flash provenanceは`~/Artifacts/`
3. Git repositoryへCSV／binaryを追加しない
4. checksum fileを同梱する
5. clean buildとdevice-flashed exact binaryを区別する
6. provenance不明binaryは`legacy-unverified`として分離する

### 22.5 Cleanup／安全原則

1. build directoryはdisposable
2. dirty変更はcommit／Issue／明示discardへ収束
3. loose patchは一時backupであり恒久保管場所にしない
4. `reset --hard`、force worktree removal等は通常禁止
5. 例外操作はexact SHA、clean gate、実行回数を明示する
6. destructive操作前にlocal-only commit、checksum、worktree mappingを確認
7. remote write、branch削除、Issue／PR変更は別scopeとして扱う

## 23. `cal_store` Issue

GitHub Issuesを有効化し、未完成`cal_store`を正式に記録した。

```text
Issue:
#2 feat(calibration): complete and reintegrate persistent cal_store

URL:
https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/2

State:
open

Created:
2026-07-29 11:30 JST
```

未完成`cal_store`はcommit
`4492da1fe1278ee30eae5f8194291344287a8cc2`
で`main`から削除された。

Issue #2では次を記録している。

- 削除前のNVS／CRC／double-buffer設計要素
- 現行`measurement_pipeline`はraw値を返す状態
- 削除前codeをそのまま復元しない方針
- measurement semanticsとrange/probe設計を先に確定すること
- host test、power-loss、CRC/version migration、実機校正を受入条件とすること
- sampling stall改善は非対象であること

## 24. Recorder sampling Issue

実機CSVで確認したFAT書込み起因のsampling stallを高優先度Issueとして作成した。

```text
Issue:
#3 perf(recorder): decouple sampling from synchronous FAT writes

URL:
https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3

Priority:
High

State:
open

Created:
2026-07-29 11:31 JST
```

Issue #3に記録した主な観測値:

- nominal sample interval: 40 ms
- 通常区間のmedian: 40 ms
- 約5秒地点のgap: 547～632 ms
- その他の周期的gap: 約196～272 ms
- 10秒記録: 197～211 samples
- 40 ms周期の期待値: 約251～252 samples
- 不足: 40～55 samples相当

推奨設計:

1. sampler producer taskとFAT writer consumer taskを分離
2. または10秒分をRAM/PSRAMへ保持し、記録後に一括保存

維持条件:

- manual trigger・10秒固定
- mode別3列CSV
- 実測`elapsed_ms`
- trigger lifetime
- 64 KiB storage preflight
- no reboot／no format／no auto-delete
- recoverable I/O error
- legacy preview互換
