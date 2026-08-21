# V-I logger documentation

> **Status: LEGACY INDEX / MIGRATION IN PROGRESS**

`docs/vi-logger/` remains a temporary index for VAMeter-Edu V-I logger documentation. Historical validations and the superseded pre-beta D2B live-validation plan have moved to the [historical archive](../archive/README.md). Current architecture, standards, and operations documents remain here until PR-DOC2 completes the role-based canonical migration.

## Document roles

- `architecture/` explains intended architecture, accepted behavior, and design rationale.
- `standards/` defines measurement semantics and other normative policies.
- `operations/` contains hardware and operational validation plans.
- `validation/` records focused test and physical-device evidence.

Implementation contracts, physical evidence, and future designs must remain distinguishable. Current implementation behavior comes from merged code and tests; measurement semantics use the standards document together with HAL implementation; physical PASS/FAIL claims come from validation evidence; future work remains in architecture documents and open Issues.

The old files under `docs/architecture/`, `docs/standards/`, and `docs/operations/` are compatibility stubs. Do not add new content to those stubs.

AI-tool-wide durable rules live under [`../ai/`](../ai/README.md). Dated handoffs and validation evidence under [`../archive/`](../archive/README.md) preserve history, but archive material does not override current code, tests, standards/architecture, or open Issues.

## Architecture

- [Local CSV download and classroom recorder behavior](architecture/local_csv_download.md)
- [V-I logger product definition](architecture/vi_logger_product_definition.md)
- [Internal-excitation measurement design](architecture/internal_excitation_measurement_design.md)

## Standards

- [Measurement semantics policy](standards/measurement_semantics_policy.md)

## Operations

- [Battery and protection test plan](operations/battery_and_protection_test_plan.md)

## Historical validation and plans

- [PR #1 recorder validation, 2026-07-29](../archive/validation/recording/2026-07-29-pr1-recorder-validation.md)
- [Pre-beta D2B V/I live-stream validation plan](../archive/plans/d2b-vi-live-validation-pre-beta1.md)
- [Archive index and complete move mapping](../archive/README.md)
