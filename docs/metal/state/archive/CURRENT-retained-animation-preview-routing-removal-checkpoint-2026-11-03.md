# Retained animation-preview routing-removal checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `38ff186e4f` (`Remove retained animation preview routing`)

## Replacement-first boundary

Commit `218b60d8b6` first implemented and physically validated the complete
Darwin animation-upload preview through targetless native Metal. That producer
preserves playback, camera interaction, rigid and scalar-skinned avatar meshes,
the default-avatar texture, preview lighting, alpha cutoff and avatar-cloth
deformation. Only after that checkpoint passed did `38ff186e4f` remove the
remaining retained-target routing.

`LLPreviewAnimation` now requests native Metal backing unconditionally for this
Apple-only product, always participates in targetless scheduling, cannot return
the retained-fallback result and cannot invoke the base framebuffer-copy
`postRender()` path. Its non-Darwin stub can only remain pending; the supported
Darwin route publishes the previously validated Metal result authoritatively.

The large renderer in this source was already Metal-only before this cleanup.
The change removes 15 lines of obsolete routing and adds three replacement
lines; it does not delete animation, cloth, interaction or preview behavior.

## Exact build and test evidence

- `tools/metal/doctor.sh` exited 0 on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- `tools/metal/build.sh` exited 0 at committed source `38ff186e4f`. The
  warnings-as-errors Xcode Release build linked, signed, validated and staged
  Alchemy Metal Development `26.4.0d68219` (`26.4.0.68219`).
- `tools/metal/verify_app.sh` checked 349/349 Mach-O files as ARM64 and deep
  strict code-sign verification passed.
- Viewer executable SHA-256:
  `242a84184a705d78adb73533c0342e31c92ec978feb1efade4b406945903e8c9`.
- Source and packaged MSL are byte-identical at
  `6fcd3271ea0fed0a7dde4fec6e66ce71a8ec3fd1b43d4637f850ac8717af8110`;
  packaged metallib SHA-256 is
  `024aba5a70e01ce501c884535ad53f438d626d756c7c2148ee08a1a7a2aacc2d`.
- The exact Ninja warnings-as-errors tree reconfigured at build 68219 and
  linked `llappearance_gl_tests`, `almetal_tests`,
  `llrender_gl_fresh_tests`, `alrender_tests`, `newview_tests` and
  `alchemy-bin`.
- With Metal API/GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,468 assertions across 55 cases. The focused lit
  preview test passed 275 assertions and covers scalar skinning, lit cloth,
  alpha cutoff, sRGB byte semantics and authoritative output resources.
- `llrender_gl_fresh_tests` passed 2,612/96,
  `llappearance_gl_tests` passed 13,207/3 and `alrender_tests` passed
  557,312/137.
- `newview_tests` self-reported build 68219 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- The complete shared-state diagnostic suite remains transparently non-green
  at 8,352/8,357 assertions, 182 passed, 4 failed and 39 skipped across 225
  cases. Its four legacy/headless frame-fence timing assertions and one
  depth-format expectation remain unrelated to this routing-only change.

## Strict inventory result

- `tools/metal/audit_gl.py --mode strict` exits 1 as required while other
  application-owned GL remains. The report is 2,408 files / 3,242 findings /
  2,931 unique path/kinds/snippet records.
- The count is unchanged because the removed routing enumerators and base
  virtual dispatch were not audit findings; no rule or suppression changed.
  Report SHA-256:
  `8a2a55adadfa99fa32b007a2d154c463a56766d393e9e82cda450e25194fc9e3`.
- The two `llfloaterbvhpreview.cpp` matches are `LLImageGL` compatibility-class
  references used to obtain the Metal device and backing texture, not retained
  draw calls.
- The executable still links OpenGL and Metal; the product no-GL gate remains
  open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. The
animation-preview case remains row 25 of the owner-attended daily queue; this
source cleanup does not promote it to visual parity.

## Next executable action

Census the remaining already-native dynamic-preview owners for obsolete
retained routing. Remove only routing whose complete Metal producer is already
validated; otherwise implement and validate the missing behavior first.
