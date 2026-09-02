# AI guidance index

This directory contains durable project context for AI coding tools and human reviewers.

## Precedence

1. Explicit user request
2. Path-specific `AGENTS.md`, when present
3. Repository root `AGENTS.md`
4. The relevant detailed document in this directory
5. Tool-specific adapter (`CLAUDE.md` or `.github/copilot-instructions.md`)

The adapters are intentionally short. Common rules belong in `AGENTS.md`; detailed stable knowledge belongs here.

## Documents

- `project-context.md`
  - educational purpose;
  - branch and scope boundaries;
  - accepted PR #1 behavior;
  - Issue #3 (open) and Issue #2 (closed, not planned);
  - paused internal-excitation V-I logger future direction (Issue #11);
  - device-hosted direct-browser product direction, now included in stable `v2.0.0`.
- `recorder-and-measurement-contracts.md`
  - UI, CSV, timing, storage, lifecycle, Files/download invariants;
  - known sampling limitation.
- `build-and-validation.md`
  - dependency setup;
  - host/desktop/device build commands;
  - direct-browser validation hierarchy;
  - AssetPool and hardware-validation boundary.
- `git-worktree-and-artifacts.md`
  - standard worktree model;
  - safe Git operation rules;
  - LabData, Artifacts, and Archive separation.
- `dependency-baseline.md`
  - the seven dependency revisions verified for the PR #1 merge-commit clean build;
  - mutable-ref review rules and fetch-helper limitations.
- [`physical-validation-and-rollback.md`](physical-validation-and-rollback.md)
  - authoritative physical candidate, device identity, flash, capture, rollback, and STOP rules.
- [`../product/device-hosted-viewer-contract.md`](../product/device-hosted-viewer-contract.md)
  - current device-hosted Viewer product contract (Student/Professional, display profiles, fail-closed behavior).
- [`../product/educational-recording-and-local-download.md`](../product/educational-recording-and-local-download.md)
  - current classroom recorder, CSV, and local-download contract.
- [`../standards/measurement-and-presentation-semantics.md`](../standards/measurement-and-presentation-semantics.md)
  - authoritative raw/processed/display/CSV/D2B value semantics.
- [`../architecture/origin-admission-policy.md`](../architecture/origin-admission-policy.md)
  - authoritative direct-browser Origin threat model and O1-RX admission policy.
- [`../architecture/direct-browser-service-profiles.md`](../architecture/direct-browser-service-profiles.md)
  - authoritative separation of SystemConfig, SystemLive, and Download route/security profiles.
- [`../architecture/systemlive-lifecycle-and-ownership.md`](../architecture/systemlive-lifecycle-and-ownership.md)
  - HTTPD server-generation and D2B stream-ownership state machines.
- [`../architecture/viewer-assetpool-integration.md`](../architecture/viewer-assetpool-integration.md)
  - byte-exact Viewer bundle / AssetPool integration contract.
- [`../architecture/resource-budget.md`](../architecture/resource-budget.md)
  - authoritative direct-browser static, AssetPool, and runtime resource facts.
- [`../operations/d2b-runtime-diagnostics.md`](../operations/d2b-runtime-diagnostics.md)
  - current `D2B_DIAG` log field reference.
- [`../validation/browser-physical-qualification.md`](../validation/browser-physical-qualification.md)
  - current browser/Viewer physical-qualification procedure.

## Related document sets

- [`../README.md`](../README.md) explains current document roles.
- [`../archive/validation/recording/2026-07-29-pr1-recorder-validation.md`](../archive/validation/recording/2026-07-29-pr1-recorder-validation.md) is the focused PR #1 physical-validation record.
- [`../archive/README.md`](../archive/README.md) explains how dated handoffs, validations, historical design drafts, and superseded plans are preserved as history and evidence.

## Update policy

Update these documents when a merged change alters a durable contract, build baseline, validation command, branch role, hardware boundary, or known limitation.

Do not add temporary machine paths, one-off recovery steps, chat transcripts, or stale commit-specific cleanup instructions. Those belong in an operation log or handoff/archive document, not in repository-wide AI guidance.
