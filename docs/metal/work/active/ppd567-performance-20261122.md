# PPD-567 implementation and measured follow-through

Status: complete for the five-part engineering goal; owner visual/performance
verification remains pending. The owner goal authorizes local implementation, ownership/output
checks, repeated controlled Club comparisons with Home regression checks, one
bounded Home probe census, a non-overlapping Club CPU budget and worker/queue
progress table. Autonomous saved-account login and native full-screen operation
are authorized. The owner explicitly approved committing and pushing this scope
on 2026-11-22 after receiving the report; preserve the accepted app.

Start: branch `codex/apple-silicon-metal`, HEAD `d219d538ab`, renderer code
`960bd3644a`; existing documentation changes are our preceding completed
investigation, retained. Unrelated worktrees remain untouched. Host doctor passes
on M1 Max64GiB/macOS26.5.2/Xcode26.6. Private evidence is under
`artifacts/metal/ppd567-20261122`.

## Contract before the fix

Original Alchemy `1eb44bbd1b`, `llreflectionmapmanager.cpp:1584` updates one
reflection-probe UBO snapshot; `setUniforms:1603` binds that current block rather
than recreating its content identity per draw. Alpha rendering changes shaders
and binds current lighting while retaining the same view's probe values. Metal
uses immutable command-completion-owned pages instead of overwriting a GL UBO.
Its new native publication mechanism must preserve the current pack, draw order,
shader ABI/math, per-view isolation and completion lifetime.

Current Metal freezes a 49,216-byte forward pack once in `renderOpaquePostDeferred`,
but slot1 and slot3 calls repeatedly hash it before the existing buffer-binding
cache helps. Publish lazily at the first actual old consumer, once per invocation,
with deduplication enabled. Rebind the same slice at every existing consumer and
encoder restart. Keep standalone command-token epochs and external FrameData;
no raw-pointer/global cache, shader change, new renderer framework or quality knob.

## Work and ownership

| Requirement | Owner / paths | Evidence required |
| --- | --- | --- |
| Local PPD567 patch | Root: `almetalworld.cpp`, headers only if needed | Once-publication source/operation proof, unchanged shader pack and bindings |
| Ownership/output checks | Peer isolated `worktrees/ppd567-tests`, existing candidate-transparency test file | Different forward packs in pending views/commands, external/standalone pages, caller reuse, output controls, abandonment/reload |
| Controlled runtime comparisons | Root exclusively: exact staged apps, CUA, recordings | Repeated warm Club A/B, at least300frames/arm/repetition, same settings/resolution/view and age; Home regression; total frame/main CPU plus targeted hashing |
| Bounded Home census | Peer private census helpers; root UI exports | Explicit<=1024receiver sampling, actual55-pack resources, ordered selection/default rules, noatomics; one-experiment-or-stop decision |
| CPU budget and queue table | Peer private source/trace reductions | Mutually exclusive Club sample categories, exact reconciliation; measured execution/overlap/queue ages versus unknowns |
| Integration/docs | Root only | Actual build/app/test receipts, retained failures, updated register/state/ledger; no unsupported architecture changes |

No unrelated build/test/analysis runs during measured live intervals. Preserve
prior raw captures and source-metadata variant; do not confuse its55 probes with
normal late capture39 or reuse replay sums as live FPS. Product release gates
remain separate from this scoped task.

## Implementation and component checkpoint

Root's invocation-local slice patch changes only the two existing forward-pack
binding sites. The original integrated fixture covers two slot1 consumers with
separate visible responses, reused caller storage, two differing packs in one or
two pending commands, reverse submission order, external/standalone pages and
candidate/fallback paths. Empty-view checks bound publication bytes below a full
pack. Correction2026-11-22/PPD582: the original “both slots” claim was false;
simple alpha uses slot1. The new review-follow-through case uses actual above-water
slot3 and restores alpha slot1. Its separate API/Shader Validation run passes
3,805assertions/1case; this is additional evidence, not part of the old12-case run.

Actual old renderer plus the new test fails exactly eight repeated 49,216-byte
dedup checks; the fixed renderer passes 51,279 assertions in that case. Full
candidate-transparency API/Shader Validation run passes 1,825,343 assertions in
12 cases. Related publication, lighting, frame data, completion, reload, encoder
cache and water checks pass 581,147 assertions in40 cases. Failed baseline proof
is expected corrective evidence, not an unexplained failure. Exact fixed source
SHA256 `958f17d0792f47d900a83ba7b30e78cd23f7d7a12fe85726690e6277b59858bc`.

The controlled A/B needs the actual camera, not avatar position. Existing LEAP
exposes camera setters but lacks a camera reader. A small read-only
`LLAgent.getCamera` reply is therefore common to BOTH new comparison builds:
position/focus, axes, matrices, FOV/aspect/clips and mode. It runs only on request,
adds no frame hook, preference or shader change, and is independently reviewed.
Its source and binary hashes are separate from the PPD567 patch. A is the unchanged
renderer plus this observer; B adds PPD567. The accepted69450 app stays untouched.

Original camera producer/getter semantics are retained; the native API extension
reports current request-time LLViewerCamera state, not an atomic captured-FRAME
record. Matched phase queries will verify actual fixed viewpoints; setters alone
are not observation. Source receipts explicitly retain local patches rather than
pretending the configured ancestor commit fully describes uncommitted code.

## Bounded Home census: completed stop decision

Root exported actual55-pack main BASE draw28470, raw buffer ranges/masks/grid and
lossless specular, normal, depth-aspect and packed integer SH KTX. The private
census normalizes only verified raw row order. Current helper bodies are verbatim;
removing diagnostic lines restores the production walker. Fixed indexed records
and local counters use no atomics. API/Shader Validation actual M1 Max run02 exits0
in0.627s, with original inputs unchanged.

Deterministic32x32 visible-pixel strata yield1024 records,966 covered receivers,
zero invalid/nonfinite ambient results and zero bit mismatches against unchanged
ambient traversal. Accepted nondefault taps average6.740; positive SH including
default average7.740/median8/max13. Default contributes to all966, including961
after nonzero accepts. Of25049 containment calls,18538 reject and6511 accept;
3074 other visits are masked out. There is no sampled manual contribution,
accepted32 saturation or zero-weight/scale opportunity. Radiance uses a labelled
dummy; no full-lighting parity or timing claim.

Decision: stop GPU kernel variants after this census. It identifies actual demand
but no newly redundant operation with demonstrated benefit; old PPD353 regressions
remain contrary evidence. No production shader/quality change is selected.
Private `census/REPORT.md` and run02 records retain exact inputs, method and limits.
PPD573/574 preserve source-extractor/format-validator failures before correction;
child run01 has no Metal device and is excluded, root run02 is actual GPU evidence.

## Exclusive CPU and worker progress additions: completed

Private `analysis-cpu-queues/REPORT.md` partitions every one of43429 Club late main
samples once:43.429 sampled CPU seconds /848 associated intervals,51.213443ms per
association.21 exclusive owner categories reconcile exactly;0.992s remains explicitly
unclassified. These are stack-ownership categories, not additional inclusive
function costs or automatically removable wall time. PPD572 missing-stack sentinel
failure is preserved; its corrected weights are retained even on background threads.

The queue table separates loading from late progress. Club decode loading tickets
wait up to~32s, then clear. Eight late point snapshots show waiting/active0 while63
new decodes complete between first/last observations. Native worker uploads/commits
advance, but other handoff and General category queue ages remain unknown. General's
5ms sampled late CPU does not identify it as the settled critical path. HTTP callback
servicing remains frame-triggered despite independently progressing request retries;
its practical latency is unmeasured and remains in the engineering backlog.

Historical precollection work: build/sign/verify two distinct common-observer A/B apps with matching
CPU symbols, then collect repeated full-screen stationary Club/Home comparisons.
Root alone controls source temporarily restored for each arm, staging and runtime.

## Live collection checkpoint —2026-11-22 09:11UTC

Both Release69452 comparison apps, strict signatures,346ARM64 app verification
and matching dSYMs finish successfully. Exact source/binary/patch manifests live
in private `identities`. Actual decoder logs identify OpenHTJ2K0.27.0. Preserved
accepted69450 is not either new comparison arm.

The first Club run is retained as an excluded camera/collector pilot: its requested
pose matched, but visual qualification faced a storefront. Revised fixed entrance
view is visually qualified before comparison; coordinates remain private.
All runs use native full screen, the same66 effective controls and fixed camera.
Protocol fixes the comparison at world-age150–200s inside90s Overview recordings
started after120s settling. This inward timing change was declared from recorder
startup latency before examining comparative costs. Initial declaration is retained.
Three Club pairs use AB,BA,AB; two Home pairs use AB,BA.

First control01b and candidate01 raw recordings are collected; candidate02 is
collected and control02 is preparing. No comparative speedup is claimed.
Strict native collector rejection for final command-event contention loss is
retained. Explicit consecutive FRAME iteration records can independently qualify
frame cadence; CPU/GPU Overview retention is qualified separately. Do not call
lossy command telemetry complete or derive exact command-stage sums from it.
The control01b149–201s bracket has788 consecutive FRAME records without holes.

PPD575 retains a draft offline GPU-column mismatch; PPD576 retains the fake camera
fixture's outdated basis after the scene aim changed. Repairs preserve strict
runtime checks. Live prepare helper verifies actual pose/axes/projection and66
controls, with a narrow expected infinite-projection-far exception: raw private
snapshot remains partial because that nonfinite scalar was filtered. Finite
reverse-infinite projection and every other field must match.

PPD577 retains root's premature-quit collection error. Candidate01 recorder
finalized0 but its collector failed the live-PID check before exports. Raw trace
is preserved, not yet qualified; separate offline export attempt01 failed its18s
export deadline and preserved original session unchanged. Retry with normal90s
export budget after collection. Post-record helper now waits for recorder-exit
qualification barrier before root quits; candidate02 passes that sequencing
barrier and retains strict footer-loss rejection separately. No viewer or
quiet_capture source fix is implied by these private collection errors.

## Final local handoff — 2026-11-22

All five authorized local requirements are complete. The final report is
[PPD-567 measured publication](../../research/ppd567-performance-20261122.md).
Owner verification remains pending; the later explicit commit/push request below
supersedes the earlier publication restriction. No product completion claim.

Final Club order is A01b, B01, B02, A02, A04, B04; Home is A01, B01, B02, A02.
The setup pilot and whole original third Club pair remain excluded and retained.
B03 has one missing FRAME iteration6258; PPD581 preserves the premature all-ten
claim and review mistake. Explicit qualification, not reducer exit0, now gates
the group comparison. All final ten records have true qualification/empty reasons,
zero fixed-window FRAME gaps and more than300 native periods and GPU associations.
Replacement A04/B04 have815/803 periods. The actual failed B03 negative-gate check
passes; final_compare.py completes0 with six Club/four Home and no missing arms.

Broad opaque-post hashing is2.193–2.940ms/period in Club controls versus
0.0186–0.0237ms in candidates; residual palette/constants remain in that category.
This is not exact49,216-byte self time. Component byte/output/lifetime checks
independently prove the missing per-view reuse contract. Whole-frame Club mean
changes are−6.35%,+1.30%,+1.62%; replacement p95 is11.76% slower. The adverse
observation is retained. Vertex-counter spans show about30% more logical bytes
in replacement B, with changed point-light/texture activity; this proves unequal
activity, not its cause or absence of regression. No isolated sustained FPS gain
is established. Home mean cadence is within0.5% across both pairs.

The bounded Home census and exclusive Club CPU/queue additions remain complete;
STOP GPU variants after census, retain PPD353 contrary evidence and PPD319. Pool
widths, budgets, graphics quality and encoding ownership are unchanged. PPD534's
miss-only ownership construction is already fixed; select another measured
registry/encoding/publication suboperation before introducing worker boundaries.
Generation-aware asset handoff ages, cancellation during permit wait, HTTP frame
servicing, main-reply age/wait correlation and release gates remain in the main
ledger rather than being silently called fixed.

Final preservation audit02 executes0: twenty selected snapshots have all66 controls
and exact per-scene finite camera matrices/pose/scalars equal. Saved18explicit/
48absent whitelist entries have zero differences; FullScreen is absent in saved
XML and false live, while actual native full screen is independently observed.
Whole XML session bytes differ, not the selected graphics controls. Each app's
CPU/metallib hashes match its own pinned identity; accepted69450 remains preserved.
Old audit01 is unchanged. Private evidence: analysis-preservation-02 and
analysis-comparison/final-comparison-02, final-paired-results.json.

Full builds/signatures/app verification/dSYMs and actual API/Shader Validation
checks described above pass. Tools: isolated test environment185testsPASS with
one optional retained-specific serializer fixture skipped. Initial default-Python
missingPyYAML and QA-venv missingNumPy failures remain; a private system-site test
venv with pinnedPyYAML6.0.3 resolves them without changing either existing env.
PPD575–581 retain private authoring/collection/reduction failures and actual fixes.
B01 separate offline export02 completes0 in12.67s with90s/export budgets and the
original failed session hash unchanged. Corrected result02 preserves all CPU/GPU/
FRAME numeric invariants while repairing metric distributions/labels. Full command
telemetry is still lossy and never supports a complete command-stage budget.

## Owner publication request and normal launch — 2026-11-22

The owner received the final report and explicitly requested committing and
pushing all reviewed changes. Publication is authorized; visual/FPS acceptance
is not inferred. The final candidate is running normally in native full screen
at Club, with **My last location** selected as instructed, and the actual region
visually verified. No recorder, LEAP helper or automatic GPU capture is active.
Private `owner-review-02` records the exact executable and normal launch; the
preceding `owner-review-01` remains retained.

Publication: reviewed source/tests/reports commit `ec696aa615` is pushed to
`origin/codex/apple-silicon-metal` (push exit0, prior `d219d538ab`). Normal candidate
PID17951 remains full screen at Club; private actual-argv/environment verification
confirms its exact tested binary, no LEAP helper and disabled heavy diagnostics.
Owner visual/performance acceptance and the outstanding engineering work remain
separate. No private recordings or unrelated worktrees were included.
