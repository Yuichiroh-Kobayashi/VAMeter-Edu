@AGENTS.md

## Claude Code adapter

- Treat `AGENTS.md` as the primary repository guidance.
- Keep this file as a thin Claude-specific adapter; do not duplicate the full project manual here.
- Before broad or multi-file changes, produce a short plan that identifies affected contracts, validation, and hardware boundaries.
- Read the relevant `docs/ai/` file before editing recorder, measurement, storage, dependency, worktree, or artifact behavior.
- Prefer small, reviewable patches and explicit validation notes.
- Do not run destructive Git operations, dependency cleanup, package installation, firmware/AssetPool flashing, device storage operations, or remote mutations unless the user explicitly authorizes the exact operation.
- When using multiple worktrees, use `git -C <exact-worktree-path>` for branch-changing commands and verify the branch before and after execution.
- Treat physical VAMeter validation as authoritative for waveform rendering, measurement behavior, storage, MSC, Wi-Fi AP download, and sampling timing.
- Unless the user specifies otherwise, give questions, progress updates, and final reports in Japanese.
