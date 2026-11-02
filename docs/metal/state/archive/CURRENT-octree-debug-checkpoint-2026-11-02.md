# Octree native debug replay checkpoint — 2026-11-02

This archive records the source-exact `RENDER_DEBUG_OCTREE` checkpoint
superseded by `docs/metal/state/CURRENT.md`.

## Observed implementation

Commit `2403e40ce39532262f34947ec29a124865e1556d` captures Develop
**Octree** across the source post-order traversal. It publishes the five-pixel
red bounds for groups whose rebuild counter just decremented, each recently
updated or moved face immediately before its draw, and the final one-pixel cyan
group bounds. Capture preserves bridge/region transforms, exact MVP, zero-copy
external vertex/index storage, object-skin palette ownership and one global
source order without changing source rebuild side effects.

Flash face replay supports static and rigged geometry, exact live `mBuilt`
alpha and all four cyan/green/yellow/red source colors. It uses triangle line
fill, additive RGB-only blending, no culling and always-pass read-only depth.
Rebuild bounds use a screen-space five-pixel wide-box path with the same blend
and depth contract. Final group bounds use filled box triangles, standard RGB-
only alpha blending, no culling and ordinary writable forward/reverse depth.

Validation rejects missing storage, invalid index enum/ranges/counts/offsets,
non-finite transforms, negative extents, invalid kinds, malformed skin palettes,
internal order errors and collisions with every global ordered owner. Only
`RENDER_DEBUG_OCTREE` was removed from the world-output veto; PHYSICS_SHAPES,
NORMALS and TEXEL_DENSITY remain fail-closed.

## Physical and build evidence

- Complete RelWithDebInfo, Debug and Release physical Metal suites: 51,021
  assertions in 52 cases per configuration under Metal API/load-action
  validation; offline MSL compiled in all three.
- Coverage includes wide rebuild bounds, static/rigged flash geometry, four
  source colors, forward/reverse no-depth behavior, writable group bounds,
  destination-alpha preservation, negative validation and an actual Octree-vs-
  Occlusion order-dependent pixel distinction.
- Neutral non-manual/non-GL gate: 484/484 in 37.44 seconds.
- Warnings-as-errors production integration passed. Source-exact Release passed
  at `2403e40ce39532262f34947ec29a124865e1556d` after 161 steps and
  staged Development `26.4.0d68083` (`26.4.0.68083`).
- Deep strict signing passed; bundle inspection found 349 ARM64 Mach-O slices
  and zero invalid slices. Source and packaged MSL compare byte-identically.
- `tools/metal/doctor.sh` confirmed macOS 26.5.2, M1 Max, 64 GiB unified memory,
  Xcode 26.6 and Apple7-family Metal support.

SHA-256 provenance:

- executable: `99c5b3141adc32ca7a22c5a6e33c36cd1a72dc4410324f1642f9f328e48fd58f`;
- source and packaged MSL: `12989bd3fcbce372c0d28a680d93afbe111994cfa871ddb83df2bccd3f54e14e`;
- compiled metallib: `c6f84163e2367f9dac55b55504773528360fedc38174516e8c8b0957ca14d5d5`.

The main executable still links Metal and OpenGL. Strict no-GL, broad live
parity, stability and qualifying performance gates remain open. No controlled
live reference/candidate Octree comparison is claimed by this checkpoint.

## Persisted owner decisions

- Exact RLVa parity is deferred non-release nice-to-have M-013. Exact RLVa
  comparisons require enabling RLV in Preferences in both viewers, saving and
  restarting both. Mandatory work is only the minimum needed for Metal/no-GL
  cutover; the durable inventory is `docs/metal/work/deferred/rlva-parity.md`.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers before capture. Off/on is invalid; both-off is diagnostic only and
  cannot close shadow parity.

## Next executable action

Audit `RENDER_DEBUG_PHYSICS_SHAPES` as the clearest remaining source owner.
Retain every shape family, hull-build side effect, bridge transform, wireframe
variant, color and render state as a coherent unit.
