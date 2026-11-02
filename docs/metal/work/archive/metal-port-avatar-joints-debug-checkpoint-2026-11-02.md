# Metal port execution — Avatar Joints checkpoint — 2026-11-02

## Completed bounded owner

Commit `e84d5e660b23e5d18f444e6d1c2b2b002ffac777` implements native Develop
**Joints** replay.

- An outer transaction scopes the shared line/sphere helper to the spatial-world
  `renderBones()` traversal and leaves model preview independent.
- Segment records retain exact line/sphere MVPs, endpoint, scale, visible and
  occluded colors, and order.
- Replay reuses the exact sphere mesh and paired encoder with forward/reverse
  visible writable and complementary occluded read-only depth.
- Exact hooks capture yellow joint-space rigged bounds and cyan world-space
  attachment-face bounds through the proven outline pipeline.
- Validation rejects invalid storage/count/data/kind/scale/color/extent/order
  and merges the owner into exact global debug source order.
- Only `RENDER_DEBUG_AVATAR_JOINTS` was removed; seven visual modes and the
  latent pathfinding guard remain fail-closed.

## Validation evidence

- Focused physical owner case: 4,339 assertions passed.
- RelWithDebInfo, Debug and Release physical Metal suites: 48,107 assertions in
  52 cases each; 1.21, 1.44 and 1.20 seconds warm elapsed.
- Neutral non-manual/non-GL gate: 484/484 in 35.56 seconds.
- Warnings-as-errors production integration and source-exact Release build pass.
- Release Development `26.4.0d68075` (`26.4.0.68075`) at `e84d5e660b`: 0 hits
  across 127 cacheable tasks.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `6234111e7e6f7a688389cc6e20de3d4ebb60ce6691d224fc0105a76d4e1acb80`.
- Source/packaged MSL SHA-256:
  `3c918952dd66d3122d683d478020e13d99e0485508e95ce9d0245dc0f13bf393`.
- Metallib SHA-256:
  `5e56cdd100abf8e6c6a40d666a9d80a9bcc5f3ae4b6a204a6f5d0eeba1c156c1`.
- Physical M1 Max doctor passed; documentation validation records the harness
  and tooling-test result.

The main executable still links both Metal and OpenGL; this checkpoint is not a
native-only release claim.

## Durable constraints

- Any shadow reference/candidate pair requires shadows enabled in Graphics
  Preferences in both viewers. Off/on is invalid and both-off cannot prove
  parity.
- Exact RLVa parity is deferred M-013 nice-to-have. Exact RLVa tests require
  Preferences enable/save/restart in both viewers; only cutover-minimum behavior
  is mandatory. Remaining exact work stays in
  `docs/metal/work/deferred/rlva-parity.md`.

## Next owner

Implement `RENDER_DEBUG_BBOXES`: retain group/drawable/face bounds, exact color
classes, bridge transforms, active-object line width, depth and source order,
then prove invalid-input and forward/reverse behavior before removing only that
bit.
