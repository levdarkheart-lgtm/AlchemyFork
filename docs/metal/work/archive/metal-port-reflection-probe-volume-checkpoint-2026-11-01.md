# Native reflection-probe volume visualization execution — 2026-11-01

**Work items:** M-008/M-009 advanced output and native-only cleanup.  
**Integrated code:** `28c4dca74be2084313f966d6fc8ed2804196fcdb`.

## Executed work

1. Traced persisted `RenderReflectionProbeVolumes` through the full-screen draw
   in `LLPipeline::renderDebug()` and confirmed that it was a reachable Metal
   publication veto independent of the general render-debug bit mask.
2. Added a constants-only reflection-probe snapshot so this developer owner
   consumes the live box/sphere/index pack without incorrectly depending on
   mirrored radiance or irradiance generations.
3. Captured that pack and the current inverse projection at the exact GL owner.
   Missing, duplicate or invalid capture fails the surrounding native
   transaction closed while the migration GL draw remains unchanged.
4. Added HDR and non-HDR full-screen Metal pipelines that sample the retained
   scene depth and reconstruct eye-space position under forward or reverse Z.
5. Replayed the authoritative GLSL manual-sphere and manual-box intersection
   math: front intersections accumulate yellow and hidden intersections
   accumulate distance-faded blue. Automatic probes remain invisible.
6. Preserved observable side effects with source-alpha RGB blending, RGB-only
   writes, read-only scene depth and ordering after object-update blips but
   before the far-plane overlay.
7. Added fail-closed validation for count bounds, integral count conversion,
   finite consumed shape data and inverse projection, nonnegative sphere radii,
   shader-readable depth and resource allocation.
8. Removed only the now-native reflection-probe-volume eligibility veto.
   General render-debug and wireframe remain fail-closed, and the latent
   pathfinding guard remains retained.

## Executed checks

- Focused physical RelWithDebInfo world case: 1,470 assertions under Metal API/
  GPU/load-store/shader validation. Coverage includes invalid input, manual
  sphere and box branches, automatic-probe exclusion, foreground/background
  colors, forward/reverse-Z controlled samples, HDR/non-HDR output,
  destination-alpha preservation and untouched scene depth.
- Complete physical Metal suites: 45,118 assertions/52 cases in RelWithDebInfo,
  Debug and Release under the same validators.
- Warnings-as-errors RelWithDebInfo production viewer: compile, link, stage and
  sign pass. The source-exact Release wrapper build also passes.
- Neutral CTest gate: 484/484 in 40.46 seconds.
- Source-exact Release Development `26.4.0d68044`: build, link, stage and sign
  pass at `28c4dca74b`; deep strict signing and 349/349 ARM64 verification pass.
- Executable SHA-256:
  `4306dea5dedc006eab0017f40404985d323f224d0b40c864b54bc61f2fcc45e0`.
- Packaged/source MSL SHA-256:
  `dd51886cc8cc33a8b19ac69206fed5045421c8905bb51c7d596fdeaefb467214`.
- Compiled metallib SHA-256:
  `b5ccb104ec22fade3cf1822595924c751128b6ca14e0ab188276890339357e58`.
- Host doctor: pass on physical Apple M1 Max with unified memory and Apple7.

## Priority decision

Continue a bounded source-led audit of wireframe and general render-debug
output. Implement the smallest confirmed reachable omission with asymmetric
physical proof and leave all other owners fail-closed.

Shadow comparisons require shadows enabled in Graphics Preferences in both
viewers. Exact RLVa parity stays in persistent M-013 as a nice-to-have, with
only minimum cutover behavior required and enable/save/restart mandatory for
any exact comparison. Full feature/stability, strict no-GL and qualifying paired
M1 Max performance gates remain open.
