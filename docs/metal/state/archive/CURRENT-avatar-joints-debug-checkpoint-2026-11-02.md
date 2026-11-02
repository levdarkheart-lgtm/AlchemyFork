# Avatar Joints native debug replay checkpoint — 2026-11-02

This archive records the source-exact `RENDER_DEBUG_AVATAR_JOINTS` checkpoint
superseded by `docs/metal/state/CURRENT.md`.

## Observed implementation

Commit `e84d5e660b23e5d18f444e6d1c2b2b002ffac777` captures Develop **Joints**
only inside the spatial-world `renderBones()` owner. An outer begin/end
transaction keeps model-preview calls independent and counts every non-null
skeleton joint. The shared `render_sphere_and_line()` capture routes records to
Collision Skeleton or Joints only while the corresponding transaction is active.

Segment records retain exact line and post-scale sphere MVPs, endpoint,
visible/occluded colors, source scale and order. Native replay reuses the exact
LLRenderSphere-compatible mesh and paired visible/occluded encoder. Validation
preserves the ordinary 0.001/gray/red-cyan-green grammar and the source-selected
0.016/yellow-gray grammar. Visible passes write depth; occluded passes use
complementary read-only depth in forward and reverse Z.

Exact source hooks also capture yellow joint-space rigged bounds and cyan world-
space rigged attachment-face bounds. Records retain kind, MVP, center, half
extent and order and replay through the established 24-vertex outline pipeline.
The owner rejects invalid storage, count, finite values, kind, scale, color,
extent, order or allocation and participates in the exact global debug merge.
Destination alpha is preserved and no new MSL entry point was required.

Only `RENDER_DEBUG_AVATAR_JOINTS` was removed from the world-output veto. The
remaining seven modes are BBOXES, OCTREE, OCCLUSION, RAYCAST, PHYSICS_SHAPES,
NORMALS and TEXEL_DENSITY. The latent pathfinding guard remains fail-closed.

## Physical and build evidence

- Focused RelWithDebInfo world-owner case: 4,339 assertions passed under Metal
  API and load-action validation.
- Complete RelWithDebInfo, Debug and Release Metal suites: 48,107 assertions in
  52 cases per configuration; warm elapsed times 1.21, 1.44 and 1.20 seconds.
- Coverage includes null storage, non-finite matrices, invalid scale/color/box
  center/kind, owner-order collision, explicit Joints-vs-Agent-Target pixels,
  independent bounds output, forward/reverse visible writable and occluded
  read-only depth, and alpha preservation.
- Warnings-as-errors RelWithDebInfo production integration compiled, linked,
  staged and signed.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure -j 8` passed 484/484 in 35.56 seconds.
- Source-exact `AL_METAL_CONFIG=Release AL_METAL_JOBS=8
  tools/metal/build.sh build` passed at `e84d5e660b` with 0 hits across 127
  cacheable tasks and staged Development build `26.4.0d68075`
  (`26.4.0.68075`).
- Deep strict signing passed; bundle inspection found 349 ARM64 Mach-O slices
  and zero invalid slices. The main executable reports arm64.
- `tools/metal/doctor.sh` passed on the physical M1 Max with 64 GiB unified
  memory and Apple7-family support. Harness/tool validation is recorded in the
  documentation commit that indexes this archive.

SHA-256 provenance:

- executable: `6234111e7e6f7a688389cc6e20de3d4ebb60ce6691d224fc0105a76d4e1acb80`;
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

Implement `RENDER_DEBUG_BBOXES`. Capture deterministic group, drawable and
per-face extents plus their finite color classes at exact traversal sites,
preserving bridge transforms and active-object line width, before removing only
that veto bit.
