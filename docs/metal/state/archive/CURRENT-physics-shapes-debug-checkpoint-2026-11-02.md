# Physics Shapes native debug replay checkpoint — 2026-11-02

This archive records the source-exact `RENDER_DEBUG_PHYSICS_SHAPES` checkpoint
superseded by `docs/metal/state/CURRENT.md`.

## Observed implementation

Commit `5110b2b9fc21e300f7cdf724aa70ceefb8624435` captures Develop
**Physics Shapes** as the source's exact three-pass transaction: depth fill,
translucent color fill and color wire. It covers box, sphere, cylinder,
primitive-volume mesh, decomposition/base hull and generated convex-hull paths,
including bridge/model transforms. Existing asynchronous hull build, fetch and
fallback selection side effects remain unchanged.

Fill replay preserves triangle topology, back-face culling, LEQUAL depth and the
source RGB-only masks. Triangle wire keeps triangle topology and culling,
applies the original polygon offset with a reverse-aware sign and reconstructs
the source three-pixel line width using nine one-pixel screen-space instances.
Cyan, magenta and yellow fallback box outlines use explicit three-pixel wide
lines and remain unaffected by triangle culling, as in the source.

Validation rejects missing storage, non-finite vertices/transforms/colors,
invalid pass/range/topology, incorrect cull/wire/offset state, non-monotonic
order and collisions with every global ordered owner. Only
`RENDER_DEBUG_PHYSICS_SHAPES` was removed from the world-output veto; NORMALS
and TEXEL_DENSITY remain fail-closed.

## Physical and build evidence

- Complete RelWithDebInfo, Debug and Release physical Metal suites: 51,308
  assertions in 52 cases per configuration under Metal API/load-action
  validation; offline MSL compiled in all three.
- Coverage includes depth-only color preservation, complete three-pass output,
  triangle-wire and fallback-wide-line paths, forward/reverse depth,
  destination-alpha preservation, invalid/no-owner inputs and cross-owner order
  rejection.
- Neutral non-manual/non-GL gate: 484/484 in 40.43 seconds.
- Warnings-as-errors integration passed. Source-exact Release passed at
  `5110b2b9fc21e300f7cdf724aa70ceefb8624435` after 161 steps and staged
  Development `26.4.0d68085` (`26.4.0.68085`).
- Deep strict signing passed; bundle inspection found 349 ARM64 Mach-O slices
  and zero invalid slices. Source and packaged MSL compare byte-identically.

SHA-256 provenance:

- executable: `791597f39f48a34c005891fd8216371dddba769993957b434185df48661363f8`;
- source and packaged MSL: `16d904f070eb3000210b7581f42312bc88bfa5f257d6ff1593c13646752f51b9`;
- compiled metallib: `f6266b2331e1a410967fb79902d33446c44647e3d73706f1e6fdf13f69f1b7ae`.

The main executable still links Metal and OpenGL. Strict no-GL, broad live
parity, stability and qualifying performance gates remain open. No controlled
live reference/candidate Physics Shapes comparison is claimed.

## Persisted owner decisions

- Exact RLVa parity is deferred non-release nice-to-have M-013. Exact RLVa
  comparisons require enabling RLV in Preferences in both viewers, saving and
  restarting both. Mandatory work is only the minimum needed for Metal/no-GL
  cutover; the durable inventory is `docs/metal/work/deferred/rlva-parity.md`.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers before capture. Off/on is invalid; both-off is diagnostic only and
  cannot close shadow parity.

## Next executable action

Audit `RENDER_DEBUG_NORMALS` in full before changing its veto. Reconcile the CPU
immediate normal/tangent line path with the drawable-buffer derived path while
preserving exact transforms, filtering, colors and rendering state.
