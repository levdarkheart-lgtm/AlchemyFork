# Apple-first architecture implementation status

Updated 2026-11-15. The original whole-engine goal is **active and incomplete**.
Detailed source and retained evidence are in the [execution ledger](apple-first-direct-cohorts-20261114.md).

The current built experiment is **96363dc6d2 / 26.4.0.69094**. Required build,
static bundle and strict source checks pass. Its actual publication suite passes
73 cases, including compact restoration, failure fallback and expired-generation
rejection. Unchanged native renderer checks from the preceding source are reused
with an explicit source-diff proof; they were not all rerun.

The owner-tested **69085 / 6af308d165** preview remains unchanged. Its five-minute
profile reports 13.998 FPS, which is not a controlled before/after result.
There is no demonstrated overall viewer FPS improvement yet.

| Area | Implemented and checked | Still required |
|---|---|---|
| Shared scene and resources | Producer generations, material/resource tuples, dirty notifications, immutable native leases, retained static Legacy/PBR groups. | Finish specialized terrain/tree/animated sharing; resolve cross-view publication churn without freezing camera-dependent work. |
| Animated data | Attachment identity survives main/probe adoption and GBuffer/alpha/glow binding; parity and queued-reader lifetime checks pass. Equal-byte upload amplification is repaired. | Preserve view-dependent system-body composition; animated GPU bounds remain conservative. Whole-engine benefit needs evaluation. |
| CPU preparation | Immutable view packets, shared frame credits, joined shadow preparation and an experimental world preparation path. | The world worker path remains disabled after measured regressions. Finish a useful preparation boundary; unused workers do not establish success. |
| Visibility and commands | Retained bounds, GPU frustum preparation, ordered compositions and two native command-bank experiments. | Banks remain disabled because complete/GPU costs are mixed or worse, especially for small groups. Establish a useful selection/path. |
| Unified-memory lifetime and admission | Bounded decode/upload/frame admission, native generation COW, CPU packet reclaim and command-completion native leases. | Resolve residency/admission interactions using typed miss/failure counters. Cache caps have not been raised. |
| Probe lighting | Eligible unblurred probe light maps stay in tile memory for the base-light pass. Exact pixels, depth, sky, shadows, HDR, reverse-Z, physical pass counts and older libraries pass. Empty local/projector owners avoid empty passes. | Whole-engine assessment; contained timing gains are not frame-FPS gains. |
| Static restoration | Owner-free compact tuples replace unused full batch metadata; SIMPLE reuses its immutable proof. Actual lifetime/fallback/parity checks pass. | New construction/warm/churn costs pass, but their census exposes zero expired-proof expansions. Complete that cost frontier (PPD262) before accepting this path. |
| Combined viewer | Current experiment builds with retained features and native/static checks. Owner runtime/profile evidence exists for the earlier preview. | Integrate remaining work, resolve demonstrated regressions, stage one verified combined candidate, then assess feature correctness, stability and whole-engine performance. |

## Current implementation queue

1. **Checked:** shared attachment publication and typed failure/source-miss logging.
   Source50 cost comparison improves CPU preparation in 84/96 conditions,
   including all 48 large-palette conditions. Logical upload bytes never exceed
   raw input after PPD251 repair. Completion/GPU results are mixed.
2. **Checked:** source50 probe fusion. Eight warm paired configurations show
   lower encoding wall duration and GPU envelope in all eight, median reductions
   of 8.31% and 28.39%. Completion improves seven of eight, median 4.87%; one
   condition regresses 3.18%. Cold single-sample timings are mixed.
3. **Doing:** measure real expired-proof restoration (PPD262). Source51's small
   construction/warm/churn cost matrix does not exercise that fallback. Warm CPU
   improves 17/18 versus raw; cold and churn CPU regress all 18. These results
   compare the current retained-group path with raw, not old/new tuple versions.
4. **Doing:** terrain pass sharing (PPD261), in a separate worktree. Seal common
   surface/material inputs once and preserve each patch's geometry, camera-based
   tessellation and query order. Production implementation and native parity,
   resource-lifetime and complete-path cost fixtures are integrated and independently reviewed; execution is next.
5. **Remaining:** useful CPU/GPU preparation and visibility, specialized source
   gaps, admission/churn integration, then the combined owner evaluation above.
   No supported percentage-complete or delivery ETA is available.

## Whole-engine evidence

The saved 301.686-second profile contains 85.117 sampled main-thread CPU seconds,
28.810 in ordinary probes. Geometry capture, shadows, resource lists, state binds
and draw encoding also contribute; inclusive categories overlap. The export has
no GPU pass/shader timing table, so it cannot rank GPU shaders or establish ALU
occupancy. Large admission-denial counters are not CPU timings. Only 217 new
stability deferrals occur between the interior census endpoints.

## Execution constraints

Keep staged 69085 and 69065 intact. No new viewer launch, login, live profile or
push has been performed. The separate owner-requested capture is complete. Raw
logs/traces stay local. All defects retain their IDs and owner-verification state.
