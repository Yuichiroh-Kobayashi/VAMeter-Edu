# Git worktree, data, and artifact policy

## Standard layout

Use host-independent paths under the user's home directory:

```text
~/Dev/VAMeter-Edu/                           # primary, main only
~/Dev/worktrees/VAMeter-Edu/dev-vi-logger/   # historical branch; current work does not require it
~/Dev/worktrees/VAMeter-Edu/issue-<n>-<slug>/
~/LabData/VAMeter-Edu/<date>-<purpose>/
~/Artifacts/VAMeter-Edu/<commit-or-release>/
~/Archive/VAMeter-Edu/<date>-<operation>/
```

Do not encode a machine name or user-specific absolute path in tracked project guidance.

## Primary and linked worktrees

- Keep the primary checkout on `main` and clean.
- Use Issue-specific linked worktrees for new work; `main` already carries the current V-I logger/recorder and device-hosted Viewer behavior, so new work does not need to start from `dev/vi-logger`.
- `dev/vi-logger` is a historical, un-merged branch retained for traceability; if a task explicitly targets it, use a dedicated linked worktree for it.
- The same local branch should not be checked out in two worktrees.
- If `git switch dev/vi-logger` is rejected because the branch is already used by its existing linked worktree, this is expected. Change directory to the linked worktree.
- Use `git worktree list --porcelain` to inspect branch/path mappings.

For branch-changing commands, prefer:

```bash
git -C "$EXACT_WORKTREE" <command>
```

Then verify:

```bash
git -C "$EXACT_WORKTREE" branch --show-current
git -C "$EXACT_WORKTREE" status -sb
git -C "$EXACT_WORKTREE" rev-parse HEAD
```

This prevents applying a fast-forward or merge in the wrong worktree.

## Pre-mutation gate

Before switch, merge, reset exception, worktree removal, or cleanup:

```bash
git status --porcelain=v1 -uall
git diff --check
git branch -vv
git worktree list --porcelain
git rev-list --left-right --count HEAD...@{upstream}
```

Stop if:

- dirty or untracked files are unexpected;
- local-only commits exist;
- refs diverge;
- upstream is missing where required;
- worktree mapping differs from the plan;
- a path is locked or prunable unexpectedly.

## Destructive and remote operations

Normally prohibited without exact approval:

- `reset --hard`;
- `git clean`;
- force switch/checkout/ref update;
- worktree force removal;
- manual worktree deletion;
- branch deletion;
- commit, push, PR/Issue mutation;
- broad filesystem deletion.

An exception must name:

- exact worktree;
- exact branch and expected current SHA;
- exact target SHA/path;
- required clean/checksum gates;
- permitted execution count;
- stop conditions.

## Dependencies belong to each worktree

Each build-capable worktree owns and validates its own dependency checkouts and ESP-IDF managed components. A docs-only worktree does not need dependency materialization unless its task explicitly changes scope.

Do not copy, link, symlink, bind-mount, or substitute dependency or `managed_components` paths between worktrees.

Reasons:

- one worktree cleanup can break another;
- provenance becomes ambiguous;
- build output may use a different dependency revision than expected;
- Git status and reproducibility become misleading.

Fetch and verify dependencies separately in every build-capable worktree. Never repair a partial checkout from another worktree.

## Data classes

### Source

Tracked in Git:

- application/source code;
- tests;
- CMake/build definitions;
- project documentation;
- dependency definitions such as `repos.json`;
- intended `sdkconfig` and `dependencies.lock`.

### LabData

Not tracked in Git:

- measurement CSV;
- experiment notes;
- dataset README;
- `SHA256SUMS`.

Use:

```text
~/LabData/VAMeter-Edu/<date>-<purpose>/
```

### Artifacts

Not tracked in Git:

- firmware binaries;
- bootloader/partition binaries;
- AssetPool;
- build metadata;
- checksums;
- clean-build provenance;
- legacy-unverified binaries.

Use:

```text
~/Artifacts/VAMeter-Edu/<commit-or-release>/
```

Distinguish:

- source commit;
- mutable build output;
- a frozen physical-candidate package;
- clean post-merge build;
- exact binary flashed to a device;
- provenance-limited legacy binary.

These are not automatically equivalent.

### Archive

Not tracked in the source repository:

- operation logs;
- cleanup/recovery reports;
- temporary patches retained for audit;
- pre-mutation manifests.

Use:

```text
~/Archive/VAMeter-Edu/<date>-<operation>/
```

## Generated output

Build directories are disposable, but deletion is still an explicit operation. Do not remove them merely to save space during an unrelated task.

Never add measurement CSV, AssetPool, or firmware `.bin` files to Git unless the repository policy is explicitly changed.

## Evidence package lifecycle

- Keep source, build output, LabData, Artifacts, and Archive roots separate.
- Freeze the exact physical-candidate bytes, metadata, and checksums outside the mutable build directory before flash.
- A failed, incomplete, corrected, or superseded evidence package remains immutable evidence of that attempt.
- A retry or revision gets a new non-colliding root. Do not overwrite or silently repair the earlier package.
- Record relationships among source identity, build identity, candidate package, flashed bytes, and collected evidence; do not collapse them into one provenance claim.
- Follow [`physical-validation-and-rollback.md`](physical-validation-and-rollback.md) for physical-candidate and rollback gates.
