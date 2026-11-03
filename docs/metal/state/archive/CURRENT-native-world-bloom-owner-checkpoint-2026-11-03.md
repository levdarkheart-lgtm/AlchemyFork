# Native-only Darwin world bloom checkpoint — 2026-11-03

## Observed source state

Source checkpoint `ae9cdcd1f7d2fcb4d414444abc1ac8fb27f3f806` makes
native Metal the only Darwin world-view HDR bloom, halation and cross-filter
authority. Native demand now follows configured effect state and exact world
capture eligibility without consulting `gCrossFilterProgram` or
`bloomHDRReady`. A native pipeline, encoding, command-completion or publication
failure leaves the exact display transaction unpublished rather than silently
substituting retained GL.

Darwin's world finalizer leaves its retained `Bloom` object empty and does not
call `generateBloomHDR`, so `colorCorrect` cannot composite a parallel GL world
pyramid or streak target. The retained world implementation remains unchanged
on non-Darwin platforms.

This is intentionally a world-view boundary, not a claim that all bloom GLSL is
dead. `LLGLTFPreviewTexture::render` still runs retained GL luminance, exposure,
bloom, color correction and final presentation for material preview. It renders
a separate complete native preview, but `postRender` adopts that target only
after the retained dynamic-texture post render reports success. Exact object
inspection therefore still finds the bloom shader program objects,
`bloomHDRReady`, `generateBloomHDR`, and a material-preview reference to the
generator. Removing them now would delete live preview behavior.

The physical native reference covers the production extract, downsample and
upsample pyramid, bloom strength, warm halation format/tint, cross-filter target
and composite, display batching, malformed resources and a too-shallow pyramid.

No viewer was launched, driven, photographed or compared. One bounded world
bloom/halation/cross-filter case is queued for an owner-attended daily review.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and the Metal compiler present.
- The required default `tools/metal/build.sh build` command exited 0 at exact
  source `ae9cdcd1f7d2fcb4d414444abc1ac8fb27f3f806`, using the Apple Silicon
  Xcode Release preset with warnings as errors. It produced Development build
  `26.4.0d68165` (`26.4.0.68165`).
- The separate warnings-as-errors Ninja RelWithDebInfo appearance-utility
  target completed at that source with no work required.
- The focused native world-bloom reference passed 67 assertions with Metal API
  and shader validation enabled.
- With the same validation layers enabled, complete `almetal_tests` passed
  52,381 assertions across 55 cases.
- `alrender_tests` passed 557,312 assertions across 137 cases.
- The CPU-only `llrender_tests` binary passed all 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases on the
  physical acceptance host.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped. Compiled and runtime build numbers both report
  68165.
- The broad `llrender_gl_shared_tests` run passed 8,335/8,340 assertions and
  181/224 cases, with four failures and 39 skips. The failures remain the
  established three frame-fence pacing/budget cases and one depth-format case;
  exit 42 is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `e45db46afe804be11f6577aef5e88dcc97e52fd1fe934c75427e64e493510a62`.
- Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,262 findings and exits 1 as expected. The
  scanner is textual and preprocessor-unaware, so compiling out the Darwin world
  call does not remove findings from the preserved non-Darwin and preview
  source. Exact report SHA-256 is
  `536fd9b2a39e6ffa41faf3ae0ad715d37e4c6f74835bc19746b47556b28a3dfd`;
  the ignored report is
  `artifacts/metal/gl-strict-native-world-bloom-owner.json`.
- Exact Darwin object inspection finds none of the retained FXAA/SMAA/CAS or
  DoF program globals and none of the retained DoF methods. It does find the
  retained bloom programs/generator and the material-preview reference described
  above; those remain a live, explicitly bounded owner.
- The packaged viewer and appearance utility have no undefined
  `CGLTexImageIOSurface2D` dependency. `otool -L` still reports both Apple
  OpenGL and Metal in each binary. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Material-preview publication still depends on
its retained GL post transaction. The application-owned GL context, traversal
and many live resources also remain. Complete parity, prolonged stability,
independent build/review evidence, strict no-GL and measured performance gates
remain open.

## Next executable boundary

Trace `LLGLTFPreviewTexture::render` and `postRender`, dynamic-texture success
semantics and `adoptMetalRenderTexture` end to end. Make a complete native PBR
preview sufficient for publication without first running the retained GL
luminance, exposure, bloom, color-correction and final-blit transaction.
Preserve failure/refresh behavior and remove only Darwin shader/resource
lifecycle proven unreachable afterward.
