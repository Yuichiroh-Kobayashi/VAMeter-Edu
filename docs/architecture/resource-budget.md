# Direct-browser resource budget

This document is the authoritative resource claim boundary for the frozen G2 direct-browser architecture. G2 Track B is accepted and frozen with the static IRAM HOLD retained; it is not a product or release qualification result.

## Dedicated static IRAM

The accepted measurement is:

```text
used:       16383 bytes
capacity:   16384 bytes
remaining:      1 byte
status:     HOLD
```

This is a real occupancy fact for the dedicated static IRAM classification. It is not by itself the full linker-segment limit, but that nuance does not turn the result into acceptance. Full IRAM and shared D/IRAM layout, deltas, and runtime behavior remain part of later qualification.

Do not remove `IRAM_ATTR`, move ISR code, change ISR placement, or alter linker ordering based only on the current symbol table. Any residency change requires cache-disabled and ISR call-graph correctness analysis, timing evidence, exact map/ELF comparison, physical stress, and rollback proof.

## O1-RX bounds and later measurement

- Per-session parser state has a design ceiling of 64 bytes, excluding allocator metadata.
- Maximum accepted Origin value is 191 bytes.
- No full-header or replay buffer is permitted.
- Actual O1-RX IRAM, DRAM, BSS, flash, heap, allocation-failure, task-stack, and HTTPD-stack costs must be measured later.

The 64-byte and 191-byte values are ceilings, not observed `sizeof`, heap, stack, or linked-image deltas. No zero-cost inference is permitted.

## AssetPool evidence

The measured P2 Student + Professional candidate fits the current 2 MiB AssetPool partition:

| Measurement | Bytes |
|---|---:|
| Candidate free space | 451360 |
| Measured P2-SP stored contribution | 20120 |

The 20,120-byte value belongs only to that measured P2-SP candidate. It is not the final Viewer size, an approved reserve, or proof that a future production Viewer fits.

## Qualification still required

Later qualification must measure at least:

- linked IRAM/DRAM/BSS/flash and partition deltas against the exact baseline;
- runtime free heap and largest allocatable block;
- HTTPD task and related task stack high-water under realistic concurrency;
- allocation failure and open/close/rejection cleanup;
- device-hosted Viewer and external-development profile behavior;
- duration/soak and classroom/product workload.

No numeric production threshold may be invented from the current evidence. Thresholds require a separate product/release decision. Until measured and approved, the status remains HOLD.

The security design is defined in
[`origin-admission-policy.md`](origin-admission-policy.md); evidence-layer boundaries are defined in
[`../ai/build-and-validation.md`](../ai/build-and-validation.md).
