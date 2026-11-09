# Current engineering state

**Observed:** 2026-11-09, physical M1 Max / 64 GiB / macOS26.5.2 / Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `d967552ae823ae6d1cc7c979c8528c4fe10fc5e8`.
**Recovery:** [NEXT](NEXT_SESSION.md), [ledger](../work/active/metal-port.md),
[campaign](../work/active/ultra-20fps.md), [history](archive/INDEX.md).

## Owner goal and honest result

The unbudgeted20FPS Ultra goal is ACTIVE and unmet. The owner permits autonomous
quit/build/login/measurement, staying at the current private location and
numeric Preset123; no travel. Prefer Mac shortcuts. Preserve original quality,
views, cadence, shadows/casters and functionality. No private uploads/releases.

After11hours the owner questioned the lack of progress. There is real CPU relief
but no established FPS improvement. Latest combined trial9.19FPS, previous
optimized8.86FPS, earlier settled9.27FPS. No repeatable isolated gain or hardware
infeasibility is proven. Do not credit tests or profiling as FPS achievement.

## Default code and app — all viewers CLOSED, user state RESTORED

`59177` repairs empty Xcode Release compiler-cache defaults. All1607actual
compile actions across36targets now use ARM64/O3/NDEBUG; previously27targets
wereO0. Main-thread Running fell91.476→72.768% of one core, without an FPS gain.

`62e2` binds only populated forward material/PBR prefixes, preserving sRGB/linear
views, shader bounds and restart ordering. Scalar map setters32→4PBR/24→3legacy;
actual native calls depend on the cache. Focused8918/1 and full391162/89 EACH
Release/RWDI pass real API+GPU validation. Shader bytecode is unchanged.

Opaque/MASK trial `d3d139` was reverted by `d967552` because its single combined
viewer window did not establish a repeatable benefit. Trial commit, private
patches and app remain available; this does not prove no other workload benefits.
Default renderer/test sources match62e2 exactly. Ninja test binaries still
contain the trial; rebuild them before any new source-specific test claim.

Default app68569 configure/build/verify349ARM64/deep-strict signature all0:
`build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
Executable SHA256 `0e4a0b138ceec2fd1d37ca72a85fa8c2a05d9a0dd1c734116b3074831951b4f4`;
metallib `8bbfe4c6f3ce45eabd4fb58049cec413178213da480b23020447f01ef864b943`.
This rebuilt default has not been launched/measured. Root owns shared source,
tests, documentation and runtime; agents are read-only/private, all finished.

## Latest controlled trial68568 — no admitted optimization

Source `d3d1393d97052418bf39bf80cc3ca007a4f00279` added matching opaque PBR PSOs
without discard, as exact1eb's opaque/MASK split does. Generic MASK remained
unchanged. Independent reviews and full API+GPU suites EACH392926/89 passed.
Private timing had a drifting identical control; no percentage benefit credited.

Full screen BEFORE login, actual Ultra6/3456×2168/Midday/Preset123,43live controls
PASS. Exactly300consecutive frames4968–5267, all native/zeroGL, all256probes
complete/faded/current and stable120. Desired discard bias4 for all300rows.
Mean108.776442ms, p50108.786063ms, p95113.496416ms,9.193167FPS.
Against68566: mean−3.57%, median−1.65%, p95−17.93% in one combined sequential
window; cannot isolate binding versus shader effects or claim repeatability.
Camera basis/projection/environment match; vertical difference only0.0057mm.
Main visible probes97versus96 and draws+0.432% do not establish a scene change.
No new broad profile or post-packet capture. World AX timed out; no screenshot
or visual pixel-parity claim. Seeded Midday and collected sun metadata match.

CSV `artifacts/metal/private-d3d139-opaque-ready-20261109-01.csv` and matching
`.provenance.jsonl`; QA run `candidate-d3d139-opaque-ready-20261109-01`.
PID44908 gracefully exited/Goodbye11:35:33UTC; PID absence verified. Temporary
state restored11:35:55UTC; d3d139startup plan cleanupDONE. All earlier plans DONE.
[Complete evidence](archive/CURRENT-68566-68568-performance-trial-2026-11-09.md).

## Remaining measured costs and quality limits

Clean68566 profile: main CPU72.768% of one core; GPU viewer-active interval
union109.245ms/present,83.169%wall, nominal,4.389–4.457GiB. Main base17.515,
haze17.027 and hero above-water21.648ms/p are inclusive overlapping labels,
including later draws. Never add them or call them individual shader timings.
Earlier wrong-scope/overlap GPU attempts were excluded and preserved explicitly.

Settled68564:9.270128FPS, bias not sampled per row. Exact external1eb original
with additive observer:6.095519FPS, all256ready,3456×2168/43controls matched, but
bias1 versus native4 and main visible104 versus96/97 leave quality parity open.
Original is closed/restored. No scene change is assumed.

## Active implementation checkpoint

KEEP per-view probe-space correctness; stale68558/10.064FPS is not a quality
baseline. Preserve shiny full-hero bypass, occlusion, Ultra4×anisotropy, bounded
lights, directR32shadows, borrowed targets/depth, projector ownership, caches,
arenas and retired geometry. Hero remains6×1024faces/two cascades, raw PCSS,
reverse-Z/clamping/alpha casters. Texture pressure scope repair stays REVERTED;
all-resource accounting against the small image budget remains unresolved.

Layered shadows, masks, ordered lists, matrix memoization, SH predecode and
scalar specialization were rejected/deferred for regressions, poor applicability
or no repeatable benefit. Avoid another speculative shader variant from noisy
microtiming. Palette-pool private proposal remains unintegrated.

## Complete-product advancement assessment

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-008 Advanced views/effects | doing | Preserve reflected-view correctness; broad parity open. |
| M-012 Native UI automation | complete | Login/fullscreen verified; world AX can time out. |
| M-014 Ultra20FPS campaign | doing | CPU relief proven; no established FPS gain. |

## Exact next actions

Next work needs a bounded source-grounded change addressing substantial remaining
GPU/CPU work, with varied correctness proof and a repeatable control. Do not
repeat broad profiles without a specific unanswered question. Reaching50ms from
roughly109ms needs much more than the rejected trial;20FPS is not proven feasible
or impossible. All viewers closed, no measurement/build process pending.

Multi-scene performance, feature/visual parity, prolonged stability, strict no-GL
and independent build/review evidence remain required. No release claim.
