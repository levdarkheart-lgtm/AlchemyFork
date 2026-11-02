# Texel Density native debug replay checkpoint — 2026-11-02

This archive records the source-exact `RENDER_DEBUG_TEXEL_DENSITY` checkpoint
superseded by `docs/metal/state/CURRENT.md`.

## Observed implementation

Commit `b0e0973898da8e3a275b8a29ed72e7ce7f4e9db5` captures and replays every
reachable Develop **Texel Density** face draw. Source inspection established
that current, desired and full modes select discard levels, query texture
dimensions, bind the checkerboard and load a texture matrix, but the bound
production `gDebugProgram` consumes only vertex position and the current
uniform color. It has no texture, UV, sampler or texture-matrix input.

Native presentation therefore retains the actual visible zero-copy indexed
uniform-color overlay instead of inventing a checkerboard shader. The unchanged
source traversal preserves each mode's texture lookup, binding and matrix side
effects. Replay retains the live model-view-projection matrix, arbitrary finite
color, u16/u32 indices, disabled culling, fill raster, standard alpha blend,
RGB-only writes and writable LEQUAL depth under forward and reverse Z. HUD
faces remain covered by ordered native indexed UI capture.

Validation rejects missing storage/buffers, invalid ranges, offsets and index
types, non-finite transforms/colors, non-monotonic order and collisions with
every globally ordered debug owner. Removing `RENDER_DEBUG_TEXEL_DENSITY`
leaves no reachable render-debug world-geometry bit in the explicit visual
veto. The separate latent OSS pathfinding guard remains fail-closed.

## Physical and build evidence

- Complete RelWithDebInfo, Debug and Release physical Metal suites: 51,784
  assertions in 52 cases per configuration under Metal API/load-action
  validation; offline MSL compiled in all three.
- Coverage includes u16/u32 indexed replay, arbitrary color, forward/reverse
  writable depth, RGB-only destination-alpha preservation, invalid/no-owner
  inputs and cross-owner order rejection.
- Neutral non-manual/non-GL gate: 484/484 in 37.42 seconds.
- Warnings-as-errors integration passed. Source-exact Release passed after 161
  steps at the full code SHA above and staged Development `26.4.0d68089`
  (`26.4.0.68089`). Generated build state embeds that full SHA.
- Deep strict signing passed; bundle inspection found 349 ARM64 Mach-O slices
  and zero invalid slices. Source and packaged MSL compare byte-identically.

SHA-256 provenance:

- executable: `f5fa05484b57fd77f108838e6d5b82ffde0e8a41697116db742b39b9e9b083f7`;
- source and packaged MSL: `73955209f611038a5e4fe0305642614fefee30766dcd26fd03b7df7d3933df20`;
- compiled metallib: `fd64dfedceb9056e85090916b7e0dab2d8e17c553cc4ffe63ea1b5e67647c297`.

The main executable still links Metal and OpenGL. Strict no-GL, broad live
parity, stability and qualifying performance gates remain open. No controlled
live reference/candidate Texel Density comparison is claimed.

## Persisted owner decisions

- Exact RLVa parity is deferred non-release nice-to-have M-013. Exact RLVa
  comparisons require enabling RLV in Preferences in both viewers, saving and
  restarting both. Mandatory work is only the minimum needed for Metal/no-GL
  cutover; the durable inventory is `docs/metal/work/deferred/rlva-parity.md`.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers before capture. Off/on is invalid; both-off is diagnostic only and
  cannot close shadow parity.

## Next executable action

Move to M-009. Run a fresh strict viewer-owned GL and presentation-authority
inventory, classify remaining findings by actual reachability, and select the
next concrete release blocker. The empty render-debug veto is a bounded fact,
not a native-only release claim.
