# Native reflection-probe volume checkpoint — 2026-11-01

**Observed:** physical Apple M1 Max acceptance host.
**Code:** `28c4dca74be2084313f966d6fc8ed2804196fcdb`.

## State established

Persisted `RenderReflectionProbeVolumes` no longer vetoes exact native world/UI
publication. The viewer snapshots the live manual-probe box/sphere/index pack
and current inverse projection at the authoritative full-screen GL draw. The
native pass reconstructs eye-space position from retained scene depth under
forward or reverse Z, then applies the source GLSL sphere/box intersection and
yellow/hidden-blue accumulation rules.

The overlay is source-alpha blended into RGB only, so destination alpha and
scene depth remain unchanged. HDR and non-HDR output are supported. Automatic
probes are ignored. Invalid counts, nonfinite consumed inputs, negative sphere
radii, unreadable depth, resource failure or duplicate capture rejects native
authority while the migration GL owner remains intact.

General render-debug and wireframe output remain reachable fail-closed owners.
The current OSS pathfinding implementation is a nonfunctional stub, but its
latent eligibility guard remains. The executable still links OpenGL; this is an
advanced-output checkpoint, not completion of parity or native-only release
gates.

## Evidence

- Focused physical replay: 1,470 assertions with Metal API/GPU/load-store/
  shader validation.
- Complete RelWithDebInfo, Debug and Release Metal suites: 45,118 assertions in
  52 cases per configuration under the same validators.
- Neutral repository gate: 484/484 tests in 40.46 seconds.
- Source-exact Release Development `26.4.0d68044` at `28c4dca74b`: warnings-as-
  errors build, link, stage and sign pass.
- Deep strict signing and 349/349 ARM64 Mach-O inspection pass; main executable
  is arm64.
- Executable SHA-256:
  `4306dea5dedc006eab0017f40404985d323f224d0b40c864b54bc61f2fcc45e0`.
- Source/packaged MSL SHA-256:
  `dd51886cc8cc33a8b19ac69206fed5045421c8905bb51c7d596fdeaefb467214`.
- Metallib SHA-256:
  `b5ccb104ec22fade3cf1822595924c751128b6ca14e0ab188276890339357e58`.

## Durable comparison prerequisites

- Any exact RLVa comparison must enable RLV in Preferences in both viewers,
  save and restart both. Exact RLVa parity is deferred M-013 nice-to-have; only
  minimum cutover support is release-critical.
- Any shadow parity comparison must enable shadows in Graphics Preferences in
  both viewers before capture. Off/on is invalid and both-off cannot prove
  shadow parity.

## Next boundary

Trace wireframe and general render-debug contribution, then port the smallest
confirmed reachable owner without weakening any remaining eligibility guard.
