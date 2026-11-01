# Metal-port execution archive — native drawable-centre Points checkpoint

**Date:** 2026-11-01
**Branch:** `codex/apple-silicon-metal`
**Code commit:** `93dbcc131f` (`feat(metal): replay drawable centre points natively`)

## Implemented boundary

- Added bounded capture around the original `renderPoints(LLDrawable*)` owner.
- Retained one opaque-white one-pixel point per valid face center, with a
  per-batch transform for region partitions and visible spatial bridges.
- Coalesced only consecutive equal-transform batches and retained traversal
  order shared with Agent Target.
- Added dedicated LDR/HDR RGB-only source-alpha point pipelines in the native
  opaque-post pass with always-pass/no-depth-write state.
- Rejected missing/mismatched storage, non-contiguous or overflowing ranges,
  zero/duplicate/non-monotonic batches, Points/Agent Target order collisions,
  non-finite data, allocation failure and inherited-state mismatch; retained
  empty/no-owner identity.
- Removed only `RENDER_DEBUG_POINTS` from the visual-output eligibility mask.
  Seventeen other visual modes and the latent pathfinding guard remain fail-
  closed.

## Validation

- Focused RelWithDebInfo physical Points/world case under Metal API/GPU/load-
  store/shader validation: 2,053 assertions, pass.
- Full RelWithDebInfo, Debug and Release physical suites under the same layers:
  45,709 assertions in 52 cases each, pass.
- Warnings-as-errors RelWithDebInfo `alchemy-bin`: compile, link, stage and sign
  pass.
- Neutral gate: 484/484 pass in 35.42 seconds.
- Source-exact Release Development `26.4.0d68055` at `93dbcc131f`: pass.
- Deep strict signing: pass. Bundle inspection: 349 ARM64 Mach-O slices, zero
  invalid slices; main executable arm64.

Provenance hashes:

- executable `13da93dd728e38b406a3714c040ae32f32547aba875344044916c9f8de79a3ca`;
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

Implement Develop **Composition** as its fixed 261-by-261 current-region point
grid. Prove coordinate bounds, blue/red partitioning, exact height transform,
point size/state and no-region/off controls before removing only
`RENDER_DEBUG_COMPOSITION`; keep the other 16 visual modes and pathfinding
fail-closed.
