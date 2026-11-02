# Bounding Boxes native debug replay checkpoint — 2026-11-02

This archive records the source-exact `RENDER_DEBUG_BBOXES` checkpoint
superseded by `docs/metal/state/CURRENT.md`.

## Observed implementation

Commit `a22cc91fc41bf8e1d3d8405907485c2f8f5514ed` captures Develop **Bounding
Boxes** only inside spatial-world debug traversal. The transaction excludes the
shadow helper's `set_color=false` calls. HUD attachments use another owner and
impostor/cutout traversal suppresses debug output, so the scoped transaction
covers reachable BBOX presentation.

Records retain live MVP, center, half extent, diffuse color, line width and exact
global order for group, per-face, active and nonactive drawable boxes. Thin
boxes reuse the generated 24-vertex line outline. The one-to-five-pixel active
object pulse uses a new 72-vertex screen-space quad expansion pipeline with no
cull, RGB-only output and forward/reverse writable depth. A finite/zero-`w`
guard prevents invalid clip-space expansion.

Validation rejects missing/null storage, non-finite data, negative extent,
colors outside the exact source palette, invalid width/order and allocation
failure. The global source-order merge includes BBOX. Destination alpha remains
unchanged. Only `RENDER_DEBUG_BBOXES` was removed from the world-output veto;
OCTREE, OCCLUSION, RAYCAST, PHYSICS_SHAPES, NORMALS and TEXEL_DENSITY remain.

## Physical and build evidence

- Complete RelWithDebInfo, Debug and Release physical Metal suites: 49,644
  assertions in 52 cases per configuration under Metal API/load-action
  validation; offline MSL compiled in all three.
- Coverage includes invalid storage/data/extent/palette/width/order, thin and
  wide forward/reverse raster/depth, destination-alpha preservation, width-5
  growth, BBOX-vs-Points order and no-owner identity.
- Warnings-as-errors RelWithDebInfo production integration compiled, linked,
  staged and signed after a 1,097-edge rebuild.
- Neutral non-manual/non-GL gate: 484/484 in 35.42 seconds.
- The unfiltered 574-test inventory had six reproducible legacy GL-only
  failures (three primary and three aggregate entries) and five skips; it is
  not claimed green.
- Source-exact Release build passed at `a22cc91fc4` after a 1,397-edge rebuild
  and staged Development `26.4.0d68077` (`26.4.0.68077`).
- Deep strict signing passed; bundle inspection found 349 ARM64 Mach-O slices
  and zero invalid slices. Source and packaged MSL compare byte-identically.
- Physical M1 Max doctor passed with 64 GiB unified memory and Apple7-family
  support. Harness/tool validation is recorded in the documentation commit that
  indexes this archive.

SHA-256 provenance:

- executable: `328024070349f48b9cb3b42f1bee2345c54dfd90c84e02d0fa79218bb81b8b37`;
- source and packaged MSL: `fa3181abe44fbaec3ffab41248d23e377ec5608faf00188f776f475404ab4d92`;
- compiled metallib: `d9370caf0b49a92972fc33b8de8e3f7451d7f21295bd5d2182ce62c0c2185959`.

The main executable still links Metal and OpenGL. Strict no-GL, broad live
parity, stability and qualifying performance gates remain open.

## Persisted owner decisions

- Exact RLVa parity is deferred non-release nice-to-have M-013. Exact RLVa
  comparisons require enabling RLV in Preferences in both viewers, saving and
  restarting both. Mandatory work is only the minimum needed for Metal/no-GL
  cutover; the durable inventory is `docs/metal/work/deferred/rlva-parity.md`.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers before capture. Off/on is invalid; both-off is diagnostic only and
  cannot close shadow parity.

## Next executable action

Implement `RENDER_DEBUG_RAYCAST`: capture selected-hit volume-octree branches,
triangle passes, hit orientation/bounds, avatar collision output and particle
crosshair before removing only that veto bit.
