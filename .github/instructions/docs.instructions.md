---
applyTo: "README*.md,CHANGELOG.md,docs/**/*.md,*.md"
---

# Documentation instructions

Follow `AGENTS.md` first.

## Phase 1 boundary

During Phase 1, focus on code-facing rules, architecture notes, hardware notes, and verification documents.

Do not create or expand:

- `docs/manuals/`
- `site/`
- `.github/workflows/`
- `.agents/skills/`

unless the task explicitly requests them.

## Audience

Write for Japanese classroom use unless the document is explicitly for developers.

Primary readers may include:

- the teacher/developer
- other teachers
- students using the device
- AI coding agents
- future maintainers

## Writing rules

- Write user-facing documentation in Japanese.
- Use short, concrete, operation-oriented sentences.
- Do not use motivational or moral language as a substitute for procedure, safety, or verification.
- Do not expose student names, faces, school-internal information, or private network details.
- Do not document unimplemented features as available.
- If behavior is unknown, write `未確認`.
- If hardware behavior is not verified, write `未検証`.

## Required structure for user operation

For procedures, prefer numbered steps.

Good:

```markdown
1. VAMeter-Eduの電源を入れます。
2. 画面にメインメニューが表示されることを確認します。
3. 測定対象を選びます。
```

Avoid vague wording such as:

```markdown
適当に接続します。
必要に応じて確認します。
```

## Documentation update rule

When code changes affect any of the following, update documentation in the same change:

- user operation
- display text
- menu flow
- connection guide
- supported probe mode
- measurement mode
- CSV format
- local download behavior
- safety behavior
- settings behavior
- calibration behavior
- troubleshooting behavior

Relevant documents may include:

- `README.md`
- `CHANGELOG.md`
- `docs/manuals/user_manual.md`
- `docs/manuals/quick_start.md`
- `docs/manuals/troubleshooting.md`
- `docs/operations/*_log.md`

If a listed document does not exist yet, do not create it automatically unless the task explicitly requires it.

## Version and implementation status

Separate implemented behavior from future work.

Use:

- 実装済み
- 未実装
- 未確認
- 未検証

Do not imply that future plans are already available.
