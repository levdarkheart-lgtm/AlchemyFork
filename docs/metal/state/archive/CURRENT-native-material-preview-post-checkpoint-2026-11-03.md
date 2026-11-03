# Native-eligible material-preview post checkpoint — 2026-11-03

## Observed source state

Source checkpoint `fa3c1454b7144354943f36a3192dcf700599bb6d` makes a
complete native Darwin PBR material preview independently authoritative for its
post processing and publication. When the Metal renderer produces the exact
preview target, `LLGLTFPreviewTexture` no longer acquires `ProbePostPing`, runs
retained luminance/exposure/bloom/color-correct/glow/final-blit work, or copies
the retained framebuffer into the dynamic texture. `postRender` restores the
dynamic-texture camera/viewport without the copy, then treats adoption of the
complete Metal target as the only success condition. Failed adoption is not
reported as a successful texture-load level and is retried.

The bounded native path covers loaded PBR material textures and geometry in HDR
mode while post processing is active and a coherent default-probe environment
and compatible destination exist. Non-Darwin behavior is unchanged.

This is not a full material-preview GL elimination. The retained sphere draw
still executes before native batch submission. Classic, non-HDR,
post-suppressed and incomplete-native configurations retain the established GL
post and framebuffer path so no unported feature mode is silently deleted.
Consequently the retained bloom programs and `generateBloomHDR` remain live on
Darwin and are not removed by this checkpoint.

The physical native reference covers glTF texture transforms, alpha
opaque/mask/blend semantics, double-sided rendering, normal/ORM/emissive
textures, dielectric and metallic response, environment/direct light,
exposure metering, every production tonemapper, bloom, warm halation,
cross-filter, chromatic aberration, descriptor validation and malformed input.

No viewer was launched, driven, photographed or compared. One bounded
native-eligible preview-post case is queued for an owner-attended daily review.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host during this session: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2,
  Xcode 26.6, Apple7-family support and the Metal compiler present.
- The required default `tools/metal/build.sh build` command exited 0 at exact
  source `fa3c1454b7144354943f36a3192dcf700599bb6d`, using the Apple Silicon
  Xcode Release preset with warnings as errors. It produced Development build
  `26.4.0d68167` (`26.4.0.68167`).
- The separate warnings-as-errors Ninja RelWithDebInfo appearance-utility
  target completed at that source.
- The focused complete PBR material-preview reference passed 201 assertions
  with Metal API and shader validation enabled.
- With the same validation layers enabled, complete `almetal_tests` passed
  52,381 assertions across 55 cases.
- `alrender_tests` passed 557,312 assertions across 137 cases.
- The CPU-only `llrender_tests` binary passed all 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases on the
  physical acceptance host.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped. Compiled and runtime build numbers both report
  68167.
- The broad `llrender_gl_shared_tests` run passed 8,335/8,340 assertions and
  181/224 cases, with four failures and 39 skips. The failures remain the
  established three frame-fence pacing/budget cases and one depth-format case;
  exit 42 is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `0bff446e430ddd1698b463ef1c73088dfb0a1409788e9155d7447eb942a18f7e`.
- Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,262 findings and exits 1 as expected.
  Exact report SHA-256 is
  `3ed21a476ca138afd494f1200b644fd10891e6361c21fd4f9168c0737be5a8d8`;
  the ignored report is
  `artifacts/metal/gl-strict-native-material-preview-post.json`.
- The packaged viewer and appearance utility are ARM64 Mach-O executables with
  no undefined `CGLTexImageIOSurface2D` dependency. `otool -L` still reports
  both Apple OpenGL and Metal in each binary. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Material-preview GL geometry still executes,
and unsupported native eligibility modes retain their complete GL post path.
The application-owned GL context, traversal and many other live resources also
remain. Complete parity, prolonged stability, independent build/review
evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Split native preview batch assembly from the retained
`gDeferredPBRAlphaProgram` draw and decide native eligibility before binding the
auxiliary screen. Skip that GL shader/geometry transaction whenever native
eligibility is exact. Then extend the native scene contract toward
classic/non-HDR/post-suppressed modes and remove preview-only retained post
resources/programs only when source reachability proves them dead.
