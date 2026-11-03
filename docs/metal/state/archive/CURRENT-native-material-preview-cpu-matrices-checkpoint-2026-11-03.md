# Native material-preview CPU-matrix/fallback-state checkpoint — 2026-11-03

## Observed source state

Source checkpoint `e55ea15076c88caab5c409b7becc9de7fa7f8770` moves the
preview-owned GL setup for a native-eligible Darwin PBR material preview into
the recoverable retained callback. Before native submission, the preview now
snapshots the base dynamic-texture camera state without calling its GL
`preRender`, constructs the preview modelview with `frameModelview()` and the
finite projection with `al_perspective()` on the CPU, and uses those matrices
for the native environment and batch constants.

A successful native preview therefore does not itself unbind texture unit zero,
set or clear the GL viewport/depth buffer, construct the GL color/depth/stencil
or scissor guards, change the auxiliary target pack, clear the GL color buffer,
load the retained projection/modelview, or acquire the retained post scope. It
also retains all earlier cuts: no GL preview shader/geometry, light/probe
mutation, post chain, final blit or framebuffer copy. `gGLLastMatrix` is still
invalidated as a C++ cache precaution because the static sphere can fall back
later after another pass changes the actual GL matrix.

This is not yet a targetless dynamic-texture transaction. The generic
`LLViewerDynamicTexture::updateAllInstances()` scheduler still unbinds the GL
shader and vertex buffer, and `ALOnDemandTarget::drawEach()` acquires, binds and
clears the shared Preview GL target before it calls the preview's `preRender`
and `render`. The scheduler then flushes GL and the native success path calls
the base `postRender(false)` viewport restore. The next boundary is a
native-first scheduler contract that can attempt this preview before taking the
shared GL target, while retaining same-frame fallback for every unsupported or
failed native attempt.

Classic, non-HDR, post-suppressed and incomplete/failed-native configurations
still run the complete retained callback. Non-Darwin behavior is unchanged.
Retained bloom and preview resources remain reachable and must not be removed.
No viewer was launched, driven, photographed or compared; the existing
material-preview row in the owner-attended queue was advanced to build 68173.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and the Metal compiler.
- `tools/metal/build.sh build` exited 0 at exact source
  `e55ea15076c88caab5c409b7becc9de7fa7f8770`, using the warnings-as-errors
  Apple Silicon Xcode Release preset. It produced Development build
  `26.4.0d68173` (`26.4.0.68173`).
- The exact-source Ninja RelWithDebInfo targets `almetal_tests`,
  `alrender_tests`, `llrender_tests`, `llrender_gl_fresh_tests` and
  `newview_tests` built successfully. The separate warnings-as-errors
  RelWithDebInfo `appearance-utility-bin` target completed with no work pending.
- Focused complete PBR material-preview coverage passed 201 assertions with
  Metal API and shader validation enabled. The full validated
  `almetal_tests` binary passed 52,381 assertions across 55 cases.
- `alrender_tests` passed 557,312 assertions across 137 cases.
  `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases.
- `llrender_tests` passed all 1,513 assertions across 203 cases: 202 passed and
  the established single-palette font case skipped.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped. Compiled and runtime build numbers both
  reported 68173.
- `llrender_gl_shared_tests` reproduced the known non-gating exit 42: 8,335 of
  8,340 assertions passed; 181 of 224 cases passed, four failed and 39 skipped.
  The failures remain three frame-fence pacing/budget cases and one depth-format
  case. This run is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` exited 0 and checked 349/349 bundled Mach-O slices as
  ARM64.
- Viewer executable SHA-256:
  `8c0fa87660d9f8e4209f3f3ff3615316108bb0668aff639dcc6a9b3705e4524e`.
  Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-native-material-preview-cpu-matrices.json` exited 1
  as expected at 2,408 files / 3,262 findings. Report SHA-256 is
  `18eb1ab8a12306e3263672506bf5a3ced248b89e298245a2edc5d67eb46e1c15`.
- `file`, `otool -L` and `nm -u` confirm both primary binaries are arm64 and
  have no undefined `CGLTexImageIOSurface2D`; both Apple OpenGL and Metal remain
  linked. The strict no-GL gate is not met.

The product remains incomplete. Application-owned GL context, scheduler,
traversal and resources; unsupported preview variants; complete parity and
stability; paired profiling; independent review; and strict linked/runtime
no-GL evidence remain open.

## Next executable boundary

Give eligible dynamic textures a fail-closed native-first attempt before
`ALOnDemandTarget` acquires/binds/clears the shared Preview GL target. If the
attempt does not publish an exact result, acquire the target and run the
existing retained callback in the same frame. Preserve all other preview and
bake scheduling behavior, then widen native material-preview eligibility only
after this boundary is physically covered.
