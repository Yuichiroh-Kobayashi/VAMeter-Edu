# V-I logger documentation

`docs/vi-logger/` is the canonical home for VAMeter-Edu V-I logger documentation.

## Document roles

- `architecture/` explains intended architecture, accepted behavior, and design rationale.
- `standards/` defines measurement semantics and other normative policies.
- `operations/` contains hardware and operational validation plans.
- `validation/` records focused test and physical-device evidence.

Implementation contracts, physical evidence, and future designs must remain distinguishable. Current implementation behavior comes from merged code and tests; measurement semantics use the standards document together with HAL implementation; physical PASS/FAIL claims come from validation evidence; future work remains in architecture documents and open Issues.

The old files under `docs/architecture/`, `docs/standards/`, and `docs/operations/` are compatibility stubs. Do not add new content to those stubs.

AI-tool-wide durable rules live under [`../ai/`](../ai/README.md). Dated handoffs under [`../handoffs/`](../handoffs/README.md) preserve history and evidence, but a handoff alone does not override current code, tests, canonical standards/architecture, or open Issues.

## Architecture

- [Local CSV download and classroom recorder behavior](architecture/local_csv_download.md)
- [V-I logger product definition](architecture/vi_logger_product_definition.md)
- [Internal-excitation measurement design](architecture/internal_excitation_measurement_design.md)

## Standards

- [Measurement semantics policy](standards/measurement_semantics_policy.md)

## Operations

- [Battery and protection test plan](operations/battery_and_protection_test_plan.md)
- [D2B V/I live-stream validation plan](operations/d2b_vi_live_validation_plan.md)

## Validation

- [PR #1 recorder validation, 2026-07-29](validation/pr1_recorder_validation_2026-07-29.md)
