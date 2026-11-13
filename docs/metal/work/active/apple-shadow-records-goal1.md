# Goal one — immutable native shadow pose records

Status: headless engineering milestone complete; owner runtime assessment failed,
[PPD-090 under investigation](g1-arrival-crash.md). Goal two stopped.
Branch: `codex/apple-shadow-records-g1`. Baseline:
`18e576cec0a05923632188ec8d7b2224657ffbfa` (accepted Alpha6 renderer retained).
Worktree: `/private/tmp/alchemy-apple-shadow-records-g1`.

Implementation/app source: `e21fe9c21832e5af88971ae0cb4f1ec2d892c36e`.
[Results and exact app/evidence paths](../../research/apple-shadow-records-goal1-results.md):
headless gate passes, 58.93%/72.13% lower CPU cost on repeated four/six-view
fixtures; single-use one-view cost +4.02% is open PPD-089. Build/static and
25 native cases/53,601 assertions pass. No viewer FPS claim; goal two not started.

## Objective and stopping point

Implement the smallest production ownership change in the
[Apple-first architecture](../../research/apple-first-renderer-architecture-2026-11-12.md):
publish an immutable, validated attachment-pose record once per actual producer
generation and reuse its native shared-memory slice across shadow batches/views.
Compare the full changed preparation and encoding path with the baseline, build
a separately named viewer, and assess continue/reject before goal two.

This is the pose-input portion of persistent render records. Geometry/material
records, detached jobs, GPU visibility and Metal 4 submission are subsequent
work, explicitly described in the [implementation route](../../research/apple-first-implementation-route.md).
The current viewer already uses native Metal and unified-memory draw pages.
No API translation, platform portability work or replacement scene engine.

Owner constraints: no viewer/utility launch, login, screenshot or interactive
profiling; no push/release; preserve accepted builds. Headless GPU execution and
builds are allowed. No in-world FPS or complete visual-parity claim follows from
this experiment. Owner assessment is the boundary before the next goal.

## Source contract before implementation

Original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`,
`LLVOAvatar::updateSkinInfoMatrixPalette`, computes a skin's packed attachment
palette once per avatar/frame. The fork retains that cache and packed rebase
origin plus joint rows. Shadow producers in `pipeline.cpp` borrow those bytes;
`ALMetalWorldGBufferRenderer::renderSpotShadow` repeatedly validates each batch
and the frame arena hashes/compares the bytes again at publication. Replacing
that borrowing with an owned immutable record is new native ownership, not a
change to animation or skinning mathematics.

System-avatar `LLViewerJointMesh::buildJointMatrixPalette` uses model-view state;
its output remains view-dependent and must not share an attachment record.
Draw order, coverage, weight/index ranges, joint limits, shadow queries, failure
transactions, native pipelines, shader constants and GPU skinning stay intact.
Frame pages cannot recycle until all referencing command completion tokens end,
including uncommitted/abandoned commands. A new pose must get a new record;
pointer identity of mutable input is insufficient. Arena mutation stays on the
existing render owner in goal one.

## Predeclared assessment

1. Correctness: byte-identical depth output for raw and immutable inputs across
   opaque/masked/PBR attachment cases, several poses/views and frame rollover;
   malformed/non-finite records rejected; view-dependent body path retained;
   command cancellation and multiple command lifetimes preserve published data.
2. Complete changed CPU cost: include record construction/finite scan/copy,
   batch preparation, arena publication and native command encoding, not only
   a cached lookup. Same optimized executable, baseline production path versus
   record path; counterbalanced repeated blocks, validation off for timing.
   Save raw samples and parameters. No measured scene distribution is invented.
3. Provisional mechanism gate: at least 10% median saving on declared repeated
   rigged multi-view fixtures, with no more than 5% median regression on a light
   one-view fixture. Report unique-pose/adverse cases even when they fail.
   Failure means revise/reject this slice before expanding it. Passing permits
   owner viewer assessment; it does not establish a whole-viewer improvement.
4. Delivery: doctor, isolated Release build, bundle verification, relevant native
   tests, harness and source review pass. Record exact SHA, commands and app path.
5. Viewer gate for goal two: owner compares this exact app with Alpha6 using the
   same region/camera/settings/cache state and checks attachments, body, shadows,
   turning/teleport and frame cadence. No automatic start of goal two.

## Work and evidence

Fixtures declared before timing (draw count is per view):

| Fixture | Views | Draws | Distinct poses | Joints |
|---|---:|---:|---:|---:|
| Light | 1 | 32 | 4 | 32 |
| Unique light | 1 | 32 | 32 | 32 |
| Repeated four-view | 4 | 256 | 8 | 64 |
| Repeated six-view | 6 | 1024 | 16 | 110 |
| Unique four-view | 4 | 256 | 256 | 64 |
| Unique six-view | 6 | 1024 | 1024 | 110 |

Each cycles opaque/legacy-mask/PBR-mask attachments and index widths, with new
poses/records each logical frame. Ten counterbalanced ABBA/BAAB blocks provide
300 measured frames per arm/fixture after warmup. The matched executable's raw
arm exercises the retained production path, including newly added null-record
branches; it is not a separately compiled untouched Alpha6 binary. Timed work
includes command creation, record sealing, batch construction/handle ownership,
frame-arena publication and complete shadow encoding. Unchanged Second Life
cull/animation/geometry traversal and matrix calculation are outside this
headless fixture. Commit/wait and retirement are reported separately. R32 shadow
depth is readable; do not claim private D32 attachment readback. Measure with
validation disabled after separate validation-enabled correctness checks.

The extra CPU-owned sealed copy is intentional in this first ownership seam;
creation, memory and unique-pose overhead must remain visible. It is not zero
copy. A later direct immutable shared-range producer must retain the same lease
and invalidation rules before eliminating that copy.

Local receipts: `/private/tmp/alchemy-apple-shadow-records-g1-evidence/`.
Root owns `pipeline.cpp`, CMake integration and canonical documentation.
Independent backend/tests/build work will receive exclusive file ownership.
Existing PPD records remain as recorded; new single-use cost is PPD-089. This
experiment claims no defect closure or owner acceptance.

| Work | State | Result / next action |
|---|---|---|
| Isolate branch and pin baseline | verified | Clean worktree at `18e576cec0`. |
| Inspect native producer/consumer and original invariant | verified | Mapping above; no skinning math change. |
| Implement producer records and completion-safe publication | verified | Actual three shadow producers, immutable identity map and every-command completion protection; source e21fe9c218. |
| Native parity/lifetime tests and full-cost paired benchmark | verified | Nine CTest entries/25cases/53,601assertions, 18 actual-source owner-cache checks, 3,600 timing samples; adverse PPD-089 retained. |
| Build separate app and assess | verified | G1/68987 Release/346ARM64/strict signatures/native imports pass; staged under the project artifacts directory. Owner scene/FPS assessment remains pending. |

Next action: owner compares exact staged G1/68987 with accepted Alpha6 under
matched settings/cache/scene. Stop here; do not start goal two or push until the
owner assesses this first milestone. The subsequent route is documented above.
