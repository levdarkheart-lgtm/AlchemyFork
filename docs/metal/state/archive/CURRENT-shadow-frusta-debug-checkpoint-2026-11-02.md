# Shadow Frusta native debug replay checkpoint — 2026-11-02

This archive records the source-exact `RENDER_DEBUG_SHADOW_FRUSTA` checkpoint
superseded by `docs/metal/state/CURRENT.md`.

## Observed implementation

Commit `c9f600b12fde7f698426a59229c52dd4c593f011` captures the centralized
Shadow Frusta source block while its exact state is live. The capture retains:

- four optional cascade point sets;
- one 24-vertex extent box and one 24-vertex camera outline per cascade;
- conditional 10-vertex side, 4-vertex near and 4-vertex far volume strips for
  each non-empty cascade;
- exact cascade order, red/green/blue/magenta colors, opaque point/line alpha
  and alpha-0.1 volume colors;
- the source RGB-only alpha blend, no-cull, one-pixel point/line and read-only
  `LEQUAL` depth contract.

Invalid state, non-finite data, storage/topology mismatch, overflow or allocation
failure makes the frame ineligible. The native renderer requires the exact batch
grammar and conditional volume presence. It renders eight-pixel cascade points,
lines and triangle strips with forward/reverse read-only depth after reflection-
probe debug output and before Wind Vectors. Destination alpha is preserved. The
implementation reuses existing offline-compiled MSL entry points.

Only `RENDER_DEBUG_SHADOW_FRUSTA` was removed from the world-output veto. The
remaining nine modes are BBOXES, OCTREE, OCCLUSION, RAYCAST, AVATAR_VOLUME,
AVATAR_JOINTS, PHYSICS_SHAPES, NORMALS and TEXEL_DENSITY. The latent pathfinding
guard remains separately fail-closed.

## Physical and build evidence

- Focused RelWithDebInfo world-owner case: 3,744 assertions passed under Metal
  API and load-action validation.
- Complete RelWithDebInfo, Debug and Release Metal suites: 47,512 assertions in
  52 cases per configuration; warm elapsed times 2.49, 2.70 and 2.55 seconds.
- Coverage includes exact topology/color/alpha, conditional volumes, forward and
  reverse read-only depth, target-alpha preservation, no-owner color/depth
  identity, invalid storage/data/topology rejection and explicit shadow-before-
  wind ordering.
- Warnings-as-errors RelWithDebInfo production integration compiled, linked,
  staged and signed.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure -j 8` passed 484/484 in 35.42 seconds.
- Source-exact `AL_METAL_CONFIG=Release AL_METAL_JOBS=8
  tools/metal/build.sh build` passed at `c9f600b12f` after 127 cacheable tasks and
  staged Development build `26.4.0d68071`.
- Deep strict signing passed; bundle inspection found 349 ARM64 Mach-O slices
  and zero invalid slices. The main executable reports arm64.

SHA-256 provenance:

- executable: `a32a43c92176b4873ccec625e951f68079c60eb67aeb49feddf76a29f2428c42`;
- source and packaged MSL: `3c918952dd66d3122d683d478020e13d99e0485508e95ce9d0245dc0f13bf393`;
- compiled metallib: `5e56cdd100abf8e6c6a40d666a9d80a9bcc5f3ae4b6a204a6f5d0eeba1c156c1`.

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

Implement `RENDER_DEBUG_AVATAR_VOLUME`. The bounded source owner is a finite
per-avatar collision-volume loop: two line-plus-sphere pairs share each joint
world matrix and switch between visible `LEQUAL` and occluded `GREATER` depth
passes. Preserve ordinary/control-avatar colors, sphere scales, source order and
fail-closed validation before removing only that bit.
