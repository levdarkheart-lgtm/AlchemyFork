# Native image-upload preview execution checkpoint

**Source:** `7f3bbc58a0`

**Build:** Alchemy Metal Development `26.4.0d68211`

**Detailed evidence:**
[state archive](../../state/archive/CURRENT-native-image-preview-checkpoint-2026-11-03.md)

## Completed implementation slice

Darwin avatar and sculpt image-upload previews now run targetlessly through
native Metal. CPU camera matrices preserve framing and interaction. Avatar
geometry, real preview textures, joint skinning, lighting, tint and alpha cut,
plus sculpt geometry, scale and solid lighting, are encoded by the native
lit-preview renderer. Complete results publish as sole authoritative backing;
incomplete attempts preserve the last valid image and retry.

The former GL background, avatar/sculpt draw, camera/matrix transaction and
framebuffer-copy publication were removed only after the replacement was in
place. This slice preserves the feature rather than treating GL removal as the
product outcome.

## Acceptance evidence

- Warnings-as-errors Xcode Release build 68211, signing and staging passed at
  exact source `7f3bbc58a0`.
- Bundle verification passed for all 349 ARM64 Mach-O slices; deep strict
  signing passed.
- Exact Ninja targets for appearance, Metal, render and viewer tests linked.
- Metal API/GPU/shader/load-action validation passed 52,447 Metal assertions,
  13,207 appearance assertions and 2,612 fresh-render assertions.
- `alrender_tests` passed 557,312 assertions; `newview_tests` passed 98,973 and
  self-reported build 68211.
- Harness validation and all 27 harness/tool tests passed.
- Strict audit remains intentionally failing at 3,244 findings; its raw count
  and 2,933 unique-record count are unchanged, one wrapper snippet changed with
  the refactor, and no rule or suppression changed.
- The exact app still links OpenGL and Metal; product and parity gates remain
  open.
- No viewer was launched or visually compared. The bounded presentation case
  is in the owner-attended daily queue.

## Next owner

Continue in the active ledger with targetless authoritative
`LLPreviewAnimation` production. Preserve playback, camera interaction,
system-avatar rigid/skinned meshes, cloth/alpha behavior, default texture,
publication and retry semantics; validate physically before deleting its
Darwin retained producer.
