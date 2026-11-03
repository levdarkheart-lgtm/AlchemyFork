# Native-only Darwin Depth of Field checkpoint — 2026-11-03

## Observed source state

Source checkpoint `9bac86a3e47ae2aaf272df6bc4a98fe254efb667` makes
native Metal the only Darwin Depth of Field authority. Configured HDR DoF now
demands the native transaction without consulting retained GLSL readiness. A
pipeline, encoding, command-completion or publication failure leaves the exact
display transaction unpublished rather than silently substituting retained GL.

Darwin compiles out the retained two-pass gather/combine call and body, its
transient blur description and its legacy outcome-comparison state. The shader
manager declares, defines, unloads and builds none of the four gather or two
combine programs. Static symbol inspection of the exact objects and packaged
viewer finds none of those globals, `LLPipeline::renderDoF` or
`LLPipeline::depthOfFieldProgramsReady`. The complete retained implementation
and lifecycle remain unchanged on non-Darwin platforms.

The native transaction covers forward and reverse Z, D32F/S8 depth, near blur,
unshaped and shaped aperture variants, highlight controls, anamorphic squeeze,
cat-eye, chromatic fringe, spherical aberration, field stretch/falloff, coma,
both supported blur formats, malformed input and aggregate failure. Its
physical reference test exercises every production variant against independent
CPU calculations.

No viewer was launched, driven, photographed or compared. One bounded DoF
appearance case is queued for an owner-attended daily review.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and the Metal compiler present.
- The required default `tools/metal/build.sh build` command exited 0 at exact
  source `9bac86a3e47ae2aaf272df6bc4a98fe254efb667`, using the Apple Silicon
  Xcode Release preset with warnings as errors. It produced Development build
  `26.4.0d68163` (`26.4.0.68163`).
- The separate warnings-as-errors Ninja RelWithDebInfo appearance-utility
  target completed at that source with no work required.
- With Metal API and shader validation enabled, `almetal_tests` passed 52,381
  assertions across 55 cases. The focused all-variant world DoF reference test
  passed 2,493 assertions.
- `alrender_tests` passed 557,312 assertions across 137 cases.
- The CPU-only `llrender_tests` binary passed all 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases on the
  physical acceptance host.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped. Compiled and runtime build numbers both report
  68163.
- The broad `llrender_gl_shared_tests` run passed 8,335/8,340 assertions and
  181/224 cases, with four failures and 39 skips. The failures remain the
  established three frame-fence pacing/budget cases and one depth-format case;
  exit 42 is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `2ff05074d265a2d17717513b38f4657616b9564e09cfc1a87f50ca32388b0b9a`.
- Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,262 findings and exits 1 as expected. The
  scanner is textual and preprocessor-unaware, so compiling out the Darwin DoF
  body and shader lifecycle does not reduce findings that remain in preserved
  non-Darwin source. Exact report SHA-256 is
  `d17b875fcc3daa97f616b06d455d22c6c2760eb703310909acb33ef5db085db1`;
  the ignored report is `artifacts/metal/gl-strict-native-dof-owner.json`.
- Static `nm` inspection finds no symbol for `gDeferredPostProgram`, its three
  gather variants, either `gDeferredDoFCombineProgram`, `renderDoF` or
  `depthOfFieldProgramsReady` in the final packaged viewer. The prior six
  FXAA/SMAA/CAS/sharpen globals remain absent as well.
- The packaged viewer and appearance utility have no undefined
  `CGLTexImageIOSurface2D` dependency. `otool -L` still reports both Apple
  OpenGL and Metal in each binary. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. HDR bloom and cross-filter native demand still
consult retained GLSL program completeness, and Darwin still executes the
retained bloom pyramid and streak path. The application-owned GL context,
traversal and many live resources also remain. Complete parity, prolonged
stability, independent build/review evidence, strict no-GL and measured
performance gates remain open.

## Next executable boundary

Trace `bloomHDRReady`, `generateBloomHDR`, cross-filter target ownership and
the color-correct composite, then make configured Darwin bloom/cross-filter
demand the native transaction directly. Compile out the retained Darwin
pyramid/streak transaction and remove only its unused GLSL program lifecycle
while preserving halation/streak variants, fail-closed native publication and
non-Darwin behavior. Run the physical bloom/halation/cross-filter oracles and
complete source-exact validation before selecting the next live owner.
