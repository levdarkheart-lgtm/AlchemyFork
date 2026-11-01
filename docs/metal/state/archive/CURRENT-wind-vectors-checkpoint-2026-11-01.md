# Native Wind Vectors checkpoint — 2026-11-01

**Observed:** physical Apple M1 Max acceptance host.  
**Branch:** `codex/apple-silicon-metal`  
**Renderer code checkpoint:** `aba5995eac` (`feat(metal): replay wind vectors natively`)  
**Source-exact build:** Release Development `26.4.0d68051`

This archive preserves the bounded Wind Vectors implementation and validation
checkpoint. It does not claim complete advanced-output parity or a Metal-only
release.

## Implementation observation

Develop **Wind Vectors** now has a native Metal owner at the exact
`LLWind::renderVectors()` boundary. The capture starts after the original
region-origin and agent-altitude translation and records the current camera
transform. It preserves the 16x16 simulator lattice, including 256 one-pixel
green sample points and 256 red velocity line segments generated from the live
`mVelX`/`mVelY` values, original cell spacing and `WIND_SCALE_HACK`.

The native opaque-post scene packs point and line vertices into one shared
buffer and selects dedicated LDR/HDR RGB-only pipelines. It encodes after
reflection-probe volume visualization and before the far-plane overlay. The
pass retains depth testing and depth writes for both forward and reverse Z,
matching the inherited GL `LEQUAL`/`GEQUAL` behavior, and does not enable
blending.

Capture and encoder validation are fail-closed. Missing storage, allocation
failure, odd line cardinality, a line-to-point ratio other than 2:1, integer
overflow or non-finite vertex data makes native presentation ineligible. Empty
or absent Wind ownership is an exact no-op.

`RENDER_DEBUG_WIND_VECTORS` was removed from the native-presentation veto only
after the asymmetric evidence below passed. The explicit render-debug world-
output set therefore decreases from 20 to 19 modes. Those 19 modes and the
latent pathfinding guard remain fail-closed.

## Physical validation evidence

All Metal tests ran with Metal API, GPU, load/store-action and shader validation
enabled:

- focused RelWithDebInfo Wind/world test: 1,559 assertions in one case;
- complete RelWithDebInfo suite: 45,215 assertions in 52 cases;
- complete Debug suite: 45,215 assertions in 52 cases;
- complete Release suite: 45,215 assertions in 52 cases.

The focused tests prove invalid/missing-storage and non-finite rejection, exact
one-pixel green point and red line coverage, RGB-only alpha preservation,
forward/reverse-Z depth pass/rejection and writes, and output identity for
empty/no-owner input.

Warnings-as-errors RelWithDebInfo production integration compiled, linked,
staged and signed. The neutral non-manual/non-GL gate passed 484/484 in 41.46
seconds. Source-exact Release wrapper build `26.4.0d68051` completed at committed
code head `aba5995eac`. Deep strict signing passed; `verify_app.sh` inspected 349
Mach-O slices with zero non-ARM64 slices, and the main executable reports arm64.

SHA-256 provenance:

- executable: `9f75bdaf63e35aa5dbc63e927634aa466f841e81292a94b404c50e072d32344f`;
- source and packaged MSL: `a33ec7160965a1c6805ad5b2a2f69d95da8bff630cf2b05c346a57deb13e2033`;
- compiled metallib: `537685a0c488cded37fc64b7a532f57610771deeaf9f17d6ae9c1772787549b1`.

The main executable still links OpenGL during migration. Strict no-GL remains
an open release gate.

## Durable comparison prerequisites

- Any exact RLVa comparison must enable RLV in Preferences in both viewers,
  save, and restart both viewers. Exact RLVa parity is deferred non-release
  nice-to-have M-013; minimum cutover behavior remains required.
- Any shadow candidate/reference comparison must enable shadows in Graphics
  Preferences in both viewers before capture. An off/on pair is invalid, while
  a both-off control can isolate a residual but cannot prove shadow parity.

## Next bounded owner

Source inspection selects Develop **Agent Target**. Its sole production
consumer draws four fixed-size colored crosshairs for the self avatar. It is a
smaller ownership boundary than Raycast, which spans octree,
object/intersection and particle-crosshair output. Remove only
`RENDER_DEBUG_AGENT_TARGET` after physical owner, transform, color, size,
depth/order and off/empty proof; retain the other 18 modes fail-closed.
