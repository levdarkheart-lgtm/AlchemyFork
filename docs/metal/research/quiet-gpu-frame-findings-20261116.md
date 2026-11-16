# Quiet Home GPU diagnosis — actual early/late captures

The supplied captures support a causal explanation for a material part of the
quiet24→14FPS decline: reflection probes progressively become available, their
per-pixel shading work grows, lighting/post commands finish later, and the main
thread waits longer at its existing two-frame completion limit. A saved late
frame with a default-only probe traversal is faster; restoring its original
traversal restores the cost. This is diagnostic replay evidence, not an accepted
quality reduction, live FPS gain, completed optimization or architecture
acceptance. Original architecture implementation remains paused for owner
discussion. PPD-319 remains open; PPD-327 records the narrower codegen question.

## Inputs and preservation

Owner input `/Users/dekstroza/Alchemy-quiet-qy__4ufq` is preserved as an exact
22,105-file/11,627,693,726-byte copy under private
`artifacts/metal/quiet-frame-analysis-20261116T174649Z/owner-run`.
`input-receipt.json` records hashes and copy equality. Xcode opens separate
APFS working copies under `replay-work/`; raw input/copy is not edited.
Derived native/CPU/GPU/log reports are sibling directories. Viewer is exact
fd5e8e088e1224e150e7ae71a17f827faa18249e/26.4.0.69200-Release, executable SHA256
3bf526349d0b2702f3de3d12433c7d223fa41e469f41b24b28523cb7c736332d.
All19 prior launch pairs/four intended native paths match the timing-only run.
No viewer, new runtime profiler, renderer build or push was started in analysis.
Xcode26.6 replays existing saved work on the same M1 Max/macOS26.5.2.

Both captures have successful stopped metadata. Early covers display218→219,
with main probe bound1 and ordinary-probe bound2; late1313→1314 has main20 and
ordinary43. Main target3456×2168, hero0, SSR ready/25iterations/4samples,
point lights9/40,259,010 rectangle pixels and projectors39/two shadowed/
203,870,474 rectangle pixels agree between the captured states. Counts are
configured array bounds, not accepted texture taps per pixel.

## Live timing and observer cost

The early capture API bracket spans world2.011→6.552s and the late bracket
62.491→72.058s. Stop calls alone cost4.204827s and9.090262s. Exclude complete
API brackets plus closing/first post-capture periods; those stalls are not
ordinary frame times. Their GPU timestamps also show capture-induced scheduling
delays, so do not use captured-command envelopes as live pass measurements.

Capture-excluded eligible cadence is24.384→19.431→14.798FPS over world0–10,
10–60 and60–120s; recorded main waits15.221→24.629→42.029ms/frame. The late
post-GPU tail is0.457ms/frame, while41.534ms overlaps waited GPU envelopes.
One missing target-GPU event leaves~0.037ms/frame unpartitioned. Lighting/post
admission wait targets remain two display frames older; the aggregate main-wait
union also includes early same-frame texture readback. Native stream is partial:153 contention/
96 budget drops,37 pending commands, explicit64MiB cutoff. No lost event is
invented. Full native limits, exact capture bounds/states and five-second bins
are in `analysis-native/`.

Capture mode also costs CPU while merely armed: world10–60 has1.702sampled
mainCPU-s directly in GPUToolsCapture over50wall-s; world75–120 has1.323 over45s,
with no active helper-stack work in either window. The timing-only run has zero
sampled tool-image main work. These are measured code costs, not a counterfactual
FPS penalty. API validation stays off. Immediately before/after late save,
two-second cadence is15.742/15.730FPS; later probe warm-up continues. Capture
overhead/changed warm-up prevent treating the two runs as interchangeable.

## Which GPU work is expensive

All replay results below use observed **Medium / Overlapping** profiling mode.
Xcode states that stages execute concurrently when possible and attempts Medium
unless thermally throttled. These are counter/replay measurements, not viewer
frame costs or hardware frequency readings. Untouched late replay varies
155.55→130.89ms across sessions; retain both, not an exact early/late delta.
Early untouched replay is91.15ms. Initial shader observations:

| Shader | Early cost share / registers | First late share / registers |
|---|---:|---:|
| simple alpha with directional shadows | 24.57% /245 | 34.30% /245 |
| base-light geometry | 6.28% /214 | 15.51% /214 |
| main projectors | 14.77% /200 | 9.00% /200 |
| SSAO blur | 11.75% /83 | 7.09% /83 |
| PBR alpha | 4.59% /254 | 6.15% /254 |
| actual atmospheric haze | 0.71% /142 | 0.79% /142 |

The encoder named atmospheric-haze is reused for ordered alpha rendering
(`almetalworld.cpp:21127`,21689+). Its high cost is primarily transparent
lighting, not the actual haze shader. Seven late simple-alpha draws with2256
indices each account for nearly all of that shader's initial34.30% share.
Actual per-shader costs, not a stale encoder name, govern attribution.

The fused main encoder has exactly251,649,472 fragment invocations in early and
second late counter observations. Configured main light rectangle sums plus one
screen predict251,622,092 pixels; its~33.46 overdraw is largely expected additive
light accumulation, not33 duplicate base passes. Despite fixed invocation count,
reported FS ALU instructions rise29,514,325,760→50,488,024,064; sampler calls per
invocation2.72→3.55. Alpha/haze invocations differ53,126,656→23,222,496, so alpha
workload cannot be compared by draw count alone. Its reported ALU instructions
rise18,748,150,784→30,314,440,960 and sampler calls/invocation8.31→16.07.
Scene loading/depth/materials also evolve; these counters alone do not isolate
probes. The controlled experiment below supplies that missing comparison.

## Saved late-frame A/B/A causal check

Xcode's embedded-source editing/reload refreshes profiling statistics, as
observed here and described in [Apple's shader inspection documentation](https://developer.apple.com/documentation/xcode/inspecting-shaders).
No captured resource or geometry is replaced. In the separate late working copy,
edit only the shared `almetal_sample_base_probes` count expression at recorded
`almetal_builtin.metal:6396`, reload, then restore and reload:

| Control | Probe-bound expression | GPU replay time |
|---|---|---:|
| A, unchanged semantic reload | clamp(int(probes.config.x),1,256) | 125.57ms |
| B, default-only probe control | clamp(min(int(probes.config.x),int(probes.config.w)+1),1,256) | 91.45ms |
| A, original expression restored | clamp(int(probes.config.x),1,256) | 121.62ms |

The original and restored lines carry the same harmless comment to establish a
real unchanged-semantic reload control. Producer `llreflectionmapmanager_config.inl`
zero-initializes the constants and assigns only config.x/count and config.y/LOD;
config.w is zero. The edited bound remains runtime-dependent, rather than a
literal1 that permits complete compile-time removal of the loop. The default
probe0 remains; local probe candidate traversal is excluded. This affects every
captured PSO/view using the shared helper, including ordinary probes. Water's
separate helper is unchanged.

B is30.17–34.12ms faster than the bracketing A controls in this saved replay.
Register counts remain245simple-alpha,214base and254PBR/material for both B and
restored A. Thus probe-dependent shading makes a material causal contribution
at fixed late captured geometry/resources. This is still a **recompiled** shader
experiment, not identical-binary uniform editing. Changed lighting can affect
downstream work; do not assign an exact main-only/probe-traversal-only cost or
predict live FPS from these numbers. Untouched replay variance remains retained.

Both recompiled B and restored A report Spilled Bytes:304simple-alpha,
80PBR-alpha,32material-alpha; base is blank/unknown. Their shader register counts
and spill observations are recorded in `analysis-gpu/parent-replay-counter-observations.json`.
Those compiler results establish spilling in the recompiled variants, not a
quantified spill penalty in the original installed binary.

The source/compile shape is a specific next question (PPD-327). Native
`ALMetalBaseProbeSums` at6293+ uses five two-element thread arrays and runtime
automatic/manual family indexing. Original pinned1eb44 class3 reflectionProbeF
uses named scalar pairs and explicit branches. Dominant simple-alpha has no
explicit device writes, yet alpha replay reports~1.05–1.12GiB buffer device
writes. Private-stack/array lowering and spilling are plausible explanations;
neither this counter nor the source difference proves their share. No scalar
rewrite or native renderer fix is implemented/accepted by this diagnosis.

The exact A/B source fragments, observations and unsuccessful CSV-export attempt
are retained in the private parent receipt. Source-level profiling charges most
simple-alpha cost to the entry line, not a precise probe-loop line breakdown.
Xcode's encoder-counter Save action stayed disabled; no CSV export is claimed.
Replay was stopped, the agent's temporary source edits were not saved, and the
UI returned to Replay. The raw captures and all adverse observations survive.

## Engineering conclusion and remaining acceptance

This now goes beyond a suspect list: measured live completion backpressure,
growing configured probe population, expensive actual fragment paths and the
fixed-capture A/B/A check jointly explain a material mechanism behind the quiet
decline. Texture/material loading and source-cache churn coexist; this does not
prove probes account for every fraction of the24→14 transition. Main shadows
already run on five workers, late decoding is nearly absent, and increasing CPU
worker count or queue depth is not the demonstrated remedy.

The next quality-preserving target is the probe sampling/accumulator codegen and
dominant transparent draws, with original scalar semantics as reference. Keep
all valid probes, weights, lighting/material behavior, overlap and completion
ownership. A specific scalar/codegen hypothesis needs its own controlled GPU
check and a combined viewer retest before any FPS benefit or fix acceptance.
No architecture resumption or push is authorized by supplying this capture.

118warnings/38source sites plus3libpng notices, zero ERROR/FATAL and normal owner
quit are retained in the log report. Startup-local probe warnings and content
failures do not establish a warning-storm FPS cause. Full runtime/CPU/observer
evidence is in `analysis-cpu/` and `analysis-log/`.
