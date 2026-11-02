# Metal port execution — Shadow Frusta checkpoint — 2026-11-02

## Completed bounded owner

Commit `c9f600b12fde7f698426a59229c52dd4c593f011` implements native Develop
**Shadow Frusta** replay.

- Capture retains four optional point sets, four exact extent boxes, four exact
  camera outlines and conditional side/near/far volume strips.
- Capture checks the exact RGB-only alpha-blend, no-cull, read-only `LEQUAL`
  depth and point/line-size state, finite data, range arithmetic and allocation.
- Renderer validation enforces cascade order, batch topology, colors, alpha and
  volume presence only for non-empty cascade point sets.
- Native encoding uses points, lines and triangle strips with forward/reverse
  read-only depth and source ordering between reflection-probe output and Wind
  Vectors. Existing MSL functions are reused.
- Only `RENDER_DEBUG_SHADOW_FRUSTA` was removed from the explicit veto; nine
  visual modes and the latent pathfinding guard remain fail-closed.

## Validation evidence

- Focused physical owner case: 3,744 assertions passed.
- RelWithDebInfo, Debug and Release physical Metal suites: 47,512 assertions in
  52 cases each; 2.49, 2.70 and 2.55 seconds warm elapsed.
- Neutral non-manual/non-GL gate: 484/484 in 35.42 seconds.
- Warnings-as-errors production integration and source-exact Release build pass.
- Release Development `26.4.0d68071` at `c9f600b12f`: 127 cacheable tasks.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `a32a43c92176b4873ccec625e951f68079c60eb67aeb49feddf76a29f2428c42`.
- Source/packaged MSL SHA-256:
  `3c918952dd66d3122d683d478020e13d99e0485508e95ce9d0245dc0f13bf393`.
- Metallib SHA-256:
  `5e56cdd100abf8e6c6a40d666a9d80a9bcc5f3ae4b6a204a6f5d0eeba1c156c1`.

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

Implement `RENDER_DEBUG_AVATAR_VOLUME`: retain per-collision-volume world
transforms, end vectors, one and 0.05 sphere scales, ordinary/control-avatar
colors and visible/occluded line-plus-sphere pass ordering. Prove empty controls,
forward/reverse depth and invalid input on the physical M1 Max before removing
only that bit.
