# Recurring code hotspots across the three busy-scene captures — 2026-11-12

The persistent CPU problem is render preparation and native draw submission on
the main thread, led by shadows. Both complete captures put combined directional
and projector shadow processing at **about 41% of main CPU**, with main scene
geometry another **about 12%**. Texture warmup changes the workload and FPS;
it does not invalidate these recurring owners or prevent selecting concrete
optimization targets from the existing evidence.

## Evidence and comparable accounting

Source inspected at `d537f10611386616203e3057004568467aa44dd2`; no renderer
implementation, build, launch or new capture was performed for this comparison.
The first two captures use cb840/68841; the latest uses the exact locally built
68849 shadow-query candidate, with implementation provenance in
[its report](shadow-occlusion-enabled-profile-2026-11-12.md).

| Main-thread stage | First retained fragment* | Clean 301 s | Enabled 181 s |
|---|---:|---:|---:|
| Combined directional/projector shadows | 36.9% | 41.3% | 41.0% |
| Main deferred geometry | 16.7% | 11.6% | 11.9% |
| UI rendering | 4.8% | 9.3% | 8.2% |
| Avatar idle/update owner | 6.0% | 8.1% | 6.4% |
| Main-thread texture update/publication | 6.0% | 6.9% | 10.4% |
| Remaining main work | 29.8% | 22.8% | 22.1% |

These rows use one mutually exclusive, precedence-defined stack partition.
Percentages are shares of sampled main Running CPU, not wall/GPU time or
whole-machine utilization. The two complete captures have 251,590 and 163,771
main samples. They are different workloads, not a paired FPS benchmark.

*The first nominal five-minute recording retained a 10-second Windowed trace,
but only **84 main Running samples, spanning 0.362 seconds**. One sample is
1.19 percentage points. It confirms coarse recurrence of the same paths,
including shadow, native draw/binding, sorting, avatar, pixel-area and label
buffer work. It cannot establish stable percentages, a full five-minute trend,
decoder activity or the narrow font-cache diagnosis below. The two complete
captures provide the quantitative ranking. No fresh capture is needed to
identify the demonstrated recurring CPU owners.

All three were reanalyzed with the same helper/taxonomy SHA256
`62b78252feb905973377cd036009e25cf88b632da58da2699de90566622d535f`.
Exact row/sample/CPU-weight totals are asserted. Latest application symbols use
the independently verified recorded PID/UUID/load map; external driver leaves
remain partly unresolved. Raw exports are preserved. Small helper/function
boundaries differ with optimization and debug-line attribution; stable frontend
owners and explicit per-stack unions are used where appropriate.

## 1. Shadow preparation and scalar native submission: largest recurring owner

`LLPipeline::generateSunShadow` (`pipeline.cpp:26753`) handles four directional
cascades and up to two projector views. It performs per-view culling, sorting,
caster capture, and native rendering. The detailed clean GPU trace observed six
shadow slices per world chain. This is not six redundant whole-mesh rebuilds:
dirty-state gates and drained priority queues prevent that interpretation.

The stable `endMetalSpotShadowCapture` owner (`pipeline.cpp:14863`) occupies
**19.48% / 20.20% of all main CPU** in the complete captures. It includes backend
admission/validation, data publication, native binding/draw calls and their
sampled descendants. It is not pure GPU time or pure encoding. The first
fragment contains the same owner in 14 of84 samples.

The explicit `stateSort OR postSort` union occupies **16.66% / 15.93% of main
CPU**, with **11.90% / 11.40% of main CPU** occurring under shadows. Source:
`pipeline.cpp:21465`, `21954`; priority rebuild at `21978`, draw-map assembly at
`22003`. These percentages overlap the shadow owner above. Although stateSort
calls postSort in source, optimized sample stacks elide the parent; their measured
intersection is zero. The report uses a measured union rather than guessing
nesting or generally summing inclusive functions.

Actual costs include visible-group/bridge traversal, eligibility checks,
priority geometry work, per-view draw maps, batch construction and native state
submission. Alpha sorting is already skipped for ordinary depth-only shadows.
Bridge/avatar-policy checks repeat; current policy snapshots could avoid repeated
`isImpostor`/`isTooSlow` decisions while retaining camera-specific bounds tests.

Static opaque bulk submission and covered joining already exist. At
`almetalworld.cpp:6681`, rigged/avatar batches are excluded from static bulk.
Their scalar loop (`7160–7294`) publishes/binds constants, palettes, weights,
joint counts, coverage inputs and issues draws. Old clean shadow stacks resolve
7.228 CPU seconds of resource-list work and5.666 of driver state emission.
Latest named draw and binding wrappers recur, but unresolved driver leaves
prevent a new complete driver breakdown. Native calls have real costs that
cannot be removed by simplifying C++ wrapper syntax.

**Direction:** reusable validated geometry/material/pose handles and compact,
ordered per-view draw records; broaden compatible rigged emission rather than
reinventing existing static batching. Keep view-dependent avatar/cloth data,
visibility, primitive order, material coverage and query boundaries correct.
Once ownership is immutable, independent preparation/encoding jobs become a
credible threading boundary. These owner percentages are not recoverable-speedup
estimates and do not by themselves justify rewriting unrelated viewer systems.

## 2. Repeated palette inspection/publication: concrete native overhead

The native shadow renderer scans every batch's skin palette for finite floats
at `almetalworld.cpp:6599`; G-buffer attachment validation repeats it at `7726`.
Latest source-line exclusive leaves are **3.805 + 1.052 CPU seconds**, disjoint,
**2.97% of main CPU**. Old named finite-check leaves total6.478 seconds/2.57%
main, with4.230 under shadows, but the old helper includes other float checks.
The first fragment also contains three finite-check samples. Absent helper names
in the latest optimized export do not mean the scan disappeared.

Frame publication already deduplicates bytes, but deduplication-enabled inputs
are still hashed and matching candidates byte-compared (`almetalworld.cpp:1433–1446`). Latest shadow hash leaves
consume2.011 CPU seconds; decoded parent sites include palette publication at
7188/7264. Validation, publication and driver owners overlap; do not add them
as independent budgets. All hash samples are not necessarily recoverable.

There is also an original-reference shortcut lost in the native capture path: original
`lldrawpool.cpp:894` returns early for consecutive equal avatar/skin runs before
calling the palette lookup. Native capture invokes that lookup per batch at
`pipeline.cpp:14087`, `14246`, `14416`. **Once-per-frame palette computation
caching still exists** at `llvoavatar.cpp:10863`; it must not be described as
missing or as recomputing every skeleton for every view.

**Direction:** a generation-qualified, immutable validated/published palette
handle, plus safe consecutive avatar/skin lookup reuse. Retain changed-data,
extent/range and invalid-input checks before any pass clears. Raw pointer identity
is insufficient; body palettes and cloth can be view-dependent.

## 3. Moving name labels regenerate glyph geometry and allocate native buffers

Name-label owners consume **3.91% / 4.54% of main CPU** across the long captures.
The latest trace attributes **5.485 CPU seconds, 3.35% of all main CPU**, to the
label subtree at the exact Position-regeneration branch. Its native allocation
and buffer-generation descendants are included in that cost, not additional.

The source path is specific:

1. `LLHUDTextScope::draw`, `llhudrender.cpp:114–146`, projects a world position,
   puts the integer screen component into a transform, and passes the changing
   fractional remainder to the text cache.
2. `llfonttextcache.cpp:286–288` compares exact x/y. At362, a changed position
   regenerates cached geometry even when the label text remains the same.
3. `genBuffers` clears its lists at391–392 and rebuilds glyph buffers. Native
   `LLRender::genBuffer` (`llrender.cpp:2588`) creates fresh LLVertexBuffers;
   `llvertexbuffer.cpp:759` allocates native storage.

This is geometry/publication churn, not proof of re-rasterizing glyph bitmaps or
missing the shaping cache. Original1eb44 has the same fractional-position split
and invalidation trigger; it is inherited relative to that recorded reference,
not caused by the new shadow queries. The first fragment contains a label→buffer
allocation sample but does not sample this precise font branch.

**Direction:** preserve effective glyph placement/phase while reusing glyph
geometry or safe backing stores when only origin changes. Simply rounding
positions could damage smooth motion, glyph phase, clipping, emoji and shadows.
Color-only recoloring and shaping/glyph caches already exist.

## 4. Native allocation/retirement and a specific batching cliff

Across all main work, native buffer creation is **3.60% / 4.28%**, vertex-buffer
allocation **3.26% / 3.82%**, and retirement/destruction **1.80% / 2.70%**.
They overlap each other and text/scene/shadow owners. This is more than a raw
memory-copy question. Existing continuing-buffer generations already reuse
completed storage safely; allocation should not be described as universally
uncached. New text buffers still allocate fresh native backing.

`llvertexbuffer.cpp:73–89` sweeps the global retirement vector, including calls
from per-buffer destruction at809/843. Reducing repeated sweeps and preserving
completed backing stores are bounded candidates; pre-encode pointers and
in-flight GPU readers must remain alive. No resource count, leak or asymptotic
runtime is inferred from CPU samples alone.

A separate native optimization cliff is explicit at `almetalworld.cpp:8138–8139`:
**any visibility query disables adjacent static G-buffer joining for the entire
scene**. The new shadow path instead splits eligible runs at query boundaries.
Applying an equivalent boundary policy to the G-buffer is source-backed work;
the number of currently affected eligible draws and the saving are unmeasured.

The confirmed D32 scratch extent-cache defect also remains at
`almetalworld.cpp:6746–6762`: alternating directional/projector sizes replace one
scratch texture repeatedly. Earlier detailed evidence proves624 allocations in
11.912 seconds. Its CPU creation ancestry is only0.41% /0.31% main in the two
runs. Fix it as a real native resource-policy issue, without promising it will
solve the 41% shadow budget or explain the texture-memory growth.

## 5. Other recurring main-thread costs: priority work and avatar updates

`LLFace::calcPixelArea` (`llface.cpp:2040`) consumes **4.12% /3.19% of main CPU**;
texture priority updates **4.04% /3.08%**. Most pixel-area work is under texture
updates, little under shadows. It recurs in the first fragment too and is distinct
from variable JPEG2000 decoder activity. The source already throttles it to
roughly0.1-second periods. Rigged bounds, current camera projection and media
priority constrain further caching or worker publication.

Avatar idle/update owners consume **8.11% /6.40%**; animation/pose alone about
**4.23% /3.38%** across all main samples. Directly named broad avatar methods are
18.18% /15.99%, with rendering and shadow descendants overlapping those values.
These are not complete avatar-caused budgets: avatars also generate attachment
visibility, render batches, palettes, native draws and labels. Animation math
alone therefore understates their effect, while texture decoding alone does
not explain the persistent render costs.

## Priorities and evidence

For the largest structural opportunity, focus on shadow preparation and rigged
native emission with reusable immutable records. For bounded local experiments,
validated palette/publication reuse, label-origin cache reuse, native buffer
lifecycle work and query-boundary G-buffer joining have concrete source targets.
D32 extent pooling is justified but has a small directly sampled CPU budget.
No measured speedup or owner acceptance is implied by this ranking.

Artifacts: `artifacts/metal/three-profile-hotspots-20261112/summary.json`,
`first/first-summary.json`, identical-taxonomy `first/first-comparison.json`,
`preparation/{old301,new181}-comparison.json`, `preparation/receipt.json`,
`preparation/findings.md`, and `emission/{clean301,enabled181}-emission.json`
plus `emission/findings.md`. Exact inputs/helpers/source hashes and successful
row/sample/weight checks are retained locally. No private data was uploaded.
The underlying [clean profile](busy-region-profile-2026-11-12.md),
[original-versus-port audit](shadow-preparation-audit-2026-11-12.md), and
[enabled-run report](shadow-occlusion-enabled-profile-2026-11-12.md) retain their
individual capture limitations. GPU shader-instruction hotspots have not been
established across all three; the separate clean GPU detail trace identifies
pass-level work without replacing these CPU conclusions.
