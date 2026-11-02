# Metal port execution — Occlusion checkpoint — 2026-11-02

## Completed bounded owner

Commit `ae691b02da37a8a5181acd1a2c2abfdd19a92dea` implements native Develop
**Occlusion** replay.

- Capture publishes each valid group vertex buffer immediately before its source
  x-ray draw and the later selected-group bounds at their separate source site.
- Records retain zero-copy vertex/index mirrors, exact MVP and global order.
- X-ray geometry preserves indexed triangle line fill, back culling, line
  polygon offset, complementary read-only depth and RGB-only alpha blending.
- Selected groups preserve filled magenta boxes, back culling and ordinary
  read-only depth.
- Validation rejects invalid storage/index/range/count/offset/transform/extent/
  kind/order and global owner collisions.
- Only the Occlusion veto bit was removed; four visual modes and the latent
  pathfinding guard remain fail-closed.

## Validation evidence

- RelWithDebInfo, Debug and Release physical Metal suites: 50,687 assertions in
  52 cases each under Metal API/load-action validation.
- Neutral non-manual/non-GL gate: 484/484 in 37.43 seconds.
- Warnings-as-errors integration and source-exact Release build pass.
- Release Development `26.4.0d68081` (`26.4.0.68081`) at `ae691b02da`
  followed a 161-step wrapper rebuild.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `cb22d7020ea6f2663c53d362847c8a7031bbc3487129b8003f06e22907cb351c`.
- Source/packaged MSL SHA-256:
  `12989bd3fcbce372c0d28a680d93afbe111994cfa871ddb83df2bccd3f54e14e`.
- Metallib SHA-256:
  `c6f84163e2367f9dac55b55504773528360fedc38174516e8c8b0957ca14d5d5`.
- Harness validation: 31 required files; tooling tests: 24/24.

The executable still links both Metal and OpenGL; this is not a native-only
release claim. Controlled live Occlusion parity remains unclaimed.

## Durable constraints

- Any shadow reference/candidate pair requires shadows enabled in Graphics
  Preferences in both viewers. Off/on is invalid and both-off cannot prove
  parity.
- Exact RLVa parity is deferred M-013 nice-to-have. Exact RLVa tests require
  Preferences enable/save/restart in both viewers; only cutover-minimum behavior
  is mandatory. Remaining exact work stays in
  `docs/metal/work/deferred/rlva-parity.md`.

## Next owner

Audit and implement `RENDER_DEBUG_OCTREE` with its full flash, group-bounds,
static/rigged geometry, matrix-palette, additive-blend and wide-outline contract
before removing only that veto bit.
