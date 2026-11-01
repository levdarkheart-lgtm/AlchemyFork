# Native world wireframe checkpoint — 2026-11-01

## Observed result

Commit `23ce2f7190` (`feat(metal): replay world wireframe natively`) replaces the
reachable global `gUseWireframe` native-presentation veto with direct Metal
triangle-line raster ownership. Main-view and reflection-probe captures retain
the live flag. Impostor bake remains explicitly filled.

Line mode covers deferred G-buffer draw-pool geometry, HDRI and WindLight sky
geometry, stars, heavenly bodies, meteors, aurora, clouds, water surfaces and
patches, forward alpha, glow, opaque and fullbright geometry. Coverage seed,
full-screen water/atmospheric haze, water exclusion, Highlight Transparent,
object-update blips, reflection-probe-volume composite and far-plane overlay
explicitly use fill. This matches the observable GL fill-mode ownership and
prevents state leakage into later diagnostics.

GL clears the wireframe G-buffer to raw 0.5 before enabling framebuffer sRGB.
Metal render-pass clears follow the sRGB attachment transfer function, so the
albedo attachment uses the linear decode of the selected UNORM8 byte 128/255,
`0.21586050011389926`; the linear attachments use 0.5. The initial decode of
mathematical 0.5 physically stored byte 127 and was rejected. The corrected
path stores byte 128 exactly.

The only remaining reachable advanced-output eligibility veto is general
render-debug output. The pathfinding veto remains latent because the linked OSS
implementation is the nonfunctional `llphysicsextensionsos` stub.

## Physical validation

On the Apple M1 Max acceptance host with `MTL_DEBUG_LAYER=1`,
`MTL_SHADER_VALIDATION=1`, load/store validation enabled and validation errors
reported through `nslog`:

- Focused RelWithDebInfo G-buffer case passed 1,491 assertions. It proves exact
  byte-128 wireframe clear, line-only geometry coverage, reduced coverage versus
  fill and untouched clear depth.
- Focused RelWithDebInfo deferred/sky case passed 1,927 assertions. It proves
  post-deferred line geometry, fill restoration for Highlight Transparent and
  line-rendered HDRI sky with untouched clear-depth interior.
- Full RelWithDebInfo, Debug and Release Metal suites each passed 45,147
  assertions across 52 cases.
- Warnings-as-errors RelWithDebInfo `alchemy-bin` compiled, linked, staged and
  signed.
- The neutral gate passed 484/484 in 35.58 seconds:
  `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure -j 8`.
- `tools/metal/doctor.sh` passed and confirmed Apple M1 Max, unified memory and
  Apple7-family Metal support.

The committed-head source-exact command
`env AL_METAL_CONFIG=Release AL_METAL_JOBS=8 tools/metal/build.sh build`
produced Development `26.4.0d68047` and exited zero. Deep strict signing passed.
`tools/metal/verify_app.sh` inspected 349 Mach-O slices with no non-ARM64 slice;
the main executable reports arm64.

SHA-256 provenance:

- executable:
  `779d8233a2d62c05de54fe527ec70143ec20d36b6c09f8d6ab75bfc3baf89b18`
- source and packaged `almetal_builtin.metal`:
  `dd51886cc8cc33a8b19ac69206fed5045421c8905bb51c7d596fdeaefb467214`
- packaged `almetal_builtin.metallib`:
  `b5ccb104ec22fade3cf1822595924c751128b6ca14e0ab188276890339357e58`

The executable still links OpenGL during migration. Strict owned-OpenGL
elimination, a controlled live candidate/reference wireframe pair, broader
feature/stability matrices and qualifying paired performance evidence remain
open. This checkpoint is not a native-only or product-complete claim.

## Acceptance prerequisites retained

- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers before capture. An off/on mismatch is invalid; both-off is only an
  isolation control and cannot prove shadow parity.
- Exact RLVa parity is deferred M-013 nice-to-have. An exact comparison requires
  enabling RLV in Preferences in both viewers, saving and restarting both.
  Implement only enough RLVa to unblock Metal authority and no-GL cutover.

## Next executable boundary

Trace the production contribution of general render-debug output. Port the
smallest confirmed reachable missing owner with asymmetric physical proof and
leave all uncovered owners fail-closed.
