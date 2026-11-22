# PPD-556 — bounded request and service ownership

Status: owner accepted; commit/push approved2026-11-22. Full application
qualification, staging and requested launch complete. Base 629984ee37; accepted app69445/source a2aaaf2942 is preserved.
Owner requests implementation plus renewed dead graphics-preference audit.
The initial local-only boundary is superseded by explicit owner acceptance and
normal branch push approval on2026-11-22.
Initially no launch/login, saved-setting mutation or live profiling was authorized.
The owner subsequently requests launch after validation; exact staged PID31214 is
observed with temporary AutoLogin=false. No login or live profiling is performed.

## Ownership

Root owns llqueuedthread/llworkerthread, shared build/llappviewer and canonical docs.
Worker reviewer owns scheduling test file after interface handoff. Asset reviewer
owns caller changes after contract handoff. Preferences agent owns independent
worktrees/ppd556-preferences settings/feature tables/help/fixtures; root adds startup
retirement names. Existing task/native-IM worktrees remain untouched.

## Initial baseline and preservation map (before repair)

Upstream 408fd6c and baseline 629984ee37 queued worker use a bounded schedule for request
admission, retries and wakes. A consumer can return incomplete after another
producer fills the freed slot, then block self-posting. Accepted requests must
retain one reusable schedule node independent of callback capacity. Bound all
registered requests through terminal reaping, not merely the currently ready set.
A separate bounded callback service preserves cache fast reads and generation-
pinned fetch completions. Its wake must not acquire a different owner's data lock.
Keep four-promoted-start fairness, finite fallback deadlines, exact generations,
wake-during-execution and exactly-once cancellation/shutdown.

Baseline caller audit: addWork initializes/sets flags before fatal admission;
cache blindly inserts returned handles, writes conflate null with read-only, LFS
rejects fatally and audio marks a rejected WAV write complete. Implement explicit
retryable busy/closed outcomes and rollback without manufacturing completions for
work never accepted. Audio WAV finalization must occur only once across retries.
Original producer/consumer behavior remains the source reference; no new graphics
backend or generic engine scheduler is introduced.

## Initial preferences follow-through (before repair)

PPD-560: two orphan keys RenderDeferred/RenderTerrainDetail, no current production
key consumers. All 66ordinary UI bindings remain live. Retire those exact keys,
preserve RLVa atmospheric/media/mesh/native cache behavior. PPD-561: correct two
native pacing descriptions while retaining policy/defaults. Baseline fixture320
checks/ASan/UBSan passes in the independent worktree. Final 344-check results are recorded below.

## Initial validation plan (subsequently implemented)

Implement reusable request schedule and independent native owner wake; test tiny
capacities, retry with full callbacks, rejected ownership/lifecycle, wake/promotion
fairness, deadline/handle reuse, paused service and active shutdown. Integrate caller
and preference tests, run normal Release build/doctor/app verification, strict
source gate and appropriate CPU/native checks. Preserve every failed attempt under
artifacts/metal/ppd556-20261121. Do not claim owner acceptance or FPS gain.

## Implementation checkpoint

Local source commits a27fb1a57e (request/service ownership and callers) and
960bd3644a09e6da8da57715947ef479fa16de18 (native preferences/classification/support).
Built source indra tree7d8c095d360bd3f77ac64d1b69e23c21840e0011; base 629984ee37.
Channel Alchemy Metal Scheduling; configure selects26.4.0.69450.

Each registered request owns one map node, extracted while running/terminal and
rekeyed/reinserted allocation-free on retry/wake/promotion. The existing default
1,048,576 registered-request cap includes terminal unreaped requests. A separate
bounded registered WorkQueue services callbacks nonblockingly; no overflow queue,
stale wake tickets or accepted retry lambdas exist. Four urgent starts bound normal
starvation. Native coalesced semaphore notification avoids cross-owner request
locks; the notification bit clears only after a successful semaphore acquisition.
The service owner alternates requests/callbacks/housekeeping, honors explicit pause,
and drains accepted work/aborts queued requests before actual thread termination.
WorkQueueBase::runOneReady reports actual progress without changing public runOne
open/closed semantics. Abort flags are atomic. Existing finish callbacks retain their
preexisting owner-lock boundary; no new broad lifecycle framework is introduced.

Worker admission is bool/nonfatal and rejected initial state rolls back. Never-
accepted cache workers can be disposed immediately instead of accumulating a frame
cleanup backlog. Cache writes and LFS expose accepted/busy/closed (cache also skipped
read-only); no null-handle maps or fabricated successful completion. Audio retries
busy WAV writes after one-time finalization. Fetch priority posts report actual
admission and carry pointer+generation lifecycle pins; deletion waits for pins.

Graphics cleanup retires RenderDeferred, RenderTerrainDetail, GPUBenchmarkResult,
SkipBenchmark, RenderClass1MemoryBandwidth, RenderCPUBasis and
ALMesaRDNA35SettingsRestored, associated masks/comments, old automatic GL bandwidth
classification and the child benchmark command/startup mode. Total exact startup
retired names is31, including prior 24. Native class 3/actual renderer/unknown bandwidth
remain. All 66 live ordinary graphics bindings, quality/preset implementations and
the manual gpu_benchmark()/ALMetalTextureBenchmark/menu/tests remain. Pacing help
matches native behavior without policy/default changes. Human support output uses
RENDERER_BACKEND through AboutRenderer; structured OPENGL_VERSION remains not
present for established API compatibility. Base English fallback avoids inventing
translations; unused AboutOGL entries retire across locales.

## Executed validation and retained failures

Evidence root: artifacts/metal/ppd556-20261121 (private, ignored).

| Check | Actual outcome / evidence |
| --- | --- |
| tools/metal/doctor.sh |0; real M1Max/64GiB/macOS26.5.2/Xcode26.6, doctor.log. |
| Actual Release queue syntax01 |0; actual TU flags, no PCH. |
| configure-tests-01/core-build-01 |0; actual llcommon_tests and llfilesystem_tests. |
| core-tests-01 |5 PASS / 1 FAIL; PPD-564 fixture incorrectly requested zero capacity. |
| PPD-564 correction |Capacity1 plus an accepted retained blocker; production positive-capacity assertion preserved. |
| configure-tests-02/core-build-02 |0; core/LFS plus scheduling/admission/preferences targets. |
| core-tests-02 |9/9 PASS, 0 skipped, 5.35 sec; exact selector in log. |
| Old actual 629984 queue baseline |Expected exit42,2 failed cases / 12 assertions, bounded 150 ms probes; only constructor capacity and manual pump adapter differ. |
| Actual modified core ASan/UBSan |793 assertions / 18 cases PASS; real scheduler/worker TUs and tests, no sanitizer findings. |
| Actual caller methods ASan/UBSan |549 scheduling, 75 admission, 105 auxiliary checks PASS. Aux mock first omitted HAVE_WORK in 3 cases; corrected accepted-state setup, retained failure. |
| New native event stress |Two real producers,256 callbacks each,capacity 2; bounded retry, progress before shutdown, exact-once slots and joined cleanup. Included in actual native/core ASan tests. |
| Final preferences ASan/UBSan |344 checks PASS; packaging0; all 66bindings, unchanged quality/manual benchmark proof. |
| Viewer syntax01 |Argument-filter/PCH setup error exits1 before source qualification; retained under PPD-563. |
| Viewer syntax02 |Four actual Release TUs pass0 after stripping both Clang PCH argument forms. |
| git diff --check |0 before both local source commits. |

Baseline evidence: artifacts/metal/production-convergence-20261121/ppd556-baseline
includes exact old source, constructor-only diff, commands, bounded futures/joins
and failure logs. Sanitizer evidence: same root/ppd556-sanitizers, with actual
source/archive hashes and link recipe. No old test binary substitutes for new source.
Caller source freeze: artifacts/metal/ppd556-20261121/caller-source-freeze.json.
Preferences proof: worktrees/ppd556-preferences/artifacts/metal/ppd556-preferences-20261121/ppd563.

Final configure:
`cmake -S indra -B build-Darwin-ninja-os-arm64-shadow-occlusion
-DAL_CHANNEL:STRING="Alchemy Metal Scheduling" -DAL_BUILD_TESTS:BOOL=ON`
exits0 (configure-viewer-01.log). Existing full build:
`AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64-shadow-occlusion
AL_METAL_CONFIG=Release AL_METAL_JOBS=8 tools/metal/build.sh build`
exits0 (viewer-build-01.log/.exit; all1462actions completed). Do not substitute a
prior candidate for this built source. Final integrated validation is running.

## Remaining qualification and maintenance boundaries

Run final integrated CPU/asset/settings/OpenHT/native GPU selections, pipeline-cache
modes separately, ARM64/signature/strict source checks and stage immutable distinct
Scheduling 69450. Preserve accepted 69445. Retain decoder notices and exact app/source
identity. Update register556/560–565 as ready for owner testing only after executed
checks; builds/tests alone do not close records or authorize pushing.

No owner saturation/FPS improvement is inferred. Existing asset frame-triggered HTTP
progress, permit wait cancellation, General job separation, immutable scene prep,
safe reply service around early GPU waits, upload preparation/policies and pipeline
archive ownership remain explicit in the canonical active ledger. Default-off live
features are not abandoned experiments. Product parity, prolonged stability,
independent reproducibility and runtime foreign-process/no-GL gates remain open.

## Final independent source review

All three assigned reviewers examined exact 960bd3644a read-only and found no
concrete blocker. Core reviewer confirms eight modified production/test files
match the retained sanitizer manifest byte-for-byte; callback progress, wake bit,
lifetime/node reuse, aborts, capacity and actual derived shutdown ordering checked.
Caller reviewer checks rollback/disposal, explicit cache/LFS outcomes, once-only WAV
ownership and priority pin/removal synchronization. Preferences reviewer confirms
exact31 unique retirement names registered before profile loads, seven new names,
all 66 live ordinary bindings and byte-identical quality/preset/manual benchmark
bodies, native About field and all locale cleanup. No reviewer launched, built,
mutated source or wrote Git during this review.

Harness02 exits0 (33 required files). Harness01 ran before a zsh receipt wrapper
used its readonly variable name status; wrapper exits1 without creating the exit
receipt. Preserve the first log and use task_harness_exit in the corrected command;
no source or harness suppression was introduced. Final CTest inventory contains
797 registered scopes. Absolute-path manifests cpu-tests01(58) and gpu-tests01(56)
have no missing names; final execution remains after the complete build.

Owner asks whether the earlier LFS backlog sleep finding is fixed. Current source
llappviewer1803–1839 confirms the queue-count delay is absent; PPD-538 was fixed and
owner-accepted in69445. New556 strengthens bounded/nonblocking LFS admission and
audio busy retry. The initial source assessment now marks this specific disposition
explicitly while retaining baseline evidence and unknown historical activations.

Owner also asks about the initial asset-handoff finding. Accepted69445 includes
PPD-532/533 bounded64-slot async fast-cache reads, direct generation/pinned FETCH
completion admission and4096decoded-ready notifications. These do not eliminate
required owner publication. Main-list fallback when admission fails, frame-triggered
HTTP service and completion-to-consumption measurements remain explicit follow-through.
The initial assessment row now distinguishes implementation from those residuals.

Owner asks whether geometry budget enforcement is fixed. Answer: only PPD-530's
misleading interface is repaired/accepted; required traversal is explicitly named
updatePriorityGeometry(), object creation retains its own allowance. Required vs
deferrable decomposition and combined scheduling remain unimplemented. Registry530
and initial assessment row4 now state this boundary expressly; the active geometry
priority retains the implementation work. No deadline/performance fix is inferred.

## Full application integration checkpoint

Full Release build01 exits0; actual Scheduling26.4.0.69450 bundle is signed. Source
deep/strict codesign01 exits0, strict GL audit01 exits0 with0blocking/0scanerrors
(scanned3639files), and all15modified settings/UI/locale XML files parse. Native
ARM64 app inspection and final58CPU/56native selection are running. Build log
retains duplicate-static-library linker warnings; they are not compile failures.
No owner acceptance, runtime or performance evidence is inferred.

Owner asks about main replies/earlier GPU waits: broader accounting remains open.
Accepted537 fixes only the latched regular MainWorkTime setting. Queue-age/wait-site
correlation and safe slack-policy changes remain unimplemented/unmeasured. Registry537
and assessment row8 now state this expressly; no active-render callback service or
measured lost-slack cost is asserted.

Binary inspection01 used an overly broad _opj_decode substring assertion, which
matched the required metadata helper _opj_decoder_set_strict_mode. Retain the
failed check in binary-inspection-01-failure.json. Corrected inspection02 compares
complete nm symbols: no _opj_decode, header/strict-mode APIs retained and real
OpenHT decoder symbols present. Direct Metal dependency and bundle/system Swift
rpaths pass, with no direct GL/Vulkan imports. This is static evidence, not a
viewer-owned runtime-context or live decoder recording.

## Final qualification, staging and launch

- viewer-build-01.exit=0; full1462-action normal Release build at960bd3644a.
- final_validation.py exits0:58/58CPU scopes,0skip,19.92sec;56/56native scopes
  with MTL_DEBUG_LAYER=1/MTL_SHADER_VALIDATION=1,0skip,270.34sec. Absolute manifests,
  complete JUnit output and exact command/env are in final-validation-01-receipt.json.
- Pipeline ordinary/instrumented/selective scopes each exit0 separately; no global
  instrumentation was disabled to satisfy archive-specific ordinary expectations.
- Actual alopenhtj2k test passes56assertions/3cases (engine identity and primary/auxiliary
  pixels); pinned source/archive, licence and attribution stay required.
- Source/staged deep-strict signatures0; verify_app01 counts346ARM64-only Mach-O
  files; strict source01=0blocking/0errors;15XML parses0; harness04=33requiredfiles0.
- stage_candidate.py exits0; immutable separate Scheduling69450 inventory matches
  source exactly:7839regular files,2internal symlinks, no external links,797452771bytes.
  Accepted69445 executable unchanged. Exact hashes are in candidate-69450-receipt.json
  and the candidate report; test and app metallibs are identical.
- Owner requests launch when finished. Exact source/stage/hash verified before
  open -n exact.app --args --set AutoLogin false. libproc observes exact staged
  executable PID31214 and it remains running after the bounded launch check.
  owner-launch-69450.json records command/path/SHA/PID, with no login or scene claim.
- Final read-only reviews clean; all new556/560–565 records ready_for_owner,
  owner verification pending and Push approval not_granted. No push/tag/release.

No builds/tests/captures remain active. Final documentation-only commit preserves
the built indra tree; no rebuild is needed for documentation. Next action is owner
testing of the running candidate; preserve explicit geometry/slack/asset residuals
and broader qualification in the canonical active ledger.

## Owner acceptance and approved publication —2026-11-22

Owner reports "looks good..commit and push" after the exact69450 launch/test.
PPD-556/560–565 move to verified/Owner verification passed/Push approval granted;
all original failure histories and broader unfinished mechanisms remain. This is
scoped candidate acceptance, not measured FPS, saturation or product completion.

Preflight: canonical branch codex/apple-silicon-metal at9f4d1b75bb, no tracked
changes, unrelated untracked worktrees/ retained. origin reports629984ee37; three
local approved commits contain only bounded scheduler/callers/tests, native settings
cleanup and corresponding docs. Read-only independent scope/privacy review is clean.
Exact staged executable SHA256 and built indra tree match the accepted receipt.

Update acceptance records, run ordinary harness/diff checks, commit documentation
and normally push HEAD to origin/codex/apple-silicon-metal. Verify the published ref
against local HEAD; save exact command/exit/local+remote identity privately in
owner-approval-push-20261122.json. No rebuild is needed for documentation; no source,
signed app, settings, unrelated worktree, tag, release or default branch changes.

Owner also asks that future launches preselect the appropriate Home/club destination
on the login screen. Resolve recent private launch/location evidence into ignored
artifacts/metal/private-testing-destinations.json, then select by test purpose and
pass --url last. Keep exact private destinations out of publication. This does not
authorize restarting the current viewer, auto-login, saved graphics changes or a
recording; it preserves the launch preference for subsequent test candidates.
