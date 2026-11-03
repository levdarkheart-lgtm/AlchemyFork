# Native-only Darwin post-antialiasing checkpoint — 2026-11-03

## Observed source state

Source checkpoint `a345287be3fdd2ac432afb6408da89c73a9f70b5` completes the
Darwin post-antialiasing owner. It builds on native SamplePath commit
`f3f341e0be` and the final SMAA resource/eligibility cut commit `94a3771606`.

The Darwin render plan requests none of `SMAAEdges`, `SMAABlend`, `SMAAArea`,
`SMAASearch` or `SMAASample`. The first two are no longer described as GL
transients and the last three have no GL texture owner. Darwin compiles out the
retained FXAA and SMAA pass bodies. Its final-post branch neither acquires an AA
target nor calls the GL passes, and native presentation eligibility no longer
consults `gFXAAProgram` or any `gSMAA*Program` completeness flag.

Native Metal is now the only Darwin post-AA authority. Selected FXAA always
demands the native quality-matched pass. Selected normal or SamplePath SMAA
always demands the native edge, lookup-weight and neighborhood transaction;
SamplePath readiness remains an explicit eligibility condition. Native pipeline
creation, input validation, encoding and command completion retain the existing
fail-closed behavior. Non-Darwin resource planning and GL execution are
unchanged.

Physical Metal coverage remains the authority for FXAA/SMAA algorithms. It
checks four FXAA quality levels against a CPU oracle, SMAA edges and lookup
weights against independent references, linear-light neighborhood blending,
predication, split display/sample sources, invalid resources and aggregate
transaction failure. No viewer was launched or driven; the combined AA visual
question is queued for an owner-attended daily review.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and the Metal compiler present.
- The required `tools/metal/build.sh` command exited 0 at exact source
  `a345287be3fdd2ac432afb6408da89c73a9f70b5`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68158` (`26.4.0.68158`).
- Exact warnings-as-errors Ninja test targets and the separate RelWithDebInfo
  appearance-utility target built successfully at that source.
- With Metal API and shader validation enabled, `almetal_tests` passed 52,381
  assertions across 55 cases. This includes the established four-quality FXAA
  oracle and the native SMAA lookup, predication and split-source regressions.
- `alrender_tests` passed 557,312 assertions across 137 cases. The focused
  resource-plan matrix passed 3,376 assertions across 37 cases and proves
  Darwin wants no retained GL SMAA resource under normal/sample and
  stencil/no-stencil combinations.
- The CPU-only `llrender_tests` binary passed all 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases on the
  physical acceptance host.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped. Compiled and runtime build numbers both report
  68158.
- The broad `llrender_gl_shared_tests` run passed 8,335/8,340 assertions and
  181/224 cases, with four failures and 39 skips. The failures remain the
  established three frame-fence pacing/budget cases and one depth-format case;
  exit 42 is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `5001ca71da5d0b27b28c250c051e806e9e5aea877ca0ac9118ebfbfa8fb779d0`.
- Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,261 findings and exits 1 as expected. The
  audit is textual and preprocessor-unaware, so compile-time Darwin ownership
  changes do not necessarily reduce its count. Exact report SHA-256 is
  `2114759ada7817a54821d55e9dab49ee076dcfa119b5c4caaa2bc3f1320606f8`;
  the ignored report is
  `artifacts/metal/gl-strict-native-antialiasing-owner.json`.
- The packaged viewer and appearance utility have no undefined
  `CGLTexImageIOSurface2D` dependency. `otool -L` still reports both Apple
  OpenGL and Metal in each binary. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. CAS already has a native implementation but its
Darwin demand still depends on `gCASProgram`, and retained final post can sharpen
through either the standalone GLSL pass or the inline blit shader. The
application-owned GL context, traversal and many live resources also remain.
Complete parity, prolonged stability, independent build/review evidence,
strict no-GL and measured performance gates remain open.

## Next executable boundary

Make native CAS the only Darwin sharpening owner. Demand it whenever configured
sharpness is nonzero, compile out the standalone GL CAS pass and prevent the
retained final blit from choosing inline GL sharpening. Native failure must
withdraw publication, and non-Darwin behavior must remain unchanged. Re-run the
existing physical CAS oracle and complete source-exact validation before
selecting the next live display-post owner.
