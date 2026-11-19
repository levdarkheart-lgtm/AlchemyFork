# Current Alchemy Metal CPU threading and dependency analysis

Analysis date: 2026-11-11. Source pin: `578191f6c66edc3e8f4823e4ffefb07ac29c2778`. Every source claim below refers to this committed revision, extracted under `/private/tmp/alchemy-threading-source-578191f6c6` (an independently extracted subset also exists under `/private/tmp/alchemy-cpu-pin578191f6`). Other ongoing work has dirty source and documentation; those changes were excluded. This is read-only source analysis. No builds, launches, runtime sampling, or original-viewer comparison were performed. Source proves dependencies and mechanisms; it does not prove which mechanism currently dominates frame time.

## 1. What the central frame actually does

`LLAppViewer::doFrame()` is at `indra/newview/llappviewer.cpp:1430`. It processes native events/input, posts the canonical mainloop event, gives the fiber scheduler an opportunity, scans movement controls, executes `idle()` (`:1558`), executes `display()` (`:1588`), calls `display_end_frame()` (`:1590`), performs reflection/snapshot follow-up, then services asset/file-thread bookkeeping and mesh-upload startup. This is a serial main-loop invocation, even though already-submitted GPU work and independent worker threads can overlap it.

A dependency graph for the ordinary connected foreground case is:

```text
native event/input collection
  -> mainloop event callbacks + cooperative fibers
  -> movement input scan
  -> idle()
       frame clock/timers, material/image publications
       mainloop WorkQueue callbacks (soft default 1 ms)
       agent controls/autopilot and outbound update
       UDP simulator message decode/dispatch (soft default 20 ms, adaptive)
       inventory/other observers + UI layout/events
       agent position
       live active-object update
         non-avatar interpolation
         -> avatar state/animation and seat-drawable adjustment
         -> flexible objects / texture animation
       -> dead object and drawable cleanup
       -> terrain/layer data and visibility/region updates
       -> drawable movement + particles
       -> camera update
       -> apparent angle/LOD inputs
       -> audio/listener and shutdown work
  -> display()
       water field and hero-probe work
       camera/environment/HUD preparation
       createObjects + processPartitionQ + updateGeom
       -> updateGL logical/native resource updates
       -> main view updateCull
       -> sun shadows + avatar impostor/profile views
       -> texture update based on object/culling stats
       -> stateSort
            visibility/LOD changes and rebuildMesh
            postSort
              rebuildGeom + rebuildPriorityGroups
                -> mesh completed-data publication
              merge draw maps / alpha ordering
              deferred mesh packing
              selection/beacons/highlight state
              vertex-buffer flush/publication
       -> native render capture via renderGeomDeferred
       -> native deferred lighting/post processing
       -> UI composition and swap
       -> release cull references / deletion protection
  -> display_end_frame()
       run mainloop callbacks in predicted GPU pacing slack
       -> native frame fence end / remaining wait
  -> reflection map update, snapshot callbacks
  -> asset-thread bookkeeping / file work / mesh-upload startup
```

This graph intentionally shows camera, geometry, publication, visibility and rendering as connected stages, rather than labeling all of `display()` GPU time. `display()` contains substantial CPU traversal and resource work before the GPU can use the native batches. Conversely, a long main-loop elapsed interval can include pacing/fence waits: it must not all be attributed to CPU computation.

Specific `idle()` boundaries: queue drain `llappviewer.cpp:5544–5556`; network `:5721` / `idleNetwork():6412`; UI `:5767`; object update `:5813`; cleanup immediately after; world movement `:5905`; particles `:5908`; camera `:5925`; apparent angles `:5937`.

Specific display boundaries: `llviewerdisplay.cpp:1883–1886` geometry preparation; `:1939` cull; `:1958` sun-shadow dispatch; `:1967` impostor update; `:2012` texture list update; `:2036` stateSort; `:2168` renderGeomDeferred; `:2186` deferred lighting; `:2199` UI; `:2210–2211` deletion protection/cull-reference release.

`gMeshRepo.update()` after rendering is not the completed mesh publication stage. At `llmeshrepository.cpp:4510` it updates metrics and starts pending upload threads. Completed mesh/skin/decomposition data is published through `notifyLoadedMeshes()`, including from `LLPipeline::rebuildPriorityGroups()` (`pipeline.cpp:21324`) during sort/rebuild.

## 2. Why existing loops cannot simply become parallel loops

### Live scene and lifetime

`LLViewerObjectList::update()` (`llviewerobjectlist.cpp:899`) creates a copied vector of live pointers (`:952`) because `idleUpdate()` can mutate the object list. This protects traversal from immediate list edits, not from concurrent mutation/lifetime changes.

Objects are processed before avatars (`:1005–1032`) for a real dependency: a rider reads its interpolated seat and can move that seat's drawable while posing. `LLVOAvatar::idleUpdate()` calls `updateMoveNormalAsync()` / `updateMoveDampedAsync()` (`llvoavatar.cpp:2827–2849`). Despite the names, those functions synchronously update the drawable and shared moved list (`pipeline.cpp:20443`, `:20474`); no worker is dispatched. `markMoved()` also recurses to parent drawables before children (`pipeline.cpp:21445` onward). Parent/seat ordering is part of correctness, not incidental loop order.

`LLViewerObject` inherits non-atomic `LLRefCount` (`llviewerobject.h:121`; `llrefcount.h:55–83`). Merely wrapping a live viewer-object pointer in `LLPointer` and capturing it on a worker can make lifetime/refcount operations race. Even an atomic refcount conversion would only address lifetime, not concurrent object contents. Complex LLSD values similarly use a non-atomic owner count (`llsd.cpp:108–110`); `workqueue.h:97–102` explicitly explains why moving rather than copying queued closures matters. Snapshot data therefore needs deliberate ownership, not shallow copies of existing mutable pointer graphs.

The display path sets `LLSpatialGroup::sNoDelete = true` (`llviewerdisplay.cpp:1899`) and later clears it and releases pipeline references (`:2210–2211`). This is an existing serialized lifetime boundary. Running the next world mutation phase alongside traversal requires stronger retained/versioned objects or detached render data; an asynchronous worker must not borrow pointers past this boundary.

### Geometry is calculation mixed with publication

`LLPipeline::updateGeom(F32 max_dtime)` (`pipeline.cpp:21350`) loops live `mBuildQ1`, directly calls `updateDrawableGeom()` and erases completed queue entries while updating drawable flags. `updateDrawableGeom()` (`:21223`) calls `LLDrawable::updateGeometry()`, which directly invokes its viewer object's method (`lldrawable.cpp:1001–1007`). The inspected body does not use `max_dtime`; the caller's nominal time argument therefore does not establish a hard frame-time cap.

`LLVOVolume::updateGeometry()` (`llvovolume.cpp:2157`) changes rigged geometry, bounding boxes, group dirtiness, transforms, faces, LOD/sculpt data and octree bounds. Independent arithmetic exists inside it, but the whole function is not a pure task.

The spatial-group compiler is an especially concrete blocker. `LLVolumeGeometryManager` has process-shared static scratch arrays `sSimpleFaces`, `sPbrFaces`, `sAlphaFaces`, etc. (`llvovolume.cpp:5419–5428`), allocated once for manager instances. `rebuildGeom()` (`:5918`) clears a group's draw map, scans live drawables/faces, updates material textures and applies queued global material overrides (`:6020–6024`), then fills/sorts/uses those scratch lists and invokes `genDrawInfo()` (`:6432–6439`). Two otherwise disjoint group rebuilds would still race this shared scratch storage.

`genDrawInfo()` (`:6611`) sorts by material/batch breaker or depth (`:6623`), assigns face texture indices/draw info, allocates vertex buffers (`:7033`), assigns each live face its new buffer and offsets (`:7068–7070`), fills geometry from live volume/transforms and finally registers draw records. Safe decomposition needs task-local scratch and a detached result, followed by a controlled publication step. Making scratch `thread_local` alone leaves live face/material/draw-map and resource-allocation side effects unresolved.

### Culling/sorting are stateful

`updateCull()` (`pipeline.cpp:20972`) changes camera clipping, installs shared `sCull`, clears it and walks each live region's partitions. `markNotCulled()` (`:21062`) marks group visibility, updates camera distances, changes occlusion state and appends to global cull result lists. A region traversal cannot be parallelized safely merely because regions are distinct: the result collection and camera/global pass state are shared.

`stateSort()` (`:21657`) resets draw orders, changes group/drawable visibility and LOD state, and calls `group->rebuildMesh()` synchronously. `postSort()` (`:22146`) rebuilds geometry, drains mesh publications via `rebuildPriorityGroups()`, merges live group draw maps, packs deferred mesh updates, sorts alpha groups, creates beacons/selected faces, reads editor state and flushes vertex buffers (`:22388`). This method is both a task boundary and a publication boundary. Moving a `std::sort` to a worker would leave most surrounding dependencies and costs in place.

The current native scene is assembled from this same live frontend. `captureMetalWorldGBufferBatch()` (`pipeline.cpp:12241`) reads `LLDrawInfo`, current vertex/index backing, texture publication and matrices, and emplaces into one `capture.batches` vector (`:12331`). Pass entry functions around `:4014` change shared flags. Native batches are a useful future interface, but the capture function itself is not reentrant. Independent packet vectors, frozen resource generations and deterministic pass/order merge are needed before concurrent capture.

## 3. Avatar animation: the promising work and the trap

`LLVOAvatar::idleUpdate()` (`llvoavatar.cpp:2715`) does much more than pose evaluation: render filtering, shared extent recalibration statics (`:2763–2787`), texture checks, projected area/cull-rank state, seat/root drawable motion, base-object interpolation, pending attachments, pose update, voice/name tags, appearance/lips/loading/wind and complexity/debug updates.

`updateCharacter()` (`:5172`) performs hidden/update-period decisions, appearance transitions (which can reset skeleton/motions), sitting transitions, root/orientation update, motion updates, head offset, footsteps and child-joint updates. Self-avatar skeleton state feeds the camera (`:5314–5327`). It should not be put one frame behind merely to make jobs convenient. Hidden avatars already use minimal motion processing; they are not equivalent to fully evaluated visible avatars.

`LLCharacter::updateMotions()` (`llcharacter.cpp:179`) forwards to `LLMotionController`. `LLMotionController::updateMotions()` (`llmotioncontroller.cpp:1007`) advances local timing, purges motions, processes loading motions, invokes additive/regular motion virtual callbacks, then blends and applies poses. `updateLoadingMotions()` (`:959`) invokes virtual initialization, activates motions and on failure changes shared `sRegistry`. `updateMotionsByType()` (`:687`) invokes arbitrary motion `onUpdate` and `requestStopMotion` callbacks, with newest-motion ordering and blend-mask dependencies. Self's stop callback calls `gAgent.requestStopMotion()` (`llvoavatarself.cpp:878–883`). Remote `LLVOAvatar` stop callback is a no-op; this is a reason to start with a restricted remote-avatar path, not proof all motions are pure.

Animation asset state also includes shared `LLKeyframeDataCache::sKeyframeDataMap` (`llkeyframemotion.cpp:52`, `:2525–2580`), loading callbacks that find live `LLCharacter::sInstances` (`:2417`) and mutable asset objects (for example loop-edit operations before `onLoadComplete`). A worker needs a retained, immutable asset generation and an explicit phase when asset loads/edits may publish.

Joint getters can write: `LLJoint::getWorldMatrix()` (`lljoint.cpp:977`) lazily recomputes parent chains; `updateWorldMatrixChildren()` (`:1041`) recursively mutates matrices/dirty flags and clears subtree state. A renderer reading a skeleton concurrently with pose evaluation is not doing a read-only access just because its entry point is called `getWorldMatrix`. A published flat pose/palette or two owned skeleton generations avoids that problem.

Existing current telemetry already distinguishes `Avatars: full update`, `hidden by period`, `hidden by cull`, `period > 1`, `joint updates`, and parallel Animesh counters (`llvoavatar.cpp:668–679`). These counters are global mutable census structures, so a parallel implementation should aggregate task-local counters instead of making every increment contend on an atomic.

## 4. Existing concurrency and scheduling policies

There are already real workers: texture fetch/cache threads, image-decode pool, image upload worker, HTTP service thread, mesh repository/LOD pool/decomposition/upload workers, file/cache purge workers, General pool, plus media/audio infrastructure. The source does not support saying the current fork is single threaded.

Important effective-count correction: `initGeneralThread()` passes default three (`llappviewer.cpp:2326`), but `ThreadPoolBase` uses `getConfiguredWidth()` (`threadpool.cpp:63–70`) and shipped `app_settings/settings.xml:12239–12255` specifies **General=1**. Actual user override must be captured at runtime. ImageDecode's stored default nine is overwritten at startup with `clamp(hardware_concurrency − 6, 2, 16)` (`llappviewer.cpp:2341–2362`). MeshLodProcessing defaults two (`llmeshrepository.cpp:1002`); LLImageGL defaults one (`llimagegl.cpp:3354–3356`). Do not confuse constructor fallback counts with effective counts.

General's FIFO is used for native renderer/pipeline creation (`pipeline.cpp:2723–2733`), water-height bakes (`alwatersurface.cpp:810–815`), audio decoding (`llaudiodecodemgr.cpp:578–624`), GLTF material parsing (`llgltfmateriallist.cpp:562` onward), plugin work and stale CEF directory deletion (`llappviewer.cpp:5224–5232`). Thus one expensive compile, bake or filesystem operation can delay unrelated work at the shipped width of one. That is a source-backed head-of-line-blocking mechanism, not a measured stall attribution. Adding all future frame-deadline jobs to this FIFO would make that risk worse.

The main pools create `std::thread`s, set names/FP mode and run queues (`threadpool.cpp:75–102`, `:156–170`). No explicit Apple QoS assignment or P/E-core-aware scheduling exists in this pool startup path. A committed-source grep found separate explicit utility-QoS GCD work in `almetalprobeworkload.mm`; therefore it would be wrong to claim the entire application has no QoS use. The OS still schedules ordinary pool threads; source alone cannot establish which cores they actually occupy.

`hardware_concurrency−6` is a rough global count, not an accounting of concurrently active image/mesh/audio/pipeline/network work or heterogeneous core speed. Independent pools have no inspected shared admission controller. Oversubscription is possible under simultaneous streaming, but it is not proved by counting thread objects: many workers may be asleep or I/O blocked. Required observations are runnable thread counts, on-core time, context switches and deadline misses, correlated with active tasks and effective pool widths.

`LLCoros` is explicitly Boost fibers (`llcoros.h:108–113`), and the default usage awaits requests across frames (`:66–72`). Mainloop `llcoro::suspend()` at `llappviewer.cpp:1515` lets those fibers run. This avoids blocking on asynchronous operations but does not move CPU-heavy continuation work to other cores. `WorkQueue::postTo` is different: it executes on another queue's consumer(s) and posts a completion to the originating queue.

## 5. Budgets, backpressure and latency

- `MainWorkTime` shipped value is **1ms** (`settings.xml:3125–3135`). `idle()` drains mainloop callbacks for that timeslice (`llappviewer.cpp:5544–5556`). `WorkQueueBase::runUntil()` (`workqueue.cpp:115`) checks time before each whole callable and cannot preempt it. One large callback can exceed the entire allowance. Queue age matters as much as size.
- Mainloop capacity is **1,048,576 entries** (`llappviewer.cpp:391`), and ThreadPoolUsing's default is the same (`threadpool.h:130`). `WorkQueue::post()` calls blocking `pushIfOpen`; `tryPost()` is the explicit nonblocking alternative (`workqueue.cpp:294–340`; `llthreadsafequeue.h:89–119`). A large bounded queue reduces ordinary producer blocking but is not workload-aware backpressure; it can accumulate stale work and memory before reaching capacity.
- `display_end_frame()` (`llviewerdisplay.cpp:1476`) runs mainloop callbacks in estimated GPU pacing slack via `fill_pacing_slack()` (`:508–533`) before ending the frame fence. This already hides some main-thread completion work under GPU wait. Its success depends on GPU slack and whole-callback cost; CPU-bound frames may have little slack. It also means moving all completion processing to a different phase can lose an existing overlap benefit.
- Simulator message processing has a **20ms per-frame default** (`llappviewer.cpp:6409`), a time check after decoded messages and an adaptive **3.5% budget increase** if buffered packets exceed decoded messages (`:6452–6483`). The code explicitly discusses possible runaway on slow consumers/fast arrivals. This can consume render deadlines under load. Moving transport alone is insufficient because handlers apply live world state; the needed split is immutable decode/validation plus ordered main-state application with urgency/coalescing policies that preserve protocol semantics.
- Mesh workers push completed records under `mLoadedMutex`. Main drains queues in one short swap (`llmeshrepository.cpp:3510–3523`), publishes headers before callbacks, then uses a shared default **2ms** budget for loaded and unavailable mesh notifications (`:3544`). Each item is processed before checking the clock, so an individual large fan-out can overrun. Header/skin/decomposition/physics queue processing is not all governed by that budget. Network submission has independent high/low-water rules (`:4742–4745`, `:4982–5015`). Faster decoding may increase completed-data pressure instead of eliminating main-thread cost.
- Texture updates are also not a hard global cap. `LLViewerTextureList::updateImages()` (`llviewertexturelist.cpp:772`) gives fast cache, fetch and create phases minimum fair shares (`:827–841`) and then runs loaded callbacks/stats. `llviewerdisplay.cpp:2010–2012` passes a 2–5ms nominal allowance, but it is neither a preemptive scheduler nor a strict cap over all callbacks.
- Native texture upload batching already has explicit ownership: one image worker, thread-local batch, a FIFO flush barrier and an upload-count cap of **16** (`llimagegl.cpp:79–143`). Publication waits for command-batch commit before the main thread exposes a new generation (`llviewertexture.cpp:1705–1762`). This is a useful local template. Increasing upload workers requires revisiting those ownership/order assumptions, not just changing a thread setting.

## 6. Concrete incremental decomposition

These are proposed designs, not existing symbols or measured wins. The order should be selected from the corresponding cost evidence, not from aesthetic preference for a job graph.

### A. Detached spatial-group geometry compilation

Target `LLVolumeGeometryManager::rebuildGeom()/genDrawInfo()`, not the entire `LLVOVolume::updateGeometry()` function at once.

1. On main, apply queued material changes, resolve immutable source volume/face attributes, determine dirty generations and capture transform/material/channel/quality keys. Reserve an object/group generation and cancellation token.
2. A worker owns a proposed GroupBuildInput with retained immutable geometry and value attributes; no `LLFace`, mutable `LLSpatialGroup` or viewer-object lifetime/refcount operations occur there. Use per-job scratch rather than shared static face arrays.
3. Worker produces packed CPU vertex/index bytes, draw records, bounds/cost data and face-to-buffer mappings. Split allocation/publication from computation initially; safely move more native resource creation only once allocator ownership is established.
4. Main validates object/group/material/generation and dirty reason before publishing. Replace the complete draw map/buffers atomically at the stage boundary. A stale result is discarded without clearing newer dirty bits. Retain an older valid render generation while rebuilding only where existing feature semantics permit it.
5. Prioritize currently visible/deadline builds and bound outstanding bytes/jobs. Coalesce repeated dirty generations of one group; do not queue every intermediate state. A task cannot block waiting for main while main is joining that task.

Benefit mechanism: expensive classification, sort, packing and geometry copy leave the main critical path and overlap other independent CPU/GPU work. Risks: snapshot cost, memory duplication, many tiny groups, stale results during edits/teleport/material streaming, group changes, changed alpha order, and resource-allocation bottlenecks. Necessary evidence: per-group face/vertex counts and dirty reason; timings for face gather/material application, sort, allocation, fill, publication; bytes copied; time-to-first-visible build; stale-result rate; queue age; actual main-thread join time. Existing zones `rebuildGeom - face list`, `genDrawInfo - sort`, `genDrawInfo - face size`, `genDrawInfo - allocate`, and `rebuildMesh - flush` provide initial segmentation.

### B. Restricted remote-avatar pose jobs

1. Keep local-avatar input, seating/parent state, appearance/asset lifecycle and motion initialization on main initially. Capture one frame time, root/seat state, selected update mode and immutable motion assets.
2. Extract eligible pose evaluation from `LLMotionController::updateMotionsByType`/blending into worker-owned local joint arrays. Keep arbitrary custom/physics/interaction motion callbacks on the owner until individually audited; initially fall back to the existing serial path for those avatars.
3. Evaluate independent remote avatars in coarse batches. Preserve per-avatar motion priority, additive-before-regular composition and constraint dependencies inside the job. Return poses plus deferred effect/stop events and local telemetry.
4. Publish complete poses before attachment bounds/rigged palettes/current camera consumers. Keep self synchronous until a design proves its camera latency unchanged. Never have render code lazily update a worker-owned skeleton; export immutable flat matrices/palettes instead.

Benefit mechanism: aggregate animation math across avatars can use more cores without sharing skeleton writes. Risks: minimal/hidden avatars may have too little work to amortize dispatch; constraints can depend on world/ground samples; custom motion virtual calls reach live state; first-visible catch-up and animation stop timing must stay correct. Necessary evidence: per-avatar full/minimal mode, active motion count, joint updates, elapsed pose-only versus side effects, biggest task duration and inter-avatar skew. Validate pose/attachment output equality or bounded numerical tolerance, seat/vehicle timing, local camera response, motion start/stop events, preview/animesh behavior. A simple avatar-count/FPS correlation is insufficient.

### C. Per-view read-only cull/batch preparation, then packet rendering

1. Separate visibility queries from `markNotCulled`/LOD/cache history updates. Use a stable spatial snapshot or retained immutable tree generation, a value camera and a per-view CullResult. Worker returns candidate IDs/visibility facts; main applies live visibility/occlusion/LOD decisions in a defined phase.
2. Prepare native draw packets from a frozen draw/resource generation. Each task owns output vectors and constant ranges. Preserve opaque material sorting and explicit alpha/HUD/water order; merge in deterministic pass sequence. Multi-view camera/global flags become per-view inputs.
3. Only after packets retain all resources/constant memory through GPU retirement should a render-owner thread consume them concurrently with the next scene update. A bounded frame queue and explicit drop/block policy are required; more queued frames can increase input latency.

Benefit mechanism: multiple views or large draw lists can be prepared concurrently; eventual render consumption no longer requires the mutable world. Risks: snapshot bandwidth, stale visibility/occlusion feedback, view-specific LOD, texture priority, selection restrictions, packet resource lifetime and alpha correctness. Necessary evidence: main/probe/shadow/HUD frontend CPU times, visible-group/draw counts, candidate-to-accepted ratio, bytes copied per packet, merge time, queue age, render starvation, GPU idle gaps and input-to-present latency. Renderer allocator/borrowed-scene ownership is a separate prerequisite being reviewed by the root agent.

### D. Scheduler policy before more worker count

Use existing ownership boundaries rather than introduce a speculative engine-wide scheduler first. Put newly proven frame-deadline work in a bounded nonblocking admission path distinct from long General maintenance tasks, with stale-generation coalescing and byte limits. Expose effective pool widths and per-class queue-age/runnable-time telemetry. Assign appropriate QoS to classes after measuring responsiveness and energy behavior; do not pin tasks to particular P/E cores as a substitute for task size/urgency design. Test streaming+rendering+audio together, not each isolated pool at maximum throughput.

## 7. Exact next evidence collection needed

The useful next runtime study is a synchronized critical-path trace of the current exact build, not an unstructured FPS benchmark:

1. Capture connected steady scene, avatar-heavy steady scene, and the first streaming interval after a region crossing, with exact camera/quality/region/asset-cache state. Use warmup and repeated >=300 measured frames for steady comparisons, following repository protocol. Do not compare the transient trace to a warm-cache steady trace.
2. Record Tracy CPU zones and native GPU/command timeline together. Separate `doFrame` runnable CPU from fence/present/locks/sleep. Include `df idle`, object/avatar/motion zones, `Update Geom`, cull, stateSort/postSort substeps, native capture, image publication, and end-frame slack work.
3. Add duration + queue-age + oldest-generation tracing around main callback execution and General submissions/completions. Record task class and payload byte size, actual effective pool width, time waiting to run, main publication cost and unfinished work at frame deadline. Avoid logging private content/identifiers unnecessarily.
4. Use System Trace/Time Profiler to establish CPU core placement, runnable queues, context switches, mutex waits and preemption. Thread count alone cannot diagnose oversubscription. Correlate with simultaneous ImageDecode, MeshLodProcessing, General and image-upload bursts.
5. For a candidate, compare `snapshot + dispatch + longest independent job chain + join + publish` against the original stage, not just worker execution speed. Require lower CPU critical-path/tail frame time, no increased visible-update latency, no material queue-age/memory regression and identical quality/feature outputs. Overall throughput is still bounded by whichever of remaining CPU critical path, GPU work and presentation pacing is longest.

No FPS multiplier, fraction of parallelizable work, thread count optimum, GPU starvation percentage, or energy improvement is claimed. The substantial finding is architectural: this native Metal fork has converted GPU execution but still obtains much of its render input through a serial, mutating world/frontend. The highest-value changes are ownership/publication boundaries around measured hot computation, with task admission and deadlines; a separate render thread or more pools alone does not establish those boundaries.
