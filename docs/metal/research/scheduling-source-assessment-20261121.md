# Scheduling assessment — source verification, 2026-11-21

Owner request: verify the supplied nine-part assessment, separate grounded facts
from hypotheses and identify feasible actions before choosing implementation.
This checkpoint changes documentation only. No renderer, scheduler, settings,
viewer process, decoder, build or live collection was changed.

## Source identity and evidence limits

- Branch/worktree: `codex/apple-silicon-metal`, canonical repository.
- Inspected HEAD: `83d33894c37087025616bd9aaf1d831cc574da97`.
- Supplied assessment revision: `797643859ddbd04d55cedd549b73cb147defe121`.
- Both revisions have identical `indra` tree
  `05091a15a707be1694ac37495414c42d90cbe951`; intervening changes are documentation.
- Initial tracked tree clean; existing untracked `worktrees/` belongs to the
  separate native-IM worktree and was left untouched. Three agents inspected
  disjoint claim groups read-only; root checked central source contracts.
- Original/reference inspection uses the locally available upstream ref
  `408fd6c014e6e3b2b90dfe61265f34df0628f450`, and the source-pinned original
  mappings already recorded for native rendering experiments. No remote
  freshness claim is made for that local ref.
- No saved user settings, current executable identity, actual pool widths,
  queue ages, active controls or current FPS were collected. Source establishes
  mechanisms, not their frequency, critical-path cost or current incident cause.

## Verdict and action map

The assessment is substantially grounded. Its strongest correction is the
separation of loading latency, frame spikes and sustained rendering throughput.
Many mechanisms are inherited Alchemy policies, rather than regressions caused
by the Metal port. A source-confirmed inefficiency is not proof of an important
whole-viewer cost. Numeric scheduling limits are not defects merely because
they are fixed.

| Finding | Grounded conclusion | Feasible action and boundary |
| --- | --- | --- |
| 1. Cache-write priority | Actual disconnected contract; PPD-528. Fetch asks for priority, but the copied request list is unused. | Design real admission priority/fairness or immutable write payload ownership. The current queue has no priority API; this is not a one-line promotion. |
| 2. General pool | Shipped width is one; three is only the constructor fallback. Invalid signed overrides are unchecked; PPD-529. | Validate overrides and expose effective width/origin first. Measure queue age by category before widening or separating jobs. |
| 3. Asset handoffs | Fast-cache gating, main-frame cache responders and polling-based decode collection are present. | Measure completion-to-consumption and lock/read delays; use bounded ready-result delivery and worker-owned I/O with generation-safe publication where justified. |
| 4. Geometry allowance | `updateGeom()` does not use the supplied time; creation/partition/rebuild/update work has no end-to-end deadline. PPD-530 retains the interface mismatch. | Attribute required and deferrable work before changing deadlines. Extract immutable computation; preserve owner-side lifecycle/publication. |
| 5. LFS foreground sleep | The conditional 10–100 ms sleep is real. Its operand is queue entries, including housekeeping, rather than measured disk demand. | Count activations and elapsed sleep. Coalesce empty housekeeping; replace demonstrated foreground stalls with bounded producer backpressure. No current activation is proved. |
| 6. Render concurrency | Existing preparation/overlap covers restricted paths; substantial scene mutation/culling/sorting/encoding retains one owner. | Select coarse immutable work using exclusive costs and requested/eligible/admitted/launched counters. Preserve failed small-job experiments and positive bounded overlap evidence. |
| 7. Fixed budgets | Values match source and can make admission rate depend on application FPS. They also bound memory and ownership. | Evaluate denials, oldest age and service capacity. Preserve hard bounds, fairness and oversized-item progress; do not raise all limits together. |
| 8. Main replies/pacing | Regular drain latches 1 ms; extra end-of-frame work exists. Earlier admission waits feed different accounting. | Correlate main-queue age with both wait sites. Service replies only at safe ownership boundaries; arbitrary callbacks inside rendering can reenter mutable scene state. |
| 9. Render CPU candidates | ResourcePlan ownership temporaries, recurring lighting allocations and serialized cached PSO construction are present. | Treat hit-path construction and frame-page reuse as measured experiments. Prewarm predictable PSOs before attempting archive-lock concurrency. |

## Exact source checks and qualifications

### 1. Texture cache priority

[Fetch state](../../../indra/newview/lltexturefetch.cpp) 2093–2098 requests
`prioritizeWrite()` when finer data needs the formatted image currently owned by
a cache write. [Cache implementation](../../../indra/newview/lltexturecache.cpp)
2290–2295 only appends the handle; 861–862 copies/clears `mPrioritizeWriteList`
without consuming `priorty_list`. The lower-resolution decoded result can still
be returned at fetch 3154–3162. This proves missing promotion, not deadlock or
missing initial display. The same disconnected code exists in the local upstream
reference. Scheduling changes must handle request replacement, in-progress
writes, cancellation and starvation. Increasing decode workers cannot remove
this ownership dependency.

### 2. General pool and configuration

[Initialization](../../../indra/newview/llappviewer.cpp) 2481 constructs
`ThreadPool("General", 3)`; ImageDecode alone is overwritten at 2526–2527.
[Settings](../../../indra/newview/app_settings/settings.xml) 12402–12403 ships
General=1. [ThreadPool](../../../indra/llcommon/threadpool.cpp) 70 uses the
override; 174–211 accepts any LLSD integer and converts it to `size_t`. Start
77–103 launches that number of workers. Zero creates no workers; a negative value
requests an enormous unsigned width. Both mechanisms are inherited upstream.
Existing debug logging is not the same as clear startup diagnostics of the
resolved value and setting origin.

The assessment's native renderer job needs a scope correction:
[pipeline](../../../indra/newview/pipeline.cpp) 2829–2887 constructs the renderer
at startup/reload; it is not ordinary per-frame draw preparation. Other users
include [materials](../../../indra/newview/llgltfmateriallist.cpp) 591–638,
[audio](../../../indra/llaudio/llaudiodecodemgr.cpp) 613–629/670–674 and
[large HTTP parsing](../../../indra/llmessage/llcorehttputil.cpp) 312–347.
One worker can create head-of-line delay; actual delay is unknown. Widening also
raises audio admission through `max_decodes = width * 2` at audio 594. Audit
ownership, memory and write backlog rather than treating width as an isolated
CPU switch.

### 3. Main-owned asset progress

[Texture list](../../../indra/newview/llviewertexturelist.cpp) 712–713 flags fast
cache work. Lines 1402–1419 skip the entire pass on failed try-lock, otherwise
read synchronously and check time after each image.
[Fetched texture](../../../indra/newview/llviewertexture.cpp) 2338–2341 returns
while flagged. [Cache](../../../indra/newview/lltexturecache.cpp) 2074–2137 does
seek/read. Cache misses can therefore wait before normal fetch starts. A bypass
must avoid duplicate requests or late low-resolution overwrites.

Cache finishWork 745–782 queues responders; application 1916–1929 invokes cache
update; cache 863–874 drains every copied completion on that caller. Fetch
2520–2554 changes `mLoaded`/`mWritten`; 2656–2662 requires the latter to advance.
The assessment is correct about delayed delivery and an unbounded responder
burst. Its phrase that the budget "bounds other work" is too broad:
[queued thread](../../../indra/llcommon/llqueuedthread.cpp) 160–189 ignores it in
threaded mode, and [worker update](../../../indra/llcommon/llworkerthread.cpp)
80–131 also scans its whole retirement list.

Texture-list 1451–1457 normally visits max(configured minimum, table size / 20),
with shipped minimum32 at settings 12337–12346. Deadline interruption at
1496–1513 and demand policies modify the circuit. Collection reaches fetch results
at viewertexture 2370–2382. A roughly20-frame/one-second circuit at20FPS is
illustrative, not every texture's latency or a universal maximum. A ready queue
needs retained identity, request generation, cancellation and deduplication;
priority/demand maintenance must still run.

### 4–5. Geometry and foreground sleep

[Display](../../../indra/newview/llviewerdisplay.cpp) 1953–1956 passes
min(5% of frame interval,5ms) to createObjects and updateGeom, with partition work
between them. Pipeline 20581–20595 checks creation time between whole objects;
21579–21621 ignores `max_dtime` while traversing priority rebuilds and moved
bridges; 21429–21440 drains the logical update queue. These policies also exist
upstream. Legacy `GL` names alone do not establish OpenGL calls. Arbitrary time
breaks may postpone frame-required geometry or resource/lifetime work.

Application 1832–1837 sleeps for min(io_pending/100,100)ms above1000 entries.
[LFS](../../../indra/llfilesystem/lllfsthread.cpp) 46–49 returns queue update;
queuedthread 206–208 returns `mRequestQueue.size()`. At1001/5000/10000 entries
the requested sleeps are10/50/100ms. This inherited policy is an activation risk,
not evidence it occurs in current sessions. The fork posts `threadedUpdate()`
unconditionally at queuedthread166–176, including empty base implementations.

### 6. Actual render parallelism and experiment balance

Pipeline 17377–17408 requires default-off `RenderMetalWorldPreparation`, at
least1024 draws and eligible inputs; excludes active animated sources, animated
GPU records, source rows and cohorts. It runs at most one worker range alongside
the owner range and joins immediately. Serial ownership does not imply all
retained data is reconstructed every frame.

Production shadow preparation at pipeline13684–13694 requires8192 inputs in one
family/pass range, followed by serial append at13944–13946. The backend's256
threshold in [shadow preparation](../../../indra/almetal/almetalshadowprepare.mm)
455–484 does not change production eligibility. Shared preparation-credit denial
can also select serial execution.

Main-light overlap at pipeline17420–17504 and shadow-view overlap15401–15466
are default-off and gated. Main-light overlap declines any current timing scope;
shadow overlap declines an actual timing window, while its null-window suppression
scope remains eligible. Not every diagnostic or external recording disables these
paths. The former overlaps G-buffer encoding with point
scalar gathering; the latter can overlap owned previous-view encoding with the
next eligible capture.

[Recorded69122 run](integrated-build-69122-quiet-club-20261115.md) reports
family maxima666/1158 below8192. PPD-241 retains the aggregate-preparation trial
winning0/108 medians and the separate successful shadow-encoding overlap trial:
eligible1024/1158/4096 cells improved owner medians9.0–16.4% and settled
medians6.5–12.8%, with one adverse GPU-sum result retained. PPD-307 records
main-light overlap owner-wall losses31/32 active cells, extremely small gathered
work and no observed overlap. These are prior bounded experiments, not current
viewer measurements or universal rejection of render workers. Default-off code
cannot be assumed inactive in a user's saved/runtime configuration.

### 7. Budget values and scope

[Budget defaults](../../../indra/llcommon/alnativeframebudget.h) 26–48 match the
assessment: upload16/frame,32inflight,32/64MiB; scene publication16/frame,
32inflight,8/32MiB; preparation2credits/32MiB logical packets; aged deferral8frames.
[Admission](../../../indra/llcommon/alnativeframebudget.cpp) 510–590 retains
large-item exceptions and capacity/fairness checks. Publication's32MiB in-flight
bound remains hard; its per-frame exception does not waive that bound.

The upload quota is specifically the decoded fetched-texture create lane at
texture-list1307–1344, covering async post and synchronous creation. It is not
every native texture creation, copy or GPU resource operation. Under its ordinary
per-application-frame policy,16 at20FPS gives an admission ceiling320/sec before
other limits. Admissions are not same-frame GPU completions. Eight-frame ageing
permits a later eligible admission, without guaranteeing400ms wall-time finish.

[Producer drain](../../../indra/newview/pipeline_producer_source_drain_native.inl)
33–43/112–122 allows256 row visits per frame,64 available to the early phase;
visits include stale/unsuccessful work. [World renderer](../../../indra/almetal/almetalworld.cpp)
2565 uses main admission{2,3}; generic class defaults are not this production
configuration. Decode-ahead at budget60–68 explicitly calls one eighth of spare
memory an initial unmeasured policy, separate from GPU upload credits.

### 8. Main replies and GPU waits

Application5718–5728 latches MainWorkTime and drains replies; settings3125–3135
ships1ms. [WorkQueue](../../../indra/llcommon/workqueue.cpp) 115–126 checks the
deadline between complete callbacks. Display532–550 and1500–1517 can run extra
mainloop work before end-frame pacing, conditional on its control and estimated
slack. [Pacing](../../../indra/llrender/alpacingslack.cpp) 31–44 uses half the
minimum of eight samples; a behind-GPU sample includes extra work **plus** wait,
and a GPU-already-done sample is zero. One zero suppresses the minimum until it
ages out. Earlier required-main waits at world2846–2911 feed native pressure
through `recordRequiredMainGpuWait`, not this slack ring. Lost useful reply time
is a plausible hypothesis requiring queue-age/wait correlation, not proved cost.

### 9. CPU work removal, compilation and obsolete decode

[ResourcePlan](../../../indra/almetal/almetalresourceplan.cpp) 174–175 passes an
owning `Use{shared_ptr,...}` temporary into `try_emplace` even on an existing key,
under the registry lock from167/208. Cached reads198–207 bypass some work.
Constructing the mapped owner only on insertion is a bounded experiment; preserve
exact generation/access/dependency/retirement accounting and measure misses too.
Do not equate the present overhead with the separate earlier exception-safety
draft PPD-415.

World13490–13505 creates a nonempty local-light buffer;27213–27230 creates base
probe constants per relevant invocation, even with inactive probe constants.
[Native buffer](../../../indra/almetal/almetalresource.mm) 616–621 directly calls
`newBufferWithLength`. Completion-aware frame pages at world1831–1847/2155–2254
are a feasible reuse boundary; integration needs slice offsets, per-view data
and exact command retirement. These allocations are per invocation, not per object.

[Pipeline cache](../../../indra/almetal/almetalpipelinecache.mm) 578–593 holds its
instance mutex through native compilation; cache-enabled policy ships true.
World27197–27209 also creates missing main-local-light variants on first use.
These concern startup/reload/first-use hitches, not a lock taken per ordinary draw.
Prewarming known variants is smaller than safely splitting mutable archive,
purge, generation and statistics ownership from compilation.

Fetch2287–2290 clears a cancelled decode handle and2563–2573 rejects stale
results. [Image worker](../../../indra/llimage/llimageworker.cpp) 228–248 still
processes accepted jobs;348–400 does unsliced pixel decode. A cancellation token
before expensive work can skip queued obsolete jobs, but cannot promise
preemption of OpenHTJ2K after decode starts. Preserve exactly-once completion,
permit/byte accounting and responder destruction semantics.

[Image upload](../../../indra/llrender/llimagegl.cpp) 3713–3734 deliberately fixes
one worker independently of saved width. Pixel conversion1883–1891 and alpha/mask
work1148–1149/1276–1278 offer a possible earlier immutable preparation stage.
Widening the existing upload owner violates current batch/flush ordering.

## Additional concrete omission: sleeping retry worker

Queuedthread493–537 posts a retry callback that can sleep its serial service for
nearly16ms, delaying other ready requests; PPD-531. This is distinct from the
foreground LFS sleep. The local upstream reference already schedules delayed
retries by handle/generation and provides wakeRequest(), plus hasThreadedUpdate()
to suppress empty posts. Those are concrete reference remedies, not permission
to import unrelated queue/lifetime changes. Review WorkSchedule compatibility,
abort/delete/replacement ordering and shutdown before adopting a scoped change.

## Priority for the next assessment

1. Correctness/robustness candidates: PPD-528 cache promotion and PPD-529 width
   validation. Promotion has the clearest broken contract; width validation has
   the smallest scope. Neither promises a steady-FPS improvement.
2. Loading/response attribution: cache finish→fetch notification,
   decode finish→collection, collected→upload posted, upload finished→published;
   General category queue age; fast-cache age/read/lock skips; LFS activations;
   scene-publication denial reasons and oldest age. Reuse existing diagnostics
   before adding narrowly scoped timestamps/counters.
3. Contained CPU experiments: insertion-only ResourcePlan ownership, frame-page
   lighting storage and predictable PSO prewarm. Compare operation and whole-frame
   costs, correctness and memory lifetime; retain adverse outcomes.
4. Completion-driven assets and timed queue retries: preserve request generations,
   cancellation and exact owner-side publication. Avoid changing every pool width.
5. Sustained-FPS work remains workload dependent. The existing
   [69187 checkpoint](engine-checkpoint-69187-findings-20261116.md) records quiet
   CPU falling while older GPU completion waits grow, plus substantial club CPU
   shadow and GPU lighting work. This older evidence is not a current profile,
   but prevents ranking these source-only CPU candidates above GPU analysis
   without a demonstrated critical-path cost.

The recent native upload/decode-ahead fixes must remain credited:
[69377 observations](world-turn-freeze-69377-20261119.md) record36420 worker posts
and no synchronous creates; [integration](warning-followup-integration-20261119.md)
documents CPU decode-ahead separation. This does not certify every present runtime
setting, but rejects diagnosing those old gates as universally still broken.

## Commands and handoff

- `git status --short`, `git branch --show-current`, `git rev-parse HEAD`,
  `git show -s 7976438`, `git diff --stat 7976438..HEAD`,
  `git rev-parse 7976438:indra HEAD:indra`: exit0, identities above.
- `uname -m`, `sw_vers`: exit0, arm64/macOS26.5.2.
- `tools/metal/doctor.sh`: first sandboxed run exit1 at denied sysctl;
  approved read-only rerun exit0, Apple M1 Max/64GiB/MacBookPro18,2,
  Xcode26.6/17F113, Metal toolchain available, unified memory true.
- Source/reference inspection used rg, numbered file reads and git show.
- `python3 tools/metal/validate_harness.py`: exit0,33required files;
  `git diff --check`: exit0. A supplementary link-check script found no missing
  targets but then failed an unnecessary heading-count assertion (substring
  counting also matched subheadings); the corrected target-only check exits0
  with all32targets existing.
- No code/tool changes: build, verify_app, unit/GPU tests, launch,
  decoder runtime check and profiling NOT RUN for this source-only assessment.
- Owner requested assessment before implementation. No fix, runtime effect,
  owner acceptance, current18–24FPS cause, push or release completion is claimed.
  Next action is to choose the scoped candidates using this separation; keep
  canonical source and the existing viewer unchanged during that discussion.
