# Physical validation and rollback

This document is the authoritative durable policy for VAMeter physical flash, readback, rollback, and device-test work. A dated evidence package records one execution; it does not replace these gates or authorize a later mutation.

## Separate authorities and roots

Keep these identities distinct:

- source worktree and exact revision;
- build root and build metadata;
- frozen physical-candidate package;
- physical LabData/evidence root;
- rollback or recovery image authority.

Mutable build output is not the flash authority. Before any flash, copy the explicitly selected bytes into a new collision-free physical-candidate package and freeze file size, SHA-256, embedded version where applicable, source/build identity, and flash layout. Do not revise that package in place.

## Pre-mutation gate

Before flash, readback, erase, reset, or rollback:

1. verify the exact source, build, candidate, and evidence roots;
2. verify every candidate file against its frozen size and SHA-256;
3. identify the exact physical device by a stable hardware/USB identity, not only a volatile serial-port name;
4. confirm the target, external circuit, Relay, power, and USB state are safe for the planned operation;
5. derive offsets, image sizes, partition boundaries, and flash parameters from the candidate's build metadata;
6. define the exact authorized write/read ranges and permitted execution count;
7. prepare rollback coverage and verify its authority before candidate mutation;
8. start serial capture before the reset or flash transition that begins the observation window.

Do not use a one-off machine path as permanent authority. Record resolved paths in the evidence for that run.

## Flash layout and rollback coverage

Rollback coverage is based on the union of flash erase sectors actually touched by the authorized writes, not only nominal file lengths. Round each write range to the device erase-sector boundaries, merge overlaps, and prove that the rollback plan covers the resulting union.

Two different claims must remain separate:

- **Exact-prestate rollback:** restores the captured bytes for the complete touched-sector union and can be verified against that capture.
- **Known-good recovery:** writes a separately authorized known-good image/layout sufficient to recover operation; it does not prove restoration of the exact prior bytes.

Readback can supplement missing rollback coverage only when the exact read ranges, device identity, storage destination, and subsequent use are explicitly authorized. An incomplete readback is not authority. Never use erase-all unless the user explicitly authorizes that exact destructive scope.

## Serial capture

Serial capture must already be active before reset and must preserve raw output, timestamps or chronology, command/result metadata, and capture-tool identity.

After the observed WSL USB CDC readback/capture failure mode, prefer Windows-native serial capture when the device is attached to Windows. Whichever host owns capture must implement:

- a liveness watchdog for the capture process and device connection;
- a byte-progress watchdog so a running process with no new evidence cannot be mistaken for a healthy capture;
- bounded retries with the failed attempt preserved;
- explicit revalidation of device identity after re-enumeration.

TCP reachability, a process remaining alive, or a serial port existing is not proof that useful evidence is being captured.

## Physical STOP conditions

Stop before further mutation when any of the following occurs:

- candidate hash, size, embedded identity, layout, source, or build identity differs;
- the exact device identity is absent, ambiguous, or changes;
- hardware-safe state is not confirmed;
- rollback coverage or recovery authority is incomplete or stale;
- the requested evidence root already exists;
- serial capture is not active, loses liveness, or stops making byte progress;
- the observed erase/write/read range differs from the authorized range;
- an unexpected reset, boot state, transport failure, or physical result makes provenance ambiguous;
- source, dependency, build input, or candidate bytes change during the physical run.

Do not automatically repair source, rebuild, regenerate assets, retry a broader flash, or change configuration after a physical failure. Preserve the evidence, report the STOP condition, and obtain a separately authorized next step.

## Evidence revisions

Failed, incomplete, and revised evidence packages are immutable. A retry receives a new root and links back to the earlier attempt without overwriting it. Reports must state exactly which hardware, bytes, layout, capture, and rollback claims were observed, and which build, runtime, browser, classroom, or release claims remain untested.

For root separation and worktree rules, also read
[`git-worktree-and-artifacts.md`](git-worktree-and-artifacts.md). For evidence-layer boundaries, read
[`build-and-validation.md`](build-and-validation.md).
