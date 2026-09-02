# WSL / Windows Serial Observation Guide

## 1. Purpose and scope

This guide describes a robust way to observe VAMeter-Edu serial output when project tools run in WSL but the ESP32-S3 serial interface is owned by Windows. It is intended for debugging and validation work that needs a durable record of reset, boot, and runtime output. It does not replace electrical-safety review, firmware build verification, browser testing, or hardware validation.

The guide uses the following labels to keep different kinds of statements separate:

- **Observed fact:** something recorded in a specific VAMeter-Edu test environment.
- **Operational recommendation:** a practice that reduces ambiguity or failure risk but is not universal ESP32-S3 behavior.
- **Hypothesis:** a possible explanation that has not been established as the cause.
- **Product requirement:** behavior required of VAMeter-Edu itself.
- **Validation-only control:** an extra control used to make a validation result attributable and reproducible; it is not necessarily product behavior.

Unless a statement is explicitly identified as a product requirement, the observer, identity, timing, staging, and record-handling controls in this guide are validation-only controls or operational recommendations. They must not be presented as firmware features or end-user requirements.

## 2. WSL / Windows / ESP32-S3 serial topology

In this topology, Windows owns the USB serial device, its PNP identity, and its COM port. A Windows-native observer keeps the serial port open and writes raw bytes and health metadata to an NTFS staging directory. WSL may prepare inputs and later import the completed validation record, but it should not act as a second serial reader.

The important boundaries are:

```text
ESP32-S3 USB serial -> Windows COM owner -> Windows observer -> NTFS staging
                                                       ^
WSL orchestration -------- pre-created trigger files ---+
WSL validation record import <-- completed files --------
```

This arrangement is especially useful when connecting the Windows host to the VAMeter-Edu access point may disrupt the host's normal network route or WSL-to-Windows interop.

## 3. Device identity

Record device identity before opening the serial port and again after any re-enumeration:

- USB vendor ID and product ID (`VID:PID`);
- the complete Windows `PNPDeviceID`, including interface and instance information;
- the Windows device description and driver;
- the current COM number;
- the observation time and host identity needed to interpret the record.

For example, PowerShell can inventory serial devices without opening them:

```powershell
Get-CimInstance Win32_SerialPort |
  Select-Object DeviceID, Name, PNPDeviceID
```

The COM number is useful diagnostic information, but it is not stable device authority. It can change after reconnection, driver changes, a different USB port, or USB re-enumeration. Match the expected device using VID:PID and the complete PNP identity, then record whichever COM number Windows assigned for that observation.

Do not publish a device-specific PNP instance suffix as a universal value. If the product, USB mode, or driver changes, review the expected VID:PID and interface identity rather than silently accepting the old pattern.

## 4. Serial-open behavior

Configure the designated serial observer explicitly:

- `DtrEnable = false`;
- `RtsEnable = false`;
- `Handshake = None`;
- the baud rate and framing expected by the firmware;
- binary-safe capture of the raw byte stream.

**Observed fact:** in the VAMeter-Edu validation environment, `USB_UART_CHIP_RESET` was observed after some serial-observer opens. The lower-level cause was not established. The procedure therefore records the raw reset and boot fields and re-establishes the expected firmware identity before beginning product validation.

Do not generalize this observation into “opening the serial port always resets ESP32-S3.” A line-state transition, driver behavior, USB re-enumeration, firmware behavior, or another interaction may be a hypothesis, but it is not an established cause without separate evidence.

Preserve the complete raw reset and boot lines, including all reported fields. Do not treat one value such as `boot:0x29` as the only valid boot value; later observations may contain other legitimate raw values, including `boot:0x2b`. Interpret the whole boot sequence and expected firmware identity rather than admitting or rejecting a run from one field alone.

If an open is followed by a reset or re-enumeration, wait for the device to return, confirm its VID:PID and complete PNP identity, and establish the expected firmware version or build identity again. Only then may a validation interval begin.

## 5. Observer lifecycle

A validation-capable serial observer should expose an explicit lifecycle:

1. **Starting:** validate configuration, target identity, output paths, and exclusive port ownership.
2. **Capture armed:** the output files and serial port are open, timestamps are initialized, and the observer is ready before a controlled stimulus occurs.
3. **Running:** record raw bytes, monotonic timestamps, liveness, byte progress, serial errors, and device re-enumeration.
4. **Stopping:** accept a predetermined stop signal, stop reading, flush buffered data, and close the port.
5. **Final summary:** write the start/end times, byte count, first/last byte times, transport epochs, exceptions, identity observations, and whether shutdown completed cleanly.

Liveness and byte progress are different signals. A heartbeat proves that the observer process is responsive; an increasing byte count proves that serial data is reaching it. Record both.

A **transport epoch** is one uninterrupted connection held by the designated serial observer. Increment the epoch after a close/reopen, port loss, or device re-enumeration. Do not concatenate epochs and describe them as one continuous observation without marking their boundaries.

The stop path should be deterministic. Prefer a single pre-agreed trigger file or other bounded control that causes one final flush and summary. A process killed without a final summary cannot prove that its last buffered bytes were preserved.

## 6. Avoid competing COM readers and live-output readers

Two independent interference classes matter: ownership contention on the serial device and file-sharing contention on output files that the designated serial observer is still writing.

### Serial-device ownership and COM-port contention

Use exactly one designated serial observer for the validation interval. Close or disable:

- a second serial-observer process;
- an IDE or ad-hoc serial monitor;
- terminal programs that automatically reconnect;
- scripts that probe or open the COM port.

A second COM reader can fail to open the port, take ownership first, change DTR/RTS behavior, trigger re-enumeration, consume bytes that the designated observer needed, or make it unclear which process produced the record.

### Active output-file sharing contention

`tail` and `Get-Content` can read an ordinary NTFS output file without opening the COM port. This is a separate interference class from serial-device ownership.

**Observed fact:** in the VAMeter-Edu validation environment, a second process did not need to open the COM port to disturb the designated serial observer. Reading an active, still-growing output file while the observer was writing it coincided with writer-side file-access contention in two runs: once when a WSL process continuously followed the file through `/mnt/c` using `tail -F`, and once when a separate Windows-native PowerShell process repeatedly performed short-lived `Get-Content -Tail 5` reads without `-Wait`.

In both observations, the writer subsequently closed and reopened, causing transport-epoch churn. A new transport epoch created this way does not by itself demonstrate a device reset, serial-device loss, or another corresponding device-side event. These observations are specific to this validation environment; they do not establish that every content read interferes with every Windows, .NET, filesystem, or observer configuration.

The default live-content policy is:

- **Routine or polling content reads of an active observer-owned output file: do not use.**
- **`tail -F` or `Get-Content -Wait` against an active observer-owned output file: do not use.**
- **Periodic `Get-Content` against an active observer-owned output file: do not use as a health or progress mechanism.**

Prefer reading content only after the writer has finalized the file. If a controlled procedure genuinely requires a live-content checkpoint, treat it as an explicitly planned exception, keep it infrequent, and recognize that it still carries non-zero interference risk unless the writer's sharing behavior has been separately verified.

A later clean validation run used process and file metadata for routine liveness and progress, with only a small number of deliberate checkpoint content reads. That experience supports the metadata-first recommendation; it does not prove that live content reads are risk-free.

### Check observer health without reading the active capture

During a controlled validation interval, do not use the content of an active writer-owned output file as the routine health or progress probe. Prefer:

- process liveness from `Get-Process` or the process table;
- file growth from metadata such as `Length` and `LastWriteTimeUtc`;
- dedicated health or liveness metadata emitted by the observer;
- an observer-owned byte counter, or file-length change, for byte progress.

For example, PowerShell can inspect process and file metadata without reading file content:

```powershell
Get-Process -Id <observer-pid>
Get-Item '<active-output-file>' |
  Select-Object Length, LastWriteTimeUtc
```

These process and metadata checks were the non-interfering discipline used after the observed incidents and should be preferred over content reads of the active writer-owned file. This does not establish that `Get-Item` is universally incapable of interference on every storage or software stack.

Before the validation interval, confirm both that the designated observer is the sole COM owner and that no second process is following or polling its active output files. If either kind of contention is detected, classify the attempt as an observer/infrastructure problem and restart from a fresh validation interval after resolving it.

## 7. WSL-to-Windows interop pitfalls

**Observed fact:** a WSL-to-Windows interop/vsock failure was observed in one validation run. A Windows launch attempted from WSL failed with `UtilBindVsockAnyPort` before the intended Windows process started.

**Operational recommendation:** validation-critical Windows processes should therefore avoid depending on a new WSL-to-Windows launch after a disruptive network transition. Start the Windows-native serial observer, browser helper, and other critical Windows processes before changing the host network connection.

Keep those processes persistent and let them react to small, pre-agreed NTFS trigger files. This leaves the critical control path on Windows even if WSL-to-Windows process launch becomes unavailable. A trigger file should contain only the minimum command state, use an unambiguous name and encoding, and never carry credentials.

This is not a claim that WSL can never launch Windows tools after Wi-Fi switching. It is a failure mode observed in one environment and a reason to remove that dependency from the critical interval.

## 8. Offline handoff before network transition

Before switching the Windows host to a device access point or otherwise disrupting the normal network path:

1. copy or verify every required Windows-native tool locally;
2. freeze the exact observer configuration and critical command arguments;
3. create collision-free NTFS staging and trigger locations;
4. start the required Windows processes and confirm their liveness;
5. arm serial capture and confirm byte progress where output is expected;
6. record the target device identity and expected firmware identity;
7. verify that stop/finalization can occur without a new WSL-to-Windows launch;
8. only then perform the network transition.

Do not plan to download dependencies, fetch instructions, construct a new critical command, or start a new Windows helper after the transition. Preparation should leave a bounded, offline-capable path to complete or safely stop the observation.

## 9. Human preparation vs product validation interval

Human preparation may include connecting USB, starting the observer, opening the serial port, handling an open-time reset, selecting the device access point, and confirming the test setup. These actions belong to preparation, not automatically to the product validation interval.

Define the validation interval explicitly after:

- the designated serial observer is armed and healthy;
- any open-time reset has completed;
- the expected USB/PNP and firmware identities have been re-established;
- the test's required initial product state is present.

Record the beginning and end of the validation interval. If a reset, port loss, observer restart, or unplanned transport epoch occurs inside it, report that event and decide the result using the test's stated acceptance criteria. Do not erase the event by shifting the interval afterward.

## 10. Observer health and absence claims

An absence claim such as “no reset appeared” or “no matching diagnostic was emitted” is supported only when:

- the observer remained alive for the complete validation interval;
- the serial transport epoch was continuous, or every boundary was explicitly allowed;
- byte progress or an independent expected heartbeat showed that the path was carrying data;
- the raw stream was retained rather than only filtered matches;
- the searched pattern, time window, encoding, and search result were recorded;
- finalization completed cleanly.

Liveness without byte progress is not enough when bytes were expected. A zero-match result from an unhealthy, disconnected, truncated, or ambiguously encoded observer is inconclusive, not evidence that the device produced no event.

Do not prove byte progress by repeatedly opening and reading the active capture file from another process. Prefer observer-owned byte counters, file-length and last-write metadata, and dedicated health or liveness records. Search or read captured content only after writer finalization where possible. If a time-bounded validation needs a pattern check before finalization, use a deliberately designed read-safe snapshot or summary when available rather than polling the active writer-owned file.

## 11. Validation record durability

Use an NTFS staging directory owned by the persistent Windows processes. Preserve raw serial bytes, decoded text, observer-health events, identity inventories, timestamps, the final summary, and the exact configuration used.

After a clean stop:

1. close all writers;
2. inventory each relative path, byte length, and SHA-256;
3. write and verify a `SHA256SUMS` manifest using relative paths;
4. import the completed staging set once into its final record location;
5. verify path, byte length, and SHA-256 after import;
6. make the completed package read-only or place it in a read-only archive.

Do not overwrite a failed or incomplete attempt with a retry. Keep the original record and use a new, collision-free staging location. Exclude credentials, private network information, personal data, and machine-specific absolute paths from anything intended for publication.

## 12. Failure classification examples

| Observation | Appropriate classification | Do not claim |
|---|---|---|
| Windows observer never reaches capture armed | Observer/infrastructure failure | Firmware or device failure |
| Open-time `USB_UART_CHIP_RESET`, followed by verified recovery before the validation interval | Recorded setup event | Universal serial-open behavior |
| Reset or port loss during a required continuous interval | Interrupted/invalid validation attempt | Continuous product validation |
| `UtilBindVsockAnyPort` prevents a Windows helper from launching | WSL/Windows interop failure | Browser, firmware, or device failure |
| A second process owns or probes the COM port | Serial-reader contention | Reliable absence of device output |
| Another process reads an active observer output file and the writer closes/reopens | Observer/file-sharing interference | Device reset, serial-device loss, or product transport failure without independent device evidence |
| File length or the observer-owned byte counter stops changing while the process remains alive | Inconclusive observer/transport condition; investigate health | “The device emitted nothing” |
| PNP or firmware identity differs from the expected target | Identity mismatch; stop and investigate | Result for the intended target |
| Healthy observer captures an expected product error during the stated interval | Product observation within that test's scope | Broader release or safety conclusion |

## 13. Recommended checklist

- [ ] Record Windows, WSL, observer, and driver versions relevant to the run.
- [ ] Inventory VID:PID, complete PNP identity, device description, and current COM number.
- [ ] Confirm that no second process is opening the COM port.
- [ ] Confirm that no `tail -F`, `Get-Content -Wait`, or periodic content reader is following active observer-owned output files.
- [ ] Use process state, file metadata, or observer-owned counters for routine liveness and byte-progress checks.
- [ ] Prefer content inspection after writer finalization; treat any live-content checkpoint as exceptional.
- [ ] Prepare local tools, exact inputs, NTFS staging, and trigger files before network transition.
- [ ] Start critical Windows-native processes before network transition.
- [ ] Configure `DTR=false`, `RTS=false`, and `Handshake=None` explicitly.
- [ ] Reach capture armed and confirm observer liveness and byte progress.
- [ ] Preserve raw reset/boot fields and handle any open-time reset before the validation interval.
- [ ] Re-establish expected USB/PNP and firmware identity.
- [ ] Record explicit validation-interval start and end times.
- [ ] Record every transport epoch, port loss, re-enumeration, and observer exception.
- [ ] Stop deterministically and require a final summary.
- [ ] Verify relative paths, byte lengths, SHA-256 values, and `SHA256SUMS`.
- [ ] Import once, verify again, and retain a read-only completed package.

## 14. Known limitations

- The observed reset and interop behavior is environment-specific evidence, not a universal rule for ESP32-S3, Windows, or WSL.
- Deasserting DTR/RTS reduces ambiguity but does not prove that no driver, USB, firmware, or hardware interaction can reset or re-enumerate the device.
- A PNP identity can change after changes to USB mode, firmware descriptors, interface selection, driver, hub, or physical topology; review the expected identity when the setup changes.
- Persistent Windows-native processes reduce dependence on WSL interop but cannot prevent Windows, USB, power, or process failures.
- Serial logs alone cannot establish electrical accuracy, classroom safety, browser correctness, build reproducibility, or release readiness.
