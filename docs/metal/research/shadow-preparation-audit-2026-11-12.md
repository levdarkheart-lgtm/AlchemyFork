# Shadow preparation and submission audit — 2026-11-12

The port has specific avoidable overheads, but the evidence does not support
blaming all shadow cost on a bad port or requiring a whole-viewer threading
rewrite before improving it. The strongest confirmed resource-reuse defect is
small in directly sampled CPU time. More substantial opportunities are native
rigged draw preparation/emission and the representation used for shadow maps.

This investigation used the existing [301-second profile and short GPU trace](busy-region-profile-2026-11-12.md),
compared the captured renderer with the original GL implementation, and ran one
small headless Metal API probe. It did not change viewer source, rebuild or
launch the viewer, change settings, or record another world session.

## Source and evidence boundaries

- Captured renderer: `cb840a5b9d68eec51a6cf5e7c0d6a3e3dbaea656`, viewer 68841.
- Original behavioral reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
  This repository's recorded reference includes prior Alchemy shadow policies;
  it is not claimed to be an untouched upstream revision or immediate parent.
- Pipeline references below refer to the captured revision. Concurrent work
  advanced the checkout from `24515e9` through the separate world-freshness fix
  `2c11d05d53` and its build record `5ed2133fd4`. Those changes were excluded
  from causal attribution to the captured binary and were not modified here.
- M1 Max/64 GiB, macOS 26.5.2, Xcode 26.6; doctor exit 0.
- Private exports, scripts, receipts and audit notes live in
  `artifacts/metal/shadow-investigation-20261112/`. Nothing was uploaded.

## What the port preserved

`sortShadow` and `postSort` are byte-identical between the original reference
and captured fork. `stateSort(LLCamera&, LLCullResult&)` only adds optional diagnostics. Cascade fitting,
the four directional views, two selected projector slots, projector fade and
selection policy, per-view culling/sorting, and moving-light cache invalidation
are inherited. The port did not introduce six shadow views where the original
had one.

Rotating projectors change the cache's light-view/projection matrices. Animated
casters also force refresh. That is required for current shadows, not evidence
of a broken projector cache. Being indoors is not a gate that disables the
directional path. Empty directional slices still issue clears, so encoder count
alone does not prove populated maps.

The port also improved one original operation: built-in avatar eligibility is
checked once per queued avatar/view before its internal pass loop, where the
original checked it for each of four avatar shadow passes. Existing frame
palette caching, byte-deduplicated publication, PSO caching, binding caches and
same-buffer offset updates are real optimizations already present.

## Confirmed port-specific resource-reuse defect

`ALMetalWorldGBufferRenderer::renderSpotShadow` retains only one scratch D32
attachment and recreates it when the requested width or height changes
(`indra/almetal/almetalworld.cpp:6695–6711`). Directional maps are rectangular
at render-derived dimensions; selected projector maps are square and capped.

The saved trace proves the resulting churn:

| Scratch extent | Recorded allocations | Reported bytes per allocation |
|---|---:|---:|
| 3472 × 2176 | 311 | 31,162,368 |
| 2048 × 2048 | 313 | 16,908,288 |

There are **624 allocations and 624 deallocations** in the 11.912-second trace.
Creation backtraces point to the exact `renderSpotShadow` allocation site.
The sum of reported allocation sizes is **14,983,790,592 bytes / 13.955 GiB**.
That is not copied bytes, resident memory, or physical memory bandwidth.
Some lifecycle events are missing; equal event totals do not establish a
general no-leak claim.

The full CPU profile attributes **1.032 sampled CPU seconds, 0.41% of main CPU**,
to that texture-creation ancestry. This is a genuine resource-policy defect,
but its direct CPU cost is not a large FPS explanation. Its indirect driver,
memory-pressure or pacing effects were not isolated.

**Correction boundary:** retain scratch resources for the active extents,
including resize/device/resource-pack invalidation. Existing ordered, tracked
GPU use makes a simple shape cache correctness-feasible without a CPU wait.
Persistent identities could introduce additional cross-frame dependencies,
however. A bounded pool of completion-retired slots can preserve available
overlap; its exhaustion policy must be explicit. Do not add a per-slice
`waitUntilCompleted`, use an unchecked ring, or trade bounded storage for an
unbounded cache. Verify creation count, output equivalence, retirement and GPU
timeline before claiming a frame-time benefit.

The D32 attachment is clear-loaded, DontCare-stored and never sampled. It also
fits a separate [Apple memoryless-storage experiment](https://developer.apple.com/documentation/metal/mtlstoragemode/memoryless).
The existing DontCare store already permits discarding depth, so memoryless is
not automatically a bandwidth improvement. The sampled shadow output must
remain persistent.

## A larger native GPU representation opportunity

For shadow detail modes at or below two, the original GL path disables color
writes and stores a depth target. Its opaque shadow fragment shader has no
discard or explicit depth calculation. The native path instead writes raw depth
to an **R32 color array**, while using a separate **D32 attachment** for depth
testing. Its opaque fragment function outputs clamped stage-in depth
(`indra/almetal/shaders/almetal_builtin.metal:3025–3038`).

This is a substantial representation choice, not proof of incorrect pixels.
It does require opaque fragment/color-output work that a native depth-only
pipeline may avoid. It is also the reason the separate scratch depth attachment
exists. This opportunity affects GPU work and does not require changing viewer
threading.

**A real M1 Max capability probe passed:** a 32×32, two-slice Depth32Float array
with render-target and shader-read usage was rendered with a nil fragment
function. A subsequent compute pass performed raw depth reads and raw gathers.
Forward-Z results were exactly 0.25; reverse-Z results were exactly 0.75.
Metal API Validation was enabled; the command completed, exit 0. Probe source
SHA256 `8fe5f9abb7e1e2288ef3f0d5cdbd8b523707871cea3443e7767920d6be6fba5e`.
The first restricted execution exposed no device; the same probe subsequently
ran on the actual M1 Max. Both receipts are retained.

This proves raw reads/gathers do not inherently require R32 color storage.
It does **not** prove the complete viewer conversion is correct or faster.
The current render-target wrapper rejects borrowed depth arrays and does not
set a depth slice (`almetalresource.mm:2699–2707,3018–3021`), so depth-array
support, format/type gates and every consumer need a deliberate scoped change.
Preserve reverse-Z, directional depth clamp, far clears, strict comparisons,
PCF/PCSS/RPDB filtering, projector coordinates and all alpha/cloth coverage.
Masked/stippled geometry still needs fragment coverage logic; the nil-fragment
opportunity is for opaque paths. This is not a claim of halving stored bandwidth:
the current scratch depth is already DontCare-stored, and the new persistent
depth map would still store 32 bits per texel.

## What the existing GPU data now says about the indoor club

For 310 complete frames, source order and allocation evidence allow a guarded
family classification. Every accepted frame has six main-thread shadow command
buffers, matching creation/commit/assignment order, a rectangular allocation
before ordinal one and a square allocation before ordinal five. The source
emits four directional slices before two projector slices.

| Source-correlated family | Mean per-frame GPU Active interval union |
|---|---:|
| Four directional slices | **5.625 ms** |
| Two selected projector slices | **2.024 ms** |

The second directional slice is largest, averaging 3.146 ms. These are
**source-correlated inferences**, not recorded per-map attachment labels. Three
frames lacking explicit allocation proof were excluded. Family intervals can
overlap and cannot be added as serial frame cost. The result neither proves
populated maps nor identifies the cause of each projector refresh.

The projectors contribute, but the directional maps remain the larger shadow
GPU family in this sample despite the indoor scene. A projector-only
optimization would leave substantial shadow work.

## CPU preparation and submission: where the larger opportunities are

Combined shadow processing consumed 103.805 sampled CPU seconds, or 41.26% of
main CPU. Within that owner:

| Named ancestry | CPU seconds | Interpretation |
|---|---:|---|
| Native shadow renderer | 44.039 | 17.50% of main CPU; includes overlapping encoding/validation work |
| Native/driver encoding union | 25.744 | 10.23% of all main CPU |
| State sorting | 14.987 | Post-sorting is largely nested here |
| Post-sorting | 14.951 | Inherited traversal; not an additional independent budget |
| Draw-descriptor capture | 14.853 | New native frontend replacing immediate GL setup |
| Resource registration | 7.228 | Nested within encoding |
| Culling union | 5.497 | Only 2.18% of main CPU |
| Finite-value checks | 4.230 | All such shadow checks, not palette-only |
| Palette/matrix preparation | 3.917 | Includes already-cached lookup and other work |

These costs overlap. They are not additive savings estimates.

Three concrete preparation/submission opportunities follow:

1. **Restore palette-run reuse and admit immutable palettes once.** The
   original skipped palette-cache lookup for consecutive avatar/skin matches
   (`original lldrawpool.cpp:882–908`). Native capture looks it up per batch
   (`captured pipeline.cpp:13990,14149,14320`). Expensive matrix construction is
   already cached, but lookup/metadata work remains. The native renderer then
   scans palette contents per batch before its existing byte-deduplication
   (`almetalworld.cpp:6529–6549`). A retained palette handle qualified by skin,
   pose/content generation, layout and view can carry validated/published state.
   Pointer-only caching is unsafe. Built-in body palettes incorporate the view
   and cannot be shared unchanged across different cameras.
2. **Prepare rigged emission packets once, then emit compact commands.** Static
   bulk shadow submission already exists. Avatars and skin-paletted draws are
   excluded (`almetalworld.cpp:6630`), leaving repeated scalar geometry,
   palette, constant and coverage binding. Prevalidated geometry/material/pose
   handles plus small view-specific records can reduce this work inside the
   current render owner. Preserve exact caster membership, primitive order,
   material boundaries, culling, alpha and failure semantics. Establish how many
   draws are eligible before assuming a large batching win.
3. **Reduce smaller publication overheads.** Attachment joint counts are
   republished as fresh four-byte values rounded to arena alignment; covered
   draws publish a 608-byte family record even when many fields are unused.
   Built-in body palette storage uses new retained per-mesh/view allocations.
   Terrain factors allocate a per-view buffer padded to all scene batches.
   These are source-backed candidates, not separately measured savings.

Whole-scene admission must remain before the destination is cleared; otherwise
a late invalid draw could leave a partial shadow map. Invalid palette lengths
should also be rejected before scanning their supplied counts. Reuse must
preserve those checks rather than simply delete validation.

The count pass is only 0.843 seconds inclusive under shadows (0.758 seconds
exclusive in the broader report). Pixel-area work is only 0.096 seconds inside
shadows. Neither is a credible explanation for the large shadow budget.
Even eliminating all measured shadow culling would remove only 2.18% of main
CPU. The existing union-cull mode is a bounded A/B candidate, not the primary
solution suggested by this evidence.

## A porting omission distinct from the measured bottleneck

The original `drawShadow` submitted per-shadow-camera occlusion queries after
opaque casters (`original pipeline.cpp:12913`). The native replacement retains
the `AlchemyRenderShadowOcclusion` setting/cull gate but has no equivalent
shadow query producer. `ALQueryPool::canBegin` requires an active visibility
capture; the native producer begins only for the main world G-buffer
(`captured pipeline.cpp:2951`) and its query DTOs feed the world renderer,
not the shadow scene. Merely adding the old function call would not restore
the missing depth/query/result ownership.

This is a source-backed optional-feature parity gap. The setting defaults off,
and its value/query state was not recorded, so it cannot be blamed for the
profiled slowdown. Restoring it needs a native per-shadow query path and
correctness tests for stale/partial/camera-moving results.

## What threading changes would actually require

The resource cache, palette reuse and GPU depth representation can be improved
within the current owner thread. The renderer is already native Metal; using
another thread does not by itself remove its per-draw work or GPU raster cost.

For a localized parallel experiment, freeze each view's inputs into owned
geometry/material/pose records, prewarm immutable pipelines, and give jobs
separate allocation/encoding state. Six view jobs or coarse draw ranges can
then be prepared/encoded during the same frame and joined before consumption.
Each job also needs an explicit scratch lease; sharing an underlying attachment
requires ordered, tracked GPU use rather than concurrently mutating its owner.
Preserve submission order and avoid delaying the first GPU work unnecessarily.
This can be attempted without overlapping simulation of the next frame or
rewriting network/inventory/UI ownership.

Calling the present functions concurrently is unsafe: `updateCull` installs
global `sCull`, camera IDs are shared, state sorting changes visibility/face
pools and can rebuild meshes, the capture reuses borrowed batch/palette arrays,
and `ALMetalWorldFrameData` mutates shared page vectors and a hash table. The
indirect helper explicitly has one CPU owner. A mutex around all of that would
retain serialization. Job-owned immutable inputs and arenas are the required
boundary, not a larger worker count.

Metal already supports [parallel command encoding within a render pass](https://developer.apple.com/documentation/metal/mtlparallelrendercommandencoder),
with independent encoders and defined execution order. That API does not make
the viewer's mutable inputs thread-safe. A later renderer that overlaps the next
world update needs a deeper ownership/lifetime redesign and a bounded queue with
an explicit input-latency policy. The current evidence does not require doing
that first.

## Recommended order and completion status

1. Correct scratch reuse as a small isolated resource-policy change. Validate
   allocation stabilization, completion/resize behavior and GPU overlap.
2. Prototype sampled D32 shadow arrays, including opaque depth-only encoding,
   against existing raw-depth/filter/coverage controls. This is the more
   substantial GPU-side opportunity supported by the successful M1 Max probe.
3. Introduce validated rigged shadow records and palette/run reuse on the
   current owner; measure complete preparation and emission rather than copy
   speed alone.
4. If native encoding still limits frames, use those owned records for a
   same-frame parallel preparation/encoding pilot. Full cross-frame threading
   redesign is a separate decision supported by later evidence.

Keep each experiment separate and retain the owner's map sizes, filtering,
caster membership and rendering behavior. Existing binding caches and static
bulk paths should be preserved. The argument-buffer path is already implemented
but deliberately opt-in after earlier world trials failed to establish a win;
enabling it blindly is unsupported by this profile.

Investigation complete; implementation and end-to-end improvement are
**unverified**. No FPS multiplier is claimed. The probe establishes API
capability only. The original/candidate viewer was not rerun for a matched
performance comparison. Dedicated labels and caster/draw counters will make
future short captures more direct, but another five-minute recording was not
needed to establish the findings above.

Private reproducibility records: `reference-audit.md`, `encoder-audit.md`,
`evidence-audit-notes.md`, the `evidence-audit-*` export/analysis scripts and
receipts, and `depth-array-probe/device-receipt.json`. Exact probe command,
device result, source hash and the initial environment failure are retained.
