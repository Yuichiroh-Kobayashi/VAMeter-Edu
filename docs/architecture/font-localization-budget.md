# Font / localization budget (Local Fault UI)

This document is the current font/localization contract for the F1-L Local
Fault UI. It exists because the Fault screen's JP/CN body text was
originally routed through an English-only font and, separately, because
the embedded CJK font subsets are a project-curated glyph set, not a full
Unicode font -- new localized text is not automatically renderable just
because `localization.csv` accepts UTF-8.

## Source commit this budget was derived against
`20515fcee3d38c84a511bc7924af3508da417f06`

## Font source file identity
```
efontCN_16_subset.cpp: 6cf90d7a295914ffb0457c04d19e9fd61ca25382ded113a72322f8c586926ea3
efontCN_24_subset.cpp: 820506eebb5227b3cef8444ab130d8616a3599596e3ee202775c4018e15f99c5
efontJA_16_subset.cpp: 4e80a3af4705fde2ab33c685b043dc01457601f5bcfbfe630914bc903e87bc43
efontJA_24_subset.cpp: 3b5b8a4c6d120242079a0534ffbcb1733b76a56cea84a0f237512388d88983ab
```
Font subset inventory is tied to these exact source-file SHA-256 values.
If any of these four files ever change, every claim in this document must
be re-derived, not assumed to still hold.

## Glyph counts
```
CN16 distinct glyph count: 163
CN24 distinct glyph count: 163
JA16 distinct glyph count: 229
JA24 distinct glyph count: 229
CN16 codepoint set == CN24 codepoint set: YES
JA16 codepoint set == JA24 codepoint set: YES
```
16px/24px CJK subsets are **not arbitrary Unicode fonts**. They are a
project-curated subset generated once via
https://forairaaaaa.github.io/lgfxFontSubsetGenerator/ and never re-run
since. The full per-codepoint inventory (locale, size, codepoint,
character, Unicode name, glyph advance, glyph width) is recorded in
`docs/architecture/font-glyph-inventory.tsv`, extracted directly from the
tracked font source via a faithful reimplementation of LovyanGFX's own
`U8g2font::getGlyph` binary-format decoder -- not the web generator, and
not guesswork.

## Font routing
```
EN body:        LoadFont14 (Montserrat-SemiBoldItalic-14.vlw)
JP/CN body:      LoadFont16 (locale-aware: efontJA_16_subset / efontCN_16_subset)
All locale title: LoadFont24 (locale-aware: Montserrat-SemiBoldItalic-24.vlw /
                   efontJA_24_subset / efontCN_24_subset)
```
`LoadFont14` is **English-only by current design** -- Montserrat has no
CJK glyphs, and this is a documented, intentional asymmetry (see
`AssetPool::loadFont14()`'s own `// Only en for 14` comment), not a bug to
fix by giving `LoadFont14` a CJK branch. Any locale-sensitive text must
route through a font *size* that has a CJK-capable family (16 or 24), not
through 14. The Local Fault renderer (`_render_fault_screen()` in
`app/apps/app_waveform/view/recorder.cpp`) does exactly this:
`AssetPool::IsLocaleEn() ? LoadFont14(canvas) : LoadFont16(canvas)` for its
body text, while its title continues to use the already locale-aware
`LoadFont24`.

## Layout geometry (240x240 canvas)
```
title: x=120 y=10 font=24 datum=top_center
body: x=120 first_center_y=92 line_height=22 datum=middle_center
ack: x=120 center_y=222 (drawn only when acknowledgementAllowed==true)
line splitter segment limit: 47 UTF-8 bytes per display segment
                              (kFaultLineBufferSize=48, minus NUL)
safe text width: 216 px (12px margin each side of the 240px canvas)
body line budget: <=5 lines before the Ack region (worst observed case:
                  Normal presentation + AppWaveform_Error_NoSpace cause,
                  5 lines at y={92,114,136,158,180}, 42px clear of Ack)
```

## Current Fault localization table
| Key | en | jp | cn |
|---|---|---|---|
| Title | Safety Stop | STOP | STOP |
| StoppedForSafety | Measurement stopped | 記録を終了しました | 录制已退出 |
| OutputOff | Output: OFF | 出力: OFF | 输出: OFF |
| OutputUnconfirmed | Cannot confirm output OFF | 出力: ? | 输出: ? |
| PowerOff | Power off the device | OFFにしてください | 请关闭 |
| CleanupPending | Shutdown not complete | 終了できません | 没有完成 |
| PowerCycle | Power-cycle the device | 再起動してください | 马上重启 |
| Ack | Encoder: Confirm | Encoder: OK | Encoder: OK |

All 24 (8 keys x 3 locales) values pass: UTF-8 byte length + NUL matches
the declared C array size, every non-Latin codepoint is present in its
target font subset, every string's measured advance width (not `len()`)
is within the 216px safe-width budget, and the 47-byte segment limit.
`en/StoppedForSafety` was shortened from its original wording ("Measurement
stopped for safety", 242px, over budget) to "Measurement stopped" (169px)
-- a pre-authorized compact alternative, not an ad hoc rewrite.

## Why the font subset was NOT regenerated
The blocking defect found in the prior gate (`G4_LOCAL_FAULT_UI_F2L_
GLYPH_COVERAGE_UNRESOLVED`) had two independent causes: (1) a font-routing
bug (fixed above) and (2) the new Fault vocabulary using characters absent
from the existing 163/229-codepoint subsets. Rather than regenerating the
font subset (a separate, higher-risk change requiring the external web
generator, out of scope for this repair), the localized wording itself was
shortened/rephrased to reuse only glyphs already present in the existing,
unchanged, already-accepted subset. `efontCN_16/24_subset.cpp` and
`efontJA_16/24_subset.cpp` are byte-for-byte unchanged from the accepted
candidate.

## Maintenance procedure for future localized UI text
Before any new/changed localized string is accepted for a screen that uses
one of these embedded fonts, all four of the following must PASS:
1. **Glyph coverage** -- every codepoint present in the target font
   subset (verify via the decoder methodology in
   `font-glyph-inventory.tsv`, not by assumption).
2. **UTF-8 byte budget** -- fits the screen's display-segment limit.
3. **Actual measured pixel width** -- computed from real glyph advance
   metrics, not character count.
4. **Vertical layout** -- line count/position budget for that specific
   screen.

Changing the font subset itself (adding new glyphs) requires a separate,
explicitly authorized generator/provenance gate, not a text change.

## Physical rendering status
**Not verified.** This budget and its underlying glyph/width measurements
are derived entirely from source and dependency-code tracing (a faithful,
validated reimplementation of the actual LovyanGFX font decoders), not
from running hardware. Physical device validation remains a separate,
later gate.
