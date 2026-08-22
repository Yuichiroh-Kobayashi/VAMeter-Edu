# Historical documentation archive

Documents in this directory preserve operational history, decisions, measured results, artifact provenance, validation evidence, and superseded plans as observed at a specific time.

Archive documents do not override current source, tests, contracts, standards, architecture, or open GitHub Issues. Use merged source and tests for current implementation behavior, current contracts and standards for normative rules, and focused evidence for physical PASS/FAIL claims. Historical PASS/FAIL values are retained as recorded.

Dates are intentionally retained in archive filenames so that evidence remains tied to its original context. A newer date alone does not make an archive document a current contract.

## Old to new path mapping

| Previous path | Archive path |
|---|---|
| `docs/handoffs/VAMeter-Edu_PR1_post-merge_handoff_2026-07-29_rev9.md` | [`docs/archive/handoffs/2026-07-29-pr1-post-merge-handoff-rev9.md`](handoffs/2026-07-29-pr1-post-merge-handoff-rev9.md) |
| `docs/handoffs/VAMeter-Edu_contest_plan_n_final_handoff_2026-08-08.md` | [`docs/archive/contest-2026/2026-08-08-plan-n-final-handoff.md`](contest-2026/2026-08-08-plan-n-final-handoff.md) |
| `docs/vi-logger/validation/d2b_ap_mode_reconciliation_offline_validation_2026-08-03.md` | [`docs/archive/validation/d2b/2026-08-03-ap-mode-reconciliation-offline.md`](validation/d2b/2026-08-03-ap-mode-reconciliation-offline.md) |
| `docs/vi-logger/validation/d2b_ap_result_propagation_offline_validation_2026-08-03.md` | [`docs/archive/validation/d2b/2026-08-03-ap-result-propagation-offline.md`](validation/d2b/2026-08-03-ap-result-propagation-offline.md) |
| `docs/vi-logger/validation/d2b_httpd_stop_serialization_offline_validation_2026-08-02.md` | [`docs/archive/validation/d2b/2026-08-02-httpd-stop-serialization-offline.md`](validation/d2b/2026-08-02-httpd-stop-serialization-offline.md) |
| `docs/vi-logger/validation/d2b_stop_failure_recovery_offline_validation_2026-08-02.md` | [`docs/archive/validation/d2b/2026-08-02-stop-failure-recovery-offline.md`](validation/d2b/2026-08-02-stop-failure-recovery-offline.md) |
| `docs/vi-logger/validation/pr1_recorder_validation_2026-07-29.md` | [`docs/archive/validation/recording/2026-07-29-pr1-recorder-validation.md`](validation/recording/2026-07-29-pr1-recorder-validation.md) |
| `docs/vi-logger/operations/d2b_vi_live_validation_plan.md` | [`docs/archive/plans/d2b-vi-live-validation-pre-beta1.md`](plans/d2b-vi-live-validation-pre-beta1.md) |
| `docs/vi-logger/architecture/vi_logger_product_definition.md` | [`docs/archive/design/vi-logger-product-definition.md`](design/vi-logger-product-definition.md) |
| `docs/vi-logger/architecture/internal_excitation_measurement_design.md` | [`docs/archive/design/internal-excitation-measurement-design.md`](design/internal-excitation-measurement-design.md) |
| `docs/vi-logger/operations/battery_and_protection_test_plan.md` | [`docs/archive/design/battery-and-protection-test-plan.md`](design/battery-and-protection-test-plan.md) |
| `docs/vi-logger/standards/measurement_semantics_policy.md` | [`docs/archive/design/measurement-semantics-policy-future-draft.md`](design/measurement-semantics-policy-future-draft.md) |

`docs/vi-logger/architecture/local_csv_download.md` and `docs/vi-logger/operations/d2b_runtime_evidence.md` described current, implemented behavior rather than a historical or future draft; their content was migrated (not archived) into [`docs/product/educational-recording-and-local-download.md`](../product/educational-recording-and-local-download.md) and [`docs/operations/d2b-runtime-diagnostics.md`](../operations/d2b-runtime-diagnostics.md) respectively.

## Historical design drafts

`docs/archive/design/` preserves design drafts for an unimplemented internal-excitation ("next-generation") V-I logger concept. None of it is implemented and none of it is current product authority. It is not an abandoned idea: science teachers have requested an internally-powered classroom V-I experiment device, and this remains a real, still-requested future requirement — but it is paused because it requires a large hardware redesign (internal power source, hardware-level protection circuitry, battery selection) rather than a firmware feature. It is tracked in [VAMeter-Edu Issue #11](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/11). Each document carries a `HISTORICAL DESIGN DRAFT — NOT IMPLEMENTED — NOT CURRENT PRODUCT AUTHORITY — PAUSED, TRACKED FUTURE REQUIREMENT` banner; treat their content as historical design exploration and reference material for Issue #11, not as a committed specification, and do not begin implementation without an explicit task request.

## Using handoffs and validation evidence

A handoff is not a standalone normative contract. It may be cited for operational history, decisions, measured results, artifact provenance, or the state observed at that time. For current behavior use merged source and tests; for current V-I logger rules use the applicable standards and architecture documents; for open or future work use the relevant GitHub Issue. Physical validation claims may cite a handoff or focused validation report but must not be inferred from design intent.

The final PR #1 handoff is [Revision 9](handoffs/2026-07-29-pr1-post-merge-handoff-rev9.md), with a focused [recorder validation record](validation/recording/2026-07-29-pr1-recorder-validation.md).
