# Metal port execution — Texel Density checkpoint — 2026-11-02

## Completed bounded owner

Commit `b0e0973898da8e3a275b8a29ed72e7ce7f4e9db5` implements native Develop
**Texel Density** replay.

- Current/desired/full retain their exact source texture queries,
  checkerboard binding and texture-matrix side effects.
- The bound production `gDebugProgram` uses only position and current uniform
  color, so native presentation replays the reachable zero-copy indexed
  uniform-color draw rather than an invented sampled-texture path.
- Replay preserves live transforms, arbitrary finite color, u16/u32 indices,
  fill raster, no culling, standard alpha blend, RGB-only writes and writable
  LEQUAL forward/reverse depth.
- HUD faces remain in ordered native indexed UI capture.
- Validation rejects invalid storage, buffers, values, ranges, offsets, index
  width, state/order and every cross-owner collision.
- No reachable render-debug world-geometry bit remains in the explicit visual
  veto. The latent pathfinding guard remains separate and fail-closed.

## Validation evidence

- RelWithDebInfo, Debug and Release physical Metal suites: 51,784 assertions in
  52 cases each under Metal API/load-action validation.
- Neutral non-manual/non-GL gate: 484/484 in 37.42 seconds.
- Warnings-as-errors integration and source-exact Release build pass.
- Release Development `26.4.0d68089` (`26.4.0.68089`) at the full source SHA
  above followed a 161-step wrapper build.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `f5fa05484b57fd77f108838e6d5b82ffde0e8a41697116db742b39b9e9b083f7`.
- Source/packaged MSL SHA-256:
  `73955209f611038a5e4fe0305642614fefee30766dcd26fd03b7df7d3933df20`.
- Metallib SHA-256:
  `fd64dfedceb9056e85090916b7e0dab2d8e17c553cc4ffe63ea1b5e67647c297`.

The executable still links both Metal and OpenGL; this is not a native-only
release claim. Controlled live Texel Density parity remains unclaimed.

## Durable constraints

- Any shadow reference/candidate pair requires shadows enabled in Graphics
  Preferences in both viewers. Off/on is invalid and both-off cannot prove
  parity.
- Exact RLVa parity is deferred M-013 nice-to-have. Exact RLVa tests require
  Preferences enable/save/restart in both viewers; only cutover-minimum behavior
  is mandatory. Remaining exact work stays in
  `docs/metal/work/deferred/rlva-parity.md`.

## Next owner

Move to M-009: refresh strict application-owned GL and presentation-authority
inventory, classify reachable blockers from source, then implement and validate
the highest-value concrete no-GL/parity boundary.
