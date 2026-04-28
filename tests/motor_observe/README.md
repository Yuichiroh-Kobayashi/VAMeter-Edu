# Motor Observe host-side test

この test は Motor Observe の `SafetyController`、`BackendController`、`NoopBackend` の状態遷移を確認する host-side test です。

GPIO / PWM / MAKER-DRIVE / モータ接続は行いません。device build とは別に、標準C++で実行します。

## 実行手順

```bash
cmake -S tests/motor_observe -B /tmp/vameter_motor_observe_test_build
cmake --build /tmp/vameter_motor_observe_test_build
/tmp/vameter_motor_observe_test_build/motor_observe_state_test
```

## CTest

```bash
ctest --test-dir /tmp/vameter_motor_observe_test_build --output-on-failure
```

## device build

この test は firmware runtime には組み込みません。device build は別途確認します。

```bash
. $HOME/esp/esp-idf/export.sh
cd platforms/vameter
idf.py build
```

## 未確認 / 未検証

host-side test で確認できるのはソフトウェア上の状態遷移です。GPIO、PWM、Port.A、MAKER-DRIVE、モータ接続、実測経路は未検証として扱います。

この test が fail した場合は、Motor Observe の物理出力実装へ進まないでください。
