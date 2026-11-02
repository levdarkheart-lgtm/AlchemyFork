# Raycast native debug replay checkpoint — 2026-11-02

This archive records the source-exact `RENDER_DEBUG_RAYCAST` checkpoint
superseded by `docs/metal/state/CURRENT.md`.

## Observed implementation

Commit `e8f6613c4e2cbbde747d8ddc1d5a832007439565` captures Develop
**Raycast** across the complete world spatial-debug traversal. Every
volume-octree branch intersected by the live cursor ray retains its ordinary
outline plus hidden and visible triangle passes. Immediate source triangles are
expanded to explicit edge pairs for native one-pixel and three-pixel lines.

Records retain exact MVP, center, half extent, palette color, line width,
expanded triangle vertices and global source order. Branch outlines use ordinary
writable depth. Hidden outlines and triangles use complementary read-only depth;
visible triangles use ordinary read-only depth. Selected-object output retains
the three differently sized, oriented and colored filled axis boxes plus the
always-visible cyan bounds. A selected avatar reuses the proven Collision
Skeleton capture. The separate post-deferred particle-hit crosshair is encoded
after reflection-probe diagnostics and before Shadow Frusta, matching source
order.

Validation rejects missing storage, non-finite transform/vertex data, negative
extent, invalid kind/palette/width/range/order, cross-owner order collisions and
allocation failure. Destination alpha remains unchanged. Only
`RENDER_DEBUG_RAYCAST` was removed from the world-output veto; OCTREE,
OCCLUSION, PHYSICS_SHAPES, NORMALS and TEXEL_DENSITY remain fail-closed.

## Physical and build evidence

- Focused RelWithDebInfo world-owner case: 6,385 assertions under Metal API and
  load-action validation.
- Complete RelWithDebInfo, Debug and Release physical Metal suites: 50,153
  assertions in 52 cases per configuration under the same validation layers;
  offline MSL compiled in all three.
- Coverage includes invalid storage/data/extent/palette/kind/range/order,
  ordinary/hidden/visible branch and triangle behavior, width growth, oriented
  axis boxes, always-visible read-only hit bounds, particle crosshair,
  forward/reverse writable/complementary/read-only depth, destination-alpha
  preservation, Raycast-vs-Points order and no-owner identity.
- Warnings-as-errors RelWithDebInfo production integration compiled, linked,
  staged and signed after a 127-step rebuild.
- Neutral non-manual/non-GL gate: 484/484 in 39.45 seconds.
- The preceding unfiltered 574-test inventory had six reproducible legacy
  GL-only failures (three primary and three aggregate entries) and five skips;
  it was not rerun or represented as a green gate for this bounded owner.
- Source-exact Release wrapper passed at `e8f6613c4e` after a 161-step rebuild
  and staged Development `26.4.0d68079` (`26.4.0.68079`). Generated build state
  embeds the full renderer commit.
- Deep strict signing passed; bundle inspection found 349 ARM64 Mach-O slices
  and zero invalid slices. Source and packaged MSL compare byte-identically.
- `tools/metal/doctor.sh` confirmed macOS 26.5.2, M1 Max, 64 GiB unified memory,
  Xcode 26.6 and Apple7-family Metal support.

SHA-256 provenance:

- executable: `f113ecab21125f63f88c2f3185038eb123ced6b0a48e1f258f73168259f8ec5e`;
- source and packaged MSL: `12989bd3fcbce372c0d28a680d93afbe111994cfa871ddb83df2bccd3f54e14e`;
- compiled metallib: `c6f84163e2367f9dac55b55504773528360fedc38174516e8c8b0957ca14d5d5`.

The main executable still links Metal and OpenGL. Strict no-GL, broad live
parity, stability and qualifying performance gates remain open. No controlled
live reference/candidate Raycast comparison is claimed by this checkpoint.

## Persisted owner decisions

- Exact RLVa parity is deferred non-release nice-to-have M-013. Exact RLVa
  comparisons require enabling RLV in Preferences in both viewers, saving and
  restarting both. Mandatory work is only the minimum needed for Metal/no-GL
  cutover; the durable inventory is `docs/metal/work/deferred/rlva-parity.md`.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers before capture. Off/on is invalid; both-off is diagnostic only and
  cannot close shadow parity.

## Next executable action

Implement `RENDER_DEBUG_OCCLUSION`: retain grouped vertex-buffer x-ray geometry
with complementary read-only line depth and the later selected-group filled
boxes before removing only that veto bit.
