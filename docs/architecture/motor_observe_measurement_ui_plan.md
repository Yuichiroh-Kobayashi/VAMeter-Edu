# Motor Observe Measurement UI Plan

## Motor Observe development freeze for constant-force device work

Motor Observe on VAMeter-Edu is fixed at the low-voltage development-validation stage for the constant-force device project.

Reason:

- VAMeter cannot be used as a standalone measurement device for polarity-reversing H-bridge motor terminal voltage.
- Bidirectional motor winding current measurement requires an additional suitable current-sensing device.
- The tested 130-size gearmotor showed start/run hysteresis and mechanical loss that make PWM/current-based force estimation unsuitable for a constant-force device.
- The constant-force device project should move to a direct-drive motor platform with torque-command capability.

Motor Observe remains useful as:

- a low-voltage motor observation prototype
- a safety-state / CSV logging / QR transfer reference
- a measurement-path documentation case study

Do not continue VAMeter-Edu Motor Observe toward classroom force-control use without a separate design review.

## Scope

この文書は、Motor Observe の次段階として作る計測用UIの計画である。

- 今回は docs-only plan である。
- 今回はコード実装しない。
- classroom release UI ではない。
- 通常 launcher 登録しない。
- assets / localization を追加しない。
- 既存 waveform `REC-*.csv` の仕様を変更しない。

Motor Observe bring-up UI、device側 `VAMeterMotorObservePwmBackend`、開発用 `MO-*.csv` logger、QR download 経路は既存実装として扱う。

一方で、実電流測定、測定経路の正式確定、負荷・stall・thermal behavior、current limit 条件、classroom-safe fixture は未検証である。生徒用UI化は、測定経路と安全条件が確定してから判断する。

## Current Baseline

現在の baseline は次の通り。

| Item | Status |
|---|---|
| Motor Observe bring-up UI | 実装済み |
| `MOTOR_OBSERVE_BRINGUP_AUTOSTART=1` 限定起動経路 | 実装済み |
| 通常 launcher 登録 | 未実装 / 入れない |
| assets / localization | 未追加 |
| device側 `VAMeterMotorObservePwmBackend` | 実装済み |
| 開発用 `MO-*.csv` logger | 実装済み |
| `MO-*.csv` QR download | chunked streaming化済み / 長めのCSVは実機再確認が必要 |
| Local CSV download transport | 固定長bufferのchunked streaming方式 |
| storage初期化後の `MO-000.csv` 作成 | 確認済み |
| `MO-000.csv` QR download | 確認済み |
| QR後の `MO-001.csv` 新規セッション開始 | 確認済み |
| 既存 `REC-*.csv` 仕様変更 | しない |
| GPIO9候補 | M1A / Port.A C9 White |
| GPIO8候補 | M1B / Port.A C8 Yellow |
| GPIO10 | measurement path relay制御専用 (PWMは使わない) |
| Base relay | measurement path relayとして使う (安全遮断として使わない) |
| High/High coast | 未実装 / 入れない |
| PWM/High / High/PWM | 未実装 / 入れない |
| motor terminal measurement by VAMeter | NoGO |

PWM出力方針は維持する。

| Condition | Output |
|---|---|
| `SafeDisabled` | Low-Low |
| `OutputArmed` | Low-Low |
| `Fault` | Low-Low |
| timeout | Low-Low |
| leaveMode | Low-Low |
| target 0 | Low-Low |
| target > 0 | GPIO9候補 PWM / GPIO8候補 Low |
| target < 0 | GPIO9候補 Low / GPIO8候補 PWM |

measurement path relay は安全遮断ではない。Motor Observe bring-up では測定経路を閉じるために使う。

- `OutputArmed` で relay ON。
- `OutputEnabled` で relay ON。
- `SafeDisabled` / `Fault` / timeout / leaveMode / QR download / onDestroy で relay OFF。
- `physical_output_allowed` は PWM 出力許可の意味を維持する。

relay 状態表:

| State / Event           | relay                  | PWM output            | target | output_pattern      |
| ----------------------- | ---------------------- | --------------------- | ------ | ------------------- |
| startup before begin    | OFF                    | OFF                   | 0      | LOW_LOW             |
| SafeDisabled            | OFF                    | OFF                   | 0      | LOW_LOW             |
| OutputArmed             | ON                     | OFF                   | 0      | LOW_LOW             |
| OutputEnabled target 0  | ON                     | OFF                   | 0      | LOW_LOW             |
| OutputEnabled target >0 | ON                     | GPIO9 PWM / GPIO8 Low | >0     | GPIO9_PWM_GPIO8_LOW |
| OutputEnabled target <0 | ON                     | GPIO9 Low / GPIO8 PWM | <0     | GPIO9_LOW_GPIO8_PWM |
| brake stop              | OFF after SafeDisabled | OFF                   | 0      | LOW_LOW             |
| Fault                   | OFF                    | OFF                   | 0      | LOW_LOW             |
| timeout                 | OFF                    | OFF                   | 0      | LOW_LOW             |
| QR download             | OFF                    | OFF                   | 0      | LOW_LOW             |
| leaveMode / onDestroy   | OFF                    | OFF                   | 0      | LOW_LOW             |

## Purpose

Motor Observe 計測用UIの目的は、開発者または計測者が小型DCモータの制御指令と測定値を同時に観察し、後から説明できる記録を残すことである。

- `target_percent`、`safety_state`、`output_pattern`、V/I/P を同時に確認する。
- 測定経路を明示したうえで CSV 記録する。
- requested target と applied target を分けて記録する。
- 測定値が何を表すかを `measurement_path_code` と semantics で説明できるようにする。
- `measurement_path_code=unknown` の V/I/P を教材判断に使わない。
- VAMeter単体で正逆に極性が反転するモータ端子電圧を測らない。
- 生徒用UI化は、測定経路、安全治具、負荷・stall・thermal behavior の確認後に判断する。

このUIは、測定の意味を確定するための開発者向け・計測者向けUIであり、授業で生徒が直接使うUIではない。

## UI Stages

### Stage 0: bring-up UI

既存状態。

- 開発者用。
- `MOTOR_OBSERVE_BRINGUP_AUTOSTART=1` の autostart 限定。
- launcher 未登録。
- assets / localization なし。
- CSV logger あり。
- QR download あり。
- `measurement_path_code` は初期値 `unknown`。
- `measurement_path_code=unknown` の V/I/P は教材判断に使わない。

### Stage 1: measurement-ready UI

次に作る候補。

- まだ生徒用ではない。
- 計測者が測定経路を選ぶ、または明示する。
- `driver_input_inline` を最初の正式測定経路候補にする。
- V/I/P を画面表示する。
- CSV に測定経路を明記する。
- storage low / CSV status を表示する。
- QR download へ安全に遷移する。
- QR download 前に `SafeDisabled / target 0 / Low-Low` を保証する。
- QR後は既存CSVへ追記せず、新規 `MO-*.csv` セッションを開始する。
- motor terminal voltage/current pathはStage 1対象外にする。

### Mechanical observation fields

Stage 1 measurement-ready UI may need to record mechanical observation separately from electrical measurement.

Candidate fields:

| Field | Meaning |
|---|---|
| motor_mechanical_type | bare_motor / gearmotor / unknown |
| load_condition | no_load / fixture_load / unknown |
| rotation_observed | stopped / rotating / stalled / unknown |
| start_threshold_pwm_percent | PWM where rotation starts from standstill |
| keep_running_min_pwm_percent | minimum PWM where rotation continues after starting |
| stall_pwm_percent | PWM where rotation stops during ramp-down |

These fields are not electrical measurements.

CSV V/I/P values alone cannot determine whether the motor is rotating. Rotation state must be observed by a person or measured by an external sensor.

### Stage 2: classroom UI

後回し。

- 教材用表示。
- 用語・表示の簡略化。
- 生徒が誤操作しにくい操作導線。
- 測定経路、安全治具、current limit 条件、storage policy が確定してから検討する。
- classroom release 判定前に、負荷・stall・thermal behavior を実測する。

## Measurement-Ready Screen Layout

Stage 1 の画面では、狭い画面でも安全状態、出力状態、測定経路、CSV状態が崩れないことを優先する。

表示候補:

| Field | Meaning | Priority |
|---|---|---:|
| `Safety State` | `SafeDisabled` / `OutputArmed` / `OutputEnabled` / `Fault` | 1 |
| `Output State` | physical output allowed / disabled | 1 |
| `Requested Target` | UI操作で要求された target [%] | 1 |
| `Applied Target` | backendへ実際に適用された target [%] | 1 |
| `Output Pattern` | `LOW_LOW` / `GPIO9_PWM_GPIO8_LOW` / `GPIO9_LOW_GPIO8_PWM` | 1 |
| `Relay` | measurement path relay state | 1 |
| `Measurement Path` | `driver_input_inline` など | 1 |
| `CSV File` | 現在の `MO-*.csv` | 1 |
| `CSV Status` | `READY` / `STORAGE LOW` / `OPEN FAIL` など | 1 |
| `Voltage [V]` | 測定経路で定義された電圧 | 2 |
| `Current [A]` | 測定経路で定義された電流 | 2 |
| `Power [W]` | 測定経路で定義された電力 | 2 |
| `Storage Status` | 空き容量または warning | 2 |
| `Motor Supply` | `battery_2aa` / `bench_limited` など | 3 |
| `Load Condition` | `no_load` / `fixture_load` / `unknown` など | 3 |
| warning message | `NOT FOR CLASSROOM USE` など | 1 |

画面が狭い場合の優先順位:

1. safety と出力: `Safety State`、`Output State`、`Requested Target`、`Applied Target`、`Output Pattern`
2. 記録の意味: `Measurement Path`、`CSV File`、`CSV Status`
3. 測定値: `Voltage [V]`、`Current [A]`、`Power [W]`
4. 運用条件: `Storage Status`、`Motor Supply`、`Load Condition`
5. 補足警告: warning message

`Fault`、`CSV: STORAGE FULL`、`CSV: WRITE FAIL`、`CSV: CLOSE FAIL` は通常表示より優先して出す。

## Operation Flow

Stage 1 の最小操作フロー案:

1. 起動する。
2. CSV begin で新規 `MO-*.csv` セッションを開始する。
3. 測定経路を確認する。最初の正式候補は `driver_input_inline` とする。
4. `SafeDisabled` を表示し、target 0 / Low-Low を確認する。
5. `OutputArmed` へ進む。物理出力はしない。
6. `OutputEnabled target 0` へ進む。target 0 / Low-Low を維持する。
7. target +10 を適用する。
8. target 0 に戻す。
9. target -10 を適用する。
10. brake stop で `SafeDisabled / target 0 / Low-Low` に戻す。
11. QR download へ入る。
12. QR中は出力制御とCSV追記を止める。
13. QRから measurement UI へ戻る。
14. 新規 `MO-*.csv` セッションを開始する。

QR downloadの注意:

- driver_outputにモータを接続した試運転では、CSV記録は進んでいるように見えたが、QR downloadで`Memory allocation failed`が出た。
- 原因仮説は、download endpointがCSV全体をRAMへ読む全量malloc方式だったことである。
- Local CSV downloadは固定長bufferのchunked streaming方式へ変更済みである。
- `MO-*.csv`と既存`REC-*.csv`のdownload path制限を維持し、任意ファイルdownloadを許可しない。
- 長めの`MO-*.csv` downloadと既存`REC-*.csv` downloadは実機で再確認する。

操作割当案:

| Operation | Stage 0 current behavior | Stage 1 proposal |
|---|---|---|
| 右スイッチ短押し | QR download | QR download。入る前に `SafeDisabled / target 0 / Low-Low`、CSV close、0 byte guard |
| エンコーダ長押し | `SafeDisabled` から `OutputArmed` | 同じ。測定経路未確認なら警告を表示し、教材判断不可を維持 |
| エンコーダ短押し | `OutputArmed` から `OutputEnabled`、`OutputEnabled` で brake stop、`Fault` で clear | 同じ。brake stop は必ず `SafeDisabled` へ戻す |
| エンコーダ回転 | `OutputEnabled` のみ target +/-10 | 同じ。`OutputArmed` では target 変更しない |

Stage 1 で測定経路選択UIを入れる場合、target操作より前に測定経路確認を完了させる。測定経路が `unknown` のままでも開発ログは続けられるが、画面とCSVに `unknown` を明示し、教材判断に使わない。

## Measurement Path Design

最初の正式候補は `driver_input_inline` とする。

| measurement_path_code | 扱い |
|---|---|
| `driver_input_inline` | 最初の正式候補 |
| `driver_input_voltage_only` | 電圧のみ |
| `motor_terminal_voltage` | VAMeter単体ではNoGO |
| `motor_side_inline` | VAMeter単体ではNoGO |
| `unknown` | 教材判断に使わない |

### Observation-only path: motor_output_to_vameter_input_open_output

A hardware check connected the MAKER-DRIVE motor output to the VAMeter input port while leaving the VAMeter output port unconnected.

This is not a valid current measurement path.

Use only as a relay / signal observation record. Do not use V/I/P values from this wiring for instructional judgment.

If such wiring must be represented later, use an explicit code such as:

- `observation_motor_output_input_only`

and keep semantics as `unknown`.

### `driver_input_inline`

| Item | Plan |
|---|---|
| 測定経路名 | MAKER-DRIVE driver input inline |
| 配線説明 | VAMeter を MAKER-DRIVE の VB+ / VB- 入力側電源経路に入れ、ドライバ入力側の電圧、入力電流、入力電力を観察する候補 |
| 測っている値 | driver input-side voltage / current / power 候補 |
| 測っていない値 | モータ巻線電流、PWM瞬時波形、モータ端子電圧 |
| CSV上の意味 | `voltage_semantics=driver_input_voltage_v`、`current_semantics=driver_input_current_a`、`power_semantics=driver_input_power_w` の候補 |
| UI表示での注意文 | `driver input value / not motor winding current` |
| 教材判断に使ってよい条件 | 配線、電源、負荷条件、VAMeter表示値、CSV値、参考測定器の結果を同じログに記録し、current / thermal / load 条件が Go になった場合 |
| NoGO条件 | 配線説明ができない、電流が過大、発熱・異臭・異音、storage failure、`measurement_path_code=unknown`、driver input current を motor winding current として扱う必要がある場合 |

Observed development result:

- `driver_input_inline` with gearmotor no-load produced coherent driver input-side voltage/current/power.
- Relay OFF / SafeDisabled was approximately 0 A.
- Relay ON / OutputArmed was approximately 25 mA.
- PWM operation increased driver input current, but this is still not motor winding current.
- Use this path as the next measurement-ready UI target.

### Other Paths

| measurement_path_code | 測っている値 | 測っていない値 | 注意 |
|---|---|---|---|
| `driver_input_voltage_only` | driver input-side voltage 候補 | current / power / motor winding current | 電圧のみ。電流・電力の教材判断に使わない |
| `motor_terminal_voltage` | NoGO | motor winding current、driver input current | H-bridge output polarity reverses. Do not use VAMeter as the measuring instrument. Use oscilloscope/differential probe instead. |
| `motor_side_inline` | NoGO | driver input current | Requires separate bidirectional/isolated current measurement design. |
| `unknown` | 未確認 | すべて教材判断不可 | bring-up baseline。V/I/Pを教材判断に使わない |

NoGO条件:

- 実電流測定結果がない。
- `driver_input_inline` での V/I/P 挙動が説明できない。
- current limit 条件が未定義。
- stall または thermal behavior が未検証。
- classroom-safe fixture が未検証。
- 測定経路をUIまたはCSVに残せない。

## CSV Extension Plan

現行 `motor_observe_csv_v0.1` は 24 列である。既存24列の順序変更は禁止する。

既存列:

```text
schema_version,session_id,sample_index,t_ms,t_s,event,safety_state,physical_output_allowed,
requested_target_percent,applied_target_percent,output_pattern,gpio9_duty_percent,
gpio8_duty_percent,measurement_path_code,voltage_semantics,current_semantics,power_semantics,
voltage_v,current_a,power_w,power_source,pwm_waveform_captured,abnormal_flag,note
```

Stage 1 で追加が必要な場合、末尾列追加のみとする。`REC-*.csv` には追加しない。

末尾追加候補:

| Column | Meaning |
|---|---|
| `motor_supply_code` | `battery_2aa` / `bench_limited` / `unknown` など |
| `load_condition` | `no_load` / `fixture_load` / `stall_test` / `unknown` など |
| `storage_status` | `ready` / `low` / `full` / `unknown` など |
| `csv_status` | `ready` / `open_fail` / `write_fail` / `close_fail` / `empty` など |
| `ui_mode` | `bringup` / `measurement_ready` |
| `firmware_commit` | commit識別子。未確認なら `unknown` |
| `build_flags` | `MOTOR_OBSERVE_BRINGUP_AUTOSTART=1` など |

`measurement_path_code`、`voltage_semantics`、`current_semantics`、`power_semantics` は既存24列に含まれているため、Stage 1 では値の注入方針を定義する。列順序は変えない。

CSV互換性ルール:

- 既存24列の順序変更は禁止。
- 既存列名の意味変更は禁止。
- 列追加は `MO-*.csv` の末尾のみ。
- `REC-*.csv` のヘッダ、reader、保存形式は変更しない。
- UI表示文字列をCSV値のソースにしない。
- HAL/internal numeric measurement values を使う。
- `HIGH_HIGH`、`PWM_HIGH`、`HIGH_PWM` を記録しない。

## Storage Plan

storage問題が再発したため、Stage 1 UI は storage 状態を表示する。ただし、最初に実装すべきは warning only である。

段階案:

1. warning only
2. manual cleanup only
3. MO-only generation cleanup
4. classroom-safe policy

### Step 1: warning only

最初に実装する。自動削除、自動format、自動eraseは入れない。

候補表示:

- `CSV: READY`
- `CSV: STORAGE LOW`
- `CSV: STORAGE FULL`
- `CSV: OPEN FAIL`
- `CSV: WRITE FAIL`
- `CSV: CLOSE FAIL`
- `CSV: EMPTY`

warning only の実装候補:

- CSV begin 前または失敗時に storage free / available の取得を試す。
- 取得できない場合は `Storage Status: unknown` とする。
- 低容量しきい値は未確定。最初は conservative な warning とし、動作停止条件とは分ける。
- `OPEN FAIL`、`WRITE FAIL`、`CLOSE FAIL` は safety 優先で `SafeDisabled / target 0 / Low-Low` へ戻す。

### Prohibited Storage Behavior

禁止:

- 自動format。
- 自動erase。
- `REC-*.csv` 自動削除。
- 設定ファイル削除。
- classroom releaseでの勝手な cleanup。
- storage異常を安全確認済みとして扱うこと。

manual cleanup、MO-only generation cleanup、classroom-safe policy は、実測ログと運用設計がそろってから別レビューにする。

## Safety Requirements

Stage 1 UI は既存 safety / backend 構造を迂回しない。

- `SafeDisabled` / `OutputArmed` で物理出力しない。
- target 0 は Low-Low。
- brake stop は `SafeDisabled` へ戻す。
- QR download 前は `SafeDisabled / target 0 / Low-Low`。
- QR中は出力制御とCSV追記を止める。
- QR後は新規CSVセッション。
- `Fault` / timeout / leaveMode は Low-Low。
- GPIO10はPWM/方向制御に使わない。measurement path relay制御に限定する。
- Base relayはmeasurement path relayとして使うが、安全遮断として使わない。
- High/High coastは入れない。
- PWM/High / High/PWMは入れない。
- current limit をUIで示す場合、測定値ソース、threshold、hysteresis、停止挙動、復帰操作、検証ログが必要。
- 未検証の current limit 条件を classroom-safe と表示しない。

QR前後の最小安全手順:

1. requested target を 0 にする。
2. backend applied target を 0 にする。
3. safety state を `SafeDisabled` にする。
4. output pattern を `LOW_LOW` にする。
5. `stop` row を書く。
6. CSV を close する。
7. close 後に file size / basename guard を確認する。
8. QR page へ渡す。
9. QR page 中は Motor Observe update と CSV append を止める。
10. 戻ったら新規 `MO-*.csv` セッションを開始する。

## Implementation Phases

Reuse -> Make の順で進める。

### Phase A: docs-only plan

今回。

実施内容:

- measurement-ready UI の段階設計を文書化する。
- 未確認 / 未検証を断定しない。
- `driver_input_inline` を最初の正式候補として扱う。
- storage warning と safety gate を計画に含める。

検証手順:

- docs差分だけであることを `git diff --stat` で確認する。
- `REC-*.csv` 仕様変更やコード変更がないことを確認する。

Go条件:

- docsのみ変更。
- Stage 0 / 1 / 2 が分かれている。
- 測定経路の未確定を断定していない。
- safety要件と禁止事項が明記されている。

NoGO条件:

- コード変更が入る。
- classroom UI として断定する。
- 自動削除や自動formatを提案する。

撤退条件:

- 計画が既存 safety / backend 方針と矛盾する場合、計画を差し戻す。

### Phase B: measurement path code injection

`driver_input_inline` を CSV に入れる最小修正。

Reuse:

- 既存 `MO-*.csv` logger。
- 既存 `measurement_path_code`、`voltage_semantics`、`current_semantics`、`power_semantics` 列。

Make:

- Stage 1 UI または build-time / settings-like source から `driver_input_inline` を設定する最小経路。
- `unknown` へ戻せる手段。

検証手順:

- host-side Motor Observe test。
- desktop build。
- device build。
- 実機で `MO-*.csv` を作成し、`measurement_path_code=driver_input_inline` が入ることを確認。
- 配線記録と CSV を同じ operation log に残す。

Go条件:

- 既存24列の順序が変わらない。
- `REC-*.csv` が変わらない。
- `driver_input_inline` の意味がUIとCSVで一致する。
- `motor_terminal_voltage` / `motor_side_inline` を有効化しない。

NoGO条件:

- 測定経路をUI表示せずにCSVだけ変える。
- `driver_input_inline` の V/I/P を motor winding current として扱う。
- VAMeter単体のmotor terminal measurementを復活させる。

撤退条件:

- 測定経路選択が誤操作を誘発する場合、`unknown` 固定に戻す。

### Phase C: measurement value display

HAL numeric V/I/P をUI表示する。

Reuse:

- 既存 HAL power monitor data。
- 既存 CSV の `voltage_v`、`current_a`、`power_w`。

Make:

- `Voltage [V]`、`Current [A]`、`Power [W]` の表示。
- semantics と measurement path の併記。

検証手順:

- desktop build。
- device build。
- 実機で `driver_input_inline` 配線を記録し、表示値とCSV値を比較。
- 参考測定器がある場合、電源電圧・入力電流の傾向を比較。

Go条件:

- UI表示値とCSV recorded value の関係を説明できる。
- `driver_input_inline` で測っている値と測っていない値が表示または文書で明確。
- motor terminal voltage/currentは表示対象外であることが明確。

NoGO条件:

- 表示文字列からCSV値を作る。
- VAMeter表示値をPWM瞬時波形として扱う。
- 実測前に教材判断へ進む。
- VAMeter単体でH-bridge出力を測る前提に戻す。

撤退条件:

- 表示値とCSV値の関係が説明できない場合、V/I/P表示を `未確認` 扱いに戻す。

### Phase D: storage warning

storage low / full の warning only。

Reuse:

- 既存 CSV status。
- 既存 storage diagnostics。
- 既存 QR download guard。

Make:

- `CSV: STORAGE LOW`、`CSV: STORAGE FULL` 表示。
- `OPEN FAIL` / `WRITE FAIL` / `CLOSE FAIL` の視認性改善。

検証手順:

- desktop build。
- device build。
- 通常容量、低容量、CSV open fail 相当のログを確認。
- QR前後で `SafeDisabled / target 0 / Low-Low` を維持することを確認。

Go条件:

- warning only。
- 自動削除・自動formatなし。
- `REC-*.csv` や設定ファイルに触れない。

NoGO条件:

- 自動format、自動erase、自動削除。
- `REC-*.csv` cleanup。
- storage warning を無視して危険な出力を続ける。

撤退条件:

- storage取得APIが不安定な場合、容量表示を `unknown` にして CSV status 表示だけ残す。

### Phase E: measurement-ready UI

計測用UIとして整理する。

Reuse:

- Stage 0 bring-up UI。
- SafetyController / BackendController。
- PWM backend。
- MO CSV logger。
- QR download page。

Make:

- 画面レイアウト整理。
- 測定経路確認表示。
- V/I/P 表示。
- storage / CSV status 表示。
- operation log と対応する確認手順。

検証手順:

- host-side Motor Observe test。
- desktop build と runtime smoke。
- device build。
- 実機で SafeDisabled、OutputArmed、OutputEnabled target 0、target +10、target 0、target -10、brake stop、QR、新規CSVを確認。
- GPIO8/GPIO9、可能ならGPIO10をオシロスコープまたはロジックアナライザで確認。
- `MO-*.csv` の safety state、target、output pattern、measurement path、CSV status を確認。

Go条件:

- safety state と output pattern が UI / CSV / 実測で一致する。
- QR中に出力制御とCSV追記が止まる。
- measurement path が `driver_input_inline` または `unknown` として明示される。
- `unknown` は教材判断に使われない。

NoGO条件:

- `SafeDisabled` / `OutputArmed` で出力が出る。
- brake stop 後に `physical_output_allowed=1` が残る。
- GPIO10をPWM/方向制御に使う、またはBase relayを安全遮断に依存する。
- High/High coast、PWM/High、High/PWM が必要になる。

撤退条件:

- 出力状態、CSV、UI表示のいずれかが説明できない場合、Stage 0 bring-up UI に戻す。

### Phase F: classroom UI decision

実測・安全・運用結果を見て判断する。

Reuse:

- Stage 1 measurement-ready UI の実測ログ。
- 低電圧モータ試験ログ。
- storage warning 結果。

Make:

- 必要なら生徒向け語彙、誤操作防止、教材用表示を設計する。
- classroom-safe fixture と current limit 条件を設計する。

検証手順:

- `driver_input_inline` の実測結果。
- no-load、想定負荷、stall回避条件、thermal behavior。
- storage長時間運用。
- 教員が説明できる操作手順。
- 生徒が誤操作しにくい導線。

Go条件:

- 測定経路、測定値の意味、安全治具、負荷条件、storage policy が説明できる。
- current limit または停止条件が検証済み。
- 教材判断に使う値と使わない値が分けられている。

NoGO条件:

- 測定経路が未確定。
- 実電流、stall、thermal behavior が未検証。
- classroom-safe fixture が未検証。
- 生徒が target 操作で危険状態へ入りやすい。

撤退条件:

- classroom release 条件を満たせない場合、Stage 1 開発者向けUIのまま維持し、launcher登録しない。

## Explicit Non-Goals

この計画で実装しないこと:

- 今回はコード実装しない。
- classroom release化しない。
- 通常 launcher 登録しない。
- assets / localization 追加しない。
- 自動削除しない。
- 自動formatしない。
- 自動eraseしない。
- `REC-*.csv` を自動削除しない。
- 設定ファイルを削除しない。
- GPIO10をPWM/方向制御に使わない。
- Base relayを安全遮断に使わない。
- High/High coastを実装しない。
- PWM/High / High/PWMを実装しない。
- driver input current を motor winding current として扱わない。
- `measurement_path_code=unknown` の V/I/P を教材判断に使わない。
- VAMeter単体でH-bridge motor outputを測らない。

## Remaining 未確認 / 未検証

### 未確認

- `driver_input_inline` 測定時の V/I/P 表示値と CSV 値の関係。
- GPIO10直接波形。
- 長時間連続運転時の storage 挙動。
- storage low warning のしきい値。
- storage cleanup UI の必要性。
- firmware commit / build flags をCSVへ入れる最小実装方法。

### 未検証

- `driver_input_inline` の正式CSV注入。
- モータ巻線電流。
- モータ端子電圧。
- 負荷時挙動。
- stall挙動。
- thermal behavior。
- current limit 条件。
- classroom-safe fixture。
- `driver_input_inline` の配線安全性と測定値の教材利用条件。
- reverse terminal voltageへの対応。現時点ではNoGO。

## Next Human Measurements

次に人間が実測で確認すべきこと:

1. `driver_input_inline` の配線図と実配線。
2. `driver_input_inline` での電源電圧、入力電流、入力電力。
3. VAMeter表示値と `MO-*.csv` 値の対応。
4. 参考測定器がある場合の比較。
5. target 0 / +10 / -10 / brake stop の V/I/P 変化。
6. GPIO8 / GPIO9 波形と、可能なら GPIO10 直接波形。
7. no-load の温度変化。
8. 想定負荷と stall 回避条件。
9. storage low / full に近い状態での CSV begin / append / close / QR。
10. QR後の新規CSVセッション開始。

## Next Minimal Codex Task

次に Codex へ依頼する最小実装候補:

1. `driver_input_inline` を既存 `MO-*.csv` の既存列へ注入する。
2. 既存24列の順序は変更しない。
3. `REC-*.csv` は変更しない。
4. UI上に `Measurement Path: driver_input_inline` または `unknown` を表示する。
5. classroom release、launcher登録、assets/localization、storage cleanup は含めない。
6. `motor_terminal_voltage` / `motor_side_inline` は含めない。

## Go / NoGO

### Go

- docsのみ変更。
- 計測用UIの段階設計がある。
- 測定経路の未確定を断定していない。
- `driver_input_inline` を最初の正式候補として扱っている。
- storage warning計画がある。
- safety要件が明記されている。
- 既存CSV列順序変更を禁止している。
- 実装フェーズごとの検証・撤退条件がある。
- 禁止事項を破っていない。

### NoGO

- コードを変更した。
- classroom release UIとして断定した。
- 測定経路未確定の V/I/P を教材判断に使う前提にした。
- 自動削除・自動formatを提案した。
- `REC-*.csv` を勝手に削除する提案をした。
- GPIO10をPWMに使う、またはBase relayを安全遮断として扱う提案をした。
- High/High coast / PWM/High / High/PWMを入れた。
- VAMeter単体でH-bridge motor outputを測る前提に戻した。
