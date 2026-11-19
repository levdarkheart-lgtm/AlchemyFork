# Threading architecture assessment for Alchemy Metal

Alchemy's native Metal renderer has removed the graphics API barrier, but its
CPU frontend still combines world mutation, geometry compilation, visibility,
draw preparation and command encoding in a largely serial frame path. The
creator's warning applies to that remaining ownership and dependency model.
Aurora provides concrete examples of how to change it: owned frame handoff,
ordered resource updates, scoped parallel computation and persistent GPU scene
tables. These concepts can be implemented in this C++ and native Metal fork.

The architectural target is a shorter frame dependency chain and less repeated
work. Maximum thread count or adoption of Metal 4 alone is not that target.

This assessment pins our fork to `578191f6c66edc3e8f4823e4ffefb07ac29c2778`
and Aurora to `a6340067e020d003f8f8e32e0e8be5bd84ff9e33`, inspected on
2026-11-11. Other work's uncommitted renderer/documentation changes were
preserved and excluded from the source pin. No original-viewer comparison is
used. Source analysis establishes mechanisms; expected performance is evaluated
below as a model, not a measured speedup.

A follow-up diff review covered concurrent commits through
`cc78820286a656e069bd55737c6ba392cca19b59`: they add current-geometry qualification
for native vertex stores and previews. They do not change the traced frame,
worker or native rendering ownership structure. Line references retain the
original inspection pin.

The detailed source traces are in [Alchemy CPU dependencies](alchemy-cpu-dependencies-2026-11-11.md),
[Aurora render ownership](aurora-render-ownership-2026-11-11.md) and
[Aurora workers and streaming](aurora-workers-and-streaming-2026-11-11.md).
The [Apple Silicon rearchitecture plan](apple-silicon-rearchitecture-plan-2026-11-11.md)
turns the findings into concrete implementation changes at source checkpoint
`17574e43db3c`, including generation reuse safety and parallel GPU work.

## Apple Silicon architecture selection

The owner's target is macOS and Apple Silicon only. Aurora is a source of
techniques, not the architecture specification or a performance baseline.
Cross-platform ownership principles can transfer, while concrete scheduling,
memory, pass and resource-management decisions must target Apple hardware.

The recommended native design combines explicit CPU work ownership with an
Apple GPU pass schedule. A dedicated render thread is one possible scheduling
choice within that design; it is not the definition of optimization. CPU
preparation and encoding overlap, tile-memory traffic, GPU shading, streaming
and input latency must be assessed together.

| Design concern | Apple-only direction |
| --- | --- |
| CPU scheduling | Coarse independent work with explicit QoS and bounded admission; compare native Dispatch with the existing pools at the extracted boundary. Let macOS balance available performance and efficiency cores. Do not inherit Aurora's pool widths or spin thresholds. |
| Frame handoff | Compact retained inputs, immutable dynamic ranges and bounded backlog. Introduce a dedicated render owner only where it produces useful CPU overlap without increasing unacceptable input latency. |
| GPU passes | Merge compatible work and retain pixel data in tile memory where semantics permit. Parallel encoding must not multiply physical passes and load/store traffic. |
| Memory | Use frame-owned shared ranges for suitable CPU-produced data and private resources for suitable GPU-only data; preserve safe reuse. Avoid whole-scene copying and unnecessary staging. |
| Submission and binding | Evaluate Metal 4 command allocators, argument tables and streaming-maintained residency; retain explicit lifetime and dependency control. Do not reproduce wgpu-specific encoder-finalization machinery. |
| Scene preparation | Persistent compact records and dirty updates, with native compute/indirect execution where it removes more CPU work than the GPU/memory cost it adds. Preserve ordering and complete feature behavior. |

Apple explicitly recommends compatible pass coalescing to avoid tile-memory
loads and stores. Our four-color G-buffer and depth are consumed later by
lighting and other effects; they cannot simply become memoryless attachments.
That requires a dataflow/pass redesign that keeps the required later outputs.
[Apple GPU optimization](https://developer.apple.com/videos/play/wwdc2020/10632/).

Unified memory also makes total traffic a shared concern. Increasing CPU
workers or adding GPU culling can consume bandwidth that another frame stage
needs. Fewer copies and less repeated work can therefore outperform wider
parallel execution. Storage choice remains specific to access/lifetime needs.
[Bandwidth analysis](https://developer.apple.com/documentation/xcode/measuring-the-gpus-use-of-memory-bandwidth),
[Apple GPU storage modes](https://developer.apple.com/documentation/metal/choosing-a-resource-storage-mode-for-apple-gpus).

An all-Metal-4 API conversion is not a mechanical prerequisite. Current native
terrain-shadow and wave-water pipelines use tessellation
(`almetalworld.cpp:2941,4656,4707`); the Metal 4 render encoder changes that
interface to mesh techniques. An alternative must preserve tessellation behavior
and required M1 Max capabilities before replacing those passes. Conventional
Metal and Metal 4 interoperation remains native Apple rendering.
[Metal 4 core interfaces](https://developer.apple.com/documentation/metal/understanding-the-metal-4-core-api).

"Use all available hardware" means choosing useful CPU, GPU and supported
accelerator work that improves sustained frame time and responsiveness at equal
quality. Occupancy is diagnostic rather than the objective: reduced work can
produce better performance with lower utilization. Media acceleration belongs
to supported media work; Neural Engine use requires an actual supported ML
workload and a demonstrated benefit. Neither is a substitute for fixing the
viewer's CPU/GPU dependency chain.

These criteria govern the migration table below. Its row order does not put
Apple GPU pass optimization or useful Metal 4 changes behind a complete CPU
threading rewrite. They are separate workstreams selected by the measured
limiter. Apple scheduling guidance supports exposing work urgency and useful
granularity rather than reserving equal work for permanently assigned cores.
[Apple Silicon CPU tuning](https://developer.apple.com/documentation/apple-silicon/tuning-your-code-s-performance-for-apple-silicon),
[game job scheduling](https://developer.apple.com/videos/play/tech-talks/110147/).

## Current frame dependencies

The connected foreground path has this shape. GPU work already submitted can
overlap these CPU stages; the arrows describe CPU call and data dependencies.

```text
main thread
  input and mainloop callbacks
    -> simulator messages applied to live world
    -> object interpolation, avatar/controller updates, particles, camera
    -> geometry, native resource and mesh-result publication
    -> cull / visibility / LOD / sort / further geometry rebuilding
    -> native batch construction
    -> Metal encoding, world / HUD / UI composition and submission
    -> pacing/slack callbacks and frame completion bookkeeping
    -> ordinary reflection-probe and snapshot follow-up

existing workers: transport, fetch/cache, decode, mesh work, native image uploads,
                 native pipeline creation and other independent jobs
GPU:             executes previously submitted native commands asynchronously
```

`LLAppViewer::doFrame()` calls `idle()` then `display()` at
`indra/newview/llappviewer.cpp:1558,1588`. `display()` directly performs geometry
preparation at `llviewerdisplay.cpp:1883`, culling at `:1939`, sorting at `:2036`,
native geometry at `:2168` and deferred lighting at `:2186`. This concentrates
both preparation and encoding on the caller; treating all of `display()` as
GPU execution would misdiagnose the architecture.

Several apparently separable stages actually perform side effects:

| Boundary | Current source behavior | Consequence |
| --- | --- | --- |
| Object updates | Seat/object interpolation precedes avatar updates; avatar processing can move the seat drawable. `llviewerobjectlist.cpp:1005` | Dependencies must survive parallelization. |
| Geometry compilation | `LLVolumeGeometryManager` shares static face scratch arrays. `llvovolume.cpp:5419` | Even different spatial groups cannot safely compile through these methods concurrently. |
| Geometry publication | `genDrawInfo()` assigns buffers/offsets to live faces and changes group draw maps. `llvovolume.cpp:6611` | Worker calculation needs detached output and a publication step. |
| Culling | `updateCull()` and `markNotCulled()` change visibility, distances, occlusion state and shared result lists. `pipeline.cpp:20972,21062` | A parallel traversal needs independent results and stable spatial inputs. |
| Sorting | `postSort()` rebuilds geometry, publishes mesh completions, merges maps, sorts transparency, processes selection and flushes buffers. `pipeline.cpp:22146` | Moving only the sorting algorithm leaves most surrounding work serial. |
| Avatar evaluation | Motion loading, virtual callbacks, pose blending, joint writes and effects are coupled. `llmotioncontroller.cpp:1007` | One worker per live avatar is unsafe without an extraction boundary. |

Some getters are also writers: joint world-matrix access can lazily update
parents and dirty flags (`lljoint.cpp:977`). A render worker reading a live
skeleton concurrently with animation is not necessarily performing read-only
work. Existing object reference counts are non-atomic; converting them to
atomic counts would address only one lifetime mechanism, not these mutations.

## What Aurora actually implements

Aurora's current macOS source selects **wgpu's native Metal backend**, with
Vulkan selected on other platforms. It is not currently a MoltenVK renderer on
macOS. The architectural lessons do not require adopting wgpu, Vulkan or Rust.
[Backend selection](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-render/src/lib.rs#L132-L137).

### An owned boundary between scene preparation and rendering

The ordinary application calls a renderer frontend that builds an owned
`FramePacket`; a dedicated backend thread consumes it. The main thread can
prepare N+1 while the render thread encodes and submits N. This is additional
CPU-to-CPU overlap beyond the CPU-to-GPU overlap Alchemy already has.

The packet contains camera/environment/settings, reference-counted GPU handles,
dirty update bytes, CPU lists for special draws, UI shapes and texture changes,
surface state and readback requests. It does not clone the whole simulation
world. Returned packet storage is reused. CPU lists and dirty bytes still have
copy costs.
[Packet](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-render/src/packet.rs#L69-L138),
[actual construction and handoff](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-render/src/main_thread.rs#L1391-L1469).

Only one backend job is outstanding; another frame does not accumulate behind
it. Main-thread preparation may already have produced the following frame,
but handoff waits until the backend becomes free. That bounds CPU render
backlog. Handoff completion means encoding/submission/presentation-call work
returned; actual GPU completion is tracked separately. Neither is proof of
physical display latency.
[Rendezvous](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-render/src/render_thread.rs#L95-L126),
[worker ordering](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-render/src/render_thread.rs#L392-L410).

### Resource updates remain attached to their frame

Retaining a buffer handle does not freeze the bytes in that buffer. Aurora
therefore gives its backend exclusive queue ownership. The main thread records
buffer/texture writes and intermediate copy submissions in an ordered journal;
the render thread replays N's journal before N's draws. This prevents N+1's
updates from entering N's submission. GPU buffer growth also retains its place
relative to earlier and later writes.
[Journal operations](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-render/src/writes.rs#L45-L125),
[actual replay](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-render/src/renderer.rs#L6434-L6447).

For native Metal this principle needs care: queue order cannot protect a shared
buffer from an immediate CPU `memcpy`. A protected range/version, an ordered GPU
copy, or completion before reuse is required. Alchemy already versions native
vertex stores and retires them against completion. That infrastructure can be
extended; a wholesale transplant of wgpu's write API is unnecessary.

### Parallel calculations retain explicit joins and publication

Aurora's frame helpers take chunks from a shared index while the calling thread
does useful work immediately. Helpers arriving after the work scope closes
do not touch its borrowed inputs. Helpers that entered must finish before the
scope returns. The implementation is synchronous fork/join, with delicate
lifetime handling; it does not permit arbitrary late world mutations.
[Worker scope](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/frame_par.rs#L90-L221).

Object planning happens one parent-depth level at a time: workers read the
world and generate plans, then the caller publishes that level's transforms.
Avatar jobs receive separate controller/work records and disjoint palette
ranges; setup and results remain serialized. Draw-list jobs produce local
vectors, merge in input order, then sort transparency on the caller. This is
the concrete pattern Alchemy needs: compute independently, join dependencies,
publish coherently.
[Object planning](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L2928-L3070),
[pose jobs](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L5626-L5777),
[draw-list merge](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L5128-L5206).

### Persistent scene tables eliminate repeated CPU work

Aurora maintains face/object/draw records and updates dirty ranges. Compute
classifies faces for several views, scans counts and scatters compact indirect
commands. Stable scatter preserves record order, avoiding nondeterministic
coplanar draw winners. Indirect GPU counts avoid reading visibility counts back
to the CPU. Transparency and several special passes still use CPU lists.
[Actual compute and draw path](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-render/src/renderer.rs#L7578-L7647),
[stable scatter](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-render/src/shaders/cull.wgsl#L558-L628).

This is capability- and scene-size-gated. Source alone does not prove the path
is enabled on an M1 Max. The concept transfers to native Metal compute and
indirect commands; wgpu's exact indirect-count interface does not.
[Feature and size checks](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-render/src/gpu_cull.rs#L948-L963).

### Designs to study without copying their assumptions

Aurora finishes three wgpu encoders concurrently using two persistent helpers.
This targets wgpu validation/tracking work; native Metal has different costs.
Its texture pages likewise address wgpu binding/tracking overhead, besides
providing material indirection. These are not reasons to add equivalent native
machinery without cost evidence.
[Encoder helpers](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-render/src/renderer.rs#L9584-L9601).

Two substantial CPU pools are sized independently, and many asset/network/result
channels are unbounded. Explicit foreground/background priority changes are
inactive on macOS. Its busy-wait thresholds, Windows allocator observations
and worker counts are not Apple Silicon tuning results. The useful design is
separation of frame work from background work, with explicit outputs and joins.
[Pool and channel setup](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/jobs.rs#L295-L357),
[priority implementation](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-render/src/frame_priority.rs#L62-L90).

## Native ownership changes required in our fork

Our current native view preparation swaps the active main/probe capture owner
(`pipeline.cpp:1684,1927`), reads a current camera (`:11554`), appends to one
batch vector (`:12331`) and borrows avatar palette memory (`:12417`).
`ALMetalWorldGBufferScene` receives pointer/count pairs into those arrays
(`:16853`). Moving that small scene structure to a worker would not move
ownership of its contents.

An asynchronous renderer introduces a lifetime interval before encoding:

```text
world/resource publication -> CPU packet waiting -> CPU encoding
                          -> committed GPU reads -> resource retirement
```

Existing GPU retention begins too late to protect every raw pointer in a
packet waiting for encoding. A valid design must cover both the CPU packet's
last use and the GPU's last use. Specific requirements are:

- Retain the exact vertex/index/texture generation before handing over a packet.
  Publishing a new image can replace the existing C++ texture wrapper
  (`llimagegl.cpp:2214`); native retention after encoding cannot repair an earlier
  dangling wrapper pointer.
- Capture immutable palettes. `updateSkinInfoMatrixPalette()` already caches by
  avatar/skin and frame (`llvoavatar.cpp:10858`); repeated calls do not prove
  repeated palette calculation. The async issue is next-frame mutation of its
  cached arrays while an earlier packet still refers to them.
- Preserve coherent texture publication, including alpha classification and
  material-pass membership, rather than pinning pixels independently of the
  facts used to classify the draw.
- Keep constants and allocator state under explicit ownership. Published
  `ALMetalWorldFrameData` ranges are GPU-completion-safe, while its mutable page
  lists/hash table are not concurrent CPU allocators (`almetalworld.cpp:1321,1419`).
  One render owner or separate task arenas can avoid a global per-draw mutex.
- Attach frame-end lifetime records to actual submission. `ALFrameFences`
  records the scheduler's latest committed serial (`alframefences.cpp:225`).
  Advancing it on the main thread before an asynchronous packet submits could
  incorrectly make resources appear reusable.
- Keep surface/size, device, region origin, histories and query results tied to
  the generation that produced them. Resize, teleport, failed publication and
  skipped packets must not publish stale state or advance histories incorrectly.

UI belongs in this boundary too. Our retained UI traversal calls native drawing
while resolving live blend/depth/texture state. A render worker needs an owned
paint-command stream and font/texture leases; it should not traverse live
widgets. HUD depth brackets, selection, shadows, mirrors, snapshots and
appearance bake/readback consumers impose ordering requirements. Aurora's egui
packet demonstrates the principle but cannot replace that work.

Exclusive queue ownership is a simple implementation option, not a universal
Metal requirement. Our existing image worker already commits immutable upload
generations before main-thread publication. It can remain a separate producer
if the new packet/resource dependency contract proves ordering and lifetime.
Forcing all uploads through a new central queue is not justified merely because
Aurora does so for wgpu writes.

## Data layout and repeated preparation

A compiled CPU-only layout probe against our pinned headers reported:

| Native type | Bytes |
| --- | ---: |
| `ALMetalWorldSimpleBatch` | 3,608 |
| `ALMetalWorldSpotShadowBatch` | 992 |
| `ALMetalWorldGBufferScene` | 152 |
| `ALMetalWorldReflectionProbeConstants` | 49,216 |

These are structure sizes, not total live memory or measured frame traffic.
For scale, an illustrative array of 10,000 general batches occupies about
34.4 MiB before external palettes and allocations. This is not an observed
per-frame batch population; total draw telemetry includes other batch types.
Optional inline material arrays also occupy storage even when inactive.

Consequently a full copy of today's capture arrays per view is a poor default
design. Moving already owned vectors can transfer their storage cheaply, but
does not fix external pointer lifetime or repeated rebuilding. A longer-term
design can keep persistent object/material/geometry IDs, dirty generations,
frame palette ranges and compact per-view references. Shared view constants
then replace some per-draw/view reconstruction, subject to shader precision,
normal-transform and region-origin invariants.

Alchemy's shadow ICB path is currently prepared on the CPU
(`almetalshadowindirect.mm:343`). It does not already implement Aurora's
persistent multi-view GPU classifier. Existing native water depth-pyramid and
wave-occlusion compute (`almetalworld.cpp:17100`) is a narrower example of GPU
visibility work, not scene-wide culling. Expanding that model requires persistent
bounds/face records, material and resource tables, output capacity policies and
stable command ordering. It can remove CPU work; it can also add GPU/memory work
in an already GPU-limited scene.

## Metal 4 and Apple Silicon

A device-only probe on this host returned Apple M1 Max, unified memory,
Apple7 support, Metal4-family support, and successful creation of a Metal 4
queue and allocator. It submitted no GPU work and opened no window. Thus
Metal 4 core availability on this machine is verified; performance and
individual advanced features are separate questions.

Our backend currently creates a conventional `MTLCommandQueue`
(`almetaldevice.mm:419`). Metal 4 offers reusable command memory, argument tables,
grouped multi-thread command submission and explicit synchronization. It also
makes application synchronization responsibility more explicit, including
untracked resources. These APIs fit an explicit dependency/ownership design;
they do not make current camera selectors, shared vectors or live skeletons
safe to access concurrently.
[Apple core API](https://developer.apple.com/documentation/metal/understanding-the-metal-4-core-api),
[encoding model](https://developer.apple.com/videos/play/wwdc2025/254/).

Apple's scheduling guidance supports substantial jobs, useful caller work,
bounded worker populations and fewer synchronization points. Jobs queued behind
asset maintenance, tiny tasks and oversubscribed pools can lose the potential
gain. Specific core placement, QoS, energy or thermal benefits require runtime
evidence; neither thread count nor the Metal version measures hardware use.
[Apple CPU scheduling](https://developer.apple.com/videos/play/tech-talks/110147/).

## Scheduling problems beyond rendering

The shipped General pool configuration is one worker, despite a constructor
fallback of three. Pipeline creation, audio decode, water bakes, material
parsing and filesystem maintenance share it. An expensive task can delay
unrelated work. New frame-deadline tasks should not automatically join that
FIFO. ImageDecode uses `hardware_concurrency - 6`, clamped to 2 through 16;
independent pools do not establish a shared CPU admission budget.

The nominal mainloop callback allowance is 1 ms and mesh notification allowance
2 ms. Each callable/item runs to completion, so one large operation can exceed
the allowance. Simulator message processing starts with a 20 ms frame budget
and can expand under backlog. These policies can produce tail latency even
when asset calculations run on workers. Faster decoding may increase main-thread
publication pressure rather than reduce it.

Our mainloop also executes some callbacks in predicted GPU pacing slack.
Removing or relocating this existing overlap without measuring it could regress
performance. Queue age, task size, runnable time, byte backlog and publication
cost are necessary observations, alongside worker execution time.

## Performance model and priorities

Let U be main-thread update/preparation, E CPU encoding, G GPU work on the
critical path, and H added packet/handoff work. With sufficient steady overlap,
an idealized current frame-period/throughput bound is `max(U + E, G)`. A correctly
decoupled design can approach `max(U + H, E, G)`, subject to dependencies,
presentation, memory contention and other waits. This is a lower-bound model,
not an exact formula for every current frame. It does not predict input-to-display
latency: pipelining can increase latency even when throughput improves.

Two illustrative cases show why stage attribution changes the decision:

| Assumed stage times | Current bound | Decoupled bound with H=1 ms | Implication |
| --- | ---: | ---: | --- |
| U=12 ms, E=10 ms, G=8 ms | 22 ms | 13 ms | Useful CPU overlap is possible. |
| U=12 ms, E=10 ms, G=30 ms | 30 ms | 30 ms | CPU overlap alone cannot improve throughput. |

These inputs are hypothetical, not Alchemy measurements. Parallel preparation
has its own bound: serial work plus parallel work divided across effective
workers, plus snapshot/scheduling/join/publication. The largest task and genuine
dependency chain matter more than average worker throughput. Reducing repeated
work can improve both serial and parallel designs.

Existing fork investigations also constrain the decision. An earlier native
whole-shadow parallel encoding experiment kept preparation on the caller and
did not establish a general benefit. It should not be revived unchanged.
Historical runtime investigations showed substantial CPU and GPU work; they
do not classify this current source as universally CPU-bound. Current
multi-scene performance remains unmeasured by this assessment.
[Earlier experiment](../state/archive/CURRENT-parallel-shadow-encoding-2026-11-10.md),
[measurement limits](../state/archive/CURRENT-20fps-owner-stop-2026-11-10.md).

## Concrete migration sequence

The following are proposed changes, not implemented symbols. They divide the
architectural work into reviewable checkpoints while retaining native Metal.

| Stage | Concrete work | Completion evidence |
| --- | --- | --- |
| 1. Measure the current dependency chain | Distinguish world/pose/geometry/publication/cull/capture/encode CPU time from resource, drawable and pacing waits. Label main, hero, ordinary-probe and shadow work separately. Record actual pool widths and queue age. | A synchronized frame/thread/GPU timeline explains the slow frames and identifies removable or parallelizable work. |
| 2. Own native draw inputs | Explicit view/pass values; retained resource generations; immutable palette ranges; owned batch storage; defined frame/scene/device identity. Execute the same packet on the current thread first. | Same native output/order, including failed uploads, deletion, resize, HUD, mirrors and snapshots; packet survives source-vector destruction. |
| 3. Extract one expensive preparation boundary | Choose detached group geometry, restricted remote-avatar pose math or independent native batch preparation from measured cost. Use task-local scratch/output and ordered publication. | End-to-end snapshot + task + join + publication improves; late results cannot clear newer dirty state; visible update latency and memory stay bounded. |
| 4. Overlap main and native render CPU stages | Render owner consumes complete owned packets while main prepares the next. Bound handoff and memory; return ticketed results; move lifetime markers to actual submission. Record UI commands instead of reading live widgets. | Lower p50/p95 frame time in relevant scenes without worse input latency, output quality, resource lifetime or backlog. |
| 5. Reduce repeated scene submission | Persistent compact opaque/masked records; dirty updates; native material indirection; carefully selected GPU culling/ICB paths. Preserve special-pass order and feature behavior. | Fewer actual CPU preparations/bindings and lower complete frame cost, with conservative visibility/order and no quality reduction. |
| 6. Adopt Metal 4 where its costs justify it | Per-owner allocators/argument tables and grouped command submission with explicit resource barriers and retirement. | Native current-API comparison isolates a benefit; API/GPU validation and lifecycle tests pass. |

Stages 2 and 3 can be developed incrementally. A full-frame CPU pipeline in
stage 4 requires the complete world/HUD/UI/resource boundary; moving a small
G-buffer call while immediately waiting for it may create little overlap.
Stages 5 and 6 are independent optimization directions and need not wait for
every avatar or geometry algorithm to become parallel. Conversely, a Metal 4
API migration is not a prerequisite for proving the ownership boundary.

The first code boundary worth preparing is explicit native view and draw-input
ownership: it removes a concrete obstacle shared by parallel preparation,
render-thread overlap and persistent records. The first performance change
should then target the measured expensive producer, rather than assume the
last encoding loop is the dominant stage.

## Measurements and validation that decide adoption

Use frame and view IDs to correlate: active main CPU, native capture, palette
and constant publication, encoding/commit, packet handoff, drawable acquisition,
resource-cap waits, pacing and GPU intervals. Count packet/dirty bytes, source
and accepted draw records, largest task, join wait, queue age and stale results.
Aggregate locally per task/frame; avoid adding per-draw atomic work to ordinary
rendering. The existing asynchronous `metal_gpu_ms` completion-window sum is
not a per-frame GPU critical-path duration.

CPU fixtures should stress source destruction after sealing, vertex/texture
replacement, palette update in N+1 while N waits, deletion and identity reuse,
teleport, resize, skipped/failed frames and shutdown. Thread-sanitized CPU-only
ownership tests can complement actual native validation. GPU tests must retain
alpha/query/HUD ordering, all required views and completion-safe reuse. Run
normal performance captures separately from validation/capture tools, using the
repository's repeated warm-up and at least 300-frame protocol. Test quiet,
avatar-heavy, PBR/reflection and motion/streaming workloads.

Aurora's source is evidence of an implemented architecture, not a matched
Alchemy performance result. Its documented macOS defaults use logical world
resolution and a lighter profile without shadows/AO. Actual render pixels and
features must match before attributing an FPS difference to threading or Rust.
[Mac defaults](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/docs/MACOS.md#L10-L27).

## Local evidence

The current fork was extracted with `git archive` into
`/private/tmp/alchemy-threading-source-578191f6c6`; the Aurora public source is at
`/private/tmp/aurora-threading-analysis-20261111`. The source hashes and probe
sources/results are retained under ignored
`artifacts/metal/threading-analysis-20261111/`.

- Host doctor completed successfully: Apple M1 Max, 64 GiB, macOS 26.5.2,
  Xcode 26.6. Initial sandbox hardware queries were denied; the read-only
  diagnostic retry completed outside that restriction.
- CPU layout probe: `xcrun clang++ -std=c++23 -O2 -I indra/almetal` against
  the listed probe source; compilation and execution exited 0. It measures
  C++ layouts, not allocation traffic or rendering performance.
- Device-only Metal 4 probe: Objective-C++ with Foundation/Metal, ARC, C++23;
  compilation and execution exited 0. It queried the device and created a
  queue/allocator only. This is capability evidence, not GPU execution proof.
- No viewer was built/launched, no account was accessed and no performance
  campaign was resumed for this analysis. Existing feature/performance gates
  and ongoing implementation work remain separate.
