# Upstream renderer review, 2026-11-13

Read-only source review of landed `65c39b7bad928e165f1acdc0cbf39478e017ef1d..408fd6c014e6e3b2b90dfe61265f34df0628f450` against fork `5a403168af62b5960126bf5bcd3c410ca791e3b4`. `original-viewer-reference` skill read. No source edits, build, GPU execution or viewer operation; conclusions below distinguish source-confirmed missing behavior from unexecuted adaptation/performance candidates. Commit/PR links are upstream `https://github.com/AlchemyViewer/Alchemy/commit/<SHA>` and `/pull/<number>`.

## Highest-priority inherited defects / missing corrections

### 1. COLRv1 failed-glyph painter state (#600) — direct CPU-side import, high

- Commit `a55fd369594c79a49627484c6b260a205774d31b`: clear the per-thread hb-raster paint before every glyph.
- Upstream describes a failed paint retaining its root surface, so the next smaller glyph composites against stale extents and reads beyond its group buffers.
- Current `indra/llrender/alfontcolrv1.cpp:164` obtains the reused painter, sets extents at285, then `hb_raster_paint_glyph_or_fail` can return false at329 before `hb_raster_paint_render`336 (the operation that normally clears it). No `hb_raster_paint_clear` exists in this file. This is the same source mechanism; actual malformed-glyph/OOB reproduction was not run here.
- No Metal shader translation is needed. Check installed hb-raster clear API, port the guard, and add a failure-followed-by-smaller-glyph regression (ideally ASan). Register inherited defect before fixing.

### 2. Deferred alpha-mask analysis leaves faces in the wrong pass (#601) — shared CPU correction, high

- Full family `bcce2bce3c9d1267fbcb35ddf0ceb24701c4c9c3`, `d66e1f48527665c9d937d85de5605d6f57d12d2e`, `830be4b4f45b4ebaac84bf7a4a8fdd706d833cb5`. Do not import only first patch: later patches handle mask-to-blend and a provisional answer that was actually read.
- A64px+ texture is analysed asynchronously; the provisional no-mask can build foliage etc into the blend pass, and a later analysis answer does not dirty its spatial group. The converse transition is broken too unless the full sequence is used.
- Current `indra/llrender/llimagegl.cpp:3020–3110` retains worker/main-queue analysis and directly assigns `mIsMask` at3081; `clearAlphaAnalysis`2290 sets false. `LLImageGL`, `LLGLTexture` and `LLViewerFetchedTexture` have no owner callback, `onAlphaMaskChanged`, `mAnsweredMask` or `mProvisionalRead`. Thus the upstream correction is missing in the native fork's still-live CPU classification path.
- Port lifecycle/atomic-owner and group-rebuild logic while preserving native upload publication. Upstream's CPU/queue tests are applicable; verify actual native mask/blend reclassification. Register as one inherited defect family.

### 3. HDR additive overflow, bloom, exposure and probe finite guards (#595) — native MSL adaptation, high

Land the final semantics, not each intermediate clamp:

- `07094f82fe6097f6e3b34d099dd7552f2f742624` with followup `977e51afe94e80045ad75140e5d4248147fb74ff`: bound light intensity *after* multiplication, cap each additive light pass at512 and base/soften at32752 so the half-float target has additive headroom. Whole-scene/final outputs retain65504. This fixes an initially inadequate4094 share.
- `74589add5f1ff53f61dae01b9bd59abf0e95d706`: sanitize bloom input before the firefly clamp, preventing infinity *0 ->NaN spread.
- `9c451dc4a23d5af3793fcbea51197bf1d519c89b`: NaN meters as black and nonfinite exposure history resets to target.
- `b929675d9cb5521a5aa11ceb61a889aaef00f666`: sanitize probe downsample, bound SH before half packing, skip SH reads when irradiance weight<=0.

Current `indra/almetal/shaders/almetal_builtin.metal` has *partial* protection: `almetal_world_bounded_radiance`11760 sanitizes inf/NaN and caps a lobe at65504; sky/final paths have further guards. But local PBR lighting11839 multiplies intensity after that clamp; local pass11901 sums and returns unbounded; base pass13234 caps at full65504, not half; projector12384 also multiplies after clamping. `almetal_bloom_extract`16991 does unguarded source RGB*firefly-scale. Exposure meter13430 still `min(dot(...),60000)` and resolve13537 mixes unguarded previous_ev. Probe downsample11207 directly writes source; SH packing11364 converts unclamped sums; SH evaluation6135–6150 mixes weighted values without the zero-scale early return. These are source-confirmed missing safety boundaries; no actual flash/overflow incident has been attributed or executed here.

Use native float/half formats and actual maximum accepted pass counts when specifying headroom; upstream63-pass assumption must be validated against native collectors rather than copied blindly. Register one HDR-finite family with subcases or split if implementation ownership demands it. Physical GPU red/green injections of NaN/inf/extreme finite values are essential.

### 4. PBR bake proxies: local editing, body hiding, refresh, shadow cache (#533) — CPU plus all native consumers, high

- `ca9043a2678e2089ac92b6c43cfe964f76871757`: store dynamic local composites in `mBakedTextures` as LLViewerTexture, use `getRenderTexture`, refresh materials on rebake.
- `ce67354a60c9747a74e1d9e9fbade1d819e7c001` plus correction `6b6f8cd1b2b7969a3c18c0b6fe2d37fff2850e31`: PBR base-colour bake proxy hides matching system body part; other PBR slots and HUD material slots must not hide it. Use final `ALBakeProxies` rule, not all-slot intermediate behavior.
- `b71807bc8313bc3994331d937f1a322ddc7bf644`: coalesce body visibility recalculation once/frame rather than once per material face completion.
- `5cc8430be5711cb781e4457c284f01fb6ae84e65`: shadow cache digests actual rendered bake, including changing local composite.
- Current `llviewerobject.cpp:5488–5489` dynamic_casts local LLViewerTexture to LLViewerFetchedTexture, dropping dynamic composites. `llfetchedgltfmaterial.h:70` only has fetched slots, no getRenderTexture/mBakedTextures. `llvoavatar.cpp:9624` counts legacy TE images alone for body hiding. `pipeline.cpp:26503` hashes mBaseColorTexture. Native capture consumers `lldrawpool.cpp:675`, pipeline13037/14491 similarly access fetched slots directly.
- Register source-confirmed inherited PBR bake behavior. Import shared representation and refresh/body rules; adapt **every native world/alpha/HUD/shadow/selection** consumer to resolved texture while preserving media and colour/data samplers. Upstream GL bind changes alone will not repair native captures.

### 5. Placed-probe occlusion flicker (#595) — CPU policy, native query integration, medium/high

- `fb1a3f1b719661d39c93b53bd32f15038928346c`: don't query boxes wholly outside current frustum; retain placed probes despite aged occlusion results so the default sky doesn't light their volume temporarily. Preview uses explicit force-default flag rather than falsifying each probe's occluded bit.
- Followup `f41086358752ab8a22b6cb8b11353bdedc6ab332` clears `mOutOfView` on return to view; must include it with first patch.
- Current `llreflectionmapmanager.cpp:2678` issues queries for all relevant allocated probes without frustum gate, `getReflectionMaps:1738` drops placed and automatic probes equally for mOccluded, `forceDefaultProbeAndUpdateUniforms:2705` still mutates per-probe occlusion. Native query data exists, so this policy remains live. Current native query results are completion-gated and may be fresher than upstream GL's3–6-frame reads, therefore precise flicker duration is not inherited as a measured fact.
- Register a source-confirmed susceptibility / missing policy with source evidence; actual flicker not reproduced. Adapt camera/view admission and preview flag in manager, retain native query result lifetimes, snapshot/off-main rules and failure behavior.

### 6. Default sky azimuth/altitude (#615) — direct CPU fix, medium

`4364fdcdba561ac91252a2cb76e2fc387c8e81c7` fixes reversed args. Current `indra/llinventory/llsettingssky.cpp:913–914` still calls `(altitude, azimuth)` instead of `(azimuth, altitude)`. Source-confirmed inherited argument defect, no renderer API work. Upstream explicitly accepts a visible behavior change: position0 sun shifts from approx80degrees elevation to horizon at80degrees azimuth. Bring upstream direction tests and visibly verify default settings; do not describe it as a lighting-neutral maintenance patch.

## Important native performance/feature adaptation candidates

### Probe relevance, pair cost and slots (#568 / #595 / #601)

- `045e8c3d16`: precompute each placed-probe shape once/frame and reject far pairs (`ALProbeEclipse`), replacing per-pair object reads and rotations.
- `e41d93f1b6d864faf83a091de51171974f2bd361`: sphere full-weight radius is8r/9 for r>=1, not r/2; also changes shader mixing to select manual-only exactly when weight>=1. Pair helper/CPU culling and native shader mixing must be integrated together.
- `bbf3754dc816709c8d8dad29bb5e165c99819ca5` with `c417d6ba989ece1d2c8ab6df3ac6772f65a0c17e`: ignore wholly unreachable probes,16m hysteresis, sync moved bounds before reach/sort, dynamic allocation based on relevant count with stepped hysteresis.
- Current manager1359–1398 still O(automatic*placed) old eclipse, reflectionmap.cpp242 uses r/2 safe conservative radius, isRelevant408 has no reach bound; dynamic sizing still counts mProbes.size(). This is confirmed avoidable-work mechanism, not a proved FPS gain. Evaluate after correctness fixes using existing busy-scene profile; include teleport region-retirement integration from general workstream.
- `3f9ab7013c` also batches probe deletion / moved-list removal. Current manager still per-element deletion; safe CPU optimization candidate, no native backend rewrite.

### Region aggregate-frustum culling (#568)

`6cf56ecc2e` adds `LLSpatialPartition::addRootBounds` and rejects an entire region when all rendered partitions' bounds are outside view, preserving corrupt/nonfinite bounds fallback and object-cache walk. Current native pipeline lacks this helper. Applies to CPU culls for main/shadow/probe views; promising measured-candidate, preserve HUD and off-main camera conditions. Not a correctness defect.

### Rigged-palette reuse (#568)

`1cb04612b4` caches palettes through a frame scratch UBO ring; GL-specific UBO mechanism is inapplicable, but once/avatar+skin+frame upload/reuse principle matches our profile. Current native lldrawpool apply_matrix_palette and pipeline capture copy palette data into native ownership rather than that UBO. Map reuse to native completion-safe buffer/CPU capture lifetime; do not cherry-pick GL UBO/shader block. Performance unmeasured for new design.

### Sky modernization (#584), large feature/performance project

`e9d2331376` merge contains ALSky replacing LLVOSky/LLVOWLSky/drawpools, ray-evaluated dome/sun/moon/bloom/clouds/aurora/meteors, packed/binned/cullable stars, GPU probes-off sky cube and one far-plane composite. Key commits `496e3edef4`, `167cb8b88e`, `5e919e7ad2`, `2cfa6eed92`, `50daef6661`, `8248f131dd`, `666714d3e8`, `e75ed4cf80`, `92f07a3808`.

Current fork retains `llvosky`, `llvowlsky`, `lldrawpoolwlsky` plus translated native raster shaders. None of ALSky/alstarfield helpers exist. Port CPU star/layer policy and MSL ray functions/direct native pass; do not import deleted GL drawpools or merge pipeline wholesale. Also carry `ba9da2e58b` depth-based sky metering + water-over-sky handling, `95362a1041` angular size from drawn body, and `d464042e3d` unclamped far-water dome *with* new composite. Those are dependencies on changed sky semantics, not standalone bug fixes of today's old native sky. `7a04019970` old sky-decode removal likewise coupled. Consider after crash/hardening/correctness imports.

## GL-specific changes to skip or retain native equivalent

- #568 NVIDIA threaded-driver stamps, mapped GL unpack rings, shared upload context/fences, pooled GL names, UBO ring splitting, `glGetError` debug callback and framebuffer-blit-to-draw substitutions: `24510cb692`, `3912e16b2d`, `b0e85533a9`, `0f7cc46bfb`, `6db055a681`, `c9faf2fd9d`, `1114426ed3`, `5f1a1f0dc9`, `99675d57cc`, `0ce29e7c99`, `ae901ea669`, `702aca10c8`, `bbb73ab0c2`, `095854fd97`, `6b9568db65`, `667f0b9319`, `f7e16075f3`, `bf3ef5e146`, `1eef5cce18`, `f4a2f817ed`, `727fd68399`: do not import backend code. Native ALMetal resources, completion timeline, retained uploads and renderer copies already own these duties. Source inspection here establishes different implementation, not global performance/semantic equivalence.
- Query-age `273d959d84`/`2f67d3a11d`: GL result minimum3frames avoids driver polling; current `ALQueryPool::result` checks actual native visibility-batch completion. No reason to add3frame latency to native solely to match GL workaround.
- `a791ef521f` early impostor target allocations addresses GL name/error synchronous waits; native target creation doesn't incur those API waits. Profile allocation before adopting lifecycle movement. Existing original corrected impostor box extent math remains in native generation; don't regress it.
- #595 `c37563fa63` probe missing-last-mip fix is **already equivalent natively**: current `mMaxProbeLOD=log2(res)-1` manager2520 and live native descriptors854/2530 allocate `round(mMaxProbeLOD)+1`, ending2x2. Do not reintroduce LLCubeMapArray GL allocation.
- #578 VRAM accounting has an independent native starting point: `almetalresource.mm:398–459` counts exact block/mip/slice/sample descriptor storage; image-owned texture bytes tracked separately. Port owner categories/pressure semantics only after reconcile native totals so memory isn't counted twice. Root covers scoped boost/pressure CPU work.
- #568 async texture/sculpt readbacks should map to native ALMetalTexture/readback and CPU image cache; GL PBO/fence classes are inapplicable. Geometry peer is auditing sculpt proportion/regeneration cases; root covers fetch floors (#622). Current native `LLImageGL::readBackRaw`2322–2503 has no GL fallback.

## Validation recommendation

First import shared CPU fixes with their existing upstream tests and exact source guard checks. For GPU fixes build focused source-identical native regressions with frozen old-MSL failing controls (extreme HDR inputs, probe NaN/SH, alpha-mask reclassification, bake editing). Then required doctor/build/verify on M1Max and exact candidate owner scenes. Build/test success alone must not close defects or authorize pushing. No tests were executed during this source-only audit.

## Final readback cross-check: high-priority fatal guard and native async adaptation

`45007f232defcd25eca93dce11beecc3a8276f0e` has a directly applicable crash fix independent of its GL async helper: current native `LLImageGL::readBackRaw`, `indra/llrender/llimagegl.cpp:2357–2359`, still treats width/height>2048 as `LL_ERRS` even though accepted MAX_IMAGE_SIZE is4096. The guard runs before the native readback branch. Upstream replaces2048 with MAX_IMAGE_SIZE and warns/returns false for invalid dimensions. Register this source-confirmed inherited fatal guard; no4096 texture execution here. This should be in the first CPU safety batch.

The same family also limits a sculpt copy to its own budget/level and preserves map proportions. Geometry peer owns the CPU budget/proportion/regenerate audit. The GPU/lifetime part is `0673bfccb8` asynchronous loaded-callback readback ->`076e6080ee` sculpt request/collect ->`d1364691b3` read-owned completion ->`45007f232d` chosen-copy-level. Current `LLViewerFetchedTexture::readbackRawImage`3036–3060 still calls synchronous native `mGLTexturep->readBackRaw(-1,..)` and no request/collect state. This is a native adaptation candidate: retain the published texture, queue a Metal copy/blit to CPU-visible storage, poll true command completion, publish on the viewer thread, cancel/expire references safely, and keep callback success/pending/failure distinct. Do not import GL PBOs/fences or conclude GPU waiting time/FPS benefit without profiling the native readback owner. Native `ALMetalTexture::readback` must be audited before implementation rather than treating it as a nonexistent backend.
