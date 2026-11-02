# Reflection-probe native cutover checkpoint — 2026-11-02

## Observed source state

- Branch: `codex/apple-silicon-metal`.
- Renderer checkpoint: `4b6093028e` (`refactor(metal): remove probe GL
  readback mirrors`).
- Host: physical M1 Max, 64 GiB unified memory, macOS 26.5.2, Xcode 26.6;
  `tools/metal/doctor.sh` exited 0 and reported Apple7-family support.
- No viewer was launched, driven, photographed or compared. Visual questions
  remain in the owner-attended daily review queue.

## Implemented cutover

`llreflectionmapmanager.cpp` no longer mirrors ordinary/default reflection
probes through GL PBOs. The removed transaction allocated GL readback rings,
requested default and per-slot SH rows, read every filtered face/mip with
`glReadPixels`, copied the PBOs to CPU vectors, allocated Metal textures, then
uploaded the same pixels and SH rows back to Metal.

The retained native path already owns the complete contract: six ordered face
captures, source-cube retention, SH projection, radiance filtering, cube-array
publication, default-cube publication, slot generation, scheduled/realtime
lane identity, and coherent radiance/SH pairing. Missing, stale, mismatched or
incomplete native data now leaves the Metal transaction incomplete. It never
imports the legacy GL result. The GL probe update remains for the still-retained
legacy renderer; this commit removes only its authority over Metal consumers.

The source change removes 722 lines and adds seven. It also removes the now-dead
`ALGPUReadback` dependency from the reflection manager.

## Static audit

```sh
python3 tools/metal/audit_gl.py --mode strict --json \
  --output artifacts/metal/gl-strict-reflection-probe-native-cutover.json
```

Expected exit status is 1. The audit scanned 2,408 files and reported 3,269
findings, ten fewer than the preceding 3,279-finding checkpoint. The reflection
manager now has zero `gl_function` findings; its 29 remaining findings are
GL shader/state/traversal references. The executable still links both
`OpenGL.framework` and `Metal.framework`, so strict native-only is not met.

## Source-exact build and physical validation

At committed source `4b6093028e9241706a5d6bcfe5f21b59a9a6dd78`:

```sh
AL_METAL_PRESET=ninja-os-arm64 \
AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 \
AL_METAL_CONFIG=RelWithDebInfo AL_METAL_JOBS=8 \
tools/metal/build.sh build
```

The warnings-as-errors command exited 0 and produced Development build
`26.4.0d68110` (`26.4.0.68110`).

- `tools/metal/verify_app.sh` exited 0: 349/349 Mach-O slices are ARM64.
- Executable SHA-256:
  `cb331c2789776603c99b59f8dbb578977270355512550e7334d229b6e101ea91`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
- Packaged metallib SHA-256:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- With `MTL_DEBUG_LAYER=1`, shader validation disabled, load-action validation
  enabled and errors sent to `nslog`, the complete RelWithDebInfo physical
  Metal suite passed 52,190 assertions across 52 cases in 2.78 seconds.
- `git diff --check` passed before the source commit.

The earlier unfiltered 574-entry CTest result remains 568 passes, six legacy
GL-host failures and five skips. It was not rerun for this Darwin-only manager
cutover and is not represented as green.

## Next executable boundary

Trace the already-native scope-sampling and one-pixel scene-readback owners in
`pipeline.cpp`, then remove the next source-proven GL/PBO or synchronous GL
fallback without weakening the native exactness gate. Continue to queue visual
questions instead of launching viewers.

