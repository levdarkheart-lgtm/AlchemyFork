# Native world wireframe implementation — 2026-11-01

## Implemented boundary

Commit `23ce2f7190` adds `ALMetalTriangleFillMode` and a checked
`ALMetalRenderEncoder::setTriangleFillMode()` mapping to Metal fill/line state.
The capture structs for G-buffer, post-deferred and all native sky families now
carry wireframe state. `LLPipeline::MetalWorldSimpleCapture` snapshots
`gUseWireframe` for main/probe capture and forces fill for impostor bake.

The implementation changes:

- `indra/almetal/almetalpipeline.h` and `.mm`: explicit encoder fill-mode API.
- `indra/almetal/almetalworld.h` and `.cpp`: wireframe state propagation, exact
  raster scope, filled diagnostic ownership and raw-gray G-buffer clear.
- `indra/newview/pipeline.cpp`: authoritative viewer-state capture and removal
  of the proven wireframe eligibility veto.
- `indra/almetal/tests/almetal_gpu_test.mm`: physical asymmetric coverage for
  G-buffer, deferred geometry, sky geometry and fill restoration.

The initial sRGB clear used the mathematical decode of 0.5 and the physical
byte assertion observed 127. The implementation was corrected to decode byte
128/255 (`0.21586050011389926`) and the same assertion then observed exactly
128. No tolerance was added.

## Validation record

Focused physical commands used the RelWithDebInfo `almetal_tests` executable
with Metal API, GPU, load/store and shader validation enabled. The G-buffer
case passed 1,491 assertions and the exact deferred/sky case passed 1,927.
Complete RelWithDebInfo, Debug and Release physical suites each passed 45,147
assertions/52 cases under the same validators.

Warnings-as-errors RelWithDebInfo production integration linked and staged. The
neutral non-manual/non-GL gate passed 484/484 in 35.58 seconds. The source-exact
Release wrapper build at committed head succeeded as `26.4.0d68047`; deep strict
signing, 349/349 ARM64 slice inspection and source/package shader hashes passed.
Exact hashes are retained in
`docs/metal/state/archive/CURRENT-wireframe-checkpoint-2026-11-01.md`.

## Ledger decision

Wireframe is no longer an M-008/M-009 eligibility fallback. A controlled live
candidate/reference pair remains useful parity evidence, so the feature-matrix
reference column remains `unverified` while Metal physical ownership is
`verified`.

General render-debug output is now the only reachable advanced-output veto to
trace. The pathfinding guard stays latent for the hard-disabled OSS stub.

The standing acceptance prerequisites remain part of the active ledger:

- shadows enabled in Graphics Preferences in both viewers before shadow pairs;
- RLV enabled, saved and both viewers restarted before exact RLVa pairs;
- exact RLVa parity deferred as M-013 nice-to-have, with only cutover-unblocking
  implementation required.
