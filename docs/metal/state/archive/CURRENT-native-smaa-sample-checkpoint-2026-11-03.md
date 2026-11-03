# Native SMAA SamplePath checkpoint — 2026-11-03

## Observed source state

Commit `f3f341e0bec64574e8d37ee310168e118c492ddd` finishes native
ownership of normal and sample-image SMAA on Darwin. The Darwin resource plan
never requests `SMAAArea`, `SMAASearch` or `SMAASample`; their GL creation
bodies and the retained `generateSMAABuffers` transaction compile out. Other
platforms preserve their original GL resources and behavior.

`RenderSMAAUseSample` remains functional. The pipeline polls `SamplePath` at
resource reconciliation and capture transaction boundaries. Its identity is
the Metal device, path, size and nanosecond modification time. An identity
change invalidates the published texture immediately. Decode or upload failures
are logged once per identity, leave native publication ineligible and retry
after 120 frames without admitting stale or partial state.

PNG input with one through four 8-bit components is uploaded into a private,
sampleable RGBA8 Metal texture. Missing color channels become zero and alpha is
opaque, matching the retained GL RGB base-format conversion. Dimensions and
component count are validated against the active Metal device.

Native SMAA has distinct display and edge sources. `SamplePath` substitutes
only the edge detector's color input. Edge output dimensions and the final
linear-light neighborhood pass still come from the actual display source.
Predication retains its independent depth input. Missing sample or predication
state and any native pass failure reject native publication.

Physical regressions use an asymmetric diagonal sample over a nonlinear real
display field. The exact edge oracle detects content and orientation, the
independently staged predicated transaction equals the complete transaction,
and the sample transaction differs from ordinary SMAA while its published
colors remain derived from the real frame. Invalid optional edge textures fail
closed. No viewer was launched, driven or compared; the bounded appearance
question remains in the owner-attended daily queue.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and the Metal compiler present.
- The required `tools/metal/build.sh` command exited 0 at exact source
  `f3f341e0bec64574e8d37ee310168e118c492ddd`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68155` (`26.4.0.68155`).
- Exact warnings-as-errors Ninja test targets and the separate RelWithDebInfo
  appearance-utility target built successfully at that source.
- With Metal API and shader validation enabled, `almetal_tests` passed 52,381
  assertions across 55 cases. Focused split-source SMAA passed 2,923 assertions
  and focused compact unorm expansion passed 46.
- `alrender_tests` passed 557,320 assertions across 137 cases. The focused
  resource-plan matrix passed 3,384 assertions across 37 cases.
- The CPU-only `llrender_tests` binary passed all 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases on the
  physical acceptance host.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped. Compiled and runtime build numbers both report
  68155.
- The broad `llrender_gl_shared_tests` run passed 8,335/8,340 assertions and
  181/224 cases, with four failures and 39 skips. The failures remain the
  established three frame-fence pacing/budget cases and one depth-format case;
  exit 42 is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `ed52efe8a51a52d88a5c17cbbab59593b4c4c9ee1c834e639a4311da80515d19`.
- Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,261 findings and exits 1 as expected.
  Precommit and source-exact reports are byte-identical at
  `5d7d1c6ee0d83f602a2055b1f884c4e570262fddbf3d7515acdf6d7ee182d904`.
  The two-finding increase is the textual audit seeing native
  `LLImageGL::getMetalDevice` references; it is preprocessor-unaware and does
  not subtract the Darwin GL SMAA bodies that now compile out. The ignored
  exact report is `artifacts/metal/gl-strict-native-smaa-sample-exact.json`.
- The packaged viewer and appearance utility have no undefined
  `CGLTexImageIOSurface2D` dependency. `otool -L` still reports both Apple
  OpenGL and Metal in each binary. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. SMAA eligibility still consults legacy GLSL
program completeness, while application-owned GL context, traversal and live
resources remain. Complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Remove SMAA's remaining Darwin dependency on the three legacy GLSL program
completeness flags and omit its dead `SMAAEdges`/`SMAABlend` transient GL
descriptions from the Darwin resource plan. Native pipeline creation and
transaction execution remain fail-closed authority. Preserve non-Darwin
behavior, add exact resource-plan and eligibility coverage, and then reassess
the next live GL owner from source reachability.
