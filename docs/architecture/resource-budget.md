# Direct-browser resource budget

This document is the authoritative resource claim boundary for the direct-browser
architecture (`O1-RX + O3`, serving the device-hosted Viewer and D2B stream included in
stable `v2.0.0`, after their introduction in `v2.0.0-beta.1`). It is not by itself a
release-qualification sign-off; see
[`../ai/build-and-validation.md`](../ai/build-and-validation.md) for the full validation
hierarchy.

## Dedicated static IRAM

The accepted measurement is:

```text
used:       16383 bytes
capacity:   16384 bytes
remaining:      1 byte
```

Only 1 byte of headroom remains in this dedicated static IRAM classification. In practice
this means: do not add code or data to this classification without first measuring its
exact IRAM cost, and treat any addition here as very likely to require moving something
else out first. This is a real occupancy fact for the dedicated static IRAM
classification; it is not by itself the full linker-segment limit, but that nuance does
not make 1-byte headroom safe to ignore. Full IRAM and shared D/IRAM layout, deltas, and
runtime behavior remain part of later qualification.

Do not remove `IRAM_ATTR`, move ISR code, change ISR placement, or alter linker ordering based only on the current symbol table. Any residency change requires cache-disabled and ISR call-graph correctness analysis, timing evidence, exact map/ELF comparison, physical stress, and rollback proof.

## O1-RX bounds and later measurement

- Per-session parser state is compile-time capped at 64 bytes, excluding allocator
  metadata (`static_assert` in `app/libs/origin_admission/origin_admission.h`).
- Maximum accepted Origin value is 191 bytes (`kMaximumOriginBytes`).
- No full-header or replay buffer is permitted.
- Actual O1-RX IRAM, DRAM, BSS, flash, heap, allocation-failure, task-stack, and HTTPD-stack costs must be measured later.

The 64-byte and 191-byte values are compile-time-enforced ceilings on this scanner's own
state, not observed `sizeof`, heap, stack, or linked-image deltas for the whole system. No
zero-cost inference is permitted.

## Recorded `v2.0.0-beta.1` resource facts

These are the released `v2.0.0-beta.1` application and AssetPool figures, retained as a
version-specific historical qualification baseline. They are not the binary identity or
an inferred size report for stable `v2.0.0`:

| Measurement | Bytes |
|---|---:|
| Application image | 1,772,352 |
| Application partition free | 324,800 |
| AssetPool image | 1,651,116 |
| AssetPool free | 446,036 |

Engineering implication for that beta.1 pair: application partition headroom (324,800
bytes) and AssetPool headroom (446,036 bytes) were the measured room available before a
partition-layout change. This is materially tighter than either
figure in isolation suggests once combined with the dedicated static IRAM headroom above
(1 byte): any feature addition that increases dedicated static IRAM usage cannot lean on
flash/AssetPool headroom to compensate, since the two are independent constraints that
must each be re-measured for that specific change. These figures supersede the earlier
pre-implementation P2 Student+Professional candidate estimate (measured before the Viewer
was integrated); they are the actual beta.1 shipped-binary sizes, not a design ceiling or
a substitute for stable-release measurement.

## Qualification still required

Beyond the `v2.0.0-beta.1` figures above, qualification for any further change should
still measure at least:

- linked IRAM/DRAM/BSS/flash and partition deltas against the exact baseline for that
  change;
- runtime free heap and largest allocatable block;
- HTTPD task and related task stack high-water under realistic concurrency;
- allocation failure and open/close/rejection cleanup;
- device-hosted Viewer and external-development profile behavior;
- duration/soak and classroom/product workload.

No numeric production threshold beyond the measured `v2.0.0-beta.1` figures above may be
invented from this evidence; a materially different feature or resource change requires
its own measurement and review before being accepted.

The security design is defined in
[`origin-admission-policy.md`](origin-admission-policy.md); evidence-layer boundaries are defined in
[`../ai/build-and-validation.md`](../ai/build-and-validation.md).
