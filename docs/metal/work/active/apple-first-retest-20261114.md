# Apple-first retest readiness — 2026-11-14

Status: complete for bounded engineering delivery; owner runtime retest pending.
Owner explicitly requests indirect failure/rejection logging,
repair of the process-cap problem and needed related diagnostics/fixes, then
another owner run before deciding how to continue the remaining architecture.
No agent viewer launch, login, audio session, new profile or push.

Branch `codex/apple-first-architecture-g2-g5`, isolated worktree
`/private/tmp/alchemy-apple-first-architecture-g2-g5`; starting HEAD
`6e0eea5be7f41a71b17df5f16f120fbbbcf27660`, old compiled candidate6bae/68995
preserved. Actual original G2–G5 architecture remains partial; this checkpoint
must not be labeled whole architecture/performance complete.

## Concrete delivery

- Explain opaque selection with cumulative per-view reason/attempt/fallback
  counters, run-size/eligibility information and safe bounded failure reasons.
  Expose existing opaque counters, support/state and main/probe distinctions.
  No threshold lowering or forced GPU/Metal4 activation.
- Fix PPD-111 default legacy16GiB cap/system-pressure conflict. Distinguish OS
  availability, configured process limit and native/resource pressure, preserve
  explicit owner limits and true pressure safety. Automatic policy must scale
  to physical hardware and leave headroom; no unbounded/all64GiB setting hack.
- Add source-cache/Dispatch activation and create-list/COW publication census so
  next real run can explain reuse, dirty work and residency churn. Aggregate
  periodically; no per-draw spam, private asset IDs or raw user settings.
- Address only source-confirmed needed correctness/performance defects. PPD-112
  selection-budget edge may be diagnosed first; do not silently change rendering
  membership/order, lighting/projectors/probes, quality or raw-image callbacks.
- Required doctor/configure/build, meaningful contained memory/native/shadow/
  generation/voice-codec checks, exact appARM64/signature/source/import/feature
  verification and separate staging. Preserve68995 app and source evidence.

## Ownership

Root: pipeline.cpp, CMake/integration, final tests/build/static/stage, register,
ledger/CURRENT. Memory agent: llcommon memory policy/helpers/tests, appheap init,
MaxHeapSize64 setting and viewertexture pressure operands. GPU agent: world.cpp,
indirect stats/header and related native tests. Residency agent: viewertexturelist
create-list accounting, llimagegl COW stats/header and disjoint resource tests.
No concurrent native tests/builds until root serializes them.

## Original invariant and evidence

Original1eb44bbd and repaired native G1 retain explicit configured memory limits,
true pressure protection, decode completion/create-list callbacks and native
caster/query order. Original post-create redundant-load/downscale behavior is
source mapped. Native ICB has no GL equivalent; its optional path must preserve
direct membership and successful direct fallback while recording its reason.
Actual6bae pressure/cap and activation audit is in
[follow-up assessment](../../research/apple-first-architecture-audit-2026-11-14.md).

New evidence root `artifacts/metal/apple-first-retest-20261114/` (canonical repo,
ignored/private). Record exact commands/status/source and failed attempts.
At that initial checkpoint, next was to implement disjoint changes, integrate
and validate; stage a concrete next
candidate. Owner runs it afterward. Continue larger architecture based on the
whole-engine retest evidence, not merely successful compilation/tests.

## First contained verification, preserved failures

- Narrow llcommon_tests build passes0; memory-budget regression passes108
  assertions/7cases. New policy input uses synthetic rawOS availability for
  isolation, not invented historical OS-free data. Evidence
  `narrow-memory-build-1.json`, `memory-policy-tests-1.json`.
- Narrow almetal_tests/llrender_tests/newview_tests build passes0,614actions,
  230.657s; source file hashes unchanged. `narrow-native-build-1.json`.
- API+shader-validated focused correctness attempt exits1 overall:
  opaque/wrapper filter3cases2pass1fail (399assertions/1failure), rigged family1
  renderSpotShadow returnsfalse at generation fixture760; native generation
  filter8 cases7pass1fail (363assertions/1failure), authored mip fixture rejected
  by createMetalTexture at318. Object residency CTest passes0. Do not infer
  PPD-113/114 repair accepted or publication correctness from build success.
  `focused-correctness-1.json` and separate logs retained. Assigned original
  source owners to investigate without concurrent builds/source overlap.
- Independent opaque/memory/residency integration reviews found no blocking
  source issue before actual native tests. Controlled typed failure injections
  are synthetic, while wrapper allocation injection exercises actual unwind.

## Source freeze and corrected focused verification

PPD-115/116 identify the two newly added fixture errors. Corrected only fixtures:
attach the current immutable rigged pose removed from source prototypes; create
an authored mip chain with the required enabled policy before switching future
partial-update policy through the existing public setter. Keep all parity and
failure assertions; new lower-mip byte comparisons strengthen coverage.

`narrow-native-build-2.json` passes0 (2.932s); focused second run passes0:
opaque diagnostics/wrapper633assertions/3cases, generation481/8, object residency
1CTest group. Native API and shader validation enabled, no skips, no viewer.
First failed receipts remain. Memory policy108/7 previously passes.

Compiled source frozen locally as `4918fd7ce3976576eababff430910d73017c9bdb`.
Final configure passes0, source SHA exact, version count 68999. Required doctor
passes0. At that source-freeze checkpoint the Full Release wrapper build was running;
no app verification/staging or
runtime performance success inferred. At that checkpoint source HEAD had to remain frozen until
final app/test/verification/staging receipts are complete; docs can update.

## Implemented behavior and diagnostic contract

Memory: shipped `MaxHeapSize64=0` resolves a finite process admission ceiling
from installed RAM. Reserve max(4GiB,25%), retaining at least half on small
machines:64→48,32→24,16→12,8→4,4→2GiB. Unknown RAM falls back16GiB;
invalid settings select the conservative automatic policy; explicit positive
settings remain authoritative. This is a governor ceiling, not an OS-enforced
allocation limit or a request to consume48GiB. Actual OS availability remains
an independent limiting operand. No owner settings/cache file is edited.

`LLMemory::getAvailableMemKB()` now means rawOS availability. Active pressure
consumers use `getScarcestFreeMemMB()` = min(known OS, process headroom, Windows
commit where applicable). Cached budget snapshots add no per-frame OS query.
The existing LLVOCache policy also uses the process limit for RAM-relative
bounds: raising the automatic ceiling can retain more offscreen objects and
change workload. New residency telemetry and CPU cases expose this coupling;
it is not claimed to be a texture-only intervention.

| Periodic prefix | What the next run can distinguish |
| --- | --- |
| `Native opaque indirect census:` | Requested/support/gates, selected/attempted/prepared/executed, typed fallback reasons and last view, separately main/probe. |
| `Native opaque run census:` | Static/rigged run histogram, query/state/gap splits, exclusions, small/oversize/16k-budget rejection. |
| `Native shadow source census:` | Key constructions, retained/transient hits, rebuilt records, cache admission, actual synchronous detached-job activation and wall duration. |
| `Native texture pressure:` | RawOS availability versus process allocation/limit/headroom, actual low flags and native texture/resource capacity. Legacy aggregate `system_low` is retained for old readers. |
| `Native texture publication:` | Redundant skips versus create calls/retries, finer-than-desired paths, accepted downscale requests, raw input traffic and pending queues. |
| `Native generation publication census:` | Source-cache estimates, full/partial vertex traffic, retained orphan generations, texture COW logical encoded mips/bytes, producer upload commits/failures. |
| `Native object residency:` | Effective RAM-relative retention bounds, factors and actual draw/near/rear/eviction radii and invisible-frame retention. |

Opaque fallback reasons are fixed enums: data publication, unavailable,
completion tracking, validation, retention, native allocation, host allocation.
No arbitrary native error strings, asset IDs or user settings are emitted.
Counters are cumulative per renderer/process. Capture wall time includes
synchronous job waiting, not isolated CPU/GPU time or framep95. `completed_views`
means renderSpotShadow returned true on CPU, not GPU completion. Pin deltas count
new unique refs added to the aggregate lease, not every source consumed by an
outer borrowed command. Native texture atomic fields are independently sampled
and can straddle worker progress.

Create success and accepted scaleDown can include skipped/queued/coalesced work;
they are not proof of distinct uploads/copies. COW byte counts are format/block
rounded one-slice logical bytes successfully encoded, including encoded work
before a later failure; no completed-GPU/bandwidth/allocation claim. Worker and
immediate-upload commits identify those producers, not every queue producer.

PPD-113 inner Batch::Impl now owns CPU lease release even if final public-wrapper
allocation throws. Stored completion tracking still blocks premature reuse.
Prepared success counts advance only after wrapper construction. PPD-114 makes
optional rigged publication failure preserve direct casters/query offsets,
matching static fallback. Required full-indirect mode remains a hard failure.
Failure tests exercise both families, exact depth/query parity and one-shot
recovery; wrapper failure exercises actual unwind and completion-gated reuse.
Typed stage injection does not recreate real system OOM/native-event failure.

No threshold or quality change: the2048draw crossover and16384packet policy
remain. PPD-112's earlier-direct-budget edge is reproduced/diagnosed with18,432
records, not silently retuned; no occurrence in the previous club run is proved.
GPU visibility/Metal4 pilot defaults are unchanged. Full architecture still
needs producer dirty-publication, broader dependency jobs and whole-frame
visibility/admission/streaming-residency coordination. Another large change
before this owner run would blur the observed regression's causes.

## Delivered candidate and final verification

Local compiled source `4918fd7ce3976576eababff430910d73017c9bdb`, branch
`codex/apple-first-architecture-g2-g5`, Release `26.4.0.68999` (`26.4.0d68999`).
Staged app:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-retest-20261114/staged/Alchemy Metal Alpha G2G5.app`

Main executable SHA256:
`13e44a15bfbe746072b5c5642f123310a8365d01947e621a25775637e19d351f`.
Bundle ID `org.alchemyviewer.viewer.metal-alpha-g2g5`, separate development URL
schemes retained. Old 68995 stage remains intact, executable SHA256 still
`96dcc743c9bfdd471984dd7843639137eeb1c4c3eb77dfd722e232ce6124e2ae`.

All evidence below is under the canonical ignored/private
`artifacts/metal/apple-first-retest-20261114/`; all commands run from the isolated
worktree above. No viewer launch/login, audio session, new profile, upload or push.

| Check | Exact command or executable receipt | Observed result |
| --- | --- | --- |
| Final configure | `final-configure-1.json` records complete cmake preset/flag argv | Exit 0,4.119s; source 4918fd7, count 68999; 30 retained feature flags and accepted OpenHT archive pinned. |
| Host | `tools/metal/doctor.sh`; `full-release-build-1-doctor.log` | Exit 0,1.041s; native M1Max64GiB host. |
| Full build | `tools/metal/build.sh build`; `full-release-build-1.json` records environment/build path | Exit 0,655.673s,1,143 actions; source SHA/cache unchanged, -O3/NDEBUG, LTO off, 8 jobs. |
| Final contained regression | `final-contained-regressions-1.json` records exact CTest/native/archive/corpus argv and the source/binary manifest | All 15 process stages exit0:61 CTest groups/312 case executions,10 exact native cases,3 archive policies/8 cases, private corpus 1 case. Combined 568,951 assertions/331 case executions, not unique scenarios. |
| Real selected codec inputs | Private corpus log and final-contained-regressions-1.json |240/240 callbacks successful,0 ordinary failures/missing/duplicate callbacks/escaped exceptions/timeouts.80 selected inputs; not proof of prior crash causality. |
| Built and staged static | `verify-g2g5-bundle.py` command recorded in built-app-static-1.json and stage-candidate-1-static.json | Both exit0:346 ARM64 Mach-O, deep strict signatures, exact source/version/features/native owned imports, media/voice/music/codecs/OpenHT license/provenance,0 private standard exception RTTI. Bundled MaxHeapSize64=0 and all 7 new diagnostic markers verified. |
| Production source | `strict-production-audit-command-1.json` records exact `tools/metal/audit_gl.py --strict` argv | Exit 0,3,549 files,0 blocking dependencies/0 scan errors. Informational compatibility names remain; this is not whole-bundle runtime proof. |
| Separate staging | `stage-candidate-1.json` records exact argv and complete inventories | Exit 0;7,839 files/432 directories/2 links with equal path/mode/link/content hashes before/copy/after; original build bundle unchanged. |
| Preservation | `delivery-integrity-1.json` | Old staged binary digest unchanged; compiled indra source unchanged since4918fd7. |

CTest 119.855 s, explicit native/archive/corpus processes 47.838 s combined. API and
shader validation enabled for native correctness except deliberate PPD-108
archive persistence policy with shader instrumentation disabled. No new timing
benchmark or frame-cadence measurement was made. Existing opt-in fixture timings
are not used as a speedup claim. Actual voice/codec exception ABI passes without
opening audio devices/sessions. Original failed focused receipts stay retained.

The final source is committed locally; subsequent documentation-only metadata
commit does not change this compiled SHA or app version. Required documentation
harness/diff checks and final worktree/source consistency are recorded separately
in `final-documentation-checks-3.json` after this note is updated.

## Owner run and continuation

This checkpoint advances correctness/diagnosability and removes the confirmed
legacy default-cap conflict. It does not establish improved FPS, unchanged live
visuals or complete Apple-first architecture. PPD-111/113/114 and new fixture
PPD-115/116 are ready_for_owner, not closed. PPD-110 adverse live comparison
remains investigating. PPD-112 is covered and visible but policy remains intact.

Next run the exact 68999 staged app. Inspect startup process budget and periodic
OS/process/native pressure, desired-LOD/create/downscale/COW/decode activity,
object residency and admitted main/probe sources. Classify zero opaque execution
from gates, run sizes/budget, attempts and typed fallback counts. Compare whole
frame CPU/GPU/command cadence and texture response with workload quantities and
quality in view; do not require an identical live world or infer causality from
one subsystem's milliseconds. Default process cap is 48 GiB on this 64 GiB host;
an explicit positive saved limit intentionally remains authoritative.

After that evidence, the remaining path is producer-amortized dirty generation
publication (remove expensive repeated source-key/capture work), dependency-safe
jobs over immutable scene inputs, then coordinated whole-frame
main/probe/streaming/resource admission and earlier GPU visibility where measured
beneficial. Preserve original caster/query/lighting/projector/probe/UI/media
behavior and evaluate complete-frame latency, responsiveness, residency and
visual quality at each integrated step. GPU/Metal4 utilization alone is not an
acceptance criterion. Prior detailed architecture/audit and adverse measurements
remain authoritative linked evidence; no whole-architecture completion claim.

Documentation check1: harness/source consistency passed, diff check caught an
extra blank line at the defect register EOF. Removed it; failed receipt retained.

Final read-only integration review matched source/version/hash, counts, policy
and open architecture limits to actual receipts. Historical pending-build text
above is explicitly marked as historical; delivery state is the final section.
