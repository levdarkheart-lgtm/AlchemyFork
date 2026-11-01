# Native far-plane projection overlay execution — 2026-11-01

**Work items:** M-008/M-009 advanced output and native-only cleanup.
**Integrated code:** `c7e82c20bd9d0394a376b32a1ea77436c0296d33`.

## Executed work

1. Traced the persisted `AlchemyRenderFarPlaneOverlay` contribution through its
   post-debug, non-HUD world boundary and confirmed it was a reachable Metal
   publication veto.
2. Captured the exact live model-view-projection matrix and all 18
   `ALFarPlane::overlayColumns()` records only after authoritative GL target
   allocation succeeds.
3. Added procedural Metal expansion to the same 12 box triangles, preserving
   nearer-orange then farther-cyan ordering, forward/reverse-Z depth behavior,
   viewport orientation and GL depth conversion.
4. Added a reusable private RGBA8/depth32-stencil8 overlay target and HDR/non-HDR
   composites with point sampling, source-alpha RGB blending, RGB-only writes
   and no scene-depth writes.
5. Released the private diagnostic target when inactive, matching the source
   owner's setting-off lifetime.
6. Added fail-closed validation for missing/invalid storage, count/cast/size
   bounds, partition consistency, finite transforms/data, extents, homogeneous
   values, colors, allocation and duplicate capture.
7. Removed only the now-native far-plane eligibility veto. General render-debug,
   wireframe and reflection-probe-volume output remain fail-closed.

## Executed checks

- Focused physical world case: 1,424 assertions under Metal API/GPU/load-store/
  shader validation.
- Complete physical suites: 45,073 assertions/52 cases in RelWithDebInfo,
  Debug and Release under the same validators.
- Full warnings-as-errors Debug viewer: compile, link and stage pass.
- Neutral CTest gate: 484/484 in 245.18 seconds.
- Source-exact Release Development `26.4.0d68042`: build, link, stage and sign
  pass at `c7e82c20bd`; deep strict signing and 349/349 ARM64 verification pass.
- Executable SHA-256:
  `080570aaed0771fca9b60044ad55f0d555601f09f24cc717f93e6df5e203c673`.
- Packaged/source MSL SHA-256:
  `230cb3ea57d16c378111dc2a26c614e6367363ea3052e76ad5dd03c53230f004`.
- Compiled metallib SHA-256:
  `3afd67c7bd57e5e3292b0a6dba30cf530b56f8daebdd83068448dec0e4771c0b`.
- Host doctor: pass on physical Apple M1 Max with unified memory and Apple7.

## Priority decision

Continue a bounded source-led audit of general render-debug, wireframe and
reflection-probe-volume output. Implement the smallest confirmed reachable
omission with asymmetric physical proof and leave all other owners fail-closed.

Shadow comparisons require shadows enabled in both viewers. Exact RLVa parity
stays in persistent M-013 as a nice-to-have, with only minimum cutover behavior
required and enable/save/restart mandatory for any exact comparison. Full
feature/stability, strict no-GL and qualifying paired M1 Max performance gates
remain open.
