# Unreferenced GL restore method removal checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.  
**Branch:** `codex/apple-silicon-metal`  
**Renderer source:** `15ed39561c` (`Delete unreferenced GL restore methods`)

## Implemented cutover

- Source call-graph tracing after the viewer/pipeline restore-lifecycle deletion
  proved that 12 class-level restore entry points had no callers.
- The declarations and definitions for `LLGLState`, `LLSky`, `LLSkyTex`,
  `LLVOSky`, `LLVOWLSky`, `LLDrawPoolWLSky`, `LLSpatialPartition`,
  `LLViewerDynamicTexture`, `LLVOPartGroup` and `LLVOAvatar` restore methods are
  deleted. The unused base and `LLViewerTexLayerSetBuffer` override of
  `restoreGLTexture()` are also deleted.
- The commit removes 141 lines across 20 files with no replacement abstraction
  or compatibility fallback.
- Live initialization is intentionally preserved: `LLBumpImageList::restoreGL`
  initializes bump images, while `LLManipTranslate::restoreGL` and
  `FSManipTranslateJoint::restoreGL` lazily create their grid textures.
- Source now contains no `restoreGLTexture` entry point. Exact packaged-binary
  inspection finds only the three intentional restore symbols above; optimized
  internal standard-bump initialization does not survive as a separate symbol.

This is dead-API deletion, not runtime no-GL proof. Real process-shutdown
resource releases remain and both OpenGL and Metal are still linked.

## Exact validation evidence

- `tools/metal/doctor.sh` exited 0 on `MacBookPro18,2`: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- `tools/metal/build.sh` completed 1,087 cacheable tasks and the
  warnings-as-errors Xcode Release build at committed source `15ed39561c`. It
  linked, signed, validated and staged Alchemy Metal Development
  `26.4.0d68197` (`26.4.0.68197`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O files as ARM64.
- Viewer executable SHA-256:
  `f935252c86ecc1514c91fd4671e570732bff36cc81468ad77fa52d3036cb3435`.
- Source and packaged `almetal_builtin.metal` are byte-identical at
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`;
  packaged metallib SHA-256 is
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- With Metal API, GPU, shader and load-action validation enabled, the complete
  physical Metal suite passed 52,414 assertions across 55 cases.
- Full `alrender_tests` passed 557,312 assertions across 137 cases.
- The exact `newview_tests` binary self-reported build 68197 and passed 98,973
  assertions across 743 cases: 742 passed and the established CRT case skipped.
- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-restore-method-deletion.json` exited 1 as expected
  at 2,408 scanned files / 3,253 findings. The one-finding reduction reflects
  audit token/call counting rather than the number of deleted methods. Report
  SHA-256:
  `f2ac606640da104c73e267b880a58ff3cbe3d3a9179be890e4f812f2dae459fc`.
- `otool -L` still finds both OpenGL and Metal in the packaged viewer. Strict
  linked and runtime no-GL gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. Dead API
deletion creates no new visual question, so no queue row was added. Existing
parity questions remain in the owner-attended daily queue.

## Next executable action

Delete the empty, single-caller `LLVOPartGroup::destroyGL()` shutdown wrapper.
Then trace the remaining shutdown-only sky, manipulator, bump, font, avatar,
dynamic-texture, box and texture-list owners to real native resource lifetimes
before removing their GL-named release paths.
