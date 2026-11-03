# Native color-grading LUT GL-owner removal checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `3269c260f7` (`Retire GL color grading volume owner`)

## Replacement-first boundary

This commit removes only the residual OpenGL color-grading volume owner after
the supported Darwin product already had a validated native Metal resource and
consumer. It does not remove color grading.

The retained feature path includes:

- `.cube` LUT parsing and 16-bit volume decoding;
- 8-bit image-strip LUT decoding and component validation;
- the shared grade strength, bypass/profile and volume-size parameters;
- Metal volume-array upload through `alMetalCreateUnormVolumeArray`; and
- native display-post resource configuration and LUT sampling.

The deleted `ALTexture3D` source/header implemented the obsolete GL 3D-texture
allocation, image upload and mip-generation route. Its `ALImageSlot` and
`LLGLSLShader` binding overloads and associated GL-only test block were unused
by the supported product. `LLPipeline` no longer carries `mCGLut` or publishes
the decoded data to GL, but continues to own `mMetalCGLut` and the
backend-neutral LUT-size parameter.

The 11-file change adds 15 lines and deletes 330, including the 216-line GL
texture unit. Source inspection finds no `ALTexture3D`, `mCGLut` or
`altexture3d` reference other than the backend-neutral `mCGLutSize`. The native
tests retain exact coverage for precision-preserving volume upload/RGB
expansion and real display-post LUT sampling/configuration.

## Exact build and test evidence

- `tools/metal/doctor.sh` exited 0 on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- The first `tools/metal/build.sh` invocation exited 65 because the existing
  generated Xcode project tried to scan dependencies for deleted
  `indra/llrender/altexture3d.cpp` before its `ZERO_CHECK` regeneration
  completed. The invocation regenerated the project; this was a stale generated
  graph transition, not a source compile failure.
- The second exact `tools/metal/build.sh` invocation exited 0 at committed
  source `3269c260f7`. The warnings-as-errors Xcode Release build reported
  `BUILD SUCCEEDED` and staged Alchemy Metal Development `26.4.0d68223`
  (`26.4.0.68223`).
- `tools/metal/verify_app.sh` exited 0 after checking 349/349 bundled Mach-O
  files as ARM64; deep strict code-sign verification exited 0.
- Viewer executable SHA-256:
  `7b57f992a4adcf1dcb14eb8dcbc6c5e2738c72f16ff2e4a20cc246fa5b8dce18`.
- Source and packaged MSL are byte-identical at
  `6fcd3271ea0fed0a7dde4fec6e66ce71a8ec3fd1b43d4637f850ac8717af8110`;
  packaged metallib SHA-256 is
  `024aba5a70e01ce501c884535ad53f438d626d756c7c2148ee08a1a7a2aacc2d`.
- The exact Ninja warnings-as-errors tree reconfigured to build 68223 and
  revision `3269c260f7`, then linked `almetal_tests`,
  `llrender_gl_fresh_tests`, `llrender_gl_shared_tests`,
  `llappearance_gl_tests`, `alrender_tests`, `newview_tests` and
  `alchemy-bin`.
- With Metal API/GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,468 assertions across 55 cases;
  `llrender_gl_fresh_tests` passed 2,612/96;
  `llappearance_gl_tests` passed 13,207/3; and
  `alrender_tests` passed 557,312/137.
- `newview_tests` self-reported build 68223 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- The complete shared-state diagnostic suite remains transparently non-green
  at 8,352/8,357 assertions, 182 passed, 4 failed and 39 skipped across 225
  cases. Its four legacy/headless frame-fence timing assertions and one
  depth-format expectation reproduce the established failure signature; there
  is no new failure signature.
- `python3 tools/metal/validate_harness.py` passed all 32 required files, and
  `python3 -m unittest discover -s tools/metal/tests -v` passed all 27 tests.

## Strict inventory result

- `python3 tools/metal/audit_gl.py --mode strict --output
  artifacts/metal/gl-audit-native-color-lut-owner-strict.json` exits 1 while
  other application-owned GL remains. The exact report is 2,406 files / 3,231
  findings / 2,920 unique path/kinds/snippet records.
- This is two files, ten findings and ten unique records below the preceding
  checkpoint; no audit rule or suppression changed. Report SHA-256:
  `bc06ed5b7171d9ce54b4c68cccb1d8708420b2b8b3f8464403be033b120c3184`.
- The executable still links both OpenGL and Metal. The static, linked and
  runtime no-OpenGL release gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. The
color-grading presentation case remains queued for the owner-attended daily
review. Deterministic GPU evidence is not being misreported as visual parity.

## Next executable action

Census the next live application-owned Darwin GL producer and its full
user-visible responsibilities. Implement and validate equivalent native Metal
behavior before retiring that producer; do not trade functionality for a lower
inventory count.
