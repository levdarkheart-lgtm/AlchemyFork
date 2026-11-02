# Avatar collision-volume native debug replay checkpoint — 2026-11-02

This archive records the source-exact `RENDER_DEBUG_AVATAR_VOLUME` checkpoint
superseded by `docs/metal/state/CURRENT.md`.

## Observed implementation

Commit `ad3970530d6a3fcf779179f981dfb490c7484985` captures Develop
**Collision Skeleton** only inside the centralized avatar collision-volume
owner. An outer begin/end transaction prevents the shared
`render_sphere_and_line()` calls from bones, model preview or raycast from
entering this owner. It requires exactly two pairs for every traversed collision
volume and rejects partial traversal.

Each pair retains the exact live line MVP, exact post-scale sphere MVP, endpoint,
visible and occluded colors, source scale and traversal order. Capture checks the
live RGB-only color mask, alpha blend, disabled culling, visible `LEQUAL`
writable depth and occluded `GREATER` read-only depth. Non-finite data, count or
state mismatch, invalid scale/color, transform disagreement, order collision or
allocation failure makes the complete frame ineligible.

The renderer builds one immutable LLRenderSphere-compatible 20x30 mesh with 651
float4 vertices and 3,600 uint16 indices. For each pair it draws the visible
line and sphere with writable depth, then the occluded line and sphere with
read-only complementary depth. Reverse-Z uses `GEQUAL` for visible output and
`LESS` for occluded output. Validation requires exact 1.0 shell / 0.05 white-dot
alternation; ordinary avatars use blue/pastel-blue shells and control avatars
use red/pastel-red shells. The owner participates in the established exact
source-order merge with Agent Target, Points, Texture Anim, Update Type, Batch
Size and Lights. Destination alpha is preserved and no new MSL entry point is
required.

Only `RENDER_DEBUG_AVATAR_VOLUME` was removed from the world-output veto. The
remaining eight modes are BBOXES, OCTREE, OCCLUSION, RAYCAST, AVATAR_JOINTS,
PHYSICS_SHAPES, NORMALS and TEXEL_DENSITY. The latent pathfinding guard remains
separately fail-closed.

## Physical and build evidence

- Focused RelWithDebInfo world-owner case: 3,907 assertions passed under Metal
  API and load-action validation.
- Complete RelWithDebInfo, Debug and Release Metal suites: 47,675 assertions in
  52 cases per configuration; warm elapsed times 1.17, 1.40 and 1.19 seconds.
- Coverage includes missing pointers, odd counts, non-finite values, invalid
  scale/color/transform/order, exact shell/dot colors, forward/reverse visible
  writable depth, forward/reverse occluded read-only depth, alpha preservation
  and cross-owner source ordering.
- Warnings-as-errors RelWithDebInfo production integration compiled, linked,
  staged and signed.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure -j 8` passed 484/484 in 41.47 seconds.
- Source-exact `AL_METAL_CONFIG=Release AL_METAL_JOBS=8
  tools/metal/build.sh build` passed at `ad3970530d` with 0 hits across 127
  cacheable tasks and staged Development build `26.4.0d68073`
  (`26.4.0.68073`).
- Deep strict signing passed; bundle inspection found 349 ARM64 Mach-O slices
  and zero invalid slices. The main executable reports arm64.
- `tools/metal/doctor.sh` passed on the physical M1 Max; the 31-file harness and
  all 24 harness/tool unit tests passed.

SHA-256 provenance:

- executable: `f14c2d56eb3e3bb82d055c1fabdce935832c32d606b64bf21be97ef493b3ca15`;
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

Implement `RENDER_DEBUG_AVATAR_JOINTS`. The bounded world call to
`renderBones()` reuses the native line-plus-sphere primitive for its skeleton
loop. Add an owner-specific outer transaction plus the yellow joint-space
rigged bounds and cyan world-space rigged attachment-face bounds, leaving model
preview calls independent, before removing only that veto bit.
