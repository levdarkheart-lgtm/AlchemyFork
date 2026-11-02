# Occlusion native debug replay checkpoint — 2026-11-02

This archive records the source-exact `RENDER_DEBUG_OCCLUSION` checkpoint
superseded by `docs/metal/state/CURRENT.md`.

## Observed implementation

Commit `ae691b02da37a8a5181acd1a2c2abfdd19a92dea` captures Develop
**Occlusion** at both exact source phases. Every non-null group vertex buffer is
published immediately before the source x-ray draw, and the later selected-group
loop publishes its filled magenta bounds. Capture uses zero-copy external vertex
and index mirrors and retains exact MVP plus global source order.

X-ray replay preserves indexed triangle line fill, back-face culling, line
polygon offset, standard alpha blending into RGB only, and complementary read-
only depth in forward and reverse Z. Selected bounds use the proven procedural
box pipeline with filled triangles, back-face culling, exact magenta and ordinary
read-only depth. Validation rejects missing storage, invalid index enum/ranges/
counts/offsets, non-finite transforms, negative extents, invalid kinds, internal
order errors and collisions with every global ordered owner. Only
`RENDER_DEBUG_OCCLUSION` was removed from the world-output veto; OCTREE,
PHYSICS_SHAPES, NORMALS and TEXEL_DENSITY remain fail-closed.

## Physical and build evidence

- Complete RelWithDebInfo, Debug and Release physical Metal suites: 50,687
  assertions in 52 cases per configuration under Metal API/load-action
  validation; offline MSL compiled in all three.
- Coverage includes complementary forward/reverse depth, x-ray alpha retention,
  culling, selected ordinary read-only bounds, negative validation, cross-owner
  collision and an actual opposite-order pixel distinction.
- Neutral non-manual/non-GL gate: 484/484 in 37.43 seconds.
- Warnings-as-errors production integration passed. Source-exact Release passed
  at `ae691b02da` after 161 steps and staged Development `26.4.0d68081`
  (`26.4.0.68081`).
- Deep strict signing passed; bundle inspection found 349 ARM64 Mach-O slices
  and zero invalid slices. Source and packaged MSL compare byte-identically.
- `tools/metal/doctor.sh` confirmed macOS 26.5.2, M1 Max, 64 GiB unified memory,
  Xcode 26.6 and Apple7-family Metal support.
- Harness validation passed for all 31 required files; tooling tests passed
  24/24.

SHA-256 provenance:

- executable: `cb22d7020ea6f2663c53d362847c8a7031bbc3487129b8003f06e22907cb351c`;
- source and packaged MSL: `12989bd3fcbce372c0d28a680d93afbe111994cfa871ddb83df2bccd3f54e14e`;
- compiled metallib: `c6f84163e2367f9dac55b55504773528360fedc38174516e8c8b0957ca14d5d5`.

The main executable still links Metal and OpenGL. Strict no-GL, broad live
parity, stability and qualifying performance gates remain open. No controlled
live reference/candidate Occlusion comparison is claimed by this checkpoint.

## Persisted owner decisions

- Exact RLVa parity is deferred non-release nice-to-have M-013. Exact RLVa
  comparisons require enabling RLV in Preferences in both viewers, saving and
  restarting both. Mandatory work is only the minimum needed for Metal/no-GL
  cutover; the durable inventory is `docs/metal/work/deferred/rlva-parity.md`.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers before capture. Off/on is invalid; both-off is diagnostic only and
  cannot close shadow parity.

## Next executable action

Audit and implement `RENDER_DEBUG_OCTREE` as one source-complete owner,
including its time-varying rebuild flash, group bounds, ordinary and rigged face
geometry, matrix palettes, additive blend and wide outlines.
