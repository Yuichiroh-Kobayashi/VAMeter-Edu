# Comments Policy

## Purpose

Comments should explain why a design choice exists, not repeat what the code already says.

## Required comments

Add comments for:

- safety-critical behavior
- hardware-specific assumptions
- calibration constants
- non-obvious UI state transitions
- compatibility constraints inherited from original VAMeter firmware

## Prohibited comments

- Do not write optimistic claims such as "safe" without verification evidence.
- Do not write classroom-specific private information.
- Do not leave AI-generated speculation as a code comment.

## Verification claims

If a comment says that a behavior is verified, safe, calibrated, or tested, the supporting evidence should exist in an operation log, calibration log, test log, or commit history. Otherwise, write `未確認` or `未検証` instead.
