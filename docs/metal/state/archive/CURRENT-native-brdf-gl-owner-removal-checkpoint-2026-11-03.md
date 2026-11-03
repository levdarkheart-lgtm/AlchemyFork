# Native PBR BRDF GL-owner removal checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `31eb491c27` (`Retire GL BRDF lookup owner`)

## Replacement-first boundary

This commit removes only the residual OpenGL PBR BRDF lookup owner after the
supported product already had validated native Metal producers and consumers.
It does not remove PBR lighting or its split-sum BRDF integration.

The retained feature path includes:

- the production 1024-sample GGX split-sum integration kernel;
- independent 512x512 RG16F Metal tables for native world and PBR-preview
  renderers;
- bilinear-clamp sampling and the existing NdotV/perceptual-roughness mapping;
- world base, local, projector, pre-water and related PBR consumers;
- the PBR material-preview consumer; and
- command-batch publication, cancellation and retry semantics so a discarded
  generation is never reported ready.

The deleted boundary is the duplicate GL render-plan entry and target,
fullscreen generator program and its two GLSL files, shader reload tracking,
deferred sampler binding/unbinding and reserved uniform. Retained GLSL traversal
continues to fail closed with its same supported-Darwin zero result; it gains no
replacement authority. The real Metal producers and consumers are unchanged.

The 12-file change adds 8 lines and deletes 370. Source inspection finds no
residual `gDeferredGenBrdfLutProgram`, `mPbrBrdfLut`, `Id::BrdfLut`,
`DEFERRED_BRDF_LUT`, `brdfLut` sampler or `genbrdflut` GLSL owner. Native BRDF
symbols remain in the Metal world, preview, MSL and GPU-test sources.

## Exact build and test evidence

- `tools/metal/doctor.sh` exited 0 on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- The exact `tools/metal/build.sh` invocation exited 0 at committed source
  `31eb491c27`. The warnings-as-errors Xcode Release build reported `BUILD
  SUCCEEDED` and staged Alchemy Metal Development `26.4.0d68227`
  (`26.4.0.68227`).
- `tools/metal/verify_app.sh` exited 0 after checking 349/349 bundled Mach-O
  files as ARM64; deep strict code-sign verification exited 0.
- Viewer executable SHA-256:
  `1d55cd4654a54209b6f3fd56ea3c647d007f6091aaa9633f0b8edbc82119973e`.
- Source and packaged MSL are byte-identical at
  `6fcd3271ea0fed0a7dde4fec6e66ce71a8ec3fd1b43d4637f850ac8717af8110`;
  packaged metallib SHA-256 is
  `024aba5a70e01ce501c884535ad53f438d626d756c7c2148ee08a1a7a2aacc2d`.
- The exact Ninja warnings-as-errors tree reconfigured to build 68227 and
  revision `31eb491c27`, then linked `almetal_tests`,
  `llrender_gl_fresh_tests`, `llrender_gl_shared_tests`,
  `llappearance_gl_tests`, `alrender_tests`, `newview_tests` and
  `alchemy-bin`.
- With Metal API/GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,468 assertions across 55 cases;
  `llrender_gl_fresh_tests` passed 2,612/96;
  `llappearance_gl_tests` passed 13,207/3; and
  `alrender_tests` passed 557,301/136.
- The focused native BRDF filter passed 56 assertions. It generates a
  production 512x512 RG16F table and compares six corners/interior samples
  against an independent 1024-sample GGX oracle within half-float tolerance.
  Full Metal coverage also exercises world command batches, discarded
  generation recovery and PBR-preview material semantics.
- `newview_tests` self-reported build 68227 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- The complete shared-state diagnostic suite remains transparently non-green
  at 8,352/8,357 assertions, 182 passed, 4 failed and 39 skipped across 225
  cases. Its four legacy/headless frame-fence timing assertions and one
  depth-format expectation reproduce the established failure signature; there
  is no new failure signature.

## Strict inventory result

- `python3 tools/metal/audit_gl.py --mode strict --output
  artifacts/metal/gl-audit-native-brdf-owner-strict.json` exits 1 while other
  application-owned GL remains. The exact report is 2,406 files / 3,217
  findings / 2,906 unique path/kinds/snippet records.
- This is seven findings and seven unique records below the preceding
  checkpoint; no audit rule or suppression changed. Report SHA-256:
  `cb28d7ae17d4ee6555cf86c9132f189c3bfb221bba03c1fdf1e8cf26705e2938`.
- The executable still links both OpenGL and Metal. The static, linked and
  runtime no-OpenGL release gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. The PBR
BRDF material case remains queued for the owner-attended daily review.
Deterministic GPU evidence is not being misreported as visual parity.

## Next executable action

Census the next live application-owned Darwin GL producer and its full
user-visible responsibilities. Implement and validate equivalent native Metal
behavior before retiring that producer; do not trade functionality for a lower
inventory count.
