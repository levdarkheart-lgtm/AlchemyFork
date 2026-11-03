# Native tone-curve LUT checkpoint — 2026-11-03

## Observed source state

Commit `56c78b9134fafdaad4e60e487aaddf892bd568c8` removes OpenGL
ownership from the Darwin baked tone-curve LUT. Publication state is now
backend-neutral. Darwin publishes only `mMetalToneCurveLut`; non-Darwin retains
the GL RGBA16 texture for later platform pruning. The retained Darwin GL post
bypasses the curve when its GL object is absent, while native grading consumes
the Metal texture.

The identity path clears loaded/dirty state and releases native storage. A
non-identity bake publishes loaded state only after the private shader-read
texture uploads and commits successfully. Failed native publication leaves the
curve unavailable and dirty so a later frame can retry; it cannot advertise a
stale or missing texture.

`alMetalCreateUnormTexture2D` is the shared native publication primitive. It
accepts 8- or 16-bit RGB/RGBA unorm data, expands RGB to opaque RGBA, validates
dimensions and byte-count overflow and publishes only after upload commit. The
physical regression uses the exact production-size 512x1 RGBA16 tone row,
verifies its descriptor and every sample, and rejects invalid width, height,
component count, component size, null input and overflow.

No viewer was launched or driven. Curve appearance, transitions and banding
are queued for an owner-attended daily review rather than inferred from
deterministic tests.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and Metal compiler present.
- The required `tools/metal/build.sh` command exited 0 at exact source
  `56c78b9134fafdaad4e60e487aaddf892bd568c8`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68149` (`26.4.0.68149`).
- `cmake --build build-Darwin-ninja-os-arm64 --config RelWithDebInfo --target
  almetal_tests llrender_tests llrender_gl_fresh_tests
  llrender_gl_shared_tests newview_tests -j 10` exited 0 and linked the exact
  source RelWithDebInfo binaries.
- With `MTL_DEBUG_LAYER=1`, `almetal_tests` passed 52,289 assertions across 54
  cases with Metal API Validation enabled. The new physical case contributes
  17 assertions for the exact 512x1 RGBA16 upload, descriptor semantics and
  invalid-input rejection.
- The CPU-only `llrender_tests` binary passed all 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases on the
  physical acceptance host.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped as expected. Compiled and runtime build numbers
  both report 68149.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; exit 42 is recorded as failed, not promoted to green.
- `cmake --build build-Darwin-ninja-os-arm64-appearance --config
  RelWithDebInfo -j 10` exited 0 with warnings as errors at build 68149.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `67465065ff88a70c05e225d5c0f7c3f79b14b75efb5d0de6ab851aa1853545bf`.
- Appearance-utility SHA-256:
  `514308e32e9afd6e17c72c5545061749b8bc1d8bb35205ccca471a11623c864a`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,259 findings and exits 1 as expected.
  Precommit and source-exact reports are byte-identical at
  `05c8780b14f7e52d6b49565c03076839279d7522bf96977d5e9770c2edb2f44c`.
  The ignored exact report is
  `artifacts/metal/gl-strict-native-tone-curve.json`.
- The packaged viewer and appearance utility have no undefined
  `CGLTexImageIOSurface2D` dependency. `otool -L` still reports both Apple
  OpenGL and Metal in each binary. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Use the pipeline BRDF LUT as the next bounded duplicate owner. Source inspection
confirms `ALRenderResourcePlan` unconditionally requests a 512x512 GL RG16F
target on Darwin, `LLPipeline::createBrdfLut` rasterizes it with the retained GL
shader, and retained GL passes bind it. The native world renderer and native
PBR preview already allocate, compute and consume their own Metal RG16F tables.
Remove the redundant Darwin pipeline target/generator without weakening native
readiness or failure handling, preserve non-Darwin GL behavior and make any
retained Darwin GL caller safe when no pipeline table exists.
