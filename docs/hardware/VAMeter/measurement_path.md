# VAMeter Measurement Path

## Purpose

Prevent AI agents from confusing sensor values, display values, calibration-adjusted values, and CSV values.

## Known hardware

- INA226 x2 are used for different resolutions and ranges.
- INA226 addresses are 0x40 and 0x41.
- VAMeter official documentation describes current detection resolutions of 2.5 µA and 250 µA.

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

## Measurement paths by use case

### USB-C power measurement path

USB-C Power MonitorでVAMeterが測る経路は、VAMeter内部のUSB-C電力測定経路である。

現時点では、USB-C経路で使うINA226アドレス、raw measured value、filtered value、displayed value、CSV recorded valueの対応は未確認。

Motor ObserveのMAKER-DRIVE駆動電源またはモータ端子を測る経路とは別に扱う。

### Direct terminal / Normal Probe measurement path

VAMeter can measure an external circuit through its measurement terminals.
In the current Motor Observe bring-up setup, VAMeter is used as a standalone measuring instrument without an additional external probe accessory.

This path may be used to measure:

- driver input-side voltage / current / power
- motor terminal-side voltage / current / power
- other external circuit points, if the wiring is explicitly recorded

The measurement meaning depends on where VAMeter is inserted in the circuit.
Do not assume that VAMeter current equals motor winding current unless the wiring path is recorded and verified.

### Motor Observe intended measurement path

Motor ObserveでVAMeterが何を測るかは、接続する測定配線で決まる。

Motor Observe bring-up UIとPWM backendは、Port.A C9(White) / GPIO9候補とPort.A C8(Yellow) / GPIO8候補からMAKER-DRIVE M1A / M1Bへ制御信号を出すための経路であり、それ自体はVAMeterの電圧・電流測定経路ではない。

Motor Observeで測定対象にできる候補は次の通り。

| 測定対象 | VAMeterが測る意味 | Status |
|---|---|---|
| MAKER-DRIVEの電源入力側 | VB+ / VB-に入る駆動電源の電圧、または駆動電源からドライバへ流れる入力電流 | 未検証 |
| モータ端子側 | MAKER-DRIVE出力端子からモータに加わる端子電圧、またはモータ側配線を通る電流 | 未検証 |
| VAMeter内部USB-C経路 | USB-C経路の電力 | Motor Observeモータ経路との関係は未確認 |
| Normal Probe経路 | 外部配線で選んだ測定点 | Motor Observeでの具体配線は未検証 |

現時点では、Motor Observeのmotor current / voltage / power measurementは未検証。
教材化前に、測定配線、VAMeter表示値、記録値、測定対象の物理的意味を同じ記録に残す。

## MAKER-DRIVE input-side measurement

MAKER-DRIVEの電源入力側を測る場合、VAMeterは駆動電源からMAKER-DRIVEへ入る電圧または電流を測る。

この場合の電流は、ドライバ入力電流である。
PWM駆動、内部Hブリッジ、回生またはブレーキ状態の影響により、ドライバ入力電流とモータ巻線電流が常に一致するとは限らない。

現時点では、VAMeterでMAKER-DRIVE入力電流をどの配線で安全に測るかは未検証。

## Motor-terminal measurement

モータ端子側を測る場合、VAMeterはMAKER-DRIVEの出力端子とモータの間の電圧または電流を測る。

この場合はPWMでスイッチングされた波形を含む可能性がある。
瞬時値、平均値、VAMeter表示値、CSV recorded valueは同じ意味とは限らない。

VAMeterのサンプリング、フィルタ、表示更新、CSV記録がPWM瞬時波形を完全に再現するとは限らない。
PWM瞬時波形の確認には、必要に応じてオシロスコープまたはロジックアナライザを使う。

現時点では、モータ端子側の電圧、電流、電力測定は未検証。

## Rules

- Do not infer sensor path from UI color or app name.
- Do not use displayed strings as calculation sources.
- Do not change CSV source without updating waveform CSV policy and validation log.
- Do not treat INA226 ALERT pins as measurement-value sources unless firmware behavior is verified.
- For Motor Observe, record whether a value is driver input-side value or motor-terminal-side value.
- Do not treat driver input current as motor winding current unless the wiring and waveform behavior are verified.
- Do not treat VAMeter display values as complete PWM instantaneous waveforms.
