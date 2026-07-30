# Git worktree, data, and artifact policy

## Standard layout

Use host-independent paths under the user's home directory:

```text
~/Dev/VAMeter-Edu/                           # primary, main only
~/Dev/worktrees/VAMeter-Edu/dev-vi-logger/   # active integration branch
~/Dev/worktrees/VAMeter-Edu/issue-<n>-<slug>/
~/LabData/VAMeter-Edu/<date>-<purpose>/
~/Artifacts/VAMeter-Edu/<commit-or-release>/
~/Archive/VAMeter-Edu/<date>-<operation>/
```

Do not encode a machine name or user-specific absolute path in tracked project guidance.

## Primary and linked worktrees

- Keep the primary checkout on `main` and clean.
- Use a dedicated linked worktree for `dev/vi-logger`.
- Use Issue-specific linked worktrees for new work.
- The same local branch should not be checked out in two worktrees.
- If `git switch dev/vi-logger` is rejected because the branch is already used, this is expected. Change directory to the linked worktree.
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

Do not link dependency directories between worktrees.

Reasons:

- one worktree cleanup can break another;
- provenance becomes ambiguous;
- build output may use a different dependency revision than expected;
- Git status and reproducibility become misleading.

Fetch and verify dependencies separately in every active development worktree.

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
