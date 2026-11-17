# Post-water transparent probe preparation — PPD363

The owner requested this repair before another Home and club recording. The
[69236 checkpoint](../../research/probe-transparency-checkpoint-69236-20261117.md)
found zero sampled prepared-alpha counts. Independent source inspection then
proved a missing post-water-only preparation owner. Those samples alone cannot
prove exhaustive runtime inactivity or the cause of the Home FPS decline.

## Contract and change

Pinned original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`,
`indra/newview/lldrawpoolalpha.cpp:152–170,261–265,647–680`, assigns ordinary
camera-side above-water alpha to the post-water pool. Pre-water and rigged alpha
write depth; ordinary rigid post-water alpha tests without writing depth. The
native capture producer already publishes mutually exclusive pre/post flags,
and the native draw path already supports both. Candidate preparation incorrectly
required the pre-water flag.

The preparation predicate now accepts either stage. Standard simple-alpha,
blend, avatar and depth-only exclusions remain in force. Stage flags, sorting,
water sign, depth policy, shader math and all valid probe taps are unchanged.
The existing immutable masks and completion-owned resources serve this consumer;
there is no added queue, command commit, wait or readback in production.

A separate cumulative diagnostic retains eligible and prepared draws by
pre/post owner plus successful CPU-encoded call count. Its independent renderer
storage survives sparse snapshot replacement and shared shader reload. Counts
publish only after this render call finishes successfully. A borrowed outer
batch may still fail later; this is not GPU completion, per-pixel coverage,
isolated GPU cost or synchronized whole-frame evidence. The latest-call
unshadowed/shadowed snapshot and its source tuple are retained alongside it.

## Source and contained checks

Root repair `035b3bbb509ff9e1b8b257a354150c3b7154c8dd`; builder
`f0c96273654c652842ca7ea6bad671224b891e01`; identical `indra` tree
`3d4a2d9bd5dd743592f72e73e07421dd54d3ca52`.
All evidence remains private under the original repository's ignored
`artifacts/metal/probe-transparency-post-water-repair-20261117/`.

- Old gate, builder743c23956d/root9ea733038e: new explicit post-only test
  exits42, actual prepared draws0 versus required2. Positive authored rendering
  and exact generic/prepared planes pass before that activation failure.
  Raw output and exact executable/library identities are retained in
  `old-gate-red-01/`; this is the expected PPD363 reproduction.
- Corrected native preparation/publication/bounds/lighting/completion suite:
  **1,777,824 assertions,36 cases, exit0**, API and GPU validation enabled.
  Seven transparent cases include the original76 arms and50 new post-only
  matrix arms, plus cumulative/reload/failure controls. New post-only coverage
  spans rigid/skinned, shadow/no-shadow, both depth conventions, HDR/SDR,
  finite/infinite projections, exact color/depth/stencil, positive probe
  changes, current pose, sort order, actual prepared selection and reuse.
- Existing world-simple, main-lighting render pass, reload, admission,
  BASE fast path and offline entry-point checks: **50,837 assertions,18 cases,
  exit0**, same binary/library, API and GPU validation enabled. This includes
  the inherited depth-only/avatar/custom alpha paths. Counts are not summed
  as unique coverage.
- Tested native executable SHA256
  `9b0e850147c9b207141f7b177c6b610b58b52d2e8de468e2f994a1ab158682a4`;
  metallib `fae7a43b5b48b7375f3a47ae52b6c7c7d89dd6b5f6a7456562d9e99438e223d0`.
- Shader source and CPU candidate adapter are byte-identical to69236. The
  retained actual-GPU containment oracle is reused by exact source hashes,
  rather than presented as a new execution. Compiler spill allocation is
  unchanged evidence-wise and remains unknown for the current whole scene.

- Doctor exits0 on this Apple M1 Max. Strict source audit scans3610 files with
  zero blockers/errors. Actual Release commands for the world renderer and
  pipeline use `-O3` and individual aggressive FP options; they are not strict
  FP despite lacking full `-ffast-math`/`-ffinite-math-only`. Metal uses the
  existing metal3.1/fast-math/macOS26.0/source-recording options. Full commands
  and the exact compdb/Ninja depfile-only difference are retained in
  `compiler-options.json`; no compiler option was changed.

## Packaging and next boundary

`tools/metal/build.sh` completes0 at clean builderf0c9627365 in Release,
4jobs. The separately staged app is:
`artifacts/metal/probe-transparency-post-water-repair-20261117/candidate-26.4.0.69238/Alchemy Metal Alpha G2G5.app`.
All8272 nonroot nodes match source kind/mode/link/bytes/hash; source remains
unchanged. Embedded full sourceSHA and CMake identity match. Deep strict
signature,346ARM64/zero-invalid slices, and no forbidden direct graphics imports
in the viewer/two owned media hosts pass. Foreign-component dynamic/runtime
context proof remains a separate product gate. The tested native executable and
metallib remain unchanged after the full app build.

Viewer executable SHA256
`e8fb943f1eede413e2c32c7182d70722a7b45476f67ba638723f93c514e45139`.
Staged metallib SHA256
`fae7a43b5b48b7375f3a47ae52b6c7c7d89dd6b5f6a7456562d9e99438e223d0`.
Private `stage-verification/receipt.json` binds the complete manifests and
verification commands. Documentation harness33required-files and both normal/
capture-mode controller dry runs exit0; neither dry run launches anything.
Private `qualification.json` indexes all commands, source identities, failures
and evidence hashes.

The qualified256MiB manual-login controller has a separately copied configuration
bound to this exact app/source. Normal recording has API/GPU validation and the
capture layer off; no settings writes or automatic login. Home180s comes first,
then a separately armed club interval covering teleport/loading and settlement.
Normal and capture-mode dry runs must launch nothing. Detailed replayable GPU
frame captures are a separate diagnostic interval, not normal FPS evidence.

No viewer launch, login, recording or push occurs in this repair step. In-world
activation, visual correctness and FPS benefit remain pending the owner Home
then club checkpoint. First verify cumulative post-water prepared counts advance;
use them to establish consumer activity rather than infer it from mask creation
or latest-call zeros. PPD319 and the full architecture/product acceptance gates
remain open. Stop after qualification for the owner re-recording.
