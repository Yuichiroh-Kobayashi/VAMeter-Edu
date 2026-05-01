# Motor Observe PWM Backend Design Review

## Scope

この文書は、Motor Observe PWM backend の設計レビューである。

この文書は実装仕様書ではない。将来のPWM backend実装前に、安全上の制限、GPIO候補、検証条件、NoGO条件を確認するために読む。

- 今回はGPIO/PWM実装ではない。
- MAKER-DRIVEはまだ接続しない。
- モータはまだ接続しない。
- Base relayを安全遮断として扱わない。
- GPIO10はPWM候補にしない。
- Base relayはmeasurement path relayとして限定的に扱う。
- firmware runtimeは変更しない。

## Current implementation note

As of the Motor Observe bring-up work, an initial device-only PWM backend and no-motor bring-up UI exist.

This document remains the design review and safety constraint source for:

- current initial PWM backend behavior
- future PWM backend changes
- bring-up UI changes
- MAKER-DRIVE connection review
- motor connection review

Do not read older `future PWM backend` wording as permission to ignore the current implementation.
When changing existing Motor Observe PWM behavior, treat this document as active constraints.

## Confirmed facts from Port.A verification

根拠: `docs/operations/port_a_verification_plan.md` の `Verification Record 2026-04-29`。

- Port.A C9(White) は GPIO9 / `HAL_PIN_BASE_GROVE_IOA` と対応する候補として記録された。
- Port.A C8(Yellow) は GPIO8 / `HAL_PIN_BASE_GROVE_IOB` と対応する候補として記録された。
- GPIO8 / GPIO9 / GPIO10 は互いに絶縁と記録された。
- GPIO8 / GPIO9 の起動直後電圧は 0 V と記録された。
- GPIO10 は Base relay control として扱う。
- GPIO10 boot voltage は未確認。
- Base relay initial state は OFF と記録された。
- 測定はテスターで実施された。
- 時間遷移、瞬間パルス、起動時グリッチは未検証。

## GPIO candidate policy

| GPIO | code name | physical note | Motor Observe use | status |
|---:|---|---|---|---|
| GPIO8 | `HAL_PIN_BASE_GROVE_IOB` | Port.A C8(Yellow) candidate | M1B candidate | 条件付き候補 |
| GPIO9 | `HAL_PIN_BASE_GROVE_IOA` | Port.A C9(White) candidate | M1A candidate | 条件付き候補 |
| GPIO10 | `HAL_PIN_BASE_RELAY_CTRL` | Base relay / G10_REL / EXT_G10 risk | measurement path relay専用 (PWMは使わない) | PWM除外 |

M1A / M1B の極性は、この文書では最終断定しない。

将来のPWM backend実装前レビューでは、Port.A C9(White) / C8(Yellow) と MAKER-DRIVE M1A / M1B の入れ替え可能性を残す。モータ未接続の波形確認で、正方向・逆方向の定義、target符号、表示上の向きを分けて確認する。

GPIO10は relay 系として扱い、Motor Observe PWM候補から除外する。Base relayはmeasurement path relayとして限定的に使うが、安全遮断として使う設計にしてはならない。

## Backend architecture

In this document, `future PWM backend` includes the current initial PWM backend and any later extension unless explicitly distinguished.

Motor Observe の物理出力は、既存の no-output 構造を迂回しない。

| Layer | Role | Physical output |
|---|---|---|
| `SafetyController` | 状態とtargetを管理する。`SafeDisabled`、`OutputArmed`、`OutputEnabled`、`Fault`、`FaultCleared` の遷移を扱う。 | 持たない |
| `BackendController` | `SafetyController` の許可状態を見て backend に target を渡す。`isPhysicalOutputAllowed()` が false のときは必ず 0 を渡す。 | 直接持たない |
| `NoopBackend` | 物理出力なし。host-side test 対象。最後に適用された target の確認に使う。 | 持たない |
| future PWM backend | device専用。GPIO8/GPIO9候補を使う可能性がある。初期化時は必ず出力OFF相当。 | 持つ可能性がある |

future PWM backend は、`SafetyController` と `BackendController` を迂回して出力してはならない。

target変更だけでは物理出力許可状態を変えない。物理出力は、`OutputEnabled` かつ `isPhysicalOutputAllowed()` が true の状態で、backend update 相当の処理が実行された場合に限る。

## Measurement path relay policy

Base relay は safety relay ではない。Motor Observe bring-up では measurement path relay として限定的に扱う。

- `OutputArmed` で relay を ON にする。
- `OutputEnabled` で relay を ON にする。
- `SafeDisabled` / `Fault` / timeout / leaveMode / QR download / onDestroy では relay を OFF にする。
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

## Proposed future backend name and location

この節は将来の配置案である。今回は以下のファイルを作成しない。

候補名:

- `motor_observe_pwm_backend.h`
- `motor_observe_pwm_backend.cpp`

配置案:

| Option | Location | Pros | Cons |
|---|---|---|---|
| app/libs に置く案 | `app/libs/motor_observe_backend/` | 既存 `Backend` interface と近い。host-side test から見通しやすい。 | GPIO、PWM timer、board pin の詳細が app/libs に入りやすい。 |
| HAL 側に置く案 | `platforms/vameter/main/hal_vameter/` | GPIO8/GPIO9、PWM channel、board固有初期化をHAL側へ閉じ込めやすい。 | `Backend` interface との接続点が分散しやすい。host-side test対象から外れやすい。 |
| interface は app/libs、device固有実装は HAL 側に置く案 | interface: `app/libs/motor_observe_backend/`、device implementation: `platforms/vameter/main/hal_vameter/` | safety/backend抽象をhost-side test可能に保ち、GPIO/PWMなどのboard固有詳細をHAL側へ閉じ込められる。 | 境界の命名と依存方向を明確にする必要がある。 |

推奨案:

- interface は `app/libs/motor_observe_backend/` に置く。
- device固有実装は `platforms/vameter/main/hal_vameter/` 側に置く。

理由:

- `SafetyController` / `BackendController` / `NoopBackend` のhost-side test可能性を維持できる。
- GPIO8/GPIO9、PWM resource、board固有の安全初期化をHAL側に閉じ込められる。
- 将来、desktopやhost testでは `NoopBackend` を使い、device buildだけが物理出力backendを選ぶ構成にしやすい。

## Safety requirements for future PWM backend

future PWM backend は、以下を満たすまでMotor Observeの物理出力候補にしてはならない。

- begin時に非ゼロ出力を出さない。
- disarm時はGPIO8/GPIO9を安全状態へ戻す。
- disabled / fault / timeout / leaveMode ではPWM duty 0にする。
- mode select / app openだけでは出力しない。
- target変更だけでは出力許可状態を変えない。
- Base relayを安全遮断として使わない。
- GPIO10はPWMに使わない。measurement path relay専用とする。
- Base TestとMotor Observeは排他にする。
- firmware起動直後に、ロジックアナライザまたはオシロスコープでGPIO8/GPIO9がLowまたは安全状態であることを確認する。
- backend初期化直後に、ロジックアナライザまたはオシロスコープでGPIO8/GPIO9がLowまたは安全状態であることを確認する。
- Fault発生時は、target保持値や表示値に関係なく物理出力を0へ戻す。
- PWM波形の確認が完了するまで、MAKER-DRIVEとモータを接続しない。

## Base Test exclusion policy

Settings Base Test は GPIO8 / GPIO9 を input pull-up として扱う。

Motor Observe PWM backend は、GPIO8 / GPIO9 を将来 output として使う可能性がある。

したがって、Base Test と Motor Observe の同時利用は禁止する。

将来 Motor Observe app を追加する場合は、以下を満たす設計にする。

- Base Test と Motor Observe が同時に動かない。
- app lifecycle / mode transition でGPIO所有権を明確にする。
- Motor Observeへ入る前に、Base Test側がGPIO8/GPIO9を保持していないことを確認する。
- Motor Observeから出るときは、必ず disarm し、GPIO8/GPIO9を安全状態へ戻す。
- Fault / timeout / leaveMode / app close では、Base Testへ戻る前に物理出力を0へ戻す。
- GPIO所有権切替は未検証として扱い、最初のdevice実装後に実測で確認する。

## Initial no-motor PWM verification plan

将来の初回PWM実装後も、この検証段階では MAKER-DRIVE とモータを接続しない。

検証条件:

- GPIO8/GPIO9に測定器を接続する。
- ロジックアナライザまたはオシロスコープを使う。
- GPIO10も観測できる場合は観測し、PWMが出ないことを確認する。
- Base relayを安全遮断として扱わない。
- Base relay操作はmeasurement path relayとして扱い、PWM出力検証と混同しない。

確認項目:

1. power-on直後にGPIO8/GPIO9のグリッチがないこと。
2. firmware起動直後にGPIO8/GPIO9がLowまたは安全状態であること。
3. backend begin直後にGPIO8/GPIO9がLowまたは安全状態であること。
4. disabled状態でGPIO8/GPIO9がLowまたは安全状態であること。
5. `OutputArmed` 状態で非ゼロPWMが出ないこと。
6. `OutputEnabled` + target指定 + update時だけPWMが出ること。
7. FaultでPWM dutyが0になること。
8. timeoutでPWM dutyが0になること。
9. leaveModeでPWM dutyが0になること。
10. disableでPWM dutyが0になること。
11. GPIO10にPWMが出ないこと。
12. measurement path relayがSafeDisabled / Fault / timeout / leaveModeでOFFになること。
13. M1A / M1B 候補の入れ替え可能性を記録すること。

測定結果は、`docs/operations/safety_test_log.md` または専用ログに記録する。

## Go / NoGO criteria for implementation

### Go conditions

将来のPWM backend実装へ進むには、少なくとも以下を満たす。

- host-side test がpassしている。
- Port.A verification record がある。
- GPIO8/GPIO9候補が整理されている。
- GPIO10がPWM候補から除外されている。
- Base relayを安全遮断として使わない設計になっている。
- no-motor PWM verification plan がある。
- MAKER-DRIVE / モータ未接続で検証できる設計になっている。
- `SafetyController` と `BackendController` を迂回しない設計になっている。
- Base Testとの排他方針が文書化されている。

### NoGO conditions

以下のいずれかに該当する場合、PWM backend実装へ進まない。

- `SafetyController`を迂回する設計。
- Base relay依存の安全設計。
- GPIO10をPWM候補にする設計。
- Base Testとの排他が不明。
- disabled状態でLowまたは安全状態を保証できない設計。
- MAKER-DRIVEまたはモータを接続しないと検証できない設計。
- mode select / app openだけで物理出力が出る設計。
- target変更だけで出力許可状態が変わる設計。

## Remaining 未確認 / 未検証

### 未確認

- branch / commit と APP_VERSION の最新対応。
- `Port.A signal 1 / signal 2` という表記の定義。
- GPIO10 boot voltage。
- Base relay が遮断する実ネット。
- Base Test / Motor Observe exclusion の具体的な実装方針。
- future PWM backend の最終配置と依存方向。
- M1A / M1B と target符号の最終対応。

### 未検証

- 起動時の瞬間的パルス。
- GPIO8 / GPIO9 / GPIO10 の時間遷移。
- PWM出力時のGPIO8 / GPIO9波形。
- disabled状態でのGPIO8 / GPIO9 Low維持。
- Base Test中とMotor Observe中のGPIO所有権切替。
- MAKER-DRIVE接続。
- モータ接続。
- Motor Observe測定経路。

## Next action

- モータ未接続のPWM backend実装前レビューに進む。
- そのレビューで、device固有実装をHAL側に置く場合の依存方向、初期化順、GPIO所有権、測定手順を確定する。
- 実装へ進む場合も、最初は MAKER-DRIVE / モータ未接続の最小PWM backendに限定する。

## Motor input policy for initial implementation

Initial Motor Observe PWM backend uses MAKER-DRIVE compatible PWM_PWM style.

- SafeDisabled / OutputArmed / Fault / timeout / leaveMode / target 0: Low/Low
- target > 0: GPIO9 candidate PWM / GPIO8 candidate Low
- target < 0: GPIO9 candidate Low / GPIO8 candidate PWM
- High/High coast: not implemented
- PWM/High and High/PWM: not implemented

Reason:

- Low/Low is treated as brake by MAKER-DRIVE truth table.
- For initial no-motor verification, brake-side behavior is acceptable.
- Coast behavior must not be introduced without separate safety review.

## Bring-up UI implementation rule

Motor Observe bring-up UI is a development-only UI for no-motor waveform verification.

Rules:

- Do not register it in the normal launcher.
- Do not add app icons for bring-up only.
- Do not add localization or assets unless explicitly requested.
- Default build must keep the normal StartupAnim -> Launcher flow.
- Bring-up UI may be opened only when an explicit development flag is enabled.
- Recommended flag: `MOTOR_OBSERVE_BRINGUP_AUTOSTART`.
- Do not enable the flag for classroom release builds.

When UI code changes:

- run host-side Motor Observe tests
- run desktop build
- run desktop runtime smoke test
- run device build

Desktop runtime may not fully validate GPIO/PWM, but it is required to catch UI, lifecycle, include, and device-dependency leakage errors.

## Development CSV logging

Motor Observe bring-up app may write a development-only CSV log for safety and measurement-path review.

Rules:

- Use a Motor Observe dedicated schema: `motor_observe_csv_v0.1`.
- Use Motor Observe dedicated file names such as `MO-000.csv`.
- Do not add Motor Observe columns to existing waveform `REC-*.csv`.
- Do not change existing waveform CSV headers or reader behavior.
- Record `requested_target_percent` and `applied_target_percent` separately.
- Derive `output_pattern` from the applied target, not from UI text.
- Use HAL/internal numeric measurement values, not display strings.
- Default `measurement_path_code` to `unknown` until wiring is explicitly recorded.
- If `measurement_path_code=unknown`, do not use V/I/P values for classroom or教材判断.
- Do not record `HIGH_HIGH`, `PWM_HIGH`, or `HIGH_PWM`.
- Do not use GPIO10 or Base relay for CSV logging.
- In the initial development build, brake stop returns to `SafeDisabled`.
- Stop / leaveMode / app close must not leave physical output allowed.
- Right-side switch access to the existing local CSV download QR page is development-only.
- Before entering the QR page, the bring-up app must force `SafeDisabled / target 0 / Low-Low`, write a CSV `stop` row, and close the current `MO-*.csv`.
- File size checks for QR download are performed only after closing the current `MO-*.csv`.
- `fsync()` and open-file size checks are not required for CSV begin success.
- Zero-byte `MO-*.csv` files must not be passed to the QR download page.
- While the QR page is shown, Motor Observe output control and CSV appending must not continue.
- Returning from the QR page starts a new `MO-*.csv` session; it does not append to the downloaded file.
- Local CSV download allows only basename `REC-*.csv` or `MO-*.csv`; arbitrary paths are not allowed.

Initial sampling interval:

- 100 ms / 10 Hz.
- PWM instantaneous waveform is not captured by this CSV.
- Use an oscilloscope or logic analyzer for PWM waveform verification.
