# Metal-port execution archive — reflection-probe neighbor links

**Date:** 2026-11-01
**Branch:** `codex/apple-silicon-metal`
**Code commit:** `b76c52576d` (`feat(metal): replay reflection probe neighbor links`)

## Implemented boundary

- Captured the sole active Develop Reflection Probes neighbor-link owner.
- Retained relevant probes and exact per-neighbor orange/yellow viewer-object
  classification in source order; excluded the compiled-out node-volume block.
- Streamed exactly two compact position/color vertices per link and one
  transform with overflow/allocation guards.
- Replayed one-pixel RGB-only links with inherited blend and forward/reverse
  read/write depth after Points/Agent Target and before probe-volume display.
- Rejected invalid counts, storage, homogeneous position, non-finite data and
  colors outside the exact source contract; retained empty/off identity.
- Removed only `RENDER_DEBUG_REFLECTION_PROBES` from the visual-output mask.
  Fifteen visual modes and latent pathfinding remain fail-closed.

## Validation

- Focused physical validator case: 2,162 assertions, pass.
- Full RelWithDebInfo/Debug/Release validator suites: 45,818 assertions in 52
  cases each, pass (5.95/6.22/5.90 seconds).
- Warnings-as-errors RelWithDebInfo production integration: pass.
- Neutral gate: 484/484 pass in 35.39 seconds.
- Source-exact Release Development `26.4.0d68059` at `b76c52576d`: pass.
- Deep strict signing and 349/349 ARM64 bundle inspection: pass.

Provenance hashes:

- executable `8460b7356784d39d0c1163a4b157e9f9bc25faf85a53c31cc84aeba99dec53e1`;
- source/packaged MSL `d05669c80b490283e9376fb797a48715bb079bc1928022884193130149643a21`;
- metallib `994eaa49f2c573361c5951c443165ce63c6919b44c032fb6d4e027358dbbcd1b`.

## Persisted decisions

- RLVa strict parity remains deferred nice-to-have M-013. Exact comparison
  requires Preferences enable/save/restart in both viewers; minimum cutover
  behavior remains required.
- Shadow parity pairs require shadows enabled in Graphics Preferences in both
  viewers; both-off is only an isolation control.
- Build/test success does not close broader live parity, strict no-GL,
  stability or performance gates.

## Next executable action

Implement Develop **Texture Anim** as a bounded flat translucent-yellow replay
of visible draw-info triangles with a non-null texture matrix. Prove static and
rigged geometry, exact color/alpha, ordering/transform, inherited state, depth
interaction and off/empty identity before removing only
`RENDER_DEBUG_TEXTURE_ANIM`; keep the other 14 visual modes and pathfinding
fail-closed.
