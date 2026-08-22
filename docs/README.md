# Documentation

VAMeter-Edu documentation is organized by role. Current implementation behavior still comes from merged source and tests; documents add product context, contracts, operating guidance, and evidence without replacing those authorities.

## Document roles

- [`../README.md`](../README.md) and [`../README_ja.md`](../README_ja.md) are the public project and release entry points.
- [`product/`](product/) describes current, implemented product behavior: the device-hosted Viewer and the educational recorder/local-download path.
- [`standards/`](standards/) defines current, durable normative rules — currently measurement-and-presentation semantics.
- [`architecture/`](architecture/) contains current direct-browser architecture, ownership/lifecycle, and resource contracts.
- [`operations/`](operations/) contains current operational guidance, such as reading runtime diagnostics.
- [`validation/`](validation/) contains the current physical-qualification procedure.
- [`releases/`](releases/) contains per-release records that expand on [`../CHANGELOG.md`](../CHANGELOG.md).
- [`ai/`](ai/README.md) contains durable guidance for AI tools and human reviewers.
- [`archive/`](archive/README.md) preserves dated handoffs, validation evidence, contest records, and historical/future design drafts. Archive documents are historical evidence, not current normative contracts.

Current documentation must describe behavior implemented in current source and tests. Future product work belongs in GitHub Issues. The role-based current-documentation structure is tracked in [VAMeter-Edu Issue #7](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/7).
