# Native far-plane projection overlay checkpoint — 2026-11-01

**Observed host:** physical Apple M1 Max.
**Branch:** `codex/apple-silicon-metal`.
**Code checkpoint:** `c7e82c20bd9d0394a376b32a1ea77436c0296d33` (`feat(metal): replay far plane overlay natively`).

## Integrated output owner

The persisted Develop setting `AlchemyRenderFarPlaneOverlay` draws 18 camera-
relative projection columns after `LLPipeline::renderDebug()`: nine nearer
orange columns, then nine farther cyan columns. The GL owner expands every
column to a 12-triangle axis-aligned box, depth-tests the pairs in a private
RGBA8 target with its own depth buffer, then source-alpha composites RGB over
the scene without changing scene alpha or depth. It is skipped for HUD-only,
cube-snapshot, impostor and cutout routes.

Commit `c7e82c20bd` captures only after the GL owner has successfully allocated
its target. It retains the live model-view-projection matrix and the exact
column centers, half extents, colors and nearer/farther partition. A procedural
Metal vertex stage emits the same triangles, applies the established viewport-Y
and GL-depth conversion, and writes a private RGBA8/depth32-stencil8 target.
Forward and reverse Z use matching clear/compare conventions.

The native result composites after Highlight Transparent and object-update
blips. Point sampling, source-alpha RGB blending, an RGB-only write mask and an
always/no-write depth state preserve destination alpha and the retained scene
depth attachment. Separate HDR/non-HDR pipelines match destination formats.
The private target is reused at a stable size and released when the setting is
inactive.

Missing storage, invalid counts, an inconsistent nearer/farther partition,
nonfinite matrices or records, invalid homogeneous values/extents/colors,
allocation failure and duplicate capture fail native authority closed. The
migration GL owner remains intact. The far-plane setting was removed from
`ALMetalWorldOverlayEligibility` only after native replay was proven.

## Physical validation

- Focused RelWithDebInfo world replay: 1,424 assertions under Metal API/GPU/
  load-store/shader validation. Coverage includes missing storage, invalid
  partition and nonfinite-input rejection; nearer-orange versus farther-cyan
  overlap in forward and reverse Z; exposed farther coverage; HDR/non-HDR
  output; preserved destination alpha; untouched scene depth; and unchanged
  background.
- Complete RelWithDebInfo, Debug and Release Metal suites: 45,073 assertions
  across 52 cases in each configuration under the same validators.
- Full warnings-as-errors Debug production viewer build: compile, link, stage
  and sign pass.
- Neutral gate: 484/484 tests passed in 245.18 seconds.
- `git diff --check`: pass before the code commit.

## Source-exact packaged evidence

Release Development `26.4.0d68042` built, linked, staged and signed from
`c7e82c20bd9d0394a376b32a1ea77436c0296d33`.

- Deep strict signature validation: pass.
- ARM64 bundle inspection: 349/349 Mach-O files valid; main executable arm64.
- Executable SHA-256:
  `080570aaed0771fca9b60044ad55f0d555601f09f24cc717f93e6df5e203c673`.
- Packaged and source MSL SHA-256:
  `230cb3ea57d16c378111dc2a26c614e6367363ea3052e76ad5dd03c53230f004`.
- Compiled metallib SHA-256:
  `3afd67c7bd57e5e3292b0a6dba30cf530b56f8daebdd83068448dec0e4771c0b`.
- The executable still links Metal and OpenGL; strict no-GL remains open.
- `tools/metal/doctor.sh` passed on macOS 26.5.2 / Xcode 26.6 with Apple M1
  Max, unified memory and Apple7-family Metal support.

## Persistent decisions and next boundary

The remaining bounded fail-closed output owners are general render-debug,
wireframe and reflection-probe-volume visualization. Exact RLVa parity remains
deferred M-013 nice-to-have; only minimum cutover support is release-relevant,
and exact comparisons require RLV enable/save/restart in both viewers. Shadow
comparisons require shadows enabled in Graphics Preferences in both viewers.

Full feature/stability, controlled live parity, strict no-GL and qualifying
paired M1 Max performance gates remain open.
