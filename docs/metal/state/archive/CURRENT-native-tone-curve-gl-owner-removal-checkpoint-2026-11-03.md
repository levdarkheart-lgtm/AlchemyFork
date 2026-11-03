# Native tone-curve GL-owner removal checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `7b7d772256` (`Retire GL tone curve owner`)

## Replacement-first boundary

This commit removes only the residual OpenGL tone-curve owner after the
supported Darwin product already had a validated native Metal resource and
consumer. It does not remove tone-curve grading.

The retained feature path includes:

- the curve editor/model and master, red, green and blue controls;
- settings, identity/bypass/amount semantics and CPU curve bake;
- the production 512x1 RGBA16 data representation;
- Metal publication through `alMetalCreateUnormTexture2D` as
  `mMetalToneCurveLut`; and
- native display-post resource configuration and per-channel sampling.

The deleted boundary is the duplicate GL `ALRenderResourcePlan::ToneCurve`
entry, raw `mToneCurveLut` name, allocation/upload/binding route, GLSL sampler
and `applyChannelCurves` function, and reserved shader-uniform slots. On an
unsupported source configuration the bake now fails closed rather than
silently recovering through GL; the shipping fork remains macOS/ARM64-only.

The 11-file change adds 35 lines and deletes 163. Source inspection finds no
residual `mToneCurveLut`, `Id::ToneCurve`, `uToneCurve*`,
`COLOR_GRADE_CURVE*`, `applyChannelCurves` or `tone.curve` owner. Native tests
retain production-sized upload/readback, exact per-channel display-post
sampling and invalid-configuration rejection.

## Exact build and test evidence

- `tools/metal/doctor.sh` exited 0 on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- The exact `tools/metal/build.sh` invocation exited 0 at committed source
  `7b7d772256`. The warnings-as-errors Xcode Release build reported `BUILD
  SUCCEEDED` and staged Alchemy Metal Development `26.4.0d68225`
  (`26.4.0.68225`).
- `tools/metal/verify_app.sh` exited 0 after checking 349/349 bundled Mach-O
  files as ARM64; deep strict code-sign verification exited 0.
- Viewer executable SHA-256:
  `67858ab25cf0d283e125689ecd0cb72658b55152b0ada32b1318f3424fea3ac9`.
- Source and packaged MSL are byte-identical at
  `6fcd3271ea0fed0a7dde4fec6e66ce71a8ec3fd1b43d4637f850ac8717af8110`;
  packaged metallib SHA-256 is
  `024aba5a70e01ce501c884535ad53f438d626d756c7c2148ee08a1a7a2aacc2d`.
- The exact Ninja warnings-as-errors tree reconfigured to build 68225 and
  revision `7b7d772256`, then linked `almetal_tests`,
  `llrender_gl_fresh_tests`, `llrender_gl_shared_tests`,
  `llappearance_gl_tests`, `alrender_tests`, `newview_tests` and
  `alchemy-bin`.
- With Metal API/GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,468 assertions across 55 cases;
  `llrender_gl_fresh_tests` passed 2,612/96;
  `llappearance_gl_tests` passed 13,207/3; and
  `alrender_tests` passed 557,308/137.
- `newview_tests` self-reported build 68225 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- The complete shared-state diagnostic suite remains transparently non-green
  at 8,352/8,357 assertions, 182 passed, 4 failed and 39 skipped across 225
  cases. Its four legacy/headless frame-fence timing assertions and one
  depth-format expectation reproduce the established failure signature; there
  is no new failure signature.
- `python3 tools/metal/validate_harness.py` passes all 32 required files, and
  `python3 -m unittest discover -s tools/metal/tests -v` passes all 27 tests.

## Strict inventory result

- `python3 tools/metal/audit_gl.py --mode strict --output
  artifacts/metal/gl-audit-native-tone-curve-owner-strict.json` exits 1 while
  other application-owned GL remains. The exact report is 2,406 files / 3,224
  findings / 2,913 unique path/kinds/snippet records.
- This is seven findings and seven unique records below the preceding
  checkpoint; no audit rule or suppression changed. Report SHA-256:
  `5380797ccdd22f938b2969bb6b5d6c74dcfa49c78373e2a472b26f820e81fad4`.
- The executable still links both OpenGL and Metal. The static, linked and
  runtime no-OpenGL release gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. The
tone-curve presentation case remains queued for the owner-attended daily
review. Deterministic GPU evidence is not being misreported as visual parity.

## Next executable action

Census the next live application-owned Darwin GL producer and its full
user-visible responsibilities. Implement and validate equivalent native Metal
behavior before retiring that producer; do not trade functionality for a lower
inventory count.
