# Current engineering state

**Observed:** 2026-11-09, physical M1 Max/64GiB/macOS26.5.2/Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `0d5d49c188f8bfee4dd866adbac757af750340d4`.
**Working source:** validated neighbor repair built; hero-forward capture extension under test.
**Recovery:** [NEXT](NEXT_SESSION.md), [ledger](../work/active/metal-port.md),
[campaign](../work/active/ultra-20fps.md), [history](archive/INDEX.md).

## Goal, authority and current runtime

The unbudgeted20FPS Ultra goal is ACTIVE and unmet. Autonomous quit/build/login/
measure is authorized only at the fixed private location/numericPreset123; no
travel. Prefer Mac shortcuts and fullscreen before login. Preserve Ultra
3456×2168/Midday, all six hero1024 faces/two cascades, PCSS, reverse-Z, clamp,
casters and viewer features. No private uploads/releases.

Viewer68590/PID80172 is RUNNING, exact source0d5d49 diagnostic app. Fullscreen
CtrlSuperF BEFORE saved-login Return; LEAP logged_in=true and fresh43-control/
location/exact-binary observe/verify/gate all0.3456×2168/Ultra6/Preset123;
new0d5d49 Midday startup plan is ACTIVE, NOT restored until verified shutdown.
Current capture is waiting for benchmark/disarm; probes are still warming.
All older plans restored. Translator remains stopped. No builds/GPU tests/Xcode
replay overlap this run. Root owns source/docs/runtime; agents are quiet.
CUA world AX recheck timed out/reset; no repeated world AX polling. LEAP/runtime
metadata verify login; CUA login phase completion summary sent. No scene-image
parity claim from this launch. Xcode profiling must wait until viewer closes.

Translator remains FULLY STOPPED by explicit user instruction. Its project
`uv run --no-sync python scripts/service.py stop` exited0; API35823,
tailscale35866 and GPUworker35824 absence verified. NEVER restart/resume it.

## Controlled viewer measurements

Best controlled68579/source1f2cc0:300 consecutive frames5032–5331,
**9.608672762FPS**, mean104.072646ms, p50103.8956665, p95109.0435.
Latest normal68581/sourceb52c1ba969:300 frames, **9.454638960FPS**,
mean105.768185ms, p50105.811167, p95110.90525. One pair does not establish a
causal regression; flag eligibility showed no FPS gain. Both Translator OFF,
43 control/location/binary gates matched, bias1,256 probes ready/stable120,
Ultra3456×2168. No heavy work overlapped formal samples. Camera basis/projection
matched; Zdelta26.7 micrometres, no within-sample movement. Main-visible96vs95.

68579 and68581 exact measured apps are preserved under/private/tmp. Earlier
68577 8.011210/68576 6.728847 had uncontrolled Translator GPU work; no pure-code
speedup claim. Original67611/exact1eb+observer6.095519FPS; full parity remains
open. Official performance gates still fail20FPS/other required scenes.
[Full comparison and hashes](archive/CURRENT-probe-ssao-and-shadow-gather-2026-11-09.md).

## Measured GPU cause

Post-formal68581 diagnostic viewer GPU interval union105.542ms/p, versus103.541
for68579. Largest inclusive contexts: main base17.33ms, hero above-water20.71ms,
main haze/later transparency15.91ms, hero haze/G-buffer/shadows also material.
These labels overlap and added diagnostic pass boundaries alter cost: do not
sum labels or equate them with isolated shader durations. Main initial-avatar
family5.35vs2.16 despite matching source counts; no universal avatar cost.
Old68566 CPU trace lacks matched frame counts; no77ms CPU floor claim.

Actual focused main-base Xcode replay/profile:1 command buffer,1 render encoder,
2 draws.31.17ms at MEDIUM PState is diagnostic, not live frame/FPS. Geometry
99.68% of command cost;7,176,832 fragment invocations,43.22 sampler calls/fragment,
37.460B ALU instructions. Integer-conditional50.11%, integer-complex8.33%,
F32 32.58%.216 registers,0 spilled bytes; occupancy UNKNOWN. Encoder ALU limiter
44.54% versus texture-read6.01%.7.04GiB L1 texture traffic is CACHE traffic,
not external DRAM bandwidth. Source-line costs remain UNKNOWN.
[Focused capture evidence](archive/CURRENT-selected-base-capture-2026-11-09.md).

## Active implementation checkpoint

479ff989 fixes diagnostic shader line flags and Xcode stale-output dependency:
source-record ON uses optimized line tables plus recorded source, with a real
compiler-options input stamp. Actual AIR has34,812 line locations/749 subprograms;
normal OFF metallib remains byte-identical ffd9e1d1…. Canonical diagnostic68587
identifies479ff989; independent ARM64 audit349/0 and deep strict signature exit0.
Saved full-build log says BUILD SUCCEEDED; original process exit is UNKNOWN.
This app was not launched; source-record cache remains ON, unsuitable as the
normal performance identity.68584 is separately preserved.

Private SH load/lifetime scheduling passed340 base bitwise pairs and172 oracle
pairs. Quiet224-sample timing found no consistent win: PBR−0.1866% matches
same-PSO control−0.1859%; all8 paired ranges cross zero. Actual tiny A/B/B/A
capture replay/profile completed:4 draws, both216 registers/0 spills, costs
49.98/50.00%. REJECTED. No production SH math change. Older predecoded-SH,
named accumulators, specialization, material split and layered prototypes also
remain rejected; do not repeat unchanged. Actual live Ultra SSR=0, so privately
validated SSR-sharing miss-path savings do not address this current workload.
[New experiment and build record](archive/CURRENT-shader-registers-and-neighbor64-2026-11-09.md).

## Exact next actions

Exact1eb manager packs up to64 neighbor candidates; class3 GLSL examines that
count while limiting ACCEPTED taps to32. Native base/legacy/water incorrectly
examined only31 neighbors. Rejected entries could hide a later containing probe
or manual box. Root changed only3 clamps31→64, keeping bounds/order/accepted cap.
Actual forward-image regression before fix exited42: late32/64 each failed,
9,159 other assertions passed. Green actual API+GPU run10932 exited0: all9,161 assertions passed.
No FPS improvement is claimed. Independent source review approved the contract
and asymmetric full-image oracle. Full Release and RWDI API+GPU suites each
passed427,464 assertions/98 cases. Independent12-case base/legacy/water fixture
passed strict expected-color and CPU-membership checks, including late64 and cap.
Normal app68588/source4b23: doctor/configure/source-recordOFF/build11888 all0;
independent349 ARM64/0 invalid and deep strict signature each actual0.
Exe44d83b1a…/metallib33bd2d7f…; full hashes in archive. No new live FPS sample.

Private scalar cursor also REJECTED:404 strict image pairs and16 neighbor
contract checks passed, but252 timing samples show mainPBR−0.55% against
same-PSO control−2.62%, legacy+1.30%, mixed+0.09%. All but the64-rejected
stress case have paired ranges crossing zero. No general gain or integration.
Next: measure hidden/overdrawn fragments in one expensive hero forward face
before implementing current-frame face-local culling. Preserve all six views.
Hero-forward diagnostic mode is now applied and independently reviewed.
Release build12288 and actual capture82486 passed3059 assertions/3 cases.
RWDI build55464 and serial validation27224 passed: EACH capture3059/3 and
normal API+GPU427476/95. Diagnostic app68590/source0d5d49 built20880 after doctor
and source-recordON configure, all actual0. Independent349 ARM64/0 invalid and deep strict signature actual0; QA dry-run0.
Actual live warmup is in progress; no hero routing/replay result claimed yet.

## Retained behavior and open release gates

Keep image/global pressure scopes3969405+46d622 (bias1), per-view probes,
shiny bypass, bindings, real occlusion, Ultra4× anisotropy, bounded lights,
direct R32 shadows, borrowed targets/depth, arenas/caches/retirement and cube
SSAO/gather repairs. Private native D32 differs from current raw-depth reference:
no migration or epsilon waiver. Hero occlusion needs independent face ownership
and complete occluder invalidation; no measured saving yet.

Xcode Color0 Auto RGB0..0.218 expands its intermediate linear-HDR preview.
Final source ordering matches original with one linear→sRGB conversion; no
proven color defect or look/settings change from the owner's question.

## Complete-product advancement assessment

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-004 Resource ownership | doing | Image/global scope repaired; broader lifecycle/caches remain. |
| M-008 Advanced views/effects | doing | Cube SSAO parity fixed; mirror costs remain. |
| M-012 Native UI automation | complete | Exact fullscreen/login verified. |
| M-014 Ultra20FPS campaign | doing | Best controlled9.61FPS; target unmet. |

Full feature/pixel parity, multi-scene performance, prolonged stability,
strict no-GL and independent build/review evidence remain OPEN. No evidence
establishes that20FPS is impossible; no product-complete claim.
