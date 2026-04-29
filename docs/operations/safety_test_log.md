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

## 2026-04-29 Motor Observe no-motor bring-up UI

### Date

2026-04-29

### Branch / commit

- branch: edu-dev
- commit: 2262b72

### Firmware version

- APP_VERSION: v1.1.0-11-g2262b72

### Target behavior

- no-motor bring-up UI
- launcher未登録
- assets/localization未追加
- `SafetyController` / `BackendController` 経由でbackend targetを操作する
- device buildでは `VAMeterMotorObservePwmBackend`
- desktop buildでは `NoopBackend`
- GPIO10未使用
- Base relay未使用
- High/High coast未実装
- PWM/HighまたはHigh/PWM未実装

### Hardware setup

- 実機未使用
- MAKER-DRIVE未接続
- モータ未接続

### Expected safe behavior

- app open直後は `SafeDisabled / target 0 / Low-Low`
- `OutputArmed` は target 0 を維持し、PWMを出さない
- `OutputEnabled` で target 変更を許可する
- brake停止は target 0 / Low-Low とし、`OutputArmed` へ戻る
- app close / destroy 相当では backend を `leaveMode()` で disarmする

### Test procedure

```bash
cmake -S tests/motor_observe -B /tmp/vameter_motor_observe_test_build
cmake --build /tmp/vameter_motor_observe_test_build
/tmp/vameter_motor_observe_test_build/motor_observe_state_test
ctest --test-dir /tmp/vameter_motor_observe_test_build --output-on-failure
cmake -S . -B build
cmake --build build -j8
cd build/desktop
timeout -s KILL 5s ./app_desktop_build
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py build
find app/apps/app_motor_observe_bringup platforms/vameter/build build -iname '*motor_observe*' -print
rg -n "app_motor_observe_bringup|Motor Observe Bring-up|BRING-UP ONLY|MAKER-DRIVE|Motor: NOT CONNECTED|GPIO10: NOT USED|Base relay: NOT USED" app platforms CMakeLists.txt
rg -n "SetBaseRelay|CytronMD|PWM_PWM|ledcWrite|analogWrite|gpio_set_level" app/apps/app_motor_observe_bringup app/libs/motor_observe_safety app/libs/motor_observe_backend platforms/vameter/main/hal_vameter/components/motor_observe_pwm_backend.* || true
```

### Result

- host-side test build: pass
- host-side test direct execution: pass
- CTest execution: pass, 1/1 tests passed
- desktop build: pass
- desktop runtime: 起動確認のみ。bring-up appはlauncher未登録のため通常メニューから未到達
- device build: pass
- build artifact:
  - `platforms/vameter/build/esp-idf/main/CMakeFiles/__idf_main.dir/home/yu-ichirou/Dev/VAMeter-Edu/app/apps/app_motor_observe_bringup/app_motor_observe_bringup.cpp.obj`
  - `build/CMakeFiles/app_layer.dir/app/apps/app_motor_observe_bringup/app_motor_observe_bringup.cpp.o`
- 禁止語句確認: bring-up app / Motor Observe safety/backend / PWM backend 内に `SetBaseRelay`、`CytronMD`、`PWM_PWM`、`ledcWrite`、`analogWrite`、`gpio_set_level` は未検出

### Judgment

- pass: no-motor bring-up UI の build取込確認
- 未確認: 実機での起動手段、app lifecycleとGPIO所有権の完全な切替、Fault相当操作の実機手順
- 未検証: 実機波形、GPIO8/GPIO9/GPIO10、MAKER-DRIVE接続、モータ接続

### Next action

- MAKER-DRIVE / モータ未接続のまま、限定的な起動手段でbring-up UIを起動する
- ロジックアナライザまたはオシロスコープで、SafeDisabled、OutputArmed、OutputEnabled target 0 / +10% / -10%、方向切替、brake停止、app close相当を確認する

### Rollback condition

- launcher通常メニューへ混入する場合
- assets/localization追加が必要になる場合
- app open / OutputArmed でPWMが出る場合
- GPIO10またはBase relayに依存する場合
- High/High coast、PWM/High、High/PWM が必要になる場合

## 2026-04-29 Motor Observe bring-up UI limited startup path

### Date

2026-04-29

### Branch / commit

- branch: edu-dev
- commit: 2262b72

### Firmware version

- APP_VERSION: v1.1.0-11-g2262b72-dirty

### Target behavior

- bring-up UI limited startup path
- `MOTOR_OBSERVE_BRINGUP_AUTOSTART` を明示的に有効化した場合だけ、StartupAnim後に `AppMotorObserveBringup` を起動する
- defaultでは従来どおり StartupAnim から Launcher に進む
- launcher通常登録なし
- assets/localization未追加
- MAKER-DRIVE未接続
- モータ未接続
- Base relay未使用
- GPIO10未使用

### Enable procedure

Desktop bring-up build:

```bash
cmake -S . -B build_motor_observe_bringup -DCMAKE_CXX_FLAGS="-DMOTOR_OBSERVE_BRINGUP_AUTOSTART=1"
cmake --build build_motor_observe_bringup -j8
```

Device bring-up build:

```bash
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py -B build_motor_observe_bringup -DCMAKE_CXX_FLAGS="-DMOTOR_OBSERVE_BRINGUP_AUTOSTART=1" build
```

Do not enable `MOTOR_OBSERVE_BRINGUP_AUTOSTART` for classroom release builds.

### Test procedure

```bash
cmake -S tests/motor_observe -B /tmp/vameter_motor_observe_test_build
cmake --build /tmp/vameter_motor_observe_test_build
/tmp/vameter_motor_observe_test_build/motor_observe_state_test
ctest --test-dir /tmp/vameter_motor_observe_test_build --output-on-failure
cmake -S . -B build
cmake --build build -j8
cd build/desktop
timeout -s KILL 8s ./app_desktop_build
cmake -S . -B build_motor_observe_bringup -DCMAKE_CXX_FLAGS="-DMOTOR_OBSERVE_BRINGUP_AUTOSTART=1"
cmake --build build_motor_observe_bringup -j8
cd build/desktop
timeout -s KILL 8s ./app_desktop_build
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py build
idf.py -B build_motor_observe_bringup -DCMAKE_CXX_FLAGS="-DMOTOR_OBSERVE_BRINGUP_AUTOSTART=1" build
rg -n "MOTOR_OBSERVE|BRINGUP|AUTOSTART|AppMotorObserveBringup|app_motor_observe_bringup" app platforms CMakeLists.txt
rg -n "SetBaseRelay|CytronMD|PWM_PWM|ledcWrite|analogWrite|gpio_set_level" app/apps/app_motor_observe_bringup app/apps/app_startup_anim app/apps/app_launcher app/libs/motor_observe_safety app/libs/motor_observe_backend platforms/vameter/main/hal_vameter/components/motor_observe_pwm_backend.* || true
```

### Result

- host-side test build: pass
- host-side test direct execution: pass
- CTest execution: pass, 1/1 tests passed
- desktop normal build: pass
- desktop normal runtime: 起動確認のみ。8秒timeout kill
- desktop bring-up build: pass
- desktop bring-up runtime: 起動確認のみ。8秒timeout kill。StartupAnim後の画面遷移は未確認
- device normal build: pass
- device bring-up build: pass
- default behavior: `MOTOR_OBSERVE_BRINGUP_AUTOSTART` 未定義時は StartupAnim から Launcher に進む
- flag behavior: `MOTOR_OBSERVE_BRINGUP_AUTOSTART=1` のビルドで StartupAnim後の起動先を `AppMotorObserveBringup` に差し替える
- 禁止語句確認: `SetBaseRelay` は既存 `app_launcher.cpp` で検出。bring-up app / StartupAnim hook / PWM backend には新規使用なし

### Judgment

- pass: limited startup path の build取込確認
- 未確認: desktop runtimeでStartupAnim後にbring-up画面へ到達すること、実機での起動操作
- 未検証: 実機波形、GPIO8/GPIO9/GPIO10、MAKER-DRIVE接続、モータ接続

### Next action

- `MOTOR_OBSERVE_BRINGUP_AUTOSTART=1` のdevice buildをflashし、MAKER-DRIVE / モータ未接続のまま、ロジックアナライザまたはオシロスコープでGPIO8/GPIO9/GPIO10を確認する
- SafeDisabled、OutputArmed、OutputEnabled target 0 / +10% / -10%、方向切替、brake停止、app close相当を記録する

### Rollback condition

- default buildで StartupAnim から Launcher へ進まない場合
- bring-up UI が launcher通常メニューへ混入する場合
- assets/localization追加が必要になる場合
- app open / OutputArmed でPWMが出る場合
- GPIO10またはBase relayに依存する場合
- classroom releaseで `MOTOR_OBSERVE_BRINGUP_AUTOSTART` が有効化される場合

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

## 2026-04-29 Motor Observe development CSV logger

### Date

2026-04-29

### Branch / commit

- branch: edu-dev
- commit: 08ad4cc-dirty

### Read files

- `README.md`
- `CHANGELOG.md`
- `AGENTS.md`
- `.github/copilot-instructions.md`
- `.github/instructions/firmware.instructions.md`
- `.github/instructions/docs.instructions.md`
- `.github/instructions/operations.instructions.md`
- `docs/canon/minimum_constraints.md`
- `docs/standards/measurement_safety_policy.md`
- `docs/standards/coding_standard.md`
- `docs/standards/naming_units.md`
- `docs/standards/review_checklist.md`
- `docs/architecture/app_map.md`
- `docs/architecture/measurement_state_machine.md`
- `docs/architecture/probe_mode_matrix.md`
- `docs/architecture/motor_observe_pwm_backend_design.md`
- `docs/hardware/VAMeter/measurement_path.md`
- `docs/hardware/MAKER_DRIVE/interface_port_a_1ch.md`
- `docs/operations/motor_observe_low_voltage_motor_test_plan.md`
- `docs/operations/safety_test_log.md`
- Motor Observe implementation files under `app/apps/app_motor_observe_bringup/`, `app/libs/motor_observe_safety/`, `app/libs/motor_observe_backend/`, `platforms/vameter/main/hal_vameter/components/motor_observe_pwm_backend.*`
- Existing waveform CSV / local download implementation files under `platforms/vameter/main/hal_vameter/components/hal_fs.cpp`, `hal_va_recorder.cpp`, `hal_web_server.cpp`, `app/apps/app_waveform/`, and `app/apps/app_files/`

### Target behavior

- Add Motor Observe dedicated development CSV logging.
- Keep existing waveform `REC-*.csv` format unchanged.
- Use `MO-000.csv`, `MO-001.csv`, ... for Motor Observe logs.
- Record safety state, requested target, applied target, output pattern, measurement path code, V/I/P values, and notes.
- Do not add launcher registration.
- Do not add assets or localization.
- Do not use GPIO10.
- Do not use Base relay as a safety disconnect.
- Do not implement High/High coast, PWM/High, or High/PWM.

### CSV specification

- schema: `motor_observe_csv_v0.1`
- sample interval: 100 ms / 10 Hz
- start: bring-up app `onResume()`
- stop: bring-up app `onDestroy()`
- measurement values: HAL numeric power-monitor data
- default `measurement_path_code`: `unknown`
- default semantics: `unknown`
- note: `measurement_path_unknown_not_for_classroom_use`

### Test procedure

```bash
cmake -S tests/motor_observe -B /tmp/vameter_motor_observe_test_build
cmake --build /tmp/vameter_motor_observe_test_build
/tmp/vameter_motor_observe_test_build/motor_observe_state_test
ctest --test-dir /tmp/vameter_motor_observe_test_build --output-on-failure
. $HOME/esp/esp-idf/export.sh && cd platforms/vameter && idf.py build
. $HOME/esp/esp-idf/export.sh && cd platforms/vameter && idf.py -B build_motor_observe_bringup -DCMAKE_CXX_FLAGS="-DMOTOR_OBSERVE_BRINGUP_AUTOSTART=1" build
cmake -S . -B build
cmake --build build -j8
```

### Result

- host-side test build: pass
- host-side test direct execution: pass
- CTest execution: pass, 1/1 tests passed
- device build: pass
- device bring-up build with `MOTOR_OBSERVE_BRINGUP_AUTOSTART=1`: pass
- desktop build: pass
- registry notice during ESP-IDF configure: component registry connection unavailable, dependency change check skipped; build continued using available dependencies
- device runtime CSV creation: 未検証

### Remaining 未確認

- exact commit after implementation
- Motor Observe CSV file visibility in local QR download workflow
- whether `MO-*.csv` should be hidden from normal record-file viewer later

### Remaining 未検証

- device runtime CSV file creation
- Motor Observe CSV contents on actual VAMeter hardware
- GPIO8/GPIO9 waveform while CSV logging is active
- GPIO10 direct waveform while CSV logging is active
- MAKER-DRIVE / motor operation while CSV logging is active
- Motor Observe measurement path

### Rollback condition

- existing waveform CSV header or reader changes are required
- CSV logging changes PWM or safety behavior
- `OutputArmed` logs or applies non-zero output
- Fault / timeout / leaveMode does not return to Low-Low
- GPIO10 or Base relay becomes part of Motor Observe logging or safety behavior
- launcher registration, assets, or localization are needed

## 2026-04-29 Motor Observe brake stop CSV state alignment

### Date

2026-04-29

### Branch / commit

- branch: edu-dev
- commit: 08ad4cc-dirty

### Target behavior

- Fix brake stop state mismatch risk.
- Brake stop returns UI and controller state to `SafeDisabled`.
- Brake stop CSV state is `SafeDisabled / requested 0 / applied 0 / LOW_LOW`.
- Brake stop CSV `physical_output_allowed` is `0`.
- `onDestroy()` writes the `stop` row after forcing requested target 0, applied target 0, and Low-Low.
- Existing waveform `REC-*.csv` format remains unchanged.
- GPIO10 unused.
- Base relay unused.
- High/High coast, PWM/High, High/PWM remain unimplemented.

### Test procedure

```bash
cmake -S tests/motor_observe -B /tmp/vameter_motor_observe_test_build
cmake --build /tmp/vameter_motor_observe_test_build
/tmp/vameter_motor_observe_test_build/motor_observe_state_test
ctest --test-dir /tmp/vameter_motor_observe_test_build --output-on-failure
. $HOME/esp/esp-idf/export.sh && cd platforms/vameter && idf.py build
. $HOME/esp/esp-idf/export.sh && cd platforms/vameter && idf.py -B build_motor_observe_bringup -DCMAKE_CXX_FLAGS="-DMOTOR_OBSERVE_BRINGUP_AUTOSTART=1" build
cmake -S . -B build
cmake --build build -j8
```

### Result

- host-side test build: pass
- host-side test direct execution: pass
- CTest execution: pass, 1/1 tests passed
- device normal build: pass
- device bring-up build with `MOTOR_OBSERVE_BRINGUP_AUTOSTART=1`: pass
- desktop build: pass
- ESP-IDF configure note: component registry connection unavailable notice occurred during bring-up build configure; build continued using available dependencies
- device runtime CSV creation: 未検証

### Remaining 未検証

- device runtime CSV creation.
- actual brake stop CSV row on hardware.
- actual `stop` row on hardware.
- GPIO8/GPIO9 waveform while CSV logging is active.
- GPIO10 direct waveform while CSV logging is active.

### Rollback condition

- brake stop leaves `physical_output_allowed=1`.
- brake stop leaves non-zero `applied_target_percent`.
- `stop` row contains previous non-zero requested target.
- CSV logging changes PWM or safety behavior.
- GPIO10 or Base relay becomes part of Motor Observe logging or safety behavior.

## Motor Observe bring-up waveform check - initial armed state

- Date: 2026-04-29
- Branch / commit: 未記録
- Build: `build_motor_observe_bringup`
- Build flag: `MOTOR_OBSERVE_BRINGUP_AUTOSTART=1`
- Hardware:
  - VAMeter-Edu
  - MAKER-DRIVE: not connected
  - Motor: not connected
- Instrument:
  - Oscilloscope

### Checked items

- Firmware flashed successfully from WSL using `/dev/ttyACM0`.
- Bring-up UI started.
- Encoder long press changed state to `OutputArmed`.
- UI showed:
  - `State: OutputArmed`
  - `Target: 0%`
  - `Output: Low-Low`
- GPIO8 waveform:
  - 0 V, no change observed
- GPIO9 waveform:
  - 0 V, no change observed
- Encoder rotation in `OutputArmed`:
  - no target change
  - no waveform change

### Judgment

PASS for `OutputArmed` no-output behavior.

### Notes

- This matches the intended design:
  - `OutputArmed` keeps target 0.
  - `OutputArmed` does not emit PWM.
  - target changes are allowed only in `OutputEnabled`.

### 未確認

- `OutputEnabled` transition by encoder short press
- target +10% waveform
- target -10% waveform
- direction change waveform
- brake stop waveform
- GPIO10 no-PWM confirmation
- Base relay non-operation confirmation

### 未検証

- MAKER-DRIVE connection
- Motor connection
- Motor current / voltage / power measurement path

### Next action

- In `OutputArmed`, press encoder shortly to enter `OutputEnabled`.
- Confirm `OutputEnabled target 0` remains Low-Low.
- Rotate encoder right and confirm GPIO9 PWM / GPIO8 Low.
- Rotate encoder left and confirm GPIO9 Low / GPIO8 PWM.
- Confirm GPIO10 remains unused.

## 2026-04-29 Motor Observe bring-up waveform and basic motor check

### Date

2026-04-29

### Branch / commit

- branch: edu-dev
- commit: 未記録
- build: `build_motor_observe_bringup`
- build flag: `MOTOR_OBSERVE_BRINGUP_AUTOSTART=1`

### Firmware version

- APP_VERSION: v1.1.0-12-g1b9636e
- Flash:
  - result: pass
  - port: `/dev/ttyACM0`
  - baudrate: 1500000

### Target behavior

- no-motor bring-up UI and Motor Observe PWM backend verification
- `SafeDisabled / OutputArmed / Fault / timeout / leaveMode / target 0`: Low-Low
- `target > 0`: GPIO9 candidate PWM / GPIO8 candidate Low
- `target < 0`: GPIO9 candidate Low / GPIO8 candidate PWM
- High/High coast: not implemented
- PWM/High or High/PWM: not implemented
- GPIO10: not used
- Base relay: not used as a safety disconnect

### Hardware setup

Initial waveform check:

- VAMeter-Edu
- VAMeter Base
- MAKER-DRIVE: initially not connected
- Motor: initially not connected
- Instrument: oscilloscope

Basic motor check after waveform result looked acceptable:

- MAKER-DRIVE connected
- Small DC motor connected
- Motor supply: two alkaline cells in series
- Motor supply voltage: approximately 3 V
- Current-limited bench supply: not used
- Load condition: no intentional mechanical load

### Test procedure

1. Flash Motor Observe bring-up build.
2. Confirm bring-up UI starts.
3. Confirm encoder long press changes state from `SafeDisabled` to `OutputArmed`.
4. Confirm `OutputArmed` keeps:
   - `Target: 0%`
   - `Output: Low-Low`
   - GPIO8: 0 V
   - GPIO9: 0 V
5. Confirm encoder rotation in `OutputArmed` does not change target or waveform.
6. Encoder short press: `OutputArmed` to `OutputEnabled`.
7. Confirm `OutputEnabled target 0` remains Low-Low.
8. Rotate encoder right and confirm positive target behavior.
9. Rotate encoder left and confirm negative target behavior.
10. Confirm direction change behavior.
11. Encoder short press in `OutputEnabled` and confirm brake stop / target 0 / Low-Low.
12. Confirm Base relay does not appear to operate.
13. After no-motor waveform result looked acceptable, connect MAKER-DRIVE and motor for basic low-voltage motor check.
14. Confirm target operation drives motor and brake stop stops motor.

### Result

No-motor waveform check:

- `SafeDisabled`: pass
- `OutputArmed`: pass
  - UI changed to `OutputArmed`
  - `Target: 0%`
  - `Output: Low-Low`
  - GPIO8 stayed 0 V
  - GPIO9 stayed 0 V
- Encoder rotation in `OutputArmed`: pass
  - no target change
  - no waveform change
- `OutputEnabled target 0`: pass
  - Low-Low maintained
- `target > 0`: pass
  - expected PWM behavior observed
- `target < 0`: pass
  - expected PWM behavior observed
- direction change: pass
  - expected behavior observed
- brake stop: pass
  - target returned to 0
  - output returned to Low-Low

GPIO10 / Base relay:

- GPIO10 direct measurement: 未確認
- Base relay sound: no relay click heard
- Judgment: no observed Base relay operation, but GPIO10 remains 未確認

Basic motor check:

- Motor supply: alkaline cells x2, approximately 3 V
- Motor operation: pass
  - motor rotation observed with target command
  - brake stop behavior observed
- Motor current / voltage / power measurement: 未検証
- VAMeter measurement path: 未検証
- Load behavior: 未検証
- Stall behavior: 未検証
- Thermal behavior: 未検証
- Hand-load behavior: not tested

Video records:

- `MOV_4767`: OutputEnabled / target operation / direction behavior
- `MOV_4769`: brake stop behavior

### Judgment

条件付きPASS.

The Motor Observe bring-up UI and PWM backend behaved as expected for the checked states.

The transition sequence and output policy are consistent with the intended design:

- `OutputArmed` does not output PWM.
- target changes are accepted only in `OutputEnabled`.
- positive target and negative target drive different PWM sides.
- brake stop returns target to 0 and output to Low-Low.
- High/High coast is not used.
- PWM/High and High/PWM are not used.

However, GPIO10 direct waveform verification was not possible, so GPIO10 remains `未確認`.

The basic motor connection check passed at approximately 3 V with two alkaline cells, but current, thermal behavior, measurement path, and load behavior remain `未検証`.

### 未確認

- GPIO10 waveform during bring-up operation
- exact Base relay electrical state during operation
- branch / commit at test time
- VAMeter measurement values during motor operation

### 未検証

- current-limited supply operation
- motor current / voltage / power measurement
- motor current limit behavior
- load behavior
- stall behavior
- hand-load behavior
- thermal behavior of MAKER-DRIVE
- thermal behavior of motor
- VAMeter current / voltage / power measurement path for Motor Observe
- classroom-safe motor fixture
- repeated operation durability

### Next action

Before expanding UI or classroom use:

1. Define the Motor Observe measurement path.
2. Record where VAMeter measures current and voltage during MAKER-DRIVE motor operation.
3. Add a current-limited supply or fuse/protection condition before load testing.
4. Perform low-voltage, no-load current / voltage / power observation.
5. Keep High/High coast, PWM/High, and High/PWM unimplemented.
6. Keep Base relay out of the safety chain.

### Rollback condition

Rollback or stop Motor Observe motor testing if any of the following occurs:

- Output appears in `SafeDisabled` or `OutputArmed`.
- GPIO8/GPIO9 do not return to Low-Low on brake stop.
- Direction change produces simultaneous unexpected drive behavior.
- Base relay operates unexpectedly.
- GPIO10 is found to be driven by Motor Observe.
- Motor or MAKER-DRIVE heats abnormally.
- Current exceeds the planned classroom-safe limit.
- VAMeter measurement path cannot be explained.

## 2026-04-29 Motor Observe CSV download QR access

### Scope

- Add development-only access from Motor Observe bring-up to the existing local CSV download QR page.
- Keep normal launcher registration unchanged.
- Keep assets and localization unchanged.
- Keep GPIO10 and Base relay unused.
- Keep existing waveform `REC-*.csv` format unchanged.

### Implementation note

- Right-side switch short press in Motor Observe bring-up enters the existing CSV download QR page.
- Before QR entry, the app forces `SafeDisabled / target 0 / Low-Low`.
- Before QR entry, the app writes a CSV `stop` row and closes the current `MO-*.csv`.
- During QR display, Motor Observe output control and CSV appending do not continue.
- Returning from the QR page starts a new `MO-*.csv` session with a new `start` row.
- Local download names are limited to basename `REC-*.csv` or `MO-*.csv`.
- Paths, `..`, query strings, non-CSV suffixes, and other prefixes are rejected.

### Expected CSV state before QR

- `safety_state`: `SafeDisabled`
- `physical_output_allowed`: `0`
- `requested_target_percent`: `0`
- `applied_target_percent`: `0`
- `output_pattern`: `LOW_LOW`
- `event`: `stop`

### Measurement caveat

- `MO-*.csv` remains a development log, not a classroom UI.
- If `measurement_path_code=unknown`, V/I/P values must not be used for教材判断.

### Verification

- `cmake -S tests/motor_observe -B /tmp/vameter_motor_observe_test_build`: pass.
- `cmake --build /tmp/vameter_motor_observe_test_build`: pass.
- `/tmp/vameter_motor_observe_test_build/motor_observe_state_test`: pass.
- `ctest --test-dir /tmp/vameter_motor_observe_test_build --output-on-failure`: pass.
- `cmake -S . -B build`: pass.
- `cmake --build build -j8`: pass.
- `. $HOME/esp/esp-idf/export.sh && cd platforms/vameter && idf.py build`: pass.
- `. $HOME/esp/esp-idf/export.sh && cd platforms/vameter && idf.py -B build_motor_observe_bringup -DCMAKE_CXX_FLAGS="-DMOTOR_OBSERVE_BRINGUP_AUTOSTART=1" build`: pass.
- ESP-IDF configure reported that the component registry connection could not be established and skipped dependency-change checks, but both device builds completed using available dependencies.
- Existing `REC-*.csv` QR download path should continue to use the same QR page and web endpoint.

### Remaining 未検証

- actual QR display on VAMeter hardware.
- actual `MO-*.csv` download from the device AP.
- actual return from QR page and creation of a new `MO-*.csv` session on hardware.
- GPIO8/GPIO9 waveform during QR transition.
- GPIO10 direct waveform during QR transition.
