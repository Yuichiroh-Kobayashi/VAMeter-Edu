# WSL / usbipd ESP32 transport guidance

## Scope and authority

This is development/toolchain guidance for ESP32-S3 USB Serial/JTAG physical
validation through Windows, WSL2 Ubuntu, usbipd-win, and ESP-IDF/esptool. Follow
the [physical-validation and rollback policy](../ai/physical-validation-and-rollback.md)
for authorization, hardware-safe state, capture, rollback coverage, and evidence
handling. This procedure does not grant permission to operate a device and is
neither release authority nor a dated validation record.

Keep USB/IP transport availability separate from source, build, firmware, and
device runtime state. In project validation, Windows reported `Attached` while
the WSL by-id link and CDC ACM node were absent, including before esptool ran.
That observation alone cannot establish a firmware reset or esptool as the cause.
An active auto-attach loop also did not guarantee continued guest availability.
These are observed failure modes, not a claim that auto-attach is generally broken.

For each physical run, use this identity hierarchy:

1. The expected USB VID:PID and serial, or an explicitly reviewed equivalent
   stable hardware identity, identify the intended device.
2. The exact WSL `/dev/serial/by-id/...` link identifies its expected interface;
   correlate it with that hardware identity in sysfs.
3. The resolved `/dev/ttyACM*` node is a transport detail only. Its number can
   change after re-enumeration without a change of device identity.

Windows `usbipd list` showing `Attached` is not sufficient proof that WSL can
access the device. Even a successful guest identity check establishes availability
only at that instant; it does not prove firmware health or a completed operation.

## Just-in-time one-shot attach

Prepare and verify offline inputs and evidence destinations before attaching.
Use the tool versions approved for the run. The device must already be shared
with usbipd and user permissions configured; installation, binding, and permission
setup belong outside a sealed validation gate. Confirm which host/process owns
the interface, including any serial observer. Do not detach an active capture or
compete for its port; use the run's approved ownership handoff. See the
[Windows serial observation guide](wsl-windows-serial-observation.md) for capture
when Windows owns the interface.

For a controlled operation, prefer a just-in-time one-shot attach unless the
run has a different validated procedure. This reduced transport exposure and
was more reliable than a long-lived auto-attach session in project validation;
it is not a universal reliability guarantee. Select the current BUSID for the
intended device; it is not a permanent hardware identity. Replace placeholders
before running the following commands in Windows PowerShell:

```powershell
usbipd list
usbipd detach --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
usbipd list
```

Run detach only as part of the authorized handoff/recovery, when needed. Inspect
each command's result and stop on errors; this is not an automatic retry loop.
Keep the intended WSL distribution running. The sharing and attach prerequisites
are described in [Microsoft's WSL USB guide](https://learn.microsoft.com/en-us/windows/wsl/connect-usb)
and the [usbipd-win project](https://github.com/dorssel/usbipd-win).

Immediately in the WSL execution environment that will perform the operation:

```bash
PORT='/dev/serial/by-id/<EXPECTED_DEVICE>'

test -e "$PORT" || { echo 'STOP: exact by-id absent'; exit 1; }
readlink -f "$PORT"
ls -l "$PORT"
```

An authorized, bounded wait for that same exact by-id may accommodate enumeration.
Record its duration and result. On timeout, STOP; do not fall back to another tty.

### Correlate the port with sysfs identity

Set expected values from the run authority. This example walks the resolved
tty's ancestors to its USB device, so matching an unrelated USB device elsewhere
in sysfs cannot accidentally authorize this port:

```bash
EXPECTED_VID='<EXPECTED_VID>'
EXPECTED_PID='<EXPECTED_PID>'
EXPECTED_SERIAL='<EXPECTED_SERIAL>'

RESOLVED=$(readlink -f "$PORT") || exit 1
test -c "$RESOLVED" || { echo 'STOP: device node absent'; exit 1; }
TTY=${RESOLVED##*/}
USB_SYS=$(readlink -f "/sys/class/tty/$TTY/device") || exit 1
while [ "$USB_SYS" != / ] && [ ! -f "$USB_SYS/idVendor" ]; do
    USB_SYS=${USB_SYS%/*}
    [ -n "$USB_SYS" ] || USB_SYS=/
done
test -r "$USB_SYS/idVendor" && test -r "$USB_SYS/idProduct" &&
    test -r "$USB_SYS/serial" || { echo 'STOP: USB identity unavailable'; exit 1; }
VID=$(cat "$USB_SYS/idVendor") || exit 1
PID=$(cat "$USB_SYS/idProduct") || exit 1
SERIAL=$(cat "$USB_SYS/serial") || exit 1
printf 'port=%s\nresolved=%s\nsysfs=%s\nvidpid=%s:%s\nserial=%s\n' \
    "$PORT" "$RESOLVED" "$USB_SYS" "$VID" "$PID" "$SERIAL"
test "$VID:$PID" = "$EXPECTED_VID:$EXPECTED_PID" &&
    test "$SERIAL" = "$EXPECTED_SERIAL" || { echo 'STOP: identity mismatch'; exit 1; }
id
ls -l "$RESOLVED"
test -r "$PORT" && test -w "$PORT" || { echo 'STOP: insufficient access'; exit 1; }
```

Record UTC time, tool versions, identity, resolved node, permissions, and ownership
in the run evidence. Recheck exact guest identity immediately before mutation
and after any re-enumeration. If descriptors or USB mode change, review that
change rather than silently accepting a new identity.

## Auto-attach and STOP rules

Auto-attach availability and behavior depend on the installed usbipd-win version;
inspect `usbipd attach --help`. It may be useful interactively. Neither a running
auto-attach loop nor Windows `Attached` replaces guest-side identity verification.

Before mutation, STOP when the exact by-id is absent, VID:PID/serial mismatch,
Windows reports Attached but the guest sees no device, ownership is ambiguous,
or permissions are insufficient. A transport STOP before any mutation command
means no firmware mutation was performed by that attempt; it is not firmware failure.

Do not guess another ttyACM port, change device permissions with chmod/chown in
a sealed gate, or silently use sudo to bypass access failure. Do not automatically
detach/attach and continue after a mutation failure, or retry write/erase commands.
Preserve the attempt and obtain the next recovery action under the physical policy.

## Command outcome and subsequent transport loss

Keep command outcome, built-in verification, exact readback, and boot/runtime
qualification as separate evidence:

| Case | Interpretation and next action |
|---|---|
| A. Command failed before a verified write | Preserve exit code and complete log. If erase/write may have started, flash may be partially changed; do not claim unchanged bytes. STOP without automatic retry or repair. |
| B. Command returned success and built-in verification passed | Preserve `write_rc=0` and the installed tool's verification output (for example, `Hash of data verified.`). This proves the recorded command-level result, not boot success or untouched-region equality. |
| C. Transport disappeared only after B | Retain the successful verified-write result. Later USB/IP loss does not retroactively make that write fail. Keep exact post-write readback pending and perform it in a separately authorized gate after transport recovery. |

Preserve the actual device-command exit code when logging through a pipeline;
the logger's success is not the device command's success. A failure, missing
verification, or ambiguous chronology is not case B or C.

esptool can enter the ROM bootloader and hard-reset the device, with USB
re-enumeration and loss of runtime RAM state. Record the reset messages and
observed disappearance/recovery. A changed ttyACM number alone is not an identity
change. A missing node alone does not prove that a reset occurred, particularly
when it disappeared before esptool was invoked.

## Reduce transport exposure

Where practical, design a mutation gate around one device operation and seal its
successful mutation evidence before a later readback gate. Do not remove required
identity, rollback, capture, or verification checks just to reduce command count.

For explicitly authorized adjacent or continuous flash ranges, one `read_flash`
operation can reduce repeated connections and resets. Verify start, exclusive
end, total length, and partition boundaries against the approved layout. Any gap
included in a combined window must also be authorized; never expand read scope
solely for convenience. Preserve the full window, validate its byte count, then
split partitions offline using offsets relative to the window start. Verify each
region's byte count and hash, retaining expected-versus-actual comparisons.

## Compact transport diagnostics

In Windows PowerShell, capture the installed capabilities and current host state:

```powershell
usbipd --version
usbipd list
usbipd attach --help
```

In the actual WSL environment, capture guest visibility without opening serial:

```bash
ls -l /dev/serial/by-id
ls -l /dev/ttyACM* 2>/dev/null || true
readlink -f "$PORT"

for vendor_file in /sys/bus/usb/devices/*/idVendor; do
    [ -f "$vendor_file" ] || continue
    usb_device=${vendor_file%/idVendor}
    printf 'sysfs=%s\n' "$usb_device"
    for attribute in idVendor idProduct manufacturer product serial; do
        if [ -r "$usb_device/$attribute" ]; then
            printf '%s=%s\n' "$attribute" "$(cat "$usb_device/$attribute")"
        fi
    done
done
```

The scan includes `/sys/bus/usb/devices/*/idVendor`, `*/idProduct`, and `*/serial`.
It is an inventory; bind the intended port to its own sysfs ancestor as above
before using it as authority. If available, read
`/sys/devices/platform/vhci_hcd.0/status` for VHCI attachment status; paths may
vary by kernel. `lsusb` is optional. Do not install usbutils just for a validation gate.

Recent `dmesg` output, when accessible without changing privileges, can distinguish
USB/IP connection closure, attachment, USB disconnect, and CDC ACM node creation.
Retain chronology and timestamp basis; do not infer unobserved Windows events
from WSL output. Kernel chronology is diagnostic evidence, not device identity
authority by itself. If host and guest disagree, report the disagreement and the
execution environment instead of diagnosing a firmware/build defect from it.

## Historical evidence boundary

Historical failed attempts remain valid historical records. A later successful
transport recovery does not rewrite a prior STOP into PASS. Retain failed,
partial, and successful attempts separately with their original scope and results.
These durable documents describe future procedure, not retrospective evidence
reclassification.
