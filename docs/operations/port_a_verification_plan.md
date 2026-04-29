# Port.A / GPIO / Base Relay Verification Plan

## 目的

この文書は、Motor Observe PWM backend 実装前に行う実機確認計画である。

- GPIO / PWM 実装はまだ行わない。
- MAKER-DRIVE はまだ接続しない。
- モータはまだ接続しない。
- VAMeter Base relay を安全遮断として仮定しない。
- Base relay が USB-C 経路、測定経路、Motor Observe 経路の全てを遮断できるとは扱わない。

## 確認対象

| 対象 | 現時点の扱い |
|---|---|
| GPIO8 | `HAL_PIN_BASE_GROVE_IOB` としてコード上に存在。実機対応は未検証。 |
| GPIO9 | `HAL_PIN_BASE_GROVE_IOA` としてコード上に存在。実機対応は未検証。 |
| GPIO10 | `HAL_PIN_BASE_RELAY_CTRL` としてコード上に存在。`EXT_G10` との関係は未検証。 |
| VAMeter Base Port.A | Motor Observe の将来候補。信号名と GPIO 対応は未確認 / 未検証。 |
| Base Grove IOA / IOB | Settings Base Test で input pull-up として扱われる。実ピン対応は未検証。 |
| PORT.CUSTOM yellow / white | pinmap 上は G8 / G9。Base Grove IOA / IOB との関係は未確認。 |
| EXT_G8 / EXT_G9 / EXT_G10 | pinmap 上の expansion 信号。Base 側信号との関係は未検証。 |
| Base relay / G10_REL | pinmap と relay_control で GPIO10 と記載。遮断対象ネットは未検証。 |

## 確認したい対応関係

以下は現時点では未確認として扱う。

- GPIO9 と Base Grove IOA の対応。
- GPIO8 と Base Grove IOB の対応。
- GPIO8 / GPIO9 と Port.A signal 1 / signal 2 の対応。
- PORT.CUSTOM yellow / white と Base Grove IOA / IOB の対応。
- EXT_G10 と Base relay G10_REL の関係。
- GPIO10 を Motor Observe PWM 候補から除外すべき根拠。

## 使用する機材

- VAMeter-Edu 実機。
- VAMeter Base。
- USB電源。
- テスター。
- ロジックアナライザまたはオシロスコープ。
- ジャンパ線。
- 必要ならブレッドボード。

この段階では MAKER-DRIVE とモータは接続しない。

## 実施前の安全条件

- MAKER-DRIVE 未接続。
- モータ未接続。
- 外部電源未接続、または電流制限付きで使用。
- GPIO8 / GPIO9 / GPIO10 を短絡しない。
- Base relay の ON/OFF を安全遮断として信用しない。
- 実施前に firmware version と commit を記録する。
- 生徒端末や校内ネットワークは使わない。
- 学校名・生徒名・校内情報を記録しない。

## 実機確認手順案

### Step A: 静的確認

1. firmware commit を記録する。
2. `APP_VERSION` を記録する。
3. `docs/hardware/VAMeter/pinmap.md` を確認する。
4. `docs/hardware/VAMeter_Base/relay_control.md` を確認する。
5. `platforms/vameter/main/hal_vameter/hal_config.h` の GPIO 定義を確認する。
6. `app/apps/app_settings/view/base_test.cpp` で GPIO8 / GPIO9 が Base Test でどう扱われるか確認する。

記録する観点:

- `HAL_PIN_BASE_GROVE_IOA` が GPIO9 として定義されていること。
- `HAL_PIN_BASE_GROVE_IOB` が GPIO8 として定義されていること。
- `HAL_PIN_BASE_RELAY_CTRL` が GPIO10 として定義されていること。
- Settings Base Test が IOA / IOB を input pull-up として扱うこと。

### Step B: 電源OFFでの導通・ピン確認

1. 電源を切る。
2. Port.A 各ピンと GPIO候補の導通を確認する。
3. GPIO8 / GPIO9 / GPIO10 間が短絡していないことを確認する。
4. GND ピンを確認する。
5. 確認方法が不明な箇所は推測で埋めず、未確認と記録する。

確認対象:

- Port.A signal 1 と GPIO8 / GPIO9 候補。
- Port.A signal 2 と GPIO8 / GPIO9 候補。
- PORT.CUSTOM yellow / white と Base Grove IOA / IOB。
- EXT_G10 と Base relay G10_REL。

### Step C: 既存 firmware 起動直後の安全確認

1. MAKER-DRIVE とモータが未接続であることを再確認する。
2. VAMeter-Edu を既存 firmware で起動する。
3. 起動直後の GPIO8 / GPIO9 / GPIO10 の電圧状態を測定器で確認する。
4. Base relay の初期状態を確認する。
5. 予期しない出力がないことを測定器で確認する。

記録する観点:

- GPIO8 の起動直後電圧。
- GPIO9 の起動直後電圧。
- GPIO10 の起動直後電圧。
- Base relay 初期状態。
- 想定外の High / Low / パルスの有無。

### Step D: Settings Base Test の確認

1. Settings Base Test を開く。
2. GPIO8 / GPIO9 が input pull-up として扱われることを確認する。
3. IOA / IOB 表示と実ピンの関係を確認する。
4. Base relay ON/OFF 操作と GPIO10 の関係を確認する。
5. Motor Observe と Base Test を同時利用しない必要性を記録する。

記録する観点:

- `IO9` 表示と実ピンの対応。
- `IO8` 表示と実ピンの対応。
- IOA / IOB の Low 検出時に relay 状態が変化するか。
- GPIO10 の電圧変化と relay 状態の対応。
- Base Test が GPIO8 / GPIO9 を占有するため、Motor Observe と排他にすべきか。

### Step E: Motor Observe backend 実装前の判定

1. GPIO8 / GPIO9 を M1A / M1B 候補として扱えるか判断する。
2. GPIO10 を Motor Observe PWM 候補から除外すべきか判断する。
3. Base relay を安全遮断として使わない設計でよいか確認する。
4. 次工程に進む条件を満たすか判定する。

次工程へ進む条件:

- GPIO8 / GPIO9 と Port.A signal 1 / signal 2 の関係が確認済み、または未確認として明確に記録されている。
- GPIO10 と Base relay / EXT_G10 の関係が記録されている。
- Base relay に依存しない安全状態設計を維持できる。
- MAKER-DRIVE / モータ未接続のまま PWM backend 実装前判断を完了できる。

## 合格基準

- GPIO8 / GPIO9 / GPIO10 の既存用途が説明できる。
- Port.A と GPIO候補の対応が確認または未確認として記録されている。
- GPIO10 を Motor Observe PWM 候補にしない判断が文書化されている。
- Base relay が全経路遮断でないことを前提にしている。
- MAKER-DRIVE / モータ未接続で完了できる。
- PWM backend 実装前の未確認 / 未検証が明確になる。

## NoGO条件

- GPIO8 / GPIO9 / GPIO10 の対応が不明なまま。
- GPIO10 と relay の関係が不明なまま。
- Port.A と Base Grove / PORT.CUSTOM の関係が不明なまま。
- 既存 Base Test と Motor Observe の排他条件が未整理。
- 起動直後に意図しない出力がある。
- Base relay を安全遮断として使わないと成立しない設計になっている。

## 記録テンプレート

### Date

### Branch / commit

### APP_VERSION

### Hardware used

### Instrument used

### Step A result

### Step B result

### Step C result

### Step D result

### Judgment

### 未確認

### 未検証

### Next action

### Rollback condition

# Verification Record 2026-04-29

## Date

- 2026-04-29

## Branch / commit

- branch: 未確認
- commit: 未確認
- note: 人間記入では `Edu V1.1.0` とされていたが、これは branch / commit ではなく firmware version として扱う。

## APP_VERSION

- Edu V1.1.0

## Hardware used

- VAMeter-Edu 実機
- VAMeter Base
- プローブ未接続
- MAKER-DRIVE未接続
- モータ未接続

## Instrument used

- テスター
- note: ロジックアナライザ / オシロスコープは未使用。時間遷移、瞬間的パルス、起動時グリッチは未検証。

## Step A result

- firmware commit: 未確認
- APP_VERSION: Edu V1.1.0
- pinmap:
  - Base relay / G10_REL / GPIO10
  - PORT.CUSTOM yellow / G8 / GPIO8
  - PORT.CUSTOM white / G9 / GPIO9
- relay_control:
  - G10_REL / GPIO10
- hal_config.h:
  - HAL_PIN_BASE_GROVE_IOB = GPIO8
  - HAL_PIN_BASE_GROVE_IOA = GPIO9
  - HAL_PIN_BASE_RELAY_CTRL = GPIO10
- Base Test code:
  - `HAL::SetBaseRelay(!HAL::GetBaseRelayState());`
- notation note:
  - 今後はコード表記に合わせ、Base Grove IOA / IOB または HAL_PIN_BASE_GROVE_IOA / IOB を優先する。

## Step B result

- Port.A C9(White):
  - GPIO9 と対応する測定結果
- Port.A C8(Yellow):
  - GPIO8 と対応する測定結果
- GPIO8 / GPIO9 / GPIO10 short check:
  - 互いに絶縁
- GND:
  - GPIOと絶縁
- notation note:
  - `Port.A signal 1 / signal 2` は曖昧なため、今回の記録では C9(White) / C8(Yellow) 表記を優先する。
- 未確認:
  - `Port.A signal 1 / signal 2` という表記の定義
- 未検証:
  - 時間遷移
  - 起動時パルス
  - PWM出力時の波形

## Step C result

- GPIO8 boot voltage:
  - 0 V
- GPIO9 boot voltage:
  - 0 V
- GPIO10 boot voltage:
  - 測定不可
- Base relay initial state:
  - OFF
- unexpected output:
  - 人間記入: 不明
- note:
  - テスター測定のため、瞬間的なHigh/Low/pulseは未検証。
- 未確認:
  - unexpected output の意味
  - GPIO10 boot voltage
- 未検証:
  - 起動直後の短時間パルス
  - GPIO8 / GPIO9 / GPIO10 の時間遷移

## Step D result

- IO9 display / pin relation:
  - High / 3.23 V
- IO8 display / pin relation:
  - High / 3.23 V
- GPIO8 / GPIO9 input pull-up behavior:
  - IO8 / IO9 が High / 3.23 V として観測されたため、input pull-up 相当の挙動が示唆される。
  - ただし、コードと実ピン対応の完全な検証は継続確認。
- Base relay ON/OFF / GPIO10 relation:
  - リレーON/OFF変化を確認
- Base Test / Motor Observe exclusion:
  - 人間記入: 意図不明
  - 整理: Base Test は GPIO8 / GPIO9 を input pull-up として扱うため、Motor Observe PWM backend とは排他にすべき可能性が高い。
- 未確認:
  - Base Test / Motor Observe exclusion の具体的な実装方針
- 未検証:
  - Base Test中とMotor Observe中のGPIO所有権切替

## Judgment

- 判定:
  - 条件付きGO
- 人間の暫定判断:
  - Go
- 理由:
  - Port.A C9(White) / C8(Yellow) が GPIO9 / GPIO8 と対応する実測結果が得られた。
  - GPIO8 / GPIO9 / GPIO10 は互いに絶縁している。
  - 起動直後の GPIO8 / GPIO9 は 0 V と測定された。
  - Base relay 初期状態は OFF と記録された。
  - GPIO10 は relay 系として扱い、Motor Observe PWM候補から除外する方針を維持できる。
- 条件:
  - 次工程でも MAKER-DRIVE とモータは接続しない。
  - 次工程のPWM backend設計では、GPIO8/GPIO9を候補として扱うが、初回実装は測定器で確認できる最小出力に限定する。
  - GPIO10はPWM候補にしない。
  - Base relayを安全遮断として使わない。
  - テスター測定では時間遷移を見られていないため、PWM実装後の最初の検証では、ロジックアナライザまたはオシロスコープで disabled時Low、起動時グリッチ、PWM波形を確認する。

## Remaining 未確認

- Branch / commit
- APP_VERSION のコード上確認
- `Port.A signal 1 / signal 2` という表記の定義
- unexpected output の意味
- GPIO10 boot voltage
- Base Test / Motor Observe exclusion の具体的な実装方針
- Base relay が遮断する実ネット

## Remaining 未検証

- 起動時の瞬間的パルス
- GPIO8 / GPIO9 / GPIO10 の時間遷移
- PWM出力時のGPIO8 / GPIO9波形
- disabled状態でのGPIO8 / GPIO9 Low維持
- MAKER-DRIVE接続
- モータ接続
- Motor Observe測定経路

## Next action

- Motor Observe PWM backend の設計レビューに進む。
- ただし、次工程でも MAKER-DRIVE / モータは接続しない。
- GPIO8 / GPIO9を候補として扱う。
- GPIO10はPWM候補から除外する。
- 最初のPWM実装では、ロジックアナライザまたはオシロスコープで確認できる最小出力に限定する。
- Base Test と Motor Observe のGPIO所有権を排他にする設計を検討する。

## Rollback condition

- GPIO8 / GPIO9 がPort.A C9(White) / C8(Yellow)と対応しないことが後続確認で分かった場合
- GPIO8 / GPIO9に起動時グリッチまたは意図しない出力が確認された場合
- GPIO10をPWM候補にしない設計が崩れる場合
- Base relayを安全遮断として使わないとMotor Observeが成立しない場合
- Base TestとMotor Observeの排他が実装できない場合
