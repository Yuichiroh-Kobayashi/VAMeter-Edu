# Safety Test Log

## 2026-04-29 Motor Observe no-output safety scaffold

### Date

2026-04-29

### Branch / commit

- branch: edu-dev
- commit: 35df819

### Firmware version

- APP_VERSION: V1.2.2

### Target behavior

- `app/libs/motor_observe_safety/` の no-output safety scaffold
- Fault 中は `targetPercent` を 0 に維持する
- 物理出力 backend は未実装

### Hardware setup

- 実機未使用
- GPIO/PWM/MAKER-DRIVE/モータ接続は未検証

### Expected safe behavior

- 初期状態は `SafeDisabled`
- `prepareOutput()` のみでは物理出力許可にならない
- `enableOutput()` は `OutputArmed` からのみ成功する
- Fault 中の物理出力許可は false
- `clearFault()` / `leaveMode()` / `timeout()` 後は `SafeDisabled`

### Test procedure

```bash
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py build
find build -iname '*motor_observe*' -print
```

### Result

- device build: pass
- `motor_observe_safety.cpp.obj` 生成を確認
- build artifact: `build/esp-idf/main/CMakeFiles/__idf_main.dir/home/yu-ichirou/Dev/VAMeter-Edu/app/libs/motor_observe_safety/motor_observe_safety.cpp.obj`

### Judgment

- pass: no-output scaffold の device build 取込
- 未検証: 実機、GPIO、PWM、MAKER-DRIVE、モータ接続

### Next action

- Motor Observe UI や物理出力 backend を追加する前に、状態遷移のソフトウェア確認を追加する
- GPIO/PWM 実装前に Port.A 割当と disabled 時出力を計測器で確認する

### Rollback condition

- Fault 中に non-zero target が保持される場合
- `SafeDisabled` / `Fault` で physical output allowed が true になる場合
- no-output scaffold が既存 voltage/current/waveform/USB-C/settings 動作へ影響する場合

## 2026-04-29 Motor Observe no-op backend scaffold

### Date

2026-04-29

### Branch / commit

- branch: edu-dev
- commit: 377b759

### Firmware version

- APP_VERSION: V1.2.2

### Target behavior

- `app/libs/motor_observe_backend/` の backend interface / no-op backend / safety接続 controller
- no-op backend は最後に適用された targetPercent のみを保持する
- `SafetyController::isPhysicalOutputAllowed()` が false の場合、backend へ適用する targetPercent は 0

### Hardware setup

- 実機未使用
- GPIO未実装
- PWM未実装
- CytronMotorDriver未導入
- MAKER-DRIVE未接続
- モータ未接続

### Test procedure

```bash
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py build
find build -iname '*motor_observe*' -print
rg -n "motor_observe_backend|Noop|SafetyController|isPhysicalOutputAllowed" app platforms CMakeLists.txt
rg -n "gpio_set_level|ledcWrite|analogWrite|CytronMD|SetBaseRelay|PWM_PWM" app/libs/motor_observe_safety app/libs/motor_observe_backend app platforms/vameter/main || true
```

### Result

- device build: pass
- `motor_observe_backend.cpp.obj` 生成を確認
- build artifact: `build/esp-idf/main/CMakeFiles/__idf_main.dir/home/yu-ichirou/Dev/VAMeter-Edu/app/libs/motor_observe_backend/motor_observe_backend.cpp.obj`
- 禁止語句は既存 app / HAL 側で検出されたが、`app/libs/motor_observe_safety/` と `app/libs/motor_observe_backend/` では未検出

### Judgment

- pass: no-op backend scaffold の device build 取込
- 未確認: 専用 unit test、将来 Motor Observe app との接続
- 未検証: 実機、GPIO、PWM、MAKER-DRIVE、モータ接続

### Next action

- Motor Observe app 追加前に、BackendController の状態遷移テストを追加する
- GPIO/PWM backend 実装前に Port.A 割当と disabled 時出力を計測器で確認する

### Rollback condition

- `isPhysicalOutputAllowed()` が false の状態で backend に non-zero target が適用される場合
- Fault / timeout / leaveMode 相当で backend が target 0 に戻らない場合
- no-op backend scaffold が既存 voltage/current/waveform/USB-C/settings 動作へ影響する場合

## 2026-04-29 Motor Observe state transition host test

### Date

2026-04-29

### Branch / commit

- branch: edu-dev
- commit: d3ad24e

### Firmware version

- APP_VERSION: V1.2.2

### Target behavior

- SafetyController state transition test
- BackendController + NoopBackend test
- Fake fault backend test
- GPIO未実装
- PWM未実装
- CytronMotorDriver未導入

### Hardware setup

- 実機未使用
- MAKER-DRIVE未接続
- モータ未接続

### Test procedure

```bash
cmake -S tests/motor_observe -B /tmp/vameter_motor_observe_test_build
cmake --build /tmp/vameter_motor_observe_test_build
/tmp/vameter_motor_observe_test_build/motor_observe_state_test
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py build
rg -n "gpio_set_level|ledcWrite|analogWrite|CytronMD|SetBaseRelay|PWM_PWM" app/libs/motor_observe_safety app/libs/motor_observe_backend tests/motor_observe || true
```

### Result

- host-side test build: pass
- host-side test execution: pass
- output: `motor_observe_state_test: pass`
- device build: pass
- Motor Observe safety/backend/test 内に禁止語句は未検出

### Judgment

- pass: 物理出力なしの状態遷移テスト
- 未確認: 将来 Motor Observe app との接続、CIへの組み込み
- 未検証: 実機、GPIO、PWM、MAKER-DRIVE、モータ接続

### Next action

- Motor Observe app 追加前に、host-side test を通常の開発手順へ組み込む
- GPIO/PWM backend 実装前に Port.A 割当と disabled 時出力を計測器で確認する

### Rollback condition

- SafetyController が Fault / timeout / leaveMode で target 0 に戻らない場合
- BackendController が `isPhysicalOutputAllowed()` false の状態で backend に non-zero target を適用する場合
- Fake backend fault 後に Fault 状態、target 0、physical output disallow を維持できない場合

## 2026-04-29 Motor Observe host test CTest registration

### Date

2026-04-29

### Branch / commit

- branch: edu-dev
- commit: 8c4be8a

### Firmware version

- APP_VERSION: V1.2.2

### Target behavior

- `tests/motor_observe/CMakeLists.txt` に CTest 登録を追加
- `tests/motor_observe/README.md` に host-side test 実行手順を記録
- GPIO未実装
- PWM未実装
- CytronMotorDriver未導入

### Hardware setup

- 実機未使用
- MAKER-DRIVE未接続
- モータ未接続

### Test procedure

```bash
cmake -S tests/motor_observe -B /tmp/vameter_motor_observe_test_build
cmake --build /tmp/vameter_motor_observe_test_build
/tmp/vameter_motor_observe_test_build/motor_observe_state_test
ctest --test-dir /tmp/vameter_motor_observe_test_build --output-on-failure
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py build
rg -n "gpio_set_level|ledcWrite|analogWrite|CytronMD|SetBaseRelay|PWM_PWM" tests/motor_observe app/libs/motor_observe_safety app/libs/motor_observe_backend || true
```

### Result

- host-side test build: pass
- host-side test direct execution: pass
- CTest execution: pass, 1/1 tests passed
- device build: pass
- Motor Observe safety/backend/test 内に禁止語句は未検出

### Judgment

- pass: CTest 登録と通常開発手順への組み込み
- 未確認: CIへの組み込み、他host環境での実行
- 未検証: 実機、GPIO、PWM、MAKER-DRIVE、モータ接続

### Next action

- CIまたは開発手順で `ctest --test-dir /tmp/vameter_motor_observe_test_build --output-on-failure` を実行する運用を決める
- GPIO/PWM backend 実装前に Port.A 割当と disabled 時出力を計測器で確認する

### Rollback condition

- CTest 登録により standalone build / direct execution が壊れる場合
- test が firmware runtime や device build に混入する場合
- Motor Observe safety/backend/test 内に GPIO/PWM/Cytron/Base relay 依存が入る場合

## 2026-04-29 Motor Observe minimal no-motor PWM backend

### Date

2026-04-29

### Branch / commit

- branch: edu-dev
- commit: 8dec2c4

### Firmware version

- APP_VERSION: v1.1.0-10-g8dec2c4

### Target behavior

- device専用の最小PWM backend
- GPIO8/GPIO9候補を ESP-IDF native LEDC で制御する
- `SafetyController` / `BackendController` は変更なし
- GPIO10未使用
- Base relay未使用
- High/High coast未実装
- PWM/HighまたはHigh/PWM未実装

### Hardware setup

- 実機未使用
- MAKER-DRIVE未接続
- モータ未接続

### Expected safe behavior

- `disarm()` は GPIO8/GPIO9 を Low/Low 相当へ戻す
- target 0 は Low/Low 相当
- target > 0 は GPIO9候補PWM / GPIO8候補Low
- target < 0 は GPIO9候補Low / GPIO8候補PWM
- `BackendController::isPhysicalOutputAllowed()` が false の場合、backendへ渡るtargetは0

### Test procedure

```bash
cmake -S tests/motor_observe -B /tmp/vameter_motor_observe_test_build
cmake --build /tmp/vameter_motor_observe_test_build
/tmp/vameter_motor_observe_test_build/motor_observe_state_test
ctest --test-dir /tmp/vameter_motor_observe_test_build --output-on-failure
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py build
find platforms/vameter/build -iname '*motor_observe*' -print
rg -n "motor_observe_pwm_backend|HAL_PIN_BASE_GROVE_IOA|HAL_PIN_BASE_GROVE_IOB|HAL_PIN_BASE_RELAY_CTRL|ledc|LEDC" platforms/vameter/main app tests CMakeLists.txt
rg -n "ledcWrite|analogWrite|CytronMD|SetBaseRelay|PWM_PWM|gpio_set_level" platforms/vameter/main/hal_vameter/components/motor_observe_pwm_backend.* app/libs/motor_observe_safety app/libs/motor_observe_backend tests/motor_observe || true
```

### Result

- host-side test build: pass
- host-side test direct execution: pass
- CTest execution: pass, 1/1 tests passed
- device build: pass
- build artifact: `platforms/vameter/build/esp-idf/main/CMakeFiles/__idf_main.dir/hal_vameter/components/motor_observe_pwm_backend.cpp.obj`
- 禁止語句確認: 新規PWM backend / Motor Observe safety/backend/test 内に `ledcWrite`、`analogWrite`、`CytronMD`、`SetBaseRelay`、`PWM_PWM`、`gpio_set_level` は未検出

### Judgment

- pass: device buildに最小PWM backendが取込まれた
- 未確認: LEDC timer/channel の最終割当、PWM周波数の教育用最終値、将来Motor Observe appとの接続
- 未検証: 実機波形、GPIO8/GPIO9 Low維持、起動時グリッチ、MAKER-DRIVE接続、モータ接続

### Next action

- MAKER-DRIVE / モータ未接続のまま、ロジックアナライザまたはオシロスコープでGPIO8/GPIO9/GPIO10を確認する
- power-on直後、firmware起動直後、backend begin直後、disabled、OutputArmed、OutputEnabled + target、Fault、timeout、leaveMode、disableを確認する

### Rollback condition

- host-side test / CTest / device build が壊れる場合
- `SafetyController` / `BackendController` を迂回する出力経路が必要になる場合
- disabled / disarm / fault / timeout / leaveMode でGPIO8/GPIO9がLow/Low相当にならない場合
- GPIO10またはBase relayに依存する設計になる場合
- High/High coast、PWM/High、High/PWM が必要になる場合

## 2026-04-29 Motor Observe PWM backend safety hardening

### Date

2026-04-29

### Branch / commit

- branch: edu-dev
- commit: 8dec2c4

### Firmware version

- APP_VERSION: v1.1.0-10-g8dec2c4

### Target behavior

- PWM backend safety hardening
- fault時に duty 0 / duty 0 を試みる
- `disarm()` はfault中でも duty 0 / duty 0 を試みる
- 方向切替時に先に duty 0 / duty 0 を通す
- GPIO10未使用
- Base relay未使用
- High/High coast未実装
- PWM/HighまたはHigh/PWM未実装

### Hardware setup

- 実機未使用
- MAKER-DRIVE未接続
- モータ未接続

### Expected safe behavior

- fault時は pending target / last applied target を 0 に戻す
- fault中に `setTargetPercent()` が呼ばれても pending target は 0 を維持する
- forward / reverse duty が同時に非ゼロになり得る入力はfault扱いにする
- non-zero duty適用前に、両channelへ duty 0 を適用する

### Test procedure

```bash
cmake -S tests/motor_observe -B /tmp/vameter_motor_observe_test_build
cmake --build /tmp/vameter_motor_observe_test_build
/tmp/vameter_motor_observe_test_build/motor_observe_state_test
ctest --test-dir /tmp/vameter_motor_observe_test_build --output-on-failure
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py build
rg -n "motor_observe_pwm_backend|HAL_PIN_BASE_GROVE_IOA|HAL_PIN_BASE_GROVE_IOB|HAL_PIN_BASE_RELAY_CTRL|ledc|LEDC" platforms/vameter/main app tests CMakeLists.txt
rg -n "ledcWrite|analogWrite|CytronMD|SetBaseRelay|PWM_PWM|gpio_set_level" platforms/vameter/main/hal_vameter/components/motor_observe_pwm_backend.* app/libs/motor_observe_safety app/libs/motor_observe_backend tests/motor_observe || true
```

### Result

- host-side test build: pass
- host-side test direct execution: pass
- CTest execution: pass, 1/1 tests passed
- device build: pass
- 禁止語句確認: 新規PWM backend / Motor Observe safety/backend/test 内に `ledcWrite`、`analogWrite`、`CytronMD`、`SetBaseRelay`、`PWM_PWM`、`gpio_set_level` は未検出

### Judgment

- pass: PWM backend safety hardening の build / host-side test 確認
- 未確認: fault時の実GPIO波形、方向切替時の実GPIO波形、LEDC resource割当の最終確定
- 未検証: 実機波形、GPIO8/GPIO9 Low維持、起動時グリッチ、MAKER-DRIVE接続、モータ接続

### Next action

- UI実装前に no-motor bring-up UI の設計を確認する
- MAKER-DRIVE / モータ未接続のまま、ロジックアナライザまたはオシロスコープでfault時、disarm時、方向切替時のGPIO8/GPIO9/GPIO10を確認する

### Rollback condition

- host-side test / CTest / device build が壊れる場合
- fault時またはfault中disarm時に duty 0 / duty 0 を試みられない場合
- 方向切替時に両方向PWMが出る場合
- GPIO10またはBase relayに依存する設計になる場合
- High/High coast、PWM/High、High/PWM が必要になる場合

## Entry template

### Date

### Branch / commit

### Firmware version

### Target behavior

### Hardware setup

### Expected safe behavior

### Test procedure

### Result

### Judgment

- pass / fail / 未検証:

### Rollback condition

- 
