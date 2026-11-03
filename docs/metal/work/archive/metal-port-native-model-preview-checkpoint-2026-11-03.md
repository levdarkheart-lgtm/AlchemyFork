# Native model-upload preview execution checkpoint

**Source:** `b3f6124664`

**Build:** Alchemy Metal Development `26.4.0d68215`

**Detailed evidence:**
[state archive](../../state/archive/CURRENT-native-model-preview-checkpoint-2026-11-03.md)

## Completed implementation slice

Darwin model-upload previews now run targetlessly through native Metal. Static
textured/material-colored faces, optional edges, physics meshes, convex hulls,
explode transforms, rigged/skinned faces and joint/bone/collision diagnostics
are retained. Degenerate-triangle wide-line and point reporting is implemented
by explicit Metal pipelines rather than omitted.

CPU camera state replaces the retained GL matrix transaction. Complete output
publishes as sole authoritative backing; incomplete attempts preserve the last
valid result and retry. Scoped native vertex uploads fill exact Metal stores
without creating GL names. The Darwin retained producer is bypassed only after
the replacement owns the feature.

## Acceptance evidence

- Warnings-as-errors Xcode Release build 68215, signing and staging passed at
  exact source `b3f6124664`.
- Bundle verification passed for all 349 ARM64 Mach-O slices; deep strict
  signing passed; source and packaged MSL are byte-identical.
- Exact Ninja targets for appearance, Metal, render and viewer tests linked at
  build 68215.
- Metal validation passed 52,468 Metal assertions, 13,207 appearance
  assertions and 2,612 fresh-render assertions. The focused native-upload case
  passed 17 assertions.
- `alrender_tests` passed 557,312 assertions; `newview_tests` passed 98,973 and
  self-reported build 68215.
- The complete shared-state diagnostic suite is transparently non-green at
  8,352/8,357 assertions because four legacy/headless frame-fence assertions
  and one retained depth-format assertion fail; the new upload case passes.
- Harness validation and all 27 harness/tool tests passed.
- Strict audit remains intentionally failing at 3,247 findings / 2,934 unique
  records; no rule or suppression changed.
- The exact app still links OpenGL and Metal; product and parity gates remain
  open.
- No viewer was launched, driven, photographed or visually compared. The
  bounded presentation case is in the owner-attended daily queue.

## Next owner

Remove the now-replaced retained model-preview GL source and any helper used
only by it, then rerun the source-exact validation and strict inventory. After
that, implement the next live Darwin GL producer's complete Metal replacement
before retiring it.
