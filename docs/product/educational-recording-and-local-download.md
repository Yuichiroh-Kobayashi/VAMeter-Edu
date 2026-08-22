# Educational recording and local download

This describes only current, implemented behavior of the classroom waveform recorder and
local CSV download path. For value semantics shared with the D2B live stream, see
[`../standards/measurement-and-presentation-semantics.md`](../standards/measurement-and-presentation-semantics.md).
For AI-tool-facing implementation contracts (the same rules, written for editors of this
code), see [`../ai/recorder-and-measurement-contracts.md`](../ai/recorder-and-measurement-contracts.md).

## Manual 5-second fallback recording

After D2B became the primary live path, the device-side CSV waveform recorder is the
fallback. It uses manual trigger only, with a fixed 5-second recording:

- pressing the encoder starts recording while idle;
- encoder rotation changes only the horizontal time-axis zoom;
- a short side-button click reaches the dedicated Help-request hook without changing
  measurement or recorder state (the project has no HelpEvent transport sink yet, so the
  hook currently emits a diagnostic log);
- a long side-button hold exits the waveform screen;
- while `state_saving` is active, both the short click and long hold are ignored: saving
  neither enters Help nor exits Waveform.

Before starting, the recorder requires at least 64 KiB free. This is conservative
headroom for temporary chunks and the final CSV to coexist, including FAT allocation and
directory metadata; it is not the measured size of one 5-second CSV. If storage
information cannot be read or the threshold is not met, recording is rejected with a
warning and the device does not reboot.

## Completion-before-rotation

The recorder checks duration completion before the second-chunk rotation point, so the
current 5-second policy avoids the known second-chunk rotation entirely. Per-sample
synchronous FAT writes remain in the measurement path, so residual sampling gaps remain
possible; this is tracked as
[VAMeter-Edu Issue #3](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3), which
is open. Neither gap-free CSV nor uniform 25 Hz sampling is claimed for the current
recorder.

## CSV schema and blank unused columns

New records use this header, followed immediately by samples, with no summary row and no
capacity or energy columns:

```csv
voltage,current,elapsed_ms
```

The active waveform mode is passed explicitly to the recorder when its trigger is
created, producing one of:

```csv
<voltage>,,<elapsed_ms>
,<current>,<elapsed_ms>
<voltage>,<current>,<elapsed_ms>
```

respectively voltage-only, current-only, and the internal USB-C `mode_both` compatibility
row. `elapsed_ms` is captured from a monotonic timer relative to the start of the
recording loop; it records real scheduler delay and jitter and is not synthesized from the
sample index.

USB-C Power Monitor opens Waveform in internal `mode_both` compatibility mode: it records
both columns, keeps the legacy combined chart with each channel's min/mid/max-following
labels, and does not use single-channel staged Auto scale, manual vertical-scale control,
or a TIME/V/I `/div` readout.

Preview remains backward-compatible with the legacy `voltage,current,time,capacity,energy`
and `voltage,current,elapsed_ms,capacity,energy` headers, legacy two-column samples, the
current three-column header, and the current voltage-only/current-only/both rows.

## REC / MO separation

Current V-I records must match `REC-[0-9]+.csv` using ASCII digits. Numbering starts at
`REC-000.csv` and uses one more than the maximum valid REC id; it does not reuse gaps.
Legacy `MO-[0-9]+.csv` files (from the unrelated, discontinued Motor Observe feature)
remain visible only as individual deletion targets: they do not affect REC numbering,
latest-record selection, preview, or QR/local download.

## Storage preflight and recoverable errors

The recorder checks FAT storage usage before recording and requires the 64 KiB margin
described above. On failure or insufficient storage, the device returns to the waveform
screen without rebooting; it does not automatically delete recordings and does not format
storage when full. Incomplete final output and temporary chunks are removed on failure.

## Delete confirmation

Record files are reachable from `Settings → Files → Record files`. The complete deletion
path is `Settings → Files → Record files → <record> → Delete`. Deletion requires
confirmation; the firmware neither deletes old recordings automatically nor formats
storage when it becomes full, and reports failures without rebooting.

## Local download

The local download path supports record files matching `^REC-[0-9]+\.csv$` (ASCII digits
only), reachable through the existing waveform record selection, QR screen, local Wi-Fi
AP, AP suffix, and `/download/<record-name>` HTTP route. It needs neither an internet
connection nor a cloud account.

## Basename/path validation and bounded streaming

The QR entry point and HTTP entry point share one basename validator. The HTTP entry point
takes the encoded basename after the literal `/download/` route, percent-decodes it
exactly once, and validates the decoded bytes — rejecting empty names, names longer than
64 bytes, control bytes (including NUL and DEL), `/`, `\`, `..`, non-ASCII digits,
unexpected extensions, and unexpected prefixes. Only a validated basename is joined to the
fixed record directory; callers cannot select an arbitrary directory.

The selected record name and path are stored as one mutex-protected value, published
before the download endpoint becomes reachable and cleared before the AP stops. The HTTP
handler streams the file with a fixed 8 KiB heap buffer instead of loading it into RAM. A
zero-byte file is a valid response with no data chunk; a short `fread` is treated as EOF
only when `ferror` is clear; read, send, close, and final-chunk failures are not reported
as successful completion.

In MSC mode VAMeter appears as USB storage; firmware-side filesystem writes must not run
concurrently with MSC access.

## Educational waveform staged Auto scale

Voltage-only and current-only waveform views have no manual vertical-scale selection.
Every new view starts at the smallest scale. Auto scale examines the positive peak across
the complete visible ring buffer: it scales up while `positive_peak >= 8 × current_scale`
and scales down by one step only when `positive_peak < 4 × lower_scale` (equality does not
scale down), so a transient peak holds the larger scale until it leaves the visible
buffer. Voltage and current keep independent view-local state.

Voltage scales: 0.1, 0.2, 0.5, 1.0, 2, and 5 V/div. Current scales: 0.1, 0.2, 0.5, 1, 2, 5,
10, 20, 50, and 100 mA/div, and 0.2, 0.5, and 1.0 A/div; microamp labels use ASCII `uA`.
The old panel-edge visible-buffer max/min labels, TIME indicator, manual-selection badge,
and manual-trigger `M` icon are not rendered in single-channel views; the centered live
value remains. Panel-edge max/min-label removal is a single-channel rule and does not
apply to the internal `mode_both` compatibility view.

Plot clipping does not modify the HAL values received by the UI or the values passed to
CSV — see
[`../standards/measurement-and-presentation-semantics.md`](../standards/measurement-and-presentation-semantics.md)
for the full raw/processed/display value-authority contract.

## Historical evidence

Physical-hardware validation evidence for this feature, including the earlier fixed
10-second recording window that predates the current 5-second fallback policy, is
preserved as dated historical evidence rather than restated here:

- [PR #1 recorder validation, 2026-07-29](../archive/validation/recording/2026-07-29-pr1-recorder-validation.md)

That evidence reflects the state of the recorder at the time it was recorded and does not
override the current 5-second contract described above.

## Related Issues

- Recorder sampling (decoupling sampling from synchronous FAT writes):
  [VAMeter-Edu Issue #3](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3).
