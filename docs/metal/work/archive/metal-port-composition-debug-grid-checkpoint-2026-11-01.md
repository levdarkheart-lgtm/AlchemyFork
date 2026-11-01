# Metal-port execution archive — native Composition debug-grid checkpoint

**Date:** 2026-11-01
**Branch:** `codex/apple-silicon-metal`
**Code commit:** `cc1dcff766` (`feat(metal): replay composition debug grid natively`)

## Implemented boundary

- Added bounded capture at the sole current-region Develop Composition owner.
- Retained exactly 68,121 post-transform heights for the fixed 261-by-261 grid.
- Derived X/Y, original loop order and blue-interior/red-border classification
  by Metal vertex ID, reducing the per-frame stream to 272,484 bytes.
- Added dedicated LDR/HDR RGB-only point pipelines after Wind Vectors with
  inherited forward/reverse read/write depth.
- Skipped world capture in the HUD-only pass and rejected wrong cardinality,
  missing storage, non-finite input, allocation failure and inherited-state
  mismatch; retained no-region/off identity.
- Removed only `RENDER_DEBUG_COMPOSITION` from the visual-output eligibility
  mask. Sixteen other visual modes and the latent pathfinding guard remain
  fail-closed.

## Validation

- Focused RelWithDebInfo physical Composition/world case under Metal API/GPU/
  load-store/shader validation: 2,092 assertions, pass.
- Full RelWithDebInfo, Debug and Release physical suites under the same layers:
  45,748 assertions in 52 cases each, pass.
- Warnings-as-errors RelWithDebInfo `alchemy-bin`: compile, link, stage and sign
  pass.
- Neutral gate: 484/484 pass in 35.65 seconds.
- Source-exact Release Development `26.4.0d68057` at `cc1dcff766`: pass.
- Deep strict signing: pass. Bundle inspection: 349 ARM64 Mach-O slices, zero
  invalid slices; main executable arm64.

Provenance hashes:

- executable `db5f7ab0f748a1a9017b843858e169b6ac8c076b07ddc7ad675dcab22bec6c56`;
- source/packaged MSL `d05669c80b490283e9376fb797a48715bb079bc1928022884193130149643a21`;
- metallib `994eaa49f2c573361c5951c443165ce63c6919b44c032fb6d4e027358dbbcd1b`.

## Persisted decisions

- RLVa strict parity remains deferred nice-to-have M-013. Exact comparison
  requires enabling RLV in both viewers, saving and restarting both. Minimum
  behavior still must not block Metal/no-GL cutover.
- Shadow pairs require shadows enabled in Graphics Preferences in both viewers.
  Both-off is only an isolation control, not parity evidence.
- Build/test success does not close broader live visual parity, strict no-GL,
  stability or performance gates.

## Next executable action

Implement Develop **Reflection Probes** neighbor links. Prove relevant-probe
filtering, orange/yellow endpoint classification, per-probe order, transform,
inherited line/depth/blend state and empty/off controls before removing only
`RENDER_DEBUG_REFLECTION_PROBES`; keep the other 15 visual modes and pathfinding
fail-closed.
