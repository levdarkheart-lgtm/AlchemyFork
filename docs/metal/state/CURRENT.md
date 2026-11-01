# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `28c4dca74b` (`feat(metal): replay reflection probe volumes natively`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `28c4dca74b` replaces the
persisted Develop **Render Reflection Probe Volumes** fallback with a native
Metal replay. Capture occurs at the authoritative GL draw boundary and retains
the live manual-probe selection pack plus the current inverse projection.
Radiance and irradiance textures are deliberately not dependencies because this
visualization consumes only probe shapes and scene depth.

The native full-screen pass reconstructs eye-space position from the retained
depth attachment under forward or reverse Z. It replays the GLSL sphere and box
intersection rules for manual probes only: intersections before scene depth
accumulate yellow and hidden intersections accumulate distance-faded blue. The
result uses source-alpha RGB blending and RGB-only writes, preserving destination
alpha and depth. HDR and non-HDR pipelines match their output formats. Invalid
probe counts, nonfinite matrices or consumed shape data, negative sphere radii,
an unreadable depth target, allocation failure or duplicate capture rejects
native authority; the migration GL draw remains intact. The
`RenderReflectionProbeVolumes` setting therefore no longer makes native world
publication ineligible.

Prior native advanced-output owners remain covered: static/rigged world and HUD
**Highlight Transparent**, **Show Updates to Objects** blips, and the Develop
far-plane projection overlay. The current OSS ARM64 viewer links only the
nonfunctional `llphysicsextensionsos` pathfinding stub, so pathfinding
visualization is not a reachable retained-Alchemy output or a release-critical
port target. The remaining reachable fail-closed output owners are general
render-debug output and wireframe; the latent pathfinding guard is retained.

The preceding production-call audit found no valid viewer-produced
reflection-probe divergence; its extra Metal bounds checks do not alter valid
data. That bounded audit remains closed without a speculative edit.

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

- Focused physical RelWithDebInfo world replay passed 1,470 assertions under
  Metal API/GPU/load-store/shader validation. It covers invalid count and
  inverse-projection input, manual sphere foreground/background behavior, the
  box branch, automatic-probe exclusion, forward/reverse-Z samples, HDR/non-HDR
  output, destination-alpha preservation and untouched scene depth.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  45,118 assertions across 52 cases under the same validation layers.
- Warnings-as-errors RelWithDebInfo production viewer integration compiled,
  linked, staged and signed. Debug and Release Metal renderer, built-in MSL and
  GPU tests compiled and linked.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure -j 8` passed 484/484 in 40.46 seconds.
- Source-exact warnings-as-errors Release Development build `26.4.0d68044`
  linked, staged and signed at committed code head `28c4dca74b`.
- Executable SHA-256:
  `4306dea5dedc006eab0017f40404985d323f224d0b40c864b54bc61f2fcc45e0`.
  Packaged/source MSL SHA-256:
  `dd51886cc8cc33a8b19ac69206fed5045421c8905bb51c7d596fdeaefb467214`;
  compiled metallib SHA-256:
  `b5ccb104ec22fade3cf1822595924c751128b6ca14e0ab188276890339357e58`.
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
| M-008 advanced views/effects | doing | Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. OSS pathfinding is a nonfunctional stub. Broader reachable debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. General render-debug and wireframe owners remain fail-closed pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. A persisted, reachable advanced-output
owner now reproduces its manual sphere/box intersections, depth reconstruction,
blend/order and attachment side effects in native Metal instead of vetoing
presentation.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live material, editing and shadow comparisons; full feature and
stability matrices; viewer-owned OpenGL removal; and paired quality-equivalent
M1 Max measurements. Independent build/review evidence and reproducible distinct
release proof also remain open. Exact RLVa behavior/image parity is separately
deferred as non-critical M-013.

**Evidence-based priority:** trace wireframe and general render-debug production
contribution. Select the smallest owner that actually emits uncaptured output,
preserve its exact state and ordering, and avoid speculative ports for inactive
or already-captured paths.

## Exact next actions

1. Trace production contribution conditions and output ownership for wireframe
   and general render-debug output; retain the latent pathfinding guard.
2. Implement the smallest confirmed reachable omission with asymmetric physical
   state/output proof; leave every uncovered owner fail-closed.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa parity
   in M-013 and require shadows enabled in both viewers for shadow pairs.
