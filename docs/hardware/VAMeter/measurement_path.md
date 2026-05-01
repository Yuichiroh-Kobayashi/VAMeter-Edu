# VAMeter Measurement Path

## Purpose

Prevent AI agents from confusing sensor values, display values, calibration-adjusted values, and CSV values.

## Known hardware

- INA226 x2 are used for different resolutions and ranges.
- INA226 addresses are 0x40 and 0x41.
- VAMeter official documentation describes current detection resolutions of 2.5 µA and 250 µA.
- VAMeter V1.2 schematic shows INA226-based sensing with `VBUS`, `IN+`, and `IN-`.
- INA226 can be used for bidirectional shunt-current measurement, but its bus-voltage input is specified for non-negative bus voltage relative to its measurement ground.
- Therefore, bidirectional current capability must not be interpreted as permission to apply reverse terminal voltage to VAMeter.

## To verify in firmware

| Item | Status |
|---|---|
| INA226 address used for low-current path | 未確認 |
| INA226 address used for high-current path | 未確認 |
| HICUR behavior and threshold | 未確認 |
| raw-to-filtered conversion | 未確認 |
| filtered-to-displayed conversion | 未確認 |
| displayed-to-CSV relationship | 未確認 |
| calibration-adjusted value source | 未確認 |
| reverse terminal voltage tolerance | NoGO / not supported as a Motor Observe measurement path |

## Measurement paths by use case

### USB-C power measurement path

USB-C Power MonitorでVAMeterが測る経路は、VAMeter内部のUSB-C電力測定経路である。

現時点では、USB-C経路で使うINA226アドレス、raw measured value、filtered value、displayed value、CSV recorded valueの対応は未確認。

Motor ObserveのMAKER-DRIVE駆動電源またはモータ端子を測る経路とは別に扱う。

### Direct terminal / Normal Probe measurement path

VAMeter can measure an external circuit through its measurement terminals.
In the current Motor Observe bring-up setup, VAMeter is used as a standalone measuring instrument without an additional external probe accessory.

This path may be used to measure only when the terminal polarity and current path are explicitly defined:

- driver input-side voltage / current / power
- other external circuit points, if the wiring is explicitly recorded

Do not use this path for H-bridge motor terminal voltage that reverses polarity.

The measurement meaning depends on where VAMeter is inserted in the circuit.
Do not assume that VAMeter current equals motor winding current unless the wiring path is recorded and verified.
Do not assume that VAMeter supports reverse terminal voltage.

### Motor Observe intended measurement path

Motor ObserveでVAMeterが何を測るかは、接続する測定配線で決まる。

Motor Observe bring-up UIとPWM backendは、Port.A C9(White) / GPIO9候補とPort.A C8(Yellow) / GPIO8候補からMAKER-DRIVE M1A / M1Bへ制御信号を出すための経路であり、それ自体はVAMeterの電圧・電流測定経路ではない。

Motor Observeで測定対象にできる候補は次の通り。

| 測定対象 | VAMeterが測る意味 | Status |
|---|---|---|
| MAKER-DRIVEの電源入力側 | VB+ / VB-に入る駆動電源の電圧、または駆動電源からドライバへ流れる入力電流 | 条件付き確認済み |
| モータ端子側 | MAKER-DRIVE出力端子からモータに加わる端子電圧、またはモータ側配線を通る電流 | VAMeter単体ではNoGO |
| VAMeter内部USB-C経路 | USB-C経路の電力 | Motor Observeモータ経路との関係は未確認 |
| Normal Probe経路 | 外部配線で選んだ測定点 | Motor Observeでの具体配線は未検証 |

現時点では、Motor Observeのmotor winding current / motor terminal voltage measurementは未検証かつVAMeter単体ではNoGO。
教材化前に、測定配線、VAMeter表示値、記録値、測定対象の物理的意味を同じ記録に残す。

## MAKER-DRIVE input-side measurement

MAKER-DRIVEの電源入力側を測る場合、VAMeterは駆動電源からMAKER-DRIVEへ入る電圧または電流を測る。

Motor Observe low-voltage no-load tests confirmed that the following wiring establishes a valid driver input-side observation path:

- power + -> VAMeter IN -> VAMeter OUT -> MAKER-DRIVE VB+
- power - -> MAKER-DRIVE VB-

Observed meaning:

- valid as driver input-side voltage / current / power observation
- not motor winding current
- not motor terminal voltage
- not pulling force

This path should be recorded as `driver_input_inline` once firmware writes the measurement path code into `MO-*.csv`.

## Motor-terminal measurement

モータ端子側をVAMeter単体で測る計画は、現時点ではNoGOとする。

理由:

- MAKER-DRIVEはH-bridge出力であり、モータ端子間の極性が方向により反転する。
- VAMeterのINA226 bus-voltage inputは逆電圧入力を前提にしない。
- PWMでスイッチングされた波形を含む。
- 瞬時値、平均値、VAMeter表示値、CSV recorded valueは同じ意味とは限らない。

VAMeterのサンプリング、フィルタ、表示更新、CSV記録がPWM瞬時波形を完全に再現するとは限らない。
PWM瞬時波形の確認には、必要に応じてオシロスコープまたはロジックアナライザを使う。

モータ端子側の電圧確認が必要な場合は、VAMeterではなくオシロスコープ、差動プローブ、または適切な絶縁測定器を使う。

モータ側電流が必要な場合は、双方向対応の外付け電流センサ、電流プローブ、または絶縁アンプを別設計とする。

### NoGO: reverse terminal voltage

Do not connect VAMeter across a polarity-reversing H-bridge motor output.

NoGO examples:

- MAKER-DRIVE M1A / M1B directly across VAMeter voltage terminals
- VAMeter terminal polarity reverses when motor direction changes
- VAMeter output terminal is left floating while motor output is connected to input
- interpreting VAMeter values as motor terminal voltage during PWM drive

Judgment:

- not valid for Motor Observe CSV measurement
- not valid for classroom use
- not valid for motor terminal voltage
- not valid for motor winding current

## NoGO measurement path example

### Motor output connected to VAMeter input only

Wiring:

- MAKER-DRIVE motor output -> VAMeter input
- VAMeter output: not connected

Judgment:

- Not valid for current measurement.
- VAMeter In-Out current path is open.
- Do not interpret current as motor current.
- Do not interpret power as motor power.
- Use only as an observation-only check if explicitly recorded.

## Rules

- Do not infer sensor path from UI color or app name.
- Do not use displayed strings as calculation sources.
- Do not change CSV source without updating waveform CSV policy and validation log.
- Do not treat INA226 ALERT pins as measurement-value sources unless firmware behavior is verified.
- For Motor Observe, record whether a value is driver input-side value or motor-terminal-side value.
- Do not treat driver input current as motor winding current unless the wiring and waveform behavior are verified.
- Do not treat VAMeter display values as complete PWM instantaneous waveforms.
- Do not apply reverse terminal voltage to VAMeter.
- Treat motor-terminal measurement with VAMeter as NoGO unless a separate design review verifies polarity, isolation, waveform, and device limits.
