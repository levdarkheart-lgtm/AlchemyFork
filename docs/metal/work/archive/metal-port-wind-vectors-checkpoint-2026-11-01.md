# Metal-port execution archive — native Wind Vectors checkpoint

**Date:** 2026-11-01  
**Branch:** `codex/apple-silicon-metal`  
**Code commit:** `aba5995eac` (`feat(metal): replay wind vectors natively`)

## Implemented boundary

- Added bounded Wind capture at `LLWind::renderVectors()` after the live
  region/agent-altitude transform.
- Captured the exact 16x16 lattice as 256 one-pixel green points and 256 red
  velocity line segments using the original simulator data and scale.
- Extended the native world scene with packed point/line storage, one camera
  transform and fail-closed capture lifecycle handling.
- Added dedicated LDR/HDR RGB-only point/line pipelines and a built-in vertex
  shader using the established GL-to-Metal clip conversion.
- Preserved opaque-post order after reflection-probe volumes and before the
  far-plane overlay, with forward/reverse-Z depth reads and writes.
- Rejected missing storage, invalid cardinality, overflow, non-finite data and
  allocation failure; retained empty/no-owner identity.
- Removed only `RENDER_DEBUG_WIND_VECTORS` from the visual-output eligibility
  mask. Nineteen other visual modes and the latent pathfinding guard remain
  fail-closed.

## Validation

- Focused RelWithDebInfo physical Wind/world case under Metal API/GPU/load-
  store/shader validation: 1,559 assertions, pass.
- Full RelWithDebInfo, Debug and Release physical suites under the same layers:
  45,215 assertions in 52 cases each, pass.
- Warnings-as-errors RelWithDebInfo `alchemy-bin`: compile, link, stage and sign
  pass.
- Neutral gate: 484/484 pass in 41.46 seconds.
- Source-exact Release Development `26.4.0d68051` at `aba5995eac`: pass.
- Deep strict signing: pass. Bundle inspection: 349 ARM64 Mach-O slices, zero
  invalid slices; main executable arm64.

Provenance hashes:

- executable `9f75bdaf63e35aa5dbc63e927634aa466f841e81292a94b404c50e072d32344f`;
- source/packaged MSL `a33ec7160965a1c6805ad5b2a2f69d95da8bff630cf2b05c346a57deb13e2033`;
- metallib `537685a0c488cded37fc64b7a532f57610771deeaf9f17d6ae9c1772787549b1`.

## Persisted decisions

- RLVa strict parity remains deferred nice-to-have M-013. Exact comparison
  requires enabling RLV in both viewers, saving and restarting both. Minimum
  behavior still must not block Metal/no-GL cutover.
- Shadow pairs require shadows enabled in Graphics Preferences in both viewers.
  Both-off is only an isolation control, not parity evidence.
- Build/test success does not close broader live visual parity, strict no-GL,
  stability or performance gates.

## Next executable action

Implement Develop **Agent Target** as four native fixed-size colored crosshairs
for the self avatar. Prove self-only ownership, transform, color, size,
depth/order and empty/off controls before removing only
`RENDER_DEBUG_AGENT_TARGET`; keep the other 18 visual modes and pathfinding
fail-closed.
