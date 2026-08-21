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
  - separate Issues #2 and #3;
  - PostContest direct-browser product direction and evidence boundaries.
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
- [`../architecture/origin-admission-policy.md`](../architecture/origin-admission-policy.md)
  - authoritative direct-browser Origin threat model, admission policy, and frozen O1-RX architecture.
- [`../architecture/direct-browser-service-profiles.md`](../architecture/direct-browser-service-profiles.md)
  - authoritative separation of SystemConfig, SystemLive, and Download route/security profiles.
- [`../architecture/resource-budget.md`](../architecture/resource-budget.md)
  - authoritative direct-browser static, AssetPool, and runtime resource qualification boundaries.

## Related document sets

- [`../vi-logger/README.md`](../vi-logger/README.md) indexes the canonical V-I logger architecture, standards, operations, and validation documents.
- [`../vi-logger/validation/pr1_recorder_validation_2026-07-29.md`](../vi-logger/validation/pr1_recorder_validation_2026-07-29.md) is the focused PR #1 physical-validation record.
- [`../handoffs/README.md`](../handoffs/README.md) explains how dated handoffs should be used as history and evidence.

## Update policy

Update these documents when a merged change alters a durable contract, build baseline, validation command, branch role, hardware boundary, or known limitation.

Do not add temporary machine paths, one-off recovery steps, chat transcripts, or stale commit-specific cleanup instructions. Those belong in an operation log or handoff/archive document, not in repository-wide AI guidance.
