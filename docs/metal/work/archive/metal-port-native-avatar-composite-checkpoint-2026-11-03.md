# Native avatar-composite execution checkpoint

**Source:** `e4d5c187a9`

**Build:** Alchemy Metal Development `26.4.0d68207`

**Detailed evidence:**
[state archive](../../state/archive/CURRENT-native-avatar-composite-checkpoint-2026-11-03.md)

## Completed implementation slice

Darwin avatar layer composites now run targetlessly through native Metal UI
capture. Existing feature decisions provide geometry, textures, sampling,
blending, color and alpha rejection; GL side effects are suppressed within the
capture. Static/processed alpha textures are native, morph masks read the live
Metal capture and resume encoding, and a complete result becomes sole
authoritative backing. Failed work retains the last valid texture and retries.

This slice replaced a live feature owner before retiring its GL authority. It
did not remove avatar appearance behavior for inventory-count progress.

## Acceptance evidence

- Warnings-as-errors Xcode Release build, signing and staging passed.
- Bundle verification passed for all 349 ARM64 Mach-O slices; deep strict
  signing passed.
- Exact Ninja targets for appearance, Metal, render and viewer tests linked.
- Metal API/GPU/shader/load-action validation passed 52,414 Metal assertions,
  13,207 appearance assertions and 2,612 fresh-render assertions.
- `alrender_tests` passed 557,312 assertions; `newview_tests` passed 98,973.
- Harness validation and all 27 harness/tool tests passed.
- Strict audit remains intentionally failing at 3,244 findings; its net +11 is
  capture boundary vocabulary, with no new GL drawing or changed suppression.
- The exact app still links OpenGL and Metal; product and parity gates remain
  open.
- No viewer was launched or visually compared. The bounded presentation case
  is in the owner-attended daily queue.

## Next owner

Continue in the active ledger with targetless native
`LLVisualParamHint`/`LLVisualParamReset`. Preserve system avatar, clothing,
rigid/rigged attachments, alpha ordering, legacy/PBR materials, camera and
retry semantics; add physical evidence before deleting the Darwin GL producer.
