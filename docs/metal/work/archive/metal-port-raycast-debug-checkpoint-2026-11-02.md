# Metal port execution — Raycast checkpoint — 2026-11-02

## Completed bounded owner

Commit `e8f6613c4e2cbbde747d8ddc1d5a832007439565` implements native Develop
**Raycast** replay.

- Capture spans the exact world spatial-debug traversal and retains every live
  cursor-ray volume-octree branch, not only the final selected object.
- Branch outline records preserve exact MVP, bounds, source palette and order.
- Source immediate triangles become explicit edge pairs for the hidden
  one-pixel and visible three-pixel passes.
- Native depth state preserves ordinary writable branch output, complementary
  read-only hidden output and ordinary read-only visible triangles in forward
  and reverse Z.
- Three oriented filled hit-axis boxes and always-visible read-only object bounds
  preserve the final selected-object annotation.
- Selected-avatar output reuses the proven Collision Skeleton owner. The later
  particle crosshair remains after reflection-probe diagnostics in source order.
- Validation rejects invalid storage/data/extent/kind/palette/width/range/order
  and allocation failure; the global render-debug merge includes Raycast.
- Only `RENDER_DEBUG_RAYCAST` was removed; five visual modes and the latent
  pathfinding guard remain fail-closed.

## Validation evidence

- Focused RelWithDebInfo owner case: 6,385 assertions.
- RelWithDebInfo, Debug and Release physical Metal suites: 50,153 assertions in
  52 cases each under Metal API/load-action validation.
- Neutral non-manual/non-GL gate: 484/484 in 39.45 seconds.
- Warnings-as-errors production integration and source-exact Release build pass.
- Release Development `26.4.0d68079` (`26.4.0.68079`) at `e8f6613c4e`
  followed a 161-step wrapper rebuild.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `f113ecab21125f63f88c2f3185038eb123ced6b0a48e1f258f73168259f8ec5e`.
- Source/packaged MSL SHA-256:
  `12989bd3fcbce372c0d28a680d93afbe111994cfa871ddb83df2bccd3f54e14e`.
- Metallib SHA-256:
  `c6f84163e2367f9dac55b55504773528360fedc38174516e8c8b0957ca14d5d5`.
- Physical M1 Max doctor passed; documentation validation records the harness
  and tooling-test result.

The main executable still links both Metal and OpenGL; this checkpoint is not a
native-only release claim. Controlled live Raycast parity remains unclaimed.

## Durable constraints

- Any shadow reference/candidate pair requires shadows enabled in Graphics
  Preferences in both viewers. Off/on is invalid and both-off cannot prove
  parity.
- Exact RLVa parity is deferred M-013 nice-to-have. Exact RLVa tests require
  Preferences enable/save/restart in both viewers; only cutover-minimum behavior
  is mandatory. Remaining exact work stays in
  `docs/metal/work/deferred/rlva-parity.md`.

## Next owner

Implement `RENDER_DEBUG_OCCLUSION`: capture grouped x-ray vertex buffers and
selected-group filled boxes with exact transforms, depth, bias, cull, blend and
order, then prove them before removing only that veto bit.
