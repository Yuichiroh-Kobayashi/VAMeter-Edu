# Suggested AGENTS.md Patch

This package does not replace the repository-root `AGENTS.md`.
Apply only the following small changes if they match the current AGENTS.md.

## 1. Keep documentation references tolerant during Phase 1

If `docs/manuals/` or `docs/standards/user_manual_policy.md` does not exist yet, agents must not invent them.
They should write `未作成` or defer user-facing manual updates.

## 2. Replace task skill path later

When `.agents/skills/` is introduced in Phase 4, replace the task skills section with the following block.

````md
## Task skills

If repeated task guides are added later, consult:

```text
.agents/skills/*/SKILL.md
```

These skills are primarily for Codex. Other AI agents may ignore them.
If no matching skill exists, do not invent one during an unrelated task.
````

Do not add `.agents/skills/` in Phase 1.
