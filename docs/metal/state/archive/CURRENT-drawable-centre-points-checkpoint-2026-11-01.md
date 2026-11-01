# Native drawable-centre Points checkpoint — 2026-11-01

**Observed:** physical Apple M1 Max acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `93dbcc131f` (`feat(metal): replay drawable centre points natively`)
**Source-exact build:** Release Development `26.4.0d68055`

This archive preserves the bounded Develop Points implementation and validation
checkpoint. It does not claim complete advanced-output parity or a Metal-only
release.

## Implementation observation

Develop **Points** now has a native Metal owner at the exact
`renderPoints(LLDrawable*)` boundary. Capture retains one opaque-white one-pixel
point for every valid face at `LLFace::mCenterLocal`. It records the current
model-view-projection transform for each region-partition or visible-spatial-
bridge traversal and coalesces only consecutive batches with the same
transform.

The native opaque-post scene stores a flat compact position/color buffer plus
ordered batches. Dedicated LDR/HDR RGB-only source-alpha pipelines use an
always-pass/no-depth-write state, matching the disabled inherited GL depth
read/write state. The encoder merges Points and Agent Target batches by captured
traversal order before reflection-probe-volume visualization, retaining whether
a drawable's points occur before or after its self-avatar crosshairs.

Capture and encoder validation are fail-closed. Publication requires the
observed blend-enabled, RGB-only color-mask, one-pixel point-size and disabled-
depth state. Missing or mismatched storage, invalid/non-contiguous ranges,
overflow, allocation failure, zero or non-monotonic batches, a cross-owner order
collision, or non-finite transforms/data makes native presentation ineligible.
Empty or absent Points ownership is an exact no-op.

`RENDER_DEBUG_POINTS` was removed from the native-presentation veto only after
the asymmetric evidence below passed. The explicit render-debug world-output
set therefore decreases from 18 to 17 modes. Those 17 modes and the latent
pathfinding guard remain fail-closed.

## Physical validation evidence

All Metal tests ran with Metal API, GPU, load/store-action and shader validation
enabled:

- focused RelWithDebInfo Points/world test: 2,053 assertions in one case;
- complete RelWithDebInfo suite: 45,709 assertions in 52 cases;
- complete Debug suite: 45,709 assertions in 52 cases;
- complete Release suite: 45,709 assertions in 52 cases.

The focused tests prove missing/mismatched storage, invalid range/order and
non-finite rejection; the same local coordinate under two transforms; exact
two-point white output; RGB-only destination-alpha preservation; forward and
reverse-Z depth identity even behind covered geometry; empty/no-owner identity;
both Points-before-Agent-Target and Agent-Target-before-Points output; and equal
cross-owner order rejection.

Warnings-as-errors RelWithDebInfo production integration compiled, linked,
staged and signed. The neutral non-manual/non-GL gate passed 484/484 in 35.42
seconds. Source-exact Release wrapper build `26.4.0d68055` completed at committed
code head `93dbcc131f`. Deep strict signing passed; `verify_app.sh` inspected 349
Mach-O slices with zero non-ARM64 slices, and the main executable reports arm64.

SHA-256 provenance:

- executable: `13da93dd728e38b406a3714c040ae32f32547aba875344044916c9f8de79a3ca`;
- source and packaged MSL: `a33ec7160965a1c6805ad5b2a2f69d95da8bff630cf2b05c346a57deb13e2033`;
- compiled metallib: `537685a0c488cded37fc64b7a532f57610771deeaf9f17d6ae9c1772787549b1`.

The main executable still links OpenGL during migration. Strict no-GL remains
an open release gate.

## Durable comparison prerequisites

- Any exact RLVa comparison must enable RLV in Preferences in both viewers,
  save, and restart both viewers. Exact RLVa parity is deferred non-release
  nice-to-have M-013; minimum cutover behavior remains required.
- Any shadow candidate/reference comparison must enable shadows in Graphics
  Preferences in both viewers before capture. An off/on pair is invalid, while
  a both-off control can isolate a residual but cannot prove shadow parity.

## Next bounded owner

Source inspection selects Develop **Composition**. Its sole production owner
emits a fixed 261-by-261 current-region point grid: coordinates 0..255 are blue,
the five-coordinate outer border is red, and height is
`getCompositionXY(x, y) * 5 + 50`. Remove only
`RENDER_DEBUG_COMPOSITION` after physical coordinate, color, height, point-state
and no-region/off proof; retain the other 16 modes fail-closed.
