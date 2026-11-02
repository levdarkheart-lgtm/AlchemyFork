# Metal port execution — Physics Shapes checkpoint — 2026-11-02

## Completed bounded owner

Commit `5110b2b9fc21e300f7cdf724aa70ceefb8624435` implements native Develop
**Physics Shapes** replay.

- One exact transaction records depth fill, translucent color fill and color
  wire in source order.
- Box, sphere, cylinder, primitive-volume mesh, decomposition/base hull and
  generated convex-hull paths retain bridge/model transforms and existing hull
  generation/fetch/fallback side effects.
- Fill and triangle-wire records preserve triangle topology, back culling,
  LEQUAL depth, RGB-only masks and reverse-aware polygon offset.
- Nine one-pixel screen-space triangle-wire instances reproduce the source
  three-pixel footprint; fallback boxes retain explicit uncullable three-pixel
  lines and source colors.
- Validation rejects invalid storage/vertices/transforms/colors/pass/range/
  topology/state/order and global owner collisions.
- Only the Physics Shapes veto bit was removed; two visual modes and the latent
  pathfinding guard remain fail-closed.

## Validation evidence

- RelWithDebInfo, Debug and Release physical Metal suites: 51,308 assertions in
  52 cases each under Metal API/load-action validation.
- Neutral non-manual/non-GL gate: 484/484 in 40.43 seconds.
- Warnings-as-errors integration and source-exact Release build pass.
- Release Development `26.4.0d68085` (`26.4.0.68085`) at full SHA
  `5110b2b9fc21e300f7cdf724aa70ceefb8624435` followed a 161-step wrapper build.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `791597f39f48a34c005891fd8216371dddba769993957b434185df48661363f8`.
- Source/packaged MSL SHA-256:
  `16d904f070eb3000210b7581f42312bc88bfa5f257d6ff1593c13646752f51b9`.
- Metallib SHA-256:
  `f6266b2331e1a410967fb79902d33446c44647e3d73706f1e6fdf13f69f1b7ae`.

The executable still links both Metal and OpenGL; this is not a native-only
release claim. Controlled live Physics Shapes parity remains unclaimed.

## Durable constraints

- Any shadow reference/candidate pair requires shadows enabled in Graphics
  Preferences in both viewers. Off/on is invalid and both-off cannot prove
  parity.
- Exact RLVa parity is deferred M-013 nice-to-have. Exact RLVa tests require
  Preferences enable/save/restart in both viewers; only cutover-minimum behavior
  is mandatory. Remaining exact work stays in
  `docs/metal/work/deferred/rlva-parity.md`.

## Next owner

Audit `RENDER_DEBUG_NORMALS` in full before changing its veto. Preserve the CPU
immediate normal/tangent line path, drawable-buffer derived output, transforms,
filters, colors, rendering state and global order as one coherent owner.
