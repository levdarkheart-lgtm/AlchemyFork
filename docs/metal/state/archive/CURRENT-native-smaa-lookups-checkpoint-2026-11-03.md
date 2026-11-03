# Native ordinary-SMAA lookup checkpoint — 2026-11-03

## Observed source state

Commit `0971551338534843ae13516f98183bb781c230f3` removes the three
duplicate Darwin GL lookup names from ordinary SMAA. `ALRenderResourceInputs`
now latches `RenderSMAAUseSample` with the rest of the resource decision. On
Darwin the resource plan requests `SMAAArea`, `SMAASearch` and `SMAASample`
only for that explicit sample-image mode; normal SMAA requests none. Non-Darwin
retains the original plan for every SMAA configuration.

Normal Darwin `generateSMAABuffers` returns without acquiring GL edge/blend
targets or binding lookup names. Native display publication already runs the
complete Metal edge, lookup-weight and neighborhood sequence. It publishes only
if the whole demanded transaction succeeds; a failure leaves the retained
full-color GL source authoritative and cannot expose a partial native AA result.

The optional sample-image feature is preserved. When enabled, its three GL
textures and original GL SMAA transaction return, while
`canCaptureMetalWorldPresentation` rejects native publication because native
SMAA does not yet consume `SamplePath`. This is a bounded documented exception,
not a deleted feature or a false native-parity claim.

The native renderer owns private RG8 area and R8 search tables, flips the source
rows exactly as the retained GL loader does, and binds them only during the
weight pass. Physical tests compare edge/complete-transaction output,
lookup-driven weights and linear-light neighborhood blending against independent
CPU references. No viewer was launched or driven; SMAA appearance is queued for
owner-attended daily review.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and Metal compiler present.
- The required `tools/metal/build.sh` command exited 0 at exact source
  `0971551338534843ae13516f98183bb781c230f3`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68153` (`26.4.0.68153`).
- The exact warnings-as-errors Ninja target build and separate RelWithDebInfo
  appearance-utility build exited 0.
- With `MTL_DEBUG_LAYER=1`, `almetal_tests` passed 52,345 assertions across 55
  cases with Metal API Validation enabled. Focused SMAA cases passed 2,916
  edge/complete-transaction assertions, 16,421 lookup-weight assertions and 419
  neighborhood-blending assertions.
- `alrender_tests` passed 557,320 assertions across 137 cases. The focused
  resource-plan matrix passed 60 assertions covering normal/sample SMAA,
  stencil, FXAA and no-AA combinations.
- The CPU-only `llrender_tests` binary passed all 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases on the
  physical acceptance host.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped. Compiled and runtime build numbers both report
  68153.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; exit 42 is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `a78f67bfcfa9da7ed2334ece2de2ad1098236560c1c4cb1124ff16017cd67191`.
- Appearance-utility SHA-256:
  `514308e32e9afd6e17c72c5545061749b8bc1d8bb35205ccca471a11623c864a`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,259 findings and exits 1 as expected.
  Precommit and source-exact reports are byte-identical at
  `daaee20b8acabf2bafe4391b09c5fe5a60082703bbefcefca5a948ca439eeca9`.
  The ignored exact report is
  `artifacts/metal/gl-strict-native-smaa-lookups.json`.
- The packaged viewer and appearance utility have no undefined
  `CGLTexImageIOSurface2D` dependency. `otool -L` still reports both Apple
  OpenGL and Metal in each binary. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Sample-image SMAA, application-owned GL context,
traversal and resources remain; complete parity, prolonged stability,
independent build/review evidence, strict no-GL and measured performance gates
remain open.

## Next executable boundary

Finish the SMAA owner without deleting `RenderSMAAUseSample`. Decode and upload
`SamplePath` as an exact native texture with source identity and failure/retry
semantics. Extend native edge detection to accept that sample independently of
the real display source which neighborhood blending must still consume. Add
physical orientation/content and split-source regressions, then remove the last
Darwin GL lookup exception and retained GL SMAA transaction while preserving
non-Darwin behavior.
