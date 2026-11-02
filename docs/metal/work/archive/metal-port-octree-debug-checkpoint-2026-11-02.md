# Metal port execution — Octree checkpoint — 2026-11-02

## Completed bounded owner

Commit `2403e40ce39532262f34947ec29a124865e1556d` implements native Develop
**Octree** replay.

- Capture follows exact source post-order and publishes rebuild-flash bounds,
  recently updated/moved face geometry and final group bounds at their separate
  source sites.
- Records retain zero-copy vertex/index mirrors, exact MVP, object-skin palettes
  and global order without changing rebuild counters or GL output.
- Static and rigged flash geometry preserves all four source colors, live alpha,
  additive RGB-only blend, triangle line fill, no culling and no-depth behavior.
- Five-pixel rebuild bounds share the flash contract; final one-pixel cyan group
  bounds use standard RGB-only alpha and ordinary writable depth.
- Validation rejects invalid storage/index/range/count/offset/transform/extent/
  kind/palette/order and global owner collisions.
- Only the Octree veto bit was removed; three visual modes and the latent
  pathfinding guard remain fail-closed.

## Validation evidence

- RelWithDebInfo, Debug and Release physical Metal suites: 51,021 assertions in
  52 cases each under Metal API/load-action validation.
- Neutral non-manual/non-GL gate: 484/484 in 37.44 seconds.
- Warnings-as-errors integration and source-exact Release build pass.
- Release Development `26.4.0d68083` (`26.4.0.68083`) at full SHA
  `2403e40ce39532262f34947ec29a124865e1556d` followed a 161-step wrapper build.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `99c5b3141adc32ca7a22c5a6e33c36cd1a72dc4410324f1642f9f328e48fd58f`.
- Source/packaged MSL SHA-256:
  `12989bd3fcbce372c0d28a680d93afbe111994cfa871ddb83df2bccd3f54e14e`.
- Metallib SHA-256:
  `c6f84163e2367f9dac55b55504773528360fedc38174516e8c8b0957ca14d5d5`.

The executable still links both Metal and OpenGL; this is not a native-only
release claim. Controlled live Octree parity remains unclaimed.

## Durable constraints

- Any shadow reference/candidate pair requires shadows enabled in Graphics
  Preferences in both viewers. Off/on is invalid and both-off cannot prove
  parity.
- Exact RLVa parity is deferred M-013 nice-to-have. Exact RLVa tests require
  Preferences enable/save/restart in both viewers; only cutover-minimum behavior
  is mandatory. Remaining exact work stays in
  `docs/metal/work/deferred/rlva-parity.md`.

## Next owner

Audit `RENDER_DEBUG_PHYSICS_SHAPES` in full before changing its veto. Preserve
box, sphere, cylinder, primitive-mesh and convex-hull production, asynchronous
hull fallback, bridge transforms, wireframe and exact rendering state.
