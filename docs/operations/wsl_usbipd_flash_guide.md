# WSL2 + usbipd + ESP-IDF Flash Guide

## Purpose

This document describes how to flash VAMeter-Edu firmware from Windows + WSL2 + usbipd + ESP-IDF.

This is a developer operation guide.  
It is not a classroom operation manual.

## Preconditions

- Windows host PC
- WSL2 Ubuntu
- ESP-IDF v5.1.3 environment
- VAMeter connected by USB
- `usbipd` available on Windows
- VAMeter-Edu repository cloned under WSL

Example repository path:

```bash
~/Dev/VAMeter-Edu
```

## Important notes

- Windows COM port names and WSL device names are different.
- In WSL, use /dev/ttyACM0 or /dev/ttyUSB0.
- Do not use ttyACM0 without /dev/.
- BUSID may change when the USB device is reconnected.
- If flash fails, check the port before changing firmware code.
- For Motor Observe bring-up, confirm that MAKER-DRIVE and motor are not connected before flashing.
- Do not treat the VAMeter Base relay as a safety disconnect.

## 1. Windows side: attach VAMeter to WSL

Connect VAMeter to the Windows PC by USB.

Open **Device Manager** and check that a USB serial device appears.

Then open **PowerShell as Administrator.**

List USB devices:

```powershell
usbipd list
```

Example:

```powershell
Connected:
BUSID  VID:PID    DEVICE                                                        STATE
1-4    303a:1001  USB シリアル デバイス (COM27), USB JTAG/serial debug unit     Shared
```

Attach the VAMeter USB device to WSL:

```powershell
usbipd attach --busid <BUSID> --wsl
```

Example:

```powershell
usbipd attach --busid 1-4 --wsl
```

If the device does not appear in WSL, detach and attach again:

```powershell
usbipd detach --busid <BUSID>
usbipd attach --busid <BUSID> --wsl
```

Example:

```powershell
usbipd detach --busid 1-4
usbipd attach --busid 1-4 --wsl
```

## 2. WSL side: check serial device

In the VS Code WSL terminal, check the serial device.

```bash
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
```

Expected example:

```bash
/dev/ttyACM0
```

Check with pyserial:

```bash
. $HOME/esp/esp-idf/export.sh
python -m serial.tools.list_ports
```

If no serial device appears, check kernel messages:

```bash
dmesg | tail -30
```

## 3. Flash normal build

Use `/dev/ttyACM0` or the actual device name shown in WSL.

```bash
. $HOME/esp/esp-idf/export.sh
cd ~/Dev/VAMeter-Edu/platforms/vameter
idf.py -p /dev/ttyACM0 -b 1500000 flash
```

## 4. Flash Motor Observe bring-up build

Use this only for developer bring-up.

This build must not be used for classroom release.

### 4.1 Flash existing bring-up build directory

If `build_motor_observe_bringup` has already been built:

```bash
. $HOME/esp/esp-idf/export.sh
cd ~/Dev/VAMeter-Edu/platforms/vameter
idf.py -B build_motor_observe_bringup -p /dev/ttyACM0 -b 1500000 flash
```

### 4.2 Build and flash with bring-up flag

```bash
. $HOME/esp/esp-idf/export.sh
cd ~/Dev/VAMeter-Edu/platforms/vameter
idf.py -B build_motor_observe_bringup \
  -DCMAKE_CXX_FLAGS="-DMOTOR_OBSERVE_BRINGUP_AUTOSTART=1" \
  -p /dev/ttyACM0 \
  -b 1500000 \
  flash
```

## 5. Common errors

### Error: `Could not open ttyACM0`

Cause:

- The port was specified as `ttyACM0` instead of `/dev/ttyACM0`.

Fix:

```bash
idf.py -p /dev/ttyACM0 -b 1500000 flash
```

### Error: `/dev/ttyACM0` does not exist

Possible causes:

- USB device is not attached to WSL.
- BUSID changed.
- VAMeter was reconnected.
- Windows still owns the device.

Fix:

1. Run on Windows PowerShell as Administrator:

```powershell
usbipd list
usbipd detach --busid <BUSID>
usbipd attach --busid <BUSID> --wsl
```

2. Run on WSL:

```bash
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
python -m serial.tools.list_ports
```

### Error: port is busy

Possible causes:

- `idf.py monitor` is still running.
- Another terminal is using the serial port.
- Another application is using the device.

Fix:

- Stop monitor.
- Close extra terminals.
- Detach and reattach USB device.

```powershell
usbipd detach --busid <BUSID>
usbipd attach --busid <BUSID> --wsl
```

### Error during flash transfer

Possible cause:

- Baudrate is too high for the current USB / WSL / usbipd environment.

Fix:

```bash
idf.py -p /dev/ttyACM0 -b 921600 flash
```

or

```bash
idf.py -p /dev/ttyACM0 -b 460800 flash
```

## 6. Motor Observe bring-up safety checklist

Before flashing or testing Motor Observe bring-up build:

- MAKER-DRIVE is not connected.
- Motor is not connected.
- GPIO8 / GPIO9 are observed only by oscilloscope or logic analyzer.
- GPIO10 is observed only to confirm it is not used for PWM.
- Base relay is not used as a safety disconnect.
- High/High coast is not implemented.
- PWM/High and High/PWM are not implemented.
- Expected output pattern:
  - SafeDisabled: Low-Low
  - OutputArmed: Low-Low
  - OutputEnabled target 0: Low-Low
  - OutputEnabled target > 0: GPIO9 PWM / GPIO8 Low
  - OutputEnabled target < 0: GPIO9 Low / GPIO8 PWM
  - brake stop: Low-Low

Record waveform verification results in:

```bash
docs/operations/safety_test_log.md
```

## 7. Recommended flash command summary

Normal build:

```bash
. $HOME/esp/esp-idf/export.sh
cd ~/Dev/VAMeter-Edu/platforms/vameter
idf.py -p /dev/ttyACM0 -b 1500000 flash
```

Motor Observe bring-up build:

```bash
. $HOME/esp/esp-idf/export.sh
cd ~/Dev/VAMeter-Edu/platforms/vameter
idf.py -B build_motor_observe_bringup \
  -DCMAKE_CXX_FLAGS="-DMOTOR_OBSERVE_BRINGUP_AUTOSTART=1" \
  -p /dev/ttyACM0 \
  -b 1500000 \
  flash
```
