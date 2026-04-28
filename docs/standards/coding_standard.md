# Coding Standard

## General

- Keep changes small and reviewable.
- Do not mix unrelated refactors with feature work.
- Prefer explicit names over clever names.
- Prefer central configuration / HAL definitions for hardware-specific constants.

## Domains to keep separated

1. UI / display
2. measurement acquisition
3. filtering / smoothing
4. calibration / correction
5. CSV recording / local download
6. settings persistence
7. hardware control
8. safety state management

## Build target

Use ESP-IDF `v5.1.3` unless repository documentation is updated.

## Required report after agent work

- files changed
- build command and result
- tests or manual verification performed
- unverified items
- rollback notes if safety or hardware behavior changed

## Documentation sync

If a code change affects user-visible behavior, measurement meaning, CSV format, settings, safety behavior, or troubleshooting behavior, update the relevant documentation in the same change or explicitly state why documentation was not changed.
