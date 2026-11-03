# Native animation-upload preview feature checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `218b60d8b6` (`Make animation previews Metal-authoritative`)

## Implemented native feature owner

- Darwin `LLPreviewAnimation` now schedules without a retained target, starts
  with native Metal backing and publishes a complete result through
  `LLImageGL::adoptMetalRenderTextureAuthoritative()`.
- A scoped CPU camera transaction restores the previous viewer camera, current
  camera and viewport metadata after every attempt without issuing the former
  retained matrix/depth/blend transaction.
- The producer retains playback advancement, yaw, pitch, pan and zoom. It
  prepares current avatar LOD and mesh data and extracts the same visible eye,
  head, upper, lower and conditional eyelash meshes drawn by the old preview;
  hair and skirt remain hidden by the preview's established policy.
- Rigid and scalar-skinned mesh transforms, the real default-avatar texture,
  preview one-light/wrapped-back shading, dummy-avatar tint and the existing
  alpha cutoff are encoded by the Metal lit-preview renderer.
- The source preview supports avatar cloth. The earlier native mirror did not.
  This checkpoint adds a lit scalar-avatar cloth vertex pipeline and passes the
  existing wind, sine-wave, gravity and depth parameters, preserving actual
  deformation instead of silently dropping the feature.
- The destination retains its sRGB sampling descriptor while rendering through
  a compatible linear attachment view, preserving the encoded RGB bytes of the
  former logical RGB target. Surviving fragments publish alpha 1 after the
  cutoff, preserving UI composition.
- Complete output becomes the sole authoritative backing. Missing geometry,
  texture, device, renderer or publication leaves the previous valid result
  intact and the preview dirty for retry.
- The Darwin background/avatar GL draws, retained camera/matrix setup and
  framebuffer-copy/mirror publication were deleted only after native
  production covered these responsibilities. Non-Darwin behavior remains
  isolated.

This preserves animation-upload preview functionality through Metal; it does
not disable animation, cloth or preview modes merely to reduce OpenGL use.

## Deterministic and physical evidence

- `tools/metal/doctor.sh` exited 0 on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 and Metal compiler
  available.
- `tools/metal/build.sh` exited 0 at committed source `218b60d8b6`. The
  warnings-as-errors Xcode Release build linked, signed, validated and staged
  Alchemy Metal Development `26.4.0d68213` (`26.4.0.68213`).
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` exited 0 and checked 349/349 bundled Mach-O files as
  ARM64. `codesign --verify --deep --strict --verbose=2` exited 0.
- Viewer executable SHA-256:
  `0b8c099db9044e89df69bb713872f558ad7e0d49fbfb5c8ccd312c762a54ef6f`.
- Source and packaged `almetal_builtin.metal` are byte-identical at
  `516ef84aa7123130e6d72cd22e07c52e0f57758085be9c265f50f4fdd89623fb`;
  packaged metallib SHA-256 is
  `1a85d0dcb0c05ea07001f668ae9cf1b4ddb2aa9ea8a8187d27daed825485e149`.
- The exact Ninja tree reconfigured at build 68213 and linked
  `llappearance_gl_tests`, `almetal_tests`, `llrender_gl_fresh_tests`,
  `alrender_tests`, `newview_tests` and `alchemy-bin` warnings-as-errors.
- With Metal API/GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,454 assertions across 55 cases. The focused
  `almetal: lit preview renders multiple indexed depth-tested batches` case
  passed 264 assertions and physically proves the new lit-cloth displacement,
  scalar skinning, rigid/lit rendering, alpha cut, sRGB byte semantics and
  opaque surviving output.
- Under the same environment, `llappearance_gl_tests` passed 13,207 assertions
  across 3 cases and `llrender_gl_fresh_tests` passed 2,612 across 96.
- `alrender_tests` passed 557,312 assertions across 137 cases.
  `newview_tests` self-reported build 68213 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- `python3 tools/metal/validate_harness.py` and the 27 harness/tool tests pass;
  `git diff --check` passes.
- Exact packaged symbol inspection finds
  `LLPreviewAnimation::renderWithoutTarget()`,
  `LLViewerCamera::setPerspectiveWithoutRenderState()` and authoritative Metal
  publication.

## Strict and binary boundary

- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-audit-animation-preview-cutover-strict.json` exited 1 as
  required while findings remain: 2,408 files / 3,244 findings. Report SHA-256
  is `8b2a1c99316a632f58ce177ac5281bc0f8eade85144e8d51a49790feb4597079`.
- The 2,933 unique path/kinds/snippet record count is unchanged from the prior
  checkpoint. The two animation-preview findings are wrapper/API references,
  not retained producer calls; no audit rule or suppression changed.
- `otool -L` still finds both OpenGL and Metal. Linked/runtime no-GL and all
  remaining feature/parity gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared.
Animation-preview presentation is queued in
`docs/metal/work/active/visual-review-queue.md` for an owner-attended daily
review. Physical GPU evidence proves resource, transform, cloth, shading,
alpha, color-storage and publication behavior, not final visual parity.

## Next executable action

Replace the retained-target producer for Darwin `LLModelPreview` with targetless
authoritative Metal output. Preserve static and skinned model modes,
textures/material colors, edges, physics overlays and convex hulls,
joint/bone/collision diagnostics, camera, UI-state updates, publication and
retry semantics. Implement missing native behavior before deleting its retained
draw.
