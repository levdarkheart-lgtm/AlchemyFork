# Normals native debug replay checkpoint — 2026-11-02

This archive records the source-exact `RENDER_DEBUG_NORMALS` checkpoint
superseded by `docs/metal/state/CURRENT.md`.

## Observed implementation

Commit `08973b83dccad57d39fa0386c4f8463385761d2a` captures and replays both
reachable Develop **Normals** source paths. `LLVOVolume` faces retain the exact
CPU construction of yellow normal and cyan tangent line pairs, including the
inverse-scale normalization rule, per-face transform and source draw length.
The non-volume path retains the live Metal position/normal/tangent and index
buffers without a per-frame geometry copy. Four native vertex variants expand
16- or 32-bit indices into one normal line and, when selected by the existing
source predicate, one tangent line per triangle-index occurrence.

Indexed expansion reproduces the GL vertex/geometry shader's view-space
direction normalization and projection. Both paths preserve one-pixel lines,
disabled culling, source-over blending, RGB-only writes and read-only LEQUAL
world depth under forward and reverse Z. Existing volume HUD normals remain in
the ordered native UI-capture path; an unexpected non-volume HUD drawable
invalidates that transaction rather than omitting output.

Validation rejects missing storage, non-finite line data, invalid kinds,
colors, ranges, index widths, buffer offsets, transforms, draw lengths,
tangent contracts, non-monotonic order and collisions with every globally
ordered owner. Only `RENDER_DEBUG_NORMALS` was removed from the world-output
veto. `RENDER_DEBUG_TEXEL_DENSITY` is now its sole member; the separate latent
pathfinding guard remains fail-closed.

## Physical and build evidence

- Complete RelWithDebInfo, Debug and Release physical Metal suites: 51,416
  assertions in 52 cases per configuration under Metal API/load-action
  validation; offline MSL compiled in all three.
- Coverage includes explicit CPU line replay, zero-copy indexed 16- and 32-bit
  normal expansion, optional tangent expansion, forward/reverse read-only
  depth, RGB-only destination-alpha preservation, invalid/no-owner inputs and
  cross-owner order rejection.
- Neutral non-manual/non-GL gate: 484/484 in 35.58 seconds.
- Warnings-as-errors integration passed. Source-exact Release passed at
  `08973b83dccad57d39fa0386c4f8463385761d2a` after 161 steps and staged
  Development `26.4.0d68087` (`26.4.0.68087`). Generated build state embeds
  the full source SHA.
- Deep strict signing passed; bundle inspection found 349 ARM64 Mach-O slices
  and zero invalid slices. Source and packaged MSL compare byte-identically.

SHA-256 provenance:

- executable: `039fbe6e2af93d5c9fc63d8c94b79a574323aa908a4a27a08c7a16c6807f445d`;
- source and packaged MSL: `73955209f611038a5e4fe0305642614fefee30766dcd26fd03b7df7d3933df20`;
- compiled metallib: `fd64dfedceb9056e85090916b7e0dab2d8e17c553cc4ffe63ea1b5e67647c297`.

The main executable still links Metal and OpenGL. Strict no-GL, broad live
parity, stability and qualifying performance gates remain open. No controlled
live reference/candidate Normals comparison is claimed.

## Persisted owner decisions

- Exact RLVa parity is deferred non-release nice-to-have M-013. Exact RLVa
  comparisons require enabling RLV in Preferences in both viewers, saving and
  restarting both. Mandatory work is only the minimum needed for Metal/no-GL
  cutover; the durable inventory is `docs/metal/work/deferred/rlva-parity.md`.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers before capture. Off/on is invalid; both-off is diagnostic only and
  cannot close shadow parity.

## Next executable action

Audit `RENDER_DEBUG_TEXEL_DENSITY` in full before changing the last explicit
visual veto. Reconcile every textured-geometry producer, shader input, state,
transform and side effect; do not infer parity from the Normals checkpoint.
