# Current engineering state

**Observed:** 2026-11-10, physical M1 Max/64GiB/macOS26.5.2/Xcode26.6.
**Branch:** `codex/apple-silicon-metal`. **Renderer code checkpoint:** `06d16eb4cb20f4dfe47b4ce71e86e1420eb9e060`.
**Working source:** exact06d16 direct default; rejected parallel prototype removed.
**Recovery:** [NEXT](NEXT_SESSION.md), [ledger](../work/active/metal-port.md),
[campaign](../work/active/ultra-20fps.md), [history](archive/INDEX.md).

## Goal and authority

Unbudgeted20FPS Ultra goal ACTIVE/unmet; infeasibility unproved. Autonomous
quit/build/login/measure authorized at the SAME private location/Preset123 only.
No travel/avatar movement. Prefer Mac shortcuts/fullscreen BEFORE login.
Preserve Ultra3456×2168/Midday/six hero1024faces/two cascades/PCSS/rawR32/
reverse-Z/clamp/casters and viewer functionality. No private uploads/releases.
Root owns shared source/docs/runtime/GPU; agents use private source/read-only
analysis. Translator FULLY STOPPED/NEVERrestart; Xcode replay STOPPED.

## Runtime and current decision

ALL viewers CLOSED; all three current environment plans restored. Exact68605
PID166 and controls68601 PID98164/4449 exited with SIGTERM/Goodbye/absence.
No profiler/GPU test/build running; all native validation/CPU trials ended0.
The restored-default source matches both completed optimized baseline suites. Canonical68606 is the rebuilt direct-default baseline (ac719b12/3932).
Experimental68605 is preserved at
/private/tmp/alchemy-metal-68605-material-table-trial-preserved.app (f3aa/3932,
deep strictsignature0). Preserved68601/f277/61c5 remains the comparison baseline.

The material cache has no demonstrated whole-viewer FPS benefit. Source now
uses material_buffers=false, original non-AB diffuse sampler, and a dedicated
lazy AB sampler only for explicit experiments. Filtering is unchanged; live
anisotropy changes retire that sampler safely. BOTH optimized API+GPU suites
pass435462assertions/102cases (63643all4phases0), packet9lcc2zss. Source review
passed; normal68606 doctor/configure/build/verify/signature all0 (39343terminal0).
Packet ys8mhyv4;349ARM64/0bad, exeac719b12, same3932 shader library. No liveFPStrial.

Normal source1407/68605 built via doctor/configure/build/verify/signature, each0;
root349 ARM64/0bad. Packet /private/tmp/alchemy-metal-material-table-app-build-20261110.y_8erv9x.
Executable f3aa9190ec08aa41656ee21147b167ad7cb101f68a60f9230fe82e57d7f2298e;
metallib3932018186338255b0523028441e5376393a51c2b3ddbad2ad538f1c530376c2.
Normal source recordingOFF/C++23/O3/NDEBUG/arm64/min26.0; independent review
of actual referenced response files/source stamp/app identity passed.

## Latest implementation decision

Whole-shadow parallel encoding was implemented and exercised with exact original
caster/order semantics. Root prepared immutable data; one worker and the main
thread encoded ordered native children. API/GPU validation passed435842
assertions/103cases, including failure/retry and two views before one commit.
Two280-sample quiet trials included complete preparation. Initial parallel CPU
cost regressed+19.20% at640 draws/+13.39% at4096. A validation-slot cache reduced
this to+9.71% at640; apparent4096 gain−4.45% overlapped SAMEparallel drift−5.24%.
No general benefit established: no viewer trial or production admission.

Both exact13-file candidates are frozen privately. Root reverse-applied the
prototype; all `indra` files match06d16. Normal68606 is unchanged. Ninja test
artifacts still contain the prototype: REBUILD before source-exact reuse.
Ten raw packets are SHA-verified under ignored
artifacts/metal/private-whole-shadow-encoding-20261110/.
[Tests, costs and limits](archive/CURRENT-parallel-shadow-encoding-2026-11-10.md).
The material cache remains explicit opt-in; its earlier micro wins did not yield
viewer benefit. [Cache history](archive/CURRENT-material-argument-buffers-2026-11-10.md).

## Viewer measurements and limits

Preserved normal68601/source77977290/exef277ea10/lib61c5:
/private/tmp/alchemy-metal-68601-normal-preserved.app.
Earlier properly gated300readyframes9507–9806:9.833798FPS,mean101.690106ms,
p50101.502709/p95106.397708. Prior best68594:9.635581FPS; one pair not causal.

Fresh68601 reference trial captured300consecutive frames9623–9922:
9.408707FPS/mean106.284533/p50106.109021/p95116.8194ms; native1/fallback0,
256ready/stable120/bias1/restorations true.43 end controls and EEP match prior;
main-visible probes96vs98 and draws−1.99%. Missing start gate and overlapping
unmeasured CUA stall make this EXPLORATORY, not a fully admitted paired baseline.
End observe/verify/gate0, exact98164 SIGTERM/Goodbye/absence and planrestore0.
Audit: /private/tmp/alchemy-metal-68601-fresh-baseline-audit-20261110.s90p5x7s.
Original67611/exact1eb+observer6.095519FPS; full parity remains open. Older68576/
68577 were contaminated by Translator GPU contention; no causal gain claim.

Properly start/end-gated repeated68601 control captured300frames9604–9903:
9.410236877FPS/mean106.267250553/p50106.547583/p95116.651958ms. Candidate68605
captured300frames6661–6960 with both43-control gates0:9.066674534FPS,
mean110.294021947/p50110.364396/p95119.195042. All native/exact/bias1/256ready120/
restorations pass. Camera/EEP exactly match; main-visible98 both, but candidate
mean draws+2.53%. Observed FPS−3.65%; not a fully isolated code regression.
Two control runs9.4087/9.4102 bracket candidate9.0667; no viewer gain demonstrated.
Audits ro6wnbu4/znv_94xq. Candidate/control2 plans restored after actual exit.

Post-sample10s CPU traces both recorded/exported0: candidate7.904sampledmainCPU-s
in10.641s; control7.705in10.456. Hero~50%active main in both. Stable shadow parent
2.459/2.381s respectively; narrower renderSpotShadow symbol attribution differs
with optimization, so .373vs2.351s is NOT a real sixfold work delta. No exact
sampledCPUms/frame; inclusive categories overlap. Packets rf4lnbpm/84up2rjj.

## Diagnosis retained from actual captures

Continuous prior12min CPU trace:920867samples; main sampled CPU/frame roughly
77.6→76.5ms while throughput fell11.8→10.75FPS. Main activity92→79%, P cores;
Nominal thermal720.726s, clocks unknown. This is not a current68605 CPU floor.
GPU completed-command sums226→473ms overlap/lag; not GPU active union or causal
shader times. Probe packs grew2→98/main and256/hero as draw counts plateaued.
Prior live diagnostic GPU union~105ms; mainbase/hero/haze inclusive times overlap.
Hero simple alpha245regs/304spillbytes/low occupancy; specialization224regs,
SAMEspill and flat viewer result. Haze encoder includes later alpha; haze FS1.79%.
Xcode replay states/serial counters do not substitute for runtime frame times.
Rejected SH/predecode/named-accumulator/scalar-cursor/simple-mask/ambient variants
remain rejected. MainUltraSSR0; no repeat of irrelevant SSR optimization.
Exact-original64candidate/32accepted neighbor repair4b23 stays; oldbest31 was wrong.

## Apple research and owner correction

No matching published AGXG13 driver bug found in bounded primary-source research.
Apple documents binding/residency cost. GameMode activation was only a possible
launch-path confound; OWNER reports seeing its fullscreen activation notification.
Leave it unchanged and de-prioritize that lead. Do NOT retry ControlCenter CUA
binding: unsupported target stalled7654s despite a60s timeout; no mode changed.
[Research and correction](archive/CURRENT-apple-metal-research-2026-11-10.md).

## Exact next actions

1. Stage-timestamp fixture veybvjku compiled/ran0 with API+GPU validation:
   eight actual samples/two passes, exact pixels/cancel and clock calibration.
   Integrate the private bounded/default-off timing helper and native hooks;
   collect actual frame/view/face/pass intervals after the formal-ready gate.
   No unsupported per-draw/statistic/utilization sampling or stage-time sums.
2. Do not revive the rejected parallel prototype or rerun its cost variants.
   Immutable regular probeUBO1 was also rejected after200strict/168timing pairs.
3. Continue GPU attribution and general CPU/GPU work toward20FPS, preserving all
   original features. Completed-command sums and overlapping stage intervals
   cannot be treated as whole-frame GPU active time.

Retain binding cache/arenas/main occlusion/Ultra4xaniso/bounded lights/directR32/
borrowed targets/image-global memory accounting/cubeSSAO/per-view probe repair.
NativeD32 changes raw-depth reference; no migration/epsilon waiver. Parallel
shadow encoding was GPU-tested but rejected on cost; ordinary-face batching is open.

## Complete-product advancement assessment

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-004 Resource ownership | doing | Completion-safe material cache validated; actual net frame benefit open. |
| M-008 Advanced views/effects | doing | All six hero faces retained; mirror GPU/CPU cost remains open. |
| M-012 Native UI automation | complete | Exact viewer fullscreen/login/camera flow; avoid unsupported system helpers. |
| M-014 Ultra20FPS campaign | doing |Direct68606 restored; parallel candidate rejected; stage timing is next. |

Full pixel/feature parity, multi-scene performance, prolonged stability, strict no-GL
and independent build/review evidence remain OPEN. No OpenGL/Vulkan/Rosetta
fallback or premature product-completion claim is authorized.
