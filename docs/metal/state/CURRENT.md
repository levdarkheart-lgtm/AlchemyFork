# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `c7e82c20bd` (`feat(metal): replay far plane overlay natively`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `c7e82c20bd` replaces the
reachable Develop **far-plane projection overlay** fallback with native Metal
replay. `ALFarPlaneOverlay::render()` captures only after its authoritative GL
target allocation succeeds. The transaction retains the exact live model-view-
projection matrix and all 18 camera-relative columns, ordered as the GL owner
draws them: nine nearer orange columns followed by nine farther cyan columns.

The native owner expands each column procedurally to the same 12 triangles,
depth-tests it in a private RGBA8/depth32-stencil8 target, and supports forward
and reverse Z. It composites the result after native Highlight Transparent and
object-update blips using source-alpha RGB blending while preserving destination
alpha and the scene depth attachment. HDR and non-HDR output pipelines match
their destination formats. The private target is reused at a stable size and
released when the setting is off. Missing, nonfinite, inconsistent or
unrepresentable capture data rejects native authority; the migration GL draw
remains intact. `AlchemyRenderFarPlaneOverlay` therefore no longer makes native
world publication ineligible.

Prior native owners remain covered: static/rigged world and HUD **Highlight
Transparent**, plus the developer **Show Updates to Objects** eight-pixel blips.
The current OSS ARM64 viewer links only the nonfunctional
`llphysicsextensionsos` pathfinding stub, so pathfinding visualization is not a
reachable retained-Alchemy output or a release-critical port target. The
remaining bounded fail-closed owners are general render-debug output, wireframe
and reflection-probe-volume visualization.

The preceding production-call audit found no valid viewer-produced
reflection-probe divergence; its extra Metal bounds checks do not alter valid
data. That audit remains closed without a speculative edit.

Exact RLVa parity is owner-deferred M-013 nice-to-have. Implement only enough
RLVa behavior to avoid blocking Metal authority and viewer-owned OpenGL removal.
The persistent inventory is [the deferred RLVa backlog](../work/deferred/rlva-parity.md).
Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both viewers before testing.

Any candidate/reference shadow comparison is admissible only when shadows are
enabled in Graphics Preferences in **both** viewers before capture. A mismatched
off/on pair is invalid; a both-off control can isolate a residual but cannot
prove shadow parity.

## Build and test evidence

- Focused physical RelWithDebInfo world replay passed 1,424 assertions under
  Metal API/GPU/load-store/shader validation. It covers invalid overlay storage,
  an invalid nearer/farther partition, nonfinite data, forward- and reverse-Z
  overlap ordering, exposed farther coverage, HDR/non-HDR output, destination-
  alpha preservation, untouched scene depth and unchanged background.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  45,073 assertions across 52 cases under the same validation layers.
- Warnings-as-errors RelWithDebInfo, Debug and Release Metal renderer, built-in
  MSL, GPU test and production viewer integration compiled and linked. A full
  Debug viewer build also staged successfully.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passed 484/484 in 245.18 seconds.
- Source-exact warnings-as-errors Release Development build `26.4.0d68042`
  linked, staged and signed at committed code head `c7e82c20bd`.
- Executable SHA-256:
  `080570aaed0771fca9b60044ad55f0d555601f09f24cc717f93e6df5e203c673`.
  Packaged/source MSL SHA-256:
  `230cb3ea57d16c378111dc2a26c614e6367363ea3052e76ad5dd03c53230f004`;
  compiled metallib SHA-256:
  `3afd67c7bd57e5e3292b0a6dba30cf530b56f8daebdd83068448dec0e4771c0b`.
- Deep strict signing passed; `tools/metal/verify_app.sh` verified all 349
  Mach-O slices as ARM64, and the main executable reports arm64.
- `tools/metal/doctor.sh` passed on Apple M1 Max with unified memory and Apple7-
  family Metal support. Harness validation and tooling tests pass after this
  documentation checkpoint.
- The main executable still links both Metal and OpenGL during migration.
  Strict no-GL remains an open release gate; no native-only claim is made.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Broad native world/UI/resource owners and physical regressions exist; complete parity remains open. |
| M-007 world/material | doing | Bounded active transparency/material and reflection-probe audits found no valid-production divergence; broader live parity remains open. |
| M-008 advanced views/effects | doing | Highlight Transparent, object-update blips and the far-plane projection overlay are native. OSS pathfinding is a nonfunctional stub. Broader reachable debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. General render-debug, wireframe and reflection-probe-volume owners remain fail-closed pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. A persisted, reachable developer output
owner now reproduces its exact columns, depth order, blend/order and attachment
side effects in native Metal instead of vetoing presentation.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live material, editing and shadow comparisons; full feature and
stability matrices; viewer-owned OpenGL removal; and paired quality-equivalent
M1 Max measurements. Independent build/review evidence and reproducible distinct
release proof also remain open. Exact RLVa behavior/image parity is separately
deferred as non-critical M-013.

**Evidence-based priority:** continue the bounded production-call audit across
general render-debug, wireframe and reflection-probe-volume visualization.
Select the smallest owner that actually emits uncaptured output, preserve its
exact state and ordering, and avoid speculative ports for inactive or already-
captured paths.

## Exact next actions

1. Trace production contribution conditions and output ownership for general
   render-debug, wireframe and reflection-probe-volume visualization.
2. Implement the smallest confirmed reachable omission with asymmetric physical
   state/output proof; leave every uncovered owner fail-closed.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa parity
   in M-013 and require shadows enabled in both viewers for shadow pairs.
