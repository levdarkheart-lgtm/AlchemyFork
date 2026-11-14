# Apple-first architecture experiment: integrated G2–G5 candidate

2026-11-14. Branch `codex/apple-first-architecture-g2-g5`, based on
`5836d41753fc3e09533d638748ac9756f89ac357`, including owner-confirmed G1 repair
`fef1e756fd`. This is the native Metal fork. Original Alchemy
`1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` supplies behavior references, not a
renderer fallback. The [executable ledger](../work/active/apple-first-architecture-g2-g5.md)
retains implementation history, failures, exact commands and next actions.

Final compiled code/test source: `6baeb0340a1ddeb89aa265a1a64321ad1e0cd57a`.
Renderer implementation is `d5ae05f2ffec7c243667d0414b5fc003a9164fb4`; the final
commit changes only two buffer fixtures to cover safe-current and leased-retired
reuse. Release configuration, doctor and required build wrapper pass for
`Alchemy Metal Alpha G2G5` version`26.4.0.68995`. Main executable SHA256:
`96dcc743c9bfdd471984dd7843639137eeb1c4c3eb77dfd722e232ce6124e2ae`.
Evidence is under `artifacts/metal/apple-first-architecture-20261113/`.
The combined implementation/comparison/build delivery is complete and ready
for the owner's runtime assessment. The tracked experiment does not claim full
viewer architecture or product-release completion. No agent viewer/window/audio
launch, login, interactive profile/capture or push occurred during that delivery.
The later owner-authorized attached profile is recorded below.

Staged application:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-architecture-20261113/staged/Alchemy Metal Alpha G2G5.app`.
The existing G1 and other viewer apps were preserved.

**Runtime assessment,2026-11-14:** the owner subsequently requested a five-minute
profile of this delivered app. [Whole-engine analysis](g2g5-busy-club-profile-2026-11-14.md)
records17.77FPS versus20.36G1 at matched published resolution/saved controls,
substantial serial source bookkeeping and streaming/publication work, and zero
logged successful opaque ICB execution. PPD-110 remains investigating. The
architectural performance objective is unmet; the delivery/headless results
below do not establish a live viewer upgrade. No code fix or new capture was
performed during that analysis.

## Concrete ownership and execution changes

| Boundary | Integrated behavior |
|---|---|
| `LLPipeline::admitMetalShadowSources`, `captureMetalShadowScene`, `LLDrawInfo` | Admit each view's ordered draw-info references once, retaining existing family/query barriers and pool completeness. Cache a producer tuple qualified by actual geometry/index generations, material/coverage values and native texture publications. Model/view/pose are resolved separately at capture time. |
| `ALMetalShadowSourceRecord`, `ALMetalShadowSourceCache`, capture `ShadowSourceLease` | Immutable native records own real buffer/texture generation leases. A command aggregate pins distinct source and pose records; workers never receive live draw infos, avatars, cull maps or gGL. Bounded cache admission resists capacity-plus-one scans. Weak overflow handles reuse only a record still owned by an actual packet/command. |
| Small shadow families | Consume the immutable producer tuple directly on the owner, borrow frozen coverage/current pose, use existing LLMatrix4a MVP composition and ordinary native validation. Avoid the extra per-index preparation packet and public-certificate comparison that failed complete-cost gates. |
| One preparation family with at least8192 inputs | Compact raw instances plus one copied view/projection context feed at most two synchronous Dispatch tasks. Preserve operation order, source order, current pose and model snapshots. Epoch cancellation rejects the entire result, never a valid-looking prefix. |
| `ALMetalWorldGBufferRenderer::renderSpotShadow` | CPU-encoded native indirect commands and completion-owned shared data are selected only for eligible contiguous opaque runs of at least2048. Static and certified attachment poses qualify. Query/state boundaries,16k packet limits and ordinary paths remain authoritative. |
| Covered/PBR, body/cloth, terrain, short/fragmented/oversized runs | Keep existing native encoding, coverage rules, tessellation and view-dependent pose behavior. The opaque selector does not force covered argument tables or alter their coalescing. Optional indirect preparation failure leaves scalar casters available. |
| `LLVertexBuffer`, `LLImageGL`, accelerated media | Shared generation owners prevent storage reuse while CPU packets or GPU work reference it. Safe unleased/completed buffer generations publish changed byte ranges; leased generations use full copy-on-write. Texture uploads/mips preserve leased generations. IOSurface media publishes a fresh completed mirror when the previous generation is leased. |
| Shadow pass lifetime | Store/sample authoritative R32F shadow data; use discard-only memoryless D32 for depth testing. No depth import or additional retained image copy. |
| Shadow scheduling and query publication | Stream independently owned shadow views to preserve CPU/GPU overlap. Bound two pending logical shadow groups and six commands per group; ordinary cube faces have distinct groups even when they share gFrameCount. Borrow existing outer owners. Publish validity/cache/query results only after successful native commit; cancellation discards them. Actual GPU completion controls reuse. |

Cache bounds are8192 entries, two-display-epoch inactivity expiry and estimated
unique native-wrapper bytes clamped to256MiB–4GiB from device working-set advice.
Active command leases are separate. The shared constant/palette slab pool is
bounded to32slots/128MiB of owned buffers; fallback arena, temporary vectors,
texture retention and driver overhead are outside that pool bound. These are
explicit ownership bounds, not a measured process-memory reduction or zero-copy
claim. Existing CPU staging and non-rendering Alchemy features remain present.

## What the fixed comparisons establish

All timing is headless native mechanism evidence on the observed M1 Max64GiB,
macOS26.5.2/Xcode26.6 host. Validation-enabled correctness and validation-disabled
timing are separate. CPU numbers are elapsed construction/encoding/commit/
same-arm-retirement costs, not the sum of every worker's core time. Completion
wall includes actual native completion; GPU timestamp envelopes are not GPU busy
time and may overlap. No FPS or live-scene speedup is asserted.

| Accepted mechanism | CPU median / p95 | Completed wall median / p95 | Consistency |
|---|---:|---:|---|
| Compact preparation,8192 inputs, serial versus two tasks | −17.72% / −14.88% | Preparation-only gate |10/10 positive block medians |
| Retained owner/large-family route,6views ×8192 rigged PBR instances,16sources | −8.50% / −8.68% | −7.55% / −6.89% |10/10 CPU and wall blocks; all50 paired wall quartets positive |
| Opaque-only selector in a mixed4608-draw view, all visible | −17.20% / −11.24% | −13.72% / −7.68% |10/10 CPU and wall blocks |
| Same mixed view,25%offscreen control | −18.02% / −14.67% | −14.42% / −8.78% |10/10 CPU and wall blocks |

Source comparison:800 exact samples; original three small/mixed cases plus one
explicit8192-instance rigged PBR family. Its smaller cases show no consistent net
benefit: CPU median changes+4.53%,+0.39%,+0.04%; wall changes+0.33%,−0.47%,+0.27%.
The strong raw control already shares current COW coverage and omits historical
per-draw material reconstruction. Both arms include cold source seals and their
actual command ownership costs. Live LLDrawInfo key/LRU/admission/cull, pose solve
and unique-source workload distribution are excluded.16-source reuse does not
prove whole-pipeline performance for8192 unique draw infos.

Mixed-selector comparison:400 exact samples,100per-arm/control,10ABBA/BAAB
blocks. Each frame streams six commands and executes exactly24576 opaque records
in12selected runs, with12slab publications/96pose copies and zero covered-table
creates/misses or GPU generation. All native depth/query/caster checks pass.

Capacity stress uses the actual extracted cache:8192capacity/8193unique sources.
Old LRU hits zero and admits8193 misses on each subsequent scan. Protected
admission hits8192 and admits zero overflow records on all five subsequent scans,
with constant8192entries/262estimated unique bytes in that shared-buffer fixture.
This proves bounded admission behavior, not live throughput. Overflow may still
rebuild after its actual command owners disappear or the viewpoint turns over.

The existing repaired-G1 club capture contains cumulative counters, not per-run
or per-family distributions. It cannot establish whether this club triggers
one2048-draw eligible run or one8192-input family. Production static opaque capture already coalesces compatible sources through
`ALMetalShadowCaptureAdmission::appendOpaque`; the2048 selector counts the
resulting native records in one eligible run, not raw LLDrawInfo admissions or
cumulative sources. The8192 preparation gate uses one admitted source-family
range before that append step. Threshold activation and actual frame-time/FPS/
latency/visual parity require the owner's combined-app assessment.

## Rejected variants and retained evidence

- Initial shared-output preparation made Dispatch slower; borrowed outputs and
  compact context changed the implementation. The compact2048 case still loses,
  so its old accepted owned-input threshold was not carried into production.
- Initial GPU ICB copying failed depth parity (PPD-095). Real GPU command emission
  repairs correctness, including ordered multi-tile/query runs, but serial
  emission caused severe completion-time regression (PPD-100). Parallel emission
  repairs that mechanism without establishing a default performance win.
- Classic GPU-generated visibility and native Metal4 compute/argument-table/
  residency/event execution pass tested output/lifetime checks but fail net-cost
  controls on this target. They remain off. Uncertain bounds stay visible.
- Global shared-data mode loses on covered/PBR direct work; only selected opaque
  packets use it. Covered work keeps the existing native path.
- One six-pass command saves commit CPU but loses useful overlap: busy completion
  wall regresses27–35% (PPD-101). Streaming stays enabled.

Benchmark defects and failed repair attempts are retained: double material-cache
frame aging (PPD-102), an invalid blocked-reader/reload oracle (PPD-104), and an
incorrect ordering assumption about overlapping GPU interval sums (PPD-105).
The latter required analysis-only repair of the same800 rows, with no native
rerun. Exact source/binary hashes, old parsers, tracebacks and adverse results
remain in the evidence directories. The lifetime oracle now gates an actual
tracked geometry dependency: an event wait applies to subsequent passes in its
command buffer, so a detached empty wait is not evidence that a reader remains
pending. See Apple's [event-wait contract](https://developer.apple.com/documentation/metal/mtlcommandbuffer/encodewaitforevent(_:value:))
and [resource synchronization rules](https://developer.apple.com/documentation/metal/resource-synchronization).
The corrected fixture verifies actual reader completion and packet ownership;
reload is tested separately because its initialization performs synchronous GPU
uploads. Build/test success alone does not close owner-pending records in the
[defect register](../POST_PORT_DEFECTS.md).

## Validation and remaining architecture path

Latest coherent native correctness passes81,815 assertions/39cases, plus351/7
resource checks, with Metal API/shader validation. It covers output/query parity,
current pose/native texture changes, whole-result cancellation, actual blocked
GPU readers, renderer destruction, completed reload, weak ownership, many probe
view groups, cache pressure,16k+tail preservation and short/fragmented/raw-pose
exclusions. Actual pipeline and full application compilation pass, as do
static346-Mach-O ARM64/signature/source/native
import/retained feature/system exception-RTTI checks pass. The real voice/codec
exception ABI and bounded240-request decoder corpus also pass. The first broad
run passed57/60CTest and9/10exact headless cases, temporarily holding delivery.
Those original failures are retained. Source review attributes the generation/label failures to obsolete
retired-first expectations and the archive failures to incompatible test
environments. The repaired two-fixture and native-generation suite passes573
assertions/11cases, covering both safe-current and forced leased/retired reuse.
Separate archive disk5905/6, actual instrumented bypass882/1, selective validation
98/1 and escaped exact PBR terrain128/1 pass without skips. The first terrain
exit5 selected no case because its comma needed escaping; no terrain source
change was made. Original failures and corrective probe receipts are retained.
The final-source corrected broad run and separate app staging also pass; the
original failure receipt remains evidence, not a substituted successful run.

Final receipts under the evidence root:

| Gate | Observed result / receipt |
|---|---|
| Final Release configure | Exit0,4.15s, exact6bae source/cache and68995 identity; `final-release-configure-2.json`. All30 baseline feature flags, ARM64, Release-O3, LTOoff, channel and pinned OpenHTJ2K checks pass. |
| Required doctor / build | Exit0/0,0.54s/25.90s final refresh after the earlier933.97s full build; `final-release-build-2.json`. Actual wrapper `tools/metal/build.sh build`, Release,8jobs. |
| Corrected broad suite |59/59 source-vetted CTest groups,10/10 exact native specializations,3/3 archive-policy processes and private corpus all exit0;15/15 command stages,563,974assertions/301cases; `final-broad-contained-2.json` plus manifest/inventory/logs and `final-regression-summary.json`. The60th original grouped cache entry is covered in its three required environments, not omitted. |
| Voice / codec boundary | Actual-library exception ABI entry passes in the broad suite; bounded private80-input/240-request corpus passes. No voice/audio device/session initialization. |
| Built and staged static verification |346Mach-O entries allARM64, strict deep signatures, source/version/channel, native owned viewer/CEF/VLC-host imports, retained music/voice/codec symbols, exact OpenHT archive/receipt/license and system exceptionRTTI/0private typeinfos pass; `bundle-static-verification-final-6bae.json` and `staging-final-6bae-static.json`. Foreign browser internals/runtime remain separately unverified. |
| Production source GL audit |3548first-party files,0blocking/0scanerrors;901informational legacy-name rows; `source-gl-audit-6bae-receipt.json` / `source-gl-audit-6bae-production.json`. Tests excluded as recorded. |
| Separate app copy | `ditto` exit0;7839regular files,2symlinks,432directories/795,773,875regular-file bytes match source-before/staged/source-after for paths, hashes, links and modes; `staging-final-6bae.json` and three full inventories. Original unchanged. |

Staged inventory tree SHA256 is
`2f32f721d45a6cf3cedb9958abb23e510543e98dab849f62c3257f5050d98cf2`.
This inventory digest is distinct from the executable SHA256 above. All source,
build, test and stage metadata were captured with HEAD frozen at6bae; subsequent
local documentation-only commits do not change the compiled app's source pin.
No tool source changed. Documentation harness33required files and whitespace
checks pass. Exact commands, environments, exit statuses and hashes are retained
in the receipts; overlapping test counts are not summed as independent coverage.

For the owner's assessment, start the staged G2G5 app manually and check About
shows26.4.0.68995. Compare with the preserved crash-repaired G1 app at the same
club, camera, window size, shadows/graphics settings and warmed texture state.
Check moving attachments, alpha/PBR shadow coverage, ordinary reflection probes,
texture updates and media/voice as well as responsiveness. A difference caused by
quality, camera, population, cache or environment does not establish a renderer
speedup. No agent launch or new profiler is required for this delivery.

This experiment establishes a usable immutable boundary inside the Second Life
shadow producer and native encoder. It does not replace the entire viewer's
threading architecture. LLSpatialGroup/LLCullResult update, stateSort/postSort,
scene mutation and most encoding remain on their existing owners. A future
GPU-owned visibility phase must consume persistent dirty-generation sources
before the repeated per-view CPU discovery, rather than adding a second cull
after CPU admission. It also needs proved conservative moving/rigged bounds,
query/order parity and a net frame-time gate. Main-view/resource publication can
then extend the proved ownership contracts at their actual source boundaries.
These are further implementation/assessment requirements, not performance
already delivered by API selection or nominal hardware utilization.
