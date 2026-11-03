# Native material-preview backing cutover checkpoint — 2026-11-03

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer commit:** `2c09afc020cfac9b9a122cfdd5286d4b72e7c8f2`

## Product movement

Darwin PBR material previews no longer allocate an OpenGL texture at
construction. `LLViewerDynamicTexture` now has an explicit initial-backing
choice. The established retained default still creates GL storage, while the
Darwin preview selects a logical `LLImageGL` wrapper whose storage is created
through `createMetalTexture()` and whose GL texture name remains zero.

The normal targetless geometry/post/publication transaction therefore owns only
the native destination. If native execution cannot complete and the retained
callback later succeeds, base `postRender()` creates GL storage immediately
before its framebuffer copy. A failed native-backing creation is logged but
does not silently invent success: the explicit retained fallback remains
available on demand.

This removes eager dual storage from one production owner. It does not make the
whole viewer no-context capable. Classic, non-HDR and post-suppressed material
preview modes still request retained fallback, other dynamic textures retain
their GL backing, and current global GL teardown destroys broader pipeline
state.

## Source changes

- `LLViewerDynamicTexture::EInitialBacking` names retained-GL and native-Metal
  construction. Retained GL remains the default for every existing caller.
- The Darwin-only native initializer keeps the logical image wrapper, clears a
  correctly sized raw image, publishes it through `createMetalTexture()`, and
  asserts that the wrapper has no GL texture.
- `LLGLTFPreviewTexture` selects native initial backing on Darwin. Non-Darwin
  construction is unchanged.
- Base `postRender(success)` preserves the retained recovery contract: a
  successful retained render creates missing GL backing before copying from the
  framebuffer. Targetless native success never enters that callback.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and the Metal compiler.
- Incremental warnings-as-errors Ninja compilation and link completed before
  commit. `tools/metal/build.sh build` then exited 0 at exact source
  `2c09afc020cfac9b9a122cfdd5286d4b72e7c8f2`. The warnings-as-errors Apple
  Silicon Xcode Release build produced Development build `26.4.0d68183`
  (`26.4.0.68183`), linked, signed, validated and staged.
- Exact-source Ninja RelWithDebInfo test targets rebuilt successfully. The
  separate warnings-as-errors appearance-utility target reported no work
  pending at build 68183.
- Focused lit/PBR material-preview GPU coverage passed 426 assertions across
  two cases with Metal API and shader validation enabled.
- With those validation layers, full `almetal_tests` passed 52,381 assertions
  across 55 cases. `alrender_tests` passed 557,312 assertions across 137 cases;
  its focused resource-plan matrix passed 3,376 assertions across 37 cases.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases.
  `llrender_tests` passed 1,513 assertions across 203 cases: 202 passed and the
  established single-palette font case skipped.
- `newview_tests` passed 98,973 assertions across 743 cases: 742 passed and one
  platform CRT case skipped. Compiled and runtime build numbers were 68183.
- `llrender_gl_shared_tests` reproduced known non-gating exit 42: 8,335 of
  8,340 assertions passed; 181 of 224 cases passed, four failed and 39 skipped.
  The failures remain three frame-fence pacing/budget cases and one depth-format
  case. This run is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` exited 0 and checked 349/349 bundled Mach-O slices as
  ARM64.
- Viewer executable SHA-256:
  `aff83936542ad53040913780376085218875fa4cf88035fd5e162aae39702ca3`.
  Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-native-preview-backing-cutover.json` exited 1 as
  expected at 2,408 files / 3,262 findings. Report SHA-256 is
  `65fb81e70570f13ae98d61d306f666439d2d8aac504946e60602480d98a7abf2`.
- `file`, `otool -L` and `nm -u` confirm the primary binaries are arm64, the
  viewer has no undefined `CGLTexImageIOSurface2D`, and Apple OpenGL and Metal
  remain linked. The strict no-GL gate is not met.
- Source inspection confirms the Darwin preview selects native backing, native
  creation asserts `!getHasGLTexture()`, and retained `postRender()` creates GL
  storage only at its successful fallback boundary.
- `python3 tools/metal/validate_harness.py` validated all 32 required harness
  files. `python3 -m unittest discover -s tools/metal/tests -v` passed all 27
  harness/tool tests.
- No viewer executable was launched, driven, photographed or compared. Visual
  material-preview questions remain in owner-attended queue row 17, advanced
  only to exact build 68183.

## Next executable boundary

Extend the complete native material-preview transaction to classic, non-HDR
and post-suppressed configurations so those modes no longer request retained
fallback. Prove retained preview bloom/program reachability dead before removal,
then preserve broader native pipeline lifetime across full GL teardown before
claiming runtime no-context operation.

The product remains incomplete. Unsupported preview modes, other dynamic
textures, application-owned GL context/traversal/resources, complete parity
and stability, paired profiling, independent review and strict release gates
remain open.
