# Native visual-parameter hint execution checkpoint

**Source:** `18a8b6924f`

**Build:** Alchemy Metal Development `26.4.0d68209`

**Detailed evidence:**
[state archive](../../state/archive/CURRENT-native-visual-param-hint-checkpoint-2026-11-03.md)

## Completed implementation slice

Darwin appearance-editor visual-parameter hints now run targetlessly through
native Metal. CPU camera setup and pipeline state sorting supply the existing
feature inputs without a retained render transaction; system-avatar meshes,
cloth, rigid/rigged attachments, alpha ordering, materials and background are
encoded by the native lit-preview renderer. Complete results publish as sole
authoritative backing, while incomplete attempts preserve the last valid image
and retry.

The former GL background draw, camera/depth/blend transaction, retained avatar
draw and framebuffer-copy publication were removed only after the replacement
was in place. This slice preserves the feature rather than treating GL removal
as the product outcome.

## Acceptance evidence

- Warnings-as-errors Xcode Release build 68209, signing and staging passed at
  exact source `18a8b6924f`.
- Bundle verification passed for all 349 ARM64 Mach-O slices; deep strict
  signing passed.
- Exact Ninja targets for appearance, Metal, render and viewer tests linked.
- Metal API/GPU/shader/load-action validation passed 52,427 Metal assertions,
  13,207 appearance assertions and 2,612 fresh-render assertions.
- `alrender_tests` passed 557,312 assertions; `newview_tests` passed 98,973 and
  self-reported build 68209.
- Harness validation and all 27 harness/tool tests passed.
- Strict audit remains intentionally failing at 3,244 findings and is exactly
  unchanged by unique classified tuple; no rule or suppression changed.
- The exact app still links OpenGL and Metal; product and parity gates remain
  open.
- No viewer was launched or visually compared. The bounded presentation case
  is in the owner-attended daily queue.

## Next owner

Continue in the active ledger with targetless authoritative
`LLImagePreviewAvatar` and `LLImagePreviewSculpted` production. Preserve their
camera, interaction, selected geometry/texture, preview lighting, alpha and
retry behavior; validate physically before deleting the Darwin GL producers.
