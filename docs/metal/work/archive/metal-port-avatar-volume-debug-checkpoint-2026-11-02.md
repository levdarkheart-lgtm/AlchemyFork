# Metal port execution — Avatar collision-volume checkpoint — 2026-11-02

## Completed bounded owner

Commit `ad3970530d6a3fcf779179f981dfb490c7484985` implements native Develop
**Collision Skeleton** replay.

- An outer transaction scopes the shared line/sphere helper to collision-volume
  traversal and requires two complete pairs per source volume.
- Capture retains exact live line and scaled-sphere MVPs, endpoint, scale,
  visible/occluded colors and traversal order.
- Capture checks RGB-only alpha blend, no cull, visible `LEQUAL` writable depth
  and occluded `GREATER` read-only depth; invalid or partial input fails closed.
- Renderer validation enforces 1.0 shell / 0.05 white-dot alternation, ordinary
  and control-avatar colors, matching transforms and exact cross-owner order.
- Native replay uses one exact 651-vertex / 3,600-index sphere mesh and draws
  visible then occluded line/sphere output with forward/reverse depth semantics.
- Only `RENDER_DEBUG_AVATAR_VOLUME` was removed from the explicit veto; eight
  visual modes and the latent pathfinding guard remain fail-closed.

## Validation evidence

- Focused physical owner case: 3,907 assertions passed.
- RelWithDebInfo, Debug and Release physical Metal suites: 47,675 assertions in
  52 cases each; 1.17, 1.40 and 1.19 seconds warm elapsed.
- Neutral non-manual/non-GL gate: 484/484 in 41.47 seconds.
- Warnings-as-errors production integration and source-exact Release build pass.
- Release Development `26.4.0d68073` (`26.4.0.68073`) at `ad3970530d`: 0 hits
  across 127 cacheable tasks.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `f14c2d56eb3e3bb82d055c1fabdce935832c32d606b64bf21be97ef493b3ca15`.
- Source/packaged MSL SHA-256:
  `3c918952dd66d3122d683d478020e13d99e0485508e95ce9d0245dc0f13bf393`.
- Metallib SHA-256:
  `5e56cdd100abf8e6c6a40d666a9d80a9bcc5f3ae4b6a204a6f5d0eeba1c156c1`.
- Physical M1 Max doctor, 31-file harness validation and 24 tooling tests pass.

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

Implement `RENDER_DEBUG_AVATAR_JOINTS`. Scope capture only around the spatial
world `renderBones()` call, reuse the exact line/sphere primitive for skeleton
segments, and capture yellow joint-space rigged bounds plus cyan world-space
rigged attachment-face bounds. Keep model-preview invocations independent and
prove forward/reverse depth, ordering and invalid-input rejection before
removing only that bit.
