# Aurora worker, simulation, and asset architecture: source audit

Inspected read-only on 2026-11-11. Aurora source is pinned to `a6340067e020d003f8f8e32e0e8be5bd84ff9e33`, from `https://github.com/Aurora-Viewer/Aurora-Viewer.git`, checked out at `/private/tmp/aurora-threading-analysis-20261111`. No Aurora code was built or executed; no viewer was launched, network session opened, or credential handled. Performance numbers below are author-recorded source comments or documentation, not independently reproduced measurements. Alchemy source was not changed.

## Executive finding

Aurora has implemented the most relevant architectural distinction: **background asset work is separate from synchronous frame work, frame computations use read-only inputs and disjoint outputs, and a dedicated render thread consumes an owned frame packet**. This is substantially more than putting API draw calls on workers. It makes some of the “deep surgery” concrete.

It does not run every subsystem independently or eliminate serial work. The main thread remains the authoritative world/UI coordinator. It prepares avatar work, applies network events, publishes object plans, merges draw lists, sorts transparency, integrates job results, schedules streaming, and hands over the frame. Its frame workers synchronously join before the next dependent stage. The important transferable idea is **explicit ownership and data-dependency boundaries**, not Rust syntax, Rayon itself, an assumed universal speedup, or the exact worker counts.

## 1. Actual execution topology

At startup the global Rayon pool contains `max(available_parallelism - 1, 2)` workers, named `aurora-worker-N`. This pool serves frame helpers and UI parallel work. Scene `Jobs::new` constructs a separate Rayon pool with `max(available_parallelism - 2, 1)` workers, named `aurora-jobs-N`. A trash thread frees deferred CPU allocations. `NetClient::new` constructs a Tokio runtime with exactly three async workers. These are actual called constructors: `App::new` creates the NetClient; `run_viewer` creates the global pool before starting the event loop.

Sources: [main.rs L264-L282](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/main.rs#L264-L282), [jobs.rs L295-L348](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/jobs.rs#L295-L348), [lib.rs L178-L184](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-net/src/lib.rs#L178-L184).

This separates long decodes from the principal frame helper queue, but **is not one globally bounded CPU concurrency budget**. The two pool sizes add up to approximately twice the reported processor count, before main, render, network, and other dedicated threads. The constructor does not distinguish Apple performance/efficiency cores. `available_parallelism` describes an available parallelism estimate, not a reservation of one physical performance core per thread. Frame helper count is separately capped at seven. Some scene/UI helpers still use the global Rayon API directly, so isolation is specific to jobs routed through `Jobs`, not a guarantee for all work in the repository.

Transfer: isolate latency-sensitive jobs from slow asset jobs, but measure a shared concurrency budget on the actual M1 Max. Do not copy both nearly-core-count pools as an Apple Silicon tuning prescription.

## 2. Caller participation and late helpers: verified implementation

`frame_par::for_chunks` posts helper tasks to Rayon and **immediately runs `shared.work()` on the calling thread**. Each participant obtains its next chunk using an atomic fetch-add. Small loops run entirely on the caller. This is not simply an ordinary parallel iterator moved to a pool while the main thread sleeps.

The closure borrows caller-owned state. The implementation erases its lifetime into a raw trait-object pointer. A separate atomic word contains a CLOSED bit and a count of helpers that have entered. A helper atomically increments the count before dereferencing the borrowed closure. On exit the caller sets CLOSED, waits for entered helpers to decrement the count, and only then returns. Helper tasks that were posted but have not entered observe CLOSED and return without touching the closure. Therefore it does **not** wait for every posted helper task to start; it **does** wait for every entered helper to finish. This is a synchronous fork/join scope, not asynchronous simulation or a worker allowed to publish arbitrarily late.

Sources: [frame_par.rs L90-L152](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/frame_par.rs#L90-L152), [L164-L221](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/frame_par.rs#L164-L221).

`fold` and `filter_map_mut` create a per-chunk result slot or mutable-slice slot, run chunks in arbitrary execution order, then collect slots in original index order. This preserves the order of the input sequence for a given invocation. It does not make upstream HashMap iteration or the entire simulation bitwise deterministic across runs. Workers cannot concurrently mutate the same output chunk through these interfaces.

Costs remain: Arc allocation, atomic claims, per-chunk mutex/result vectors, helper scheduling, and a join. The join spins before yielding after a 500-microsecond threshold. An entered helper preempted mid-chunk can still hold the main thread. The raw-pointer lifetime protocol is delicate: it is a design to study, not a safe drop-in implementation for C++ without an independently reviewed scope/lifetime model.

Granularity is fixed in source: planning chunks32/per-helper8; draw lists128/4; poses1/8; flexible simulation1/2. Help is requested once `ceil(items/chunk)` reaches `per_helper`: the first planning/list/pose thresholds are225/385/8 items, respectively; at most7 helpers. Author comments call these dev-profile estimates, not calibrated M1 Max thresholds. [Grain constants](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L230-L255), [helper count](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/frame_par.rs#L49-L57).

## 3. Object placement: borrowed read-only world, parallel plans, serial publication

`Scene::sync` builds a list of moving, dirty, waiting, and scheduled-LOD objects. It plans one parent-depth level at a time. `SyncCtx` contains borrowed references to World, mesh/geometry stores, palettes, object records, transform cache, materials, and related inputs, plus small copied scalar values. **It does not clone the entire world for every worker.** Within a level, `frame_par::fold` computes `(object index, Plan)` results. After joining, the main thread publishes each transform into the cache, then proceeds to the next dependent level. This preserves parent-before-child visibility.

Sources: [SyncCtx fields](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/sync_plan.rs#L231-L255), [borrowed construction](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/sync_plan.rs#L627-L652), [level loop](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L2928-L2984).

The main thread then walks the plans, calls `apply_move` or `sync_object`, changes renderer/world bookkeeping, and manages a distance-ranked backlog. New/full syncs are subject to a time budget; moves and avatars can bypass it, and already displayed faces can continue moving while a full rebuild waits. This is an intentional serial integration boundary and a scheduling/latency tradeoff, not all-object parallel mutation.

Source: [plan application and budget](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L2987-L3070).

Transfer: a promising Alchemy boundary is pure per-object/view preparation over a frozen or read-only scene view, followed by explicitly ordered publication. It requires mapping callbacks, drawable mutations, parent ordering, avatar seating, and resource ownership first. Keeping the whole mutable Alchemy object graph shared under one coarse lock would defeat the purpose.

## 4. Avatar poses: parallel computation with sequential side effects

`update_poses` gathers owners on the main thread, looks up world transforms, resolves animation start/stop signals, constructs grounding/IK/physics inputs, and removes each motion controller from its map into a work vector. It splits the palette array into disjoint per-owner mutable slices and pairs each slice with one mutable work item. The parallel closure evaluates motions and writes only that owner's palette/controller/physics work. It reads rig/world/support data shared immutably for the scope.

After joining, motion/physics state returns to maps, timestamps update, and attachment/world pose matrices are copied/derived serially. Cross-avatar IK references use a shared first-pass pose snapshot, then at most one additional parallel solve for dependent avatars. This avoids an uncontrolled iterative inter-avatar dependency loop, though it still imposes another dependency stage and copies pose matrices.

Sources: [serial gather/controller ownership](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L5416-L5525), [parallel palette solve and second pass](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L5626-L5729), [publication/copy](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L5730-L5777).

Transfer: per-avatar pose evaluation is plausible in Alchemy only after splitting animation notifications, network-facing start/stop behavior, skeletal state mutation, and attachment dependencies from the pure solve. GPU skinning does not eliminate CPU animation/controller work. Do not infer the complete Aurora animation system is asynchronous or every avatar is evaluated every frame: distance/impostor policies also reduce work.

Flexible objects follow a similar design: gather independent mutable simulation states, read shared world/geometry data during the scoped parallel computation, then apply returned updates. [flexible.rs L136-L175](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/flexible.rs#L136-L175).

## 5. Draw preparation: local vectors, deterministic merge relative to input, serial alpha sorting

`build_lists` calls `frame_par::fold` over a visit list. Each chunk accumulates its own opaque/masked/blended/glow/shadow/reflection/probe/debug/selection lists and texture-usage notices. Main-thread data is read during this scope. The resulting chunk outputs are collected in input order, merged serially using vector appends, and assigned to scene lists. Transparent entries are then sorted and their run metadata built on the caller; usage notices update texture priority serially.

Sources: [Local structure and fold](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L4828-L4899), [merge and sorting](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L5128-L5206), [usage publication](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L5241-L5262).

GPU culling mode avoids rebuilding all CPU opaque draw lists, but does not eliminate CPU list construction for transparency and ancillary features. The fold still allocates multiple local vectors, appends data and copies draw records. Parallel preparation should be compared with **removing repeated preparation entirely**. Many small draw jobs and packet copies may cost more than an optimized serial or persistent-record/GPU-driven path.

Transfer: independent per-view or per-chunk capture records can be built in parallel if ownership, alpha order, reflection/shadow membership and side effects are explicit. This attacks preparation before the narrow final Metal encoding stage that Alchemy already experimentally found insufficient.

## 6. Asset work: prepare output away from the frame, integrate under budgets

`Scene::stream` routes texture, mesh, animation, sound, material and environment work through the Jobs and Fetcher systems. Texture decode input is shared through `Arc<Vec<u8>>`, not copied once per worker. Decode jobs produce mips, classification, picking data and optionally staged GPU upload data. Mesh background jobs read/parse cache data. `PreparedGeom::new` builds picking copies and per-joint bounds and tries to stage geometry on the worker; if staging is unavailable it retains CPU geometry instead.

Sources: [stream dispatch](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L5267-L5293), [decode dispatch](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/textures.rs#L2831-L2869), [prepared geometry](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/jobs.rs#L46-L118), [mesh read/parse](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/meshes.rs#L605-L653).

`process_results` still runs on the main thread. It shares a time budget between geometry placement (first half), download integration (up to three quarters) and completed jobs (remainder), guaranteeing at least one item per category if available. Staging readiness is checked before placement. Texture uploads separately have time/byte/item budgets and may defer page allocation. Thus asset loading remains eventually integrated into authoritative scene and renderer state rather than mutating them concurrently.

Sources: [result integration](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L1996-L2066), [upload gating](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/textures.rs#L3283-L3333).

The background job/result and trash channels are **unbounded**, and `Jobs::spawn` has no global semaphore. Texture decodes have a separate max count based on reported parallelism; the main thread chooses highest-priority candidates when slots are scarce. That bounds active texture decodes, not total queued asset/result bytes. `spawn_many` queues jobs from one parent submission, using `spawn_fifo` instead of nested joins to avoid the documented nested-work/stack growth failure. These are actual queue policies, but source inspection alone does not prove memory boundedness under arbitrary arrival rates.

Sources: [unbounded channels/spawn](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/jobs.rs#L309-L357), [batch policy](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/jobs.rs#L378-L407).

## 7. Network: transport and decoding off-main, world application remains main-thread

The spawned Tokio task owns `run_session`; its select loop handles UDP datagrams, a25ms maintenance timer, commands, event queue results, capability completions and other session responses. `on_datagram` decodes messages there. This separates network servicing from frame rate. However, one session's select-loop callbacks still run in sequence; three runtime workers do not imply one simulator datagram is parallel-decoded by three cores.

Sources: [runtime task](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-net/src/lib.rs#L245-L270), [session loop](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-net/src/session.rs#L579-L638).

Commands and decoded event channels are unbounded. The main frame polls events and applies them through `bridge_runtime.receive(&mut world, ...)`/application handlers, stopping after an8ms budget. Event timestamps preserve when the network received motion, avoiding making movement time begin only when the next frame catches up. The session also observes backlog length when adjusting bandwidth throttle; this is adaptive upstream pressure, not a hard queue byte bound.

Sources: [channels](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-net/src/lib.rs#L212-L214), [main application](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/app.rs#L9287-L9299), [backlog throttle](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-net/src/session.rs#L839-L855).

Fetcher bounds active HTTP concurrency with permits (adaptive24–192 by default, limited24 option and configurable override), but request/result channels remain unbounded. Requests can be replaced/reprioritized while queued. Cancel removes only not-yet-sent requests; in-flight HTTP finishes and reports normally, and callers ignore keys they have forgotten. This is useful honest cancellation semantics, not cancellation of running CPU/GPU work.

Sources: [limits](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-net/src/lib.rs#L131-L174), [fetch channels/cancel/permits](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-net/src/fetch.rs#L504-L549).

## 8. Staleness and lifetime: specific guards, no universal worker epoch

Texture refresh waits for that entry's pending read/decode/download to complete before forgetting it; forgetting increments a per-texture epoch. Pending uploads store that epoch and mismatches are rejected. This matters: a texture decode result itself does not carry a universal world epoch, so do not describe this as “all jobs can finish late safely because every job is epoch tagged.” The busy-entry sequencing is part of correctness. The upload queue also suppresses coarser results when a finer result is queued.

Sources: [refresh contract](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/textures.rs#L1815-L1837), [defer refresh until idle](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/textures.rs#L2655-L2672), [upload epoch rejection](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/textures.rs#L3297-L3311).

Prepared meshes/textures carry staging-pool identity checks to reject results staged for a lost/replaced device. `Scene::clear` releases render/world-associated state on logout/teleport but is not cancellation of every asset job or a global CPU-job generation bump. Content-addressed assets can validly be retained across regions. The inspected Jobs wrapper has an in-flight count but no general cancellation token or scene epoch. This is not itself a bug; it is a limit on what source inspection lets us claim.

Sources: [staging identity](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/jobs.rs#L58-L66), [stale result rejection](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L2063-L2066), [scene clear](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/mod.rs#L2236-L2284).

Transfer: distinguish immutable asset identity from mutable object/region/frame identity. Alchemy view workers and render packets need explicit owner generation, device generation and completion lifetime where applicable, even if an immutable asset cache remains reusable after teleport.

## 9. Priority and allocator claims are platform-specific

The foreground frame-priority function raises Windows threads only. Its non-Windows path is a no-op. Background Jobs lower Windows thread priority, and use nice+10 only under `target_os=linux`; there is no macOS QoS operation in that function. Hence **Aurora's explicit frame-first priority tuning does not currently transfer to Apple Silicon by running the same code**. OS defaults may still schedule threads appropriately; this audit did not measure that.

Sources: [frame priority implementation](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-render/src/frame_priority.rs#L62-L90), [background implementation](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/jobs.rs#L213-L248).

The process uses mimalloc by default, with a system-allocator comparison switch, and a dedicated trash thread moves expensive frees off-main. Source comments motivate them with Windows ETW heap-lock observations and author-reported stalls. The mechanism is real; the Windows heap diagnosis does not establish Apple's allocator as a bottleneck or justify switching Alchemy's allocator without profiling. Off-main freeing moves work/latency; it does not erase memory traffic or guarantee a bounded trash backlog.

Sources: [allocator switch and motivation](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/allocator.rs#L1-L42), [trash thread](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/crates/aurora-viewer/src/scene/jobs.rs#L409-L420).

## 10. What the evidence proves and what to transfer first

Actual source verifies a working design intent and concrete called paths. It does not independently verify a successful build at this SHA, grid feature parity, stable macOS behavior, or a quality-equivalent M1 Max speed comparison against Alchemy. Comments quote performance improvements, but they are not this audit's repeatable benchmark receipts. The profiling design logs per-second frames/tails, named main/render stages, packet handoff and wait costs. The off-CPU fields are explicitly Windows/Linux, so the same telemetry is not complete Apple CPU attribution.

Source: [profiling definition](https://github.com/Aurora-Viewer/Aurora-Viewer/blob/a6340067e020d003f8f8e32e0e8be5bd84ff9e33/docs/TESTING.md#L11877-L11925).

Recommended Alchemy investigation order:

1. **Current critical-path measurement:** separate active CPU preparation, scheduling/lock waits, encode/submit/present, GPU work, frame limiter, and resource pressure; old high main CPU alone is not proof that more workers are the next win.
2. **Reduce repeated work first:** stable frame records/dirty ranges, one preparation per owner/frame where semantics permit, shared immutable view inputs; compare this with parallelizing the repeated traversal.
3. **Extract a bounded pure preparation phase:** parent-level transforms or an independently owned avatar/view job; immutable borrowed inputs or compact owned snapshots, disjoint output slots, ordered single publication. Do not mix this experiment with a renderer API migration.
4. **Separate streaming latency from frame deadlines:** budget result integration/upload, cap queued bytes as well as active work, preserve fallback/retry/current-generation rules. Existing Alchemy workers are a starting point, not something to replace blindly.
5. **Consider render-thread handoff only with measured overlap potential:** own every packet resource until GPU completion, account for packet copy/merge and added input latency, keep backlog bounded and preserve UI/macOS window ownership.
6. **Tune Apple-specific concurrency from evidence:** do not copy Aurora's Windows priority override, 500us spin, fixed Grain values, allocator choice or near-full-size duplicate pools without M1 Max power/tail-latency measurements.

The architectural target is a dependency graph with explicit lifetimes and publication boundaries. A complete rewrite of networking, inventory or UI is not required merely to obtain these properties, but splitting legacy callbacks and global mutable render state can make each extracted boundary significant engineering work.
