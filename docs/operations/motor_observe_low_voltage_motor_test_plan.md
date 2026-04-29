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
| brake停止 | target 0 / Low-Lowで停止 | pass / fail / 未検証 |
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

### Direct VAMeter connection note

In the current development setup, VAMeter is used directly as the measuring instrument.
No additional external probe accessory is used.

Record whether VAMeter is connected to:

- MAKER-DRIVE input-side supply path
- motor terminal-side path
- another explicitly drawn path

Do not record VAMeter values without the wiring description.

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
