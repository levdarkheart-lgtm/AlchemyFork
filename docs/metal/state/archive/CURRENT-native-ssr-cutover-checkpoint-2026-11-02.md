# Native SSR GL-bootstrap cutover checkpoint — 2026-11-02

## Scope and source

Commit `53de84c79c` removes the GL framebuffer/PBO bootstrap and fallback used by
Metal screen-space-reflection consumers. It deletes the paired readback queue,
CPU color/depth staging, private-texture upload and GL-generation bookkeeping
from `pipeline.cpp` and removes their public declarations.

The established native final-color publisher remains unchanged. Startup and
resize now encode one complete frame without SSR feedback, publish its coherent
native color/depth history, and reject that warm-up frame from UI composition,
snapshots and drawable presentation. Subsequent consumers accept only matching
Metal capture dimensions and RGBA8-or-R11G11B10F plus R32F formats. A failed
publication retains the prior native pair or repeats warm-up when none exists.

This is an owner-level data-path cutover. It does not remove the GL context,
global traversal or unrelated GL renderer owners, and it does not claim visual
parity or product completion.

## Static inventory

Command:

`python3 tools/metal/audit_gl.py --mode strict --json --output artifacts/metal/gl-strict-ssr-native-warmup.json`

Expected strict exit: 1. Result: 2,408 scanned files and 3,337 findings, down
from 3,351. The 14 removed findings are GL calls in the retired SSR framebuffer
and PBO path. The artifact is local/ignored.

## Build and physical validation

- `tools/metal/doctor.sh`: exit 0 on physical M1 Max, 64 GiB, Apple7 support.
- `AL_METAL_PRESET=ninja-os-arm64 AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64
  AL_METAL_CONFIG=RelWithDebInfo AL_METAL_JOBS=8 tools/metal/build.sh build`:
  exit 0, warnings as errors, build `26.4.0d68103` from `53de84c79c`.
- `tools/metal/verify_app.sh build-Darwin-ninja-os-arm64/newview/RelWithDebInfo/Alchemy\ Metal\ Development.app`:
  exit 0, 349 checked Mach-O slices, zero non-ARM64 slices.
- `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=0
  MTL_DEBUG_LAYER_VALIDATE_LOAD_ACTIONS=1
  MTL_DEBUG_LAYER_ERROR_MODE=nslog ctest --test-dir
  build-Darwin-ninja-os-arm64 -C RelWithDebInfo -V -R
  '^almetal_tests/almetal_gpu_test$'`: exit 0, 52,190 assertions in 52 cases.
- Existing physical cases cover rendering with no SSR pack, invalid partial-pack
  rejection, native paired history publication, row convention, format/depth
  extraction, target reuse and all current SSR consumer families.

Executable SHA-256:
`12fcfbe8780f4bae179f3d1e1f77f99fb50b405d7ff4331542b9eefa43f909c8`.
Source and packaged MSL SHA-256:
`fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
Metallib SHA-256:
`ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.

## Broad gate result

The deliberately unfiltered 574-entry RelWithDebInfo CTest inventory completed
in 1,268.61 seconds. It passed 568 entries, failed six and skipped five. Three
unique failures reproduced in focused reruns and are confined to untouched GL
test code:

- `alframefences_gl_test`: four pacing/budget assertions;
- `llrendertarget_gl_test`: one depth-format assertion;
- `aldof_gl_test`: two cases require unavailable clip control.

The llrender and newview combined targets duplicate those failures, and the DoF
benchmark duplicates clip-control failure. This checkpoint makes no green broad-
gate claim. The prior 484/484 Release non-manual/non-GL gate remains the latest
green architecture-neutral evidence.

## Visual-validation policy

No viewer was launched, driven or compared. Per owner direction, future visual
questions are collected in
`docs/metal/work/active/visual-review-queue.md` and handled in one owner-attended
batch at most once per day.

## Next action

Inspect the avatar-impostor bake's remaining GL attachment/PBO ingress against
its native bake owner, then remove the next proven duplicate crossing without
global draw suppression.
