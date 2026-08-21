# V-I logger documentation

> **Status: LEGACY MIXED-GENERATION INDEX**

`docs/vi-logger/` mixes documents from different implementation generations and is not a single current product authority. Historical validations and the superseded pre-beta D2B live-validation plan have moved to the [historical archive](../archive/README.md). The role-based current-documentation structure and remaining migration are tracked in [VAMeter-Edu Issue #7](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/7).

## Document roles

- `architecture/` contains mixed-generation architecture and design rationale pending role-based classification.
- `standards/` defines measurement semantics and other normative policies.
- `operations/` contains hardware and operational validation plans.

Implementation contracts, physical evidence, and future work must remain distinguishable. Current implementation behavior comes from merged code and tests; measurement semantics use the applicable standards document together with HAL implementation; physical PASS/FAIL claims come from focused evidence; future product work belongs in open GitHub Issues.

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
