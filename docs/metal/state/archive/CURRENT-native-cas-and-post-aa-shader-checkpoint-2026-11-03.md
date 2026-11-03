# Native-only Darwin CAS and post-AA shader checkpoint — 2026-11-03

## Observed source state

Source checkpoint `641076dd483ae3c76307c02d21b95508568e5d7c` contains commits
`3400172d70` and `641076dd48`. It makes native Metal the only Darwin CAS
authority and removes the retained post-AA/sharpen GLSL program lifecycle from
the Darwin viewer.

Native CAS demand now follows nonzero configured sharpness rather than
`gCASProgram` completeness. Native pipeline creation, encoding, command
completion and publication remain fail closed; a failed required native CAS
transaction withdraws native publication. The retained Darwin final-post path
does not call standalone CAS, cannot select inline blit sharpening and always
uses the ordinary final blit. Non-Darwin standalone and inline GL behavior is
unchanged.

The Darwin shader manager no longer declares, defines, constructs, unloads or
loads `gFXAAProgram`, the three `gSMAA*Program` stages, `gCASProgram` or
`gBlitWithEffectsSharpenProgram`. Advanced Graphics no longer filters native
FXAA/SMAA choices through those absent legacy program-completeness flags. Static
symbol inspection of the exact objects and final packaged viewer finds none of
the six retained globals.

Physical Metal coverage compares the production CAS kernel against an
independent scalar reference and exercises its complete position in the world
final-presentation chain. The prior complete FXAA/SMAA, SamplePath and resource
plan coverage remains green. No viewer was launched, driven, photographed or
compared. The combined FXAA/SMAA/CAS appearance question is queued for an
owner-attended daily review.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and the Metal compiler present.
- The required default `tools/metal/build.sh build` command exited 0 at exact
  source `641076dd483ae3c76307c02d21b95508568e5d7c`, using the Apple Silicon
  Xcode Release preset with warnings as errors. It produced Development build
  `26.4.0d68161` (`26.4.0.68161`).
- The separate warnings-as-errors Ninja RelWithDebInfo appearance-utility
  target completed at that source.
- With Metal API and shader validation enabled, `almetal_tests` passed 52,381
  assertions across 55 cases. The focused production CAS oracle passed 117
  assertions; the complete world final-presentation chain passed 1,850.
- `alrender_tests` passed 557,312 assertions across 137 cases.
- The CPU-only `llrender_tests` binary passed all 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases on the
  physical acceptance host.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped. Compiled and runtime build numbers both report
  68161.
- The broad `llrender_gl_shared_tests` run passed 8,335/8,340 assertions and
  181/224 cases, with four failures and 39 skips. The failures remain the
  established three frame-fence pacing/budget cases and one depth-format case;
  exit 42 is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `9207ac558015e6c2df61a17041fc7ee5d086a6e32181b04aeabe3652db52fa67`.
- Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,262 findings and exits 1 as expected. The
  one-finding increase is a textual, preprocessor-unaware consequence of
  splitting the final blit into a Darwin ordinary-blit line and a non-Darwin
  conditional-blit line; it is not a new live GL program. Exact report SHA-256
  is `b258e96fccba33f01a3d729b86aa04c99bccacb039fe3f5221f117970aa73d49`;
  the ignored report is
  `artifacts/metal/gl-strict-native-post-aa-shaders.json`.
- Static `nm` inspection finds no symbol for `gFXAAProgram`,
  `gSMAAEdgeDetectProgram`, `gSMAABlendWeightsProgram`,
  `gSMAANeighborhoodBlendProgram`, `gCASProgram` or
  `gBlitWithEffectsSharpenProgram` in the final packaged viewer.
- The packaged viewer and appearance utility have no undefined
  `CGLTexImageIOSurface2D` dependency. `otool -L` still reports both Apple
  OpenGL and Metal in each binary. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Depth of Field native demand and publication
still depend on retained GLSL readiness and outcome, and Darwin still executes
the retained DoF path. The application-owned GL context, traversal and many
live resources also remain. Complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Trace the Darwin DoF resource plan and `mMetalDepthOfFieldGL*` publication
gate, then make configured DoF demand the native transaction directly. Compile
out the retained Darwin DoF call/body and remove its unused Darwin GLSL program
lifecycle while preserving every optical variant, fail-closed native
publication and non-Darwin behavior. Run the physical all-variant DoF oracle
and complete source-exact validation before selecting the next live owner.
