# Retained model-preview source-removal checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `1830ac9d0b` (`Remove retained model preview renderer`)

## Replacement-first boundary

Commit `b3f6124664` first implemented and physically validated the complete
Darwin model-upload preview feature set through targetless native Metal. Only
after that checkpoint passed did `1830ac9d0b` delete the retained/non-Darwin
model-preview renderer.

The removal covers 1,019 lines of retained draw/matrix/shader/physics/rigging
code plus five orphaned helper, declaration and constant lines. It deletes the
GL material-binding and ground-plane helpers, makes native backing unconditional
for this Apple-only product, leaves targetless scheduling enabled and prevents
`postRender()` from restoring framebuffer-copy publication. On Darwin,
`render()` now has exactly one route: `renderMetal()`.

The preserved Metal implementation still owns static textured/material faces,
edges, physics meshes/hulls/explode, rigging/skinning, joint/bone/collision and
degenerate diagnostics, camera/UI updates, publication and fail-closed retry.
This checkpoint removes a proven-obsolete implementation; it does not remove a
viewer feature.

## Exact build and test evidence

- `tools/metal/build.sh` exited 0 at committed source `1830ac9d0b`. The
  warnings-as-errors Xcode Release build linked, signed, validated and staged
  Alchemy Metal Development `26.4.0d68217` (`26.4.0.68217`).
- `tools/metal/verify_app.sh` checked 349/349 Mach-O files as ARM64 and deep
  strict code-sign verification passed.
- Viewer executable SHA-256:
  `c7e0c724c0dfeb55136a1e623fe14216bdb74b827795e1f76842aa1e57d21513`.
- Source and packaged MSL remain byte-identical at
  `6fcd3271ea0fed0a7dde4fec6e66ce71a8ec3fd1b43d4637f850ac8717af8110`;
  packaged metallib SHA-256 remains
  `024aba5a70e01ce501c884535ad53f438d626d756c7c2148ee08a1a7a2aacc2d`.
- The exact Ninja warnings-as-errors tree configured and linked all requested
  viewer/test targets at build 68217.
- With Metal API/GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,468 assertions across 55 cases,
  `llrender_gl_fresh_tests` passed 2,612/96 and
  `llappearance_gl_tests` passed 13,207/3.
- The focused Metal-only upload case passed 17/17 assertions.
  `alrender_tests` passed 557,312/137. `newview_tests` self-reported build 68217
  and passed 98,973 assertions across 743 cases with the established CRT skip.
- The complete shared-state diagnostic suite was rerun and remains transparently
  non-green at 8,352/8,357 assertions: four legacy/headless frame-fence timing
  assertions and one depth-format expectation fail. The focused new upload case
  passes in the same environment.
- Harness validation covers 32 required files; all 27 tooling tests and diff
  checks pass.

## Strict inventory result

- `tools/metal/audit_gl.py --mode strict` exits 1 as expected while other
  application-owned GL remains. The exact report is 2,408 files / 3,242
  findings / 2,931 unique path/kinds/snippet records.
- This is a five-finding/three-unique reduction from the feature checkpoint.
  No rule or suppression changed. Report SHA-256:
  `82107e21c11b1654fc74517ae29df7cfafb5869d2dec8e8dde7c54665348e010`.
- The five remaining `llmodelpreview.cpp` findings are three unique static-audit
  matches on the `LLImageGL` compatibility class name used to access native
  Metal device/texture backing, not retained GL draw calls.
- The executable still links OpenGL and Metal; the product no-GL gate remains
  open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. The
model-preview case remains in the owner-attended daily queue; this source
cleanup does not promote it to visual parity.

## Next executable action

Census the next live application-owned Darwin GL producer, identify its full
user-visible responsibilities, then implement and validate those behaviors in
Metal before retiring it.
