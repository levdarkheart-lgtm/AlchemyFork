# Active Metal work — optional native Aurora review prototype

Accepted feature `codex/aurora-native-temporal-20261126`, parent318f0ac8b6. Owner explicitly
authorizes implementation, offline tests, packaging and feature-branch push. Owner now authorizes merging
this retained chain into codex/apple-silicon-metal and triggering Alpha9. Evaluation,
Temporal and Spatial branches/packages remain preserved. Native default, existingmodes,
output-basedshadows, nativeUI, combinedlightingOFF and PPD610qualityfailures remain.

## Immediate executable action

Owner accepts69531reported water blinking in the tested scene: PPD624/630verified.
Preserve exact source/executable/shader identities and accepted tile/glass/foliage.
No further patchAinvestigation or visual changes solely for that metric. Mixed
comparison and receiver-band limitations remain; reopen only on visual regression.
GPU cost stays unmeasured because no existing readback-free/timed arm is available;
do not interrupt the owner's running69531or create another profiling campaign.
PPD631metadata-only repair and source gates pass. Fast-forward the usual Metal
branch, dispatch one immutable-source Alpha9build and verify run identity.
No new local viewer build needed; root owns integration, docs and release dispatch.
Commit `f971d11de29daf60233f9f5875cf957ca069e95c` is69531appsource.
Preserved69527source is5d44ab6411a47bad3e437ed2da78179b7121e978.
See [water report](../../research/temporal-water-history-20261126.md).

| Item | State | Acceptance / next boundary |
| --- | --- | --- |
| PPD629 runtime MSL dialect | open | Complete builtin rejects `coherent` under SDK-default compileSource; pin/verify runtime dialect separately. Water test uses offline Metal3.1; packaged metallib remains required. |
| Native pinnedAuroraOFF graph + mainID8 | complete | Realopaque/final/depth/motion/glow, existingworldownership, no wgpu runtime. |
| PPD614 missingtranslatedpyramidfunctions | ready_for_owner | Reflectednames+functionvalidation;882worlds/195MRGBexact. |
| Submission/resource safety | complete |12componentcases +2actualrenderer cases/204assertions API/GPU pass. |
| PPD610 quality | open | Area.821401<.85;stop18/36.015886/.012319>1/255; ownerliveacceptancepending. |
| Actualtarget nativecost | complete |10.926median/12.392p95ms, allintegrationpasses included; noFPSclaim. |
| Preserved69516 delivery | complete | 69516 full build, regressions, login smoke, mounted package checks and all three iCloud uploads verified; owner live review next. |

Release build69516, deep/strict ad-hoc signing and 346 ARM64 checks pass. All three
CPU and fifteen affected Metal suites pass after the inherited PPD616 test-only
oracle correction; the application is unchanged. 198 tools (one existing skip)
and seven translation/analysis checks pass. Full-screen login-only smoke reaches
STATE_LOGIN_WAIT and closes normally; PPD617 records the failed cloned-settings
override and verified actual preference bytes. No login or live rendering test.
Initial wrong entry names, dependency-log recovery and fixture oracle failures
are retained; no quality threshold relaxation. Component memory plateaus.
Mounted DMG matches all 8293 app entries; signatures/346 ARM64 checks and ZIP CRC/
hashes pass. All three iCloud uploads are confirmed at18:16:49UTC with matching
destination bytes. PPD618 retains the initial transient error and recovery.
The authorized new branch is pushed; application source stays b6f, with only
test/documentation follow-up. Owner live review is next. Evaluation/Temporal/
Spatial/usual branches and prior packages remain unchanged.

## Preserved Spatial parent checkpoint

The following Spatial sections describe prior69464 evidence. Their review-only override and “no Temporal”
statements are historical, superseded by the active Temporal state and PPD600 retirement above.

## Active quality work — PPD596

Implemented locally: exact 2/3 and 3/4 scales plus scoped per-view Spatial material sampling. Native default
and IDs 0/1/2 remain. Output-based shadows, native UI and independent captures are preserved. A dedicated
fragment uniform keeps cached material packets immutable. Main alpha depth/color draws share the policy;
explicit environment LODs, shadows and UI are unchanged. Linear fallback retains the policy selected before
drawing and reports its effective method, dimensions and bias. A nonpersistent quiet-session override permits
the requested fixed-resolution mip comparison without adding a graphics preference.

Verified: three CPU suites and fourteen relevant Metal suites, including material/alpha/terrain sampling,
scaler reuse, presentation failure, cutout, AA and owned-input paths. Release build 69464/source a5142ae5c2,
deep/strict signing and 346 ARM64 checks pass. The new alpha fixture initially omitted its required environment;
the fixture was corrected without weakening production validation or assertions, and the passing retest is
retained alongside that failure. The older hidden probe suite requires external source inputs and is excluded.

Completed runtime disposition: see the [quality review](../../research/metalfx-quality-20261124.md). Bounded
observed53-probe rates are Native15.14/unbiased50 35.68/biased50 35.58/67%25.41/75%21.07/restored50 35.90 worlds/s.
The restored row is a labelled20s prefix; warming controls and full invalid A12 remain excluded. Observer holes
mean only B2 passes every local population gate; endpoint policy proof is positive for retained observations.
Five lossless PNGs/1:1 crops and qualitative motion clips are retained. UI crops are byte-identical; mip bias
modestly helps textures while missing raster detail persists.67% is the balanced recommendation,75% best scaled
still. Continuous motion/scene acceptance remains limited, not silently closed.

Final Native/original controls/CAS restored, review override false, screenshot mode restored and PID22729 closed.
Review DMG/source/evidence bundle on iCloud Desktop; review-branch publication now authorized. PPD596 is ready for owner review. Separate PPD597
records the existing hidden-only probe fixture's default CTest registration failure; do not count it as passing
or restart probe work. Temporal is a subsequent owner decision; no implementation here.

## Previous integration checkpoint

The focused integration follow-up (PPD593–595) is implemented: real graphics-preset
round trip/older-preset Standard compatibility, visible effective fallback reason,
independent native same-size ordinary snapshots, and a borrowed-main late-failure
test after real MetalFX encoding. Release build,14 selected CTest suites (3CPU+
11Metal with API/GPU Validation),346 ARM64 checks and signing pass. That checkpoint had no new live
login/UI/snapshot smoke test or timing run; the later69464 review above supersedes that runtime boundary.
Review this focused diff and current candidate next; do not restart broad FPS work.
The separate69462 review DMG and focused diff/verification bundle are on iCloud
Desktop for review;69457 remains unchanged. The owner now authorizes review-branch publication only.

Initial69457 evidence follows; it does not measure the69462 changes.
Implementation, source-review corrections, clean control/candidate builds and
focused CPU/GPU correctness checks are complete. One quiet Home sequence observes
Native15.135, Half-linear35.570 and MetalFX35.026 successful joined worlds/s.
One FX lifecycle row is lost; no timed restoration or repeated-pair claim is made.
Original shadows/native UI are preserved; actual scaler encoding is observed.
Verified stills and camera-sweep review videos are retained privately. Resize/cache
reuse and same-size Snapshot fallback were exercised. Original controls/CAS/camera
and Native mode were restored, then the viewer shut down normally.

The [implementation record](../../research/metalfx-spatial-20261124.md) owns concise
results; [CURRENT](../../state/CURRENT.md) records observations. PPD591 is ready for
owner quality/selector review; broader moving-quality and stability acceptance
remain open. Previous maintenance below is history, not this feature's proof.

The exact69457 app is privately packaged on iCloud Desktop for the owner's other
Apple Silicon/macOS26+ Mac; image/signature/full-content checks pass. External
execution remains unverified. PPD592 retains inherited optional VLC dependency
references for a separate media-packaging follow-up; no app bytes were changed.

## Remaining engineering

| Priority | Requirement | Concrete next boundary |
| --- | --- | --- |
|1 | PPD591 optional scaling / PPD319 Home | Review the implemented quality/performance trade-off. Native exact-quality Home improvement remains unresolved; prior probe, BASE-SSR, combined-pass, queue-depth and tiny-adoption experiments stay closed. |
|2 | General service | Measure category queue age and establish ownership before separating file/audio/CPU work. Invalid widths are already repaired. |
|3 | Geometry/scene preparation | Separate immutable preparation from required lifecycle work before imposing a combined deadline. API renaming did not enforce one. |
|4 | Safe main replies | Correlate queue age with required-main and end-pacing waits; consume at a safe owner boundary. A live setting did not unify wait accounting. |
|5 | Upload preparation | Identify conversion/alpha/mask cost, denied credits and backlog age while preserving the single upload owner and memory/progress bounds. |
|6 | Pipeline/archive | Remaining serialized compile/archive ownership; required point pipelines are already eager. |
|7 | Retained incidents | Current recurrence and cause for002/015/311/319 and loading/appearance reports. Accepted maintenance does not establish every incident resolved. |
|8 | Product qualification | Reference-backed feature/visual parity, prolonged stability, representative performance, runtime/foreign no-GL and reproducible release. |

## Completed boundaries and retained history

PPD582/585/586/587/588/589 production repairs and PPD590 existing-harness repair
are implemented and qualified, with owner acceptance
separate from code/test completion. PPD584 reconciles current fields against source
and accepted history. Accepted Maintenance69445 and Scheduling69450 remain preserved;
completed preference, asset and abandoned-path repairs are not active requests.

The [truth audit](../../research/register-truth-audit-20261122.md) retains the original
211field corrections/44scoped accepted-removal closures and74specific missing-proof
actions. Those74 are evidence/acceptance gaps, not74 newly confirmed source bugs.
The [register](../../POST_PORT_DEFECTS.md) retains all IDs, failures and owner decisions;
resolve each remaining gap only from source and actual evidence.

[Review findings and measurements](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/a6089d691c113b9e6bc66a09f6b65b020e70960f/docs/metal/research/review-home-admission-20261122.md)
remain historical evidence. New native telemetry, input/memory/admission helpers,
metric manifests and study-specific tools/tests remain review-only. PPD583 is
excluded by owner direction, not an active usual-branch implementation task. Failed
experiments are not production capabilities or future tasks to reactivate.

Home FPS is not fixed. The [published PPD567 record](../../research/ppd567-performance-20261122.md)
retains mixed cadence and adverse Club p95; lower redundant CPU work alone does not
establish a viewer FPS benefit.

## Complete-product checkpoint gate

| Product ID | State | Remaining boundary |
| --- | --- | --- |
| M-008 | doing | Broad feature/visual acceptance and representative performance. |
| M-011 | doing | Runtime/foreign-process and reproducible release qualification. |
| M-012 | doing | Maintained tools verified; PPD597 manual-suite registration open. |
| M-013 | doing | Retained features and prolonged stability. |

## Non-negotiable open gates

Cancellation/request identity, generations, native completion retirement, memory
bounds and retained features/quality remain invariants. Owner publication approval
is scoped to this maintenance; it does not waive product or release gates. Required
independent build/review evidence remains. A bounded candidate never implies product completion.
Do not mark feature parity from build/test success alone.
