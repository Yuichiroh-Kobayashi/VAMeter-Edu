# Motor Observe Low Voltage Motor Test Plan

## 目的

この文書は、Motor Observeの次工程として行う低電圧モータ試験計画である。

- 教室実装前の開発者向け確認である。
- まだ授業用運用ではない。
- まだ負荷試験ではない。
- まだ過電流保護試験ではない。
- Motor ObserveでVAMeterが何を測るかを記録し、Go / NoGO判断に使う。

## 前提

- MAKER-DRIVEを使う。
- 小型DCモータを使う。
- 電源はアルカリ乾電池2本直列、約3 V、または電流制限付き電源を使う。
- MAKER-DRIVEは1chのみ使う。
- High/High coastは使わない。
- PWM/High、High/PWMは使わない。
- Base relayを安全遮断として使わない。
- GPIO10は使わない。
- `MOTOR_OBSERVE_BRINGUP_AUTOSTART=1` は開発者向けbring-up buildだけで使う。

## 安全条件

1. 電池または電流制限付き電源を使う。
2. 初回は無負荷で行う。
3. 手で軸を止めない。
4. 長時間連続運転しない。
5. MAKER-DRIVEやモータが熱くなったら停止する。
6. 異音、異臭、発熱、電圧低下があれば停止する。
7. 生徒には触らせない。
8. 学校名、生徒名、校内情報を記録しない。
9. Base relayを安全遮断として扱わない。
10. GPIO10をMotor Observeの制御信号として使わない。

## 確認項目

| 項目 | 期待結果 | 記録 |
|---|---|---|
| SafeDisabled | モータ停止 | pass / fail / 未検証 |
| OutputArmed | モータ停止 | pass / fail / 未検証 |
| OutputEnabled target 0 | モータ停止、Low-Low | pass / fail / 未検証 |
| target +10% | 低速回転 | pass / fail / 未検証 |
| target +30%程度 | 回転 | pass / fail / 未検証 |
| target -10% | 逆方向または反対入力側回転 | pass / fail / 未検証 |
| direction change | 異常なく方向が変わる | pass / fail / 未検証 |
| brake停止 | SafeDisabled / requested target 0 / applied target 0 / Low-Lowで停止 | pass / fail / 未検証 |
| 電源電圧 | 極端に落ちない | measuredVoltageV / 未検証 |
| 可能なら電源電流 | 過大でない | measuredCurrentA / 未検証 |
| VAMeter表示値 | 測定経路と意味を説明できる | displayedVoltageV / displayedCurrentA / displayedPowerW / 未確認 |
| MAKER-DRIVE発熱 | 異常発熱なし | pass / fail / 未検証 |
| モータ発熱 | 異常発熱なし | pass / fail / 未検証 |
| 電池発熱 | 異常発熱なし | pass / fail / 未検証 |
| Base relay | 動作しないこと | pass / fail / 未確認 |
| GPIO10 | 使っていないこと | pass / fail / 未確認 |

## 測定経路の記録

試験前に、VAMeterの測定配線を次のどちらかとして記録する。

| 測定配線 | VAMeterが測るもの | 注意 |
|---|---|---|
| MAKER-DRIVE電源入力側 | VB+ / VB-へ入る電源電圧、ドライバ入力電流、入力電力 | ドライバ入力電流とモータ巻線電流は一致しない可能性がある |
| モータ端子側 | モータ端子電圧、モータ側配線電流 | PWM瞬時値と平均値、表示値、CSV値は一致しない可能性がある |

測定配線を説明できない場合、VAMeter表示値は`未確認`として記録する。
ハードウェア挙動を確認していない場合は`未検証`として記録する。
Motor Observe開発用CSVで`measurement_path_code=unknown`が出ている場合、そのV/I/P値を教材用判断に使ってはいけない。

### Direct VAMeter connection note

In the current development setup, VAMeter is used directly as the measuring instrument.
No additional external probe accessory is used.

Record whether VAMeter is connected to:

- MAKER-DRIVE input-side supply path
- motor terminal-side path
- another explicitly drawn path

Do not record VAMeter values without the wiring description.

## Motor Observe development CSV

Motor Observe bring-up appは、開発用CSVを専用ファイルとして記録する。
これは授業用UIではない。
既存waveform CSVの列構造、保存形式、UIは変更しない。

### ファイル名

- device build: `/spiflash/rec/MO-000.csv`、`/spiflash/rec/MO-001.csv` のように保存する。
- desktop build: 実行ディレクトリに `MO-000.csv`、`MO-001.csv` のように保存する。
- 既存waveform CSVの `REC-*.csv` とは接頭辞で区別する。

### 記録開始・停止

- bring-up appの`onResume()`で自動的に記録開始する。
- bring-up appの`onDestroy()`で`stop`行を出して記録停止する。
- bring-up app内の右スイッチ短押しで、既存のCSVダウンロードQR画面へ入る。
- QR画面へ入る前に`SafeDisabled / target 0 / Low-Low`へ戻し、`stop`行を書いてCSVをcloseする。
- QR画面へ渡す前のfile size確認は、CSV close後に限定する。
- close後に0 byteまたはstat失敗の場合はQR画面へ渡さない。
- QR画面表示中はMotor Observe出力制御とCSV追記を継続しない。
- QR画面から戻る場合は、既存CSVへ追記せず、新しい`MO-*.csv`セッションを開始する。
- 通常launcher登録はしない。
- assets / localizationは追加しない。

### QR download

- `MO-*.csv`は開発用ログとして、既存ローカルCSVダウンロードQR画面から取得できる。
- local download対象のbasenameは`REC-*.csv`または`MO-*.csv`だけを許可する。
- slash、`..`、query、`.csv`以外、`REC-` / `MO-`以外のprefixは許可しない。
- 既存waveform `REC-*.csv`のヘッダ、保存形式、readerは変更しない。
- `MO-*.csv`をwaveform CSVと同じ意味で扱ってはいけない。
- 実機での`MO-*.csv` QRダウンロード確認は未実施なら`未検証`として記録する。

### サンプリング周期

- 初期値: 100 ms、10 Hz。
- PWM瞬時波形は記録しない。
- PWM瞬時波形の確認はオシロスコープまたはロジックアナライザで行う。

### CSV仕様

- schema: `motor_observe_csv_v0.1`
- columns:
  - `schema_version`
  - `session_id`
  - `sample_index`
  - `t_ms`
  - `t_s`
  - `event`
  - `safety_state`
  - `physical_output_allowed`
  - `requested_target_percent`
  - `applied_target_percent`
  - `output_pattern`
  - `gpio9_duty_percent`
  - `gpio8_duty_percent`
  - `measurement_path_code`
  - `voltage_semantics`
  - `current_semantics`
  - `power_semantics`
  - `voltage_v`
  - `current_a`
  - `power_w`
  - `power_source`
  - `pwm_waveform_captured`
  - `abnormal_flag`
  - `note`

### 初期値と注意

- `measurement_path_code`: `unknown`
- `voltage_semantics`: `unknown`
- `current_semantics`: `unknown`
- `power_semantics`: `unknown`
- `power_source`: `hal`
- `pwm_waveform_captured`: `0`
- note: `measurement_path_unknown_not_for_classroom_use`

`current_a`は、測定配線が確定するまでmotor winding currentとみなしてはいけない。
`voltage_v`、`current_a`、`power_w`はHALの数値データから記録する。
UI表示文字列はCSVへ使わない。

### output_pattern

| 条件 | output_pattern | GPIO9 duty | GPIO8 duty |
|---|---|---:|---:|
| physical backendなし | `NOT_APPLICABLE` | 0 | 0 |
| applied target = 0 | `LOW_LOW` | 0 | 0 |
| applied target > 0 | `GPIO9_PWM_GPIO8_LOW` | applied target | 0 |
| applied target < 0 | `GPIO9_LOW_GPIO8_PWM` | 0 | abs(applied target) |

`HIGH_HIGH`、`PWM_HIGH`、`HIGH_PWM`は出力しない。

### CSVで確認するGo条件

- `OutputArmed`で`physical_output_allowed=0`、`applied_target_percent=0`、`output_pattern=LOW_LOW`になる。
- `OutputEnabled target 0`で`output_pattern=LOW_LOW`になる。
- `target > 0`で`output_pattern=GPIO9_PWM_GPIO8_LOW`になる。
- `target < 0`で`output_pattern=GPIO9_LOW_GPIO8_PWM`になる。
- brake stop後は`SafeDisabled`、`requested_target_percent=0`、`applied_target_percent=0`、`output_pattern=LOW_LOW`になる。
- brake stop後の再開には、再度OutputArmed / OutputEnabledへ進む操作が必要になる。
- Fault / timeout / leaveMode後に`applied_target_percent=0`、`output_pattern=LOW_LOW`になる。
- `stop`行は`requested_target_percent=0`、`applied_target_percent=0`、`output_pattern=LOW_LOW`になる。
- `measurement_path_code`が空欄にならない。
- `measurement_path_code=unknown`の場合、教材用判断へ進まない。

### CSVで確認するNoGO条件

- `OutputArmed`で`physical_output_allowed=1`または非ゼロ`applied_target_percent`になる。
- brake stop後に`physical_output_allowed=1`が残る。
- `stop`行に終了前のnon-zero requested targetが残る。
- Fault中に非ゼロ`applied_target_percent`が残る。
- `HIGH_HIGH`、`PWM_HIGH`、`HIGH_PWM`が出る。
- `measurement_path_code`が空欄になる。
- `current_a`を測定経路未確認のままmotor winding currentとして扱う。

## 記録テンプレート

### Date

### Branch / commit

### APP_VERSION

### Build flag

### Motor

### Motor supply

### Measurement wiring

### Target steps

### Observed rotation

### Supply voltage

### Supply current

### VAMeter voltage

### VAMeter current

### VAMeter power

### Temperature / heating

### Abnormal event

### Judgment

### 未確認

### 未検証

### Next action

### Rollback condition

## Go条件

- SafeDisabled / OutputArmed / target0 / brake停止で停止する。
- target操作に対して予期した方向に回る。
- direction changeで異常がない。
- 発熱・異臭・異音がない。
- 電源電圧が極端に落ちない。
- VAMeter表示値の意味が説明できる、または未確認として記録されている。
- MAKER-DRIVE / モータに異常がない。
- Base relayを安全遮断として使わずに成立する。

## NoGO条件

- SafeDisabled / OutputArmedで回る。
- target0やbrake停止で止まらない。
- direction changeで異常がある。
- 異常発熱、異臭、異音がある。
- 電池や配線が発熱する。
- 電源電圧が大きく落ちる。
- VAMeter測定経路が説明できないのに教材化へ進む。
- Base relayを安全遮断として使わないと成立しない。
- GPIO10をMotor Observe制御に使う必要が出る。

## Rollback condition

次のいずれかが起きた場合は試験を停止し、Motor Observeの教材化判断へ進まない。

- 停止状態でモータが回る。
- Low-Lowへ戻らない。
- GPIO10またはBase relayに依存しないと停止できない。
- MAKER-DRIVE、モータ、電池、配線が異常発熱する。
- 測定経路とVAMeter表示値の意味を記録できない。
