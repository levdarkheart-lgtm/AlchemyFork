# Probe SSAO parity repair and shadow-gather candidate

Observed2026-11-09 on physical M1 Max/64GiB/macOS26.5.2/Xcode26.6;
branch `codex/apple-silicon-metal`.20FPS Ultra goal remains active and unmet.
The completed viewer window regressed; no net viewer improvement is claimed.

## Original contract and retained repair

Source `0fa4e331333f99401955bc0722929fafb6bdff22` changes only the native
probe-face branch of `LLPipeline::renderDeferredLighting`: pass `has_ssao=false`
to both native light-map and base lighting. Main calls retain the global setting.
Hero and ordinary owner entrypoints both reach this branch; lighting callees
require the corresponding cube/target context. Shadows/casters/cadence unchanged.

Exact original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` pipeline11716
selects `gDeferredSunProbeProgram` for cube captures. Shader manager2091–2102
builds it with class2/deferred/sunLightF.glsl, whose green occlusion channel is
explicitly1. Original pipeline11752 skips cube light-map blur; resource-plan
comments likewise assign SSAO to the main view. Native previously propagated
global SSAO into faces, performed8tap occlusion plus two full-face blur passes,
and consumed that occlusion in reflected lighting. Two independent source
audits and final diff review approved the narrow correction.

Existing native light-map GPU regression passes1992assertions/1case with Metal
API+GPU validation. Doctor/configure/build/verify349ARM64/deep-strict signature
all actualexit0. Initial sandboxed doctor failed on sysctl permission; host
diagnostic rerun0. Actual pipeline response file contains `-O3 -DNDEBUG`.
No shader change in this build: metallib
`8bbfe4c6f3ce45eabd4fb58049cec413178213da480b23020447f01ef864b943`.
App68576 executable
`75a0c59d7b862ba3d4668bd01cf9f9fe4ecc4fe257746386589589a6bfbb26e5`;
identity `artifacts/metal/private-0fa4e3-build-identity-20261109.json`.

Build/test logs `/private/tmp/alchemy-metal-probe-ssao-` followed by
`{doctor,configure,build,verify-app,signature,gpu-release}-20261109.log`.
Preserved measured app `/private/tmp/alchemy-metal-68576-probe-ssao-measured.app`.

## Completed resident sample — regression, not a speedup

Fullscreen shortcut before Return login; exact native binary, pinned location,
Ultra6/Midday/Preset123/3456×2168 and43live controls passed startup and final
observe/verify/gate. All256probes initialized and reached120stable frames before
300consecutive measured frames7623–7922. Initialization, not the login counter,
controlled admission. All samples exact native/zeroGL, desired discard bias1,
no missing/nonfinite bias observations, no sampled camera changes. Main-visible
97 versus baseline98; cross-run camera translation difference5.722e−6m. Controls,
sun/environment and hero/HUD restoration match. Full pixel/geometry parity is
not established. No external scene change is assumed.

| Controlled window | Mean ms | p50 ms | p95 ms | FPS |
| --- | ---: | ---: | ---: | ---: |
|68572/source46d622,bias1|106.585321|105.948188|110.883167|9.382155|
|68576/source0fa4e3,bias1|148.613868|149.572063|156.249375|6.728847|

Mean frame time increased39.43%, FPS decreased28.28%. Mean recorded draws fell
2.874% (55420.33→53827.41); this is not proof of identical pixel work. Formal
image storage7629MiB, image target about45996–46006MiB, device9514–9526MiB,
recommendation59392MiB; system_low0. Budget pressure does not explain this
window. No build, GPU test, capture or profile overlapped formal samples.
Private gather CPU compilation ended during early initialization, more than
ten minutes before formal samples. Native API/shader validation/HUD/capture
variables were unset for collection. Post-sample pmset reported no thermal or
performance warnings, not a proof of identical clock history.

CSV `artifacts/metal/private-0fa4e3-probe-ssao-20261109-01.csv`, SHA
`5678a0b3e307a11a0d4457dcbe3a0aed096ba8e5e5d3fcef31a905d64a1348a7`;
sibling `.provenance.jsonl`, SHA
`3c12eeca04bc1b765effe954ffbb138b07c9c8814f1afc97d4c42dad2abb9afe`.
QA run `candidate-0fa4e3-probe-ssao-20261109-01`. Independent report
`/private/tmp/alchemy-metal-68576-probe-ssao-comparison-20261109/formal-comparison-68572-vs-68576.json`,
SHA `44df65c9b2d5090dcf3a6faaaa6fe457a1d457fcc69c7be2938fd64cf8528c2d`;
FINDINGS SHA `3df1ff6c9f82b4cc4540b6448d231a64704cd0b2426f2ea6c6177ea016a117ac`.
Official perf_compare with `--enforce --require-provenance` actualexit1;
`artifacts/metal/private-0fa4e3-vs-46d622-perf-20261109.json` retains the failed
performance gate. Do not describe code removal as observed frame-time savings.

## Specific GPU attribution after formal collection

One `xcrun xctrace record --template 'Metal System Trace' --attach 38935
--time-limit 15s` completed actualexit0; output saved before exports/parsers.
Directory `/private/tmp/alchemy-metal-68576-regression-attribution-20261109`.
Symbol processing emitted DWARF warnings; no CPU symbol-completeness claim.
Exporter/comparison/union parser each actualexit0. Trace is separate from the
formal CSV:15.738528s,96present requests,6.099681requests/s under instrumentation.

Viewer Active GPU interval UNION143.135660ms/present,87.3082%wall; device-wide
87.8510%. Nominal thermal metadata,9.2588–9.3088GiB viewer allocation. Largest
inclusive encoder context is main initial opaque/post-deferred44.843684ms/p;
main base17.632984, haze17.090225, hero above-water20.167778. Different encoders
overlap; these numbers cannot be added or called individual shader costs.
Main opaque CPU encoding itself0.199080ms/p, not44.84ms CPU submission.

Trace confirms ZERO hero/ordinary horizontal/vertical SSAO blur encoders; main
has97of each. Thus original capture scheduling is observably restored, although
net performance regressed. Historical68566/bias4 is uncontrolled context only:
its main initial opaque group4.945755ms/p was much smaller. No68572/bias1 GPU
trace exists to establish a direct controlled cause. Existing source snapshots
show initial103batches (44fullbright,2shiny,3avatar-alpha,54glow), same category
counts as68566;68572had109with6extra glow. Snapshots precede the GPU trace and
exports lack physical draw/primitive counts. Exact costly draw/resource remains
unresolved. Current full-detail texture residency differs from historicalbias4.

Attribution FINDINGS SHA
`c640b2f9d579b240a063905424b8a93bf10b5a06159c45cbfad7b9e6c3408f46`;
viewer-union report SHA
`2d3c395f6fa1f9aa06a483f18c21af5916a4781711a9e792413c20014dc29c25`.
PID38935 subsequently quit gracefully: exact executable checked before SIGTERM,
Goodbye logged and PID absence verified.0fa4e3startup plan restored afterward.

## Raw shadow gather — tested candidate, viewer performance pending

Exact original shadowUtil.glsl272/327 uses one native textureGather per2×2
footprint. Native helper7431 used four clamped scalar reads for the same texels.
Counts8blocker/12filter and all PCSS math already matched; do not claim fourfold
bandwidth savings. Candidate preserves the original integer lower-left calculation,
then gathers at `(lower_left+1)/resolution`, clamp-to-edge, raw componentX.
Apple MSL specification section6.13 confirms xyzw order(0,1),(1,1),(1,0),(0,0).
Primary PDF/order proof retained privately; no guessed swizzle.

Private `/private/tmp/alchemy-metal-shadow-gather-20261109.u34k2G` preserves
full frozen5dea shader prefix, named helper chain only; two independent source
reviews preceded root compilation. Combined shader
`a75d5217eaa55be9263965bb4fdf26cc374c7f5c7bfd5ebac194205bd80d2f6d`;
correctness host `5e2867035a5161e149498ee726fd74c9e180bfbdbc92f02f2daac0d500da36b8`.
All three compile/link steps actualexit0. After viewer exit and state restoration,
native API+GPU validation session72157 actualexit0:15680raw queries/62720float
components EXACT;96whole light-map outputs EXACT (48RG32F,48actualRG8).
Lit/shadowed/mixed/clear/cascade controls nonvacuous; RG8 population count is not
measured by the diagnostic and must not be interpreted as zero coverage.
Correctness summary SHA
`dcd1bbbbed7396f88ea1206fdb699f8aaf5333fab89e5a80eb4ed8d4ff2b5793`.

Separately quiet timing session18272 actualexit0 after trace processing stopped.
Validation/capture/HUD unset, all thermal0. Six cases/168samples,7ABBAblocks,
14samples per variant and8–20ms common calibrated command buffers; same target
and SAME actual PSO control. Timing host
`b3dab4f6c4bf824312b451615859e9abb916dc968d0428915cdbf538fcb3af90`.

| Case | Original→gather median ms | Paired block range | Faster blocks |
| --- | ---: | ---: | ---: |
|SAME actual PSO,main resolution|8.239687→8.197791|−4.39..+4.25%|3/7|
|PCSS,no blockers,3456×2168/2048maps|3.522225→2.518292|−29.27..−26.21%|7/7|
|PCSS,blocked,same dimensions|8.345281→6.475750|−23.19..−20.24%|7/7|
|PCSS,varied UV/depth/slope|8.245542→6.317240|−24.56..−20.13%|7/7|
|Gaussian unchanged path,1024²|0.724045→0.746849|+2.05..+8.09%|0/7|
|Probe single tap unchanged path,1024²|0.148624→0.164948|+6.20..+15.90%|0/7|

Main PCSS pass medians22.4–28.5%lower; unchanged-path regressions16/23microseconds
per fixture pass remain explicit. Compiler/layout versus measurement cause is
unresolved; no general no-regression or live FPS forecast. Timing summary SHA
`a2ceea312af46faf473762fcf23a2bea2894da050ee83b0c82144afadc4e8418`.
Extended production/nonPOT/16K-edge raw proof completed with API+GPU validation:
23480queries, zero changed components/class differences, actualexit0/thermal0.
Independent review approved.30715f3cde5d6b29e2f0fe974e5d0ef72a7db94f commits the
minimal helper and permanent184line regression. Focused34626/1 and full426213/91
EACH Release/RelWithDebInfo API+GPU suites actualexit0. Initial sandboxedNinja
failed writing the compiler module cache; host retry0. Xcodeconfigure/build0,
verify349ARM64/deep-strict signature0. Build68577 executable
712bee665eefd7b6ca0255460070ba9fd023e0d4b3774310279bab27902d0bba,
metallibffd9e1d178988b0719c516cd1e87444907d59d063630d7fd9ac2d17cafaa9347.
No live FPS result yet: PID58207 launched full-screen before login, startup/end
settings checks are not yet a completed sample. Fresh live43control/location/
binary gate passed;30715fstartupplan installed/notrestored during warmup.

## Completed68577 resident comparison and attribution

68577/source30715f:300consecutive frames10487–10786,8.011210FPS,
mean124.825083ms,p50121.765792,p95154.566750. All Release/Ultra3456×2168/exact
native/zeroGL,ready256/stable120,bias1finiteall300,43controls/sun/environment/
restoration match both prior windows. Main/end view dictionaries match68572
EXACTLY; sampled camera changes0. Main-visible98 versus97/98 prior. Mean draws
about+.038%versus68576 and−2.837%versus68572. Source initial105batches now
45fullbright/2shiny/3avatar-alpha/55glow; prior103and109 differ slightly.

Mean frame time fell16.007%versus immediate68576, FPS rose19.058%; versus the
preserved68572, mean increased17.113%, FPS fell14.612%. Thus the gather window
improved against the immediate regression but did not beat the earlier native
baseline. Both official `perf_compare --enforce --require-provenance` checks
exit1; failed target/regression requirements remain explicit. No build, GPU test
or profiling overlapped formal samples. Live start/end QA gates actualexit0.

CSV `artifacts/metal/private-30715f-shadow-gather-20261109-01.csv`, SHA
`e0fdc3c1f9582695ded5d63fc67df94c773f4dc84cda565270234e76543813f8`;
sibling provenance SHA
`35b0973327713ea4290ea46e9873103bcc414359369bf5d6f28c52baff226448`.
Independent report
`/private/tmp/alchemy-metal-68577-shadow-gather-comparison-20261109/formal-comparison-68572-68576-vs-68577.json`,
SHA `7f61cf0a61b40b9126b86fbcb75991c1eedfbdad8e5673c32ec9fab155159cb5`;
FINDINGS SHA `8733149fb21df5eefbaa8e5c24c298c17b228f38bdf709ab6de1b27019f4b834`.

Separate post-formal15s viewer-only Metal System Trace attached58207 completed
actualexit0. Exporter and both parsers actualexit0, no overlapping GPU fixture.
Directory `/private/tmp/alchemy-metal-68577-gather-attribution-20261109`.
106present requests/15.702s under instrumentation are6.7507requests/s, NOT the
formal8.011FPS. Viewer Active GPU UNION129.3503ms/present versus143.1357 in68576,
87.321%wall versus87.308%. Main initial opaque inclusive group20.7564ms/p versus
44.8437, CPU encoding0.2010 versus0.1991. Main light-map4.7430 versus5.4177;
base17.9999 versus17.6330; haze16.5667 versus17.0902. Main G-buffer4.3786 versus
10.3596 while hero G-buffer8.5231 versus6.5200 and hero shadow groups7.4195 versus
5.7880. Different groups overlap; do not sum them or attribute all24ms of opaque
change to this source patch. Shadowed forward alpha also calls the changed
helper, so its potential influence is not restricted to the light-map pass.
Actual traced PSO flags/physical draw identity remain unverified.

Device Active wall95.121%versus viewer87.321% indicates additional nonviewer GPU
activity in this diagnostic window;68576device87.851%versus viewer87.308% was
different. Both report Nominal thermals. Actor attribution is pending; do not
blame an unrelated process from its CPU usage. Both traces retain main SSAO
blur and exclude cube blur. Attribution FINDINGS SHA
`5bde95c2b9c5d37a61060c7ddefb5c9b6cba3a90bb7ff2273f6acf8c0a274ba8`;
viewer-union detail
`a587908ccf7db47b63c34af4808b6a1df168e04f55e8704918451f94555b6441`.

58207exact executable checked, gracefulSIGTERM/Goodbye/PIDabsence verified,
30715fstartupplan restored afterward. All viewers closed. Measured68577 app
preserved `/private/tmp/alchemy-metal-68577-gather-measured.app`, actualditto0.

## Further bounded source work and depth-array prerequisite

Private main-initial-family diagnostic patch is source-reviewed, not applied.
It splits contiguous families only, defaultOFF/main-only and only after the
renderer observes benchmark timing true→false. Same command and attachment
LOAD/STORE order, initial stencil clear once, shared forward bindings restored.
Its added tile boundaries change scheduling/cost; any future trace is diagnostic,
not unmodified FPS or individual shader timing. A real mixed-family color/depth/
stencil parity fixture and activation counter are being prepared before use.

Private `/private/tmp/alchemy-metal-depth-array-feasibility-20261109.kDERPt`
passed the first actual D32-array capability gate. Full frozen prefix plus only
tiny R32→D32 per-slice fragment-depth initialization and three-way oracle;
two independent source reviews, three compile steps0, nativeAPI+GPUvalidation
session98353actualexit0/thermal0.8960raw queries and96whole light maps were exact
across R32four-read→renderedD32four-read→D32gather. No silent format/type fallback.
The current `texture2d_array<float>` raw read/gather binding works in these
tested native D32-array cases. Input values finite normal-range/zero; no claim
about input NaN/subnormal preservation. Host
`418f6ea5802bbbeb155cc5a3b4b3ed7b74f00a44a84b9a386f7c92c8bb46b075`,
combined shader
`8e70964c85d1b9991d382c9516eb45dae163bae977bae047269dedaaeb52476d`.

This does not prove fragment-free caster geometry, depth clamping/bias, alpha/
checkerboard coverage, avatars/terrain or production attachment-slice lifetime.
No depth-array renderer migration or FPS gain is claimed. It establishes a
necessary prerequisite for later removal of the R32 color representation.

## Product advancement and next boundary

The cube repair closes a real original-contract discrepancy, not the performance
goal. Gather now has exact output proof, full native GPU regression/build review
and a measured recent viewer improvement, but remains slower than68572. Next
validate the family-attribution diagnostic before another capture; isolate
the remaining main opaque GPU cost. Direct depth-array caster parity remains
a separate unverified mechanism after the first capability gate.
Investigate main initial opaque GPU cost with explicit scope; do not blame haze,
CPU alone or a specific shader from inclusive labels.20FPS, full visual/feature
parity, multi-scene performance and release gates remain open. Hardware
infeasibility has not been established.


## Controlled 68579 completion and Translator GPU contention

Source `1f2cc0e786f6c88f5a1dee23215c17650e6e83b0`, executable
`bf4cdfef2fe95cf91991f26e4f948328878fa8627871100825d54b11fe6d69f5`.
Metallib `ffd9e1d178988b0719c516cd1e87444907d59d063630d7fd9ac2d17cafaa9347`.
The exact app was preserved at `/private/tmp/alchemy-metal-68579-controlled-measured.app`
with executable hash verified; `ditto` and preservation check exited 0.

The user explicitly superseded pause/resume with a complete Translator stop.
From `/Users/dekstroza/Programming/translator`, root ran
`uv run --no-sync python scripts/service.py stop`, actual exit 0. API35823 and
its tailscaled35866 stopped; GPU worker35824 absence was verified. Translator
must remain stopped. No SIGSTOP was issued. Private stop record is
`artifacts/metal/private-1f2cc0-translator-stop-20261109.json`.
The earlier 68577 trace attributed 33.98% of wall time to that Python GPU actor,
including 7.30% outside viewer GPU activity. Earlier comparisons therefore do
not isolate renderer source changes from competing GPU work.

Formal unsplit collection completed 300 consecutive frames5032–5331:
mean104.0726461133ms, p50103.8956665ms, nearest-rank p95109.0435ms,
9.6086727622FPS. All rows native/noGL/Release/Ultra3456×2168, bias1,
main-visible95; all256 probes ready/faded/current and stable120. Fresh start/end
43-control, exact binary and location gates exited 0; environment/sun/hero/HUD
restoration matched. Sampled camera changes0; basis/projection unchanged.
Vertical camera delta versus68572/68577 was30.52 micrometres, versus68576
24.80 micrometres. Mean draw count55781.49, +3.591% versus68577.
No build, GPU test, profiling or heavy parsing overlapped the formal window.
FPS +19.94% versus68577, +42.80% versus68576, +2.414% versus68572;
Translator stopped is an explicit changed control, not a pure code gain.
20FPS remains unmet; hardware infeasibility is not established.

Raw data: `artifacts/metal/private-1f2cc0-family-diagnostic-20261109-01.csv`
and sibling `.provenance.jsonl`. Independent comparison JSON at
`/private/tmp/alchemy-metal-68579-family-diagnostic-comparison-20261109/formal-comparison-68572-68576-68577-vs-68579.json`,
SHA256 `39a27d619640cdf02a3db9d85769f145beca2323a5a41b128e70773b25bc00ce`.
Pressure samples: images7599MiB, all device9483–9510MiB, target45985–46006MiB,
recommendation59392MiB, system-low0. Native query299 intervals consumed119002
spatial skip events, not unique groups or directly measured saved GPU time.

### Post-sample diagnostic: initial forward families are not the dominant cost

After formal timing true→false, the opt-in diagnostic split contiguous main-view
families into six encoder runs without regrouping draws. Trace recorder75165,
serial exporters/parsers all exited0. 131 presents/15.706s gave8.3405 instrumented
requests/s, separate from9.6087 formal FPS. Viewer Active GPU interval union
103.5406ms/present (86.3584% wall), whole-device87.4914%, thermalNominal.
Family interval union2.61622ms/present: avataralpha2.16112, fullbright.31048,
glow.17212, shiny.06002; per-label sums overlap and must not be added. Added
attachment load/store boundaries change scheduling and cost; do not equate
these durations with individual shader time or the old unsplit20.756ms group.
Largest inclusive contexts: heroabove-water21.067, mainbase17.333,
mainhaze/latertransparency15.716, mainGbuffer9.108, heroHaze8.333,
heroGbuffer7.616, heroShadows6.271ms/present. These also overlap.
Translator35824 has zero Active rows. Other GPU work remains: WindowServer,
Dullahan and unknown actors, with nonviewer outside union.77878% wall.
Private trace evidence directory:
`/private/tmp/alchemy-metal-68579-family-attribution-20261109`;
FINDINGS SHA256 `eebc1718407318251aae95dd77241329fe462974e7979e6f537c1e4a757dfa13`.

After trace/export completion, root verified PID78103/executable, gracefully
sent SIGTERM, observed Goodbye and PID absence (session93455 exit0). Then
`alchemy-metal-session-state-20261109.py restore --plan artifacts/metal/private-1f2cc0-startup-setup-plan-20261109.json`
exited0. All viewer startup plans are restored; no viewer remains running.
Translator remains stopped. No screenshot or full pixel-parity claim.

### Native depth geometry feasibility: distinguish export from hardware depth

Private `alchemy-metal-depth-geometry-20261109.q6gN3i` compile steps exited0;
API+GPU run79366 exited2 on strict A/B differences. A=current R32 fragment
export, B=the same pass's retained hardware scratch depth, C=fragment-free
native D32 array slice. B/C match bit-for-bit in all896 production slice cases
and192 nonzero-bias diagnostic cases, with zero neighbor-slice corruption.
A/B differ in221 production cases/5522 pixels and51 bias cases/196 pixels.
Only1208 of5522 production mismatch values were logged; full numerical maxima
and complete nonfinite scans are unavailable. Largest logged production gap
4.768371582e-7; near-zero differences are not uniformly one ULP and can switch
clear-far classification. No epsilon waiver is inferred.
Exact original1eb opaqueShadowF writes white without gl_FragDepth/discard and
its color writes are disabled: hardware depth is authoritative. Thus B/C are a
source-grounded direction, not proof of original cross-API bitwise parity or
approval for production migration. Alpha/cloth/terrain/caster/readback gates
remain. Grouped summary SHA256
`9ef52be408638d08e4b540d00f88c0c2b786a94d91e6b59a6aa568ab33ca1506`.

### Next narrow resource candidate

Current resource.mm adds PixelFormatView for swizzle or linear/sRGB views,
although its only view creations preserve component layout. Apple explicitly
says this flag is unnecessary for these views and can inhibit lossless texture
compression on M1. The candidate removes only this automatic flag; formats,
upload bytes, swizzles, view identities, shader-write flags and sampling remain.
Original1eb imagegl2873–2946 preserves encoded asset bytes and channel swizzles;
fetchedgltfmaterial86–159 chooses color decoding versus raw data at binding.
Native compatible views are the implementation mechanism for that invariant.
Ordinary retained radiance already has shader-read-only usage; hero filtered
radiance genuinely requires ShaderWrite and is not changed.
This is compression eligibility, not measured compression ratio or FPS benefit.
Primary sources: https://developer.apple.com/videos/play/wwdc2021/10153/ and
https://developer.apple.com/videos/play/tech-talks/10580/.


Formal68579 CSV SHA256 `e344e81c5df5d7675e03284e0c86f4598d7cf5c3378ff84afa752cb1ea4c5f17`;
provenance `b379600dec103a79a2afb01d35d70a370ea96ce8c7b07b05992657ba14cd2a07`.
Independent official `perf_compare.py --enforce --require-provenance` against
68577 and68572 both actualexit1: provenance verified,300 samples/noP95 regressions,
p50wins14.68%/1.94% below20%, other three required scenarios absent.
Full report `/private/tmp/alchemy-metal-68579-formal-independent-20261109.HMcYRO/independent-formal-summary.json`
SHA256 `abed389ee8e095d03ccb09a3e525c6a24a986cd4a5e63ef98f8dca8caad9f2e8`.


### Compatible-view eligibility implementation b52c1ba969

Commit `b52c1ba96913fecc4c81f082ce97bffd6b43f6eb` changes only automatic native
PixelFormatView usage plus tests. New combined RGBA/BGRA sRGB identity/swizzle
fixture executes both actual native view reads, an independent CPU transfer
oracle, stable/distinct handles and raw upload/readback checks. Existing R8
swizzle/mip test and real resampler read|RT / BRDF compute read|Write tests
assert exact native usage. No shader/format/quality change. Independent review
approved. Doctor0, Release builds86560/88117 and RWDI49668 actual0. Focused GPU
validation0:131assertions/2cases; middle-wildcard resampler/BRDF filters did not
match, so those were verified by full suites instead. Metal API+GPU Release90528
and RWDI27625 EACH actual0:426552 assertions/92 cases. Logs use
`/private/tmp/alchemy-metal-texture-usage-` prefix and `-20261109.log` suffix.
Configure72329/build70026/verify65462/deepstrictsignature all0,349ARM64 components.
App68581 exesha `3d3977af32ff663b19bfef4a9f3dbf5c88d6721f0a105a5d0de0e10f6595fb0f`,
unchanged metallib `ffd9e1d178988b0719c516cd1e87444907d59d063630d7fd9ac2d17cafaa9347`.
First gitcommit attempt sandbox rejected index.lock, no mutation; authorized
escalation succeeded. Configure repeated after commit to refresh exactsource,
so build68580 number was reserved but measured candidate is68581.

Fresh b52c1b Midday plan installed/launchdryrun0/launch0; fullscreenCtrlSuperF
beforeloginReturn via CUA, actualLEAPlogged_in true. Freshstart43controls/
location/exactbinary gates0. PID97769 currently warming all256 probes; no
performance result yet. CSV/run names private-b52c1b-texture-usage-20261109-01 /
candidate-b52c1b-texture-usage-20261109-01. Translator original servicePIDs absent
again verified(ps exit1,empty); never restarted. No compression-ratio/FPS claim.
