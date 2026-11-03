# Native material-preview readiness checkpoint — 2026-11-03

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer commit:** `a8524f0d4f04e4b066ce93080da99e77fdf095c7`

## Product movement

Eligible Darwin PBR material previews no longer wait for the retained
auxiliary GL screen or `ProbePostPing` resource-plan entry before attempting
their complete native Metal geometry, post-processing and publication
transaction. The dynamic-texture targetless contract now reports one of three
outcomes:

- `COMPLETED`: native publication succeeded and the scheduler counts the draw
  without acquiring or touching the shared GL target;
- `RETAINED_FALLBACK`: the ordinary target-backed callback should execute in
  the same frame; or
- `PENDING`: neither native publication nor a safe retained fallback completed,
  so the item stays pending without acquiring an invalid GL target.

When a native attempt fails and retained preview resources are already ready,
the existing one-shot skip still sends the same item through the retained
callback in that frame without repeating the identical Metal attempt. When
those resources are absent, the scheduler leaves the preview pending and the
next frame retries native publication. Non-Darwin behavior and all other
dynamic textures retain the original target-backed contract.

This is a readiness cutover, not resource elimination. A live material preview
still sets `ALRenderResourceInputs::mMaterialPreview`, which plans the retained
probe pack and post targets even when native publication will succeed. The
dynamic-texture base still owns a GL texture, the scheduler still returns early
when global GL is disabled, classic/non-HDR/post-suppressed preview modes still
fall back, and application-owned OpenGL remains linked and reachable.

## Source changes

- `LLViewerDynamicTexture::renderWithoutTarget()` now returns the explicit
  `ETargetlessRenderResult` tri-state. Existing subclasses default to retained
  fallback.
- `LLViewerDynamicTexture::updateAllInstances()` distinguishes a completed
  native transaction from an item that must stay pending; only an explicit
  retained-fallback result enters shared-target setup.
- Darwin `LLGLTFPreviewTexture::needsRender()` no longer gates the native
  attempt on the retained auxiliary screen or post plan. Other platforms keep
  the original readiness gate.
- `LLGLTFPreviewTexture::retainedRenderReady()` confines those retained
  prerequisites to fallback selection after a failed native attempt.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and the Metal compiler.
- `tools/metal/build.sh build` exited 0 at exact source
  `a8524f0d4f04e4b066ce93080da99e77fdf095c7`. The warnings-as-errors Apple
  Silicon Xcode Release build produced Development build `26.4.0d68177`
  (`26.4.0.68177`), linked, signed, validated and staged successfully.
- Exact-source Ninja RelWithDebInfo test targets rebuilt successfully. The
  separate warnings-as-errors appearance-utility configuration reported build
  68177 and its target had no work pending.
- Focused lit/PBR material-preview GPU coverage passed 426 assertions across
  two cases with Metal API and shader validation enabled.
- With the same validation layers, full `almetal_tests` passed 52,381
  assertions across 55 cases. `alrender_tests` passed 557,312 assertions across
  137 cases.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases.
  `llrender_tests` passed all 1,513 assertions across 203 cases: 202 passed and
  the established single-palette font case skipped.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped. Compiled and runtime build numbers both
  reported 68177.
- `llrender_gl_shared_tests` reproduced the known non-gating exit 42: 8,335 of
  8,340 assertions passed; 181 of 224 cases passed, four failed and 39 skipped.
  The failures remain three frame-fence pacing/budget cases and one depth-format
  case. This run is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` exited 0 and checked 349/349 bundled Mach-O slices as
  ARM64.
- Viewer executable SHA-256:
  `e30f980e846efd1c3614cc059ad0f40350566a39d8c66b41b785454b41ae6ef3`.
  Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-native-material-preview-readiness.json` exited 1
  as expected at 2,408 files / 3,262 findings. Report SHA-256 is
  `e1751475ea7cb202f3e20b092e1ef6e04d54f933994def55c9fd4119b89e9937`.
- `file`, `otool -L` and `nm -u` confirm both primary binaries are arm64 and
  have no undefined `CGLTexImageIOSurface2D`; both Apple OpenGL and Metal remain
  linked. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared. Visual
  material-preview questions remain in owner-attended queue row 17, advanced
  only to exact build 68177.

## Next executable boundary

Make retained preview resource demand explicit instead of equating it with a
live preview. On Darwin, successful native previews must stop planning/building
the probe pack and `ProbePostPing`; a failed native attempt must request the
retained plan, stay pending until it is ready, then execute the preserved
fallback. Preserve the current non-Darwin plan and fail-closed behavior. After
that resource cutover, remove the global GL-disabled scheduler gate and dual GL
texture ownership only when all affected dynamic-texture paths have a safe
owner.

The product remains incomplete. Unsupported preview modes, other dynamic
textures, application-owned GL context/traversal/resources, complete parity
and stability, paired profiling, independent review and strict release gates
remain open.
