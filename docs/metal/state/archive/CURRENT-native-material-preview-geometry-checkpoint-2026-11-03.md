# Native-eligible material-preview geometry checkpoint — 2026-11-03

## Observed source state

Source checkpoint `ed90d8b6a0b81fa349ed6cf5062ef03068384cb9` makes the
native-eligible Darwin PBR material preview authoritative before retained GL
geometry execution. The established retained draw is now a recoverable local
callback. Darwin first assembles and submits the native scene; when that
produces the exact destination, the preview does not bind the auxiliary GL
screen, select or bind `gDeferredPBRAlphaProgram`, issue
`LLRenderPass::pushGLTFBatch`, flush that target, acquire the retained post
target, run retained post processing or copy the retained framebuffer during
publication. Non-Darwin executes the same retained callback and preserves its
prior fail-before-draw resource behavior.

This checkpoint also corrects an accidental file-scope `ALTransientTargets`
initialization introduced by source checkpoint `fa3c1454b7`. The transient
target and retained post pointer now have function-local lifetime inside
`LLGLTFPreviewTexture::render()`. The bad placement happened to compile on
Darwin because the invalid early-return branch was preprocessed out, but was
not accepted as a valid cross-platform implementation.

The cut remains deliberately eligibility-bounded. Native input currently
requires HDR PBR mode, active post processing, complete geometry/textures,
default-probe state and a compatible destination. Classic, non-HDR,
post-suppressed and incomplete/failed-native configurations execute the
retained callback and complete GL post path. Camera/matrix establishment,
preview-sphere preparation and some shared GL state/light setup also still
precede the native attempt. The retained bloom program lifecycle and
`generateBloomHDR` therefore remain reachable and must not be removed yet.

No viewer was launched, driven, photographed or compared. The existing
material-preview row in the owner-attended daily queue was advanced to build
68169 and now covers native geometry, post and publication together.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and the Metal compiler present.
- `tools/metal/build.sh build` exited 0 at exact source
  `ed90d8b6a0b81fa349ed6cf5062ef03068384cb9`, using the Apple Silicon Xcode
  Release preset with warnings as errors. It produced Development build
  `26.4.0d68169` (`26.4.0.68169`).
- The warnings-as-errors Ninja RelWithDebInfo production object compiled before
  the source checkpoint. At exact source, the five required test binaries
  rebuilt successfully. The separate RelWithDebInfo appearance-utility target
  `appearance-utility-bin` completed at exact source with no work pending.
- Two auxiliary target-name mistakes were recorded rather than hidden:
  `appearance_utility` in the main multi-target build and `alchemy-bin` in the
  appearance configuration each exited 1 as unknown targets before useful
  build work. The corrected commands above succeeded.
- Focused PBR material-preview coverage passed 201 assertions with Metal API
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
  68169.
- The broad `llrender_gl_shared_tests` run passed 8,335/8,340 assertions and
  181/224 cases, with four failures and 39 skips. The failures remain the
  established three frame-fence pacing/budget cases and one depth-format case;
  exit 42 is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `963903f13474c4f6848acf85dcf63d23b9a9f2f0eab2d2f1a45c5ca97342613c`.
- Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,262 findings and exits 1 as expected.
  Exact report SHA-256 is
  `8cea2f2a7d299d91b7a5ed09a09c06a59a8de213848cccf726e7c66d82163f19`;
  the ignored report is
  `artifacts/metal/gl-strict-native-material-preview-geometry.json`.
- The packaged viewer and appearance utility are ARM64 Mach-O executables with
  no undefined `CGLTexImageIOSurface2D` dependency. `otool -L` still reports
  both Apple OpenGL and Metal in each binary. The strict no-GL gate is not met.
- `python3 tools/metal/validate_harness.py` validated all 32 required files, and
  `python3 -m unittest discover -s tools/metal/tests -v` passed all 27 tests.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Unsupported preview eligibility modes retain
their GL geometry and post path, and shared camera/matrix/light preparation
still includes GL-owned state. The application-owned GL context, traversal and
many other live resources also remain. Complete parity, prolonged stability,
independent build/review evidence, strict no-GL and measured performance gates
remain open.

## Next executable boundary

Split native preview camera/matrix/light derivation from the remaining shared
GL state so an eligible preview does not clear or mutate a retained GL render
context before native submission. Then extend the native scene contract toward
classic, non-HDR and post-suppressed configurations, and remove preview-only
retained resources/programs only when source reachability proves them dead.
