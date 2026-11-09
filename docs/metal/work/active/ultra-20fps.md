# Ultra performance campaign — active owner goal

Owner instruction, 2026-11-08: improve the native Metal viewer toward at least
20 FPS on Ultra, preserving functionality/quality and using general changes.
Codex may quit, relaunch, log in, restore setup and measure autonomously. Stop
with an evidence-based explanation if the target proves infeasible. No token
or time budget was requested.

## Constraints and acceptance

- Live tests stay at the current private location and camera Preset123. Do not
  move the avatar or visit other regions. Use Midday, full screen before login,
  pinned Ultra controls, original shadow/mirror cadence and matching resolution.
- Try Mac keyboard shortcuts first; inspect outcome before using mouse fallback.
- Do not lower quality, omit required views/casters or tune to scene identities.
  Exercise low/high overlap, heterogeneous materials, geometry and view counts
  in deterministic renderer fixtures. The full live multi-scene release gate
  remains unexecuted; this goal does not waive PRODUCT/PERFORMANCE contracts.
- Aim for mean frame time at or below 50 ms over repeated resident 300-frame
  captures; report p50/p95, variability, exact source/settings and limitations.
  A transient counter or microbenchmark is not attainment of 20 FPS.
- Compare the exact external OpenGL reference at 1eb44bbd1b lineage when useful;
  retain no OpenGL fallback in the Metal fork. General performance benefit
  requires quality and functionality proof.

## Baseline and evidence

Renderer 6c694df237, Release68554, 3456x2168, current Ultra scene: paired CSV
observes 8.409 to 9.739 FPS, p50 117.437 to 101.876 ms, p95 133.511 to119.074 ms.
GPU main opaque cost varies approximately 0.3–20 ms across unchanged-shader
runs, materially confounding complete source attribution. Main thread remains
94.28% of one core; hero views are 50.44% of sampled main work. Eligible static
shadow draws coalesce about70%, yet all logical captures are still constructed.
Full GPU-validated Release/RelWithDebInfo suites each pass57,344/75.
[Complete runtime record](../../state/archive/CURRENT-68554-direct-shadow-runtime-2026-11-08.md).

## Preserved baseline — normal texture bias,9.38FPS

Resource scope3969405 and capacity46d622 fix the all-device/image threshold
mismatch and replace arbitrary recommendation/16 with non-image reserve then
original headroom/divisor once. Scope-only diagnostic proved image-cap pressure
continued despite ample global memory. Both repairs preserve original controller,
manual overrides and global/system safeguards. Tests159/21pressure+29/3capability
each configuration, independent review/build/349ARM64/signature allpass.

68572full-screen Ultra/Midday/Preset123,3456×2168,43controls,all256probes ready,
300consecutive frames9637–9936:9.382155FPS,mean106.585ms,p50105.948,p95110.883,
bias1all300. Prior68566bias4was8.864746; differing image workloads prevent an
isolated speedup claim. Normal bias is sustained with7838MiB image storage.
Fullpixelquality parity remains open, and20FPS is not achieved or proven
impossible. Use this preserved app as the next native baseline.

Root owns shared edits/runtime. All viewers closed and user state restored.
Canonical68574/source74ee5d adds validated fragment-constant API support only;
full391587/90checks pass each config, build/349/signature0. It is unmeasured,
with production shader unchanged. No experiment overlapped formal collection.
[Complete evidence](../../state/archive/CURRENT-resource-aware-texture-budget-2026-11-09.md).

## Latest gather result —8.01FPS, target still unmet

30715f nativegather preserves exactintegerfootprint andrawPCSS. Initial15680+
extended23480queries/96fullmaps passstrictbytes; permanent34626/1 andfull426213/91
API+GPUchecks EACHconfigpass. QuietfixturePCSS22–29%lower; unchangedGaussian/
probe controls+.023/.016ms retained, no no-regressionclaim.
68577resident300frames8.011210FPS,mean124.825083/p50121.765792/p95154.566750ms,
bias1/ready256/43controls/camerafixed, exactmain/endcamera match68572. Recent
68576original-contractcubeSSAOfix had6.728847FPS/148.613868msmean despitefewer
draws; latestmean16.007%betterrecent but17.113%worsepreserved68572. Bothofficial
performancegatesfail. No20FPS orhardwareinfeasibilityclaim.
PostformalGPUmaininitialopaqueinclusive44.844→20.756ms/p, lightmap5.418→4.743,
baseabout18; overlapandexternalGPUactivityprecludefullcausalclaim. Allviewers
closed/plansrestored; evidencein[repair/gatherrecord](../../state/archive/CURRENT-probe-ssao-and-shadow-gather-2026-11-09.md).

## Latest controlled result —9.61FPS, Translator stopped

68579/source1f2cc0,300readyframes5032–5331:9.608673FPS, mean104.072646ms,
p50103.895667/p95109.0435ms; bias1/native/Ultra3456×2168/43controls match.
Sampledcamera0changes, basis/projectionmatch,30.52micrometreZdelta versus68572/
68577; main-visible95 and drawmean55781.49. FPS +19.94%vs68577/+2.41%vs68572.
User explicitly fully stopped Translator via uv(actual0), worker35824 absent.
Earlier trace found PythonGPU33.98%wall/7.30%outside viewer: changed workload
control prevents purecodeattribution. Translator must STAY STOPPED.
Both officialcomparisons exit1: provenance passes, p50targets/requiredscenes unmet.
All viewers closed/startupplans restored; exact68579app preserved.

Postformal family diagnostic viewerGPUunion103.5406ms/p; initialfamilies only
2.61622ms/p (avatar2.16112), addedencoder boundaries altercost. Largest inclusive
contexts remain heroabove-water21.067/mainbase17.333/mainhaze15.716/mainGb9.108;
labels overlap. This redirects work away from speculative initialalpha fixes.

Root implemented b52c1ba969 compatible texture-view usage repair: remove unused
PixelFormatView without changing format/quality/bytes/swizzles/colorvsdata views
or real ShaderWrite. Apple M1 guidance supports lossless compression eligibility,
not measured ratio/FPS. Independent review approved, both optimized426552/92 API+GPU0;
appbuild/349ARM64/sign0; controlledcomparison completed below.
[Complete evidence](../../state/archive/CURRENT-probe-ssao-and-shadow-gather-2026-11-09.md).

## Texture-usage candidate measured —9.45FPS, no gain

68581/sourceb52c1ba969 completed300readyframes5032–5331:mean105.768185ms,
p50105.811167/p95110.90525,9.454639FPS. Versus68579 FPS−1.603%,p50+1.844%,
p95+1.707%. BothTranslatorOFF/43controls/environment/bias1/ready256match;
mainvisible96vs95/draws−.0525%/cameraZdelta26.70micrometres. Onepairnotcausalproof.
Officialenforce/provenanceexit1, no claimofFPS/compressiongain. Bothapps preserved,
allviewersclosed/plansrestored. Trace68510/8exports/parsersactual0:viewerGPUunion105.542vs103.541ms/p,
mainbase17.331vs17.333/heroabove20.709vs21.067, no broadreduction.
NextchangesneedactualGPU/compiledcostproof; no further livecandidate basedon
source-duplicate workalone.20FPSremainsunmet, notproveninfeasible.

## Decisions and next implementation boundary

- Keep compiler, forward-binding, scope/capacity and existing correctness repairs.
  Do not restore lower texture quality to simplify a performance comparison.
- Use controlled68579/bias1/Translator stopped for subsequent comparisons; retain68572 as historical quality baseline. Earlier no-discard trial
  stays reverted; source/patch/app preserved. Rebuild stale GPU test binaries.
- Existing query metadata proves main occlusion is consumed; no need repeat the
  missing-query audit. Earlier sampled-attachment compression/excess-forward-loop hypotheses were ruled out; generic compatible-view usage is a newly verified separate candidate.
- Private classic/SSR specialization is not admitted:55/324strict failures and
  modern2.7–3.5%effects near control variation. Private material split preserves
  depth/water bits but has69/388strict failures and mixed performance: uniform
  5.9–12.5%faster, checkerboards2.7–4.6%slower. Neither is a default renderer
  improvement. [Evidence](../../state/archive/CURRENT-base-specialization-experiments-2026-11-09.md).
- Next change needs a specific substantial GPU cause. Do not repeat small
  variants or infer individual shader milliseconds from inclusive labels.
- The11hour status concern remains valid: tests/profiling alone do not achieve
 20FPS. Keep owner informed; no lower quality, travel or scope waiver.

[Architecture experiments](../../state/archive/CURRENT-ultra-goal-experiments-2026-11-08.md)
retain rejected layered results and actual M1 timestamp capability limits.
No capture/validation-enabled CSV is performance evidence. Raw captures remain
private; broad pixel/feature parity, multi-scene performance, prolonged stability,
strict no-GL and independent build/review evidence remain required.


## Register result and restored neighbor-selection contract

The focused profile identifies216 registers and ALU-heavy geometry lighting.
The new SH lifetime candidate retained216 registers and showed no consistent
speedup; scalar neighbor addressing also lacked a general win. Both remain
private. Do not revive the already rejected SH predecode or earlier variants.

A separate source audit confirmed64 examined neighbor candidates in original1eb,
with only32 ACCEPTED taps. Native31 truncation could omit a late containing
probe/box. Repair4b23c0b345 preserves the accepted cap, order and bounds. Actual
forward regression fails late32/64 before repair and passes after; both optimized
API+GPU suites427464/98 pass, plus12 independent all-walker expected-color cases.
This restores quality, not a measured FPS gain. Normal app build passed; no new
live FPS sample. Best controlled9.61FPS remains below20FPS.

Next decision requires an expensive hero-forward capture: actual fragment
invocations and early/late-depth rejection, not an assumption that mirrors run
main PCSS. Consider conservative current-frame face-local culling only if it
can remove substantial measured work, preserving all six views and draw order.
[Full evidence](../../state/archive/CURRENT-shader-registers-and-neighbor64-2026-11-09.md).
