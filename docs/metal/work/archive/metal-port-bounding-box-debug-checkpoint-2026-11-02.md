# Metal port execution — Bounding Boxes checkpoint — 2026-11-02

## Completed bounded owner

Commit `a22cc91fc41bf8e1d3d8405907485c2f8f5514ed` implements native Develop
**Bounding Boxes** replay.

- Capture is scoped to world spatial debug traversal and excludes the shadow
  helper plus HUD/cutout/impostor nonowners.
- Group, face and drawable records retain exact MVP, center, extent, source
  palette, one-to-five-pixel width and global order.
- Thin output reuses the 24-vertex outline; wide output uses a new 72-vertex
  screen-space quad expansion pipeline with forward/reverse writable depth.
- RGB-only output retains destination alpha; no cull and clip-space guards match
  the bounded source requirements.
- Validation rejects invalid storage/data/extent/color/width/order and merges
  BBOX into exact global debug order.
- Only `RENDER_DEBUG_BBOXES` was removed; six visual modes and the latent
  pathfinding guard remain fail-closed.

## Validation evidence

- RelWithDebInfo, Debug and Release physical Metal suites: 49,644 assertions in
  52 cases each.
- Neutral non-manual/non-GL gate: 484/484 in 35.42 seconds.
- Unfiltered inventory: six reproducible GL-only failures of 574 entries and
  five skips; this is not represented as a passing full gate.
- Warnings-as-errors production integration and source-exact Release build pass.
- Release Development `26.4.0d68077` (`26.4.0.68077`) at `a22cc91fc4` followed
  a 1,397-edge rebuild.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `328024070349f48b9cb3b42f1bee2345c54dfd90c84e02d0fa79218bb81b8b37`.
- Source/packaged MSL SHA-256:
  `fa3181abe44fbaec3ffab41248d23e377ec5608faf00188f776f475404ab4d92`.
- Metallib SHA-256:
  `d9370caf0b49a92972fc33b8de8e3f7451d7f21295bd5d2182ce62c0c2185959`.
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

Implement `RENDER_DEBUG_RAYCAST`: retain selected-hit branch boxes and triangle
passes, object/avatar hit markers and particle crosshair, then prove invalid-
input and forward/reverse behavior before removing only that bit.
