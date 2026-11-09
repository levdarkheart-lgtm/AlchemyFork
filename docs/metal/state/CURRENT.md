# Current engineering state

**Observed:** 2026-11-09, physical M1 Max/64GiB/macOS26.5.2/Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `b52c1ba96913fecc4c81f082ce97bffd6b43f6eb`.
**Working source:** texture-usage repair built/verified; controlled68581 RUNNING.
**Recovery:** [NEXT](NEXT_SESSION.md), [ledger](../work/active/metal-port.md),
[campaign](../work/active/ultra-20fps.md), [history](archive/INDEX.md).

## Goal and latest result

The unbudgeted20FPS Ultra goal remains ACTIVE/unmet. Autonomous quit/build/login/
measure is authorized at the fixed private location/numericPreset123 only; no
travel. Prefer Mac shortcuts. Preserve Ultra3456×2168/Midday, all six hero1024
faces/two sun cascades, PCSS/clamp/casters and functionality. No private uploads.

Latest controlled68579/source1f2cc0 completed300 consecutive frames5032–5331:
**9.608672762FPS**, mean104.072646ms, p50103.8956665, p95109.0435ms. All native/
noGL/Release/Ultra3456×2168; bias1;256 probes fully ready/stable120; main-visible95.
Fresh start/end43-control/location/exact-binary gates0; environment/sun/hero/HUD
restoration matched. Sampled camera changes0; basis/projection matched; vertical
position differed30.52 micrometres from68572/68577. Drawmean55781.49, +3.591%vs68577.
No build/test/profile/heavy parsing overlapped the formal window.

FPS +19.94%vs68577(8.011210), +42.80%vs68576(6.728847), +2.414%vs68572(9.382155).
Translator GPU work was removed for68579: this is a changed control, not pure
source attribution. Target20FPS is unmet; hardware infeasibility is unproved.
RawCSV/provenance: artifacts/metal/private-1f2cc0-family-diagnostic-20261109-01.*.
Independent comparison SHA39a27d619640cdf02a3db9d85769f145beca2323a5a41b128e70773b25bc00ce.
Images7599MiB/device9483–9510/target45985–46006/recommendation59392/system-low0.
Native query119002 spatial skip EVENTS/299 intervals, not unique groups/savedtime.

## Runtime and external service

Previous68579 PID78103 gracefully quit, Goodbye/PIDabsence verified, then1f2cc0
Midday plan RESTORED (all earlier plans restored too). Current68581/PID97769 is
RUNNING; fresh b52c1b Midday plan INSTALLED, not restored. Fullscreen before login;
start live43control/location/binary observe/verify/gate0. Warming256 probes; do
not restart or run GPU/build/profile/heavy parsing during formal collection.
Canonical68579 and preserved `/private/tmp/alchemy-metal-68579-controlled-measured.app`
executable bf4cdfef2fe95cf91991f26e4f948328878fa8627871100825d54b11fe6d69f5;
metallib ffd9e1d178988b0719c516cd1e87444907d59d063630d7fd9ac2d17cafaa9347.
Preservation ditto/hash check0. Doctor/configure/build/349ARM64/signature0;
full426444 assertions/91cases EACH Release/RWDI API+GPU0. No screenshot claim.

Translator user instruction is FULL STOP, superseding prior pause/resume.
`uv run --no-sync python scripts/service.py stop` from its project exited0:
API35823/tailscaled35866 stopped; GPUworker35824 absence verified. DO NOT RESTART.
No SIGSTOP was issued. Earlier68577 GPU trace attributes33.98%wall to Python35824,
7.30%outside viewer. Stop record private-1f2cc0-translator-stop-20261109.json.

## Measured GPU attribution

Post-formal68579 diagnostic trace75165 and serial exports/parsers all0.131
presents/15.706s are instrumented8.3405requests/s, separate from formalFPS.
Viewer Active GPU interval UNION103.5406ms/p (86.3584%wall), device87.4914%,
thermalNominal. Initial forward family union2.61622ms/p: avataralpha2.16112,
fullbright.31048, glow.17212, shiny.06002. Six contiguous encoder runs preserve
draw order; added load/store boundaries alter scheduling/cost. Do not equate
these with unsplit production cost or individual shader duration.
Largest inclusive contexts: heroabove-water21.067, mainbase17.333,
mainhaze/latertransparent15.716, mainGbuffer9.108, heroHaze8.333,
heroGbuffer7.616, heroShadows6.271ms/p. Labels overlap; never add them.
Translator actor has zero Active rows. Other GPU activity still exists;
nonviewer outside union.77878%wall. Family attribution FINDINGS SHA
 eebc1718407318251aae95dd77241329fe462974e7979e6f537c1e4a757dfa13.
[Complete source/build/runtime evidence](archive/CURRENT-probe-ssao-and-shadow-gather-2026-11-09.md).

## Active implementation checkpoint

Root owns shared source/docs/runtime. New resource.mm candidate removes ONLY
automatic PixelFormatView usage for compatible sRGB/swizzle views. Apple says
these views need no flag; the flag can disable lossless compression. Formats,
bytes, view identities, swizzles, actual ShaderWrite/Read/RT flags unchanged.
Original1eb imagegl2873–2946/fetchedgltfmaterial86–159 preserve uploaded encoded
bytes and select color versus raw-data decoding at bind. Native views preserve
that contract. Ordinary radiance already shader-read-only; hero ShaderWrite is
required by real compute writes. No radiance format/precision reduction.
GPU tests cover combined RGBA/BGRA sRGB+swizzle, both actual view reads, raw byte
preservation, privateR8 alpha+mips, resampler read|RT and BRDF read|Write. Two
read-only audits support candidate; Release and RWDI API+GPU426552/92 EACH actual0; app68581 build/349ARM64/
deepstrictsignature0. Exact b52c1ba969 source; measurement in progress.
Eligibility is not proof of compression ratio, bandwidth saving or FPS gain.

Keep0fa4e3 original cubeSSAO parity repair: neutralAO1/no face blur, mainSSAO
unchanged. Keep30715f rawPCSS gather: original2×2footprint/order/math/8+12samples,
exact raw/full-map tests and22–29% lower private PCSS fixture, no4×bandwidthclaim.
1f2cc0 defaultOFF post-sample family diagnostic changes no formal timing pass.

PrivateD32 feasibility firstgate8960queries+96maps exact/API+GPU0. Geometrygate
q6gN3i actualcompile0/run79366exit2: old scratch hardwareB and fragment-freeD32C
bit-exact all896production+192bias cases; R32exportA differs fromB in221cases/
5522pixels. Only1208production mismatch values logged; maxloggedgap4.768e-7,
no uniform1ULP/epsilon waiver or global nonfinite/max claim. Original stores
hardware depth, making B/C source-grounded direction, not completed migration.
Alpha/cloth/terrain/receiver/pixel gates remain; private only, no production edit.

## Retained quality baseline and constraints

Preserved68572/source46d622 baseline9.382155FPS/mean106.585321/p50105.948188/
p95110.883167,bias1;68576/source0fa4e3 6.728847;68577/source30715f 8.011210.
Prior external GPU controls were unrecorded. Keep3969405 image/global scopes and
46d622 C=max(R−max(A−I,0),0), then original divisor/headroom ONCE; manual/global/
system controller unchanged. Normalbias1 is not per-texture/pixel parity proof.
Keep per-view probe correctness/shiny bypass/bindings/real occlusion/Ultra4×/
bounded lights/directR32 shadows/borroweddepth/projectors/arenas/caches/retirement.
Original67611/exact1eb+observer6.095519FPS,bias1/main104; broad parity remains open.
OldCPU68566 trace lacks matchedframecounts: no77ms CPUfloor claim. Rejected
classic/SSR/materialsplit/layered candidates remain rejected; no repeated trials.

## Complete-product advancement assessment

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-004 Resource ownership | doing | Image/global scopes corrected; compression eligibility candidate. |
| M-008 Advanced views/effects | doing | Cube SSAO parity fixed; main and mirror GPU costs remain. |
| M-012 Native UI automation | complete | Exact fullscreen/login verified; world AX/capture timeout limits. |
| M-014 Ultra20FPS campaign | doing | Controlled9.61FPS with Translator stopped;20FPS unmet. |

## Exact next actions

Finish current68581 controlled300-frame comparison against68579, Translator
STILL STOPPED, same camera/Midday/Ultra/ready256probes/bias1. Do not duplicate
run: candidate-b52c1b-texture-usage-20261109-01. After endgate/actual trace exits,
gracefullyquit97769/Goodbye/PIDabsence BEFORE restoring freshb52c1b Middayplan.
No agent GPU/build/runtime work without root release. Broader feature/pixel parity,
multi-scene performance, prolonged stability, strict no-GL and
independent build/review evidence remain OPEN. No release/product-complete claim.
