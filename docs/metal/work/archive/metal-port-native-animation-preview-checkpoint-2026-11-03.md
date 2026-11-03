# Native animation-upload preview execution checkpoint

**Source:** `218b60d8b6`

**Build:** Alchemy Metal Development `26.4.0d68213`

**Detailed evidence:**
[state archive](../../state/archive/CURRENT-native-animation-preview-checkpoint-2026-11-03.md)

## Completed implementation slice

Darwin animation-upload previews now run targetlessly through native Metal.
CPU camera matrices retain framing and interaction. The producer advances the
avatar, prepares the current mesh data and retains the old eye/head/body mesh
set, rigid and 15-joint scalar skinning, real default-avatar texture, preview
lighting, alpha cut and encoded RGB output.

The native renderer now implements lit avatar-cloth deformation from the same
wind, sine, gravity and depth inputs used by the retained shader. Complete
results publish as sole authoritative backing; incomplete attempts preserve the
last valid image and retry. The old Darwin background/avatar draw,
camera/matrix transaction and framebuffer-copy publication were removed only
after the replacement existed.

## Acceptance evidence

- Warnings-as-errors Xcode Release build 68213, signing and staging passed at
  exact source `218b60d8b6`.
- Bundle verification passed for all 349 ARM64 Mach-O slices; deep strict
  signing passed.
- Exact Ninja targets for appearance, Metal, render and viewer tests linked at
  build 68213.
- Metal API/GPU/shader/load-action validation passed 52,454 Metal assertions,
  including 264 focused lit-preview assertions, 13,207 appearance assertions
  and 2,612 fresh-render assertions.
- `alrender_tests` passed 557,312 assertions; `newview_tests` passed 98,973 and
  self-reported build 68213.
- Harness validation and all 27 harness/tool tests passed.
- Strict audit remains intentionally failing at 3,244 findings; its 2,933
  unique-record count is unchanged and no rule or suppression changed.
- The exact app still links OpenGL and Metal; product and parity gates remain
  open.
- No viewer was launched or visually compared. The bounded presentation case
  is in the owner-attended daily queue.

## Next owner

Continue in the active ledger with targetless authoritative `LLModelPreview`
production. Preserve model-upload preview geometry, materials, skinning,
edges, physics and convex hulls, joint/bone/collision diagnostics, camera,
UI-state updates, publication and recovery; close any native gap before
deleting retained production.
