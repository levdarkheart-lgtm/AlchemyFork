# Apple Silicon rearchitecture plan

Rearchitect the existing Alchemy Metal fork by separating authoritative world
updates, independent preparation, native render inputs and GPU execution.
Develop Apple GPU pass and memory improvements alongside this CPU work. Preserve
the functioning viewer through small changes with explicit correctness and
performance evidence.

This is a proposed implementation sequence based on source checkpoint
`17574e43db3cf381f1b4213c11590285a9175cea`, 2026-11-11. No implementation or
performance gain is claimed by the plan. The [architecture assessment](threading-architecture-2026-11-11.md)
and its source appendices supply the diagnosis. The [product contract](../PRODUCT.md)
and [performance contract](../PERFORMANCE.md) remain authoritative.

## Target ownership

```text
main application owner
  input, network-event application, world/inventory/UI state
  animation and geometry lifecycle, scene publication
      |
      +--> independent preparation jobs
      |      read-only inputs, private output, bounded work
      |    < results joined/published at defined dependencies
      |
      v
persistent native scene records and exact resource generations
      |
      v
owned native frame/view inputs and ordered UI paint commands
      |
      v
native render owner
  explicit Metal pass schedule, encoding, submission, GPU histories
      |
      v
Apple GPU
  tile-aware rendering, suitable compute, native indirect submission

actual GPU completion and ticketed results return asynchronously
```

These are ownership boundaries, not one permanent thread per box. Independent
jobs can initially join within the main frame. The render owner can initially
execute on the main thread. Thread overlap is introduced only after the data
boundary is safe and the expected overlap is useful.

World, inventory, protocol behavior, audio/voice and widget logic remain with
their established owners. New rendering work does not require a replacement UI
framework, general ECS, cross-platform graphics abstraction or engine-wide task
framework. Existing Metal image uploads and resource retirement are foundations.

## Two coordinated workstreams

**CPU and scene ownership:** eliminate ambient current-view/pass reads from
detached work, retain exact resource generations, extract useful calculations,
reduce repeated preparation and eventually overlap CPU frame stages.

**Apple GPU execution:** map real producer/consumer dependencies, reduce
attachment traffic and pass boundaries, improve material/resource submission,
and adopt native Metal 4 mechanisms where they benefit the workload.

Both workstreams use the same frame/view identity and measurement vocabulary.
They can run independently in separate files/worktrees. One integrator owns
`pipeline.cpp`, `llrender.cpp`, shared shaders and final integration. Do not
combine a new scheduler, changed shading and altered pass layout in one
performance experiment: attribution and regressions would become ambiguous.

## Initial changes

### Change 1 Establish the exact frame cost and dependency census

Extend the existing opt-in instrumentation in `almetalbenchmark.*` and native
diagnostics rather than create another telemetry system. Distinguish:

- World/network callback application, pose evaluation, geometry preparation and
  publication, culling/sorting, native record construction and encoding.
- Main, hero, ordinary-probe, shadow and HUD/UI work.
- Active CPU work from queue/resource/drawable/pacing waits.
- Packet/constant/palette bytes, record counts, queue age, longest preparation
  task and effective pool widths.

Use existing Tracy zones and native command completion/timing tickets. Aggregate
locally per stage; avoid normal-rendering per-draw atomics. Keep the asynchronous
command-duration sum separate from whole-frame GPU timing. Some M1 Max counters
are unavailable; do not fill missing quantities with estimates presented as
measurements.

For each important attachment, record its writer, later readers, required
preservation, format and existing load/store actions. `ALRenderResourcePlan`
describes resources and allocation intent; it is not already a complete GPU
execution dependency graph. Add the missing dataflow description without
introducing a general-purpose render-graph framework.

**Acceptance:** the exact source/build can explain where a slow frame's time
and waits occur. Instrumentation off preserves ordinary behavior; instrumentation
on does not add GPU waits or artificial pass splits. Warm/cold and validation
runs remain separate. Runtime profiling remains a distinct execution activity;
this planning request does not restart the previously stopped campaign.

### Change 2 Separate a small native packet from the live frontend

Start at `captureMetalWorldGBufferBatch()` in
`pipeline_buffer_publication_native.inl`, the current source owner. Pilot only
the static, opaque, simple, nonrigged subset. This bounds the initial contract;
other features continue through the same existing native renderer.

The owner first runs `prepareMetalBuffers(true)` and resolves current texture,
material, model and view state. That method can flush and publish native stores;
it must stay outside the pure worker calculation. Give the packet explicit
camera/pass values, moved record storage, exact resource-generation leases and
copied constants. Avoid references to live `LLDrawInfo`, `LLFace`, viewer objects
or the current-camera selector in its executor.

Run the packet inline first using the existing native encoder. Preserve current
draw order, query insertion points and the whole capture's failure admission.
Use stable source-order output positions rather than concurrent vector append.
Do not copy the entire world or every large batch vector just to make it owned.

**Acceptance:** the packet can execute after its producer vectors and temporary
inputs are destroyed; output and transaction failure behavior match. The packet
executor does not rediscover ambient camera/pass/world state. This establishes
an ownership seam, not an FPS claim.

### Change 3 Protect resource generations before encoding

Add packet pins to the existing native storage-generation/retirement machinery.
Keep the established unique resource owners and typed wrappers; do not convert
the live viewer object graph to global shared ownership.

The reuse rule becomes:

```text
storage may be recycled only when
  no queued/encoding CPU packet can still use that generation
  AND no encoded, uncommitted command can still submit a read
  AND every submitted GPU reader of that generation has completed
```

Retaining an `MTLBuffer` alone is insufficient: `LLVertexBuffer` currently takes
completed stores back from its generation pool and can write new contents into
them. A packet waiting before encoding must prevent that reuse. Deduplicate
leases by generation per packet instead of adding shared-owner operations to
every draw. Cancellation releases CPU pins. Encoding transfers protection to
the pending command before packet pins are released; commit binds that ownership
to actual GPU completion, while abandonment releases it. Alternatively keep the
packet's protection until a commit receipt exists. An encoded-but-uncommitted
command must never look complete or make storage reusable.

Texture generations need equivalent protection through publication/replacement,
with coherent alpha/material classification. In-place dynamic/media updates
need their own ordered-update or versioning policy. Frame-owned palette arrays
must survive N+1 animation updates. Existing cached palette computation should
remain cached.

**Acceptance:** a delayed packet survives buffer reuse pressure, texture
replacement, palette changes, cancellation and object deletion; no storage is
recycled early. Submitted and abandoned commands retire correctly. No global
per-draw mutex or blanket synchronous GPU drain is introduced.

Changes 2 and 3 form one correctness milestone. Change 2's output is not admitted
to asynchronous execution until generation protection is complete.

### Change 4 Parallelize one measured preparation phase

Use the pilot input records for a pure batch-building function. Give workers
preassigned output ranges and task-local status/counters. The owner merges
status and publishes only a complete transaction. Start with substantial chunks;
short inputs use the same calculation inline.

Use a bounded native Dispatch/QoS execution path for the experiment, comparing
complete stage cost with inline execution. Existing pools can remain where they
already work. Do not send frame-deadline jobs behind General's pipeline compile,
audio decode and maintenance FIFO, or create a full-sized pool per subsystem.
Let macOS choose performance/efficiency cores; express useful work and urgency.

If measurements instead identify geometry compilation or remote pose math as
the dominant producer, the packet/lifetime seam remains useful, but the first
parallel performance change should target that producer:

- Geometry: replace shared static face scratch with job-owned scratch; capture
  immutable volume/face/material input; produce detached bytes/draw maps; publish
  only if group/material generations are still current. Scratch privatization
  alone does not make live face publication safe.
- Animation: keep asset lifecycle, arbitrary callbacks, seat/root changes and
  local camera dependencies on the owner; extract eligible remote pose math
  into independent joint arrays, then publish before attachment/bounds consumers.

**Acceptance:** snapshot, dispatch, work, join, merge and publication together
improve the targeted stage and relevant frame tails. Preserve visible update
latency and numerical/ordering contracts. Track retained bytes, queue age and
stale-result rate; bound jobs and bytes and coalesce obsolete generations.

## Apple GPU work that proceeds alongside the CPU changes

### Change 5 Optimize one proven pass or memory cost

Use the attachment census to choose a compatible pass pair or unnecessary
copy/store. Preserve all required shading and later consumers. The four-color
G-buffer is sampled later; changing those attachments to memoryless without
restructuring their consumers is invalid. Screen-space effects, histories, HUD
depth and snapshot consumers constrain fusion.

Investigate native tile-local lighting/composition where dataflow permits.
Physical pass coalescing, appropriate storage modes and reduced attachment
traffic are Apple-specific priorities. Avoid adding a physical pass per worker.
Parallel CPU encoding and a low-bandwidth GPU pass schedule must coexist.
[Apple GPU guidance](https://developer.apple.com/videos/play/wwdc2020/10632/).

**Acceptance:** native validation and required pixel/order behavior hold; actual
GPU/CPU frame measurements improve. Fewer encoders alone are not proof, and a
microbenchmark gain is not automatically a viewer gain.

### Change 6 Introduce compact persistent native scene records

Begin with stable opaque geometry. Assign stable generation-qualified geometry,
material and instance identities; update changed ranges. Keep transforms,
palettes and view constants separate from persistent material data. Build
per-view references without reconstructing every large general batch.

Do this first with existing native CPU visibility decisions and draws. Then
evaluate compute visibility/compaction and ICB execution for a bounded class.
Preserve stable draw order where coplanar output depends on it, conservative
bounds, selection restrictions and correct pass membership. Transparency, HUD,
water and special passes retain explicit order and may use different strategies.
The old path is replaced by validated native implementations incrementally;
this does not introduce a second graphics backend.

**Acceptance:** dirty updates reduce actual record construction/binding work and
total traffic. GPU culling is adopted only when its compute, memory and table
maintenance cost improves relevant scenes. No lost faces when capacity is
exhausted; no weaker reflection/shadow cadence or quality.

### Change 7 Adopt useful Metal 4 command and binding mechanisms

Evaluate bounded per-owner command allocator reuse, native argument tables,
streaming-maintained residency and grouped command submission in the owned
native boundary. Manage explicit resource barriers, native object lifetime and
allocator reset after actual GPU completion. Reuse the submission/lifetime
contract established above.

This can begin once the required ownership/dependency boundary exists; it does
not wait for every scene job to be parallel. Current terrain/water tessellation
requires separate treatment because Metal 4 changes that interface. Preserve
native supported paths and M1 Max behavior until an alternative proves parity
and benefit. All involved APIs remain native Metal.
[Metal 4 command design](https://developer.apple.com/videos/play/wwdc2025/254/).

**Acceptance:** the same workload has lower complete command-management cost
without resource hazards, growth or feature loss. The API version itself is not
an acceptance result.

## Whole-frame overlap after the ownership boundary is complete

### Change 8 Give rendering a bounded asynchronous owner

Expand the owned packet to all required world/shadow/probe/post/HUD/UI passes
before claiming whole-frame decoupling. Record UI paint operations and retained
font/texture inputs on the main owner; do not move live widget traversal to the
render thread. Special synchronous snapshot/appearance consumers need explicit
ordered flush/result boundaries.

Then run native encoding/submission on a dedicated owner while the main thread
prepares the next frame. Begin with one handed-over CPU packet and no growing
queue behind it. Separate main/render handoff completion from actual GPU
completion and presentation. Frame-end markers must follow actual committed
commands, not the main thread's preparation clock.

Keep the image upload worker as an independent producer while upload and render
commands use the same ordered queue and its commit-before-publication and
generation contract remains sufficient. If rendering moves to a separate Metal 4
queue, carry a producer event/value and encode a consumer wait, or establish
another proved GPU dependency; commit on the old queue alone is insufficient.
Require explicit sequencing for writes into storage reused across frames.
Exclusive queue ownership is an option, not an automatic requirement for native
Metal.

Return ticketed results with frame, scene/device generation and request identity.
Test resize, region-origin change/teleport, skipped packets, failed publication,
selection and delayed picking, shader reload, device/surface loss and shutdown.
Failure must not publish partial state or advance GPU histories incorrectly.

**Acceptance:** CPU stages overlap in a trace and improve relevant throughput
and p95 frame time without unacceptable input/camera latency, memory growth or
stability regressions. If extra handoff hurts a class of workloads, retain the
same native executor's appropriate scheduling strategy; do not ship a second
renderer or compromise quality to manufacture a gain.

## First implementation batch and source ownership

The first batch should contain the instrumentation/dependency census, a small
explicit native view/packet seam and generation-pin contracts. Run inline and
prove lifecycle correctness before worker scheduling. Keep the first performance
change independent from a shader or physical pass redesign.

| Owned area | Existing source boundary | First deliverable |
| --- | --- | --- |
| Frame evidence | `newview/almetalbenchmark.*`, existing Tracy/native diagnostics | Per-stage/view costs, waits and bytes with source/build identity. |
| Packet producer | `newview/pipeline_buffer_publication_native.inl`, `pipeline.cpp` | Explicit qualified inputs and static opaque pilot; current failure admission preserved. |
| Packet executor | `almetal/almetalworld.h`, `almetalworld.cpp` | Owned input consumed inline by the current native renderer. |
| Generation safety | `llrender/llvertexbuffer.*`, `llimagegl.*`, `almetal/almetalresource.*` | CPU pins plus GPU completion protect actual generation reuse. |
| Apple GPU pass work | `almetal/almetalworld.cpp`, native shaders and resource descriptions | Real reader/writer/lifetime census, then one isolated measured optimization. |

These are coordinated edit sets, not permission for concurrent edits to the same
files. Use isolated worktrees for independent changes and a single integrator
for shared producers, renderer state, shader changes and canonical documentation.
Base integration on the completed source checkpoint rather than interleaving
with another task's resource-publication fixes.

## Adoption criteria

Use M1 Max as the first physical target, with runtime feature checks for later
Apple Silicon. Preserve actual pixels, resolution, render settings, all required
views, avatar density and warm/cold cache state. Evaluate quiet, avatar-heavy,
PBR/reflection and motion/streaming scenes. Follow existing repeated warm-up and
at least 300-frame comparison requirements; measure validation separately.

Run appropriate native tests and actual application builds/verification after
implementation changes. Use CPU sanitizer tests for detached ownership and
generation races, and Metal API/GPU validation for command/resource correctness.
Integration-only fixtures are insufficient for visible feature parity or an
end-to-end performance claim.

A structural change may be necessary groundwork without an immediate speedup;
label it accordingly. A performance change is adopted for lower measured frame
cost and preserved responsiveness/quality. Neither high utilization nor one
isolated fast shader establishes that result. Avoid time/FPS promises until the
baseline identifies the work and a prototype measures it.
