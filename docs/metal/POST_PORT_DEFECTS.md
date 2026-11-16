# Post-port defect register

This is the mandatory, authoritative record of defects discovered in the ported
viewer and reported regressions, including inherited issues. Read it at session start. Register a discovery or owner
report here before a repair attempt or handoff, even when its cause is unknown.
Other notes, the feature matrix and work ledger link to these IDs; they do not
replace this register. This register also applies while broader port work remains.

## Maintenance contract

- Append the next stable `PPD-NNN` record and advance the next-ID marker. Never
  remove or renumber a record, including closed, duplicate, reverted or failed
  attempts. Reopen the existing record when the same defect recurs.
- Keep source-confirmed defects separate from reported symptoms whose cause is
  unproved. Cross-reference related IDs without implying causal confirmation.
- Every record has the fields shown below. Update evidence, next action and a
  dated history after a material finding, attempted fix, failed retest, revert,
  deferral or owner decision. Preserve earlier failures in that history.
- Status is `open`, `investigating`, `ready_for_owner`, `verified`, `deferred`,
  `dismissed` or `duplicate`. Confirmation is `reported`, `reproduced` or
  `source_confirmed`; it describes evidence, not fix completion.
- `verified` requires documented corrective verification and owner confirmation
  that the repair works. A successful build or automated test alone is not
  closure. Moving to `verified` does not authorize a push.
- `deferred` needs a recorded owner decision and a `Resolution` field explaining
  the deferral. `dismissed` needs evidence that the report is not a defect and a
  `Resolution`. `duplicate` needs `Duplicate of: PPD-NNN` naming another retained
  record. None of these statuses permits deleting history.
- Owner verification is `pending`, `passed` or `failed`. Push approval is
  `not_granted` or `granted`; record the actual owner decision before changing
  it. Keep proposed fixes local until the owner tests, agrees they work and
  explicitly approves the push. Register maintenance does not grant publishing
  permission.
- The ordinary harness permits open records and checks IDs, required fields,
  evidence links and closure rules. The release harness rejects unresolved
  records, including deferred ones. Git-backed checks also compare retained IDs
  against available HEAD/parent history. A shallow export can check structure,
  but cannot reconstruct omitted history or discover an undocumented defect.

Use one `## PPD-NNN — Title` section per record. Keep metadata fields as single
`- Field: value` lines; use paragraphs and a History subsection for detail. Source
references may be unknown for a newly reported issue; write that uncertainty
explicitly. Evidence must link to a retained repository note or primary source.

<!-- next-defect-id: PPD-317 -->

## PPD-001 — Projector preflight can block the entire world frame

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, `indra/newview/pipeline.cpp`, `LLPipeline::setupSpotLight` and the deferred-light traversal.
- Evidence: [Source comparison and failed-session investigation](work/active/busy-turn-local-investigation.md), [signed-range contract and regression work](work/active/projector-per-light-admission.md).
- Next action: Owner verifies busy-location lighting/current-world behavior after the approved4976f233b8 publication. Retain PPD-002 separately until its reported symptom is verified.
- Verification: Local6404f9f6ad collector probe72checks/0failures versus37 baseline failures; native Metal27016assertions/1case pass with API/GPU validation. Candidate68869 build passes; bundle/signature verification recorded in the linked repair note. Owner scene verification pending.
- Owner verification: pending
- Push approval: granted

Native projector collection returns before completing the entire lighting stage
for some UI/protocol-admitted inputs. For example, independent light radius
1 metre and prim Z scale 2 metres produce `1.5 * radius - scaleZ = -0.5`;
FOV zero is also an admitted input. A finite negative range can still shade
correctly under the original forward cookie projection and signed LOD math;
zero range/FOV is singular. The confirmed mismatch is that the original continues the remaining
lights/world, while native composition and later world publication are withheld.

Camera-dependent light selection can expose this path. Attribution to the
owner's busy-region freeze, PPD-002, remains unconfirmed. Missing native cookie
or shadow data and other projector validation failures need separate diagnosis.

### History

- 2026-11-12: Source comparison confirms the whole-stage failure-scope defect.
  Analysis only; no correction selected or approved.
- 2026-11-12: Owner selects PPD-001 for repair. Exact production collector and
  composition probe fails15/34 baseline checks for middle negative-range,
  zero-range and zero-FOV lights. Original GLSL and current MSL both support
  finite nonzero signed range; preserve that light, rather than simply skip it.
  Repair and physical Metal regression verification in progress, local only.

- 2026-11-12: Local6404f9f6ad repairs per-light admission, preserves signed
  projection, checks inverse success and shares complete packed-math validation.
  Real cookie/shadow/encoder failures stay transactional. Automated source/GPU
  regressions and candidate build pass; ready for owner scene test. No push.

- 2026-11-12: Owner-requested exact68869 launch/restart and pre-teleport
  profile completed locally:181.041s trace saved/TOC exported0, teleport start
  and finish inside capture. Detailed metrics and owner acceptance pending.

- 2026-11-12: Owner requests second retained-cache run. Exact68869/PID89531
  recorded181.345s post-arrival; xctrace teardownSIGTRAP prevented normal save.
  Raw ktrace cloned/imported0, CPU/frame coverage verified; exact-build function
  names restored offline with immutable sample data. This is instrumentation recovery, not viewer
  corrective verification or push approval. See the repair note.

- 2026-11-12: Completed [retained-cache CPU/decode/GPU analysis](research/warm-cache-profile-analysis-2026-11-12.md).
  Mainshadow42.43%, decoder142.911CPU-s but only0.645s inlast30s, cadence22.377FPS
  late. VerifiedGPUaverages remain substantial; detailedGPUevents absent even
  afterboundedreimport. No ownerverification/push inferred fromprofiling.

- 2026-11-12: After explicitly approving the restored4normal/6maximum decoder
  policy, owner asks which renderer work is local. Integrator names directional
  shadow occlusion1a12 and this6404 projector repair/tests; owner replies
  "commit them too and push". Push approval granted for those named changes.
  This does not supply explicit scene-correctness verification or close PPD-002.
  Approved scope pushed normally to codex/apple-silicon-metal at4976f233b8;
  private captures and five untracked research notes excluded.

## PPD-002 — Busy-region world freezes while nametags keep turning

- Status: investigating
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-10
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, `indra/newview/llviewerdisplay.cpp`; current world and current-camera UI precede the same swap.
- Evidence: [Initial crowded-region report](work/active/crowd-live-68643.md), [failed 68845 retest and revert](work/active/busy-turn-local-investigation.md), [68869 pre-teleport profile analysis](research/projector-teleport-profile-2026-11-12.md).
- Next action: Capture local/projector completion flags and the first projector return reason or renderer error during a retained-world frame, without changing draw admission.
- Verification: Owner-run 68845 retest failed; live cause remains unknown. The previous automated impostor test did not verify this incident.
- Owner verification: failed
- Push approval: not_granted

LEFT/RIGHT ARROW turning moves nametags smoothly while the background and own
avatar remain fixed, then catch up. The failed-session log shows an older world
retained while new capture frames and UI submissions advance. Current geometry,
light map and base lighting exist in 61 of 62 sampled retained-world receipts;
final lighting composition and post products do not. Geometry and base target
presence is not a GPU completion or displayed-frame receipt.

PPD-001 is a plausible source-level cause; that connection has not been proved.
Do not close this report by fixing a related source defect without verifying the
reported behavior. PPD-003 limits the existing lighting-failure diagnostics.

### History

- 2026-11-10: Owner reports turning/body lag in the earlier crowded-region test.
- 2026-11-12: Owner confirms the whole background briefly freezes, then catches
  up. Candidate 68845 does not fix the behavior.
- 2026-11-12: Owner requests reverting the unapproved push. Normal revert
  `0f7b488264fc60e177e30d445502c8008d4fd37a` restores the pre-push source; owner
  requires explicit approval before future fix pushes.

- 2026-11-12: Local68869 profile analysis observes Overview zero-present buckets
  across49.368–64.358s while main/decoder work and viewer-loop statistics advance.
  Per-frame exports are empty; visibility/instrumentation/publication/queued GPU
  work remain unseparated. Relationship to the owner-reported world-only freeze
  remains unproved; no corrective verification or closure inferred.

## PPD-003 — Continuous lighting retries can suppress failure warnings

- Status: open
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference: No equivalent native retry counter in `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`; inspect its per-light traversal and error reporting as the nearest original boundary.
- Evidence: [Lighting failure-counter analysis](work/active/busy-turn-local-investigation.md).
- Next action: Specify a bounded diagnostic outcome that survives continuous-frame resets and identifies the first local/projector failure; preserve rendering behavior.
- Verification: Source trace only; no diagnostic correction or runtime validation performed.
- Owner verification: pending
- Push approval: not_granted

After a successful G-buffer capture, the next continuous transaction resets
local/projector failed-frame counters. A repeated lighting failure can therefore
remain below the 120-failure warning threshold indefinitely. Some collector
returns do not call `markIncomplete`, so a zero first-incomplete line does not
prove lighting succeeded. This finding does not apply to every G-buffer failure
counter: some incomplete G-buffer retries can accumulate to their threshold.

### History

- 2026-11-12: Identified while investigating PPD-002; no fix implemented. Missing
  warnings must not be used as evidence that local/projector lighting completed.

## PPD-004 — Missing native impostor data invalidates unrelated world batches

- Status: open
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, `indra/newview/llvoavatar.cpp`, `LLVOAvatar::renderImpostor` skips an unavailable avatar target without vetoing the world.
- Evidence: [Reverted candidate and continuing investigation](work/active/busy-turn-local-investigation.md), [candidate source and recorded regression proof](https://github.com/levdarkheart-lgtm/AlchemyFork/commit/2c11d05d53396b45263f517e1521e1d867ae7077).
- Next action: Keep this distinct from PPD-002; reassess original-equivalent per-avatar readiness and obtain owner-approved verification before reintroducing a correction.
- Verification: Old actual-method fixture had 43 intended failures; candidate passed 695 assertions across 3 cases. Candidate was reverted and is not the active implementation.
- Owner verification: pending
- Push approval: not_granted

The native draw caller can see a logically complete impostor target while its
compatible four-texture publication is absent. Capture then clears accepted
world batches and marks the whole scene incomplete, rather than limiting the
unavailable draw to that avatar. The candidate preserved other batches, rebake
retry and transactional publication in a physical native test. It did not
execute the real avatar scheduler or establish the cause of PPD-002.

### History

- 2026-11-12: Source defect and bounded actual-method reproduction recorded.
- 2026-11-12: Candidate `2c11d05d53` built as 68845; automated tests passed.
  Owner's PPD-002 retest failed, and the owner requested reverting the push.
  Candidate removed by `0f7b488264`; retain this defect and its failed delivery
  history. Automated proof is not owner verification or push approval.

## PPD-005 — ImageDecode worker sizing halves Apple Silicon CPU count

- Status: verified
- Confirmation: source_confirmed
- Owner: Codex texture decoder workstream
- Discovered: 2026-11-12
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, `LLAppViewer::initThreads` sizes CPU workers before window/vendor initialization; inherited heuristic also exists in pre-fix `d537f10611`.
- Evidence: [Worker sizing fix, actual SL codec benchmark and local candidate](work/active/texture-decoder-workers-and-codecs.md).
- Next action: Accepted decoder/worker scope is published as4842cb9460 on codex/apple-silicon-metal; unrelated renderer fixes retain separate approval requirements.
- Verification: Actual old/fixed sizing bodies:16 cases, old9 failures/fixed0; M1 Max selects2/4 workers. Source64f10455ce build/346 ARM64 files/strict signature and56 tools tests pass. Isolated OpenJPEG CPU batch time falls46.30% full/48.31% mixed; owner explicitly accepted the later68860 experiment and approved this scope on2026-11-12. No controlled viewer loading/FPS gain is established.
- Owner verification: passed
- Push approval: granted

The default-false legacy graphics-vendor flag halves10 reported M1 Max CPUs
to5 before subtracting the six-core reserve, leaving only two decode workers.
The minimal local160a5f45bb fix removes vendor-dependent halving, preserves
EmulateCoreCount and the2–16 bounds, and logs the selected counts. Four workers
follow the existing formula. This is inherited startup ordering, not established
as a Metal-introduced regression or a cause of PPD-002.

### History

- 2026-11-12: Source diagnosis and owner-authorized local sizing fix completed;
  current patched OpenJPEG retained. Original actual-body test fails9/16;
  fixed body passes16/16 on the M1 Max.
- 2026-11-12: Benchmarked192 complete and48 partial actual SL textures against
  Grok/OpenHTJ2K. Codec alternatives improve isolated complete-input batches
 14–21% at equal four-worker concurrency; Grok partial reconstruction differs
  substantially. Neither alternative integrated into the application.
- 2026-11-12: Local68855 built/checked, unlaunched, ready for owner testing.
  Registered at handoff after the mandatory register was introduced concurrently;
  no push approval granted, no push/upload performed.
- 2026-11-12: Owner requested a separate [OpenHTJ2K experiment](work/active/openhtj2k-local-experiment.md), build and exact launch, then recording only on their signal before teleport. PPD-005 remains ready_for_owner; no acceptance or push approval inferred.
- 2026-11-12: Local db398/68860 built/tested/launched; runtime confirms4 OpenHTJ2K workers with stable cache identity. Owner-triggered181.755s [profile](research/openhtj2k-profile-2026-11-12.md) saved and analyzed; teleport actually completed34s before recording. Keep experimental correctness/owner acceptance pending; no push approval.
- 2026-11-12: Owner explicitly accepted the experiment and requested committing/pushing these changes. Acceptance covers decoder/worker changes and their scoped evidence; it does not approve unrelated shadow-renderer work or establish a controlled end-to-end speedup.
- 2026-11-12: Approved decoder scope pushed non-destructively to codex/apple-silicon-metal at4842cb9460 and remote tip verified. Separate publication-base image5/5,56 tools and32-file harness checks pass; unrelated renderer/governance work and private data excluded.

- 2026-11-12: Owner requests a separate [six-decoder cold/warm experiment](work/active/six-decoder-cold-warm-experiment.md).
  Local reserve4 trial selects6 on ten-core M1 Max; original accepted sizing
  defect remains verified. New concurrency policy is unaccepted, not adaptive,
  and has no push approval. Empty dedicated cache then same populated cache
  requested; existing cache and prior apps preserved.
- 2026-11-12: Local68876 built/checked and two verified-empty-cache login
  captures saved/analyzed normally. [First cold run](research/six-decoder-cold-profile-2026-11-12.md)
  uses184.944decoderCPU-s/peak5.72cores; [second new-location run](research/six-decoder-unvisited-cold-profile-2026-11-12.md)
  uses62.173CPU-s/peak2.43cores. Scenes differ; neither establishes a matched
  four-to-six speedup or optimal concurrency. Second cache retained warm;
  six-worker owner acceptance and push approval remain pending/not_granted.
- 2026-11-12: Owner reports quicker HUD/world loading with six workers and
  requests [hardware-aware adaptive admission](work/active/adaptive-decoder-concurrency.md)
  while continuing to play in68876. Implement/build locally without changing
  the running viewer; profile afterward. Subjective improvement is recorded;
  cross-hardware optimum, adaptive behavior and push approval remain unverified.
- 2026-11-12: Local98f41/68880 adaptive candidate built/ARM64/signature/strict
  source checks pass;44test cases/1222assertions plus platform and controller
  sanitizer checks pass. Worker admission, callbacks, post failure and draining
  exercised. Original apps/cache preserved. Candidate remains unlaunched;
  runtime loading benefit, owner confirmation and push approval are pending.
- 2026-11-12: Owner-triggered68880 first-login/heavy-return profiles save and
  [fully analyze](research/adaptive-decoder-profiles-2026-11-12.md). Adaptive
  rise/reduction and no-cancellation accounting observed; heavy peak5.901decode
  cores and mean observed queue wait2.484s. Different destinations/clipped
  teleport onset prevent a controlled speedup claim. PPD-008 black-login report
  is investigated separately. No adaptive push approval inferred.
- 2026-11-12: Owner requests why mean admission wait is2.48s. Read-only
  [queue forensics](research/decoder-queue-wait-forensics-2026-11-12.md) validates
  12005 starts/29818200ms wait,97.555% in the first logged27s. Arrivals outrun
  six busy workers; gate/downshift and rounding do not explain the large burst.
  FIFO lacks decode priority/cancellation; actual obsolete waste unmeasured.
  No source change, new capture, fixed/adaptive speedup or push approval.
- 2026-11-12: Owner explicitly authorizes the [larger burst candidate](work/active/eight-burst-decoder-experiment.md)
  and viewer launch. Valid topology now budgets up to fastest-tier count within
  enabled-total-minus2 headroom, normal half-peak; M1 Max4/8, unknown-topology
  fallback preserved. Local299e748/68884 doctor/build/static+47cases1500assertions
  pass; exactPID61154 normal4/max8/adaptive1 startup observed, old apps/cache
  preserved. Both owner-triggered traces save; owner reports slower gray
  textures and worse FPS, exits/rejects8 and requests restoring4normal/6maximum.
  Source restored exactly98f41; prior68880 relaunchedPID63726/cache retained.
  Owner explicitly approves committing/pushing restored4/6 decoder scope;
  then explicitly includes named shadow-occlusion/projector repairs and tests.
  No eight-worker gain is claimed; active source returns to4/6.


- 2026-11-15 integrated69122 profile evidence: Owner-authorized busy-club teleport trace300.725s records one Texture WARNING: OpenHTJ2K SOT marker expected but3FEE found. Asset bytes/cause and retry/fallback outcome unproved; no codec source change, decoder regression or new renderer error inferred. See [two-scene report](research/integrated-build-69122-quiet-club-20261115.md); raw asset identifiers remain private. Existing accepted experiment/history retained.

## PPD-006 — Legacy avatar alpha loses the original water-clipping exception

- Status: open
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-11
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, `class2/deferred/alphaF.glsl` bypasses `waterClip` under `IS_AVATAR_SKIN`; the original avatar alpha program enables that permutation.
- Evidence: [Original exception and current reachable native consumer](work/active/avatar-alpha-water-clip.md).
- Next action: Design a bounded coverage comparison on both water-plane sides for the legacy avatar-alpha path, with ordinary rigged-object clipping controls, before choosing a correction.
- Verification: Source comparison only; no physical regression test or corrective implementation verified.
- Owner verification: pending
- Push approval: not_granted

Eligible legacy avatar skirt, eyelash and hair draws reach the native generic
pre-water simple-alpha fragments, which apply water-plane discard without the
original avatar-specific exception. This is a separate source-confirmed coverage
divergence. It has not been demonstrated as the cause of the coat, beard-light
or turning reports. Preserve ordinary rigged-object clipping when investigating.

### History

- 2026-11-11: Frozen source audit records the missing avatar-alpha exception;
  no GPU execution or visual-cause claim.
- 2026-11-12: Rechecked against committed `8599bfce5195`; the path remains
  reachable. Migrated the previously recorded finding into this mandatory
  register. No fix, build or runtime test performed.

## PPD-007 — HUD font position regeneration creates fresh native buffer stores

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex font-position investigation
- Discovered: 2026-11-12
- Original reference: Local upstream/develop `65c39b7bad928e165f1acdc0cbf39478e017ef1d` and immediate child `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`; exact x/y invalidation and fresh display-list buffers predate this port.
- Evidence: [Source, geometry, allocation and profile audit](research/font-position-cache-audit-2026-11-12.md), [active audit ledger](work/active/font-position-cache-audit.md).
- Next action: Select a local completion-safe backing-store reuse experiment, or prove a smaller effective-placement cache key; preserve glyph phase/scale/clipping/underline/emoji/fallback/right_x and lifetime semantics.
- Verification: Actual cache-decision body12checks and actual placement-math12checks pass in private CPU probes; exact68849 binary/dSYM raw-PC audit confirms5.485CPU-s/3.34919%main CPU in name-label Position-selected ancestry. No rendering fix, GPU/visual test or measured speedup for this issue.
- Owner verification: pending
- Push approval: not_granted

Projected fractional label origins can change with unchanged text. Exact x/y
comparison selects full regeneration, clears captured lists and creates fresh
native buffers per successful nonempty flush. Some regeneration is necessary
because glyph phase/UV/extent/carry can change. The profile identifies the first
selected invalidation reason; it does not prove all that time is avoidable or
that later style/span/color conditions are unchanged. This is a confirmed
performance mechanism, not a proved visual correctness defect or Metal-introduced
invalidation regression. Existing bitmap/shaping/replay caches remain functional.

### History

- 2026-11-12: Owner requests grounding of the earlier code/profile claim. Source
  chain, true local upstream ancestry, actual-body probes and seven-call-site
  machine-code audit confirm the mechanism and profile attribution. Constrained
  reuse options recorded; no implementation selected or push approval inferred.

## PPD-008 — Login world stays black briefly while UI remains visible

- Status: verified
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference: 1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 indra/newview/llviewerdisplay.cpp628 begins world traversal at STATE_PRECACHE; llstartup.cpp2727/2760 and llprogressview.cpp retain a1s normal curtain fade without native readiness. Candidate pipeline.cpp2130 delays asynchronous world-renderer warm-up until STARTED. No original runtime comparison completed.
- Evidence: [Owner report and capture context](work/active/login-black-world-investigation.md).
- Next action: Retain the verified correction and scoped owner-approved publication; reopen on recurrence. Keep PPD-002/003 and music-backend work separate.
- Verification: Localb0585e4c1a/68894 build/static pass;133actual-body assertions and31,825nativeGPU assertions/16cases pass with API/GPU validation. ExactPID8114 prewarms7.36349s at LOGIN_WAIT, first real world+curtain fade20:23:29. Owner confirms black screen resolved.
- Owner verification: passed
- Push approval: granted

Keep this distinct from PPD-002 busy-turn retained-world freezes and PPD-001
source-confirmed projector admission. It predates this adaptive test according
to the owner; no causal link to decoder concurrency is established. Expected
startup waiting, incomplete native transactions and presentation failure remain
unseparated. Aggregate zero-cadence buckets do not prove black pixel content.

### History

- 2026-11-12: Owner reports black world/visible UI for roughly5–10seconds during
  first adaptive68880 login, also observed once before. Registered before
  diagnosis/repair; first saved180.840185s capture includes login and world
  entry. Cause unknown, no repair attempted or push approved.
- 2026-11-12: Eight original/candidate source reads and capture-aligned log
  establish the late-warm-up/cleared-background mechanism: STARTED/warm start
  +25.965s, warm complete/first logged world-source availability+32.965s,
  worker duration6.60176s, UI submissions between. Upgrade source confirmation
  only; visual attribution remains unproved and the record stays investigating.

- 2026-11-12: Owner supplies black-world screenshot19:45:42 after100% login.
  Exact68889/PID77975 logs STARTED/main-warm19:45:38, factory8.20129s and first
  final BGRA8 world source19:45:46. Screenshot is inside that interval, with
  UI/HUD/name tags present. This reproduces pixels and aligns the late native
  preparation mechanism. Generic world+UI completion19:45:32 can be clear+UI;
  it is not real-world readiness. Original prepares shader programs before login
  and draws at PRECache; native preparation is gated untilSTARTED. Owner asks
  diagnosis first; renderer code unchanged. Early prewarm/readiness is next.
- 2026-11-12: Owner explicitly requests fixing the black screen. Implement early
  asynchronous preparation and real-world-qualified curtain completion locally,
  preserving pre-STARTED scene admission, device/build/reload lifetimes and an
  actionable Quit path on delayed or failed preparation. Do not interrupt the
  current owner session or push before owner testing/approval.
- 2026-11-12: Local implementation separates early background PSO creation from
  scene admission, validates build/device/shader lifetimes and gates the curtain
  on a real post-login world-backed submission.63curtain assertions/13cases pass;
  timeout remains opaque/actionable. Full build and renderer fixture underway;
  no owner visual retest, active-viewer restart or push yet.

- 2026-11-12: Finalb0585e4c1a/68894 build/signature/346ARM64/strict-source
  pass.133source checks and31,825physicalMetal assertions pass. ExactPID8114
  starts at20:22:04 and completes main renderer preparation at20:22:11,
 7.36349s while waiting at login. Owner logs in; first real world submission
  and qualified curtain fade both occur20:23:29. Owner confirms: "its good,
  black screen is resolved". Mark verified/owner passed; push not granted.

- 2026-11-12: Owner explicitly authorizes pushing the verified black-screen fix to GitHub. Publish the PPD-008 source and evidence documentation on codex/apple-silicon-metal; independent native-music research/backend work remains local.

## PPD-009 — Startup media indicator active while parcel music is silent

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, `indra/newview/llstatusbar.cpp`, `llvieweraudio.cpp`, `llviewermedia_streamingaudio.cpp`; aggregate all-media control and stream transport/fade ownership.
- Evidence: [Startup media-state investigation](work/active/music-startup-indicator.md).
- Next action: Owner tests local68889: initial Play when parcel music is stopped, one click starts music, later Pause works, other-media fallback works without an enabled parcel stream. Keep the separate startup-delay report PPD-010 open.
- Verification: Actual-body22check fixture fails7 baseline checks/takes2clicks versus zero failures/1click in candidate. Local9f8d175830/68889 doctor/build/346ARM64/signature/strict-source and staged XUI checks pass. ExactcandidatePID77975 launched with retained cache at LOGIN_WAIT; owner explicitly requests music-fix commit/push. No separate audible-timing measurement.
- Owner verification: pending
- Push approval: granted

Owner reports initial active indicator without audible music. OFF changes the
indicator correctly; subsequent ON starts music after several seconds, and later
pause works. The top-bar button controls music, parcel media and in-world media;
active appearance alone is not proof of an audible music stream. Preserve the
owner's autoplay preferences and all-media behavior during diagnosis.

### History

- 2026-11-12: Report registered before repair. Owner clarifies top-bar control.
  Original comparison finds the host indicator, audio fade/URI and parcel
  autoplay paths unchanged. Backend MEDIA_LOADING counts as playing; failed
  same-URI requests may remain latched, but that explanation is unverified.
  No viewer/audio control or source change performed for this report.
- 2026-11-12: Owner confirms silence persists beyond30seconds. Current
  saved/default settings show streaming enabled but autoplay0/off; two saved
  world-session logs contain no nonempty music-start request. OFF/ON manually
  starts music independently of autoplay. This supports intended aggregate
  behavior, without proving which branch was active at the reported moment.
  No source/preferences changed; any later fix remains local pending approval.
- 2026-11-12: Owner rejects the aggregate explanation as a resolution: active
  button must represent land music rather than require OFF/ON first. Correct the
  UI selector: when streaming music is enabled and a parcel URL exists, derive
  the indicator from parcel audio; otherwise retain aggregate behavior. Keep
  global start/stop action, loading cancellation and autoplay preferences.
  This intentional source-confirmed UX correction supersedes the earlier
  no-repair conclusion; owner test/push approval remains pending.
- 2026-11-12: Local9f8d175830 corrects music-primary selection, initial Play
  and tooltips without changing global actions, decoder4/6, autoplay, fades or
  transport.22actual-body checks pass; loading cancellation is preserved.
  Separate68889 app built/verified, prior68880 hash unchanged. No push/launch.

- 2026-11-12: Owner requests exact candidate launch;68889/PID77975 reaches
  LOGIN_WAIT with normal4/max6/adaptive1 and retained cache, no recording or
  music-preference override. Owner then explicitly says commit/push music fix.
  Approval applies to PPD-009, not a new playback backend or PPD-008 repair.

## PPD-010 — Parcel music starts slowly compared with Firestorm

- Status: ready_for_owner
- Confirmation: reported
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, viewer audio fade and plugin-backed streaming; official Firestorm `176c8a16ced2d13d455e68b06c500c8def0e40d6` macOS build/native stream selection.
- Evidence: [Music startup latency source comparison](work/active/music-start-latency.md), [Apple-native playback investigation](research/apple-native-parcel-music-2026-11-12.md).
- Next action: Retain the owner-accepted native replacement; formal closure of the original relative-latency report awaits corroborated timing/reproduction. Owner acceptance and approval to publish this backend are already granted.
- Verification: Local f826eea135/68898 replaces VLC music hosting with native Core Audio/AVFoundation. Source-identical native backend passes271assertions/14cases; full build,346ARM64 slices, deep strict signature and strict source gates pass. No controlled audible comparison or owner verification of this candidate has run. Owner tests exact68898 and states "verified, works. you can commit and push this to github"; overall native-music acceptance and scoped publication approval are explicit. No controlled latency benchmark or specific live voice-call observation is inferred.
- Owner verification: passed
- Push approval: granted

Keep delayed playback separate from PPD-009's false-active/first-click UI issue.
A fade is a volume ramp, not a fixed silent wait. The7000ms constant applies
only to adjustable-buffer backends and is not passed to the VLC stream adapter.

### History

- 2026-11-12: Owner reports5s+ starts versus1–2s in Firestorm and asks which
  backend it uses. Pinned official source confirms normal macOS ReleaseFS uses
  FMOD; this FAudio viewer falls back to a separate VLC stream plugin. Stop
  deletes that plugin, so later play repeats cold startup. This is a candidate
  overhead source, not a measured explanation of the full delay. No settings,
  stream state or latency code changed; next step is matched stage timing.

- 2026-11-12: Owner requests Apple-native alternatives. Source trace and muted
  standalone AVPlayer tests confirm MP3/AAC/HE-AAC, ICY metadata, PLS/redirects
  and Ogg/Vorbis viability on M1Max/macOS26.5.2. Default synthetic MP3 readiness
  still takes about6s; native API selection alone is not a latency fix. Core
  Audio packet/queue investigation and exact evidence are retained in the linked
  research. No production replacement, audible comparison or closure.

- 2026-11-12: Owner authorizes replacing VLC parcel music with the researched
  native Apple backend, conditional on preserving voice. Implementation isolated
  from the separate black-screen session; [native replacement ledger](work/active/native-parcel-music.md)
  records ownership, protocol tests and voice boundaries. No push approval or
  successful integrated playback inferred from authorization.

- 2026-11-12: Owner confirms the exact launched68898 works and explicitly
  authorizes committing/pushing the native-music replacement. Mark owner
  verification passed and scoped push approval granted; preserve prior failed
  candidates and the limits of automated versus real-call evidence.


The owner accepts the new backend, but the original Firestorm-relative latency
comparison remains reported rather than reproduced or source-confirmed. Keep
that evidence distinction; do not invent a timing measurement to close it.

## PPD-011 — VLC parcel music cannot deliver structured ticker metadata

- Status: verified
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference: Inherited in `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`; `indra/media_plugins/libvlc/media_plugin_libvlc.cpp` title messages, `indra/llplugin/llpluginclassmedia.cpp` metadata receiver and `indra/newview/llviewermedia_streamingaudio.cpp` consumer.
- Evidence: [Native-music investigation and source trace](research/apple-native-parcel-music-2026-11-12.md).
- Next action: Retain the owner-accepted native music implementation and corrective evidence; reopen on regression. Future fixes still require explicit approval before pushing.
- Verification: The inherited VLC metadata gap is removed from the music route. Actual native tests deliver split ASCII/Latin1/legacy ICY song/station tags through the existing LLSD signal on the viewer update thread. Native suite271assertions/14cases and candidate build/static checks pass; real parcel ticker verification remains pending. Owner tests exact68898 and states "verified, works. you can commit and push this to github"; overall native-music acceptance and scoped publication approval are explicit. No controlled latency benchmark or specific live voice-call observation is inferred.
- Owner verification: passed
- Push approval: granted

This is a source-confirmed inherited message-contract gap, separate from
PPD-010 startup delay and PPD-009 playback indication. It does not establish
that a particular parcel station supplies usable metadata, nor that metadata
caused either playback symptom. No fix attempted.

### History

- 2026-11-12: Discovered during the owner-requested native Apple music study.
  Current/original sender, sole receiver assignment, adapter and ticker readers
  inspected independently. Registered before implementation or handoff;
  retain the defect even if replacing the VLC music path later repairs it.

- 2026-11-12: Owner authorizes the native replacement; implementation will map
  ICY/station metadata into the existing ticker contract. Actual protocol and
  metadata regressions are being added before candidate verification. No closure
  or push approval; owner verification remains pending.

- 2026-11-12: Owner confirms the exact launched68898 works and explicitly
  authorizes committing/pushing the native-music replacement. Mark owner
  verification passed and scoped push approval granted; preserve prior failed
  candidates and the limits of automated versus real-call evidence.

## PPD-012 — Initial native music candidate cannot start its default output

- Status: verified
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference: The prior VLC music path at `9f8d17583097d25a7d086296d9553858f6c9853b` does not use AudioQueue. This is a new, unshipped native-replacement candidate issue, not attribution of PPD-010's original latency report.
- Evidence: [Native replacement attempts and actual tests](work/active/native-parcel-music.md).
- Next action: Retain the owner-accepted native music implementation and corrective evidence; reopen on regression. Future fixes still require explicit approval before pushing.
- Verification: Initial queue_start -66680 and finite-EOS failures are retained below. Corrected production backendc25f7872 passes271assertions/14cases plus focused11assertion finite-tail retest. Full f826eea135/68898 build,346ARM64 slices, deep strict signature and strict source checks pass. No viewer launch or actual voice call was performed by this task. Owner tests exact68898 and states "verified, works. you can commit and push this to github"; overall native-music acceptance and scoped publication approval are explicit. No controlled latency benchmark or specific live voice-call observation is inferred.
- Owner verification: passed
- Push approval: granted

The first candidate set kAudioQueueProperty_CurrentDevice to a null CFStringRef.
The setter and getter appear successful, but an actual start fails on this Mac.
A previous never-started queue probe did not verify playback; retain that failed
inference. No running viewer or published music backend was changed.

### History

- 2026-11-12: Source review also identifies and repairs synchronous parser-callback
  errors being overwritten, incomplete queue setup, stale reconnect clock values,
  discarded playlist alternatives and blocking per-frame native state reads.
  These are initial candidate findings, not proven causes of existing viewer
  symptoms. Queue callbacks reclaim compressed buffer storage before decoded
  audio necessarily finishes, so tail accounting is being corrected against the
  native playback clock. Detailed attempts remain in the linked ledger.
- 2026-11-12: First actual MP3 test fails and full initial suite is stopped rather
  than treating protocol metadata or AVFoundation fallback as a Core Audio pass.
  Muted queue-start comparison reproduces the null-device cause; corrections and
  full retest are in progress, local only.

- 2026-11-12: Corrected default output passes all5actual CoreAudio formats.
  Source-pinned full14-case retest passes13cases/267assertions but fails finite
  EOF state (1assertion):155MP3packets describe178560nominalframes while the file
  has176400validframes +576priming +1584remainder. Nominal remaining-frame count
  cannot reach zero against a trimmed4s playback clock. Native asynchronous stop
  and IsRunning completion replace that EOF inference. Focused real-body retest
  passes11assertions, confirms>=3.9s media and>=3.5s walltime before stopped; full
  repaired-source retest and bundle validation remain in progress.

- 2026-11-12: Final repaired-source suite passes all14cases/271assertions with
  unchanged source/binary hashes. Exact f826eea135/26.4.0.68898 candidate builds
  and passes ARM64/signature/source gates; WebRTC code is unchanged and477voice
  symbols remain in the final executable. Ready for owner testing, not closed;
  no launch, real voice call or push performed.

- 2026-11-12: Owner confirms the exact launched68898 works and explicitly
  authorizes committing/pushing the native-music replacement. Mark owner
  verification passed and scoped push approval granted; preserve prior failed
  candidates and the limits of automated versus real-call evidence.


## PPD-013 — Water coverage forms camera-angle-dependent bands

- Status: verified
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, class3/deferred/waterHazeV.glsl passes full clip-space float4; waterHazeF.glsl divides interpolated XY/W in the fragment. Native haze pre-divides before perspective interpolation.
- Evidence: [Owner images and water investigation](work/active/water-angle-artifacts.md).
- Next action: Publish the owner-accepted PPD-013 correction and retain its regression evidence; reopen on recurrence. Keep PPD-015 separate and uncorrected.
- Verification: Locala6f4e8ff41/Water Haze Fix26.4.0.68894 built and verified346ARM64/signature/strict-source. Identical physical GPU test executable fails8checks with old shader, passes171318assertions with corrected library;32197existing water/world assertions/4cases also pass, with API/GPU validation. Original shader math restored. Owner tests the exact launched candidate and confirms "fixed.you can commit and push it"; scene acceptance and scoped push approval are explicit.
- Owner verification: passed
- Push approval: granted

Broad diagonal bands appear/disappear when the avatar/camera turns. Further
rotation changes much of the visible shallow surface to blue water. This is
separate from PPD-008 startup and PPD-002 busy-region retained-frame incidents.
Depth conflict, coverage clipping and geometry/culling are hypotheses, not a
confirmed attribution. Do not hide the symptom with an arbitrary water-height
offset, disabled quality feature or changed depth admission.

### History

- 2026-11-12: Owner provides three screenshots and offers live camera rotation.
  Exact app selection identifies68898; first screenshot tool request times out,
  retry via combined accessibility/screenshot succeeds twice. Viewer controls
  and settings remain untouched. Private images retained outside Git. Register
  before any renderer repair; investigate locally.

- 2026-11-12: Original/native audit confirms an interpolation contract defect:
  native field haze divides clipXY/W per vertex then perspective-interpolates
  those UVs again, unlike original full-clip interpolation. VariableW yields
  incorrect scene-depth sampling, reconstruction and haze/water-plane coverage.
  A physical GPU field-vs-fullscreen regression is being prepared before local
  corrective validation. Attribution to all owner bands remains pending retest;
  occlusion Y-flip suspicion is withdrawn after composing viewport transforms.

- 2026-11-12: Actual M1Max red/green run uses the same test executable
  SHA643eab51: uniformW controls pass the frozen old library; four perspective/
  depth-convention combinations fail8checks, with maximum channel error up to
  .293213. Corrected library passes all171318assertions. Existing real-world/
  water/normal/cull cases pass32197assertions/4cases. Separate Release candidate
  builds and passes signature/ARM64/source gates. No viewer restart, scene
  resolution claim or push; owner visual test remains next.

- 2026-11-12: Owner requests the candidate launch. Exact copied Water Haze
  Fix68894/sourcea6f4e8ff41/SHA4994146e startsPID49892 at21:48:15UTC; native
  process path and3s stability verified. Owner retests and reports "fixed.you
  can commit and push it". Mark verified/owner passed and approve PPD-013
  publication; no separate PPD-015 implementation or HUD repair approved.

## PPD-014 — Automatic CI spends unwanted runs and manual releases select incompatible jobs

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference:81bff208a7 `.github/workflows/metal-harness.yml`, `build.yaml` and `tag-release.yaml`; default-branch inherited schedules remain active unless disabled.
- Evidence: [Workflow failure investigation and manual release ledger](work/active/manual-release-actions.md).
- Next action: Owner confirms the manual workflow/release experience if desired; retain disabled automatic workflows and use the Metal branch selector for future explicit builds.
- Verification: Run43338929395 succeeds and publishes26.4.0.68901-alpha5 from9646f4e. Hosted116tests, precompile source identity,346ARM64slices, deep strict signatures and package/provenance checks pass. Downloaded ZIP/checksum/provenance match GitHub digests; extracted app/three owned executable hashes and license match CI. Static release evidence; owner confirmation pending.
- Owner verification: pending
- Push approval: granted

Owner explicitly asks to fix unrequested automatic Actions, investigate previous
failures and manually trigger a release from the current Metal branch. This
approval covers the workflow repair/publication/dispatch and requested release,
not raising spending limits or changing billing/repository permissions.

### History

- 2026-11-12: Registered owner report and distinct log-confirmed causes before
  final repair validation/publication. Disable legacy automatic workflows so
  main-branch schedules cannot keep creating runs; keep old failed history.
  Stale queued runs reject both normal and force cancellation with contradictory
  409s; no active jobs are observed. Do not report them successfully canceled.

- 2026-11-12: Parallel canonical register already reserves PPD-013 for the water
  report. CI receives PPD-014 before publication; preserve that unrelated report
  and its unapproved working files. Workflow publication excludes canonical
  shared records until their parallel work can be integrated safely.

- 2026-11-12: Scoped workflow publication ea1ff8d9d5 pushed; canceled initial
  incorrect source-SHA dispatch43312529221 before build. Corrected43312577772
  reaches ARM64 runner/toolchain but fails because cached Python tools omit pip.
  Add NumPy to that cached environment; actual pip-free imports and108tests pass.
  Repair bfaaa21cda pushed; new manual run43314101072 targets its exact SHA.
  Follow this run through packaging and release assets; no success claim yet.

- 2026-11-13: Run43314101072 compiles and packages successfully;346ARM64
  slices and strict signature verification pass. Publication is correctly
  stopped by missing embedded source SHA. Actual minimal CMake reproduction
  proves BuildVersion.cmake skips source-hash discovery whenever revision or
  GITHUB_RUN_ID supplies the numeric version. Local cached builds hid this
  because AL_GIT_REVISION_COMMIT already existed. Separate source identity from
  numeric version selection and add a generated-header gate before compilation.
  No tag/release was published. Dependency binary cache saved successfully.


- 2026-11-13: Source SHA discovery now runs independently of numeric version
  overrides and clears stale cached identity on Git failure. Eight real-CMake
  cases and116tool tests pass; actual workflow guard rejects empty/wrong identity
  and passes against the corrected production header before compilation. Retry
  current approved branch including water3e17d4e; earlier attempt published nothing.


- 2026-11-13: Repair9646f4e published; explicit run43338929395 reuses
  dependency cache and passes the actual configured-header identity gate before
  compilation. Three-CPU viewer build is running; expected26.4.0.68901-alpha5.
  Preserve the previous source-identity failure; no release success claim yet.


- 2026-11-13: Run43338929395 publishes 26.4.0.68901-alpha5; exact tag source
  9646f4e839ed1d32d2b5bb1fcecc9a1a9601e2e5. Independent download verifies
  ZIP SHA256 08a6d2163dc422b7c006e9a0c5bdd37d3fa26b03253878203a16402f87bc01b7,
  three asset digests,346ARM64slices, signatures and owned executable/notice
  hashes. All116hosted tool tests pass. Mark ready_for_owner, not owner-verified;
  preserve prior failed attempts and all unrelated open product defects.


- 2026-11-13: Owner requests the complete pushed upstream-port release as Alpha6,
  with local packaging if hosted budget blocks it. Manual43473591856 at exact
  af72868396 starts on ARM64 and passes source/toolchain/dependency/configured
  identity gates; no current billing rejection. A verified local incremental
  package finishes ahead of the fresh hosted compile, so the run is deliberately
  canceled and its publication step skipped. Local346ARM64/signatures/source/
  dependency/116tools and full ZIP roundtrip checks pass. Alpha6 published as
  release444559882/tag26.4.0.68984-alpha6, with ZIP/SHA/provenance. Independent
  downloaded bytes match all three GitHub digests; extracted app/source/import/
  feature/signature/ARM64 checks pass. Owner workflow confirmation remains pending.
  See [Alpha6 release ledger](work/active/release-alpha6-20261113.md). No new
  workflow/source fix, billing change or unrelated defect closure.

## PPD-015 — Water grazing-angle steepness uses eye-space vertical

- Status: open
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, class1/environment/waterVaryingsV.glsl50–53 separates eye-space vary_position and agent-relative view; class3/environment/waterF.glsl194–197 scales steep_wave.z using normalized agent-relative view.z.
- Evidence: [Separate water-coordinate audit](work/active/water-angle-artifacts.md).
- Next action: Add an independent actual-shader grazing-angle/coordinate-space regression before considering a separate correction; keep distinct from PPD-013 haze interpolation and owner band attribution.
- Verification: Current native shader calculates both eye-space view_vector and agent_view_vector but scales steep_wave.z with eye-space Z at almetal_builtin.metal8028. Source mismatch confirmed independently; no physical regression, runtime attribution or correction performed.
- Owner verification: pending
- Push approval: not_granted

The original steepening follows the viewing angle relative to the horizontal
water plane. Native uses the camera optical-axis component instead, affecting
refraction XY and sun-normal inputs. This does not establish that it caused
PPD-013 bands or the supplied coverage changes. No speculative combined fix.

### History

- 2026-11-12: Discovered during PPD-013 source audit; original and native
  producer/consumer coordinates inspected and independently confirmed. Register
  before any repair. Haze-only candidate deliberately retains this behavior
  so its scene retest isolates the confirmed interpolation correction.


## PPD-016 — Thin lower HUD line changes coverage during movement

- Status: deferred
- Confirmation: reported
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, indexed HUD geometry capture, fullbrightV/F texture/alpha handling and ordinary AnisoWrap sampler; native lldrawpool.cpp captureMetalHUDBatch and ALMetalUIRenderer::drawHUDIndexed. No original runtime comparison yet.
- Evidence: [Owner HUD images and source audit](work/active/hud-bottom-line-investigation.md).
- Next action: Park at the owner’s request; resume the original-viewer/actual-face comparison only when the owner asks to revisit it.
- Verification: Owner images21:50:15/21:50:51/21:51:50 show a thin lower line at the selected HUD frame boundary. It moves with the HUD and changes from continuous to two end segments. Source audit finds matching sampler, alpha-product/cutoff, texture transform and triangle winding; no native defect or corrective change established.
- Owner verification: pending
- Push approval: not_granted
- Resolution: Owner does not recall original-viewer behavior and explicitly says "lets park it for now". Retain report and evidence without claiming a renderer defect or repair.

Owner briefly suggests a horizon dependency, then explicitly withdraws it and
notes the line could belong to the HUD itself. Do not retain the withdrawn
horizon restriction as a reproduction condition. A thin actual side face and
transparent atlas/mip edge coverage are candidates, not confirmed causes.

### History

- 2026-11-12: Registered owner images and movement observations. Selected HUD
  outline shows a large mostly transparent frame with lower geometry at the
  line position. Read-only original/native audit finds no concrete mismatch;
  await comparison or inspect exact batch/face before changing renderer policy.
  No UI, shader, sampler, geometry or alpha-cutoff change made for this report.

- 2026-11-12: Owner does not remember whether original Alchemy/Firestorm shows the line and explicitly parks this investigation. Mark deferred; no renderer change, closure or push approval.

## PPD-017 — HTTPS login redirects can resend credentials off TLS

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction cc2838504c / #637. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; login/lllogin.cpp:201; _httpoprequest.cpp:559; llxmlrpctransaction.cpp:214.
- Evidence: [Upstream general audit](research/upstream-general-review-2026-11-13.md), [prioritized upstream backlog](research/upstream-port-review-2026-11-13.md).
- Next action: Owner tests exact First Fixes68914 candidate for ordinary login/world/inventory/fonts and retained media/voice behavior; do not close or push without the required owner decisions.
- Verification: Local1f635d6b75 /26.4.0.68914 Release candidate build,136registered entries,116tool tests,346ARM64files, deep strict signature, embedded source and owned imports pass. Both secure-login redirect layers and their registered tests pass; ordinary HTTPS and explicit plain-grid semantics preserved.
- Owner verification: pending
- Push approval: granted

The application accepts next_url without retaining HTTPS; HTTP redirects also lack an HTTPS-only restriction. Both layers need the final upstream guard. No downgrade or credential exposure was reproduced.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-018 — Simulator Xfer names choose local cache files

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction 4fa42289ac / #614. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llmutelist.cpp:958; llviewerobject.cpp:3627; llxfermanager.cpp:439; llxfer_file.cpp:86.
- Evidence: [Upstream general audit](research/upstream-general-review-2026-11-13.md), [prioritized upstream backlog](research/upstream-port-review-2026-11-13.md).
- Next action: Owner tests exact First Fixes68914 candidate for ordinary login/world/inventory/fonts and retained media/voice behavior; do not close or push without the required owner decisions.
- Verification: Local1f635d6b75 /26.4.0.68914 Release candidate build,136registered entries,116tool tests,346ARM64files, deep strict signature, embedded source and owned imports pass. Xfer short-name/header/callback tests pass; local task/mute filenames are viewer-generated and their client integration builds.
- Owner verification: pending
- Push approval: granted

A remote name is reused for the local cache target and cleanup; short names can also reach suffix substrings with invalid positions. Upstream generates independent local temporary names and bounds suffix tests.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-019 — IM binary buckets can be read beyond their copied bytes

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction 9063a7dfe8 / #645. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llviewermessage.cpp:2266; llimprocessing.cpp:1107,1381,1408.
- Evidence: [Upstream general audit](research/upstream-general-review-2026-11-13.md), [prioritized upstream backlog](research/upstream-port-review-2026-11-13.md).
- Next action: Owner tests exact First Fixes68914 candidate for ordinary login/world/inventory/fonts and retained media/voice behavior; do not close or push without the required owner decisions.
- Verification: Local1f635d6b75 /26.4.0.68914 Release candidate build,136registered entries,116tool tests,346ARM64files, deep strict signature, embedded source and owned imports pass. Exact production bounded bucket-text body passes9 ASan/UBSan checks; old constructor control reproduces heap overflow. Full IM application builds; no live IM session test.
- Owner verification: pending
- Push approval: granted

Declared size is passed beyond a bounded copy, and session/inventory text is consumed as an unbounded C string. Keep copied byte length and bounded NUL searches together.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-020 — Out-of-domain packet IDs can hang the main thread

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction 6687792a35 / #636. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llcircuit.cpp:734–749.
- Evidence: [Upstream general audit](research/upstream-general-review-2026-11-13.md), [prioritized upstream backlog](research/upstream-port-review-2026-11-13.md).
- Next action: Owner tests exact First Fixes68914 candidate for ordinary login/world/inventory/fonts and retained media/voice behavior; do not close or push without the required owner decisions.
- Verification: Local1f635d6b75 /26.4.0.68914 Release candidate build,136registered entries,116tool tests,346ARM64files, deep strict signature, embedded source and owned imports pass. Registered circuit49assertions/11cases pass; removing only the domain guard times out in the same test.
- Owner verification: pending
- Push approval: granted

The predicate computes distance modulo 24 bits but the loop compares against the original 32-bit input. A high-bit number is unreachable by that wrapping loop. This is separate from PPD-002's unproved render-freeze cause.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-021 — Network object and layer decoders lack required payload bounds

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction #619 and #634; 5ad54288ed, a0235bf308, a6c038538f, df1de743b5, 8a0c98ae13. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; LLNameValue/LLDataPacker; llvovolume.cpp:479; llviewerobject.cpp:1889,6795; llvlmanager.cpp:95; initial Xfer packet reader.
- Evidence: [Upstream general audit](research/upstream-general-review-2026-11-13.md), [prioritized upstream backlog](research/upstream-port-review-2026-11-13.md).
- Next action: Owner tests exact First Fixes68914 candidate for ordinary login/world/inventory/fonts and retained media/voice behavior; do not close or push without the required owner decisions.
- Verification: Local1f635d6b75 /26.4.0.68914 Release candidate build,136registered entries,116tool tests,346ARM64files, deep strict signature, embedded source and owned imports pass. Registered object/message/primitive/layer/profile tests pass, with malformed input cases and complete cache lifetime prerequisites; no malformed live simulator traffic executed.
- Owner verification: pending
- Push approval: granted

Retained subcases: static name-value buffer overflow/stale fields, particle/TE/extra-param/motion length and type checks, fixed object-data buffers, patch-size and bitstream bounds, partial wind/land patches, and short initial Xfer headers. See the detailed general-review rows for the complete corrective sequences; not every malformed input was independently executed.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-022 — Mesh decoding has unsafe weights and unbounded face/index work

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction #570 plus efd275c864 in #567. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llmath/llvolume.cpp:2379,2413,2616–2653.
- Evidence: [Upstream geometry audit](research/upstream-geometry-review-2026-11-13.md), [prioritized upstream backlog](research/upstream-port-review-2026-11-13.md).
- Next action: Owner tests exact First Fixes68914 candidate for ordinary login/world/inventory/fonts and retained media/voice behavior; do not close or push without the required owner decisions.
- Verification: Local1f635d6b75 /26.4.0.68914 Release candidate build,136registered entries,116tool tests,346ARM64files, deep strict signature, embedded source and owned imports pass. Expanded actual compressed decoder plus SIMD kernels passes201assertions/4cases under ASan/UBSan and registered tests; old weight/short-attribute controls reproduce heap overflow. PPD-043 zero fallback corrected separately, PPD-044 cost report remains open.
- Owner verification: pending
- Push approval: granted

Retained subcases: an influence read can consume up to three bytes after a one-byte check; missing trailing vertices leave skin weights uninitialized; face/per-face/LOD index counts are allocated without upstream work caps. These production trees are identical to the fork base.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Independent final review identifies another inherited decoder subcase: nonempty Normal/TexCoord arrays shorter than the Position count are SIMD-read past their ends. Upstream 2a599a405c has the minimal size predicates; add compressed-input/ASan controls before the combined candidate is finalized. High-valence optimizer cost dc0f4b478c remains outside this minimal malformed-array repair.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-023 — Disconnect destroys a world still used in the current frame

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction 9e06c2d585 and bd1a07cc24 / #569. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llappviewer.cpp:6622; lltextureview.cpp:559; llviewerdisplay.cpp:2750; llscenemonitor.cpp:448,453.
- Evidence: [Upstream general audit](research/upstream-general-review-2026-11-13.md), [prioritized upstream backlog](research/upstream-port-review-2026-11-13.md).
- Next action: Owner tests exact First Fixes68914 candidate for ordinary login/world/inventory/fonts and retained media/voice behavior; do not close or push without the required owner decisions.
- Verification: Local1f635d6b75 /26.4.0.68914 Release candidate build,136registered entries,116tool tests,346ARM64files, deep strict signature, embedded source and owned imports pass. Original-mapped world/reset/monitor guards compile in exact candidate; no actual in-world disconnect retest, so owner confirmation remains pending.
- Owner verification: pending
- Push approval: granted

Disconnect deletes LLWorld while later current-frame texture/UI work dereferences it. Related scene-monitor visibility paths also assume an existing singleton. Preserve both retained-world cleanup and monitor guards; no live crash was reproduced.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-024 — Random UUID generation shares unsynchronized state

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction 9ebf45f009 / #596. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llcommon/lluuid.cpp:752–840.
- Evidence: [Upstream general audit](research/upstream-general-review-2026-11-13.md), [prioritized upstream backlog](research/upstream-port-review-2026-11-13.md).
- Next action: Owner tests exact First Fixes68914 candidate for ordinary login/world/inventory/fonts and retained media/voice behavior; do not close or push without the required owner decisions.
- Verification: Local1f635d6b75 /26.4.0.68914 Release candidate build,136registered entries,116tool tests,346ARM64files, deep strict signature, embedded source and owned imports pass. UUID37136assertions/25cases pass, including200000 distinct concurrent IDs and unchanged deterministic asset/name vectors; original produces140214distinct/200000 in retained run.
- Owner verification: pending
- Push approval: granted

getCurrentTime and generate share lazy initialization, time and sequence fields without complete synchronization. Upstream reproduced duplicate IDs under concurrency; no collision benchmark was run on this Mac. Deterministic combine/name-based asset ID behavior must remain unchanged.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-025 — Reliable packet queues can lose acknowledged data and retain stale history

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction #617, #630, #637; f90c8559b3, 670b85ad16, 010f7371f4, d557bdde72. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llpacketring.cpp:62–91; message.cpp:742,1166; LLCircuitData reliable history.
- Evidence: [Upstream general audit](research/upstream-general-review-2026-11-13.md), [prioritized upstream backlog](research/upstream-port-review-2026-11-13.md).
- Next action: Owner tests exact First Fixes68914 candidate for ordinary login/world/inventory/fonts and retained media/voice behavior; do not close or push without the required owner decisions.
- Verification: Local1f635d6b75 /26.4.0.68914 Release candidate build,136registered entries,116tool tests,346ARM64files, deep strict signature, embedded source and owned imports pass. Protocol20599assertions/52cases, circuit49/11 and bitpack36/9 pass; normal full-header dependency rebuild avoids the recorded narrow-link stale-ABI fixture failure.
- Owner verification: pending
- Push approval: granted

A full ring overwrites an older packet after it was acknowledged. Related final upstream corrections handle duplicate ACKs, copy windows, queue impairment and peers that never ping growing reliable history. These are protocol mechanisms, not an attribution to a rendering incident.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-026 — Object cache clear and persistence mishandle retained entry state

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction #602 and #630; 29eff897e3, 4bb9b798e6, fdda86251c, 79057e0958. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llviewerregion.cpp:852,3150–3153; llvocache.cpp:2102; cache tree/set/cursor references.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Local1f635d6b75 /26.4.0.68914 Release candidate build,136registered entries,116tool tests,346ARM64files, deep strict signature, embedded source and owned imports pass. Deferred removal and safe entry release tests pass in registered cache suite; current source includes pending-removal gates and safe reader ordering. Disk/extras pairing remains unimplemented and this record stays investigating.
- Owner verification: pending
- Push approval: granted

Retained subcases: map clearing leaves raw cache entry references/cursors; removal during a cache read needs deferral; cache objects and extras are not paired when no glTF override ever made extras dirty. Upstream tests are not current-Metal runtime verification.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending. PPD-026 covers cache-removal lifetime prerequisites here; disk/extras pairing remains later.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-027 — Escaped floater names bypass resolved-name restrictions

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction 53654b4781, landed through ec47f7a577. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llviewerfloaterreg.cpp openfloater command policy and handler.
- Evidence: [Upstream general audit](research/upstream-general-review-2026-11-13.md), [prioritized upstream backlog](research/upstream-port-review-2026-11-13.md).
- Next action: Owner tests exact First Fixes68914 candidate for ordinary login/world/inventory/fonts and retained media/voice behavior; do not close or push without the required owner decisions.
- Verification: Local1f635d6b75 /26.4.0.68914 Release candidate build,136registered entries,116tool tests,346ARM64files, deep strict signature, embedded source and owned imports pass. Registered shared decoded-name policy regression passes; full app integration builds. No restricted floater opened during testing.
- Owner verification: pending
- Push approval: granted

The restriction check uses escaped params[0] while opening resolves an unescaped name. The two stages can disagree on the allowed floater. No restricted floater was opened during this audit.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-028 — Failed COLRv1 glyph painting retains stale raster surfaces

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction a55fd36959 / #600. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llrender/alfontcolrv1.cpp:164–336.
- Evidence: [Upstream renderer audit](research/upstream-renderer-review-2026-11-13.md), [prioritized upstream backlog](research/upstream-port-review-2026-11-13.md).
- Next action: Owner tests exact First Fixes68914 candidate for ordinary login/world/inventory/fonts and retained media/voice behavior; do not close or push without the required owner decisions.
- Verification: Local1f635d6b75 /26.4.0.68914 Release candidate build,136registered entries,116tool tests,346ARM64files, deep strict signature, embedded source and owned imports pass. Registered real painter failure/recovery15assertions/1case passes; original stale width116 versus expected18 reproduced. No real allocation failure or live font incident claimed.
- Owner verification: pending
- Push approval: granted

The per-thread painter is reused without hb_raster_paint_clear. Failure after painting begins can leave a larger root surface for the next smaller glyph, allowing out-of-bounds compositing. Native font upload does not remove this CPU painter mechanism.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-029 — Valid large texture readback reaches a fatal legacy size guard

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction 45007f232d, landed in #568. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llrender/llimagegl.cpp:2355–2359.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source-only upstream/current comparison; no corrective implementation, runtime reproduction or owner retest in this audit.
- Owner verification: pending
- Push approval: granted

readBackRaw fatally rejects dimensions above 2048 before reaching native readback although accepted images extend to MAX_IMAGE_SIZE (4096). Upstream uses the accepted limit and nonfatal failure for invalid requests.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-030 — URI first-call initialization races and malformed URI handling diverges

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction 584b2cea53, 517072b781, 080980a07d, 3be4f909db / #604. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llcommon/lluri.cpp, unchanged from fork base.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source-only upstream/current comparison; no corrective implementation, runtime reproduction or owner retest in this audit.
- Owner verification: pending
- Push approval: granted

A plain initialized bool guards sorting of a shared string, so concurrent first calls can race. Retained companion subcases: malformed percent escapes lose/change text, an empty query returns an array, and leading-colon scheme handling differs. No current race reproduction was run.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-031 — Shared LLSD date and string contracts retain upstream correctness defects

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction #603, #605, #607–612, #643; 8000ca483b, 1d410c39b9, 15fd4bac40, 2ed88ce337, 58a1362221, 58f471334a. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llcommon/llsdserialize.cpp/.xml.cpp, lldate.cpp, llstring.cpp/.h.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source-only upstream/current comparison; no corrective implementation, runtime reproduction or owner retest in this audit.
- Owner verification: pending
- Push approval: granted

Retained subcases: failed/empty unzip result interpretation, single-line deserialization, empty XML keys and malformed XML/DOCTYPE acceptance, quoted URI/control serialization, nonfinite real formatting, date fraction/timezone round trips, separator/CR removal, and empty-target replace looping. The last is latent: upstream found no current empty-target caller. These are CPU contracts; no exploit or live corruption is asserted.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-032 — Nonfinite quaternion normalization emits invalid transforms

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction 6a0a982f5f / #609. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llmath/llquaternion.h:526–577, unchanged from fork base.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source-only upstream/current comparison; no corrective implementation, runtime reproduction or owner retest in this audit.
- Owner verification: pending
- Push approval: granted

An infinite or overflowing squared magnitude passes the positive threshold, producing zero/NaN components instead of the identity fallback. This is independent of Metal and has not been assigned to a current visual incident.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-033 — New mesh faces inherit placeholder face textures and materials

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction 93c320b7eb / #633; full TE prerequisite 444eb52a3e / #619. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; LLPrimitive::setNumTEs; LLViewerObject::setNumTEs.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source-only upstream/current comparison; no corrective implementation, runtime reproduction or owner retest in this audit.
- Owner verification: pending
- Push approval: granted

When a loaded mesh gains faces, old code copies the last existing face despite earlier wire data describing the new faces. TE, material and glTF override can all be wrong until another update. Full viewer material assignment was not runtime tested.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-034 — Octree child capacity can overflow beneath a full parent

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction 6ff9523744 and df5199689b / #568. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llmath/lloctree.h; LLSpatialGroup::updateInGroup.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source-only upstream/current comparison; no corrective implementation, runtime reproduction or owner retest in this audit.
- Owner verification: pending
- Push approval: granted

A child insertion can ignore its own capacity after a parent fills. Bring legal capacity/minimum-size clamps and retain the intentional exemption for unsplittable tiny nodes. Dense-geometry omission is a source-level risk, not a reproduced scene symptom.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-035 — Asynchronous alpha-mask answers do not rebuild dependent faces

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction bcce2bce3c, d66e1f4852, 830be4b4f4 / #601. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llimagegl.cpp:2290,3020–3110; LLGLTexture; LLViewerFetchedTexture.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source-only upstream/current comparison; no corrective implementation, runtime reproduction or owner retest in this audit.
- Owner verification: pending
- Push approval: granted

Provisional no-mask results may select a blend pass, but a later mask answer does not notify owners to rebuild; reverse transitions and provisional answers that were consumed also need handling. The asynchronous CPU path remains live here.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-036 — PBR bake proxies lose local composites and body-hide ownership

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction #533; ca9043a267, ce67354a60, 6b6f8cd1b2, b71807bc83, 5cc8430be5. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llviewerobject.cpp:5488; llfetchedgltfmaterial.h:70; llvoavatar.cpp:9624; native lldrawpool/pipeline texture consumers.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source-only upstream/current comparison; no corrective implementation, runtime reproduction or owner retest in this audit.
- Owner verification: pending
- Push approval: granted

Fetched-only slots discard dynamic local composites; PBR base-color bake body hiding, refresh and actual-bake shadow invalidation are missing. Retain final base-color-only/HUD exclusions and coalesced body visibility update. No owner appearance incident is assigned.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-037 — Native HDR bloom exposure and probe paths lack complete finite boundaries

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction #595; 07094f82fe, 977e51afe9, 74589add5f, 9c451dc4a2, b929675d9c. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; almetal_builtin.metal local/projector lighting, base output, bloom extract, exposure meter/history, probe downsample/SH.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md), [sky port](work/active/upstream-sky-port.md).
- Next action: Owner tests exact Sky26.4.0.68965 HDR/exposure/lighting behavior. Keep retained HDRI boundary concern qualified by actual finite-green baseline; no nonfinite owner attribution or closure/push inferred.
- Verification: Prior remaining-batch finite boundaries retained. Current real M1 retained HDRI baseline404checks is finite/nonnegative; new bounded sky production GPU cases and full final512781GPUassertions53cases pass. Private bound-removal control saturates to65504 rather than Inf, so source clamp concern is not a reproduced Metal overflow. Owner scene acceptance pending.
- Owner verification: pending
- Push approval: granted

Native radiance clamping is partial: later multiplication/addition can overflow, bloom can multiply infinity by zero, exposure history can retain nonfinite values, and probe/SH writes lack final upstream protections. No flash/NaN incident was reproduced or tied to the owner's reports.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


- 2026-11-13: Reopened narrowly before sky replacement for remaining retained native HDRI G-buffer store: current fragment clamps only at1073741824 then writes dithered R11G11B10F emissive (max65024), allowing overflow before later base-light clamp. Vintage raster intermediates clip HDR layers at1. Source-confirmed storage boundary, no flash/NaN owner attribution. Preserve prior lighting/bloom/exposure/probe corrective proof while new sky composite performs finite floating layer math and final bounded stores.

- 2026-11-13: Actual retained HDRI baseline on M1 Max at finite exposure1/2/1e6 passes404finite/nonnegative/ordinary checks underLLDB, child0. Native RG11 packing did not reproduce nonfinite output; the earlier missing-clamp source concern is not a demonstrated Metal overflow. Keep this passing baseline and qualify attribution. New floating composite still validates explicit finite/range contracts separately.

- 2026-11-13: Coherent sky sourceefc3dc17d0 /68965 builds and full final CPU/UI/GPU/bundle checks pass; ordinary HDRI baseline remains404finite-green. Owner scene acceptance/push remain pending; no actual overflow attributed.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-038 — Placed probe admission drops lighting on stale occlusion state

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction fb1a3f1b71 and f410863587 / #595. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llreflectionmapmanager.cpp:1738,2678,2705.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source-only upstream/current comparison; no corrective implementation, runtime reproduction or owner retest in this audit.
- Owner verification: pending
- Push approval: granted

Placed probes are rejected like automatic probes using occlusion state; off-frustum queries and preview mutation also remain. Native results are completion-gated, so upstream GL flicker latency is not a measured Metal fact.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-039 — Default sky swaps azimuth and altitude arguments

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction 4364fdcdba / #615. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llinventory/llsettingssky.cpp:913–914.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source-only upstream/current comparison; no corrective implementation, runtime reproduction or owner retest in this audit.
- Owner verification: pending
- Push approval: granted

Default sun/moon construction passes altitude first to an azimuth-first API. Upstream deliberately changes visible default sun/moon positions when correcting the call; this is not a lighting-neutral update.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-040 — Texture priority and lifetime retain stale upstream demand

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction #578, #601, #622; a4e519127d, 2ffcc7daf6, 3397e5f6cc, 6811bd93f6, 95df03ec7f. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; LLViewerTexture boosts; llviewertexturelist.cpp:965; llviewertexture.cpp:911,2040,3837; region retirement owners.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source-only upstream/current comparison; no corrective implementation, runtime reproduction or owner retest in this audit.
- Owner verification: pending
- Push approval: granted

Retained subcases: permanent temporary-use boosts, stale face-walk maxima, ineffective media decay, face area counted as visibility, and short/floor fetch results retried without upstream floor handling. Retirement/purge is a larger adaptation candidate, not a measured memory/FPS fix. Preserve current decoder admission.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-041 — Upstream reports additional missing general viewer state corrections

- Status: ready_for_owner
- Confirmation: reported
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction #593, #606, #613, #620, #635, #640, #642, #644–647, #650, #655, #660, #665, #667. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; See the general-review inventory rows and exact commit lists; current per-case runtime evidence is not available.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source-only upstream/current comparison; no corrective implementation, runtime reproduction or owner retest in this audit.
- Owner verification: pending
- Push approval: granted

Retained reported subcases: object roster/orphan/region overrides; inventory IDs/counts/cycle guards; friends/mutes/group replies; notecard counts; SLURL/RLVa parsing; login/maturity/local-teleport/progress; Script Studio analysis/publish; camera steering; run notices and graphics-preset key omissions. These known upstream corrections remain candidates pending focused current-source/behavior tests; optional new features are excluded.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-042 — Coroutine reached locks retain upstream thread-blocking ownership reports

- Status: investigating
- Confirmation: reported
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction 35fc15e182 / #591; Script Studio publish correction #606. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; llcorehttp/_mutex.h uses std::mutex; remaining Script Studio/shared process/table owners are mapped by upstream.
- Evidence: [Upstream general audit](research/upstream-general-review-2026-11-13.md), [prioritized upstream backlog](research/upstream-port-review-2026-11-13.md).
- Next action: Owner verifies Script Studio/server lifetime behavior; retain the wider unproved lock report without asserting every upstream site is a current Mac defect. Larger scheduler/typed-lock project needs an actual native call graph/workload.
- Verification: Scoped immutable built-in-name snapshot lifetime and coroutine-safe external-editor publication fixes integrated and tested; final197CTest +22contained UI +nativeGPU/build/static checks pass. Broad lock-type replacement is not imported: current HTTP critical sections do not suspend, glibc writer preference and Windows job publication are separate. No TSan or actual Mac deadlock reproduction claimed.
- Owner verification: pending
- Push approval: granted

Upstream reports coroutine deadlocks from thread locks and stale process/builtin table publication. The current HTTP mutex still uses the old type; this audit did not independently exercise every coroutine call graph or deadlock. Do not replace native per-thread locks indiscriminately.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.


- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: d7b1bbbf1c imports independent snapshot/coroutine publication fixes. Keep this record scoped/partially addressed; no broad38-file lock refactor or unsupported precise waiter imported.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-043 — Zero mesh normals become NaN during tangent cache preparation

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: bbd5193f99 and d1b8554520 indra/llmath/llvolume.cpp LLVolumeFace::cacheOptimize final normalized-frame conversion; upstream 7a7296abfc adds unit3_or_zero in its replacement implementation.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: First Mikk zero boundary retained. Remaining direct legacy and65535-vertex fallback old controls fail15/17checks in2cases; cea5e68c23 preserves ordinary fast normalization and zeros missing normals. Normal/ASan/UBSan311checks/10cases pass, valid-output fingerprint remains004fd052b5f3a793. Final combined actual math suite passes with existing19geometry expected failures recorded asPPD-053.
- Owner verification: pending
- Push approval: granted

This inherited final normalization path is distinct from PPD-022's byte-array bounds. The short-array repair intentionally selects the original absent-attribute fallback, exposing zero normalization in the retained older tangent implementation. Upstream's replacement uses unit3_or_zero; transplant that semantic boundary without importing the geometry rewrite.

### History

- 2026-11-13: Registered failed fixed-candidate public-API regression before correcting the zero-normal boundary. No visible incident, runtime owner verification or push approval is inferred.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

- 2026-11-13: Remaining allocation review flags the legacy createTangents normalize3fast loop as a possible unhandled zero-normal fallback. Requested an actual production-path probe before broadening the First Mikk-boundary result; do not infer complete zero-normal path coverage from the earlier three-vertex decoder tests.


- 2026-11-13: Reopened after actual frozen51d53a6dee direct createTangents and65535-vertex cacheOptimize fallback controls produce NaN normals (15failedchecks/17assertions/2cases, normal child42 underLLDB; no fault). First201/4 Mikk decoder proof remains valid but did not cover the legacy fallback. Register this confirmed remaining path before repair.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-044 — High-valence mesh cache ordering has an unbounded cost report

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream dc0f4b478c describes quadratic meshoptimizer vertex-cache scoring for repeated high-valence indices. Current retained LLVolumeFace::cacheOptimize still calls the older optimizer without that cost budget.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source/upstream report only; no adversarial runtime timing or causal freeze attribution on the current fork. PPD-022 allocation/index caps do not by themselves bound vertex valence scoring.
- Owner verification: pending
- Push approval: granted

Keep this reported cost mechanism separate from PPD-022 memory safety and PPD-002 owner freezes. No measured M1 CPU stall or speedup is asserted.

### History

- 2026-11-13: Retained after independent final review; the first malformed-array batch does not claim to resolve all algorithmic decode costs.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.


- 2026-11-13: Independent review found the optional budget returned success for invalid no-tangent indices. Production asset callers request tangents and already reject them, but the public cacheOptimize(false) contract needs unconditional structural/index admission before the cost guard. Register this correction before repair; retain zero-work success.

- 2026-11-13: Actual retained cacheOptimize(false) fan control exceeds incidence budget100040000>2^26; baseline reorders, fixed preserves all30000indices (3checks). Fixed public compressed decoder201/4 and unconditional empty/bad-index/nontriangle admission3/1 pass underASan/UBSan. No viewer freeze cause or FPS improvement demonstrated. PPD-048 separately tracks inherited allocation hazards.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-045 — Texture boost test registration links unused viewer-only methods

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Local remaining-candidate b17a8c5aad indra/newview/CMakeLists.txt newview_tests UNITaltextureboost.cpp; ALTextureBoostLevels tests are header-only while the RAII implementation calls real LLViewerTexture hold methods absent from the test target.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Actual Debug/O0/fno-lto boost test compiles and links without dead-strip, boost implementation or viewer support library; nm has no LLViewerTexture hold dependencies. LLDB child exit0,1088assertions/6cases. Canonical production source retained and tests use the inline levels model; combined target still building.
- Owner verification: pending
- Push approval: granted

This is a new local backport-integration issue, not an inherited viewer incident. No published branch or First candidate is affected.

### History

- 2026-11-13: Registered independent review finding before correcting CMake test ownership.

- 2026-11-13: 4e230f49ad removes the unnecessary test implementation unit; independent no-dead-strip Debug probe passes. No Release-only linker inference used.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-046 — New HDR GPU test fixture triggers desktop crash dialogs

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator and HDR test owner
- Discovered: 2026-11-13
- Original reference: New local indra/almetal/tests/almetal_hdr_finite_test.mm Fixture::render in the remaining-HDR worktree; this is test fixture behavior, not a published viewer incident.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner confirms unexpected dialogs cease in the normal workflow; Ignore the old dialog and do not use Reopen. Corrective runner executions already pass with explicit arguments under LLDB; no viewer launch or push.
- Verification: Owner reports two dialogs. Actual09:47:54 PID6697 report shows strlen(nullptr) in Catch::MessageBuilder fromFixture::render.09:48:37 PID6906 shows SIGABRT from Metal render-pipeline descriptor validation; test omitted function constants. Later run manually stopped130; no further runtime until review.
- Owner verification: pending
- Push approval: granted

The owner screenshot names almetal_tests. Both actual crash reports identify the scratch test app, and the source/stack confirms the two fixture defects. These are unexpected failures, not successful red controls. Raw diagnostics and private screenshot stay outside Git; source/log summaries are retained.

### History

- 2026-11-13: Owner reports two pop-ups. Runner paused, both reports independently inspected, implementation work continues without launching further GPU tests. New fixture fixes must pass review and corrective execution before readiness is claimed.

- 2026-11-13: Null-safe diagnostics and production-equivalent function-constant specialization reviewed. Final isolated LLDB HDR1230/2 and existing243759/19 pass with actual child exit0/MetalAPIvalidation; earlier faults/interruption are not counted as passes. No viewer launched.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-047 — Temporary boost leaves legacy NO_DELETE state after demand expires

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: LLViewerTexture::updateBoost sets NO_DELETE for an effective boost; LLGLTexture::setActive refuses NO_DELETE. Scoped level release returns NONE without restoring the state. Inherited state contract predates the native boost import.
- Evidence: [Remaining upstream batch](work/active/upstream-remaining-batch.md).
- Next action: Keep nonblocking legacy state invariant visible; native residency/downscale/delete consumers do not consult NO_DELETE. Reconcile explicit pins and any future state reader before cleanup; do not invent a current native leak or perform a speculative unpin.
- Verification: Independent source review confirms the state remains; native lazy flush and downscale do not consult it, so no memory leak, performance loss or visible incident is demonstrated.
- Owner verification: pending
- Push approval: granted

Effective scoped priorities do release correctly. Keep this legacy state invariant separate from PPD-040 priority correctness and unmeasured native resource/accounting consequences.

### History

- 2026-11-13: Retained from independent material/texture review; no speculative unpin or accounting redesign applied.

- 2026-11-13: Full source reader audit finds only state/profile bookkeeping, avatar debug logging and setActive refusal. Actual native pressure/LOD/downscale/reference deletion uses other owners/flags; scoped boost priorities release correctly. Remains unmodified and no measured functional consequence is claimed.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-048 — Retained mesh optimizer allocation failures can lose buffers

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator and geometry reviewer
- Discovered: 2026-11-13
- Original reference: Retained LLVolumeFace::cacheOptimize Mikk re-weld/index shuffle; upstream dc0f4b478c also repairs allocation failure handling in its replacement implementation. The local incidence-budget backport covers cost only.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md), [geometry port](work/active/upstream-geometry-port.md).
- Next action: Owner tests exact Alchemy Metal Geometry26.4.0.68952 candidate for this record’s affected behavior; retain local scope until owner verification and explicit push approval.
- Verification: Combined source3ee7786e67 preserves strong face constructors/copy/remap/normals/tangents, staged simplifier/LOD and optional avatar ordering; actual allocation controls and targeted sanitizer checks pass. Expanded geometry evidence is in upstream-geometry-port.md; parent68936 history retained. Final combined candidate checks pass; owner scene acceptance pending.
- Owner verification: pending
- Push approval: granted

Keep this allocation/lifetime issue distinct from PPD-022 decoder bounds and PPD-044 optimizer cost. No complete dc0f4b478c allocation-safety equivalence is claimed.

### History

- 2026-11-13: Registered independent source findings before repair investigation; full candidate build running at2a5af4fdac. Existing hazards predate the new budget guard.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


- 2026-11-13: Reopened for owner-authorized full geometry port before adaptation. Final upstream generateNormals/remap/createTangents contain uncaught allocation exceptions, unchecked aligned results or destructive partial publication. Source-confirmed incoming-path hazards, not a new viewer OOM incident. Preserve current68836 safety semantics under the replacement; stage required output and retain valid face onfailure.

- 2026-11-13: Core source review adds retained/default face constructor and copy/assignment allocation boundaries to the reopened scope before repair: null extents dereference, fatal copy bad_alloc and destructive unchecked copies would defeat uploader model transactions. Adapt nonfatal constructors/strong assignment with real allocation controls; no physical OOM attribution.

- 2026-11-13: Incoming simplifier compact/makeEmpty and previewLOD generation add destructive allocation/publication boundaries: target resized before all allocations, existing LOD models/buffers cleared before replacements ready. Uploader stages complete replacement faces/model lists before swap; preregistered before adaptation.

- 2026-11-13: Integrated into local combined geometry source3ee7786e67; focused corrective verification above. Full canonical candidate validation is still running; owner verification and push approval remain pending. See linked geometry work note for exact scope and controls.

- 2026-11-13: Retained preceding-batch verification checkpoint: Required/optional allocation failure controls pass293/8 normal+ASan/UBSan and217/4 private aligned/null/new/fallback; exact old source fails both allocation cases. Ordinary fingerprint unchanged004fd052b5f3a793. Native LLFace4calls/preview1call and intersection now propagate false before raw tangent reads; final build/math/native checks pass. No physical OOM scene test.

- 2026-11-13: Final combined sourceb16fa27efa /Alchemy Metal Geometry26.4.0.68952: doctor/configure/build,192contained CPU commands (13existing case skips),22UI processes,269602nativeGPU assertions/35cases,116tools,346ARM64/signature/source/import/features and strict3540file audit0blockers/0errors pass. Corrective evidence above remains scoped; no viewer launch, owner acceptance, measured FPS or push. Status ready_for_owner, not closed.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-049 — Scratch Script Studio test links mismatched noticebar implementation

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex general viewer test owner
- Discovered: 2026-11-13
- Original reference: Isolated scratch viewer links omitted matching noticebar/datapacker objects; canonical implementation units are present and rebuilt. This is a scratch linkage/setup fault, not a viewer incident.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Matching noticebar fixes zero line counts. Original SIGBUS is cache unpackUUID dispatch into libc++abi type-info vtable; saved stale-datapacker control reproduces under LLDB and matching unmodified current lldatapacker.cpp alone removes it. Corrected UI518/38 and cache3998/19 pass with actual child exit0. No product workaround or viewer attribution.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered unintended scratch fixture failure. Direct failing launches stopped; subsequent tests require LLDB containment and actual child exit/state verification. Do not treat a setup failure as product evidence.

- 2026-11-13: Original09:58:08 OS report and contained stale-datapacker stack/link control preserved privately. Initial noticebar/SIGBUS association was provisional; the cache fault is independently traced to stale datapacker ABI, with corrective isolated-source proof. Canonical full build recompiles both.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-050 — Imported noticebar test assumes a fixed font-dependent height

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex general viewer test owner
- Discovered: 2026-11-13
- Original reference: Imported Script Studio noticebar regression expects28pixels for one line; actual native Mac font metrics produce30pixels.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Independent test-only23cea92614 derives one-line expected height from actual text pixels/skin margins with28minimum; line count/wrap/clamp/tooltip/shrink assertions retained. Correctly linked contained UI subset passes518assertions/38cases, childexit0.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered platform-dependent test expectation before correction; root canonical target building. PPD-049 retains prior scratch linkage/SIGBUS history separately; no product attribution inferred.

- 2026-11-13: Portable test correction tested in isolated worktree; production layout unchanged. Integrator will cherry-pick after current frozen-source build completes.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-051 — Whole assets share texture HTTP admission

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex general viewer owner
- Discovered: 2026-11-13
- Original reference: Upstream05c6ab9032 separates complete sound/animation/gesture asset requests from texture policy with AP_ASSET and AssetFetchConcurrency. Current fork routes these requests through AP_TEXTURE.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Actual shared queue ownership confirmed by source. No measured audible delay, throughput or frame-rate consequence on M1 Max.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered independent applicable upstream admission change before repair; no renderer or asynchronous framework dependency.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-052 — Sculpt texture copies lose aspect and finer same-level data

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex sculpt/native texture owner
- Discovered: 2026-11-13
- Original reference: Upstream85bf118da5/04d9015143 and45007f232d chosen-copy correction. Current saveRaw can resize512x8 sculpt pixels to128x128, rebuild admission only follows discard decrease, and CPU-only completion lacks postCreateTexture volume notification.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source-confirmed current paths; no owner scene comparison or measured decode performance claim.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered producer/consumer findings before implementation. Full geometry and GL asynchronous readback architecture are unnecessary prerequisites for this correction.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-053 — Retained geometry suite has19 expected-failure contracts

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex geometry integration
- Discovered: 2026-11-13
- Original reference: Existing indra/llmath/tests/llvolume_test.cpp explicitly marks19 generation contracts !shouldfail; the file is unchanged fromb04d9bf6ef through this remaining batch. Corresponding normal/stitching/cap/hollow/flexible/degenerate repairs are in the larger upstream geometry stack.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [geometry audit](research/upstream-geometry-review-2026-11-13.md).
- Next action: Owner tests exact Alchemy Metal Geometry26.4.0.68952 candidate for this record’s affected behavior; retain local scope until owner verification and explicit push approval.
- Verification: Old0c68 exact archive with only expected-failure tags removed:44cases25pass/19fail,4274assertions/23fail, normal child42. Final geometry suite now uses ordinary tests; mapped former19 pass and final core ASan+UBSan334840assertions/138cases (one optional corpus skip), child0. Combined canonical candidate checks pass; owner acceptance pending.
- Owner verification: pending
- Push approval: granted

Contracts include cylindrical/mirrored/creased normals, S/T seams, caps, zero-area tangent contribution, normal length, hollow walls, flexible partial rebuilds and placeholder normals. This is a grouped register of existing expected-failure evidence, separate from repaired decoder bounds/cost/allocation/legacy zero/sculpt copy paths. It is not a newly introduced regression or full-stack port claim.

### History

- 2026-11-13: Registered known retained geometry contracts after observing actual isolated and final combined suite output. Current batch preserves all existing test expectations and records the larger project separately.

- 2026-11-13: Owner explicitly requests geometry next and sky afterward. Begin coherent final upstream geometry port from0c68a66e21, preserving all prior native/backport fixes. No owner acceptance or push approval is inferred; retain original expected-failure history and remove annotations only with actual corrective evidence.

- 2026-11-13: Real0c68 production math library linked with its existing test file after removing only !shouldfail annotations:44cases,25pass/19fail;4274assertions,4251pass/23fail. LLDB child42/state10, no fault. Frozenbaseline executable and exact compile/link receipts in artifacts/metal/upstream-geometry-20261113/baseline/. This establishes active failing contracts before replacement.

- 2026-11-13: Integrated into local combined geometry source3ee7786e67; focused corrective verification above. Full canonical candidate validation is still running; owner verification and push approval remain pending. See linked geometry work note for exact scope and controls.

- 2026-11-13: Retained preceding-batch verification checkpoint: Finalaf5c263988 llvolume_test returns0 under its existing !shouldfail policy:34cases,15pass/19fail as expected;3963assertions,3940pass/23fail as expected. Separate new safety tests311/10 pass. No new unexpected failure or owner visual incident.

- 2026-11-13: Final combined sourceb16fa27efa /Alchemy Metal Geometry26.4.0.68952: doctor/configure/build,192contained CPU commands (13existing case skips),22UI processes,269602nativeGPU assertions/35cases,116tools,346ARM64/signature/source/import/features and strict3540file audit0blockers/0errors pass. Corrective evidence above remains scoped; no viewer launch, owner acceptance, measured FPS or push. Status ready_for_owner, not closed.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-054 — Uploader Restore Normals loses original model faces

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex uploader integration
- Discovered: 2026-11-13
- Original reference: Current llmodelpreview.cpp generateNormals rewrites shared mBaseModel/high-LOD objects before capturing mModelFacesCopy. Restore then overwrites base originals with generated high-LOD normals; ordinal saved-face lists can target replacement models. Upstream ea8e64184b/0bf5fe1633 capture model identities before rewrite and discard obsolete copies.
- Evidence: [Geometry port](work/active/upstream-geometry-port.md), [geometry audit](research/upstream-geometry-review-2026-11-13.md).
- Next action: Owner tests exact Alchemy Metal Geometry26.4.0.68952 candidate for this record’s affected behavior; retain local scope until owner verification and explicit push approval.
- Verification: Actual old preview methods21failed checks versus corrected66assertions/3cases. Registered simplifier/model1244assertions/25cases and transactional LOD controls96/5 pass. Native preview blocks remain byte-identical to base. No live uploader/owner acceptance.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered actual shared-model ordering/lifetime defect before repair in the owner-authorized geometry batch. No remote upload or account operation.

- 2026-11-13: Integrated into local combined geometry source3ee7786e67; focused corrective verification above. Full canonical candidate validation is still running; owner verification and push approval remain pending. See linked geometry work note for exact scope and controls.

- 2026-11-13: Final combined sourceb16fa27efa /Alchemy Metal Geometry26.4.0.68952: doctor/configure/build,192contained CPU commands (13existing case skips),22UI processes,269602nativeGPU assertions/35cases,116tools,346ARM64/signature/source/import/features and strict3540file audit0blockers/0errors pass. Corrective evidence above remains scoped; no viewer launch, owner acceptance, measured FPS or push. Status ready_for_owner, not closed.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-055 — Planar mapping, tangent frames and alpha picking disagree

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex geometry/native consumer integration
- Discovered: 2026-11-13
- Original reference: Current LLFace planarProjection picks axes per vertex normal; planar normal-map frames still use ordinary UV tangents. LLVOVolume::lineSegmentIntersect sends volume-space p/n to surfaceToTexture, which expects agent-space hits. Upstream120e1fe63d/b003070a1d/bff2be1727 and final planar tails align mapping, frames and opaque hit tests.
- Evidence: [Geometry port](work/active/upstream-geometry-port.md), [geometry audit](research/upstream-geometry-review-2026-11-13.md).
- Next action: Owner tests exact Alchemy Metal Geometry26.4.0.68952 candidate for this record’s affected behavior; retain local scope until owner verification and explicit push approval.
- Verification: Shared strict-planar arithmetic and native tangent consumers integrated; CPU plane/UV/frame/picking contracts and real GLTF primitive192assertions/5cases pass. Generated volume/native G-buffer851assertions/1case and combined native269602/35 pass; owner scene parity unverified.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered source mapping and hit-space mismatch before native consumer changes. Final planar math requires strict arithmetic compilation and tests, including per-vertex fallback.

- 2026-11-13: Integrated into local combined geometry source3ee7786e67; focused corrective verification above. Full canonical candidate validation is still running; owner verification and push approval remain pending. See linked geometry work note for exact scope and controls.

- 2026-11-13: Final combined sourceb16fa27efa /Alchemy Metal Geometry26.4.0.68952: doctor/configure/build,192contained CPU commands (13existing case skips),22UI processes,269602nativeGPU assertions/35cases,116tools,346ARM64/signature/source/import/features and strict3540file audit0blockers/0errors pass. Corrective evidence above remains scoped; no viewer launch, owner acceptance, measured FPS or push. Status ready_for_owner, not closed.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-056 — Refilled shared geometry can exceed an individual face slot

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native geometry integration
- Discovered: 2026-11-13
- Original reference: Current LLFace::getGeometryVolume validates writes against the whole vertex/index buffer, without requiring the current volume face to fit mGeomCount/mIndicesCount. New geometry sculpt triangle omission can change shared face counts at the same level. Upstreamd1da0360ee rejects refills whose individual slots no longer fit and requests group relayout.
- Evidence: [Geometry port](work/active/upstream-geometry-port.md).
- Next action: Owner tests exact Alchemy Metal Geometry26.4.0.68952 candidate for this record’s affected behavior; retain local scope until owner verification and explicit push approval.
- Verification: Exact local slot admission precedes any writes in native LLFace, while failed fills propagate to group relayout instead of registration. Native producer compiles; exact guard with real adjacent LLVB slots/SIMD index helper old19failed assertions versus fixed87/1pass. Rejected fills preserve adjacent bytes/content stamp. No complete LLFace execution or owner scene attribution.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered ownership-boundary guard required by coherent new geometry before LLFace changes. Keep distinct from whole-buffer allocation failurePPD-048 and old decoder boundsPPD-022.

- 2026-11-13: Integrated into local combined geometry source3ee7786e67; focused corrective verification above. Full canonical candidate validation is still running; owner verification and push approval remain pending. See linked geometry work note for exact scope and controls.

- 2026-11-13: Final combined sourceb16fa27efa /Alchemy Metal Geometry26.4.0.68952: doctor/configure/build,192contained CPU commands (13existing case skips),22UI processes,269602nativeGPU assertions/35cases,116tools,346ARM64/signature/source/import/features and strict3540file audit0blockers/0errors pass. Corrective evidence above remains scoped; no viewer launch, owner acceptance, measured FPS or push. Status ready_for_owner, not closed.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-057 — Incoming typed scratch allocation can wrap its byte count

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex geometry prerequisite integration
- Discovered: 2026-11-13
- Original reference: Pinned upstream408 ALScratchArena::Scope::allocate<T>(count) multiplies count*sizeof(T) without checking overflow; raw allocate guards later rounding only. A wrapped typed count can produce an undersized allocation.
- Evidence: [Geometry port](work/active/upstream-geometry-port.md).
- Next action: Owner tests exact Alchemy Metal Geometry26.4.0.68952 candidate for this record’s affected behavior; retain local scope until owner verification and explicit push approval.
- Verification: Actual scratch/compact62assertions/14cases pass, including typed overflow before multiplication and no allocator/counter mutation. Actual custom registry hostile remap/allocator controls pass; ARM64 package built with pinned provenance.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered typed scratch pre-multiply overflow before adapting new geometry prerequisite. Keep separate from allocator lifetime/face publicationPPD-048.

- 2026-11-13: Integrated into local combined geometry source3ee7786e67; focused corrective verification above. Full canonical candidate validation is still running; owner verification and push approval remain pending. See linked geometry work note for exact scope and controls.

- 2026-11-13: Final combined sourceb16fa27efa /Alchemy Metal Geometry26.4.0.68952: doctor/configure/build,192contained CPU commands (13existing case skips),22UI processes,269602nativeGPU assertions/35cases,116tools,346ARM64/signature/source/import/features and strict3540file audit0blockers/0errors pass. Corrective evidence above remains scoped; no viewer launch, owner acceptance, measured FPS or push. Status ready_for_owner, not closed.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-058 — Failed geometry fills can still publish draw batches and lose retries

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native geometry integration
- Discovered: 2026-11-13
- Original reference: LLVolumeGeometryManager::genDrawInfo warns after getGeometryVolume returnsfalse but still registers the face; rebuildGeom clears dirty flags afterward. LLPipeline::rebuildPriorityGroups unconditionally clears queue flags/removes every group. New fallible tangent/planar allocation and slot admission make this boundary required.
- Evidence: [Geometry port](work/active/upstream-geometry-port.md).
- Next action: Owner tests exact Alchemy Metal Geometry26.4.0.68952 candidate for this record’s affected behavior; retain local scope until owner verification and explicit push approval.
- Verification: Exact queue control old7/20failed, fixed20/20pass, including retry-vector OOM. Exact in-place rebuildMesh control old14/66failed, fixed66/66 across3scenarios: all merged ranges/face admissions withdrawn, child transform restored, retry preserved. Child42/0 normally underLLDB; no physical OOM/owner incident.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered ignored failure publication and retry loss before integrating fallible geometry. Preserve normal successful scheduling and all native render consumers.

- 2026-11-13: Exact old/current rebuildPriorityGroups function bodies compiled with a small queue-collaborator harness. Existing source fails failure-retention/allocation-fallback checks (normal child42); repaired normal/failing/recovery/dead/allocation-fallback cases pass20checks (child0), both underLLDB without fault. Root LLFace/LLVOVolume/pipeline/preview TUs also compile against fresh core headers; full app still required.


- 2026-11-13: Final review extended the same admission defect to existing in-place rebuildMesh: failed fill dirtied/queued the group but retained draw-map ranges and flushed a partially modified buffer. Before correction, record the need to withdraw all group draw admission on failure and rebuild complete geometry on retry. No owner incident or observed physical OOM attributed.

- 2026-11-13: Integrated into local combined geometry source3ee7786e67; focused corrective verification above. Full canonical candidate validation is still running; owner verification and push approval remain pending. See linked geometry work note for exact scope and controls.

- 2026-11-13: Final combined sourceb16fa27efa /Alchemy Metal Geometry26.4.0.68952: doctor/configure/build,192contained CPU commands (13existing case skips),22UI processes,269602nativeGPU assertions/35cases,116tools,346ARM64/signature/source/import/features and strict3540file audit0blockers/0errors pass. Corrective evidence above remains scoped; no viewer launch, owner acceptance, measured FPS or push. Status ready_for_owner, not closed.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-059 — glTF primitive attributes can be shorter than positions

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex geometry loader integration
- Discovered: 2026-11-13
- Original reference: Current newview/gltf/primitive.cpp Primitive::prep reads attributes at each POSITION index without matching count checks or requiring JOINTS_0/WEIGHTS_0 together; old triangle unroll also subtracts2 from short strip/fan counts. Final upstream2bb25308c2 validates attribute counts/skin pairing before geometry work;7a7296abfc replaces Mikk dependency with compatible tangent API.
- Evidence: [Geometry port](work/active/upstream-geometry-port.md).
- Next action: Owner tests exact Alchemy Metal Geometry26.4.0.68952 candidate for this record’s affected behavior; retain local scope until owner verification and explicit push approval.
- Verification: Actual primitive.cpp final192assertions/5cases pass; old bounded control49failed/70checks in4cases. Short attribute/skin/topology rejection, generated/authored tangents, strips/fans/unindexed picking and points/lines covered. Native tangent arrays retained for fork consumers.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered source-confirmed attribute/skin and short-topology admission before loader adaptation. Keep direct primitive tangent behavior where this fork still stores/reads it; exclude unrelated sky or backend changes.

- 2026-11-13: Core inspection adds serialized-vs-runtime topology divergence and old fan corner: unroll writes TRIANGLES runtime data but preserves original mMode; octree branches on mMode and its first fan triangle repeats vertex0 (index-2). Before repair, authorize actual runtime-mode topology and valid strip/fan minimums, preserving serialized fields and authored-tangent paths. Inspect unindexed admission too; no picking incident attributed.

- 2026-11-13: Integrated into local combined geometry source3ee7786e67; focused corrective verification above. Full canonical candidate validation is still running; owner verification and push approval remain pending. See linked geometry work note for exact scope and controls.

- 2026-11-13: Final combined sourceb16fa27efa /Alchemy Metal Geometry26.4.0.68952: doctor/configure/build,192contained CPU commands (13existing case skips),22UI processes,269602nativeGPU assertions/35cases,116tools,346ARM64/signature/source/import/features and strict3540file audit0blockers/0errors pass. Corrective evidence above remains scoped; no viewer launch, owner acceptance, measured FPS or push. Status ready_for_owner, not closed.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-060 — Avatar mesh loader ignores its detail-UV header flag

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex avatar geometry integration
- Discovered: 2026-11-13
- Original reference: LLPolyMeshSharedData::loadMesh reads HasDetailTexCoords but never assigns mHasDetailTexCoords. A valid flagged binary contains an extra UV stream that the loader can skip before interpreting the following face fields. The issue also exists in pinned upstream.
- Evidence: [Geometry port](work/active/upstream-geometry-port.md).
- Next action: Owner tests exact Alchemy Metal Geometry26.4.0.68952 candidate for this record’s affected behavior; retain local scope until owner verification and explicit push approval.
- Verification: Valid real LLM fixture safely returns incorrect data in old loader (child42), corrected flag consumes exact detail UVs and LOD inherits pointer/flag. Final avatar suite19555assertions/6cases pass. All29bundled LLM headers have detailflag0; no bundled visual incident asserted.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered pre-existing ignored loader flag before bounded fixture/repair. Do not infer it affects ordinary bundled meshes without inspecting their flags.

- 2026-11-13: Integrated into local combined geometry source3ee7786e67; focused corrective verification above. Full canonical candidate validation is still running; owner verification and push approval remain pending. See linked geometry work note for exact scope and controls.

- 2026-11-13: Final combined sourceb16fa27efa /Alchemy Metal Geometry26.4.0.68952: doctor/configure/build,192contained CPU commands (13existing case skips),22UI processes,269602nativeGPU assertions/35cases,116tools,346ARM64/signature/source/import/features and strict3540file audit0blockers/0errors pass. Corrective evidence above remains scoped; no viewer launch, owner acceptance, measured FPS or push. Status ready_for_owner, not closed.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-061 — Avatar morph instances initialize binormals from normals

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex avatar geometry integration
- Discovered: 2026-11-13
- Original reference: Existing LLPolyMesh::initializeForMorph copies mBaseNormals into both instance binormal arrays despite retaining distinct mBaseBinormals. Same path exists in pinned upstream408.
- Evidence: [Geometry port](work/active/upstream-geometry-port.md).
- Next action: Owner tests exact Alchemy Metal Geometry26.4.0.68952 candidate for this record’s affected behavior; retain local scope until owner verification and explicit push approval.
- Verification: Real LLM fixture distinct normal/binormal detects old instance initialization (child42); corrected instance/scaled binormals match base and actual morph consumers. Final avatar19555assertions/6cases pass, including LOD aliases. Owner shading acceptance pending.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered real initialized-data mismatch before narrow geometry repair. Keep separate from cache triangle order and avoid an unproved avatar lighting/FPS claim.

- 2026-11-13: Integrated into local combined geometry source3ee7786e67; focused corrective verification above. Full canonical candidate validation is still running; owner verification and push approval remain pending. See linked geometry work note for exact scope and controls.

- 2026-11-13: Final combined sourceb16fa27efa /Alchemy Metal Geometry26.4.0.68952: doctor/configure/build,192contained CPU commands (13existing case skips),22UI processes,269602nativeGPU assertions/35cases,116tools,346ARM64/signature/source/import/features and strict3540file audit0blockers/0errors pass. Corrective evidence above remains scoped; no viewer launch, owner acceptance, measured FPS or push. Status ready_for_owner, not closed.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-062 — Transactional tangent import loses flexible-frame buffer reuse

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex geometry core integration
- Discovered: 2026-11-13
- Original reference: New local core d81f565dff createTangents adapter allocates a fresh aligned output even when mTangentsStale has an existing buffer. Final upstream reuses that buffer for flexible partial frames.
- Evidence: [Geometry port](work/active/upstream-geometry-port.md).
- Next action: Owner tests exact Alchemy Metal Geometry26.4.0.68952 candidate for this record’s affected behavior; retain local scope until owner verification and explicit push approval.
- Verification: Oldd81 stale path7failed/9checks replacing6owners; final arena stage/copyback9/9checks passes with warm owner reuse. Final core ASan+UBSan334840assertions/138cases child0, one optional corpus skip. No measured frame-rate claim.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered new local integration issue before correction. Preserve both transactional failure safetyPPD-048 and upstream intended buffer reuse rather than trading one for the other.

- 2026-11-13: Integrated into local combined geometry source3ee7786e67; focused corrective verification above. Full canonical candidate validation is still running; owner verification and push approval remain pending. See linked geometry work note for exact scope and controls.

- 2026-11-13: Final combined sourceb16fa27efa /Alchemy Metal Geometry26.4.0.68952: doctor/configure/build,192contained CPU commands (13existing case skips),22UI processes,269602nativeGPU assertions/35cases,116tools,346ARM64/signature/source/import/features and strict3540file audit0blockers/0errors pass. Corrective evidence above remains scoped; no viewer launch, owner acceptance, measured FPS or push. Status ready_for_owner, not closed.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-063 — Selected material buffer clone ignores native allocation failure

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native geometry integration
- Discovered: 2026-11-13
- Original reference: LLVertexBuffer::clone returnsvoid and ignores allocateBuffer false; native allocation updates requested counts even if CPU storage is null. LLFace selection recursion can pass count admission then dereference absent stores. Matching counts also bypass layout-change offset recalculation. U32 index sources expose logical half-counts while allocateBuffer sizes U16 units; old clone underallocates those stores.
- Evidence: [Geometry port](work/active/upstream-geometry-port.md).
- Next action: Owner tests exact Alchemy Metal Geometry26.4.0.68952 candidate for this record’s affected behavior; retain local scope until owner verification and explicit push approval.
- Verification: Real native clone5265assertions/4cases pass with Metal validation and five indexed pixel draws; safe old-method control8failed assertions before unsafe writes. Verbatim clone allocation collaborator old5/17failed, fixed17/17pass. Layout offsets, U32 physical/logical capacity, null CPU rejection and warm reuse covered; no physical OOM claim.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered ignored result and null storage boundary before narrow clone/selection correction. Existing slot-count and recursive-result guards remain necessary but insufficient on their own.

- 2026-11-13: Integrated into local combined geometry source3ee7786e67; focused corrective verification above. Full canonical candidate validation is still running; owner verification and push approval remain pending. See linked geometry work note for exact scope and controls.

- 2026-11-13: Final combined sourceb16fa27efa /Alchemy Metal Geometry26.4.0.68952: doctor/configure/build,192contained CPU commands (13existing case skips),22UI processes,269602nativeGPU assertions/35cases,116tools,346ARM64/signature/source/import/features and strict3540file audit0blockers/0errors pass. Corrective evidence above remains scoped; no viewer launch, owner acceptance, measured FPS or push. Status ready_for_owner, not closed.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-064 — Partition buffer reuse compares vertex count with index count

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native geometry integration
- Discovered: 2026-11-13
- Original reference: LLSpatialPartition::rebuildGeom compares getNumVerts()!=index_count instead of getNumIndices. Grass uses8vertices/12indices per blade, so a dirty grass group always reallocates even at matching dimensions; terrain also inherits the base path.
- Evidence: [Geometry port](work/active/upstream-geometry-port.md).
- Next action: Owner tests exact Alchemy Metal Geometry26.4.0.68952 candidate for this record’s affected behavior; retain local scope until owner verification and explicit push approval.
- Verification: Exact production dimension predicate control old1/4failed, fixed4/4pass for unchanged8vertex/12index grass-sized slots, changed dimensions and null buffer. Normal native publication retained; no measured timing or frame-rate claim.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered source-confirmed dimensional comparison before repair; distinguish dirty rebuilds from every rendered frame.

- 2026-11-13: Integrated into local combined geometry source3ee7786e67; focused corrective verification above. Full canonical candidate validation is still running; owner verification and push approval remain pending. See linked geometry work note for exact scope and controls.

- 2026-11-13: Final combined sourceb16fa27efa /Alchemy Metal Geometry26.4.0.68952: doctor/configure/build,192contained CPU commands (13existing case skips),22UI processes,269602nativeGPU assertions/35cases,116tools,346ARM64/signature/source/import/features and strict3540file audit0blockers/0errors pass. Corrective evidence above remains scoped; no viewer launch, owner acceptance, measured FPS or push. Status ready_for_owner, not closed.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-065 — Base grass and terrain allocation failure clears geometry retry state

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native geometry integration
- Discovered: 2026-11-13
- Original reference: Retained base LLSpatialPartition::rebuildGeom clears GEOM_DIRTY after allocateBuffer(false), nulling its buffer/map but losing the failed group from priority retries. Grass and terrain inherit this path; distinct from PPD-058 volume fill rejection and PPD-064 dimension reuse.
- Evidence: [Geometry port](work/active/upstream-geometry-port.md).
- Next action: Owner tests exact Alchemy Metal Geometry26.4.0.68952 candidate for this record’s affected behavior; retain local scope until owner verification and explicit push approval.
- Verification: Exact old3ee function control12pass/7failed checks, finalb16 function67assertions/2cases pass underLLDB child42/0 normally. Failed required allocation keeps dirty state, retry succeeds; warm8v12reuse, empty/dead/clean paths and owner lifetimes covered. No physical OOM/GPU/owner scene attribution.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered inherited retry-state loss before narrow correction. Current source3ee full build is allowed to finish, then the final source will be rebuilt and validated; intermediate app is not owner-ready.

- 2026-11-13: Final combined sourceb16fa27efa /Alchemy Metal Geometry26.4.0.68952: doctor/configure/build,192contained CPU commands (13existing case skips),22UI processes,269602nativeGPU assertions/35cases,116tools,346ARM64/signature/source/import/features and strict3540file audit0blockers/0errors pass. Corrective evidence above remains scoped; no viewer launch, owner acceptance, measured FPS or push. Status ready_for_owner, not closed.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-066 — Retained raster sky lacks coherent upstream ALSky composition

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex sky integration
- Discovered: 2026-11-13
- Original reference: Pinned408fd6c014 #584 replaces old sky objects/pools with ALSky ray layers, packed/cullable stars, GPU environment cube; ba9da2e58b/95362a1041/d464042e3d depend on that new ownership. Native fork still uses retained raster LLVOSky/LLVOWLSky/LLDrawPoolWLSky.
- Evidence: [Sky port](work/active/upstream-sky-port.md), [renderer audit](research/upstream-renderer-review-2026-11-13.md).
- Next action: Owner tests exact Alchemy Metal Sky26.4.0.68965 for this record’s affected behavior; retain local scope until owner verification/agreement and explicit push approval.
- Verification: Native ALSky owner/layers/stars/cube and actual viewer replay integrated through efc3dc17d0. Exact68965 build,206CPU commands/24UI commands/512781GPUassertions53cases,346ARM64/signature/source/import/features checks pass. Candidate-only login/location/10live controls and visible world/sky observed; owner day/night/layer acceptance pending.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Owner explicitly authorizes sky after geometry. Registered before replacing sky state/pass semantics; retain geometry candidate and all prior source/test fixes. Separate in-world/owner acceptance from synthetic tests and static/source evidence.

- 2026-11-13: Final sourceefc3dc17d0 /Sky68965 validation passes; corrective evidence above and linked sky note retain all failed controls/builds/faults. Ready for owner, not closed; no push approval.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-067 — New ray dome samples inactive optics maps

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native sky renderer
- Discovered: 2026-11-13
- Original reference: New local native27a948adc9 modern dome reuses sky_color, which samples rainbow/halo before multiplying zero strengths. Final408 windlightDomeAt conditionally reads maps only with active moisture/ice.
- Evidence: [Sky port](work/active/upstream-sky-port.md).
- Next action: Owner tests exact Alchemy Metal Sky26.4.0.68965 for this record’s affected behavior; retain local scope until owner verification/agreement and explicit push approval.
- Verification: Actual new native production shader control failed1/47checks at zero optic strengths before conditional reads; repaired shader and modern242875assertions13cases pass. Ordinary UNORM optics assets cannot encode NaN; no owner incident attributed.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered new native sky port no-read contract gap before changing frozen renderer source. Keep passing retained HDRI404checks separate and preserve all red/green outcomes.

- 2026-11-13: Final sourceefc3dc17d0 /Sky68965 validation passes; corrective evidence above and linked sky note retain all failed controls/builds/faults. Ready for owner, not closed; no push approval.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-068 — New sky capture packs disabled body quads at degenerate directions

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex sky CPU integration
- Discovered: 2026-11-13
- Original reference: First uncommitted native ALSky capture adaptation calculates sun/moon/bloom rayQuad payloads unconditionally, unlike final408 layer guards. An exactly nadir body has degenerate corners and rayQuad divides by zero even while that body's layer is disabled.
- Evidence: [Sky port](work/active/upstream-sky-port.md).
- Next action: Owner tests exact Alchemy Metal Sky26.4.0.68965 for this record’s affected behavior; retain local scope until owner verification/agreement and explicit push approval.
- Verification: Old incoming unguarded disabled-body capture fails64/206checks normally child42. Layer-gated packing passes206assertions4cases; canonical combined CPU/GPU suite and final68965 build pass. No upstream or owner incident attributed.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered before correcting the initial adaptation; preserve final upstream layer admission rather than changing pure body math or removing native finite checks.

- 2026-11-13: Final sourceefc3dc17d0 /Sky68965 validation passes; corrective evidence above and linked sky note retain all failed controls/builds/faults. Ready for owner, not closed; no push approval.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-069 — Initial native replay treats an empty star field as frame failure

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex sky integration
- Discovered: 2026-11-13
- Original reference: New root native replay calls renderModernSkyStars whenever the view enables STARS, but that API rejects draw_count0/null buffer. Final ALSky renderStars simply emits no draw until asynchronous stars land, at RenderStarCount0, or after optional buffer failure.
- Evidence: [Sky port](work/active/upstream-sky-port.md).
- Next action: Owner tests exact Alchemy Metal Sky26.4.0.68965 for this record’s affected behavior; retain local scope until owner verification/agreement and explicit push approval.
- Verification: Root skips draw_count0 while retaining STARS policy and selected attachment preopaque black clear. Actual native empty-star/selected-attachment readback and final modern242875assertions13cases pass; other optional layers and layer0 dome remain admitted.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered incoming integration completeness error before adding draw-count admission. Do not drop STARS policy or clear after opaque draws; no stars must still leave the far background defined.

- 2026-11-13: Final sourceefc3dc17d0 /Sky68965 validation passes; corrective evidence above and linked sky note retain all failed controls/builds/faults. Ready for owner, not closed; no push approval.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-070 — Fixed exposure metering still reads obsolete sky flags

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex sky integration
- Discovered: 2026-11-13
- Original reference: New dynamic exposure scene supplies completed post-water depth/provenance, but retained fixed/classic renderExposureMeter wrapper at pipeline.cpp9135 still classifies old G-buffer flags.
- Evidence: [Sky port](work/active/upstream-sky-port.md).
- Next action: Owner tests exact Alchemy Metal Sky26.4.0.68965 for this record’s affected behavior; retain local scope until owner verification/agreement and explicit push approval.
- Verification: Both production exposure routes use completed post-water target depth and water stencil. Actual GPU sky/geometry/provenance meter cases and retained HDR-finite/world exposure suite pass in final512781assertions53cases. Owner exposure behavior pending.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered before correction during coherent native sky integration. Keep failed controls, exact source identity and owner acceptance separate.

- 2026-11-13: Final sourceefc3dc17d0 /Sky68965 validation passes; corrective evidence above and linked sky note retain all failed controls/builds/faults. Ready for owner, not closed; no push approval.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-071 — New sky CPU and Metal constant blocks have different layouts

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native sky renderer
- Discovered: 2026-11-13
- Original reference: New native27a948adc9 public ModernSkyConstants is816bytes; embedded private MSL dome/cloud structs include extra raster fields, producing1008bytes. First actual modern composite draw fails Metal API validation.
- Evidence: [Sky port](work/active/upstream-sky-port.md).
- Next action: Owner tests exact Alchemy Metal Sky26.4.0.68965 for this record’s affected behavior; retain local scope until owner verification/agreement and explicit push approval.
- Verification: Initial27a draw actually fault-stopped underLLDB PID84613 at Metal validation: CPU816/MSL1008. Backtrace saved and child killed without resume. Repaired production payload sizes/offsets pass actual canonical GPU layout19assertions1case before draws; final512781GPUassertions53cases pass/APIvalidation. Final viewer logged in underLLDB without a fault observed at handoff.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered before correction during coherent native sky integration. Keep failed controls, exact source identity and owner acceptance separate.

- 2026-11-13: Final sourceefc3dc17d0 /Sky68965 validation passes; corrective evidence above and linked sky note retain all failed controls/builds/faults. Ready for owner, not closed; no push approval.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-072 — New sky replay omits the dome when optional layer mask is zero

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex sky integration
- Discovered: 2026-11-13
- Original reference: New root base-light replay gates compositeModernSky on config[0]!=0. Zero optional layers is a valid WindLight dome-only sky; the dome shader renders independently of layer bits.
- Evidence: [Sky port](work/active/upstream-sky-port.md).
- Next action: Owner tests exact Alchemy Metal Sky26.4.0.68965 for this record’s affected behavior; retain local scope until owner verification/agreement and explicit push approval.
- Verification: Distinct captured modern_sky_drawn admits optional-mask0 dome and excludes hidden/cutout views. Actual dome/layer GPU readback, CPU view controls and final68965 build pass. Owner visual acceptance pending.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered before correction during coherent native sky integration. Keep failed controls, exact source identity and owner acceptance separate.

- 2026-11-13: Final sourceefc3dc17d0 /Sky68965 validation passes; corrective evidence above and linked sky note retain all failed controls/builds/faults. Ready for owner, not closed; no push approval.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-073 — New environment cube capture advances sky layer state

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex sky CPU integration
- Discovered: 2026-11-13
- Original reference: Root metalSkyEnvironment calls full captureNativeView(bake,1,1) before current beginView, reusing prior layer bits. Full capture ticks meteor/aurora clocks and builds/culls stars, while final408 environment bake binds only the dome.
- Evidence: [Sky port](work/active/upstream-sky-port.md).
- Next action: Owner tests exact Alchemy Metal Sky26.4.0.68965 for this record’s affected behavior; retain local scope until owner verification/agreement and explicit push approval.
- Verification: New const dome-only capture shares packing without beginView/layer/star/clock side effects. Actual old-method control fails9/228checks; corrected236assertions3cases and actual cube/provider controls77assertions6cases pass. Canonical suite passes.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered before extracting the correct dome-only ownership boundary; retain .25s key/retry and successful independent-command publication semantics.

- 2026-11-13: Final sourceefc3dc17d0 /Sky68965 validation passes; corrective evidence above and linked sky note retain all failed controls/builds/faults. Ready for owner, not closed; no push approval.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-074 — New depth metering changes the retained direct-kernel test ABI

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native sky renderer
- Discovered: 2026-11-13
- Original reference: New extended almetal_world_exposure_meter expects32constant bytes plus depth/water textures, but retained actual HDR finite fixture directly calls the original16byte/three-texture kernel. Source review catches this before executing that fixture.
- Evidence: [Sky port](work/active/upstream-sky-port.md).
- Next action: Owner tests exact Alchemy Metal Sky26.4.0.68965 for this record’s affected behavior; retain local scope until owner verification/agreement and explicit push approval.
- Verification: Legacy exposure kernel retains16B payload/textures0..2; distinct scene depth kernel uses32B/depth3/water4. Final retained HDR-finite and world exposure cases pass alongside modern completed-depth/water metering. The prior fixture ABI mismatch was registered before repair.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered before preserving the legacy test ABI and adding the explicit production depth kernel; do not silently update old controls to hide a changed contract.

- 2026-11-13: Final sourceefc3dc17d0 /Sky68965 validation passes; corrective evidence above and linked sky note retain all failed controls/builds/faults. Ready for owner, not closed; no push approval.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-075 — New water convergence applies the EEP dome to HDRI skies

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex sky integration
- Discovered: 2026-11-13
- Original reference: Root hazeOnWater lacks final408 ALSky::useHDRISky exclusion. HDRI is already shaded colour; convergence against the EEP dome changes water incorrectly in HDRI and partial-HDRI views.
- Evidence: [Sky port](work/active/upstream-sky-port.md).
- Next action: Owner tests exact Alchemy Metal Sky26.4.0.68965 for this record’s affected behavior; retain local scope until owner verification/agreement and explicit push approval.
- Verification: Final water gate excludes HDRI in addition to probe/impostor/underwater/cutout/sky-hidden cases. Actual provider/view controls77assertions6cases and canonical native water/dome GPU checks pass; owner environment transition acceptance pending.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered incoming integration boundary before repair; preserve native command lifetime and final view policy rather than removing effects.

- 2026-11-13: Final sourceefc3dc17d0 /Sky68965 validation passes; corrective evidence above and linked sky note retain all failed controls/builds/faults. Ready for owner, not closed; no push approval.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-076 — New neutral sky cube can publish from a cancelled view batch

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex sky integration
- Discovered: 2026-11-13
- Original reference: Root provider guards dome bake against active command batches but neutral creation can borrow one and cache its output before that enclosing batch commits. A retry after failed early allocation can then publish an uninitialized cube if the view batch cancels.
- Evidence: [Sky port](work/active/upstream-sky-port.md).
- Next action: Owner tests exact Alchemy Metal Sky26.4.0.68965 for this record’s affected behavior; retain local scope until owner verification/agreement and explicit push approval.
- Verification: Provider returns prior/null before neutral creation or bake when a view batch is active; independent completion alone publishes cube/key. Actual provider cancel/retry/device/bake-failure controls77assertions6cases plus native cube face/mip/neutral GPU readback pass.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered incoming integration boundary before repair; preserve native command lifetime and final view policy rather than removing effects.

- 2026-11-13: Final sourceefc3dc17d0 /Sky68965 validation passes; corrective evidence above and linked sky note retain all failed controls/builds/faults. Ready for owner, not closed; no push approval.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-077 — New sky header depends on a viewer-only Metal include path

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex sky integration
- Discovered: 2026-11-13
- Original reference: First combined972cda4a11 build compiles the viewer but newview_tests lens/finalization translation units include pipeline.h -> alsky.h without the viewer's private almetal include directory. Bare almetalworld.h cannot be found.
- Evidence: [Sky port](work/active/upstream-sky-port.md).
- Next action: Owner tests exact Alchemy Metal Sky26.4.0.68965 for this record’s affected behavior; retain local scope until owner verification/agreement and explicit push approval.
- Verification: Initial972 full build exited1 on newview lens/finalization test include visibility. Relative public dependency fixed8b77; finalefc doctor/configure/build all0 and both affected canonical contained tests pass. Initial failed receipts retained.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered before correcting include visibility; no broad Metal link/dependency changes or test removals.

- 2026-11-13: Final sourceefc3dc17d0 /Sky68965 validation passes; corrective evidence above and linked sky note retain all failed controls/builds/faults. Ready for owner, not closed; no push approval.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-078 — Haze source contract forbids the new water-dome ray rotation

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex sky integration
- Discovered: 2026-11-13
- Original reference: Final408 class3/deferred/hazeF.glsl rotates normalized water dome rays while passing geometry positions unrotated to the atmosphere helper. The imported native source-contract fixture forbids any eye_to_sky occurrence in the haze pass.
- Evidence: [Sky port](work/active/upstream-sky-port.md).
- Next action: Owner tests exact Alchemy Metal Sky26.4.0.68965 for this record’s affected behavior; retain local scope until owner verification/agreement and explicit push approval.
- Verification: Canonical8b77 alfarplane test fails1/364checks normally child42. Correctedefc exact-function geometry/raw-position and separate normalized dome-ray assertions pass full actual alrender1146808assertions145cases; canonical final206CPU commands pass with13existing skips.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered before correcting the fixture. Preserve failing log and distinguish an obsolete lexical assertion from an unproved rendering defect. Initial suspected overbroad substring was disproved by exact-function inspection; rotation occurs in the correct new water branch.

- 2026-11-13: Local efc3dc17d0 preserves the geometry one-rotation and water dome ray contracts separately. Full actual alrender scratch-linked suite passes1,146,808assertions/145cases, LLDB child0/APIvalidation. Combined exact-source rebuild and final app verification underway.

- 2026-11-13: Final sourceefc3dc17d0 /Sky68965 validation passes; corrective evidence above and linked sky note retain all failed controls/builds/faults. Ready for owner, not closed; no push approval.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-079 — Visual-QA diagnostic booleans lack a typed session declaration

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex sky validation
- Discovered: 2026-11-13
- Original reference: Native settings.xml declares HDR/emissive/reflections/dynamic exposure Boolean. The external visual-QA skill handles LLSD integer0/1 as Boolean only when its immutable session XML includes that control type; CLI-only diagnostic overrides lack that declaration.
- Evidence: [Sky port](work/active/upstream-sky-port.md).
- Next action: Owner tests exact Alchemy Metal Sky26.4.0.68965 for this record’s affected behavior; retain local scope until owner verification/agreement and explicit push approval.
- Verification: First68965 run reached correct world position but gate blocked true versus LLSD1 without declared Boolean types. Derived local profile includes actual source type declarations; same binary PID97692 passes unchanged observe/verify/gate10live controls and global position at13:03UTC. Original profile/installed skill/blocked receipts unchanged.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered before correcting task-local QA profile. Do not alter observations, falsify Boolean types, mutate immutable run receipts or modify installed skill code.

- 2026-11-13: Derived local profile preserves saved graphics and adds actual source Boolean declarations. Same binary PID97692 relaunch passes unchanged observe/verify/gate with10live controls and verified world-global destination. First PID97037 was deliberately quit normally0/state10, not a crash; reopened viewer left for owner testing.

- 2026-11-13: Final sourceefc3dc17d0 /Sky68965 validation passes; corrective evidence above and linked sky note retain all failed controls/builds/faults. Ready for owner, not closed; no push approval.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-080 — Owner HDRI preview appears washed out with no visible cloud detail

- Status: investigating
- Confirmation: reported
- Owner: Codex sky integration
- Discovered: 2026-11-13
- Original reference: Final408 ALSky HDRI uses RenderHDRIExposure in EV, multiplying sampled linear EXR by2^EV. Native ALSky capture and modern sky branch carry the same explicit exposure factor. Original same-file/runtime comparison has not run.
- Evidence: [HDRI preview exposure investigation](work/active/hdri-preview-exposure.md).
- Next action: Retain EV0/-2/-3 owner evidence and compare exact-file original/native output if appearance remains suspect; do not treat HDRI EV adjustment as resolution of the broader pre-existing whole-scene report PPD-081. Leave owner viewer untouched.
- Verification: Owner split.5 screenshot atEV0 is washed out; screenshots at-2 show some clouds and-3 show clear blue/cloud detail. Control responds and HDRI cloud content is present. Native exposure/sample/linear/common-post source matches408, with no extra HDRI gain identified. No same-file original runtime comparison or numerical clipping/nonfinite reproduction; no closure.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered owner report before repair. Preserve raw supplied images locally; separate reported appearance from PPD-037's finite-storage investigation. No renderer or UI settings changed by the agent; source audit and bounded diagnosis underway.

- 2026-11-13: Owner supplies Midday settings: global exposure1.0,ACES tone map/mix.70,auto exposure enabled; HDR/emissive/SSR enabled. EV-2 and-3 recover progressively more HDRI clouds. Source-only original/native audit finds no HDRI-specific gain or gamma mismatch. Owner then states overall scene and sky were overexposed before this port; PPD-081 separately tracks that broader concern.

- 2026-11-13: Owner-authorized verified original67611 launched/PID10889 after candidate normalquit0; owner supplies14:31 same-claimed Daylight/Lev1233/HDRI.5/-3 reference image. Both supplied images visibly retain left cloud detail and pale right background; original foreground appears somewhat lighter in available areas. Different framing/occlusion plus AA/profile mismatch block controlled QA, so no parity, brightness-regression attribution or closure. [Comparison record](work/active/exposure-reference-comparison.md) retains exact images/identities/failed preflight. Reference left running; no shader/default/display change or push.

- 2026-11-13: Owner disputes the asserted framing difference. Exact compared images confirmed as native14:08:20/reference14:31:18, shown side by side unaltered. Camera/framing cause was not measured and remains unknown; prior camera explanation was premature, not a validated reason for the visible differences.

- 2026-11-13: After viewing the exact pair side by side, owner confirms matching framing and identifies the native exposure dialog as the visible obstruction. Framing objection withdrawn; do not discount the owner comparison on that basis. No numerical exposure delta, cause or repair is established by this correction.

## PPD-081 — Owner reports pre-existing overall scene and sky overexposure

- Status: investigating
- Confirmation: reported
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Owner says sky and overall scene looked overexposed before the current upstream sky changes. Exact first affected build and a matched original-viewer runtime are unknown. Inspect native common exposure/bloom/tonemap/display against original1eb44bb and final408 before choosing a correction.
- Evidence: [Overall exposure investigation](work/active/overall-exposure-investigation.md), [HDRI preview report](work/active/hdri-preview-exposure.md).
- Next action: Compare original/native at the same verified effective environment, camera, diagnostic baseline and asset state, including actual adaptation/output values. Source audits found no cause; preserve current owner viewer and do not treat HDRI EV adjustment as global repair.
- Verification: Owner report plus images at global exposure1.0,ACESmix.70,auto exposure enabled; HDR/emissive enabled. Brighter-than-expected appearance is reported, but numerical clipping, double exposure, gamma mismatch and causal attribution to any specific commit are unproved. PPD-080 confirms the independent HDRI EV control responds.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered before any repair. Broaden investigation to pre-existing common scene output, keep HDRI-specific record/history and avoid attributing it to latest sky port. Read-only parallel audits assigned; no viewer manipulation/source change yet.

- 2026-11-13: Source-only meter/post/ordinary-sky/presentation audits find no duplicated exposure, gamma encode or added gain versus original/upstream; no cause diagnosed. XDR/TrueTone/auto-brightness context retained. Owner14:05 image confirms diagnostic Exposure1/ACESmix1/center meter/EV[-2,+2]/comp0/speeds3,1/trims10,95/sky.25/gradeOFF. Source defaults unchanged; no fresh original/native parity comparison, shader fix or scene acceptance. Separate transition concern retained as PPD-082.

- 2026-11-13: Owner-authorized verified original67611 launched/PID10889 after candidate normalquit0; owner supplies14:31 same-claimed Daylight/Lev1233/HDRI.5/-3 reference image. Both supplied images visibly retain left cloud detail and pale right background; original foreground appears somewhat lighter in available areas. Different framing/occlusion plus AA/profile mismatch block controlled QA, so no parity, brightness-regression attribution or closure. [Comparison record](work/active/exposure-reference-comparison.md) retains exact images/identities/failed preflight. Reference left running; no shader/default/display change or push.

- 2026-11-13: Owner disputes the asserted framing difference. Exact compared images confirmed as native14:08:20/reference14:31:18, shown side by side unaltered. Camera/framing cause was not measured and remains unknown; prior camera explanation was premature, not a validated reason for the visible differences.

- 2026-11-13: After viewing the exact pair side by side, owner confirms matching framing and identifies the native exposure dialog as the visible obstruction. Framing objection withdrawn; do not discount the owner comparison on that basis. No numerical exposure delta, cause or repair is established by this correction.

## PPD-082 — Classic-sky blend policy can stop exposure before history admission changes

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: LLSettingsSky blend copies target canAutoAdjust while lerping probe ambiance. Original1eb/final408 generateExposure always writes fixedEV0 when classic disables metering. Native pipeline7758–65 stops history updates on classic, but8104–08/8213–19 still requires fresh history until effective ambiance reaches0.
- Evidence: [Overall exposure investigation](work/active/overall-exposure-investigation.md).
- Next action: Reproduce a PBR-to-legacy blend through actual native history/publication control and verify the original fixedEV0 invariant before selecting a repair. Keep separate from unproved steady overexposure PPD-081.
- Verification: Read-only source confirms differing policy boundaries can coexist during interpolation. Steady classic skies correctly bypass history through no_post. No runtime transition reproduction, frame-withholding measurement, owner symptom attribution or correction yet.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered source-permitted transition concern from the PPD081 audit. No source/UI edits or failing GPU execution; preserve current owner viewer and retain concern for a bounded original/native transition check.

## PPD-083 — Owner reports a tint in neutral-looking scene surfaces

- Status: verified
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Owner compares Metal14:08:20 with original14:31:18 and reports a hue in the Metal image's lower grey pavement/road areas while the original looks more neutral. Original1eb reference67611 and native sourceefc3dc17d0 are available; exact corresponding surface samples and colour-profile normalization are unmeasured.
- Evidence: [Neutral surface colour investigation](work/active/neutral-surface-colour-investigation.md), [comparison](work/active/exposure-reference-comparison.md).
- Next action: Retain owner-passed Sky68980 colour/probe repair and evidence. Keep unrelated exposure, preview and asynchronous-SH concerns separate; the owner explicitly approved publication of the full committed upstream-port series on2026-11-13.
- Verification: Clean same-coordinate asphalt/concrete patches reproducibly show darker and relatively warmer encoded RGB in Metal. Example asphalt means67.72/63.34/60.17 versus original74.51/71.67/70.67; identical embedded ICC bytes and same UI grey31/31/31. This confirms supplied-image differences, not a cause or full live-pipeline parity. In-viewer Metal export is unprofiled and retained separately; no repair.
- Owner verification: passed
- Push approval: granted

### History

- 2026-11-13: Registered new colour-specific report separately from broader brightness PPD-081. Reference PID10889 had already exited normally0/state10 when owner requested switching back. Exact unchangedSky68965 restarted PID12146, explicit same destination and frozen agreed settings, HDRI.5/EV-3; owner will reload EXR and capture. No rendering code change or agent scene capture.

- 2026-11-13: Owner supplies clean Metal14:48:40 OS screenshot and native in-viewer Snapshot14:49:55. Clean OS pair now both3456x2234; provided original reference14:31 retained. Pixel/profile analysis in progress; no cause, colour correction or closure yet.

- 2026-11-13: Read-only PNG/profile and five same-material sample measurements reproduce the image-level difference; no transforms/edits used. Raw controls/coordinates retained. Source/runtime material/lighting/post/presentation boundary still needs isolation; newer upstream algorithm changes and mismatched inputs remain candidates, not established causes. No blanket gamma/colour adjustment or owner closure.


- 2026-11-13: Profile-normalized Metal viewer-export/OS comparison reproduces five material patches within2/255 in98.94–99.44%of pixels; tint precedes WindowServer. Actual edge-measured offset0,+66; no warp/reframe. Source lighting/haze audit finds no confirmed cause. Owner may log out; candidate12146 normal-exits0/state10 at15:12:34 without fault. No shader/global-colour fix or closure.


- 2026-11-13: Controlled old-folded-skirt versus final sphere HDRI ray model yields relatively warmer truncated-SH upward diffuse for one controlled EXR. Loaded ownerfile/world/actual SH still unmeasured; physical production test pending. This is intentional upstream geometry, not a proved native regression or083cause. Mip candidate084 independently dismissed by originalGL parity.

- 2026-11-13: Owner reports new image looks fixed and replaces first after-image with clean16:28:21 capture (earlier black square was an attachedHUD). ExactSky68980/PID41725 is logged in atSpareTime105/21/24; installed QA verifies executable/global position/46livecontrols. Actual log16:22:06 records defaultHDR R11G11B10F radiance/SH publication queued, with no matched format/SH/GPU errors in bounded review. Clean3456x2234 image and reference/prior share identicalICC. Same five unchanged concrete/asphalt patches: normalizedRGB-mean RMSE8.91064→.51602/255 (94.21%lower); all15channelmean residuals<1/255 (max.967714). Owner assessment plus image/runtime/physical evidence verifies this reported colour failure in the tested setup. No pixel/all-scene/performance parity or push approval inferred; preview087 and async-error088 remain separate.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

- 2026-11-13: Owner-approved full upstream series published by ordinary fast-forward37989d9d66 on origin/codex/apple-silicon-metal; exactremote verified independently. No implementation change in publication receipt.


## PPD-084 — Native sRGB mip generation may differ from original raw-byte policy

- Status: dismissed
- Confirmation: reproduced
- Owner: Codex texture/resource integration
- Discovered: 2026-11-13
- Original reference: Original1eb/final408 LLImageGL clears sampler and uses SKIP_SRGB_DECODE during mip generation for a documented raw-byte pyramid. Native LLImageGL::generateMetalMipmaps calls ALMetalTexture::generateMipmaps on the original sRGB texture; it does not select its raw linear view.
- Evidence: [Neutral surface colour investigation](work/active/neutral-surface-colour-investigation.md).
- Next action: Preserve verified original-driver mip behaviour; retain the bounded native regression test. Continue PPD-083 elsewhere; no raw-view producer change.
- Verification: Actual AppleM1Max OpenGL4.1Metal90.5 immutable sRGB storage + SKIP_DECODE + sampler0 + glGenerateMipmap returns188/128/192/128, exactly matching production native LLImageGL. Explicit GL DECODE also188; raw RGBA8/raw Metal view controls128. Old comment does not describe actual driver behaviour. Both native/raw-policy and GL/raw-policy assertions intentionally fail without a fault; no producer repair applied.
- Resolution: Proposed parity defect disproved by actual original/native GPU control. Changing native mip generation to raw128 would diverge from the reference; preserve188. This dismisses the candidate, not owner colour report PPD-083.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered semantic candidate during owner-authorized PPD083 diagnosis before any corrective edit. Source path evidence is distinct from tested GPU behaviour and the owner's tint report.


- 2026-11-13: PPD084 native physical baseline underLLDB reproduces mip1red188 versus raw-view/linear-storage128 for actual LLImageGL coloured2x2 input; originalGL physical control remains pending. Native child14528 exits42/state10 with one intended failing policy assertion, no fault; authored sampling control31assertions passes. This does not establish083 causality.


- 2026-11-13: External originalGL physical control on sameM1Max returns188 matching native, with allGLerrors0. LLDB14772 exits42/state10 only because the proposed raw128 expectation fails, no fault. Original EXTdecode support/storage present. Candidategap claim withdrawn; no producer edit. A source comment or method name is insufficient evidence for changing established behaviour.


- 2026-11-13: Verified original-driver oracle retained in local test3b4ddbfdbe; canonical physical70assertions/2cases pass underLLDB26821 normal0/state10 with MetalValidation. No production change. Original/native mip parity remains matched; dismissed candidate stays in history.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-085 — Upstream PBR sun contribution cap remains at the old boundary

- Status: open
- Confirmation: source_confirmed
- Owner: Codex shader integration
- Discovered: 2026-11-13
- Original reference: Original1eb clamps unscaled nl*(diffuse+specular) at65504 then multiplies sunlight*3*visibility. Upstream07094f82fe moves complete sun contribution inside clamp;977e51afe9 sets punctual512 and completed-base32752. Current native retains original sun ordering/cap while already applying completed-base32752.
- Evidence: [PBR contribution-cap source finding](work/active/pbr-sun-contribution-cap.md).
- Next action: Reproduce extreme sun/ambient ratio with the actual native base renderer before selecting a bounded512 contribution repair. Keep separate from PPD-083 ordinary pavement: original1eb and native share old ordering.
- Verification: Exact original/final/native source and hashes retained. Native builtin13223–37 still uses65504 helper; final complete-base32752 protects storage but does not limit sunlight share and can scale other lighting when exceeded. No actual owner brightness or tint attribution, physical regression or repair.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered during PPD083 bounded surface audit. No shader edit. Source defect concerns unported upstream high-intensity behaviour; it does not establish the cause of owner ordinary-surface images.


## PPD-086 — HDR ordinary probe publication rejects its producer's packed-float format

- Status: verified
- Confirmation: reproduced
- Owner: Codex native probe integration
- Discovered: 2026-11-13
- Original reference: Original ordinary HDR radiance uses packed R11/G11/B10 storage. Native updateReflectionProbeFace now produces rg11b10_float and filterReflectionProbeRadiance preserves it; LLReflectionMapManager still allocates/expects rgba16_float in bootstrap, publication and reset preservation.
- Evidence: [HDR probe publication format investigation](work/active/hdr-probe-publication-format.md), [owner colour investigation](work/active/neutral-surface-colour-investigation.md).
- Next action: Retain owner-passed Sky68980 colour/probe repair and evidence. Keep unrelated exposure, preview and asynchronous-SH concerns separate; the owner explicitly approved publication of the full committed upstream-port series on2026-11-13.
- Verification: Exact owner-session copy failure physically reproduced before repair. Shared packedR11 producer/publication/reset contract plus world/preview admission repaired; scoped rollback and realtime restart preserve retryability. Canonical b81a4645e0 fullbuild, doctor, 346ARM64/signature/owned-import/source gates pass; new3903/3 plus related4504/6 GPU assertions/cases pass underLLDB with API/GPUvalidation, realchild0. Scheduled actual producer failure/retry and SH contribution measured. Owner clean-image colour confirmation passed; full live reset coverage remains incomplete and separate088 async-error edge stays open.
- Owner verification: passed
- Push approval: granted

### History

- 2026-11-13: Registered immediately after isolating actual owner-session copy failures, before repair. Independent source and physical audits assigned. PPD083 attribution remains candidate, though missing environment contribution is consistent with darker/warmer image; no global exposure/colour adjustment selected.



- 2026-11-13: Physical baseline27930 reproduces exact HDRcopyfailure at producer→sharedmanagerformat boundary (291/292assertions, child42/state10, nofault); SDR and actualSHpass. Selected sharedR11 policy b5dfa1b571 also admits R11 in actual world validator. Failure rollback guard/realtime restart and actual draw-consumer proof under test; no scene acceptance.


- 2026-11-13: Canonical b81a4645e0 produces signed Sky26.4.0.68980/8118f71cf6af9216a3f7c8532f4e9b22239d3e1ab38ca4a08cc847d86a6db779. Fullbuild/doctor/verify0,346ARM64,deepstrictsignature,embeddedSHA,ownedviewer/CEF/VLCimports0forbidden; strict3540files0blockers0errors. IntegratedLLDB32384 passes3903assertions/3cases;41119 passes4504/6, bothchild0/state10 with API/GPUvalidation. Actual SH ambient increases measured pixels; no full-viewer relaunch/scene parity/FPS claim. New immutable QAconfig dryrun0; old run receipts preserved. Owner logged out; viewer left closed and fix local.

- 2026-11-13: Owner reports new image looks fixed and replaces first after-image with clean16:28:21 capture (earlier black square was an attachedHUD). ExactSky68980/PID41725 is logged in atSpareTime105/21/24; installed QA verifies executable/global position/46livecontrols. Actual log16:22:06 records defaultHDR R11G11B10F radiance/SH publication queued, with no matched format/SH/GPU errors in bounded review. Clean3456x2234 image and reference/prior share identicalICC. Same five unchanged concrete/asphalt patches: normalizedRGB-mean RMSE8.91064→.51602/255 (94.21%lower); all15channelmean residuals<1/255 (max.967714). Owner assessment plus image/runtime/physical evidence verifies this reported colour failure in the tested setup. No pixel/all-scene/performance parity or push approval inferred; preview087 and async-error088 remain separate.


- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

- 2026-11-13: Owner-approved full upstream series published by ordinary fast-forward37989d9d66 on origin/codex/apple-silicon-metal; exactremote verified independently. No implementation change in publication receipt.


## PPD-087 — Material preview rejects the ordinary non-HDR default radiance format

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex material-preview integration
- Discovered: 2026-11-13
- Original reference: Ordinary native default probes use rgba8_unorm when HDR is disabled, and fillMetalDefaultProbe passes the default cube directly to the material preview. ALMetalPBRPreviewRenderer currently accepts only rgba16_float. Original default-cube sampling permits RGB8 and packed HDR radiance.
- Evidence: [HDR probe format consumer review](work/active/hdr-probe-publication-format.md).
- Next action: Owner tests material/mesh previews in HDR and SDR on signed Sky68980. Retain actual-format regression checks; do not infer scene-tint closure from preview success.
- Verification: Actual preview baseline4format rejections reproduced underLLDB; minimal47d7991dea whitelist acceptsRGBA16/R11/RGBA8. Fixed2202assertions/2cases pass, including actualSHreadback, identical blue radiance/diffuse output, alpha invariance, invalidsRGB/type rejection. Canonical integrated3903/3 and related4504/6 pass with fullbuild/signature/ARM64/source/import gates. Owner verification remains pending.
- Owner verification: pending
- Push approval: granted

### History

- 2026-11-13: Registered before preview validator repair. Exclusive preview-file/test ownership assigned separately; root retains manager/world-validator/format-contract integration.


- 2026-11-13: Actual preview baseline28271 underLLDB reproduces four R11/SDR render-null failures (1166/1170assertions pass, child42/state10, nofault). RGBA16, actualSHprojection/readback and invalid-format/type rejection pass. Exclusive minimal validator repair now under physical test.



- 2026-11-13: Owner explicitly confirms resolution and authorizes committing/pushing the ENTIRE completed upstream-port series, including general/remaining fixes, geometry, sky and associated colour/probe repairs. Approval covers the implementation/tests/records already committed throughaf3558f689 plus this approval record; it does not close unverified/partial reports or authorize future unrelated repairs.

## PPD-088 — Errored asynchronous probe SH can strand a scheduled radiance update

- Status: open
- Confirmation: source_confirmed
- Owner: Codex native probe integration
- Discovered: 2026-11-13
- Original reference: Native SH projection has an explicit asynchronous completion ticket. collectNativeMetalProbe consumes an errored ticket and clears pending identity, while the scheduled logical update can already be in radiance phase. Original GL projection does not have this native ticket state.
- Evidence: [Probe publication review and limits](work/active/hdr-probe-publication-format.md).
- Next action: Inject actual SH command failure or equivalent typed completion failure and verify scheduled restart from irradiance with matching generation, retaining the preceding published pair. Keep separate from fixed normal format/copy path PPD-086.
- Verification: Independent source review finds nativeSHPublishedThisPass remains true after ticket error; scheduled radiance face5 then lacks ready/queued SH and can retry indefinitely. Realtime PPD086 restart handles its lane. No errored GPU ticket was observed in owner logs or intentionally generated on hardware here; no attribution to owner tint and no correction selected.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered during repair review before any asynchronous-error change. PPD086 correction is specifically format publication, consumer admission and normal failed-copy retry, not a claim to solve all GPU execution failures.

## PPD-089 — Immutable attachment records cost more for single-use poses

- Status: open
- Confirmation: reproduced
- Owner: Codex Apple-first experiment integration
- Discovered: 2026-11-13
- Original reference: Native fork baseline `18e576cec0a05923632188ec8d7b2224657ffbfa` uses raw per-frame attachment palettes with byte-based arena deduplication; upstream `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` owns the same packed producer contract. The immutable record is new on the isolated experiment branch.
- Evidence: [Goal-one implementation, complete-cost comparison and limits](research/apple-shadow-records-goal1-results.md), [predeclared experiment](work/active/apple-shadow-records-goal1.md).
- Next action: Assess single-use overhead with the combined68995 candidate. Owner authorization to continue all goals supersedes the earlier per-goal stop. G2–G5 removes avoidable per-output ownership work but does not establish a single-use live-scene gain; preserve this measured regression.
- Verification: At `e21fe9c21832e5af88971ae0cb4f1ec2d892c36e`, the headless one-view/32-draw/32-distinct-pose/32-joint fixture costs p50 0.038375ms raw versus 0.039916ms record (+4.02%, about1.54us); p95 0.042876ms versus0.044751ms. All ten counterbalanced blocks are slower (0.83–5.79%). Costs include copy/finite validation, binding, commit and same-frame ownership cleanup. Multi-view/repeated cases improve; this is a synthetic mechanism regression, not an observed viewer regression.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered immediately after the single declared 3,600-sample comparison. Overall predeclared mechanism gate passes, while the adverse result remains open and explicit. Native correctness passes25cases/53,601assertions; no viewer launch, owner acceptance or performance waiver. Experiment stops for owner assessment before goal two.

- 2026-11-14: Combined continuation source d5ae05f2 is built locally as68994. SourceV3 small cases still show no consistent complete-cost improvement; the8192-family mechanism gain does not close the original single-use cost record. Owner combined-app assessment remains pending.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-090 — G1 terminates shortly after busy-scene arrival

- Status: verified
- Confirmation: reproduced
- Owner: Codex experiment crash integration
- Discovered: 2026-11-13
- Original reference: Native baseline `18e576cec0a05923632188ec8d7b2224657ffbfa` retains the Alpha6 image worker/codec path. Inspected upstream `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` image job and WorkQueue223–244 preserve processRequest/finishRequest and forward unexpected exceptions to mainloop; ordinary codec failures follow the existing failed-image result. New shadow records have not been established as the cause.
- Evidence: [Owner report, existing log evidence and investigation](work/active/g1-arrival-crash.md).
- Next action: Retain the owner-confirmed exception-ABI repair in every continuation build; preserve the vendor boundary contract. No push permission is inferred. Broader architecture/scene performance remains separately unverified.
- Verification: Diagnostic68989 retry identifies std::runtime_error primary5channel decode bypassing base catch. Joint real voice/codec baseline fails3cases/SIGABRT; scoped PPD-091 boundary repair passes3cases/34assertions and total50cases/1,311assertions. Corrected fef/68991 builds/verifies346ARM64/signatures/native/features/source and zero private standard RTTI. Staged copy7,841entries matches. Abort mechanism repaired; exact original input/error reason unavailable and owner confirms crash gone on the repaired G1 source; combined architecture runtime acceptance remains separate.
- Owner verification: passed
- Push approval: not_granted

### History

- 2026-11-13: Registered before repair. The delivered G1 runtime assessment fails despite prior headless correctness/timing passes. Investigating existing artifacts and source without launching the viewer; shadow-record attribution remains unproved.
- 2026-11-13: Preserved delayed IPS/logs privately, matched exact G1 UUID/hash and symbolicated mainloop rethrow. Decode sources/archive match Alpha6; no original throw identified. Bounded read-only cache selection passes240/240real worker requests. Added stage/type/message diagnostics preserving original propagation, with std/nonstd/responder regression checks; narrow build/5CTest entries and doctor pass. This improves the missing evidence and is not a crash repair; diagnostic app packaging pending.
- 2026-11-13: Diagnostic efd0367ca4/68989 full Release build passes0;47cases/1,277assertions include diagnostic-recorder failure retaining the original exception. Initial static source/signature/architecture/native-feature/marker check passes. Separate staging/verification in progress; PPD-090 remains investigating and owner acceptance remains failed. No viewer launch, crash-fix claim, goal two or push.
- 2026-11-13: Separately staged diagnostic68989 passes346ARM64/native/features/source/marker/deepstrict-signature checks;7,841copied entries match. MainSHA1d423bd711caf6530f0a3a6520b23350f7861f0cb29467493cc10d59cad357eb; original crashed bundle unchanged. Ready for one owner arrival retry to capture missing exception evidence. No fix established; owner verification stays failed and no new-goal/push authorization inferred.
- 2026-11-13: Owner retry on exact diagnostic68989/efd crashes20:23:26.8084. New context identifies primary J2C decode,282193bytes,1024x1024,5components,discard0; native thrown type is std::runtime_error but the catch-all handles it, proving std::exception matching failed. Private executable typeinfo mismatch is separately registered PPD-091 and reproduced headlessly with the actual linked voice-wrapper archive. Prior240-case corpus excluded components>4; it did not cover this bake class. No agent viewer launch or corrective runtime success.
- 2026-11-13: Joint real voice-wrapper/image regression fails3cases/SIGABRT before correction, then passes3cases/34assertions after scoped PPD-091 compiler-boundary repair; all6image/worker/ABI entries pass50cases/1,311assertions. Representative real5channel bakes pass12requests; exact offending input remains unavailable. Repairing the demonstrated abort mechanism, not declaring the codec input valid or owner scene stable. Full-app build/verification next; owner verification remains failed.
- 2026-11-13: Corrected fef1e756fd/68991 full build/static/deep signatures/346ARM64/source/native/retained features and final imported system exception RTTI pass; staged7,841entries identical, original apps preserved. Ready for owner same-scene retry. No agent launch, owner confirmation/closure, goal two or push.

- 2026-11-13: Owner explicitly reports that the other chat fixed this crash and that they confirmed it gone, then authorizes continuation through all architecture goals before one combined test. The continuation baseline contains fef1e756fd; the existing five-minute G1 profile is pinned to that repaired source. This records owner confirmation, not a new agent launch, universal stability claim or push approval.

- 2026-11-14: Combined G2–G5 source d5ae05f2/68994 retains the repair. Full Release build and final static verifier pass; final viewer imports libc++ standard exception typeinfo with zero private standard exception RTTI. This is regression evidence, not a new owner runtime test or push approval.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred. Earlier owner-confirmed repair verification remains valid and is not replaced by this build evidence.


## PPD-091 — Private standard-exception RTTI breaks base-class catches

- Status: verified
- Confirmation: reproduced
- Owner: Codex C++ exception ABI integration
- Discovered: 2026-11-13
- Original reference: Upstream `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` llwebrtc/CMakeLists.txt disables RTTI for subclasses of vendor WebRTC classes. Native fork combines this with hidden visibility. The standard C++ contract requires a thrown std::runtime_error to match catch(const std::exception&); the original WorkQueue intentionally forwards unexpected failures, while ordinary codec errors should remain failed image requests.
- Evidence: [ABI correction and actual-library regression](work/active/exception-abi-repair.md), [G1 crash investigation](work/active/g1-arrival-crash.md).
- Next action: Retain the owner-confirmed exception-ABI repair in every continuation build; preserve the vendor boundary contract. No push permission is inferred. Broader architecture/scene performance remains separately unverified.
- Verification: Baseline actual-library probe fails42; joint voice/codec3cases fail/SIGABRT. Darwin-only PRIVATE wrapper -fno-exceptions alongside -fno-rtti restores std catches;3cases/34assertions and total50cases/1,311assertions pass. Corrected fef/68991 final app imports system exception RTTI with0private standard typeinfos;346ARM64/signature/source/native/features pass and7,841staged entries match. Actual vendor flags prove the boundary configuration. Owner confirms the repaired crash gone; keep the scoped vendor boundary and all evidence. No push approval.
- Owner verification: passed
- Push approval: not_granted

### History

- 2026-11-13: Registered before production repair after diagnostic retry exposed a standard exception handled by catch-all. Actual archive reproduction fails as observed; source/linker option trials are isolated under private evidence. PPD-090 remains the owner-visible crash, and both records stay open until corrective evidence and owner confirmation.
- 2026-11-13: Visibility-only and RTTI-enabled compiler trials rejected (ineffective or missing vendor RTTI). Applied Darwin-only private no-exception wrapper flag matching verified vendor flags; preserved normal caller/codec EH. Actual joint-library regression changes failed3cases/SIGABRT to passed3cases/34assertions;6CTest entries50cases/1,311assertions pass. Final app build/verification and owner acceptance remain pending.
- 2026-11-13: Final corrected G1/68991 and staged copy pass0private standard exception typeinfos (system imports), source/version/native/features/346ARM64/deep signature gates; sourcefef1e756fd, mainSHA c188ebdfd45139677fcb7c45a9bfcacaa80f11867dcc9184090ba88a0dec9d08. Ready for owner verification; no closure/push or agent viewer launch.

- 2026-11-13: Owner explicitly reports that the other chat fixed this crash and that they confirmed it gone, then authorizes continuation through all architecture goals before one combined test. The continuation baseline contains fef1e756fd; the existing five-minute G1 profile is pinned to that repaired source. This records owner confirmation, not a new agent launch, universal stability claim or push approval.

- 2026-11-14: Combined G2–G5 source d5ae05f2/68994 retains the Darwin wrapper exception boundary. Full Release build and final static verifier pass, importing system std::exception RTTI with zero private standard typeinfos. Owner-confirmed G1 verification is retained; no new runtime or publication claim.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred. Earlier owner-confirmed repair verification remains valid and is not replaced by this build evidence.


## PPD-092 — Accelerated media can overwrite a leased native texture generation

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex Apple-first architecture integration
- Discovered: 2026-11-13
- Original reference: Native IOSurface media paint uses a reusable render-target mirror in LLViewerMediaImpl::paintMetal from the G1 baseline; original GL binding did not provide the new immutable native CPU-record lease contract.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Owner checks accelerated media playback and texture replacement in the combined68995 viewer; native generation lease/COW tests already pass. Keep actual IOSurface/media behavior unverified until observed.
- Verification: Source integration now allocates a fresh IOSurface mirror when its current LLImageGL native generation is leased, keeping copy/commit/adoption ordering. Coherent native resource/COW suites pass; actual accelerated media playback/owner acceptance remains untested.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered before adapting the external writer to the new lease model. LLImageGL upload COW alone is insufficient because the IOSurface blit bypasses it. Existing media and all old evidence remain retained.
- 2026-11-13: Added the accelerated writer lease check alongside native COW publication. Coherent validation18/18 passes with API/shader checks, including351assertions/7native generation cases; this is not a new browser/media runtime test.

- 2026-11-14: Integrated repair and final coherent native checks are retained in source d5ae05f2/68994; full Release build/static verification pass. Updated next action reflects owner combined-app testing. Automated evidence does not close this record or grant push permission.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-093 — Metal 4 allocation failure can strand an unused visibility slot

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native GPU scheduling
- Discovered: 2026-11-13
- Original reference: Apple-first G2–G5 implementation on baseline5836d41753; this is a pre-delivery source-review finding, not a reported owner regression.
- Evidence: [Combined architecture implementation ledger](work/active/apple-first-architecture-g2-g5.md).
- Next action: Keep the slower Metal4 visibility mode disabled. If revisiting it, inject pre-submission event-allocation failure and verify slot reclamation separately from submitted-event/consumer completion.
- Verification: Slot reuse now separates actual compute submission from chosen Metal4 mode: pre-submission failures need no GPU signal, while submitted compute still waits for its event and consumer completion. Coherent extended GPU lifetime/cancel/parity suite passes; forced native event-allocation failure is not independently injected.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered before repair after independent source review during the first native-slice build. Preserve failed paths and test actual reclamation/range semantics before enabling the combined app.
- 2026-11-13: Applied compute_submitted guard and retained dual completion requirements. Real GPU generation/classic/Metal4 cancel/lifetime tests pass in coherent18/18; no claim of forced event-allocation-failure execution.

- 2026-11-14: Actual compute-submission tracking is integrated in d5ae05f2 and native cancellation/lifetime suites pass. Full-cost classic/Metal4 GPU generation remains slower and off; native event-allocation failure itself has not been injected.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred. Metal4/GPU-generation variants remain disabled after adverse net-cost gates.


## PPD-094 — Texture-index map span can exceed the vertex storage lane

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native generation publication
- Discovered: 2026-11-13
- Original reference: Apple-first G2–G5 implementation on baseline5836d41753; this is a pre-delivery source-review finding, not a reported owner regression.
- Evidence: [Combined architecture implementation ledger](work/active/apple-first-architecture-g2-g5.md).
- Next action: Owner exercises geometry/attachment updates in the combined68995 viewer; retain exact final-lane and leased/unleased dirty-publication tests and the unchanged full-copy path for leased generations.
- Verification: Repaired span to (count-1)*stride+lane_size with zero-count handling. The coherent native resource suite passes351assertions/7cases, including the exact final texture-index lane and leased/unleased dirty publication; owner viewer acceptance pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered before repair after independent source review during the first native-slice build. Preserve failed paths and test actual reclamation/range semantics before enabling the combined app.
- 2026-11-13: Corrected candidate passes exact native final-lane and partial-range tests,351assertions/7cases, and coherent18/18 validation.32small safe writes publish2,048B versus2,621,440B under forced full orphans; byte reduction only, not CPU/FPS.

- 2026-11-14: Integrated repair and final coherent native checks are retained in source d5ae05f2/68994; full Release build/static verification pass. Updated next action reflects owner combined-app testing. Automated evidence does not close this record or grant push permission.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-095 — Extended indirect shadows fail native depth parity

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native shadow execution integration
- Discovered: 2026-11-13
- Original reference: Existing native scalar shadow shader/binding path on baseline5836d41753 is the comparison. The new rigged/covered indirect path must preserve its R32 output.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Retain passing actual-command-emission parity tests and keep classic/Metal4 GPU-generation variants off because they fail complete-cost gates. Reconsider only with a changed persistent-input/visibility implementation and a new correctness/net-cost gate.
- Verification: Actual MSL render_command emission repairs the initial CPU-copy depth mismatch. Coherent native output/query/current-pose/cancellation/lifetime suites pass, including multi-tile ordering. Serial and repaired parallel GPU generation both fail net-cost controls on M1 Max, so production enables only separately tested selective CPU-encoded opaque ICB runs.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered immediately after first actual GPU execution and before correction. Resource leases/native publication and detached preparation suites pass independently. The extended indirect path is not eligible for the combined app until parity is repaired.
- 2026-11-13: Real GPU generation repairs the demonstrated copy-command parity failure. Coherent validation18/18 includes latest generated-only setup and ordered query runs; scalar/CPUICB/classic/M4 R32 parity passes. Performance selection and owner scene acceptance remain separate.

- 2026-11-14: Integrated repair and final coherent native checks are retained in source d5ae05f2/68994; full Release build/static verification pass. Updated next action reflects owner combined-app testing. Automated evidence does not close this record or grant push permission.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred. Metal4/GPU-generation variants remain disabled after adverse net-cost gates.


## PPD-096 — Source lease deduplication loses indexed coverage slots

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native source admission integration
- Discovered: 2026-11-13
- Original reference: Existing indexed material shadow contract preserves every texture slot, including repeated white/default/diffuse handles. New ALMetalShadowSourceRecord factory validates one ordered lease per slot.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Owner checks repeated/aliased alpha and PBR coverage textures in the combined68995 viewer. Retain ordered per-slot texture leases, including aliases; native two/eight-slot cases pass.
- Verification: Actual pipeline source builder now preserves one native texture lease per shader-visible slot, including aliases. Two/eight repeated-slot source cases pass in the coherent native prepare suite; pipeline compiles, owner scene acceptance pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered before repair during independent review of actual pipeline integration. Existing native helpers passed their initial single-slot fixtures; those tests did not cover this caller contract.
- 2026-11-13: Replaced deduplicated push with ordered per-slot lease publication. Coherent native repeated-texture prepare cases pass and actual pipeline syntax passes0; no owner scene regression was claimed or observed.

- 2026-11-14: Integrated repair and final coherent native checks are retained in source d5ae05f2/68994; full Release build/static verification pass. Updated next action reflects owner combined-app testing. Automated evidence does not close this record or grant push permission.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-097 — Abandoned batched shadow work can retain published validity and pending queries

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex frame transaction integration
- Discovered: 2026-11-13
- Original reference: Native G1 shadow capture may borrow an existing probe/hero command batch. Original GL draws did not expose an uncommitted native transaction; native validity/cache/query publication must follow actual commit or cancellation.
- Evidence: [Combined architecture implementation goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Owner checks shadows, occlusion, ordinary probe faces and cancellation/reload in the combined68995 viewer. Preserve streamed per-view commits, borrowed outer ownership and exactly-once validity/query settlement.
- Verification: Exactly-once synchronous commit/cancel observers, per-slice pending/revision validity and deferred cache/query publication are integrated. Abandoned native query batches fail instead of remaining pending. Native settlement/observer/cancel/lifetime suites pass; fixed headless comparisons prove six streamed commands per group. Whole-invocation batching was slower and rejected (PPD-101); logical group identity repairs probe admission (PPD-103).
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered before transaction changes. Four/six shadow views can share one command only after failure/publication semantics and logical-frame admission are explicit; reducing command count alone is not sufficient.
- 2026-11-13: Coherent Release build0 and18/18 API/shader-validated suites pass: native admission/reload/cancel/observer semantics and25assertions/2query abandonment cases. Pipeline compile0 and independent source review cover same-command receiver validity and per-slice revision guards; one-command/six-real-slice output proof is next.

- 2026-11-14: Integrated repair and final coherent native checks are retained in source d5ae05f2/68994; full Release build/static verification pass. Updated next action reflects owner combined-app testing. Automated evidence does not close this record or grant push permission.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-098 — Shadow query settlement can throw during capture cleanup

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex frame transaction integration
- Discovered: 2026-11-13
- Original reference: Native G2–G5 SettleShadowVisibility capture cleanup; original query ticket ownership must fail open when native work is not published.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Retain recoverable query ticket ownership and noexcept cleanup in the combined68995 viewer; fault injection of actual allocation failure remains unexecuted.
- Verification: Source repair keeps local/shared ticket ownership recoverable across allocation and observer registration failures, discards every ticket including any partial prefix, and catches cleanup exceptions. Coherent native settlement/observer-exception/cancel cases pass. Actual allocation failure was not injected; no owner incident is attributed to this source-confirmed hazard.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered before repair after coherent ordinary native tests pass. This is an exceptional-path source finding, not a reproduced owner crash or attribution to PPD-090.

- 2026-11-14: Integrated repair and final coherent native checks are retained in source d5ae05f2/68994; full Release build/static verification pass. Updated next action reflects owner combined-app testing. Automated evidence does not close this record or grant push permission.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-099 — Retained-source candidate adds complete native CPU cost

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native source architecture
- Discovered: 2026-11-13
- Original reference: Same-binary current-owned/raw native API versus retained-certified sources on the G2–G5 candidate. This is not an untouched Alpha6 comparison or a live pipeline benchmark.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Owner assesses the combined68995 viewer. Keep the large-family8192 threshold, ordinary small-family owner path and exact cost limitations; do not infer live key/admission/cull or unique-source throughput from a16-source fixture.
- Verification: Initial600-row retained candidate and second compact trial remain rejected. Changed owner fast path plus compact8192 threshold passes sourceV3 exact800-row analysis: six-view8192 family CPU median/p95−8.50/−8.68%, completed wall−7.55/−6.89%, all10 CPU/wall blocks positive. Small cases CPU+4.53/+0.39/+0.04% show no consistent net win. Fixture includes source seals/command ownership but excludes live LLDrawInfo key/LRU/admission/cull and pose solve;8192 instances reuse16 sources.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered immediately after the first complete-cost run; ordinary depth/lifetime parity had passed. Successful correctness does not make this performance result acceptable. No combined viewer delivered and no in-world attribution claimed.
- 2026-11-13: Compact-instance/shared-context/one-command aggregate lease candidate implemented with current raw control strengthened to explicit model/context setup; original600-row failure retained. Pipeline syntax/source lifetime review pass; native correctness and new fixed cost trials pending. No activation pass yet.
- 2026-11-14: Compact V2 fixes most overhead but still fails strengthened raw control (busy median+0.935%,light+9.699%); fixed compact Dispatch supports8192 only, not2048. V3 owner-direct source consumption below8192 plus streamed per-view command ownership implemented; new declared8192 single-family control added. Native parity/cost validation next, all earlier failures retained.

- 2026-11-14: Integrated repair and final coherent native checks are retained in source d5ae05f2/68994; full Release build/static verification pass. Updated next action reflects owner combined-app testing. Automated evidence does not close this record or grant push permission.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-100 — Serial GPU command generation causes severe completion-time regression

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native GPU scheduling
- Discovered: 2026-11-13
- Original reference: Same production native renderer, current bulk direct versus CPU ICB/classic GPU generation/Metal4 GPU generation; identical source packets, six views and depth parity. This is not viewer frame/FPS evidence.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Keep classic and Metal4 GPU command generation off. A future implementation must move persistent dirty-source visibility before repeated CPU discovery and pass full-cost controls; retain actual-emission/multi-tile parity tests.
- Verification: Baseline1 passes exact9000rows/90groups,100samples per arm/case,10balancedblocks, source/binary unchanged, actual Metal4 timing feedback required. Static256 all-visible completion-plus-cleanup wall medians regress784.87% classic/811.36% Metal4; static2048 regress1503.99%/1513.22%. Mixed25%reject still regresses1283.47%/1355.77% at2048. CPU ICB saves19.02% CPU at static2048 but wall p95 regresses6.38%; this is not an activation pass. Current compact kernel emits every command serially in one thread per run.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered immediately after first complete-cost GPU trial. Correctness previously passed all tested modes; nominal CPU/API benefits do not offset this measured completion-time regression. New GPU modes remain disabled in the production scene. All baseline scripts, raw rows, feedback counters and hashes retained.
- 2026-11-13:256-lane stable tiled-prefix command emission and GPU-only resource/template cleanup candidate implemented; native/offline MSL syntax passes. New multi-tile/query-run ordering proof added; full native correctness and original9,000-row cost comparison pending. Modes remain disabled.
- 2026-11-14: Parallel candidate passes80,364/27native correctness but new9,000-row cost still fails completion-wall gate (static2048 classic+39.71%,Metal4+20.89%). Slab/direct-data experiment is next; GPU modes remain off. PPD-102 separately records benchmark epoch aging concern without overturning prior negative results.

- 2026-11-14: Parallel stable SIMD-prefix generation repairs the serial emission bottleneck and preserves ordered257/511/259-draw runs in a1027-input native case, but the fixed9000-row comparison still fails activation. The corrected9600-row streamed shared-data comparison also does not justify GPU/Metal4 defaults. Only the separate400-row mixed opaque CPU-ICB selector passes and is enabled in d5ae05f2.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred. Metal4/GPU-generation variants remain disabled after adverse net-cost gates.


## PPD-101 — Whole-invocation shadow batching loses useful CPU/GPU overlap

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native frame scheduling
- Discovered: 2026-11-13
- Original reference: Same native current-bulk renderer, six streamed commits versus one transaction containing four directional and two projector passes. Quality/source preparation held equal; no viewer frame/FPS claim.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Owner assesses streamed shadow behavior in68995. Retain six per-view commits and two pending logical groups with borrowed probe/hero ownership, successful-commit publication and actual completion reuse; whole-invocation batching remains rejected.
- Verification: Fixed1200rows/12groups,100samples per arm/case,10balancedABBA/BAAB blocks, native6vs1command counts and source/binary unchanged. One transaction saves1–2%CPU at2048draws but completion-plus-cleanup wall medians regress27.53%rigged opaque/27.86%riggedPBR/35.13%static; p95+20.26%/+25.24%/+25.59%, zero positive wall blocks. LightPBR wallmedian+7.34%/p95+22.93%. Reduced commit count alone is not a performance benefit.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered before replacing the candidate batching default. Six-slice/query pixel parity passed; whole-mechanism performance fails because GPU work waits for all CPU preparation instead of overlapping successive views. Existing G1 standalone output/order remains the comparison, outer owned batches remain borrowed.
- 2026-11-14: Root restored streamed per-view commit with exact failure/query/publication guards and preserved borrowed owners. Source syntax passes0 before group-ID followup; PPD-103 fixes ordinary-probe admission identity. No runtime acceptance claimed.

- 2026-11-14: Source d5ae05f2 preserves streamed per-view commits with corrected logical group identity. Accepted source800 and mixed-selector400 timing receipts use actual six-command streaming; no rejected one-command result was relabeled.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred. Rejected whole-invocation batching remains off; streamed per-view commands are retained.


## PPD-102 — GPU cost fixture advances material-cache epochs twice per frame

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native performance evidence
- Discovered: 2026-11-13
- Original reference: Production advances frame data once per gFrameCount; generation cost fixture advances at start and again during same-arm retirement. Native material cache retains immediately preceding generations.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Retain corrected once-per-frame fixture lifecycle, independent warm renderer/arena per arm and material key/create counters. Preserve the earlier receipts and do not reinterpret them as passing.
- Verification: The corrected streamed shared-data comparison passes strict9600-row/96-group structural checks with independent warmed arm state and correct material epoch lifecycle. Global covered/PBR shared-data and GPU/Metal4 variants still fail cost gates. Selective opaque CPU ICB is separately accepted by400 mixed-control rows with zero covered-table creations/misses.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered before fixture repair while implementing the declared completion-owned slab trial. All previous9,000-row receipts remain immutable with their exact fixture source/hash and limitations.

- 2026-11-14: Integrated repair and final coherent native checks are retained in source d5ae05f2/68994; full Release build/static verification pass. Updated next action reflects owner combined-app testing. Automated evidence does not close this record or grant push permission.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-103 — Main-display frame identity can exhaust shadow admission across probe faces

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex shadow view admission
- Discovered: 2026-11-13
- Original reference: display() increments gFrameCount; display_cube_face() does not. Ordinary reflection faces plan two or four sun cascades and beginMetalWorldGBufferCapture starts an outer command only for the hero specialization, not every ordinary probe.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Owner checks ordinary reflection-probe faces and shadows in68995. Keep monotonic shadow-group identity separate from gFrameCount, two pending logical groups and six commands per group, while preserving existing outer owners.
- Verification: Source d5ae05f2 allocates/restores a logical shadow-group serial for each independently generated invocation. Native32-command/eight-group admission proof and coherent settlement/lifetime suites pass; actual pipeline and full Release build pass. No simulation/display frame fence was reassigned.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-13: Registered before repair while reviewing streamed per-view admission. Earlier assumption that every probe had a pre-existing outer batch was incorrect; source shows only hero face begin creates that command. Do not raise an arbitrary global width or silently drop later faces.
- 2026-11-14: Capture-local monotonic shadow group serial/current scope now feeds admission independently of gFrameCount. Independent source review confirms ordinary-probe call chain, group/renderer swap alignment and reload retention. Two pending logical groups/six commands per group; simulation/presentation/native-resource frame accounting unchanged. Native policy regression and final app test pending.

- 2026-11-14: Integrated repair and final coherent native checks are retained in source d5ae05f2/68994; full Release build/static verification pass. Updated next action reflects owner combined-app testing. Automated evidence does not close this record or grant push permission.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-104 — Slab lifetime fixture lacks a dependency on the purported GPU gate

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native lifetime validation
- Discovered: 2026-11-14
- Original reference: New stable shadow slab lifetime fixture on db90ed329f2c630a7fac47233506861018315f95b708a6755b5efe9a68140434; actual native completion token must be the retirement authority.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Retain the corrected actual-resource gate for blocked lifetime/cancel/destruction and the separate completed-reload test. Do not demand an earlier reader remain pending after synchronous reload uploads.
- Verification: Both failed29/30-case snapshots are retained. Gate3 gives the wait a tracked dependency on actual geometry and tests all original three-slot/weak-owner/cancel/retry/renderer-destruction assertions before release; completed reload is tested separately because reload performs synchronous commitAndWait uploads. Coherent final native suite passes81,815assertions/39cases plus351/7resource checks; no production allocator change or timeout increase was needed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before test-oracle repair. All3failed assertions/receipts/source hashes retained; native lifetime proof is required before slab activation. Hidden slab cost will also use streamed six-view submissions to match the selected production policy, retaining all prior rejected one-command results.
- 2026-11-14: Gate2 adds tracked exact-position write/read dependency and preserves assertions, but29/30cases still fail (81,422/81,429assertions) because five-second watchdog releases the gate during reload. Independent source proof: reloadShaders calls create(), whose SMAA/default/hero/probe/SSR/grade uploads synchronously commitAndWait on the same device queue. Requiring prior readers still pending after that synchronous operation creates a fixture self-wait. Approved split: retain all original blocked slot/lifetime/cancel/renderer-destruction checks before gate release; completed reload pool-transfer/content proof remains in its separate case. No timeout increase, production allocator repair or timing result claimed.

- 2026-11-14: Integrated repair and final coherent native checks are retained in source d5ae05f2/68994; full Release build/static verification pass. Updated next action reflects owner combined-app testing. Automated evidence does not close this record or grant push permission.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-105 — Streamed source-cost parser incorrectly orders overlapping GPU intervals

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native performance evidence
- Discovered: 2026-11-14
- Original reference: Per-command GPU timestamp intervals may overlap; their sum is not a logical frame span or GPU busy time.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Retain original native800-row receipt and failed parser alongside analysis-only repaired results. Continue treating GPU interval sums as overlapping envelopes, not GPU busy time or a logical span.
- Verification: Native sourceV3 exits0 with800 rows and unchanged5038ce41 binary. Analysis-only v2 replaces the invalid sum<=span condition with finite/positive/per-view envelope bounds and preserves exact source/count/counter checks. The same rows pass without native rerun:8192 family CPU−8.50%, completed wall−7.55%; small cases remain non-wins.129 evidence files are copied with verified hashes.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before analysis-only repair. Native receipt, source snapshots, parser hash and failed traceback remain immutable; no V3 performance acceptance claim yet.

- 2026-11-14: Integrated repair and final coherent native checks are retained in source d5ae05f2/68994; full Release build/static verification pass. Updated next action reflects owner combined-app testing. Automated evidence does not close this record or grant push permission.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-106 — Bounded source LRU can thrash on repeated scans exceeding entry capacity

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native source-cache admission
- Discovered: 2026-11-14
- Original reference: G2 per-LLDrawInfo retained source working set,8192entry/count and byte budget. Repeated per-view ordered scans must not turn bounded caching into reconstructing every source each view.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Owner assesses bounded cache behavior in68995. Keep current-epoch protected admission, generation replacement, count/byte/age bounds and weak overflow reuse only while actual packet/command ownership survives. Live source-key/admission/unique-source cost remains unmeasured.
- Verification: Actual helper reproduction: old8192-entry LRU yields0hits/8193admissions on each subsequent8193-source scan (failure exit42). Protected admission yields8192hits/0admissions on all five subsequent scans with constant8192entries/262estimated shared-wrapper bytes. Final native154assertions/7cache cases pass and helper is used by pipeline in d5ae05f2; no capacity increase.8192 preparation fixture still reuses16 sources and does not prove whole-pipeline unique-source throughput.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before cache-policy repair after independent cardinality review. The valid8192 native preparation/encoder mechanism win remains bounded to its declared reuse fixture; key/admission/unique-source costs remain unmeasured until the added cache stress proof.

- 2026-11-14: Integrated repair and final coherent native checks are retained in source d5ae05f2/68994; full Release build/static verification pass. Updated next action reflects owner combined-app testing. Automated evidence does not close this record or grant push permission.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-107 — Broader vertex generation and label expectations fail on combined source

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex combined regression integration
- Discovered: 2026-11-14
- Original reference: Vertex generation/lifetime contracts originate in LLVertexBuffer; native safe-current publication is new in G5. No owner scene failure is attributed.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Retain both safe-current and actual leased/retired-store fixture branches in final broad verification. No production buffer or label change is indicated; preserve the original failure and independent lease-only-after-completion coverage.
- Verification: Final contained broad regression at d5ae05f2/68994 fails llvertexbuffer_test line143 and alvertex_label_metal_test lines62/130/136. Expected first vertex generation differs; one buffer label differs; expected index orphan remains the same pointer. These are observed assertion failures, not yet proof of unsafe generation reuse or a stale test. Evidence files final-broad-contained-1.json and matching ctest/specialization logs remain local under the combined evidence root.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered after the first final broad run:57/60CTest entries and9/10exact specializations pass; bounded private codec corpus and actual voice/codec exception ABI pass. App build/static checks pass, but combined app acceptance remains held until triage and corrective verification. No viewer launch or push.

- 2026-11-14: Independent source review confirms the failing pointer/label checks assume unconditional retired-first reuse. G5 intentionally prefers a completed unleased current store before scanning retired stores. Retired first retains its old label correctly; index remains current when unleased/completed. No byte/lifetime defect is established. Authorized fixture repair must explicitly cover safe-current in-place bytes/labels AND force a real current lease to exercise retired reuse/index orphan; queued GPU old-byte checks stay intact.

- 2026-11-14: Two-fixture repair reviewed independently by root and CPU peer, then committed locally as6baeb0340a1ddeb89aa265a1a64321ad1e0cd57a (+45/−6; production/CMake/tools unchanged from d5ae05f2). Narrow build exits0 in2.12s; API/shader-validated fixtures plus native generation suite pass573assertions/11cases. Both safe-current and forced leased orphan/recycle bytes/labels, selective index updates and queued GPU old-byte checks remain. Lease acquisition also marks a new open read epoch; separate generation cases isolate lease-only-after-completion protection. Broader final-source recheck remains required.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-108 — Pipeline archive test fails under validation instrumentation

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex combined regression integration
- Discovered: 2026-11-14
- Original reference: Native pipeline binary archive policy has no original OpenGL equivalent; inspect ALMetalDevice producer and cache test together.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Retain the corrected separate disk, actual-instrumented and selective environments in final broad verification. Preserve the original failed globally instrumented receipt; no archive policy or assertion relaxation is needed.
- Verification: Existing suite is correct; external broad runner wrongly combined disk persistence assertions with shader instrumentation. Correct native probes pass without skips: disk5905assertions/6cases proves persistence/hits, actual instrumented bypass882/1, selective native label-validation98/1. Same almetal binary hash before/after, no production/test source change. Receipt ppd108-109-native-probes-1.json preserves three environments and logs.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered after the first final broad run:57/60CTest entries and9/10exact specializations pass; bounded private codec corpus and actual voice/codec exception ABI pass. App build/static checks pass, but combined app acceptance remains held until triage and corrective verification. No viewer launch or push.

- 2026-11-14: Source attribution confirms an external runner environment mismatch: cache.mm build() preserves actual shader-validation PSOs and deliberately bypasses archive persistence. Existing tests already separate disk, instrumented-bypass and selective-validation tags. No production/test source repair needed; rerun disk checks with API validation and shader instrumentation off, plus separate actual-instrumented/selective checks with their required environments. Do not weaken disk-hit or bypass expectations.

- 2026-11-14: All three policy probes pass without skips in their existing required environments. Mandatory disk persistence/hits and actual shader validation/bypass/selective assertions remain unchanged; PPD108 is a verification-runner mismatch, not a production archive regression.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-109 — Exact PBR terrain specialization selection exits without running

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex combined regression integration
- Discovered: 2026-11-14
- Original reference: Catch test selection/harness is a verification mechanism, not the terrain rendering contract.
- Evidence: [Combined architecture goal](work/active/apple-first-architecture-g2-g5.md).
- Next action: Keep the escaped exact literal in final headless runner and require one executed case. No terrain implementation change is indicated; preserve the no-match exit5 as a selector failure.
- Verification: Metadata proves original quoted comma-bearing selector exits5 with no case. Escaping the comma selects exactly the intended registered case; actual API/shader-validated PBR terrain rendering then passes128assertions/1case, no skip. Same almetal binary hash before/after, no terrain source change. Receipt ppd108-109-native-probes-1.json preserves original failure and corrected native probe.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered after the first final broad run:57/60CTest entries and9/10exact specializations pass; bounded private codec corpus and actual voice/codec exception ABI pass. App build/static checks pass, but combined app acceptance remains held until triage and corrective verification. No viewer launch or push.

- 2026-11-14: The selected PBR terrain case contains a comma, unlike passing exact selections. Runner must validate Catch registered name/filter syntax and require exactly one case; no terrain implementation repair is authorized without a rendered failure.

- 2026-11-14: Metadata-only selection receipt ppd109-selection/receipt.json confirms the original quoted comma-bearing literal exits5 without a selected case. Escaping the comma returns0 and lists exactly one matching registered case; a unique-prefix control selects that same case. Native rendering retest with the corrected escaped literal remains next, not inferred from enumeration.

- 2026-11-14: Correct escaped exact PBR terrain case now executes and passes128assertions/1case under native validation. This closes the investigative attribution question, not owner runtime acceptance; no terrain code was changed.

- 2026-11-14: Final combined code/test6baeb0340a/Release68995 is built and separately staged. Final59CTest+10exact+3archive-policy+private corpus stages all pass0 (563,974assertions/301cases;240/240requests); actual voice/codec ABI, built/staged346ARM64/signatures/nativeownedimports/source/features/systemexceptionRTTI and strictproduction3548files0block/0errors pass. Full7839file/2link/432directory staged inventories equal; oldapps preserved. Exact receipts and headless/performance limits are in the combined results/ledger. No agent launch or push; no new owner runtime acceptance inferred.


## PPD-110 — Combined candidate profile has slower cadence and higher submission pressure than G1

- Status: investigating
- Confirmation: reproduced
- Owner: Codex Apple-first performance assessment
- Discovered: 2026-11-14
- Original reference: Repaired native G1 sourcefef1e756fd/68991,301.864885s busy-club capture2026-11-13, same M1Max64GiB host. This is a sequential live workload comparison, not original OpenGL or a controlled identical-world replay.
- Evidence: [G2–G5 busy-club capture and comparison](research/g2g5-busy-club-profile-2026-11-14.md), [headless implementation results and limits](research/apple-first-architecture-g2-g5-results.md).
- Next action: Implement producer-amortized immutable source publication at pipeline.cpp captureMetalShadowScene/shadowSourceKey/cache/aggregate ownership using the new real workload census; preserve exact invalidation/completion and assess whole-frame cadence/latency/quality. Recovered FPS alone does not close architectural performance regression.
- Verification: Actual302.345784s Game Performance Overview attach to verified6bae/68995/PID19459 exits0 at its5m limit. Whole presentedFPS17.773vs20.364(−12.72%); associateddisplay56.385vs49.135ms, associatedGPUspan87.220vs85.394ms(+2.14%); commandbuffers/frame32.842vs23.882(+37.52%). Last60FPS17.925vs20.113(−10.88%). Nominal thermals throughout. Both individual-frame tables empty: no framep95. Saved graphics match before/after/current/G1; live camera/population/cache/probe/viewport equivalence and causal attribution remain unverified.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered the adverse observed comparison before any repair attempt. Owner explicitly requested this capture after prior no-agent-launch/profile delivery; only attachment to the already running viewer occurred. Owner permitted normal quit after measurement ended; process absence after trace finalization is not a crash report. Headless gates remain mechanism evidence, not a live performance claim. CPU attribution/333DWARF-warning coverage check is next; raw trace/logs stay private/local.

- 2026-11-14: Completed whole-engine analysis under owner steering. Actual published3456×2168 matches both runs,420saved controls match. Main CPU49.30vs33.97ms/estimatedpresent; sourcecapture7.34vs2.22 while admittedmain sources8482vs6548/frame(+29.54%). Positively observed key/cache/aggregate ownership costs1.40/1.05/1.36ms separately. Logged generic indirect draw/run/prepared deltas0 cover new opaque execution path too, so the accepted ICB mechanism has no recorded successful execution in the periodic window; Dispatch/cache-hit/eligibility counts remain unknown.

- 2026-11-14: Streaming is normal engine work, not dismissal of the adverse result:56,104vs453 decode completions (~124.7×rate),628.601vs10.103decoderCPU-s; maintextureupdate4.39vs0.99ms/present; waiting peaks2508/4.64soldest. Texture creations16.22vs6.97/frame, buffers256.26vs217.99/frame but buffer/s only+2.39%. Currentpressure system-low17/30,bias1–3.029vs1; pool+200.074MiB at same published resolution,0evictions. Cause of churn/extra commands is not isolated. Source graph confirms separately bounded main/shadow/probe/upload producers on common nativequeue; +19.91%estimatedCB/s. Full assessment/report and safe receipts are retained. No codefix/newcapture/restart/push, no closure or causalFPS claim.

- 2026-11-14: Owner explicitly requests logging silent opaque preparation outcomes and needed workload diagnostics, fix process-cap issue and prepare next candidate for owner run. New bounded retest goal active; no viewer/profile/push. Root owns pipeline log integration and final delivery, disjoint agents own native stats, memory policy and residency census.


- 2026-11-14: Retest candidate4918fd7/68999 delivered with aggregate opaque/source/publication/residency telemetry and PPD111/113/114 repairs. Doctor/build/static/61CTest+10exact+3archive+corpus stages pass; no new viewer/profile/FPS evidence. Larger architecture remains partial. Evidence: [retest delivery and exact receipts](work/active/apple-first-retest-20261114.md). Owner verification pending; no push.


- 2026-11-14: Owner authorizes five-minute profiling after manually starting exact4918fd7/68999 and reporting arrival. Root attaches to verifiedPID58603, Game Performance Overview5m, evidence retest-busy-club-profile-20261114-101904. Capture in progress; no new performance outcome or defect closure inferred. No app launch/settings change/push.

- 2026-11-14: Owner-authorized retest4918fd7/68999/PID58603 completes301.767390s/time limit/exit0.20.3569FPS vs prior17.7729 (+14.54%) and G120.3639 (same); mean display interval49.131ms, command buffers24.404/present. CPU-to-display97.144ms remains+5.98% vs G1.420saved controls and3456×2168 match; live demand quantified, no p50/p95. Main capture6.832CPUms/present vs G12.221 with18.33% more source inputs; decoder storm absent. Recovery is observed, full architectural superiority/cap-fix causality unproved. No new renderer code, launch or push. Evidence: [completed retest analysis](research/apple-first-retest-profile-2026-11-14.md), private retest-busy-club-profile-20261114-101904 receipts.


## PPD-111 — Legacy process-cap headroom feeds texture system-pressure control on Apple UMA

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native resource/streaming budget integration
- Discovered: 2026-11-14
- Original reference: Original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 already has post-create immediate-downscale handling; repaired native G1fef1e756fd and current6bae share the inspected memory/texture governor source. MaxHeapSize64 is a configured process limit, not measured physical RAM availability.
- Evidence: [Architecture, fast-path and log follow-up audit](research/apple-first-architecture-audit-2026-11-14.md), [original profile assessment](research/g2g5-busy-club-profile-2026-11-14.md).
- Next action: Owner confirms automatic startup ceiling48GiB on64GiB hardware (or intentional positive saved limit), separates rawOS/process pressure and checks desired-LOD/create/decode/COW churn alongside changed object residency. No runtime repair claim before this check.
- Verification: Sourcechain initMaxHeapSizeGB→updateMemoryInfo clamps available=min(OSavailable,max(heapcap−processallocation,0)); Darwin getFreeSystemMemory feeds that clamped value into the512MiB low-memory test. DefaultMaxHeapSize64=16.0 has noAlchemy/user override. All5current and5G1 logpairs match max(16384MiB−allocation,0). Current allocation14902.05–16890.92MiB produces0–1481.95MiB headroom and crossescap; G1allocation12722.55–12987.67 leaves3396.33–3661.45MiB. True pre-clamp OS availability was not logged, so system_low is not proof physical64GiB RAM was exhausted. Current14emergency low/overbudget transition actions and ONCE milestones700→4500 prove at least3800finer-than-desired create-list conditions/scaleDown calls during274s; G1bothwarning patterns0. Redundant-load skipping and queued/coalesced/declined scaleDown paths mean these are not3800fresh uploads/GPU copies.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before any repair after independent verification of shipped/alchemy/user numeric settings, exact source chain and10log pairs. Process-cap pressure is confirmed; the whole FPS delta, actual pre-clamp OS pressure, G2retention contribution and exact repeated-asset/copy counts remain unproved. This inherited policy conflict is material to the promised Apple UMA architecture and must be considered with decode/publication/retirement work. No runtime setting was changed.

- 2026-11-14: Owner authorizes repair before next run. Source-equivalent safety/user limits must remain; replace legacy default with explicit automatic physical-aware policy and distinguish rawOS/process/native operands. Add regression coverage for the observed16GiB crossing and real pressure/explicit limits; no claim of runtime repair before owner test.


- 2026-11-14: Implemented finite RAM-aware automatic default0 (64→48GiB), preserved explicit positive caps and real OS/process pressure protection; rawOS availability no longer overwritten by process headroom. Memory108assertions/7cases and LLVOCache residency regressions pass; larger cap also changes existing object-retention bounds, now logged. Exact4918fd7/68999 build/static/separate stage and broader checks pass. Owner runtime outcome pending. Evidence: [retest delivery and exact receipts](work/active/apple-first-retest-20261114.md). Owner verification pending; no push.


- 2026-11-14: Retest verifies actual automatic49152MiB cap on65536MiB physical RAM;30complete-bin samples bias1, no OS/process/native low flags or emergency logs. Process allocation11542.9–11746.4MiB would also be non-low under old16384MiB cap, so prior cap crossing not exercised and FPS recovery not isolated to repair. Create-list/COW churn low; LLVOCache factor1 and effective bounds/radii exposed. Keep owner verification pending; no closure or new source fix. Evidence: [completed retest analysis](research/apple-first-retest-profile-2026-11-14.md), private retest-busy-club-profile-20261114-101904 receipts.


## PPD-112 — Earlier direct bulk selection can consume the opaque ICB candidate budget

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex shadow packet selection
- Discovered: 2026-11-14
- Original reference: Native static bulk selection retained from G1, extended by the selective opaque ICB experiment in6bae; original OpenGL has no ICB packet budget. Query/cull/caster ordering and direct rendering must remain preserved.
- Evidence: [Architecture, fast-path and log follow-up audit](research/apple-first-architecture-audit-2026-11-14.md).
- Next action: Preserve diagnosed16k selection edge, but use current small-run distribution for any later bounded packet-policy trial. No eligibility/attempt exists in this retest; do not attribute it to preparation failure or force activation without complete-path evidence.
- Verification: almetalworld.cpp7227–7242 applies cumulative16384-record static bulk budget before7244–7256 extracts runs>=2048 for opaqueICB. Valid preceding16runs×1024 can consume thatbudget and exclude a later2048run before opaque eligibility is considered. renderSpotShadow scene validation has no16384total-scene cap; the cap belongs to prepared packets. This is a source-visible selection/lost-opportunity edge, not a visual defect, a measured policy-speedup or proof of why the actual zero-ICB capture occurred.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before any repair. Actual runtime support/allocation and enabled caller are confirmed, but eligibility/run histograms and optional opaque fallback reasons are unlogged. Those missing observations must remain unknown; the source-visible budget edge must not be substituted as the captured cause.

- 2026-11-14: Retest changes will expose selection/oversize/threshold/fallback reasons. Do not forceactivation or lower thresholds. Fix this conditional edge only if independent bounded-packet policy is established and covered, otherwise retain explicit diagnostic evidence for the next run.


- 2026-11-14: Native fixture reproduces16×1024 preceding records excluding later2048 opaque run at16384 cumulative direct budget while every caster/query remains correct. New census exposes small/budget/oversize/state/query causes without threshold or policy change. No captured-club occurrence inferred. Evidence: [retest delivery and exact receipts](work/active/apple-first-retest-20261114.md). Owner verification pending; no push.


- 2026-11-14: New retest census proves no eligible2048draw run:36,720main/8,488probe active/completed views all no_candidate; selected/attempted/prepared/executed/fallback/budget/oversize deltas0. Earlier16k edge did not occur here. This resolves current zero execution as eligibility, not preparation failure; original68995 lacked these counters, so its exact cause remains unobserved. Keep bounded policy and conditional edge history; no threshold lowering or code repair. Evidence: [completed retest analysis](research/apple-first-retest-profile-2026-11-14.md), private retest-busy-club-profile-20261114-101904 receipts.


## PPD-113 — Native shadow packet wrapper allocation can leave a slot leased

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native opaque preparation diagnostics
- Discovered: 2026-11-14
- Original reference: Native indirect/direct command helper at6bae; no originalGL packet wrapper equivalent. Preserve actual command-completion and packet-lease ownership during optional direct fallback.
- Evidence: [Retest readiness implementation ledger](work/active/apple-first-retest-20261114.md).
- Next action: Owner retests exact4918fd7/68999. Inner-owner unwind repair and actual completion-gated native tests pass; ordinary runtime acceptance and any real OOM occurrence remain distinct.
- Verification: prepare sets slot.leased=true and transfers slot into Batch::Impl before allocating the final public Batch wrapper. Existing lease release occurs only in Batch::~Batch. If final wrapper new throws, local Impl destruction does not clear leased, stranding the pool slot. The direct packet route has the same ownership shape. This is a source-confirmed allocation-failure hazard; no actualOOM/capture incident is proved.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before repair after root/source peer inspection during typed failure diagnostics. The retest logging change must not turn recoverable host allocation failure into persistent pool loss. Failed captures and all earlier lifetime tests remain retained; no runtime memory fault injection or viewer launch.


- 2026-11-14: First focused native attempt preserved: wrapper allocation test passes, but combined opaque parity case fails renderSpotShadow on rigged family1 under API/shader validation; source owner investigating before candidate delivery. No defect closure from build/review. See retest ledger and focused-correctness-1.json.

- 2026-11-14: Moved CPU lease release into Batch::Impl destructor; final wrapper allocation failure now unwinds ownership. Native tests exercise direct+ICB throw, no reuse behind pending token, then actual completion/reuse and rendered pixels. Focused633assertions/3cases and final combined suite pass; failure success counters advance after wrapper allocation. No actual prior OOM attribution or owner closure. Evidence: [retest delivery and exact receipts](work/active/apple-first-retest-20261114.md). Owner verification pending; no push.


## PPD-114 — Optional rigged opaque data-publication failure aborts a shadow view

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native opaque preparation diagnostics
- Discovered: 2026-11-14
- Original reference: Native6bae selectiveopaque route is an optional optimization over original/G1 direct caster rendering. Static optional preparation already preserves scalar membership on publication failure.
- Evidence: [Retest readiness implementation ledger](work/active/apple-first-retest-20261114.md).
- Next action: Owner retests exact4918fd7/68999 shadow/caster/query behavior. Optional static and rigged typed preparation/publication-failure parity tests pass; required full-indirect failure remains hard.
- Verification: In the rigged extended preparation branch, publishShadowData failure returns false/unwinds the view even when only opaque_indirect is selected. Static optional preparation returns null to its caller, clears selectedruns and continues direct rendering. This source-confirmed asymmetry violates the selected path's optional contract; no actual prior capture failure is attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before repair after root/agent source review. Needed for meaningful failure diagnostics and feature-preserving fallback, not a claim the previous zero-execution capture was caused by publication failure. No runtime launch or capture.

- 2026-11-14: First focused native parity attempt fails at rigged family1 renderSpotShadow before full expected result. Failed receipt retained; investigate actual error/source before repairing or relaxing tests. Static/gate and wrapper cases pass, not owner runtime acceptance.


- 2026-11-14: Optional rigged data publication now records typed fallback, clears stale error/selected runs and preserves direct rendering; static parity remains. Both families tested for six preparation reasons, publication failure, exact depth/query results and one-shot recovery. Focused633/3 and final suite pass after separately registered PPD115 fixture correction. No past-profile causal claim. Evidence: [retest delivery and exact receipts](work/active/apple-first-retest-20261114.md). Owner verification pending; no push.


## PPD-115 — New rigged fallback fixture omits the current pose palette

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex opaque failure regression integration
- Discovered: 2026-11-14
- Original reference: The immutable source prototype intentionally clears per-pose skin palette pointers; production preparation attaches the current owned palette. This is a new diagnostic test fixture defect, not proof of a production caster failure.
- Evidence: [Retest readiness ledger](work/active/apple-first-retest-20261114.md).
- Next action: Retain corrected current-pose fixture and failed first receipt; owner acceptance of retest candidate pending. Production skin validation was not relaxed.
- Verification: First focused filter exits42,3cases2pass1fail. Family1 baseline render fails before fault injection because weight_offset48 has no palette. Native source validation correctly rejects incomplete skinning input. Failed focused-correctness-1-opaque.log is retained locally.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before fixture correction; no production validation relaxation, new viewer launch or prior-profile attribution.


- 2026-11-14: Corrected test-only current owned110-joint identity palette attachment, retaining all depth/query/fallback assertions and adding error INFO. Second focused native filter passes633assertions/3cases; final suite passes. First failure retained, no production source workaround. Evidence: [retest delivery and exact receipts](work/active/apple-first-retest-20261114.md). Owner verification pending; no push.


## PPD-116 — New authored-mip publication fixture disables the required creation policy

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native publication regression integration
- Discovered: 2026-11-14
- Original reference: Authored multi-level native texture creation requires mipmaps enabled, whereas later partial writes require generation disabled. Public setUseMipMaps changes future policy without deleting already authored native levels.
- Evidence: [Retest readiness ledger](work/active/apple-first-retest-20261114.md).
- Next action: Retain corrected public-policy fixture with four-mip340-byte COW and old/new lower-mip parity coverage; owner candidate acceptance pending. Production API requirements unchanged.
- Verification: First focused native generation filter exits42,8cases7pass1fail. createMetalTexture rejects the new fixture at318 because mUseMipMaps is false. This is rejected API input, not observed native allocation failure. Failed focused-correctness-1-generation.log is retained locally.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before fixture correction. Do not weaken production texture validation or infer runtime residency performance from synthetic correctness fixtures.

- 2026-11-14: Corrected test-only creation policy to mipmaps=true, then existing public setUseMipMaps(false) before partial update. Preserved4mips/340logical bytes, old lease pixels, current level0 and successful/declined downscale checks; added current lower-mip parity. Second native generation filter481assertions/8cases and final suite pass. First rejection retained; no production API relaxation. Evidence: [retest delivery and exact receipts](work/active/apple-first-retest-20261114.md). Owner verification pending; no push.


## PPD-117 — Raw-image live-count statistic becomes negative during retest

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex retest instrumentation audit
- Discovered: 2026-11-14
- Original reference: Original1eb44bbd and current4918fd7 LLImageRaw(const U8*,U16,U16,S8) omit the sRawImageCount increment while the destructor always decrements. Other constructors increment; this inherited statistic is not a memory-pressure control operand.
- Evidence: [Retest checkpoint and capture](work/active/apple-first-retest-20261114.md).
- Next action: Retain the observed negative statistic and map all constructor/destructor count paths before a separate bounded bookkeeping repair. Do not interpret it as negative memory, allocation failure, memory corruption or a performance cause; no source fix during the captured experiment.
- Verification: Exact68999 five-minute retest numeric publication logs contain26/30 negative raw_images samples: first negative10:19:44UTC −12, minimum−21, last−20. Root inspects llimage.cpp constructor823–830/destructor852–857; constructor copies allocated pixels without incrementing, destructor decrements. Uses are statistics/UI/new telemetry, not the texture pressure governor. This is newly exposed inherited accounting, not proof of native COW/resource corruption.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered during read-only capture analysis before any repair. Private evidence retest-busy-club-profile-20261114-101904/memory-analysis/memory-summary.json. No renderer/policy/code change or push; raw count excluded from memory occupancy conclusions.

- 2026-11-14: Root independently confirms omitted increment in original1eb44bbd and current LLImageRaw const-pixel constructor, unconditional decrement in destructor; capture count remains negative26/30samples (min−21). Stat consumers only; not a pressure operand or GPU/memory-corruption explanation. Retain for bounded bookkeeping repair with constructor/lifetime coverage; no fix mixed into capture. Evidence: [completed retest analysis](research/apple-first-retest-profile-2026-11-14.md), private retest-busy-club-profile-20261114-101904 receipts.


## PPD-118 — A replenished downscale queue can starve fetched texture creation

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex whole-engine streaming admission integration
- Discovered: 2026-11-14
- Original reference: Existing updateImagesCreateTextures drains queued pressure downscales first and may return BudgetExhausted before serving creates. Preserve original raw/callback ordering and minimum useful progress across streaming stages; do not change desired LOD or hide pressure work.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md).
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Read-only streaming audit finds the downscale-first BudgetExhausted return can repeat under a continually replenished pressure queue, leaving creates unserved. This is a conditional source hazard, not a proved cause in the68995/68999 captured intervals. Agent reports fairness change while integration is in progress; no test/runtime acceptance inferred.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered from the streaming ownership audit during implementation before integrated acceptance. Proposed policy shares the slice when both queues wait and still permits a create admission attempt; native in-flight and true pressure safety remain. No source-quality/LOD target reduction or prior-profile causal claim.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-119 — New scene working-set admission can evict a newer source or undercount a duplicate

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex shared source residency integration
- Discovered: 2026-11-14
- Original reference: New ALNativeSourceWorkingSet native publication policy; original GL has no detached producer publication cache. Preserve current source and actual-reader ownership under stale/invalid input, and account each immutable native allocation consistently.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md).
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Peer source review finds retain removes the current entry before validating an older serial; removal can clear the current State payload. The candidate resource loop skips same-control-block duplicates before checking capacity, so100/200byte declarations can be silently counted100. New code is not in the profiled68999 app; no runtime incident is attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before repair from independent pre-integration review. Initial simple CPU cases do not cover these adversarial admission inputs. No app launch/profile/push.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-120 — Reentrant source destruction can make State publication report stale success

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex shared publication lifecycle integration
- Discovered: 2026-11-14
- Original reference: New ALNativePublication::State native immutable-input boundary; original GL has no corresponding shared payload seal. Publication must remain dirty/failed if destruction invalidates the source during replacement.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md).
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: State::publish checks revision, replaces mPublication (which can destroy the prior payload), then sets mDirty=false. A prior payload deleter can reenter invalidate, resetting the new payload/incrementing revision; outer publish then reports true with an empty clean state. This is a pre-integration source edge, not a bug observed in68999.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before repair after independent review. Initial common policies compile and basic tests do not establish this reentrant lifecycle contract. No new viewer/capture/push.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-121 — Shared decoder backpressure omits in-flight upload byte debt

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex whole-engine streaming admission
- Discovered: 2026-11-14
- Original reference: New ALNativeFrameBudget downstream admission; original accepted decode responders and active jobs must survive pressure changes. Byte debt and request count are separate resource operands.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md).
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Source review finds decodeCeiling considers in-flight upload count, queued logical input bytes and pending publications, but not upload_bytes_in_flight. An individually admitted large upload can occupy the byte lane while request count remains low. Pre-integration policy gap, not an observed68999 incident.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before repair after streaming owner review; common files were frozen for root checks. No source/client runtime acceptance or causal performance claim.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-122 — HTTP 4xx warning demotion checks the internal flag instead of the HTTP code

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex warning audit / LLCore HTTP logging
- Discovered: 2026-11-14
- Original reference: Compiled4918fd7 indra/llcorehttp/_httppolicy.cpp406–407 and httpcommon.h263–270/307–311. HTTP encodes (code,success/error flag), so getType carries the code and getStatus the flag.
- Evidence: [Whole-session warning audit](research/retest-whole-session-warnings-2026-11-14.md).
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Full session75HTTP warnings:47HTTP403,2HTTP404,26Easy_28. Intended4xx demotion calls getStatus()>=400, so it never matches ordinary HTTP error flags. Actual delivery failures remain real; their causes are unproved.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered in isolated documentation audit before repair. Compiled source4918fd7, private retest-busy-club-profile-20261114-101904 warning audit artifacts. No source fix, build, live capture or push. Pending coordinated local record integration without changing the active implementation checkout.


- 2026-11-14: Coordinated import from codex/retest-warning-audit-20261114 branch audit. Retained branch-qualified alias `codex/retest-warning-audit-20261114:PPD-118` maps to this canonical record; original audit history remains on that branch. No conflicting canonical ID was renumbered or removed. Source fixes are owned in the separate warning-fix worktree; root integration/test acceptance remains pending.




- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-123 — Live SMAA readers request an undeclared RenderSMAAUseSample control

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex warning audit / Native graphics setting schema
- Discovered: 2026-11-14
- Original reference: Compiled4918fd7 pipeline/resource-plan live RenderSMAAUseSample readers and shipped setting declarations. Missing-control path returnsfalse.
- Evidence: [Whole-session warning audit](research/retest-whole-session-warnings-2026-11-14.md).
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Six missing-control warnings for RenderSMAAUseSample during startup; static readers exist and the compiled setting declaration is absent. Resulting AA effect remains unproved.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered in isolated documentation audit before repair. Compiled source4918fd7, private retest-busy-club-profile-20261114-101904 warning audit artifacts. No source fix, build, live capture or push. Pending coordinated local record integration without changing the active implementation checkout.


- 2026-11-14: Coordinated import from codex/retest-warning-audit-20261114 branch audit. Retained branch-qualified alias `codex/retest-warning-audit-20261114:PPD-119` maps to this canonical record; original audit history remains on that branch. No conflicting canonical ID was renumbered or removed. Source fixes are owned in the separate warning-fix worktree; root integration/test acceptance remains pending.




- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-124 — Picks context-menu enable callbacks have no Pick.Enable registration

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex warning audit / Picks UI callback integration
- Discovered: 2026-11-14
- Original reference: Compiled4918fd7 menu_picks.xml enable bindings and LLUICtrl::initEnableCallback failure path. Literal Pick.Enable registration not found.
- Evidence: [Whole-session warning audit](research/retest-whole-session-warnings-2026-11-14.md).
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Five warnings cover Teleport, Info, Edit, Map and Delete enable rules. The enable callback lookup fails; commit handlers are separate, so five broken actions are not established.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered in isolated documentation audit before repair. Compiled source4918fd7, private retest-busy-club-profile-20261114-101904 warning audit artifacts. No source fix, build, live capture or push. Pending coordinated local record integration without changing the active implementation checkout.


- 2026-11-14: Coordinated import from codex/retest-warning-audit-20261114 branch audit. Retained branch-qualified alias `codex/retest-warning-audit-20261114:PPD-120` maps to this canonical record; original audit history remains on that branch. No conflicting canonical ID was renumbered or removed. Source fixes are owned in the separate warning-fix worktree; root integration/test acceptance remains pending.




- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-125 — Hero-probe face transactions are rejected during teleport

- Status: investigating
- Confirmation: reproduced
- Owner: Codex warning audit / Native mirror transition consistency
- Discovered: 2026-11-14
- Original reference: Compiled4918fd7 pipeline.cpp12241 PBR MASK/pass guard,15761 terrain compound guard, and hero publication/warm-up branches. Different from unproved PPD088 SH-ticket and PPD103 admission attribution.
- Evidence: [Whole-session warning audit](research/retest-whole-session-warnings-2026-11-14.md).
- Next action: Trace transition input/material/terrain revisions and mirror publication recovery. Preserve fail-closed guards until the inconsistent state or lifecycle cause is known; do not weaken validation to hide warnings.
- Verification: Five actual post-capture rejected face transactions at10:25:20–22:one PBR MASK/pass mismatch and four terrain compound rejects. Teleport starts18/arrives20–21; quit51/cleanup53, so these are transition, not shutdown. First successful hero publication20 does not prove final recovery or a broken mirror in the earlier club with unproved owner eligibility.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered in isolated documentation audit before repair. Compiled source4918fd7, private retest-busy-club-profile-20261114-101904 warning audit artifacts. No source fix, build, live capture or push. Pending coordinated local record integration without changing the active implementation checkout.


- 2026-11-14: Coordinated import from codex/retest-warning-audit-20261114 branch audit. Retained branch-qualified alias `codex/retest-warning-audit-20261114:PPD-121` maps to this canonical record; original audit history remains on that branch. No conflicting canonical ID was renumbered or removed. Source fixes are owned in the separate warning-fix worktree; root integration/test acceptance remains pending.




- 2026-11-14 final candidate handoff: Combined69008 builds/contained native checks pass, but this incident attribution remains unproved; no live teleport/shutdown retest or owner closure. Keep investigating.
- 2026-11-15: Owner-authorized69065trial snapshot(source14837e631a8e96492bb3037de24dd1c1a8030ac5,08:44:55–08:48:29UTC) adds27hero/mirror warm-up/incomplete warnings. Six-face radiance publication succeeds08:46:32(line1563), then aborted faces recur at1590/2308/2350 with first_incomplete_line15588, mapping to exact-source terrain compound preflight. Individual predicate and causal identity with earlier teleport case remain unproved; do not claim permanent mirror failure/recovery or weaken guards. Private snapshot hash37a69080e41dea552883f663e35af3be825b37db0fd713dc97d8c2435d15b8d3 verified; no new launch/profile. See direct-cohort ledger for retained evidence path.

- 2026-11-15 source audit of exact69065 anded235 confirms classic terrain compound preflight precedes all texture/map/material checks. GroupSources eligibility is PARTITION_VOLUME and cannot convert terrain face pools; shared timing/allocation interaction remains unproved. Successfulsixface publication preceded later aborts; no permanent mirror-failure claim. First predicate is unobserved; bounded first-reason diagnostics are next, preserving prepare-once evaluation and incomplete/clear/cancel/retry. No guard relaxation or stale resource recovery authorized by this evidence.

- 2026-11-15 69085 owner profile imported: exact6af/40700 measurement09:57:14.760–10:02:16.447UTC has no hero/mirror warnings; earlier startup snapshot has20warnings with renderer0/GBuffer0/line0, distinct from69065terrain guard. Absence of later warnings does not prove mirror recovery. New typed classic preflight diagnostics and actual ordered-guard tests integrated for next source; original root cause remains unknown.

- 2026-11-16 owner69151 whole-session warnings confirm classic-terrain missing_buffer during a second landmark teleport07:11:32–35, after the requested busy recording stopped. Exact2038 helper returns before native preparation and clears the hero-face transaction. Original1eb44 lldrawpoolterrain.cpp353 calls LLFace::renderIndexed, whose llface.cpp1969–1975 skips a face with no vertex buffer. This is a source-confirmed transition-handling difference: no GPU buffer exists to render in either path. Register before narrow parity repair. Omit only an otherwise qualified face with a null geometry buffer, preserving every other owner/preparation/range/material guard and retaining already-captured draws. Add explicit skipped-unready census and actual-body ready/null/mixed/native-prepare-failure tests. Successful publication between warnings does not prove final recovery; owner runtime acceptance remains pending.

- 2026-11-16 narrow parity repair integrated from d9eccdef2c: ordered missing_buffer skips only the current face and increments a capture-owned cumulative census; preceding pointer failures and every nonnull native-publication/range/material guard retain incomplete/clear behavior. Previously accepted draws remain owned. Production diagnostics now print unready_buffer_skips even without failures. Three new actual-body cases exercise protected phase memory, ready/null/ready progress and nullskip followed by real missing-publisher failure; no viewer/runtime recovery claim. Build/native execution pending on next frozen source.

- 2026-11-16 repair verification at frozen43a44724c7/build26.4.0.69162 (89): doctor/configure/build exits0, build38.247s. Actual production preflight include passes213assertions/7cases with protected-memory and real missing-publisher controls, including the three new omission/failure progression cases. Exactapp/ARM64/signature/import/native-resource checks and strict3598files/0blockers pass. Receipts frozen-source-build-89.json and terrain-validation-89.json retain exact argv/source/log hashes. Earlier88 full34renderer proof remains separate, not relabelled89. No viewer launch, owner transition retest or final mirror-recovery evidence; record remains open.

## PPD-126 — Shutdown reports one remaining volume and LOD reference

- Status: investigating
- Confirmation: reproduced
- Owner: Codex warning audit / Scene volume ownership cleanup
- Discovered: 2026-11-14
- Original reference: Compiled4918fd7 LLVolumeLODGroup cleanup and LLAppViewer volume-manager cleanup warnings.
- Evidence: [Whole-session warning audit](research/retest-whole-session-warnings-2026-11-14.md).
- Next action: Identify the remaining reference owner and expected shutdown ordering before changing lifetimes. Distinguish it from clearDeleteList which actively drains509 queued workers; no live GPU leak is established.
- Verification: One volume group/LOD3reference remains at10:25:53 cleanup and produces four warning lines. No evidence attributes it to source cache, a live leak or frame-time degradation.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered in isolated documentation audit before repair. Compiled source4918fd7, private retest-busy-club-profile-20261114-101904 warning audit artifacts. No source fix, build, live capture or push. Pending coordinated local record integration without changing the active implementation checkout.


- 2026-11-14: Coordinated import from codex/retest-warning-audit-20261114 branch audit. Retained branch-qualified alias `codex/retest-warning-audit-20261114:PPD-122` maps to this canonical record; original audit history remains on that branch. No conflicting canonical ID was renumbered or removed. Source fixes are owned in the separate warning-fix worktree; root integration/test acceptance remains pending.




- 2026-11-14 final candidate handoff: Combined69008 builds/contained native checks pass, but this incident attribution remains unproved; no live teleport/shutdown retest or owner closure. Keep investigating.
- 2026-11-16 fresh69172/source350b803597 normal shutdown repeats one remainingvolume/LOD1reference(fourwarninglines, oneincident). FullCPU/GPU scene captures close normally, Goodbye12:20:37. Retain ownership anomaly; no permanentGPUleak or frame-ratecause proved. Private fresh analysis-warnings-review/summary retain source/log evidence.

## PPD-127 — Startup XUI parameter parsing emits an unresolved warning

- Status: ready_for_owner
- Confirmation: reported
- Owner: Codex warning audit / XUI schema and parser integration
- Discovered: 2026-11-14
- Original reference: Compiled4918fd7 llinitparam.cpp115 parserWarning callsite; exact parameter effect remains unknown.
- Evidence: [Whole-session warning audit](research/retest-whole-session-warnings-2026-11-14.md).
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: One generic parser warning occurs outside measurement. It is recorded separately from five known Picks enable failures; resulting UI behavior and root cause remain unverified.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered in isolated documentation audit before repair. Compiled source4918fd7, private retest-busy-club-profile-20261114-101904 warning audit artifacts. No source fix, build, live capture or push. Pending coordinated local record integration without changing the active implementation checkout.

- 2026-11-14: Coordinated import from codex/retest-warning-audit-20261114 branch audit. Retained branch-qualified alias `codex/retest-warning-audit-20261114:PPD-123` maps to this canonical record; original audit history remains on that branch. No conflicting canonical ID was renumbered or removed. Source fixes are owned in the separate warning-fix worktree; root integration/test acceptance remains pending.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-128 — Accelerated media checks COW leases before invalidating publisher ownership

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex cross-engine publication integration
- Discovered: 2026-11-14
- Original reference: LLViewerMediaImpl::updateAcceleratedTexture reads the current native destination and tests image leases before native blitToMetal; successful replacement/adopt or in-place/mark follows commit. Preserve original media frame/readers/failure and playing-state semantics; native shared source publication is new.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md).
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Source audit locates active llviewermedia.cpp3331–3365 and llcefaccelinterop.cpp158–195. Post-write mark is too late to release cache-only publication ownership before the COW decision, forcing avoidable replacement when no actual reader requires it. No actual prior media/COW profile cause is established; current retest COW counters were zero.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before scoped hook repair. Fresh bump/terrain/preview replacements are distinct safe paths; unused LLRenderTarget external attachment API has no production caller and is not silently redesigned. No viewer launch/profile/push.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-129 — Attachment pose cache can mark an incomplete palette current before allocation finishes

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex immutable pose publication
- Discovered: 2026-11-14
- Original reference: Existing LLVOAvatar::updateSkinInfoMatrixPalette writes entry.mFrame before vector resize/packing; preserve the original exact matrix and frame-cache result, but publish cache validity only after the complete tuple exists.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md).
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Pose producer review identifies early mFrame assignment before allocation/packing. An exception can leave an entry apparently current with incomplete storage; new pose/lifecycle stamps must not repeat that ordering. Source hazard, not an actual prior runtime exception attribution.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before validity-stamp repair during immutable body/attachment publication integration. Body snapshot creation already seals only complete records; keep attachment cache consistent with that transaction rule. No viewer/profile/push.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-130 — New direct-shared void setters can leave stale native data accepted after both fallbacks fail

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex direct shared-write integration
- Discovered: 2026-11-14
- Original reference: New opt-in LLVB direct shared path for fresh immediate/UI storage; ordinary CPU-shadow producers retain existing behavior. A requested write must not silently leave the old store certified current after an unrecoverable allocation failure.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md).
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Independent source review finds prepareNativeSharedWrite returnsfalse on double allocation failure; flush_vbo logs/returns before mVerticesWritten, allowing prepareMetalBuffers/hasPublishedMetalVertexBuffer to accept the old initialized generation despite lost void-setter edit. Existing fallback case exercises native-unavailable plus successful shadow materialization, not this arm. New code is not in68999 and no runtime failure is attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before repair from cross-agent ownership review. Exact layout/normal reader-retirement paths appear sound; double failure needs explicit publication-failure state rather than invented saved edit bytes. No app launch/profile/push.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-131 — Empty direct-shared maps can rotate and copy an unchanged native store

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex direct shared-write integration
- Discovered: 2026-11-14
- Original reference: Existing zero-count LLVB mapping is an empty declared edit; no native publication/copy is expected. New opt-in direct-shared producer must preserve that range contract and backing reader ownership.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md).
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Independent source review finds mapVertexBuffer calls prepareNativeSharedWrite before count>0, so an empty map can clone/preserve the entire borrowed generation, mark it uninitialized and republish without a declared edit. Existing empty-range tests use the unchanged ordinary CPU-shadow path. No actual68999/runtime incident is attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before repair with PPD130 as a separate direct-policy edge. Normal exact layout/alias/reader paths remain; new direct policy is not accepted from source review alone. No viewer/profile/push.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-132 — Hero removal invalidates its active vector traversal

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex whole-engine probe integration
- Discovered: 2026-11-14
- Original reference: Existing LLHeroProbeManager::update maintains scene ownership before optional native reflection work; erasing an element must not invalidate the next traversal step.
- Evidence: Independent probe-owner source audit during [whole-engine integration](work/active/apple-first-engine-integration-20261114.md).
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: update range-iterates mHeroVOList while unregisterViewerObject erases that same vector. This is a concrete C++ iterator invalidation hazard; no captured crash or remaining-volume attribution is claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before narrow iterator-safe correction by the exclusively assigned probe owner. No viewer launch/profile/push.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-133 — Special shadow resources must join their actual command owner

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native source integration
- Discovered: 2026-11-14
- Original reference: Existing synchronous special terrain/tree/body captures are retained. New shared source/job architecture requires exact wrapper generations to follow the shadow packet command owner, independently of the next main G-buffer capture.
- Evidence: Independent resource-ownership review during [whole-engine integration](work/active/apple-first-engine-integration-20261114.md).
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Main world_resources and shadow_source_lease are separate owners. Main-only resource retention cannot establish special-shadow ownership, and generated terrain height was a raw wrapper field. This is an integration coverage gap, not an attributed prior viewer crash.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before explicit special-shadow packet ownership integration. No viewer launch/profile/push.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-134 — Animated UV native test used reverse-Z depth equal to its clear value

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex native preparation validation
- Discovered: 2026-11-14
- Original reference: Forward/reverse-Z shadow conversion and animated cutout pixels must remain equivalent between direct and prepared inputs.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Private apple-first-engine-integration-20261114/native-correctness-1-shadow-uv.log; actual exit42. All direct/prepared pixel comparisons pass; four reverse-Z combinations fail animation-change/clear assertions.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Fixture clip Z0 is unchanged by reverse-Z shadow conversion and equals clear0. Both alpha states produce the same all-zero output despite correct UV parity. Production shader/UV changes are not justified by this fixture failure.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered actual failure before test-only correction. Previous failed evidence retained.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-135 — Fetched terrain binding owners do not pin exact native generations

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex scene publication integration
- Discovered: 2026-11-14
- Original reference: Existing fetched fallback selection remains owner-controlled. Prepared native scene packets require exact resource generations independently of mutable LLImageGL facade ownership.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Independent source review of ALMetalFetchedTextureBinding::nativeTexture during whole-engine integration.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: ALMetalTerrainFetchedBindings retains LLTexture/LLImageGL owners, while nativeTexture dynamically reads current backing. Retaining the requested image alone misses the actual resolved default/null fallback. No runtime regression or leak attribution is claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before exact resolved-native lease capture. Existing synchronous binding behavior remains authoritative.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-136 — New PBR publication copied a fourth float from a three-component emissive color

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex scene publication integration
- Discovered: 2026-11-14
- Original reference: Original PBR collector copies sizeof(material->mEmissiveColor.mV), preserving the destination fourth lane default.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Independent source review of new shared PBR factory; LLGLTFMaterial::mEmissiveColor is LLColor3.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: New destination emissive_color has four floats; source mV has three. Copying destination size reads beyond the source subobject. No prior viewer rendering failure is attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before restoring the original exact copy size. No viewer launch/profile/push.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-137 — New packet resource deduplication must roll back failed retention

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex scene packet integration
- Discovered: 2026-11-14
- Original reference: Existing shadow source/palette retention rolls back an inserted identity if its strong-owner vector insertion fails.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Independent review of new WorldResourceLease::retain and ShadowSourceLease::retainResource.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Inserting into identity set before vector push can leave the set entry behind when push throws. Subsequent retention would otherwise skip the missing owner. No runtime allocation failure is attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before matching established retention unwind semantics. No viewer launch/profile/push.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-138 — Shared shadow factory rejected GLTF blended alpha coverage

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex scene publication integration
- Discovered: 2026-11-14
- Original reference: PASS_ALPHA uses legacy_blend as the sweep policy; the actual GLTF capture intentionally emits pbr_blend for scalar/indexed GLTF materials.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Independent source comparison of captureMetalShadowScene and captureMetalSpotShadowPBRBatch.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: New factory compared prototype.coverage with the unconverted sweep policy and would discard a shadow view with GLTF blended casters. This new code has not been delivered/launched.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before preserving original GLTF alpha coverage conversion in the shared factory.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-139 — Qualified bounds must cover actual referenced indices

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native visibility integration
- Discovered: 2026-11-14
- Original reference: Native direct draws consume actual index bytes; LLVB validateRange checks declared vertex/index capacities but does not inspect each index value.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Source audit of the new bounds qualification against LLVertexBuffer::validateRange.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Trusting only declared mStart/mEnd would not independently prove the bound encloses the submitted indexed geometry. Native visibility remains off pending cost gates; no displayed omission is attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before strengthening conservative source qualification.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-140 — Realtime probe continuation could admit successive fresh groups on one credit

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex shared probe scheduling
- Discovered: 2026-11-14
- Original reference: Existing scheduled12-face and realtime irradiance/radiance sequences retain their configured work. A new optional group requires a new admission after the prior coherent transaction finishes.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Independent read-only probe scheduling review during whole-engine integration.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Realtime radiance completion can fall through to new oldest-probe selection with the previous retained credit, chaining fresh groups around admission. New integration is not in the profiled viewer.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before narrowing group continuation to its actual transaction boundary.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-141 — Forward and impostor consumers need shared-source completion ownership

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex shared scene integration
- Discovered: 2026-11-14
- Original reference: Prepared G-buffer/shadow consumers retain immutable source input owners through actual command completion; forward and impostor phases consuming the same shared sources require the same boundary.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Independent source review of renderOpaquePostDeferred/compositeImpostorPostDeferred and their pipeline scene construction.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Cached shared sources skip the old raw-getter read-stamp path. State invalidation plus next-capture aggregate reset must not permit native in-place updates while forward commands are pending. Current prepared/shadow commands already pin their packet correctly; forward paths did not.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before extending exact command ownership to every consumer of the new shared source. Initial dirty integration build is diagnostic only and will be followed by a frozen-source build.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-142 — Realtime probe progress could starve pending scheduled refreshes

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex shared ordinary probe scheduling
- Discovered: 2026-11-14
- Original reference: Realtime and scheduled probe work share bounded capacity, while both must retain bounded progress under main/memory pressure.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Independent control-flow audit after PPD140 credit rollover repair.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Realtime work runs first and can consume/reset the manager's one shared deferral age every eight-frame progress window. After realtime radiance retires its credit, the scheduled request starts at age0 again and can be denied indefinitely. This integration is not in the profiled app.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before finite-turn fairness correction. Do not restore unbounded old-credit rollover to avoid starvation.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-143 — Failed world preparation/encoding can leave a borrowed batch committable

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native world transaction integration
- Discovered: 2026-11-14
- Original reference: A failed constituent pass must invalidate its borrowed outer command transaction. Existing actual main/hero callers explicitly cancel failed transactions.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Independent query/cancel audit of frozen snapshot1; qualified API concern rather than an observed viewer publication failure.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Some renderInternal early returns after acquireCommandBuffer do not call finishCommandBuffer(false). Existing owner cancellation prevents an established live corruption counterexample; newly added preparation/retention failures should not rely on every future caller remembering that cancellation.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered qualified API-level concern, retaining actual caller protections and absence of observed corruption/performance attribution. No viewer launch/profile/push.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-144 — New pose helper omitted the SIMD math include prerequisite

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex body pose integration
- Discovered: 2026-11-14
- Original reference: llmatrix4a/llvector4a require the llmath SIMD declarations; a new reusable pose helper must include its actual prerequisites independently of the viewer PCH.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). integration-viewer-build-1 exit1 after649.919s, alavatarpose_test.cpp unknown LLSimdScalar/LLVector4Logical types.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Viewer translation units compiled through the PCH, but the contained test exposed the missing header prerequisite. The failed build/changed-source manifest is retained and is not a delivery binary.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered actual integration build failure before header correction. No viewer launch/profile/push.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-145 — Small prepared-world tickets add avoidable mechanism overhead

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex measured world preparation integration
- Discovered: 2026-11-14
- Original reference: Same native direct/prepared shaders and ordered input data; preparation/jobs must earn their scheduling/allocation cost while shared immutable source ownership remains intact.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). complete-path-costs-1:16/128/1024 mixed draws, direct/prepared native completion included, no competing compiler or validation layers.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: At128 draws serial ticket median0.563750ms versus direct0.541479ms; two-job16draw tail1.10346ms versus direct0.276292ms. At1024, two-job median2.03013ms versus direct2.07631ms and tail2.36046ms versus2.53175ms. Isolated mechanism data, not live viewer FPS.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered measured small-work overhead before selecting execution policy inside the same owned-source architecture. GPU visibility also loses all six contained cost rows and remains OFF.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-146 — Disabled GPU visibility should not scan every newly published source index range

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex persistent source visibility integration
- Discovered: 2026-11-14
- Original reference: Native visibility is a cost-gated optional consumer; ordinary shared source publication must not pay its unused index/position scan.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). New Payload extension qualifies conservative bounds unconditionally, while RenderMetalNativeVisibility defaultsfalse and fails contained cost gates.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Default-off visibility currently leaves a per-dirty-source indexed position scan in the main/probe/shadow factory. This is source-confirmed avoidable work; no live FPS attribution is claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before removing inactive-consumer work while retaining its tested opt-in architecture.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-147 — Visibility cost fixture labeled a periodic mask as an exact offscreen percentage

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native mechanism measurement
- Discovered: 2026-11-14
- Original reference: A stated50percent offscreen fixture should contain exactly half the even draw count; correctness/cost claims must describe the generated workload.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). complete-path-costs-1 and almetal_world_visibility_test.mm's index%100 mask.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: The first16draw50label row actually makes16draws offscreen;128makes78;1024makes524. The observed optional path still loses every row, but these are not exact50percent workloads.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered benchmark reporting/fixture discrepancy before exact half-membership correction. No live performance attribution.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-148 — Prepared world constants counted reuse but rebound duplicate slab offsets

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex prepared world integration
- Discovered: 2026-11-14
- Original reference: Existing native G-buffer reuses the previous400byte common constant slice when equal, including after another family overwrites its binding slot. Alchemy region/agent-space geometry often shares these per-view values.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Source comparison of bind_world_constants plus retained-constant-reuse-census.json from the actual68999 busy-club retest, and corrected complete-path-costs-2 showing4096draw regression.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: New prepared branch memcmp-detects equal WorldSimpleConstants and increments reused_bindings but always binds current record.offset. Direct path rebinds previous_world_slice. This defeats native binding cache reuse and repeatedly addresses duplicate bytes; actual prior club log has substantial main/probe reuse. No live new-architecture FPS attribution is claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before preserving existing Second Life per-view constant reuse; retained4096draw measured loss rather than hiding it by assuming unique object transforms in a generic engine benchmark. Existing prior app was68999; counter evidence is its captured run.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-149 — Prepared slabs still published duplicate common constant bytes

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex prepared world data publication
- Discovered: 2026-11-14
- Original reference: Direct native constant reuse avoids both new bindings and repeated GPU arena publication of identical400byte common constants. Preserve this across worker preparation.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). PPD148 sharing repair passes762assertions/6native cases, but complete-path-costs-3 still shows4096median loss6.61208ms vs6.37077ms. Prepared constants allocation/publication retains one duplicate range per original record.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Rebinding the previous offset fixes binding identity but does not remove duplicate CPU slab/GPU publication bytes. Existing Alchemy per-view common-value reuse is measured in the retained club run; no new app FPS claim follows from a contained fix.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered remaining physical publication redundancy after PPD148, retaining all measured large-view losses. No fabricated unique-transform benchmark or nominal worker-utilization success claim.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-150 — GPU packet ownership unnecessarily held reusable CPU capture storage

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex CPU/native publication lifecycle integration
- Discovered: 2026-11-14
- Original reference: Existing capture clears and reuses batch/query vector capacity after synchronous native encoding. Detached CPU work needs owned immutable arrays until join/encoding, while GPU commands need exact native resource generations and published constant pages afterward.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). WorldViewInputs adopts moved capture vectors and prepared.inputLease retains whole input/constant metadata through GPU completion; next capture must allocate new vector capacity.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Entire CPU view arrays are not read by the GPU after native constant publication/encoding; holding them in the GPU lease defeats previous capture capacity reuse. Source-confirmed unnecessary lifetime/allocation churn, not an attributed new runtime leak or FPS result.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before splitting CPU-ready and native-completion ownership. Unique strong ownership is a backstop; caller must establish joined/quiescent CPU consumers, not race a weak-reader lock.

- 2026-11-14 repair attempt for PPD150: Split native resource ownership and add owner-only CPU storage reclamation. Empty/sky-only inputs may legitimately have a null native resource lease; renderer retention now checks nonnull before retainUntilCompletion, preserving successful empty clear. Actual delayed-GPU and empty prepared rendering tests are required before acceptance.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-151 — Compaction byte assertion omitted real query-uniform publication

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex native publication validation
- Discovered: 2026-11-14
- Original reference: Actual frame-data published bytes include both prepared geometry constants and ordered query constants.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). compaction-cpu-retirement-correctness-1 exit42:11/12cases and1473/1475assertions pass; both failures are8416publishedbytes versus7968preparedspan.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: The448byte difference is four112byte query constant publications. Warming a previous command does not remove command-local query publication. Rendering/lifetime/reclaim assertions pass; this is fixture accounting, not a proven renderer over-upload bug.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered actual failed receipt before correcting the independent accounting baseline.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-152 — Stable material constants were still rebuilt and published for every view

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex persistent native material publication
- Discovered: 2026-11-14
- Original reference: Original architecture publishes stable geometry/material data from dirty owners once; main/probe views consume immutable native resources while camera/pose/UV values stay per-view.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Shared factory caches material prototypes, but direct/prepared G-buffer paths still repack/publish the same material/PBR constant tables on each consuming view.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Material/PBR constant builders depend on stable batch/material inputs; current publication stops at CPU templates. This is a remaining original GPU-publication boundary, not a claimed measured viewer regression.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered remaining stable GPU material-table work before implementation; no shader, texture quality, draw/query order or rendering backend change authorized by this record alone.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-153 — Actual-owner fixture first included native cache inside its frontend namespace

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex native material integration validation
- Discovered: 2026-11-14
- Original reference: Actual native cache/type definitions must be global and linked to the actual renderer, while only viewer-frontier dependencies live in fixture Inputs namespace.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). frozen-source-build-5 exit1, alpipeline_buffer_publication_metal_test.cpp cannot pass Inputs::ALMetalWorldMaterialDataCache to the global native source publisher.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Header first inclusion inside namespace Inputs created an unintended distinct type. Production pipeline includes it globally. No viewer runtime defect is attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered actual build failure before fixture include repair.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-154 — Cost fixture samples never advanced native frame-data ownership

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex mechanism cost validation
- Discovered: 2026-11-14
- Original reference: Actual outer display frames advance ALMetalWorldFrameData generation; completed pages can retire/reuse and frame-local cache entries reset. Multiple views within a frame share that generation.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Existing cost fixture initializes generation1 once, then runs every timed sample/method/count in that same frame; beginFrame source confirms retirement only on generation change.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Hundreds of samples in one protected frame bias arena growth/reuse and method ordering. Earlier contained costs remain retained as provisional same-generation observations; they do not establish steady per-frame performance.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered missing frame lifecycle before cost-fixture correction and a representative main-plus-probe uniform-publication comparison.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-155 — Draw-count-only CPU preparation policy adds work after native material reuse

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex whole-engine cost policy
- Discovered: 2026-11-14
- Original reference: Worker preparation must improve complete native cost and frame latency; unchanged immutable source/input ownership does not require an extra worker for each view.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Corrected complete-path-costs-4 uses one FrameData generation per outer frame. Native material scalar encoding at main1024 plus2probe128 beats adaptive: legacy0.510104ms versus0.562480ms, PBR0.601104ms versus0.648125ms. At4096 aggregate CPU also increases with workers.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Native material mechanism reduces main/probe encoding/publication work, but its now-cheaper scalar path changes the worker crossover. Simple1024 improves in this diagnostic while4096 regresses;12samples do not establish a safe general live-view threshold. Not a claimed viewer FPS regression.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered measured combined mechanism mismatch before policy correction; costs1–3 retained as provisional same-generation observations under PPD154. No nominal utilization gate.



- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.
## PPD-156 — Sealed per-view batches still copy inactive authored material arrays

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex scene source publication
- Discovered: 2026-11-14
- Original reference: A published source shares immutable stable material data across per-view references; a mutable original author may retain earlier references without reaching that sealed snapshot.
- Evidence: [Whole-engine integration ledger](work/active/apple-first-engine-integration-20261114.md). Default ALMetalWorldSimpleBatch copies engaged private optional arrays even when const consumers exclusively use sealed shared snapshots.
- Next action: Owner evaluates exact69008 combined candidate in ordinary and busy/loading/moving/teleport use; retain this history and failed receipts. No push until owner agreement and explicit approval.
- Verification: Source-confirmed redundant CPU copy, not a proved viewer performance loss. Do not reset original authored optionals while old producer mutable references can exist.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Independent bounded review found no PPD152 semantic/ABI/lifetime counterexample and identified this remaining stable-source CPU copy. Registered before implementation.

- 2026-11-14 final candidate handoff: Local implementation/test repair included in exactf3a7ac5f58/69008. Required build7,86selectedCTestgroups+10nativecases+3archivepolicies, focused3621/17, strict source and exact staged static/inventory checks pass; see [whole-engine results](research/apple-first-whole-engine-integration-2026-11-14.md). Cost-gated preparations/visibility remain OFF; contained success is not owner/runtime closure.


## PPD-157 — New pending-GPU fixture would reload shaders behind its closed synthetic gate

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native resource-table test integration
- Discovered: 2026-11-14
- Original reference: Native renderer creation/reload has legitimate synchronous initialization uploads; an artificial queue gate must be released before a same-queue wait.
- Evidence: [Persistent scene/command work](work/active/apple-first-persistent-native-scene-20261114.md). Test-author self-review of new resource fixture found reloadShaders before signaling its event gate; production creation commits/waits for uploads. No execution or observed hang.
- Next action: Release/settle artificial pending GPU before shader reload checks; keep sampler/cache changes while pending and separately verify new shader context/output. Run actual native tests after source freeze.
- Verification: Test-only ordering flaw found and corrected during uncommitted fixture development before execution; no production deadlock attribution.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Author reported this pre-execution self-review finding while integrating the new test. Fixture ordering corrected; retained here before any claimed check result.


## PPD-158 — Native group admission must preserve the optional raw-map depth-prepass consumer

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native group membership integration
- Discovered: 2026-11-14
- Original reference: llviewerdisplay.cpp RenderDepthPrePass calls renderObjects(PASS_SIMPLE) after stateSort and before main G-buffer capture. Original1eb44bbd and current retained path enumerate raw render-map draws.
- Evidence: [Persistent scene/command work](work/active/apple-first-persistent-native-scene-20261114.md). Independent consumer audit identifies this unconverted raw PASS_SIMPLE enumeration in the new group-range experiment.
- Next action: Exclude group conversion while RenderDepthPrePass is requested, retaining raw native enumeration; validate guards and exact merged raw/group order before enabling the experiment.
- Verification: Compatibility contract in new uncommitted code, not an observed visual regression. Existing generic world-depth draw path appears inert under native ownership, so no new missing-depth symptom is claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Independent audit found the raw-map consumer; new group guard now excludes the setting. Shadows use their own shadow-flagged stateSort and do not reuse converted main ranges. Tests/build pending.


## PPD-159 — New world-resource helper lacks the encoder's narrow private access grant

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native world-resource integration
- Discovered: 2026-11-14
- Original reference: Existing native shadow/resource helpers access encoder internals through explicit private friendship. Resource declarations must not use the indirect accessor that flushes indexed joining.
- Evidence: [Persistent scene/command work](work/active/apple-first-persistent-native-scene-20261114.md). native-core-diagnostic-build-1 fails new almetalworldresources.mm access to nativeDeviceForIndirect/nativeCommandBufferForIndirect/nativeHandleForResources;3 compiler errors.
- Next action: Add only ALMetalWorldResources to the encoder's existing private friend list, preserving restricted resource declaration access; rebuild and run joining/resource/order fixtures.
- Verification: Actual compiler integration failure in new helper, not a viewer/runtime bug. No successful candidate build claimed from this diagnostic.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered actual diagnostic compilation failure before private friendship repair. Earlier shader compilation/core steps remain diagnostic only; no full app result.
- 2026-11-14: Clean144aa7 full required build0 and clean7422 build2 pass. The narrow friend declaration resolves the actual compiler failure; actual native resource8889assertions/5cases and prior-compiled-library4721/1 pass. No viewer/owner test or push.


## PPD-160 — New group publisher neither refreshes nor independently accounts retained residency

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native group publication integration
- Discovered: 2026-11-14
- Original reference: ALNativeSourceWorkingSet bounds strong publisher retention by count, unique native bytes and age; ordinary hits touch the corresponding entry. View/GPU reader leases are separate.
- Evidence: [Persistent scene/command work](work/active/apple-first-persistent-native-scene-20261114.md). New group hits bypass per-source touch, while source max_age defaults2. Child eviction invalidates the group, and a transient source can then be placed into an unaccounted group publisher after its child entry is gone.
- Next action: Give complete group publications their own unique resource/serial residency entry and O(1) warm touch; distinguish ordinary child cache eviction from actual producer mutation using a weak owner relay, retaining before-write invalidation. Reject group cache admission when its residency cannot be retained.
- Verification: Source-confirmed integration defect before native group execution. Potential cache churn and unbounded publisher retention; no live viewer memory/FPS incident attributed. Existing synthetic tiny-budget test acceptance must be corrected to preserve the real budget contract.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Root review identified the source age2 policy and missing group retention/touch before the first native run. Registered before group residency repair; retained source/GPU reader semantics must still pass.
- 2026-11-14: Actual group-owner/capture/range native1152assertions/18cases pass on clean7422 with validation, including unique union/age/count/byte retention and pre-COW readers. Independent churn regressionPPD164 is retained and being repaired; no owner runtime verification.


## PPD-161 — Resource query fixture boxes contain no pixel centers

- Status: investigating
- Confirmation: reproduced
- Owner: Codex world-resource validation
- Discovered: 2026-11-14
- Original reference: A before-geometry visibility query must cover actual raster samples at the fixture viewport, not just a nonempty continuous-space box.
- Evidence: [Persistent scene/command work](work/active/apple-first-persistent-native-scene-20261114.md). native-focused-correctness-1 world-resources exits42:4cases/3pass,7734assertions/7670pass; all64 failures are the same expected.queries.front()>0 assertion returning0 across formats/families.
- Next action: Enlarge the fixture's0.015 NDC half-extents to cover known pixel centers at side32, retain positive-before-query and exact old/new ordered result/output assertions, then rerun native validation and previous-library parity.
- Verification: Query center coordinates0/-0.25 lie between32pixel viewport sample centers spaced0.0625 NDC; nearest X centers differ0.03125, so0.03wide boxes miss both neighbors. Direct/prepared/table pixels, actual bound counters, lifetime/fallback/joining checks otherwise pass. This is not attributed to a production renderer query bug.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered actual failed native receipt before fixture raster calibration repair. Full required build1 at144aa7 passes0/839.404s; group-owner native1152/18passes separately. Previous-shader process was not reached because the focused runner correctly stopped on the first failing group.
- 2026-11-14: Fixture half-extent.08 repair on7422 passes all8889resource assertions/5cases and4721old-compiled-library parity assertions/1case with positive query coverage and identical ordered results. This closes no production viewer incident; owner acceptance remains pending.


## PPD-162 — New group publisher prepares a disabled simple render pass

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native group admission
- Discovered: 2026-11-14
- Original reference: Original/current postSort checks hasRenderType(pass) before populating a view's render map; disabled passes must not acquire new optional native source preparation.
- Evidence: [Persistent scene/command work](work/active/apple-first-persistent-native-scene-20261114.md). New after-flush group publication loop could prepare PASS_SIMPLE sources while that render type is disabled, although the map-admission loop correctly skips it.
- Next action: Include PASS_SIMPLE enablement in the shared group-path policy predicate, preserving the existing disabled native pass behavior and avoiding unused publication work.
- Verification: Source-confirmed inactive CPU work under the new diagnostic feature; no runtime timing or visible regression attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered after first frozen source build and before the disabled-pass guard repair. New controls remain OFF pending complete-path cost gates.
- 2026-11-14: PASS_SIMPLE policy guard is implemented in7422. Required build2 and native correctness pass; no live disabled-pass performance/render outcome has been inferred.


## PPD-163 — Native resource hot hits repeat immutable validation and command ownership work

- Status: investigating
- Confirmation: reproduced
- Owner: Codex persistent native command preparation
- Discovered: 2026-11-14
- Original reference: Producer-qualified immutable resource tables should avoid revalidating/rebuilding their unchanged texture tuple on every draw; each command needs exact source/table ownership, not duplicate retain operations for repeated inputs.
- Evidence: [Persistent scene/command work](work/active/apple-first-persistent-native-scene-20261114.md). Corrected costs1: warm512 single-slot legacy CPU0.189521->0.217187ms shared,0.205709->0.229458ms distinct; PBR0.223563->0.249770ms shared. Eight-slot warm tuples improve, so no unconditional resource-table default is justified.
- Next action: Use private source/cache/sampler epoch proof for the hot table path, preserve slow-path validation, and deduplicate already-retained source/table pairs within a command using bounded identity checks. Apply equivalent numeric-source retention improvements to the scalar path and rerun full costs/correctness.
- Verification: prepare currently copies the32view key and repeats weak-context/native-sampler validation on every hit; bind repeats the full tuple comparison. Command ownership only deduplicates adjacent sources. Measured mechanism regression, not attributed live viewer FPS.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered actual warm single-slot losses and source-confirmed repeated immutable work before cache-proof/command-retention repair. Cold/churn overhead also remains in evidence.
- 2026-11-14: Frozen resource component diagnostic build0/9.446s and native12379assertions/7cases0/4.909s pass with Metal validation, including17source retention collisions and old table lease semantics. Quiet component costs0/2.254s show eight-slot warm benefits23–50% CPU, but single-slot/shared legacy three-view CPU+4.47% and an adverse completion row remain. Exact component hashes/receipts retained; full frozen app/native/cost checks still next.


## PPD-164 — One changed source rebuilds every native descriptor in its group

- Status: investigating
- Confirmation: reproduced
- Owner: Codex producer group publication
- Discovered: 2026-11-14
- Original reference: Dirty producers publish changed native descriptions once; unchanged immutable scene membership should not require whole-group LLDrawInfo/native prototype rediscovery after a single resource update. Before-write invalidation must still release cache-induced GPU reader leases.
- Evidence: [Persistent scene/command work](work/active/apple-first-persistent-native-scene-20261114.md). costs1 warm512/oneview CPU0.592084->0.241459ms, but one-change-per16source-group churn0.705292->2.267620ms; three-view churn1.964000->2.763170ms.
- Next action: Separate persistent CPU-only source descriptions/weak native generation identities from completion-owned strong native view leases, refresh only changed source revisions, and rebuild the group resource union only when allocation identities actually change. Preserve byte/count/age budgets and all COW/reader tests.
- Verification: Group publisher drops all source payloads before COW; after child-cache age eviction this destroys unchanged CPU descriptions too. seal revisits/reconstructs every source and resource union. Measured complete-path churn regression; no live scene/FPS attribution.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered warm improvement together with cold/churn regressions before incremental CPU/source-lifetime separation. Do not enable solely on favorable warm512 results.
- 2026-11-14: Incremental CPU-only rows/weak native tuple reuse authored, four new actual-group cases frozen; native execution/cost3 pending. No viewer/owner acceptance or push.


## PPD-165 — Persistent CPU group layout needs a separate bounded residency policy

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex incremental group publication
- Discovered: 2026-11-14
- Original reference: Native working-set age/count/bytes limit strong GPU publishers. CPU-only descriptions retained after those publishers are evicted must also have an explicit bound without reintroducing strong native reader leases.
- Evidence: [Persistent scene/command work](work/active/apple-first-persistent-native-scene-20261114.md). The PPD164 in-progress Layout retains one full CPU Batch/weak-resource record per source through LLSpatialGroup even after group/child publisher age eviction; that metadata is outside native working-set accounting.
- Next action: Bound owner-side CPU layout retention independently, reclaim old/inactive layouts conservatively, report actual CPU occupancy, and retain before-write/native completion semantics. Compact source/instance work remains a separate subsequent boundary.
- Verification: New-source review finding before the first execution of incremental layouts; no observed viewer memory growth or failure attributed. Unbounded is with respect to the native cache policy, not an allegation of leaked scene objects.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered the distinct CPU retention requirement before adding layout admission/eviction policy; existing native budgets alone cannot certify CPU bounds.
- 2026-11-14: Owner-only bounded CPU LRU and current-frame admission protection implemented; row/byte/pressure/reader cases authored, execution pending. No viewer/owner acceptance or push.


## PPD-166 — CPU layout eviction callbacks can invalidate an in-progress admission

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex group CPU residency integration
- Discovered: 2026-11-14
- Original reference: ALNativePublication State permits owner-thread reentrant notifications; older admission/withdrawal must not replace or remove a newer exact tuple (PPD120).
- Evidence: [Persistent scene/command work](work/active/apple-first-persistent-native-scene-20261114.md). Independent source review found CpuLayoutResidency victim removal invokes State::evict observers. A callback may reentrantly admit a newer tuple for the incoming publisher before outer retain resumes; ignored map insertion failure then duplicates list/charges, and old-entry removal can erase a newer lookup.
- Next action: Recheck the exact publisher tuple after callbacks; honor duplicate insertion outcomes, erase only an entry's own lookup, and prevent stale seal from installing its old Layout after a newer publication. Add actual State observer reentrancy coverage.
- Verification: Source-confirmed new-policy integration defect before first native execution of incremental group code. No observed viewer memory/FPS incident or native test failure attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Independent residency review reported the concrete callback sequence; registered before transactional admission repair. Resource component checks remain independently passing and are not group-policy validation.
- 2026-11-14: Exact tuple checks after both ledgers, protected lookup insertion/erase and stale holder cleanup implemented; actual recursive seal and invalidate-only observer cases authored, execution pending. No viewer/owner acceptance or push.


## PPD-167 — Native source residency admission also lacks post-callback tuple guards

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex shared native source residency
- Discovered: 2026-11-14
- Original reference: State dependent notifications and existing PPD120 publication reentrancy require exact tuple identity throughout admission/retirement.
- Evidence: [Persistent scene/command work](work/active/apple-first-persistent-native-scene-20261114.md). Root follow-through from PPD166 found ALNativeSourceWorkingSet::retain checks the caller tuple only on entry, then removes/trims entries with State::evict callbacks, builds a candidate from the possibly newer state publication, and ignores lookup insertion success. remove erases lookup by identity unconditionally.
- Next action: Preserve the caller's exact tuple across callback boundaries, protect newer lookup entries, and transact accounting/insertion so reentrant admissions cannot duplicate or misattribute resource charges. Add focused native-source-working-set observer regressions; preserve current-frame protection and bounded admission.
- Verification: Source-confirmed contract defect after State dependent notifications were introduced; no live viewer fault/FPS effect attributed. Keep distinct from CPU-only policy PPD166 while testing their shared callback interaction.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered root source finding before the shared native-ledger repair. Existing passing tests did not include eviction-observer reentrant admission; no previous evidence is relabeled as coverage.
- 2026-11-14: Shared native ledger freezes caller tuple/revision, rechecks after each callback, protects lookup/accounting and detaches clear ledger before notification; four actual Observer regressions authored, execution pending. No viewer/owner acceptance or push.


## PPD-168 — Source sealing can acknowledge an authoritative mutation during residency admission

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex compact source/native tuple integration
- Discovered: 2026-11-14
- Original reference: A cache-only withdrawal leaves a caller's transient immutable reader valid; a changed producer revision must reject stale sealing (PPD120/167).
- Evidence: [Persistent scene/command work](work/active/apple-first-persistent-native-scene-20261114.md). Source publish checks State.publish, installs weak transient metadata, then ignores native working-set admission outcome and returns true even if its eviction callback invalidates the producer revision.
- Next action: Recheck the authoritative revision after residency callbacks while preserving legal cache-only transient readers and any newer publication/metadata; add typed source-owner regression.
- Verification: Source-confirmed in inherited source factory, found during native-tuple ownership split. No viewer incident or frozen build3 result attributed. Repair is on the separate compact worktree, not silently applied to the build3 source pin.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before narrow post-admission producer-revision guard. Native working-set admission itself is separately repaired asPPD167; factory acknowledgement still needs this distinct boundary check.
- 2026-11-14: Narrow producer-revision recheck is implemented in compact worktree; native tuple/source-owner tests and actual cache-withdraw versus authoritative-mutation callback cases are authored/frozen. Required build4/native retest pending; old frozen3/afa8b13 is unchanged.


## PPD-169 — New native tuple fixture uses unsupported Catch logical decomposition

- Status: investigating
- Confirmation: reproduced
- Owner: Codex compact capture validation
- Discovered: 2026-11-14
- Original reference: Catch assertions containing operator|| need the complete expression parenthesized so the decomposer does not bind to its left operand.
- Evidence: [Compact source/view work](work/active/apple-first-compact-source-instances-20261114.md). Clean2eb22a0a9f required build4 exits1/62.804s; compiler points to alpipeline_buffer_publication_metal_test.cpp774, CHECK(first.owner.owner_before(tuple) || tuple.owner_before(first.owner)).
- Next action: Parenthesize the complete owner-identity expressions in the new fixture, preserve assertion meaning, then run a new clean required build and all compact/ownership cases.
- Verification: Actual test compilation failure; doctor/configure pass, full viewer build and source4 native results are not claimed. No production renderer bug or viewer crash attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered the failed frozen-source build before fixture assertion syntax repair; retain build4 log/receipt rather than overwriting it.
- 2026-11-14: Both owner-before OR expressions are fully parenthesized; assertion meaning is retained. New clean required build5 and native execution are pending.


## PPD-170 — Compact capture adds a second per-source hash allocation on the actual viewer frontier

- Status: investigating
- Confirmation: reproduced
- Owner: Codex compact capture integration
- Discovered: 2026-11-14
- Original reference: Compact per-view references must improve complete source-capture/ownership/encoding work, including actual owner retention rather than shared-prototype component construction alone.
- Evidence: [Compact source/view work](work/active/apple-first-compact-source-instances-20261114.md). fa96867 required build5 and native correctness pass. Costs5 actual512unique LL sources warm CPU0.640125->0.715146ms(oneview),1.975270->2.178540ms(threeviews); other cold/churn actual rows also regress. Manual eight-prototype native packets improve, so their gain cannot certify real capture.
- Next action: Give the CPU-only descriptor aggregate bounded identity dedupe with a strong vector instead of a second allocating unordered set; keep exact native-resource aggregate behavior unchanged, verify collision/reader lifetime, and rerun full actual-source costs.
- Verification: Source-confirmed retainCpu currently allocates the same WorldResourceLease hash-set node per unique source in addition to the native tuple aggregate. Each reference needs a CPU lifetime pin, but that pin does not require per-source hash-node allocation. No live viewer FPS effect attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered the real-frontier regression before ownership-allocation repair. Preserve all24actual and45component cost pairs; no default compact performance acceptance is claimed from component-only gains.
- 2026-11-14: CPU-only WorldCpuLease vector+16-slot retention cache implemented; native aggregate unchanged. Actual-owner fixture shares the production helper, adds17source collision/nonadjacent retention and cancellation/abandon coverage; execution/build6 pending.
- 2026-11-14: Source7 actual corrected costs still regress6.6–9.4% for512unique sources despite passing native cases. Default compact activation is rejected; nonpersistent RenderMetalCompactInputs OFF preserves the native authored-batch path. Keep shared ownership/type changes and continue next concrete GPU source consumer; do not close this performance record.


## PPD-171 — Paired capture cost arms advance source-cache age twice per viewer frame

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex actual native cost fixture
- Discovered: 2026-11-14
- Original reference: Native source working-set age is measured in viewer frames (default2); alternating benchmark arms must each advance one logical frame per sample while FrameData independently protects all native consumers.
- Evidence: [Compact source/view work](work/active/apple-first-compact-source-instances-20261114.md). Actual cost6 churn512 updates32sources/sample but reports3264publications across12samples, rather than384; raw samples alternate ~512 versus32 publications. Both actual-compact and native-group cost loops increment Inputs::gFrameCount once per arm, and alternating order creates a3tick gap. The first dirty-source admission trims unchanged entries older than2 before their warm touches.
- Next action: Advance source/CPU-cache logical viewer frame once per paired sample; keep independent native FrameData generations per measured arm so constants/pages cannot cross-arm deduplicate. Apply the same normalization to both actual-source and native-group costs and retain raw publication counts.
- Verification: Fixture epoch-domain defect, not a demonstrated viewer cache bug. Warm rows with zero publications and correctness receipts remain valid; churn comparisons with collateral age eviction need qualified replacement evidence. Do not raise production age limits to hide it.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered the exact3264=6*512+6*32 source census before timing-fixture normalization. Prior costs retained; no live viewer/frame regression attributed to this benchmark artifact.
- 2026-11-14: Both actual and group fixtures now advance one paired logical viewer frame with independent native epochs per arm. Exact cold/warm/churn source-publication assertions authored; production max_age2 unchanged. Build7/retest pending.
- 2026-11-14: Clean75c7ae61 full costs7 pass exact publication assertions:512source churn has384publications per arm across12samples/32updates, oneview andthreeviews. Prior3264collateral age artifacts retained; production age2 unchanged. Owner/runtime performance remains unverified.


## PPD-172 — Owned compact packets are fully revalidated through the borrowed-scene entry point

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex immutable native input consumption
- Discovered: 2026-11-14
- Original reference: ALMetalWorldViewInputs has private construction/immutable owned arrays; the scalar consumer should preserve that qualified ownership boundary, following the existing prepared path, rather than repeat complete ordered-shape validation as if receiving a mutable borrowed scene.
- Evidence: [Compact source/view work](work/active/apple-first-compact-source-instances-20261114.md). adopt validates the complete raw/ref permutation, ownership, query order and supported sources. Production then discards the type and calls render(inputs.scene()), which repeats the O(N) permutation/source walk. The if(!prepared) branch repeats this scan for scalar Scene consumption. Prepared consumption already skips it; borrowed direct scenes still require that validation.
- Next action: Add an explicit owned-input render entry point that preserves the immutable type, avoid the duplicate shape scan for owned scalar consumption, preserve existing prepared behavior/per-draw validation and cancellation/epoch checks, and keep arbitrary borrowed Scene rendering fully validated. Update real capture/cost callers and test output/lifetime equivalence.
- Verification: Source-confirmed redundant work on compact views, identified while actual warm512 reference capture remains slower after PPD170. Its net timing contribution is unproved until full actual-source costs rerun; no FPS claim.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before introducing the narrow typed consumer; no untrusted/public Scene validation is waived and no geometry/matrix/shader arithmetic change is intended.

- 2026-11-14: Exact99d33 source review corrects the initial scope assumptions: prepare.mm validates the whole view only in adopt, and renderInternal guards full shape validation with if(!prepared). Create/prepared already use qualified inputs; repair only the scalar type-discarding entry. No create/prepared algorithm change.

- 2026-11-14: Explicit renderOwnedInputs entry implemented; private shape qualification is inaccessible to borrowed Scene callers. Per-draw checks remain; new malformed borrowed-copy and owned per-draw rejection/parity/cancel/lifetime cases authored. Both actual/native packet cost arms use the same typed entry. Build7 pending.
- 2026-11-14: Clean75c7ae61 required build7 and native actual2471/28+compact1990/8+resource12379/7+oldshader4721/1 pass. New borrowed-copy and typed per-draw rejection/lifetime/parity cases pass. Actual compact cost remains adverse, so no causal net performance success is claimed.


## PPD-173 — Native source-row lookup cannot recover capacity after scene churn

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native source-page implementation
- Discovered: 2026-11-14
- Original reference: Online Second Life travel and source churn require a bounded working set that can recycle idle GPU source identities while actual readers keep immutable old generations.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md), in-progress tree based14d57c. almetalworldsourcepages.mm initially consumes free slots until4096 identities are reached, but erases none except clear(). Native reader COW exists, so permanent exhaustion is unnecessary. No viewer run or runtime regression is attributed.
- Next action: Recycle least-recently-used slots that are not selected in the current view, advance nonwrapping allocation generation, remove exact old key/identity indexes and preserve old bank/page reader generations. Add churn beyond capacity with an old live FrameView. Replace quadratic completion-lease union scans in the same new consumer with bounded identity lookups before complete-path cost testing.
- Verification: Source-confirmed implementation defect, registered before repair. Build/native execution pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered source review finding; source-row experiment remains defaultOFF, unbuilt/untested. Required native scalar draws remain available on optional admission failure.
- 2026-11-14: Preallocated LRU/current-view slot protection, exact index removal, nonwrapping generations, idle resource-context table release and hash/control-block lease unions implemented. Capacity2/churn10 with old gated GPU pixels and generation checks authored. Required build9/native execution pending.


## PPD-174 — Source model re-registration can retain withdrawn owner proof

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex source-row capture provenance
- Discovered: 2026-11-14
- Original reference: Model ownership must be currently available, while already-copied immutable views remain valid.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md), new helper registerSourceModel initially ignores an invalid snapshot or matrix mismatch without erasing an earlier valid registration for the same owner pointer during one Capture. Ordinary begin-once clears it, so no runtime symptom is attributed.
- Next action: Erase that pointer's owner proof on invalid/stale replacement registration; preserve existing copied candidate rows, and test valid-to-withdrawn/invalid/stale registration before later capture.
- Verification: Source-confirmed provisional API defect registered before repair; build/native execution pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Recorded independent capture-helper review; native source-row diagnostic remains defaultOFF.
- 2026-11-14: Narrow pointer-proof erasure and actual-owner valid-to-retired/stale-to-fresh cases authored; copied old view rows stay immutable. Required build9/native execution pending.


## PPD-175 — Source-row consumer can ignore a compact view's UV override

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex source-row renderer integration
- Discovered: 2026-11-14
- Original reference: Optional GPU source-row rendering must preserve resolved compact per-view transforms or retain ordinary native rendering.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md). Consumer initially compares resolved MVP/modelview/normal to copied region rows, but SourcePages encodes source.texture_matrix without comparing the valid compact Draw.textureMatrix override. Production owner rejects UV animation, while a public valid Scene can supply a distinct override and otherwise reach the new path. No viewer execution is attributed.
- Next action: Reject mismatched per-view/prototype UV matrices from optional source-row selection before freeze, retaining normal native draw output. Add actual renderer UV-override parity/fallback regression.
- Verification: Source-confirmed provisional consumer defect, registered before repair.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Independent shader/renderer fixture review reported the missing qualification; new path remains defaultOFF, unbuilt/untested.
- 2026-11-14: Exact64-byte resolved/prototype texture-matrix qualification added before native freeze; actual compact UV-override parity/fallback fixture authored. Build9/native execution pending.


## PPD-176 — Source-row bound counter can include a failed G-buffer render

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex source-row renderer telemetry
- Discovered: 2026-11-14
- Original reference: Accepted-render counters must distinguish a fully encoded accepted native view from abandoned partial work; none is a GPU completion count.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md). Initial scalar source-row branch increments renderer-global bound immediately per draw; a later encoder/coverage/transaction failure can abandon that work while leaving bound inflated. Existing material counters accumulate locally until finishCommandBuffer accepts the render.
- Next action: Accumulate source-row bound locally and publish at the existing successful render boundary. Document requested/fallback as setup attempts and bound as accepted G-buffer render draws, not GPU-completed work.
- Verification: Source-confirmed telemetry defect registered before repair; no live counter result is attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Independent integration review reported the premature count; new path remains defaultOFF.
- 2026-11-14: Bound draws now accumulate locally and publish at successful finishCommandBuffer boundary, with setup-attempt and accepted-render semantics documented. This is not a GPU-completion count. Build9/native execution pending.


## PPD-177 — Native source FrameView lacks its narrow encoder declaration access

- Status: investigating
- Confirmation: reproduced
- Owner: Codex source-row integration
- Discovered: 2026-11-14
- Original reference: Native source resources must be declared against the exact command/encoder while ordinary binding changes keep wrapper barriers.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md), required build9 at244ed38006: doctor/configure0, build1 after13.687s. almetalworldsourcepages.mm129/131 calls existing private nativeCommandBufferForIndirect/nativeHandleForResources; ALMetalWorldSourceFrameView was not added to the narrow friend list. MSL compiled before the C++ failure.
- Next action: Grant only the source FrameView the existing private native bridge, preserving public encoder encapsulation, then rerun required build/native checks at a new clean source.
- Verification: Compiler failure reproduced; receipt frozen-source-build-9.json and log retained privately.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered exact compile failure before repair. No native test/runtime success is claimed for this checkpoint.


## PPD-178 — Native source draw fixture uses a reserved MSL keyword

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native source draw fixture
- Discovered: 2026-11-14
- Original reference: Native tests must compile their own ordered-draw shader before testing the production encoder.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md). Required native-source-row-correctness-10 at ea7ca55efc: source_row_draw_test runtime shader names its vertex_id parameter vertex, a reserved MSL stage keyword; newLibraryWithSource rejects it.
- Next action: Rename only the fixture parameter and rerun native ordered-draw correctness. Production builtin MSL compiled in build9/10.
- Verification: Registered before repair. Exact build10/native/static receipts retained privately; source-confirmed counterexamples are distinguished from executed failures.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Recorded validation/review finding; source-row experiment remains defaultOFF and full architecture stays partial.


- 2026-11-14: Fixture parameter renamed vertex_index, assertions unchanged; clean build11/native retest pending.

## PPD-179 — Nested source-pointer reflection aborts under Metal API validation

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native source-page ABI
- Discovered: 2026-11-14
- Original reference: A reflected source-row ABI must use supported Metal API operations and retain graceful optional failure.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md). Required native-source-row-correctness-10 SIGABRT: MTLDebugArgumentEncoder newArgumentEncoderForBufferAtIndex:0 reports No argument buffer at that index when called on the pointer bank encoder. Build10/static69020 pass but GPU source path is unverified and fails this API gate.
- Next action: Replace unsupported nested lookup with an explicit directly reflected SourceRow layout entrypoint sharing the exact MSL type, then prove real bank-to-row shader reads, layouts and fallback on M1 validation.
- Verification: Registered before repair. Exact build10/native/static receipts retained privately; source-confirmed counterexamples are distinguished from executed failures.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Recorded validation/review finding; source-row experiment remains defaultOFF and full architecture stays partial.


- 2026-11-14: Added shared-type reflection-only layout kernel and direct function-buffer argument encoder, with function/type/size/alignment checks. Missing-layout compiled69020 negative fixture authored. Build11/native retest pending.

## PPD-180 — Derived resource-table generation is absent from source-row hit proof

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native source-page cache
- Discovered: 2026-11-14
- Original reference: Stable source revisions do not guarantee that a derived context/sampler argument table allocation still exists.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md). Independent review: a live ResourceSource can evict its old derived Table after more than four other contexts/samplers. Row keeps only weak Source/Material plus raw native handle signature; recycled native buffer address can falsely hit while encoded GPU table address is stale. This is a permitted address-reuse counterexample, not a runtime reproduction.
- Next action: Keep weak exact ResourceTable control-block identity in Row; require the current prepared table to match on hit, and reset it on eviction/invalidation. Test real table/context churn and output/lifetime without assuming allocator address reuse.
- Verification: Registered before repair. Exact build10/native/static receipts retained privately; source-confirmed counterexamples are distinguished from executed failures.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Recorded validation/review finding; source-row experiment remains defaultOFF and full architecture stays partial.


- 2026-11-14: Weak exact derived Table control-block proof added/reset at all retirement paths. Real5-context churn, old gated-reader survival and renewed publication/pixel cases authored without allocator-reuse assumptions. Build11/native retest pending.

## PPD-181 — World shader reload can commit one owner before the other is ready

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex shared world shader-context continuation
- Discovered: 2026-11-14
- Original reference: Shared main/probe library epochs require one staged coherent owner group while old native readers retain old contexts.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md). Actual pipeline reload calls world owners sequentially; each Renderer.reloadShaders stages then swaps its own Impl, and shared shadowMaterials reload mutates FrameData before that swap. A later owner failure can split world shader state. Global builtinReloadGeneration advances later, so it cannot alone key an atomic shared-world lease. No runtime symptom is attributed.
- Next action: Stage one explicit shared builtin lease and both owner replacements, perform fallible shared shadow work once, then commit no-fail swaps. Keep explicit older-library reload private; test second-stage failure and old native FrameView survival.
- Verification: Registered before repair. Exact build10/native/static receipts retained privately; source-confirmed counterexamples are distinguished from executed failures.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Recorded validation/review finding; source-row experiment remains defaultOFF and full architecture stays partial.


## PPD-182 — Derived-table fixture repeats unsupported Catch OR decomposition

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native table-generation fixture
- Discovered: 2026-11-14
- Original reference: New native regression assertions must compile with the repository Catch version without weakening their intended ownership check.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md). Build11 atd17e9a0a8c doctor/configure0, build1 after60.117s; source_pages_test.mm320 CHECK(a.owner_before(b) || b.owner_before(a)) lacks outer expression parentheses. This repeats the Catch rule previously recorded at a different site in PPD169. New direct-layout MSL compiled.
- Next action: Fully parenthesize this ownership comparison; retain its meaning and rerun required build/native gates.
- Verification: Compiler diagnostic and failed frozen-source-build-11 receipt retained; no native success is inferred.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before repairing the new assertion site.


## PPD-183 — Scalar GPU source-row submission regresses complete actual-source cost

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native whole-engine source/submission architecture
- Discovered: 2026-11-14
- Original reference: Persistent GPU source inputs must reduce complete viewer-source and submission cost; data-layout changes alone are not a performance improvement.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md). fac8f0b2f1 build12/static69024 and all new native gates pass. Actual per-source Legacy/PBR capture/adopt/publication/encode/retire benchmark passes67910 assertions,48paired comparisons, exact path/publication counts and parity. Every CPU comparison regresses22.0–66.3%;512source warm CPU rises22.0–29.5%, cold32.7–44.8%, churn25.5–29.4%; GPU sums also rise~5–10% at512. Raw samples and negative results retained, not whole-viewer FPS evidence.
- Next action: Keep scalar RenderMetalSourceRows defaultOFF. Consume retained GPU rows through bounded persistent command banks and producer-sealed groups to remove repeated per-draw preparation/submission, preserving exact order/query/feature behavior. Measure the complete actual-source path before activation; do not accept shared-prototype/component wins as owner-frontier improvement.
- Verification: Reproduced contained M1 cost regression at an otherwise passing correctness checkpoint. Original architecture remains active/partial.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before command-bank/group work. Source-row layer is retained as a qualified native mechanism, while scalar default activation is rejected.


## PPD-184 — Publication callback copies can discard denied-work retry age

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex dirty-source publication admission
- Discovered: 2026-11-14
- Original reference: Optional dirty work must preserve finite retry age under a persistently busy or memory-constrained engine.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md), unfrozen isolated publication-budget implementation. SourcePages.freeze copies std::function admission by value per call; the actual app policy captures mutable pendingFrame by value, so the updated age dies with each copy. Shared-pointer test closures initially conceal this. No built/runtime attribution.
- Next action: Store one shared callback holder per context and invoke its same mutable closure, with reentrant setter lifetime/epoch protection; add a real value-captured retry-age regression.
- Verification: Source-confirmed new implementation defect, registered before repair. No execution claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Backend author reported before freeze; finite-progress repair is required before integration.


## PPD-185 — Asynchronous renderer attachment reads owner-only library state

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex shared world-library integration
- Discovered: 2026-11-14
- Original reference: Workers must consume an acquired immutable library lease/epoch; mutable FrameData and renderer publication remain on their owner.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md). Unbuilt shared-world implementation a4726cdf6d worker invokes renderer.setFrameData(build.frame_data); that setter now reads FrameData.builtin_library to mark shared membership. Owner reload can assign this same non-atomic shared_ptr while worker attaches. Epoch callback rejection occurs later and cannot prevent the data race. No runtime race is claimed.
- Next action: Keep worker construction limited to its captured device/library lease; move FrameData attachment to the main callback after device/FrameData/epoch/global-generation qualification, before publication. Explicitly retain owner-only library APIs.
- Verification: Source-confirmed threading defect registered before repair.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Root integration review identified the worker-to-owner state read before native build.


## PPD-186 — Material-group admission failure can leave an unbudgeted publication

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex material-group publication
- Discovered: 2026-11-14
- Original reference: Published immutable groups require bounded residency; failed admission must withdraw only its own candidate and preserve any newer reentrant publication.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md), unfrozen isolated material-group producer. New State.publish followed by native/CPU admission has only an outer bad_alloc catch; unlike the Simple guards retained from PPD165/166, it can leave that exact candidate published after admission OOM. No built/runtime attribution.
- Next action: Guard post-publish admission with exact candidate/revision cleanup on failure, retaining newer reentrant state. Add actual-owner failure/reentrancy cases; preserve visibility-disabled material admission qualification.
- Verification: Source-confirmed author review finding recorded before repair.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Material producer author reported before freeze; bounded exact-owner repair is required.


## PPD-187 — Command-bank encoding rereads mutable caller records after admission

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native source command banks
- Discovered: 2026-11-14
- Original reference: An admitted immutable command bank must encode exactly the already-validated key/index/range snapshot.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md), unfrozen command-helper reviewed at b88bb2e0e5e734a70df51d6ef12aa2d880b93da0b597f10a2f310de3bc397863. Helper copies/proves keys and native index leases before admission, then rereads borrowed Draw records when encoding after callback. A callback can change index/count/offset without changing helper epoch, causing bank key and native leases to certify A while GPU commands encode B. No runtime attribution.
- Next action: Encode only the validated native-pinned key snapshot; never reread caller Draw fields after callback. Add actual callback-mutation pixel/key/warm-hit regression and preserve epoch/command checks.
- Verification: Independent source review finding, registered before repair.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Recorded in isolated notes tree while required build13 keeps the source checkout frozen and clean.


## PPD-188 — Source FrameView does not prove native buffer device ownership

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native source-generation proof
- Discovered: 2026-11-14
- Original reference: Indirect resource use must retain the ordinary native encoder's exact device rejection before any Metal command encodes a foreign resource.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md). Independent command-bank review: SourcePages validates geometry ranges/material/resource matches, but not native MTLBuffer.device for vertex/index/numeric buffers. A validly shaped manual source can combine A textures with B index/numeric buffers and obtain a FrameView, unlike direct drawIndexed which rejects foreign index devices. One-device M1 runtime cannot reproduce a distinct GPU; this is a source-confirmed API proof gap.
- Next action: Establish exact native-device proof once per unique buffer/generation before indirect row publication; retain bounded hot proofs without producer pins. Add defensive cold bank index-device check. Preserve same-native-device wrapper compatibility and report unavailable multi-device execution honestly.
- Verification: Source-confirmed counterexample, registered before repair; no multi-device runtime claim.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Independent reviewer and command-bank author reported the missing prerequisite before native bank execution.


## PPD-189 — Material capture bridge skips invalid direct-probe input

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex material-group capture bridge
- Discovered: 2026-11-14
- Original reference: Native probe capture must retain its incomplete-transaction handling when a required draw record is unavailable.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md). Unfrozen group capture raw lambda skips null DrawInfo like main pools; corresponding direct-probe Legacy/PBR loops mark incomplete and clear. Pipeline glue review found the scope mismatch. No runtime attribution.
- Next action: Keep main null-skip, but mark incomplete/clear when direct_probe_scene_active; test both scopes and raw/group order.
- Verification: Source-confirmed new bridge regression registered before repair.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Producer and glue authors reported before freeze.


## PPD-190 — Material retirement fixture creates a fresh raw read before checking in-place reuse

- Status: investigating
- Confirmation: reproduced
- Owner: Codex material-group native fixture
- Discovered: 2026-11-14
- Original reference: Cache-only retirement tests must distinguish real generation leases and the established raw-handle frame-read guard.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md). Build14/static69041 and new native bank1942/5 pass; actual-publication suite has42/43 cases passing,9223/9225 assertions. Both failures are material churn test1554 expecting wrapper identity unchanged. Its immediately preceding getMetalVertexBuffer() records a fresh last_read_frame; upload_metal_buffer deliberately orphans an incomplete raw read even when no lease remains. leased_orphans stays unchanged, so idle-cache lease retention is not established.
- Next action: Snapshot the cached weak-layout prototype pointer for identity before write, avoiding a new raw read; retain leased-orphan, restoration, pixel and COW assertions and rerun. Do not weaken the renderer's raw-read protection to satisfy the fixture.
- Verification: Two executed fixture failures with source-confirmed observation side effect; no viewer defect attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before fixing the native retirement observation.


## PPD-191 — Cached indirect banks do not improve the actual per-source path on M1

- Status: investigating
- Confirmation: reproduced
- Owner: Codex end-to-end native scene/submission architecture
- Discovered: 2026-11-14
- Original reference: A faster architecture must improve complete preparation/submission cost and preserve GPU cost; fewer CPU execute calls alone are insufficient.
- Evidence: [GPU source-row work](work/active/apple-first-gpu-source-rows-20261114.md), clean ec44584c06 build15/static69044/native gates pass. Actual command-bank benchmark passes79512 assertions and48pairs with proven ICB execution, MRT/depth/query parity and exact publication counts. AllCPU comparisons are40.1–121.8%slower. Warm512 CPU is60.2–70.6%higher; GPU sums26.8–85.1%higher. Warm512 one-view CPU Legacy0.741->1.199ms/PBR0.725->1.236ms; three-view Legacy2.279->3.651/PBR2.195->3.585ms. Raw samples retained; no live viewer FPS attribution.
- Next action: Keep banks defaultOFF and reassess the GPU indirection choice. Prioritize a complete producer-sealed cohort transport/ordinary native consumer, removing per-source packet expansion and redundant preparation while preserving direct shader GPU behavior; measure before activation. Grouped CPU data alone does not prove bank usefulness.
- Verification: Reproduced contained M1 complete-path regression, separate from earlier scalar-row PPD183.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered after real bank-path correctness/cost proof. No indirect-bank performance/default acceptance.


- 2026-11-15: Begin source26 retained GPU cohort publication/command-bank repair attempt. It removes per-view N-reference/request/key preparation at the actual SourcePages/renderer boundary rather than merely reusing a scratch vector. Prior cost15 losses remain authoritative until complete cold/warm/churn costs pass; no activation or speedup claimed.

## PPD-192 — Cohort owner-rejection fixture aliases the producer's first member

- Status: investigating
- Confirmation: reproduced
- Owner: Codex direct cohort native fixture
- Discovered: 2026-11-14
- Original reference: Exact type-erased ownership proof requires the original shared control block and stored address; an alias with both equal is indistinguishable from the original owner.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Clean c57b223531 build16/static69046 pass. API/GPU validation cohort suite has4/5 cases passing,969/970 assertions. Its supposed wrong alias is &CohortCpu::sources, the first data member, which has the same address as CohortCpu. matchesOwners correctly accepts the identical address/control block; no production owner-proof defect is established.
- Next action: Use a distinct non-first-member address, assert that the aliased address differs, and rerun the exact-owner/isolation suite. Preserve independent wrong-control-block, expired-owner and pending-native-generation checks.
- Verification: Executed fixture failure with source-confirmed alias address mistake; remaining renderer and actual-owner checks are separate.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before correcting the fixture; no viewer crash or rendering regression attributed.
- 2026-11-14: Test-only2180adfac9 required17/static69047 passes; repaired exact-owner/isolation suite971assertions/5cases passes with API/GPU validation. Wrong address and wrong control block are independently rejected; pending native-generation lifetime cases remain intact. Automated repair evidence does not close the record.


## PPD-193 — Material cohort lifetime fixture still requires the displaced argument-table path

- Status: investigating
- Confirmation: reproduced
- Owner: Codex actual producer cohort fixture
- Discovered: 2026-11-14
- Original reference: The retained direct path must preserve actual native-generation lifetime and old pixels; optional argument-table bindings are a different rendering mechanism.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Build16 actual-publication API/GPU suite passes43/44 cases and9419/9421 assertions. The two failures are Legacy/PBR bound_draws==1 expectations in the old-generation lifetime fixture. Cohorts deliberately use ordinary direct shaders even when the resource-table flag is enabled, so their argument-table bound count is zero. Native COW/orphan, CPU graph expiration, pending completion and old-image assertions pass. Separate resource-table and prior-library tests remain required.
- Next action: Assert accepted direct cohort draws and unchanged resource-table bind counters in this lifetime fixture. Keep all generation-retention, CPU expiration and image assertions and rerun the actual producer suite.
- Verification: Executed stale mechanism assertion, distinct from a resource lifetime failure or viewer runtime report.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before updating the fixture to prove the actual selected direct-cohort mechanism.
- 2026-11-14: Test-only2180adfac9 exact-tag actual-publication retest passes9423assertions/44cases with API/GPU validation; native old-generation COW/CPU expiration/completion/pixel assertions retained. Combined17 also passes88CTest plus specialization/archive/corpus gates. No owner or publishing acceptance inferred.


## PPD-194 — Direct cohorts repeat excessive producer work during cold loading and churn

- Status: investigating
- Confirmation: reproduced
- Owner: Codex retained native producer architecture
- Discovered: 2026-11-14
- Original reference: Retained scene publication must improve total preparation cost while preserving responsive loading and mutation, not only steady unchanged inputs.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md), clean c57b223531 actual-direct-cohort-costs-16 passes87566 assertions/48 comparisons. Warm512 CPU falls62–67% with GPU sums close to ordinary draws, but all16 cold cases regress7.6–153.7% CPU and all16 churn cases regress5.6–117.5%. Material groups require a resource-enabled source flavor while ordinary flagOFF capture republishes a second flavor; group certificate sealing also recopies/revalidates every unchanged row on partial mutation. Raw timings/publication counts are retained; no viewer FPS claim.
- Next action: Keep activation OFF. Reuse immutable per-source static proof at the existing source-publication boundary and remove duplicate material source flavor work without forcing argument-buffer rendering. Recheck complete cold/warm/churn cost and exact native lifetime/accounting before activation.
- Verification: Reproduced complete contained cost regression; steady direct-consumer improvement does not satisfy the full gate.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before addressing repeated static work and source flavor duplication. Scalar rows/command banks remain independently rejected under PPD183/191.


## PPD-195 — Optional source-capability promotion loses an existing ordinary lease on allocation failure

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex immutable source capability promotion
- Discovered: 2026-11-14
- Original reference: Optional native preparation must retain required ordinary draws when setup fails; a real producer-revision change still invalidates the old input.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Read-only review of the unbuilt PPD194 follow-up: world() has a valid current WorldVariant, then make_shared<Payload>(*value) for optional capabilities can throw into its outer bad_alloc handler, returning an empty WorldLease instead of the still-valid ordinary source. No executed viewer incident attributed.
- Next action: Contain optional promotion allocation failure and return the original immutable lease only if the exact producer identity/revision/publication remains current. Preserve failure for real revision changes or stale publications; add a focused failure/reentrancy regression where practical.
- Verification: Source-confirmed exception path, registered before repair. Existing required build17 pertains to the earlier source, not this follow-up.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Independent reviewer reported before the PPD194 source freeze/build.
- 2026-11-14: Repair also checks the exact original publication before committing a successful promotion. The deterministic callback can install a newer same-revision publication and return normally; the stale outer promotion must reject it as well as the throwing replacement case. This extends the same promotion ownership regression, not viewer-incident attribution.
- 2026-11-14: Clean21efebeb18 required19/static69049 and all17 focused native processes pass. Actual9882assertions/47cases includes the promotion allocation/reentrancy cases; unchanged original lease survives failed setup while invalidated/newer publications are rejected. Automated verification retained separately from owner acceptance.


## PPD-196 — Promotion parity fixture renders a source prototype without its view transform

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native capability-promotion fixture
- Discovered: 2026-11-14
- Original reference: Retained source prototypes contain static scene facts; actual capture supplies current modelview/projection/normal transforms before ordinary rendering.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Clean e5fd8bd98e required18/static69048 pass. Native cohort1080/6, renderer764/1 and prior library286/1 pass. Actual producer suite passes46/47 cases,9787/9788 assertions; the new promotion parity case passes semantic-key/native-tuple/revision/lease checks, then directly renders a view-independent prototype with its unset modelview transform. Ordinary validation correctly rejects it. Other new source-record reuse and allocation/reentrancy cases pass.
- Next action: Copy the two static prototypes and apply the same actual fixture view matrices before comparing rendered output. Preserve source-publication and old-tuple assertions; rerun the exact actual-producer suite.
- Verification: Executed incomplete fixture input, not an observed viewer regression.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before repairing the promotion parity input; failed18 evidence retained.
- 2026-11-14: Test-only21efebeb18 supplies identical current fixture transforms to both immutable snapshots. Required19/static69049 and exact-tag actual9882/47 pass with API/GPU validation; original key/revision/tuple and output assertions retained. Record remains unclosed without owner confirmation.


## PPD-197 — Ordinary direct cohorts require an unused argument-resource descriptor

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex direct source compilation and typed native ownership
- Discovered: 2026-11-14
- Original reference: Ordinary native material shaders bind their existing resources directly. The argument-table descriptor is a newer optional native mechanism, with no required upstream equivalent or role in direct-cohort shader selection.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Source20 native10198/48 and complete cost91726/48 pass, but cold work remains slower. Material group flavor forces native_resources=true, ordinaryMaterial/source-record sealing requires ALMetalWorldResourceSource, and restoration republishes it. Its publish routine snapshots8 padded texture slots (24 Legacy or32 PBR native views and repeated device/type/sample checks). The direct cohort renderer explicitly bypasses resource-table preparation/binding, while exact geometry/numeric/texture leases already exist in the producer's native tuple. This is confirmed unused preparation; its share of the observed regression is unmeasured.
- Next action: Decouple ordinary immutable proof from optional argument-table descriptors. Validate optional descriptors when present, and prove every required typed texture generation from the completed producer tuple before cohort publication. Preserve resource-table/SourceRows requests, exact ownership, restoration and direct output; measure complete costs before claiming improvement.
- Verification: Source-confirmed coupling; contained sampling20 has only41 main-thread samples dominated by waits and is insufficient to attribute cold CPU cost. A separate no-render compilation-stage fixture is authored for follow-up measurement.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before removing the unused descriptor prerequisite. NativeResourceSource remains required for the optional argument-table/SourceRows paths.


## PPD-198 — New source-proof comparison treats a std::array as an iterator

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex direct source proof comparison
- Discovered: 2026-11-14
- Original reference: Static texture-role comparisons must iterate the real declared std::array storage and preserve exact used-slot bounds.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Independent review of the unbuilt PPD197 follow-up found std::equal(prior.diffuse_textures, prior.diffuse_textures + prior.texture_count, batch.diffuse_textures) in new source proof helpers. diffuse_textures is std::array<const ALMetalTexture*,32>, so the expression cannot compile. No executed build or viewer failure attributed.
- Next action: Use .begin() or .data() with the already-validated count in both helpers; retain exact role/key/material/geometry proof checks and compile the coherent follow-up.
- Verification: Source-confirmed type error reported before source freeze/build or correction.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-14: Registered before the author corrects the in-progress proof helper expressions.


## PPD-199 — Shared plain/material fixture leaves conditional expressions ungrouped in Catch assertions

- Status: investigating
- Confirmation: reproduced
- Owner: Codex actual cohort fixture
- Discovered: 2026-11-15
- Original reference: Repository Catch assertions require conditional expressions to be grouped so the decomposer receives the resulting boolean.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Clean3af1013468 required21 doctor/configure exit0; build exits1 after63.043s. Three new REQUIRE(family ? material_call : simple_call) expressions in actual-owner fixture1620/3347/3366 fail because Catch decomposes the condition into ExprLhs<unsigned int>. No production-code diagnostic or native runtime outcome is inferred. Earlier logical-operator fixture cases are retained in PPD169/182.
- Next action: Parenthesize all three complete conditional expressions without changing the selected calls or required assertions, then rebuild and execute native/cost checks.
- Verification: Executed compiler errors and failed frozen-source-build-21 receipt retained.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before correcting the three new conditional assertion sites.


## PPD-200 — Plain cohort transient retry fails after invalidation and eviction

- Status: investigating
- Confirmation: reproduced
- Owner: Codex plain immutable group restoration
- Discovered: 2026-11-15
- Original reference: A valid completed source must survive cache-only withdrawal through real transient readers and permit the established native retry, without manufacturing an unbudgeted publisher or pinning idle native generations.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Clean3a3c0cd3f0 required22/static69054 pass. Actual native producer suite passes48/50cases,10361/10363 assertions; existing invalidation/eviction/destruction/transient-retry case fails REQUIRE(retry) at2337 after the SIMPLE transport extension. Cohort1763/7 and earlier native renderer/resource-policy gates pass. Cause is not yet attributed to the fixture or production restoration.
- Next action: Trace the exact lost generation/proof and model identity through the executed retry case, preserve real-reader and byte accounting checks, repair the earliest incorrect boundary, and rerun. Keep activationOFF.
- Verification: Executed failed retry, source attribution pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before investigating or changing the retry path; failed22 evidence retained.


## PPD-201 — Expanded cohort fixture routes a plain pass through material range admission

- Status: investigating
- Confirmation: reproduced
- Owner: Codex mixed plain/material fixture
- Discovered: 2026-11-15
- Original reference: PASS_SIMPLE uses the native simple range API; material range admission deliberately rejects that pass index.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Actual native22 discovery-bypass case now iterates family0 but still unconditionally calls pushNativeMaterialRange. The helper correctly rejects type0; no renderer failure is established by that assertion.
- Next action: Select the existing SIMPLE range API for family0 and retain material admission for families1/2; preserve matrix/owner/discovery assertions and rerun.
- Verification: Executed fixture route failure, separate from PPD200 transient retry.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before correcting the expanded fixture route.


## PPD-202 — Lazy material storage adds an uncontained allocation to PBR terrain capture

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex batch storage experiment and terrain capture integration
- Discovered: 2026-11-15
- Original reference: Native terrain capture must reject an incomplete transaction through its existing fail/clear path. Lazy authored slots are a new CPU storage mechanism, not a reason to bypass that boundary.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Unbuilt lazy SlotStorage experiment makes first mutable pbrSlots() allocate. captureMetalWorldPBRTerrainBatch uses that accessor outside a local bad_alloc guard, with callers in lldrawpoolterrain.cpp also lacking a surrounding allocation guard. The function already had allocation-bearing vector operations; this adds a new throwing point even with retained vector capacity. No executed out-of-memory crash or viewer incident is claimed.
- Next action: Audit remaining writable-slot capture boundaries, contain allocation failure in the existing complete-transaction failure path before integrating the storage experiment, and retain terrain inputs/output/feature tests. Do not change shader behavior or treat the proposed size reduction as a verified speedup.
- Verification: Source-confirmed new exception boundary, registered before integration repair. Storage experiment remains unbuilt and separate from required23.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered after exception audit and before caller repair or storage integration.


## PPD-203 — Lazy slot layout trades smaller batches for slower unsealed PBR authoring/copying

- Status: investigating
- Confirmation: reproduced
- Owner: Codex measured CPU batch layout experiment
- Discovered: 2026-11-15
- Original reference: CPU layout changes must preserve efficient raw/forward material preparation as well as retained opaque scenes; smaller sizeof alone is not a performance acceptance gate.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Standalone ARM64/O3 mechanism benchmark compiles the exact old23 and candidate headers, executes no viewer/GPU, includes allocation/retirement,4096 rows/24samples plus4warmups/twoseparate runs, equal checksums. Actual sizeof falls3672→1688. PBR unsealed-copy median0.446–0.449→0.524ms (+17–18%); fresh PBR author/move0.590–0.593→0.622–0.630ms (+5–7%). Legacy copies improve; fresh Legacy is roughly flat/slightly higher. This is an input-storage mechanism result, not rendered alpha or whole-viewer FPS.
- Next action: Keep layout acceptance open. Compare complete before23/after costs and native alias/output correctness, including raw preparation risk; avoid unnecessary publication staging copies only if transactional snapshot semantics remain intact. Do not infer a full-engine regression or win from this narrow check alone.
- Verification: Reproduced mechanism tradeoff with exact header/source/command receipts and raw logs in raw-slot-storage-cpu-before-after-23.json. Candidate is not owner-tested or pushed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before accepting or integrating the experimental layout; source-derived size prediction is now independently compiled, while rendered-path evidence remains pending.


- 2026-11-15: Native24 material storage2513/6 passes at104981, confirms1688-byte batches and snapshot/copy/alias semantics. Complete25 cost comparison against23 retains small raw-path tradeoffs and large ordinary changes up to+0.139ms; separate runs do not isolate layout causes. Default layout acceptance remains open, no FPS gain inferred.

## PPD-204 — Large shadow preparation bypasses shared preparation admission

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex frame-wide native CPU preparation
- Discovered: 2026-11-15
- Original reference: Native shadow preparation replaces owner-side traversal with detached immutable-input work; additional CPU jobs must share the same frame admission as main/probe preparation, while denied work retains the required serial result.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Full-goal source audit: main extra preparation obtains FrameBudget::Work::preparation, but shadow count>=8192 calls alMetalPrepareShadowBatches with allow_dispatch=true. shadowprepare.mm uses dispatch_apply_f(2) without that caller obtaining shared preparation credit. It is locally bounded but can overlap other admitted work outside the global CPU plan. No executed oversubscription/FPS incident attributed.
- Next action: Admit extra shadow preparation through the existing shared budget, hold CPU-local credit through join and preserve serial fallback on denial. Check the dispatch helper's actual extra-worker count and add focused admission/parity/lifetime evidence without moving mutable viewer state to workers.
- Verification: Source-confirmed missing integration, registered before repair.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered from original-goal audit; disabled or rejected world submission experiments do not waive this existing shadow path.


- 2026-11-15: Native24 shadow98414/14 passes, including two new shared preparation-budget cases. Busy/pressure behavior follows existing budget policy; credits retire on serial denial/join/failure. Source25 production and native binary unchanged, owner scene acceptance remains pending.

## PPD-205 — Optional CPU slot freezing rejects a valid reader-pinned transient source

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native scene publication
- Discovered: 2026-11-15
- Original reference: Required forward rendering must retain its valid authored source when optional CPU slot freezing is unavailable; cache withdrawal is not producer invalidation.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Source24 review finds that a native_materials=false authored variant can legally survive optional freeze allocation failure. After child cache eviction, a real reader can retain that exact weak transient at the unchanged producer revision. world() retries CPU freezing, but its publication-only promotion guard rejects the transient because State.publication is null. No executed allocation-failure crash is claimed.
- Next action: Preserve the exact current authored transient when CPU freezing is its only missing optional capability, without creating a child publisher or weakening revision/newer-publication checks; add an actual-owner regression fixture.
- Verification: Source-confirmed lost required fallback; candidate is unbuilt.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before repair. PPD-195 promotion freshness guards and PPD-200 plain-source transient history remain separate and must be preserved.


- 2026-11-15: Actual25 producer11788/53 passes including all authored transient/freshness scenarios after fixture observation counts corrected as PPD206. No physical OOM execution or owner verification claimed.

## PPD-206 — New forward/glow fixtures assume dependency observation is bypassed

- Status: investigating
- Confirmation: reproduced
- Owner: Codex actual native source fixture
- Discovered: 2026-11-15
- Original reference: ALScenePublicationNative::world always invokes observeMetalSourceDependencies before selecting a cached source. Direct ordinary forward requests retain that established observation boundary.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Required24/static69057 pass at10498116fc. Actual source suite passes51/53cases,11773/11788assertions. All15 failures are new dependency_walks==walks expectations:14 PPD205 scenarios plus one repeated post-water glow call. Each actual call observes once (3 versus2 or6 versus5), as the source explicitly requires. The new fallback identity/revision/publication and glow output checks pass.
- Next action: Correct only the two new fixture expectations to count one established dependency observation per world request; retain all source identity, ownership, cutoff and failure assertions. Rebuild/retest without changing production observation semantics.
- Verification: Executed assertion failure with source-confirmed fixture mismatch; production performance is not inferred from a stub invocation counter.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before correcting the fixture; retain native-correctness-24-actual-publish.log and its failed counts. Native material2513/6, shared shadow98414/14 and earlier cohort/renderer/bank/policy gates passed separately.

- 2026-11-15: Fixture-only7b49 rebuild25/static69058 and actual11788/53 pass. Both tests now count the established single dependency observation, retaining all ownership/fallback/cutoff/freshness assertions. Failed24 evidence preserved; automated checks do not close owner register.


## PPD-207 — Denied command-bank publication evicts usable warm banks

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native command publication
- Discovered: 2026-11-15
- Original reference: A warm immutable bank needs no new publication credit; denial of unrelated optional cold work must preserve usable cached generations.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). SourceCommands prepareForCommand and new source26 prepareCohortForCommand evict cache entries for bank/index capacity before invoking publication admission. With maximum_cached_banks=1, cached A and a new B whose admission is denied, B first removes A, then returns no bank. A subsequently cannot use its former warm hit under the same denial. No executed viewer incident or performance attribution is claimed.
- Next action: Make admission failure preserve cached warm banks, covering both bank-count and index-capacity eviction boundaries; add a focused native A/B-denial/A-hit regression. Keep all byte/live-bank bounds and completion credit semantics.
- Verification: Source-confirmed counterexample registered before repair; new native fixture not yet executed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered during source26 integration review. The existing generic path shares this ordering; cohort-per-group use makes preservation of warm cache particularly relevant.


- 2026-11-15: Source26 authored repair stages native bank/cache/index registry and one completion-owner retention before no-throw cache commit. Both APIs include A/Bdeny/A-hit and index-capacity fixtures; root bounded source review passes, native execution pending.

- 2026-11-15: Source29 corrected banks3348/9 and broader88CTest pass, including A/Bdeny/A-hit, index-capacity and pending/abandon cases. Owner/runtime acceptance remains pending.

## PPD-208 — Idle GPU cohort tables retain uncharged native texture generations

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex retained GPU cohort resource publication
- Discovered: 2026-11-15
- Original reference: Optional native caches require explicit bounded lifetime/accounting and must preserve existing usable publications on unrelated failed work.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Source26 CohortBinding owns strong native ResourceTables, whose native texture arrays retain MTLTexture generations. Cache native/cohort identities are weak, but without later cohort freeze an expired binding can persist after CPU generations disappear. Current metadata bounds count containers, not retained asset bytes. No executed memory leak or COW incident is claimed.
- Next action: Bound retained native asset footprint, prune expired cohort caches at the existing frame-owner boundary even when the experimental flag is off, report the separate retained footprint, and test expiry/flag-off maintenance/pending-command protection.
- Verification: Source-confirmed integration issue, registered before repair. Source26 is unbuilt.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered during coordinated SourcePages integration review; no viewer execution or performance attribution.


- 2026-11-15: Source26 authored repair counts unique native root texture allocations, derives cap from existing GPU working-set advice, reports it separately and prunes at ordinary FrameData.beginFrame. Pending FrameViews remain native readers; native fixtures authored, execution pending.

- 2026-11-15: Native27 pages1547/12 plus broad29 pass including cap/dedup/expiry and pending-reader checks; frame-boundary hook compiled. No owner/runtime closure.

## PPD-209 — GPU cohort cache evicts warm entries before allocating its new list node

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex retained GPU cohort resource publication
- Discovered: 2026-11-15
- Original reference: Optional native caches require explicit bounded lifetime/accounting and must preserve existing usable publications on unrelated failed work.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Source26 cold freeze evicts LRU cohort bindings, then calls cohorts.push_back, which may allocate and fail. The outer bad_alloc handler then returns failure after discarding usable warm bindings. No executed allocation failure is claimed.
- Next action: Stage the fallible list node before eviction, then commit via no-throw splice; preserve a usable uncached native FrameView if optional cache insertion fails, and retain bounded cache accounting.
- Verification: Source-confirmed integration issue, registered before repair. Source26 is unbuilt.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered during coordinated SourcePages integration review; no viewer execution or performance attribution.

- 2026-11-15: Source26 authored repair stages cache list/accounting allocations before eviction and returns valid uncached FrameViews on optional cache insertion failure. One-shot CPU allocation seam and preservation fixture authored; no actual OOM execution claimed.


- 2026-11-15: Native27 cache-insertion fault case and broader29 pass; valid uncached view/old warm membership preserved. Physical OOM not claimed.

## PPD-210 — New GPU cohort cost assertion fails Catch2 decomposition

- Status: investigating
- Confirmation: reproduced
- Owner: Codex integration fixture
- Discovered: 2026-11-15
- Original reference: Catch2 logical-OR assertions require parentheses around the complete expression.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Required26 at e7eba56957 passes doctor/configure but build fails58.746s at the new cost include line199: CHECK(!capture.source_rows||capture.source_rows->candidates.empty()). Compiler diagnoses unsupported operator|| decomposition. Production SourcePages/commands compilation did not report errors in that attempt. Strict26 passes3587files/905info/0blockers/errors.
- Next action: Parenthesize that complete fixture expression, rebuild from a fresh clean source pin and run native/cost gates.
- Verification: Executed compile failure; no production renderer failure attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before the single assertion repair; preserve failed required26 receipt/log.


- 2026-11-15: Fixture expression repaired before required27; required29/static69063 and broader gates pass. Failure26 retained.

## PPD-211 — Mixed renderer fixture omits its required CPU reference owner

- Status: investigating
- Confirmation: reproduced
- Owner: Codex mixed cohort renderer fixture
- Discovered: 2026-11-15
- Original reference: alMetalValidateWorldView requires both cpu_source_owner and source_owner for reference storage; retained cohort views carry their separate owners.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Required27/static69061 pass. Native page1547/12, material2513/6, shadow98414/14 and directcohort1763/7 pass. Expanded renderer suite3/4cases,2216/2217assertions passes its other new rendering/failure cases; mixed SIMPLE/material/PBR/raw/reference case fails incomplete scene at275 because it supplies reference_count=1 without cpu_source_owner. Validation correctly rejects it.
- Next action: Give the referenced prototype actual shared CPU ownership and provide that owner on the mixed scene. Preserve production validation and all pixel/query/order checks, then rebuild/retest.
- Verification: Executed fixture failure with source-confirmed missing input, no production render defect attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before fixing the mixed fixture; native-correctness-27 failure receipt retained.


- 2026-11-15: Shared referenced prototype owner repair passes source29 renderer2331/4 and broader gates; failure27 retained.

## PPD-212 — Native range fixture waits on an unattached visibility result batch

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native command range fixture
- Discovered: 2026-11-15
- Original reference: VisibilityResultBatch::attachCommand must associate its actual command before wait; production world rendering already does so.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Native27 source-command suite passes8/9cases,3195/3196assertions. Its new low-level range helper creates a query batch and binds its buffer but omits attachCommand; wait at570 correctly returns unavailable despite the submitted render. PPD207 denial/capacity/lifetime cases pass. Old-library renderer400/1 also passes.
- Next action: Attach the helper query batch to its exact command before encoding. Preserve real query sample checks, whole/ranged pixel parity, and production wait/device validation; rebuild/retest.
- Verification: Executed fixture lifecycle failure, no production query defect attributed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before correcting the low-level native fixture; retain native-remainder-27 failure evidence.


- 2026-11-15: The attempted private API repair failed as213; final native shared query-buffer fixture reads only after exact command completion. Source29 banks3348/9 and broad gates pass.

## PPD-213 — Query-fixture repair calls a private attachment method

- Status: investigating
- Confirmation: reproduced
- Owner: Codex integration fixture
- Discovered: 2026-11-15
- Original reference: VisibilityResultBatch command attachment is private to its production render owners. Low-level encoder fixtures can instead use an explicitly owned native query buffer and read it only after actual command completion, as almetal_indexed_join_test.mm does.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Required28 at5855c64132 passes doctor/configure but fails19.320s compiling the PPD212 attempted attachCommand call because that API is private. Root inspected the implementation without checking the declaration before the attempted repair. No production code changed.
- Next action: Replace only the low-level fixture query store with a shared native buffer, bind it on the native pass descriptor, and assert real counting results after commitAndWait. Keep higher-level renderer query/lifetime coverage and production private API unchanged.
- Verification: Executed fixture compile failure; preserve required28 receipt.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before correcting the root-authored private API call; no private API exposure or production validation relaxation authorized.


- 2026-11-15: Root correction uses existing indexed-join native pass/query pattern, no private API exposure. Required29/static69063/native/broad pass; failure28 retained.

## PPD-214 — Busy-frame CPU publication can starve older GPU metadata work

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex shared native frame admission
- Discovered: 2026-11-15
- Original reference: Bounded frame-wide publication must guarantee progress across CPU compilation and native GPU metadata, not only within one traversal phase.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). CPU postSort compilation runs before G-buffer native page/bank publication and shares Work::scene_publication. Busy/pressure progress requires scene_publications_this_frame==0; releasing a CPU-local credit does not reset that count. Two aged permanently ineligible compilation candidates can alternate admitted attempts, retain old pending age after seal failure, and consume that first slot every frame. An older valid GPU metadata request, called later, is then denied indefinitely. No executed viewer performance attribution is claimed.
- Next action: Provide bounded cross-phase progress fairness in the existing shared budget, including failed CPU attempts, older GPU requests and vanished pending owners; preserve idle quotas/bytes/completion retirement and direct fallback. Add a deterministic repeated-busy-frame integration model using actual CPU admission and GPU metadata policy.
- Verification: Source30 native frame-budget gate passes1690assertions/24cases, including five new repeated-frame fairness models. Owner runtime acceptance remains pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered from bounded G5 source audit while source29 fixture build runs. This is separate from the earlier shared shadow-credit integration PPD204.


- 2026-11-15: Source30 authored fixed two-category age/service arbitration plus5focused models; default metadata API and CPU kind routing, idle caps/leases retained. Read-only peer/root reviews find no blocker, execution pending.

- 2026-11-15: Source8691dc3430 required30/static69064/native/broad gates pass. Cross-phase progress cases execute in native-correctness-30.json; no live FPS attribution or owner closure.

## PPD-215 — GPU cost fixture applies steady-state assertions to its first warm-up

- Status: investigating
- Confirmation: reproduced
- Owner: Codex three-arm native cost fixture
- Discovered: 2026-11-15
- Original reference: A warm-condition fixture must first publish its just-invalidated state during the initial warm-up; measured steady-state samples begin only after the declared warm-ups.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Source29 three-arm run completes all144summary rows/2016samples but fails128 of140088assertions. Exactly32 each of getter/dependency/image/compile zero assertions fail at warm condition sample0 (warmup1), covering2families/4counts/2viewcounts/2cohortmodes. That first warm-up correctly performs the initial publication. All pixel/query and other assertions pass. Timings remain provisional because the gate failed.
- Next action: Apply only those four steady-state zero-work assertions to measured warm samples after warm-ups; retain setup costs/raw warm-up output and every other parity/admission check. Rerun after the coherent next build.
- Verification: Source30 corrected three-arm gate passes140840assertions/1case,144summary rows and2016samples. Initial warm-up costs remain recorded. No viewer performance acceptance claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before correcting the measured-sample guard; failed cost29 receipt/log retained without alteration.

- 2026-11-15: Root adds sample>=warmups guard only to four steady-state expectations. Warm-up raw rows and all parity/admission checks retained; source30 rerun pending.


- 2026-11-15: Corrected source30 cost gate passes; actual-gpu-cohort-costs-30.json/log retain all phases and parity checks. Warm CPU gains do not erase measured cold/churn/GPU regressions. Failed29 evidence retained.

## PPD-216 — Many-group cost fixture omits production pending/candidate container work

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex many-small-group acceptance fixture
- Discovered: 2026-11-15
- Original reference: Complete cost coverage must include the actual pending-range map and full cold-candidate storage used by postSort, especially for many small groups.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Peer review of new, unbuilt alpipeline_many_cohort_cost_native.inl identifies vector<Pending{Group*,raw_begin}> and vector<Pending*> where production uses unordered_map<Group*,vector<PendingGroupRange{type,raw_begin,count}>> and full ColdCohort records with shared publisher, admission pointer and flavor. Omitted per-group allocations/control copies materially narrow the intended loading-cost boundary. No executed benchmark result exists for this fixture.
- Next action: Mirror the production containers inside the existing timed region before including/building the new fixture; retain arm isolation, warm-up/loading observations and query/parity checks.
- Verification: Corrected source31 builds and hidden many-small gate passes1871896assertions/1case. Loading/warmup/measured costs are retained; no viewer FPS claim.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered from bounded peer review while source30 broader checks run.

- 2026-11-15: Author freezes corrected timed pending map, per-group vectors, full candidate records/control copies, repeated warm checks and expected-span allocation (new fixture SHA256 f3e9655c896565dfaa53db70d58a00a73ee83bac33b345c5c00de5c437276ba4). Root comparison with postSort confirms repaired boundary. Hidden fixture is included for test-only source31; execution pending.


- 2026-11-15: Read-only peer review passes corrected SHA. Required31 doctor/configure/build and static69065/strict pass; new native gate completes9.106s,24summary rows/2016raw samples, with all query/pixel/depth checks passing. PPD217 retains the newly exposed GPU cost regression.

## PPD-217 — Many-small retained GPU workload repeatedly rebuilds banks and falls back

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native cohort command admission/cache selection
- Discovered: 2026-11-15
- Original reference: Bounded native metadata must retain a useful working set and preserve ordinary direct rendering when GPU command preparation costs more than it saves. Original ordered group/query boundaries remain required.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Source14837e631a/69065 new many-small gate passes1871896assertions/1case, but GPU mode loses completion/query settlement in all8 measured scenarios versus direct cohorts (+28.3 to148.9%) and increases GPU time26.6–104.6%. Actual bank draw fraction spans24.7–100%; requested draws, denials, evictions, builds and raw fallback are retained. At64groups/2sources/1view, measured12frames request1536draws, execute492throughbanks, fallback1044, build165banks and evict177. These demonstrate repeated work despite64warmupframes; cache/admission/fallback costs are observed, not proved the sole cause. No viewer FPS regression is inferred.
- Next action: Select a bounded useful retained working set and avoid repeatedly rebuilding uncached small cohorts; preserve direct cohorts, ordered query boundaries, denial progress and exact completion lifetimes. Rerun the same complete cold/warm/churn and many-small costs before any bank activation. Do not raise cache limits blindly.
- Verification: Executed contained cost regression with passing pixel/depth/query parity; owner runtime acceptance pending. Command banks remain off in the owner preview launcher.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered after first passing many-small cost31 run and before a cache/admission repair. Raw loading/warmup/measured samples and the unchanged cache limits are retained.



- 2026-11-15: PPD218 exact-row repair removes false invalidation for fitting64group cases. Source32 full-bank steady reuse is proven, but GPU mode still loses completion/query settlement23.6–105.9% versus direct cohorts across all8many-small scenarios and increases GPU time24.5–136.1%.256group cases retain real bank pressure (192builds over12measured frames, about25–26%bankdraw fraction); no claim that repairing invalidation alone makes banks worthwhile. Keep bank mode off; next investigation is useful retained per-pass publication/command granularity and stable generation admission, not larger cache limits.

- 2026-11-15: Source36 six-qualifier constant-region hypothesis passes required/static/strict/all21native and unchanged3arm/many costs, but35-versus36 GPU effects vary widely without consistent advantage. Root reverts only six shader qualifiers for source37; failed performance hypothesis and exact control remain retained. No causal GPU-cache or viewer FPS claim;217 stays open.

- 2026-11-15: Source38 ordered renderer costs pass; all8many scenarios have100%banked draws/no warm rebuild/fallback and CPU10.60–51.39%lower, but GPU87.41–166.90%higher and settlement8.94–142.52%higher. Keep bank modeOFF; investigate direct static bindings with fixed-address native view slots as a bounded new hypothesis, not a measured repair.

- 2026-11-15: Source41 direct-bank native gates and both4arm cost matrices pass. Manydirect versus oldbank improves all8CPU/GPU/settlement, but versus direct GPU remains37.22–102.17%higher andcomplete wins2/8. Warm512complete wins6.51–42.06%; cold/churn remain adverse. Keep controlOFF, extend realuniqueindexedmesh coverage42 before generalizing3-index fixtures.

## PPD-218 — Page-wide freshness invalidates unrelated retained cohorts

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native source-page cohort freshness
- Discovered: 2026-11-15
- Original reference: Immutable publication must invalidate dependencies that actually changed. Current physical page copy-on-write must still preserve every submitted reader and exact source revision/allocation generation.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). SourcePages::changed(page) advances page_content on every row allocation/write; current(CohortBinding) compares whole-page stamps. In64groups/2sources,128rows share one page: publishing one unrelated row invalidates all other bindings on that page. pruneCohorts drops their native table/membership owners, which can make existing row.table weak proofs expire and cause further row writes; SourceCommands then removes expired memberships. Measured source31 cache uses only8–40banks (limit64), zero capacity failures;473pagefailures are admission denials. Actual fallback1044draws comprises946source_setup and98bank_admission. The repeating page-wide invalidation loop is source-confirmed, distinct from a claim that cache capacity is exhausted or that it is the sole cost cause.
- Next action: Replace whole-page cohort freshness with precise bounded row-dependency invalidation, retaining constant-time warm qualification, native-only idle ownership and exact per-command page/bank reader leases. Register dependencies before cache commit, handle allocation rollback, invalidate before own-row mutation, and unregister on cache eviction. Add unrelated-row growth/update, own-row rewrite/reuse, pending-reader and fault/rollback coverage; rerun complete costs.
- Verification: Source3fa3820969/69067 required/static/strict and all20 native processes pass; SourcePages3644assertions/17cases includes new sibling/pending and bounded convergence cases. Many-small1872424/1 passes strict64group full-bank/zero-rebuild assertions. No viewer performance or owner closure claim.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before row-selective repair. Three independent source/counter reviews agree caps are not binding in the64group case; source31 evidence and owner preview remain untouched.

- 2026-11-15: PPD218 source freeze replaces page stamps with exact row reverse dependencies owned by stable cache entries. Node arrays are staged before fallible cache insertion/asset accounting and charged in existing metadata cap; warm qualification is constant time. Two focused native cases and stricter64group measured cost assertions authored. Root/independent lifetime reviews pass; required32/native/cost execution pending.

- 2026-11-15: Required32 doctor/configure/build exit0, build34.304s; static69067 and strict3587files/0blockers/0errors pass. Native20processes pass, including pages3644/17, banks3348/9, actualproducer12397/53 and source-admission1874/6. Corrected3arm cost140840/1 and many-small1872424/1 pass. All four64group measured scenarios have fullbank use and zero page/bank rebuilds/denials/fallback.64x2x1 cost CPU0.766479ms before to0.187416ms after, nativebankdraws492/1536 to1536/1536. Owner preview69065 remains untouched. PPD217 stays open for remaining GPU-time/true pressure costs; no unconditional activation or owner verification inferred.


## PPD-219 — Optional bounds promotion rejects a valid withdrawn transient source

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex retained visibility publication
- Discovered: 2026-11-15
- Original reference: Optional GPU preparation must preserve required ordinary native rendering over an exact immutable reader-held generation after cache-only withdrawal. PPD195/205 source-identity, revision and replacement guards remain required.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). In the unbuilt retained-visibility worktree, raw NativeVisibility requests a non-key bounds capability. world() can find an ordinary variant retained by the current reader, while its optional-promotion original_current still requires the child's State.publication()==that payload. Cache-only withdrawal removes that publisher but not the exact reader lease, causing an empty required source instead of fail-visible ordinary rendering. The earlier transient guard excludes this newly introduced bounds-only native-material request. This is source-confirmed in new code; no observed viewer regression or executed test result is claimed.
- Next action: Extend the exact authored-transient fallback guard only for optional bounds. Preserve no resources/cohort-source promotion, no child-cache resurrection, exact identity/revision/transient owner, and newer-payload rejection. Add actual-owner withdrawn-reader bounds fallback plus revision/newer replacement controls before build.
- Verification: Registered before repair; source review only. Existing PPD195/205 tests/guards must remain and execute with the new capability.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Architecture source owner reports this concrete pre-freeze defect before repair in codex/apple-first-retained-cohort-visibility. Root allocates219; next220. Owner69065 app and tested source32 remain unchanged.


- 2026-11-15: Exact transient bounds-promotion and actual owner revision/newer-generation protections pass source35 native gates; owner verification remains pending.

## PPD-220 — Retained visibility arguments lack exact cohort generation qualification

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex retained visibility consumer
- Discovered: 2026-11-15
- Original reference: Optional culling may suppress required geometry only when its bounds/arguments belong to the exact immutable source and native generation being rendered. Mismatched optional preparation must fail visible through ordinary native rendering.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). In the unbuilt retained visibility consumer, generic Arguments::appliesTo(index,count,MVP) can accept a view created from a different certified cohort/native generation when logical range and MVP coincide. The new cohort API has exact certificate and CPU/native control-block identities available, but this generic consumer check does not compare them. No observed viewer incident or executed failure is claimed.
- Next action: Add appliesToCohort(index,count,CohortView) and use it only for retained cohort indirect selection. Require exact pointer/control-block ownership plus matching copied MVP/range. Wrong cohort/native-generation arguments must retain required direct rendering; store qualification identities weakly so GPU retirement does not retain CPU source graphs. Add swapped-generation/cohort negative tests and execute pending-lifetime coverage.
- Verification: Source-confirmed pre-freeze qualification gap registered before repair; native execution pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Architecture source owner reports before repair; root allocates220, next221. Existing raw trusted metadata contract is unchanged; no renderer/backend default activation authorized.


- 2026-11-15: Exact current cohort/native/MVP qualification and both raw/cohort mismatch directions pass source35 visibility2506/9; owner verification pending.

## PPD-221 — Retained visibility native row cap charges logical instead of allocated bytes

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex retained visibility native row storage
- Discovered: 2026-11-15
- Original reference: Apple unified-memory admission must account for actual retained native allocation debt; small logical records can occupy larger physical Metal buffer allocations.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). The unbuilt new cohort visibility storage charges48logical bytes per row against its8MiB live native cap. Independent MTLBuffer allocations may have larger allocatedSize, so many small buffers can exceed the cap physically while appearing within it. One attempted allocation is limited by the existing65536row per-view bound; no arbitrary-size allocation or observed viewer OOM is claimed.
- Next action: Charge immutable MTLBuffer.allocatedSize after creation and reject unknown/over-cap debt before retention/encoding. Keep logical upload bytes separate, subtract the exact admitted charge at native buffer retirement, and add small-row actual-debt/cap rejection coverage.
- Verification: Source-confirmed new-path accounting gap registered before repair; native execution pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Architecture source owner reports before repair; root allocates221, next222. No geometry/asset residency ownership or default activation is added by this correction.


- 2026-11-15: Source35 native tests verify physical allocatedSize accounting for small retained row buffers; large cap exhaustion/recovery is not yet tested or claimed.

## PPD-222 — New retained visibility cold publication bypasses shared frame admission

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex retained visibility and FrameData admission
- Discovered: 2026-11-15
- Original reference: Optional native metadata publication must coordinate with the existing frame-wide main/probe/CPU compilation/streaming policy and retain credits only through actual command completion. A private memory cap is not frame admission.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). The unbuilt new Visibility::encode cold cohort row cache and dirty raw bounds generation allocate/write optional native metadata without invoking FrameData's stored source-publication callback. Existing8MiB live storage bounds retention, but does not schedule this work with main/probe publication. Source owner confirms the missing callsite before repair. No observed viewer stall is attributed.
- Next action: Accept optional FrameData at encode, coalesce only new cohort/dirty or COW raw bounds work, request existing shared metadata admission before native allocations/writes, and retain granted credit on the exact command. Denial returns no optional visibility arguments so required direct rendering continues. Unchanged warm argument/compute output requires no extra publication ticket. GroupSources raw fallback must avoid optional per-source bounds scans after group admission denial. Preserve standalone API compatibility and add real denial/warm/retirement/cost counters; no new scheduler.
- Verification: Source-confirmed new integration gap registered before repair; native execution pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Root requests G5 scope audit; source owner confirms missing callback and proposes existing-policy wrapper. Registered222 before repair, next223. Memory debt221 and exact generation220 remain separate invariants.


- 2026-11-15: Source35 admission2248/8 and visibility2506/9 verify shared publication denial/direct fallback, warm zero-ticket, age, abandonment/completion and reentrant generation guards.

## PPD-223 — Cold persistent cohort region mapping bypasses publication work charge

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex ordered-pass source publication
- Discovered: 2026-11-15
- Original reference: A persistent native mapping created for an optional retained composition is publication work. Required transient per-view camera/global buffers remain distinct.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). In the frozen unbuilt ordered-pass backend, freezeInternal computes mapping_bytes/view_bytes for a persistent region map but does not include its writes in SourcePublicationWork::chargedBytes(). Resource preparations return0publicationBytes when an existing table is reused. A new composition reusing valid rows/tables can therefore allocate/copy its persistent multi-region map with charged==0, bypassing the existing admission callback. The earlier exclusion for required per-view maps does not cover this new retained cache allocation. Root confirms exact charge and existing-table code paths; no runtime incident is attributed.
- Next action: Charge persistent certified-cohort mapping writes explicitly before allocation/publication, preserve ordinary transient per-view buffer behavior and zero warm tickets, and add a mapping-only cold composition denial case over already reused rows/tables. Verify no allocation/cache eviction on denial and exact command-only credit retirement.
- Verification: Sourcef78f994b64/69073 required/static/strict and all20native processes pass. SourcePages5364assertions/22cases includes mapping-only denial, old-warm preservation and actual command abandonment/completion; shared admission1993/7 also passes. Actual renderer composition integration/performance remains pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Root finds this after independent backend review, before first backend build; allocates223, next224. Source32/owner69065 unchanged. Physical storage and logical work accounting remain separate.

- 2026-11-15: PPD223 repair frozen/reviewed in ordered-pass backend: encoded_mapping_bytes participates checked chargedBytes, allocation diagnostic includes retained map while local capacity counts it once; warm and ordinary transient view maps unchanged. Added mapping-only8B denial/old-warm/grant-abandon/gated-completion case. Independent/source reviews pass; no native execution yet.



- 2026-11-15: Required33 doctor/configure/build0 (build163.914s), static69073/346ARM64/signatures/source/imports and strict3587files/0blockers/0errors pass. Native20processes pass, pages5364/22 incl5ordered-pass cases, banks3348/9 andactualproducer12397/53. Existing-single3arm140840/1 andmany1872424/1 regressionsuites also pass; no pass-renderer consumption or performancegain claimed.

## PPD-224 — Optional visibility capture allocation failure discards required geometry

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native world capture optional metadata
- Discovered: 2026-11-15
- Original reference: Failure of optional visibility metadata must fail visible and preserve admitted required geometry. Required geometry/source allocation failures retain their established incomplete-capture handling.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). captureMetalPublishedWorldBatch currently places optional visibility vector growth/publishBounds inside its outer bad_alloc handler, which marks the whole capture incomplete and clears required draws. The retained visibility sparse index vector adds another optional allocation at that boundary. Source owner confirms this existing/new-path failure boundary before repair; no observed viewer OOM/crash is attributed.
- Next action: Catch only optional visibility preparation allocation failure locally, clear/disable that capture's optional metadata and skip optional view construction while retaining raw/ref/cohort geometry/order/native owners. Preserve required-source/geometry OOM behavior. Add actual-function failure injection verifying ordinary rendering/query output and no repeated optional work after disable.
- Verification: Source-confirmed pre-freeze failure-isolation defect registered before repair; native execution pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Architecture source owner reports before patching sparse capture; root allocates224, next225. Existing owner-passed apps and source33 backend build are unaffected.


- 2026-11-15: Source35 actualproducer14269/58 includes optional visibility metadata fault injection preserving required geometry/query inputs; owner verification pending.

## PPD-225 — Visibility cost labels requested offscreen fraction as actual for one source

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex retained visibility cost fixture
- Discovered: 2026-11-15
- Original reference: Performance evidence must identify the geometry workload actually submitted, including integer rounding in small adverse cases.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Pre-execution peer review of new alpipeline_cohort_visibility_cost_native.inl finds requested outside_percent=50 with1group/1source displaces0sources because the odd-source predicate never selects the only source. That row duplicates actual0% while labeled50%. Other even-source shapes realize the intended fraction; no executed benchmark result exists yet.
- Next action: Keep the requested percentage and adverse one-source scenario, and emit actual outside source count and effective fraction in every raw and summary row. Do not claim a fractional source was culled or count that label as evidence of50% actual offscreen geometry.
- Verification: Registered before narrow fixture-only label correction; no production changes or native results claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Independent cost peer reports before feature freeze/build; root allocates225, next226. All unexecuted cost evidence remains unclaimed.



- 2026-11-15: Fixture-only correction emits requested_outside_percent, actual_outside_sources and effective_outside_percent inraw+summary rows; adverse1source requested50 reportsactual0. Correctedfile SHA474be95c5baf7a435a58b0cca566af07af6bd59eca47130d03f0d54a4e86bdf8; diffcheckpasses, executionpending.

- 2026-11-15: Source35 cost gate passes with explicit requested/actual/effective offscreen output; one-source requested50 correctly reports actual0. No fractional geometry or independent unique-scenario claim.

## PPD-226 — Retained visibility implementation uses incomplete world types

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex retained visibility integration
- Discovered: 2026-11-15
- Original reference: Objective-C++ implementation units require complete declarations before accessing native batch members or FrameData methods; the backend deliberately has no precompiled header.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Final root review of almetalworldvisibility.mm finds new accesses to ALMetalWorldSimpleBatch::index_count and ALMetalWorldFrameData::requestSourcePublication. Its included cohort/draw/source-pages headers only forward-declare those world types; almetalworld.h supplies their definitions. Generated visibility source contains shader text, and almetal/CMakeLists explicitly avoids a backend PCH. No compiler attempt has been made yet.
- Next action: Include almetalworld.h directly in this implementation before first feature build, retaining the public forward declarations and existing ownership/API behavior.
- Verification: Source-confirmed missing direct include registered before the one-line integration correction; required34 compilation will verify it.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Root identifies this after complete feature freeze and before committing/integrating the slice. No production behavior or native test relaxation is involved; next227.

- 2026-11-15: Root adds direct world.h include after source-owner notification; no other source change follows full visibility freeze. Required34 compile/native/cost gates next.


- 2026-11-15: Direct include compiles successfully in required35 and native visibility passes; no owner runtime result claimed.

## PPD-227 — Visibility cost fixture restores a const device pointer to mutable owner state

- Status: investigating
- Confirmation: reproduced
- Owner: Codex retained visibility cost fixture integration
- Discovered: 2026-11-15
- Original reference: The fixture must preserve the actual pipeline device pointer type while swapping independent arm contexts; no production API constness change is needed.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Required34 at60a28bf449 passes doctor/configure but build fails59.265s in new cost fixture line62. MaterialCacheScope stores previous_device as constALMetalDevice* then assigns it back to mutable Inputs::sNativeSceneVisibilityDevice in its destructor. This discards qualifiers and Clang rejects it. Failed34receipt/log retained; no native execution occurred for this source.
- Next action: Match the saved device field to the actual mutable owner pointer type only. Keep arm swapping, all workload/parity/admission assertions and production APIs unchanged; rebuild as35 and execute required gates.
- Verification: Executed fixture compile failure; correction/retest pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before the one-declaration fixture correction; next228. No successful build34 or performance result claimed.


- 2026-11-15: Narrow saved-pointer type correction afe694394b passes required35 build and all21 native processes; failed34 receipt/log retained.

## PPD-228 — Cohort bank mode implicitly enables the slower scalar source-row capture

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex ordered-pass capture integration
- Discovered: 2026-11-15
- Original reference: Unsupported/denied retained group preparation must keep required ordinary native rendering. Enabling cohort command preparation must not silently select a different experimentally regressive per-source route.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). pipeline.cpp beginMetalWorldGBufferCapture calls beginSourceRows((source_rows()||source_banks())&&!impostor_bake), so bank mode enables scalar SourceRows even when GroupSources is on and SourceRows/CompactInputs controls are off. Raw fallback therefore captures per-source candidates/references and their known costs. Earlier audits recorded this behavior; ordered-pass consumer integration now reaches the concrete correction boundary. No new viewer FPS incident is attributed.
- Next action: Preserve explicit SourceRows and prior non-GroupSources bank semantics, but activate the implicit scalar path only for bank mode with GroupSources off. Update actual capture/cost fixture policy and add unsupported/denied mixed raw/cohort fallback assertions without scalar expansion. No default activation or required feature removal.
- Verification: Source-confirmed capture callsite before repair; integration/native/cost tests pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Architecture renderer owner reports callsite outside its ownership; root allocates228 and exclusively patches pipeline.cpp in consumption worktree. Next229.


- 2026-11-15: Root integrates capture policy, ordered renderer and actual mixed raw/cohort CPU/GPU-denial fixture for source38. Existing explicit SourceRows/nonGroupSources semantics retained; native execution pending.

- 2026-11-15: Source39 actual-owner14537/59 passes the complete mixed raw/cohort CPU/GPU-denial fixture after232setup correction, retaining no scalarSourceRows/resource-descriptor expansion and exact output/query checks. Source38 renderer5046/8 and full accepted-bank cost checks also pass; owner runtime pending.

## PPD-229 — Retained frustum visibility remains a net cost in cheap draw workloads

- Status: investigating
- Confirmation: reproduced
- Owner: Codex retained visibility dispatch/selection
- Discovered: 2026-11-15
- Original reference: Useful native visibility must remove enough work to repay capture, dispatch and indirect draw costs. Preserving ownership/correctness alone does not justify default activation.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Source35 corrected new cost gate passes5672016assertions/1case,504summary rows/14064raw samples. Against direct cohorts, visibility wins CPU only5of168configuredscenarios and GPU only4of168; warm actual50%offscreen has0of22CPU/GPUwins. Many warm50% loses settlement34–72% and GPU123–184%, despite zero warm row uploads, expanded source members, admission tickets or fallback. Geometry is explicitly3indices/source; these are cheap-draw contained results, not proof about heavier mesh workloads or viewer FPS. Current encode issues one dispatch per cohort and CPU consumption qualifies per draw; their separate cost shares are not yet measured.
- Next action: Keep control off; investigate batched dispatch, cheap coarse acceptance/rejection over immutable bounds and useful geometry-cost selection. Extend workload geometry coverage before claiming general benefit, preserve full capture/query/parity/admission boundaries, and never hide fallbacks or offscreen rounding. Remaining CPU direct bindings and frustum-only scope stay explicit.
- Verification: Executed performance rejection with passing geometry/MRT/depth/query correctness. Whole-engine owner performance acceptance remains pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered after first passing visibility35 gate before any dispatch/selection repair; raw logs/summary and current defaultsOFF retained. Next230.

- 2026-11-15: Batched retained-cohort dispatch assigned as a bounded repair attempt; test-only heavier indexed mesh coverage assigned separately. No default activation or measured repair success yet.


- 2026-11-15: Heavier baseline37 passes5532576/1 with144summary rows/6144raw samples, but many warm50% still loses GPU27.48–44.98% and CPU12.29–15.73%. Integrate one packed multi-cohort dispatch with separately bounded/reported native descriptor debt; native parity/cap/lifetime/admission and allcheap/heavy cost retests pending.

- 2026-11-15: Source38 packed dispatch passes6110352/1 cheap and5551008/1 heavy cost gates. One dispatch/view is proven; warm96B/group descriptor uploads are reported (not zero total uploads). Heavy many50% GPU−21.11..+11.73%, settlement−11.41..+0.79%, CPU+7.91..+14.55%; conditional selection/remainingCPUwork remain open. Native7521/13 passes true cap/borrowed/pending lifetime cases.

## PPD-230 — New heavy-mesh fixture can miss all target samples

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex retained visibility mesh-cost fixture
- Discovered: 2026-11-15
- Original reference: Geometry-cost parity must include real visible coverage; matching empty outputs cannot prove useful raster/depth correctness.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). During authoring, source review finds the new32x32 triangle grid gaps align with16px target pixel centers. The heavier helper could therefore compare empty images. No native failing run or production defect is attributed; original3-index workload remains unchanged.
- Next action: Shift internal grid phase without changing bounds/endpoints and require non-clear depth coverage for heavier cases before accepting their parity/cost evidence. Execute the full new geometry tag after integration.
- Verification: Source review during fixture authoring; corrected mesh and non-empty coverage assertion are authored but unexecuted.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Fixture owner reports sampling trap before freeze; root assigns230 and retains attribution separately from production PPD229. Next231.

- 2026-11-15: Source37 heavier gate5532576/1 passes non-clear depth coverage plus full output/query parity; phase correction and original cheap workload retained. Owner verification remains pending.


## PPD-231 — Private bundle verifier requires the displaced single-cohort consumer symbol

- Status: investigating
- Confirmation: reproduced
- Owner: Codex architecture bundle evidence
- Discovered: 2026-11-15
- Original reference: Compiled feature checks must verify the implementation actually used by the native renderer; dead-stripped unused wrappers are not product requirements.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Source38 required build7246b19e96 succeeds, but static-native-38 fails gpu_cohort_native_view because private verify-g2g5-bundle-30.py requires defined freezeCohortForCommand. Ordered renderer now calls freezeCohortsForCommand; the unused single-cohort wrapper is dead-stripped from the viewer. SourcePages single API compatibility is still covered in contained native tests. No production link/render failure is attributed.
- Next action: Preserve old verifier/failing receipt; create private verifier38 requiring the actual plural compiled symbol plus composition and batched visibility census/kernel evidence. Retest the same unmodified source38 bundle under a new receipt.
- Verification: Executed static marker failure after successful build; native rendering suites are still running. No original verifier or app modification is needed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before updating the private evidence verifier. Source38 build/source/binary remain frozen; next232.


- 2026-11-15: Corrected private verifier38 passes exact unchanged69079 bundle, requiring plural compiled consumer plus composition/descriptor census and packed kernel markers. Original failed static38 receipt and old verifier remain retained.

## PPD-232 — New mixed fallback fixture assumes zero disables scene admission

- Status: investigating
- Confirmation: reproduced
- Owner: Codex ordered renderer denial fixture
- Discovered: 2026-11-15
- Original reference: ALNativeFrameBudget normalizes scene_publications_per_frame to at least one; denial tests must exhaust an actual frame quota without changing production admission semantics.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Source38 actual producer test has59cases/14531assertions,58cases/14523assertions pass and eight checks fail in the newly added PPD228 case. The fixture sets limit0 then expects its first CPU compile to be denied; budget.cpp:20 clamps0 to1, so producer/publication/layout are correctly created. All later raw/MRT/query/native-bank-denial assertions pass. No production admission regression is attributed.
- Next action: Set explicit limit1, acquire and release the one scene credit before the CPU compile, then require per-frame count1 and in-flight0. Keep every existing no-producer/no-publication/ordinary raw/query/GPU-denial assertion. Rerun the complete actual-owner scope.
- Verification: Executed fixture assumption failure; native renderer5046/8 and visibility7521/13 pass in the same source. Failed38 receipt/log retained; repair/retest pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before fixture-only correction. Production budget limits and renderer code remain unchanged; next233.

- 2026-11-15: Explicit limit1/acquire/release fixture repair independently reviewed against budget retirement and per-frame checks; all original assertions retained. Required39/complete actual-owner retest pending.


- 2026-11-15: Explicit quota exhaustion correction1630611063 passes source39 actual-owner14537/59 with all original denial assertions retained. Native tail and broader88CTest+10specialization+3archive processes also pass; failed38history retained. No production budget change.

## PPD-233 — New direct-bank scene guard is authored in the renderer factory

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex direct-bound bank renderer integration
- Discovered: 2026-11-15
- Original reference: Scene validation belongs in renderInternal, where the scene exists; renderer construction is independent of per-view settings.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). During first unbuilt WIP in isolated direct-bound-cohort-banks tree, generic patch context places source_direct_command_banks validation in create() near world.cpp:3548. That function has no scene parameter. No build, native test or owner incident is attributed; source39 product is unaffected.
- Next action: Move only the scene guard to the exact renderInternal validation boundary before feature freeze/build. Keep factory behavior unchanged and preserve old bank control semantics.
- Verification: Author source review before compilation; correction and executed gates pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Source author reports before moving the guard; root assigns233. Next234.


- 2026-11-15: Source author moved guard to exact renderInternal boundary in frozen world.cpp; no feature build/native execution yet. Source39 remains unaffected.

- 2026-11-15: Source40and41 compile successfully with guard at renderInternal;41 renderer9394/9 includinginvalidconfiguration/oldlibrary directfallback pass. No owner runtime attribution or closure.

## PPD-234 — Direct-bank WIP applies the index/ICB ceiling to all retained scene assets

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex direct-bound command resource admission
- Discovered: 2026-11-15
- Original reference: Native command allocation/index retention and gross existing scene-asset retention are distinct scopes. SourcePages already resolves its asset ceiling from the device recommended working set; shared assets are not new ICB allocations.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Unbuilt direct-bank WIP sourcecommands.mm:124–145 adds binding_bytes (including geometry/numeric/tables/root textures) to the existing64MiB ICB-plus-index admission sum. A legal material/resource set can exceed this before any large ICB allocation, so the new experiment can reject useful banks due to an unrelated inherited ceiling. No runtime rejection or source39 regression is attributed.
- Next action: Preserve existing64MiB command/index bound. Account and bound the additional deduplicated gross binding-asset retention separately using the existing hardware-derived SourcePages working-set policy, including pending generations and idle pruning. Report both scopes/caps and test actual denial/recovery; do not increase existing command/index limits or count overlapping gross retention as new physical allocation.
- Verification: Root source/policy review during first unbuilt implementation; correction and executed tests pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before correcting WIP admission scope. Source39 and original bank mode remain unchanged; next235.

- 2026-11-15: Backend author separates extra binding-asset retention from unchanged64MiB ICB/index scope, reusing exact resolved SourcePages asset limit via FrameView; pending/idle scope and independent eviction predicates authored. Native separation/denial/retirement tests pending; no raised existing cap or duplicate policy.


- 2026-11-15: Source41 commands4055/12 verifies independent native asset cap versus unchangedICB/index cap and denial/retirement; role-overlap safeguards236 included. Broader/cost gates pass, owner pending.

## PPD-235 — New direct-bank many-group cost gate omits strict warm-reuse acceptance

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex direct-bound actual-cost fixture
- Discovered: 2026-11-15
- Original reference: Fitting64group native-bank scenarios must prove complete accepted work and zero warm N-command reencoding, new policy/slot allocation, admission, eviction or fallback; logging alone is weaker than this mechanism gate.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Before first direct-bank build, new many-cost mode3 checks requested/bound/fallback only. Its new counters are emitted, while old mode2 additionally requires hits and zero builds/evictions/page failures/admission denials. A mode3 rebuilding every warm frame could still pass the new gate. No executed failure or performance outcome is attributed.
- Next action: Extend only fitting64group measured warm mode3 checks with exact composition hits, zero bank builds/direct encoded calls, zero policy/slot allocations/binding work and unchanged admission/eviction/page counters. Preserve loading and256pressure rows without assuming their warm success.
- Verification: Root pre-build fixture review; narrow strengthened assertions and execution pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before unfreezing only the new mode3 assertion hunk. Next236.


- 2026-11-15: Narrow fitting64 measured direct-mode assertions now require exact2compositions/view cache/page/direct/slot hits, zero bank/page builds/evictions/failures/denials and zero direct encoded calls/policy/slot allocation/binding charge. Loading and256pressure unchanged; source40 execution pending.

- 2026-11-15: Source41 newmany4arm cost2568360/1 passes strict64 measured reuse with fullacceptedwork/zeroNreencodes,policy/slot allocation,bindingcharge/denial/eviction. All32summaryrows/2688rawsamples retained;256adverse cases unfiltered.

## PPD-236 — Failed direct-bank staging permanently changes a shared buffer accounting role

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex direct-bound native accounting
- Discovered: 2026-11-15
- Original reference: Failed optional preparation must preserve existing valid native owners and their budget accounting. A native buffer may legally serve both geometry and indices; actual active leases determine scope.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Post-freeze source review finds Commands::markIndex reclassifies an existing binding-only shared native lease before new admission/capacity success. BankA can bind allocationX as geometry, then denied BankB stages X as indices. A still owns X but accounting remains in the index role, changing live scope after failed work. LLVB commonly separates allocations, but the helper accepts legal aliasing; no runtime incident or source39 regression is claimed.
- Next action: Remove destructive role promotion. Track/deduplicate actual index and binding leases by role with transactional staging, retaining support for native aliases. Scope counters may overlap for a dual-role allocation and must not be summed as unique physical RAM. Preserve all existing limits, old valid bank reuse and native completion/abandon retirement; add denied-alias regression coverage.
- Verification: Source author reports before repair; executed gates pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered after initial source freeze but before any direct feature build. Only command accounting/header comments and its native fixture unfreeze for this repair. Next237.


- 2026-11-15: Immutable allocation+role registry repair and actual legal combined-buffer Ageometry/Bindex denial/retirement/recovery fixture are frozen. Independent gross scopes may overlap and are not summed as physical RAM; native execution pending.

- 2026-11-15: Source41 command4055/12 includeslegalcombinedbuffer Ageometry/Bindex deniedpreparation, unchangedolddebt/cache, actualGPUreader retirement andproperrecovery output. Allnative/broad gates pass; owner pending.

## PPD-237 — Generic inherited-bank API accepts a direct-binding view

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native command ABI validation
- Discovered: 2026-11-15
- Original reference: A frozen view must be consumed with its actual native binding ABI. Direct-bound views declare resources and rely on non-inherited per-command buffers; they cannot supply the old inherited source-row state.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Root pre-build review finds generic prepareForCommand validates source/index rows but does not reject frame.directBindings(). matchesSourceCommand accepts private cohort membership, so a valid direct frame can reach generic inherited ICB construction. Direct FrameView::bind declares native resources without binding the old row/view arrays. Production renderer uses prepareCohortForCommand correctly; no runtime incident is attributed.
- Next action: Reject direct-binding frames at the generic inherited API entry before staging/admission. Add valid-direct-frame wrong-API rejection followed by proper direct preparation/output recovery, retaining ordinary/generic compatibility.
- Verification: Source-confirmed consumer ABI mismatch before first feature build; correction/test pending.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Root registers before narrowly unfreezing Commands implementation/native fixture. Next238.

- 2026-11-15: Generic inherited entry guard now rejects direct frames before staging/admission. Test supplies matching nonempty certified descriptors, checks callback0/unchanged cache/native/copy debt then proper cohort Legacy/PBR output/query recovery. Source40 execution pending.


- 2026-11-15: Source41 commands4055/12 proves matchingnonemptydirectframe descriptors rejected atwronggenericAPI withcallback0/noallocateddebtchange, thenproperdirectAPI renders Legacy/PBR output/query recovery. Oldgeneric/oldlibrary controls pass.

## PPD-238 — New slot-pressure fixture exceeds the Metal command-queue capacity

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex direct-slot native fixture
- Discovered: 2026-11-15
- Original reference: almetaldevice.mm creates its queue with max64 non-completed command buffers; the installed MTLDevice.h documents that upper bound. Slot pressure can involve many view occurrences on one command and must not require more queue buffers than the device permits.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Before first source40 native run, root review finds the new128slot test creates128uncommitted native command buffers plus denial/blocker commands. Queue construction explicitly caps64; the fixture can block before testing slots. No hung process or runtime viewer incident occurred.
- Next action: Keep128slot pressure but use only a few native commands: separate abandon/GPU-gated commands for the first two views, and one shared uncommitted command retaining the remaining126distinct view-slot readers. Preserve full-cap denial, CPU-borrowed protection, actual GPU completion and slot-reuse assertions. Do not raise production queue or slot limits.
- Verification: Source/installed SDK confirmation before any native execution; fixture repair pending. Source40 build may finish but this fixture must not run until corrected.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered during source40 build after root queue-cap review. Unfreeze only existing source-pages native test; no production change. Next239.

- 2026-11-15: Fixture-only repair uses3commands for128view slots plusdenial/blocker(total5), preserving all128cap/borrowedCPUreader/actualGPUcompletion/exactstorage-reuse/no-allocation assertions. Production queue64 and slot128 unchanged. No native test or hang occurred for40; full41execution follows.

- 2026-11-15: Source41 pages5914/25 finishes normally and proves128distinctslots on bounded nativecommands, borrowedCPUreader protection, actualGPUcompletion andsameallocation recovery. No unmodified238fixture was executed/hung; productionqueue64 unchanged.


## PPD-239 — Stability helper can recreate pending age on a stale call

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex optional cohort stability admission
- Discovered: 2026-11-15
- Original reference: Stale/duplicate owner-frame consideration must not create new pending work or alter service age after publication completion.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Unbuilt stable-cohort-admission helper calls notePending(frame) before rejecting stale/duplicate frames. After publicationCompleted clears pending state, a stale standalone helper call can recreate an older pending age. The wrapper already short-circuits duplicate/stale work and actual frame budgets do not rewind; no source42 or runtime incident is attributed.
- Next action: Reject stale/duplicate frames before notePending and add completion-then-stale regression coverage requiring pending to remain absent; preserve valid unstable pending age and eventual eligibility.
- Verification: Source author reports before repair; no build/test execution.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Registered before reordering the new helper. Next240.


- 2026-11-15 source43 6af308d165/69085 required build, static/strict and all21native processes pass. Common stability/frame-budget5091assertions/27cases and actual owner15220/60 include stale-after-completed regression plus current raw pixels during deferral. Owner confirmation/push remain pending; no closure from tests.

## PPD-240 — Trial cohort publication failures lack a diagnostic reason breakdown

- Status: investigating
- Confirmation: reproduced
- Owner: Codex native cohort publication diagnostics
- Discovered: 2026-11-15
- Original reference: Required raw rendering remains available after optional compilation failure; failures/unsupported declines must be distinguishable from GPU errors and cannot justify repeated work without source evidence.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Verified private69065/source14837e631a8e96492bb3037de24dd1c1a8030ac5 trial snapshot reports39088admitted/1149230frame-limit-denied/35576published/3512failed compilation attempts. Failure totals decompose into2252unsupported declines and1260publication failures; no reason breakdown identifies the1260. Counters are process-cumulative main/probe and raw fallback is retained. No CPU/FPS regression or dropped draws are inferred.
- Next action: Trace exact source factory return paths and record bounded typed reason counters before any retry/caching policy. Preserve required rendering and source generations. Evaluate repeated small-group work with actual1.59draws per captured group occurrence and existing contained cost evidence.
- Verification: Snapshot bytes/hash and summary verified; no viewer/profile/source fix yet.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Imported owner-authorized separate Trial run evidence without touching its app/preferences/source. Raw log stays private; next242after241.

- 2026-11-15 exacttrial/current source audit:1260 combines source/proof/certificate/state publication, native/CPU retention, supersession and allocation failures; it does not specifically mean State::publish failed. Historical CPU-layout rejections0 excludes ordinary rejection returns, not CPU allocation failure. Source-cache resource estimates plateau near3.89GB with2169entries, making native byte admission a candidate only: effective cap/reason unknown. No per-camera main/probe flavor-key churn found. Add typed existing-exit buckets without new producer walks/callbacks; no cap increase, negative cache or causal FPS claim. Separate scalar region-origin invalidation candidate needs its own producer regression before repair.

- 2026-11-15 69085 interior measured census endpoints add861unattributed group factory failures, alongside630unsupported returns; counters are not CPU times. New11stage/6miss/unsupported-mask and exact configured cap census integrated with existing fault injections; no cap/policy change. Probe source-build imbalance remains unattributed, not a camera invalidation proof.


- 2026-11-15 integrated69122 profile evidence: Exactbeb6837b71 quiet301.061s/club300.725s runs now expose publication failure stages. Club native_retain group-failure delta4003, other recorded stages0. This is optional retained-path failure/cost evidence, not ERROR/FATAL or dropped-draw proof. Both scenes churn the byte-capped native cache; no policy repair performed. [Report](research/integrated-build-69122-quiet-club-20261115.md).

- 2026-11-15 Architecture Session source follow-up: Ready source lookups already use touch; byte-denied publication later expires and creates a new payload/serial. No safe producer-revision-only rejection memo identified. PPD295 targets temporary weak-resource staging allocation before admission, with caps/policy unchanged; this does not claim to solve total cache churn.

## PPD-241 — Trial shadow capture failures lack a source-level attribution

- Status: investigating
- Confirmation: reported
- Owner: Codex shadow source capture diagnostics
- Discovered: 2026-11-15
- Original reference: Shadow source/preparation rejection must remain separate from GPU device/command failure and preserve existing transactional/fail-closed behavior.
- Evidence: [Direct cohort work](work/active/apple-first-direct-cohorts-20261114.md). Same verified69065trial census logs failed_views7main/1probe without shadow warnings or Metal command/device errors. Detached prep stays0 because max_family_inputs2201/1293 are below8192. A counter alone does not prove a visual regression or GPU fault, and no cause is identified.
- Next action: Inspect exact failed-view increment sites and existing source diagnostics; add bounded attribution only where absent. Do not lower dispatch thresholds without complete-path cost evidence or weaken shadow validation.
- Verification: Owner-authorized log observation, no reproduced rendering failure or source repair.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15: Retained as a distinct observation from hero-face terrain rejection125 and optional group publication240. No new viewer/profiler action.

- 2026-11-15 exacttrial/current audit: failed_views samples !spot_shadow_capture_complete at captureMetalShadowScene scope exit before final reconciliation/encoding/commit. Possible admission/source/palette/matrix/logical/pool-hook exits are not distinguished.7main/1probe therefore means CPU capture incompleteness, not GPU errors; final transaction failures are outside this counter. Detached threshold8192 was not reached. Add first-reason buckets before clearing; preserve first failure and count later reconciliation separately. No threshold change or attribution to125/240.

- 2026-11-15 69085 initial log reports4main/0probe CPU failed views; measured profile still needs typed attribution. Root adds first CPU stage/pass/map/line buckets and separate end-incomplete total, preserving first failure before later phases/clearing. Independent source peer review passes; begin failures before capture-active remain outside this counter as before. No native GPU failure attribution or threshold change.


- 2026-11-15 integrated69122 profile evidence: Main/probe detached_eligible/calls/completed and compact dispatch/serial counters remain0 in both actual69122 trace windows; cumulative maximum family inputs666/1158, below8192 threshold. Main failed_views4→4 and probe0→0. No new shadow failure is observed, but useful parallel-render preparation has not been demonstrated in these real SL workloads. Preserve prior small-family cost losses rather than lowering thresholds blindly. [Report](research/integrated-build-69122-quiet-club-20261115.md).

- 2026-11-15 zero-job source audit confirms production8192-per-family threshold, distinct from backend256; actual666/1158 maxima explain zero eligibility rather than shared-budget denial. World workers are separatelyOFF and excludecohort/animated inputs. Both detached mechanisms were unexercised; no broken callsite proved. Investigate bounded within-view small-family coalescing with complete copy/job/join costs, retaining old small-family losses.

- 2026-11-15 Before an aggregate-preparation prototype: register a contained current-scalar vs aggregate-serial vs aggregate-Dispatch comparison with mixed small families at the observed423–867 mean-view scale. It must include source/pose acquisition, snapshots, output allocation, ordered family barriers/admission, scratch and owner retirement; preserve opaque/query/masked/pool boundaries. This is not a threshold change or a claim of actual segment-size distribution. Existing per-family2048/256 losses remain.

- 2026-11-16 source75 `a5ea338f22d2ef3b419e6ff47c3cc7cb625695ce`, build69134: corrected fixture v2 checks876100/1 PASS; complete108-triple/8748-sample preparation rerun6388/1 PASS. Aggregate serial and Dispatch each win0/108 median comparisons against current raw scalar preparation. At867/1158/8192 logical draws Dispatch median changes+45.16%/+40.89%/+35.27%. No production threshold/queue change selected. Encoding/GPU are excluded; matching compact certificates can skip downstream validation, so this cannot reject the existing large-family path. Source75 full34 scopes1130416 assertion-executions/349 case-executions PASS under API/GPU validation. Actual segment-size histogram and useful whole-engine preparation contribution remain unproved.

- 2026-11-16 bounded encoding-overlap source audit: `renderSpotShadow` consumes explicit native scene inputs, but an owned batch list alone is insufficient. `beginShadowViewBatch` reuses renderer command state and the next `beginFrameData` changes its frame owner; encoding mutates arenas/maps/caches/stats. Body palette owners are separate from source leases. Target ownership, TLS submission scope and owner-side settlement also require isolation. The shared admission budget itself is mutex-protected. First restrictable future boundary is one non-query view with a private encoder/arena/cache context and complete packet, joined before owner commit and dependent lighting. Current APIs offer no isolated context. No worker encoding was added, and no concurrent correctness or benefit is claimed. Query and borrowed probe/hero transactions retain existing ordering.

- 2026-11-16 continuation begins implementing the missing private native shadow encoder-state boundary in an exclusive worktree. This is follow-through on useful owned CPU work under241, not a reproduced race in current serial code. New context must own mutable command/admission/frame-arena/material/depth state and reuse immutable executables; no second full-world PSO build, shared-Impl swapping, or capture-wide mutex. Root alone owns pipeline integration and contained execution. Caller-side commit/cancel/settlement stays authoritative, and production activation requires actual correctness and full-path cost evidence. The verified69134 candidate remains untouched.

- 2026-11-16 original mapping rechecked at `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45:indra/newview/pipeline.cpp`: renderShadow12811 sorts then draws with that view's matrices; projector14471–14485 clears/draws/flushes before marking its cache key drawn. The native packet/context is a new CPU ownership mechanism, not an original graphics backend. Preserve draw/query order, exact matrices, all-far clears, successful submission before validity/cache publication, and completion-based native retirement. Current serial code is not accused of a race.

- 2026-11-16 root captured-view packet now moves batch storage and separate body-palette owners without per-draw copies. CPU arrays can recycle after encoding; a separate shared producer/body owner survives native completion. Query/native-visibility/bounds inputs decline without consumption. Actual optimized packet test object linked against frozen75 native objects passes59assertions/3cases; source/library/object hashes and commands saved in private `shadow-captured-view-20261116T002516Z/receipt.json`. This is CPU ownership proof, not a full new product build or GPU-overlap validation. Header syntax and diff checks pass. Root adds owner-thread-only encode classification/time census (independent/borrowed × non-query/query) before selecting worker coverage; no renderer activation or threshold change.

- 2026-11-16 frozen author context `a14d07eba067fed770a5038c0db9b03eff0517dc` copied exactly (world.h2d08fbdb0290…, world.cppce369bc74dbc…, native test e22017b11eb7…). It builds a shadow-only peer by sharing immutable shadow PSO/depth/library owners; mutable arena/material/indirect/depth/command state stays private. It does not call the full world factory, swap shared Impl, dispatch workers, or issue its own main shadow-frame admission ticket. Explicit outer submission owner and nonblocking local one-command bound are required. Root adds the four native context cases to CMake; these are unexecuted pending frozen76 build. CPU packet59/3 remains separate evidence. Parent reload/anisotropy snapshots, prefix cancellation and simultaneous native context encoding are specified by actual API tests, not yet observed. Production queue/admission/settlement integration and profitable useful overlap remain open.

- 2026-11-16 first post-integration documentation harness check rejected CURRENT161lines and active ledger181lines, one over each limit. Preserved all content while joining one existing wrapped line per file; rerun required before frozen76. No renderer/build/test failure implied.

- 2026-11-16 frozen76 source3d6e6004d0ddff4a06286f41016e6920072c05be, app69137: doctor/configure/build0 (build173.145s), native context348/4 and existing extended-shadow/material/reload/admission18390/29 PASS with API/GPU validation. Full34 renderer scopes1130475assertion-executions/352case-executions, native bundle/strict3595files0blockers0errors PASS. Integrator3ed2540223 has the identical tracked tree1afcd4254376ad054a7d3ed2f2998f7e0d34876f; receipt source-tree-identity-76.json records this distinction. No runtime worker or extra global admission is implied.

- 2026-11-16 independent review gap closed with one additional real cutout/sampler test: source80fc60b5ebd9… passes44assertions/1case via optimized test object linked against frozen76 native objects and exact76 metallib, API/GPU validation enabled. Both old/new contexts match direct scalar alpha-discard pixels after parent reload/anisotropy4/destruction and material-buffer first use. Private shadow-context-covered-20261116T005033Z/receipt.json preserves all input hashes/commands. Formal future combined build includes this added case; this evidence is not a new full app build or FPS claim. Production outer-admission and owner settlement remain open.

- 2026-11-16 next G3 implementation assigned to exclusive /private/tmp/alchemy-apple-first-shadow-view-overlap fromdfb1f5dad3: actual one-packet capture/encoding overlap, including native query arrays/results with owner-only ALQueryTicket settlement. Preserve original shared2-group×6-view admission and command/source-lease completion identity; do not create a duplicate old command or constrain GPU overlap to the preliminary context local1 limit. Projector key/publication acceptance and all resize/reload/swap/profile/destruction joins are required. Author owns pipeline/world/shadow/settings and dedicated tests; root owns CMake/docs/integration/all execution. No worker activation or speedup claim before real correctness and complete serial/overlap costs.

- 2026-11-16 author productionc70940fefb and native tests0f74f4a69a integrated as5328e5c9cb/5be781e3f7. One owned packet adopts the original admitted command into private encoding state; native query results, exact completion and projector publication settle on owner join. Shared2-group×6-view authority remains original. Lifecycle/readback/same-camera query drains are explicit. Dedicated four-case correctness and six-cell query/nonquery complete-cost coverage are frozen but unexecuted. CMake now links the task implementation and tests; runtime control stays nonpersistent/defaultOFF. Independent review and frozen79 build/checks remain required. Owner reports Testing Integrated Build is analysing the viewer; no duplicate launch/profile or overlapping cost execution.

- 2026-11-16 frozen79 source705788d139/app69148 passes doctor/configure/build, actual new task2424assertions/5cases, byte49/4, context392/5, sharedshadow18390/29 and full34renderer scopes1130524assertion-executions/356case-executions with API/GPU validation. Static native and strict3597files/0blockers/0errors PASS; actual task/pipeline/common objects verify O3/NDEBUG/ARM64/noLTO. Complete6cell/120row native cost6716/1 PASS but unconditional overlap is not selected:256draw owner-phase wall+17.9/+19.1%, settled+7.65/+7.0%, actual overlap misses7/8 and8/8.867draws gives weak owner-2.6% and settled-0.5/-2.3%;1158owner-6.8/-8.4% and settled-5.9% with actual overlap. These are local32×32 native costs, not viewer FPS. Register the small-job scheduling loss before repair. Add one shared cheap1024-native-batch eligibility boundary before context/snapshot, preserve ordinary native path below it, and measure the complete policy on32/256/867/1024/1158/4096 query/nonquery cases. Keep every79 sample and all adverse/cold results; no persistent worker framework or threshold claim without actual1024 evidence.

- 2026-11-16 policy80 source frozen for validation: one shared typed1024 native-batch gate is used by production before context/snapshot and the cost comparison after real scalar capture. Below-work views take the original native serial branch without packet/context/job; dedicated below_work/minimum logs distinguish that decision from budget/adoption/timing denials. Start API remains force-capable for correctness coverage. New frontier32/256/867/1024/1158/4096 query/nonquery retains240 raw rows,2warm/8pairs and actual0/3workers, source/buffer/arena/query/order/admission/retirement parity. Root removes redundant pure constexpr-comparison checks; meaningful actual native policy/cost assertions remain. Build80/correctness/cost remain unexecuted, selection still OFF.

- 2026-11-16 frozen80 source1f83c401af/build69149: doctor/configure/build0 (build31.827s), all new native/preparation/context/shared-shadow checks and full34 scopes1130524/356 PASS with API/GPU validation; static native and strict3597/0blockers/0errors PASS. The complete gated12cell/240row cost20476assertions/1case PASS; all6 eligible1024/1158/4096 query/nonquery cells improve owner-phase9.0–16.4% and settled6.5–12.8% medians with0/8 overlap misses. All6 smaller cells prove0workers/context and exact serial branch; every adverse timing remains reported. GPU-sum4096/nonquery+10.16% is retained although envelope/settled improve; no cause or whole-world gain claimed. Select bounded overlap for the OWNER launcher only, not unconditional dispatch; packaged setting remains defaultOFF/nonpersistent. New architecture-combined-69149 staging is VERIFIED8273 identical inventory nodes with strict native verification and every SHA256SUMS row OK. Exeb0de99a90277f19c12f9a93edc59f9b6d8ecd73a2d3e3f0e0b672cc8cceb6745. Original69122/69129/69134 apps and failed79/77 evidence preserved. Owner full-engine activation/quality/loading/query/projector/probe/real GPU performance remain open. No viewer launch/login/live profile/push.

- 2026-11-16 delivery-document harness initially rejects a stale relative implementation-status link copied from state/CURRENT into work/active/metal-port. Correct the active-ledger link to its local sibling and retain this failed metadata check; no runtime/build defect or app change. Rerun harness before the documentation-only checkpoint commit.

- 2026-11-16 exact69172/source350b803597 live session: cumulative main failed views10/probe0, all pool-stage at captureMetalSpotShadowTerrainBatch failure lambda line14554(pass/pool93,map3,directional1). Increments3startup/7post-busy, none inside either300s window. Specific operand remains unlogged; do not attribute measured clubFPS or PPD125 nullVB to this aggregate. Preserve first-reason guards and add precise helper operands before repair. Private analysis-warnings-review/summary/census-series retain timestamps.


- 2026-11-16 fresh69172 actual5min windows: mainfailedviews3already present at12:06:21startup,probe0; no increment in quiet/club. Same shadow-terrain compound lambda attribution remains operand-unknown. Opaque indirect0selected because no run reaches2048eligible draws; preparation/fallback failures0. No threshold lowering or GPUerror inference.

## PPD-242 — First terrain diagnostic can survive into a later hero face

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` terrain face draw behavior remains the rendering contract; this first-failure diagnostic is new. Source43 `6af308d165` does not contain the affected WIP diagnostic.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Reset or qualify first terrain diagnostic at actual hero-face begin; retain cumulative/recent counters and re-review source before integration. Validate current-face attribution with the actual capture boundary.
- Verification: Read-only peer source review finds resetCompleteness/newGBuffer reset insufficient when beginMetalHeroProbeFaceCapture starts a later face before a new GBuffer. No affected build/test/viewer execution.
- Owner verification: pending
- Push approval: not_granted

The isolated PPD125 diagnostics WIP stores a first terrain preflight snapshot.
A failed face followed by a later hero face that aborts before GBuffer reset can
append the earlier face's diagnostic to the new abort. This is a diagnostic
freshness defect, not evidence of stale rendered terrain or the owner incident.

### History

- 2026-11-15 peer review identifies this new WIP diagnostic lifetime mismatch before integration. Rendering failure/cancel/retry remains unchanged. Register before repair; source43 and owner69065 unaffected.
- 2026-11-15 author resets only the first diagnostic at actual beginMetalHeroProbeFaceCapture, retaining recent/cumulative counters. WIP pipeline.cpp SHA d1ad9070d150eb813005d7049f2912b7cd8bde42d240484fc7fbff3ce0bff167; focused peer re-review PASS on exact hash, no affected execution/integration yet.

- 2026-11-15 Actual hero-face begin first-only reset and ordered terrain actual-body tests integrated. Source peers pass; actual hero begin transaction is explicitly outside contained runtime coverage. No owner confirmation/closure.

## PPD-243 — Main/probe attachment preparation discards immutable palette identity

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, `LLVOAvatar::updateSkinInfoMatrixPalette`: attachment layout is SKIN_ORIGIN float4 plus12floats/joint,1–110joints; view matrices and skinning/material behavior remain unchanged. Body-avatar composed palettes are view-dependent and separate.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Preserve the existing immutable attachment record through actual world capture/adoption/validation/binding; use existing shared FrameData identity publication with exact layout/data qualification. Keep raw/body behavior. Run actual native parity/lifetime and complete preparation costs, including any batch-stride cost.
- Verification: Independent exact6af source audits agree: capture obtains/CPU-pins record but stores raw floats only; ViewInputs::adopt copies per view, world validation finite-scans per draw, GBuffer setVertexData hashes/compares per draw. Shadows already preserve record and use FrameData::publishPaletteRecord. No new implementation build/cost evidence or FPS attribution yet.
- Owner verification: pending
- Push approval: not_granted

This is confirmed repeated CPU preparation, not proof of an owner FPS regression.
Main, reflection-probe and shadow consumers can use the same immutable producer
palette and shared native frame arena. Losing its identity before adoption defeats
that existing sharing and must be repaired at the capture/resource owner boundary.
Unqualified raw palettes and view-dependent body composition keep their original
copy/finite-validation path; no shader, quality, pose or culling change is intended.

### History

- 2026-11-15 registered before implementation after independent source audits. Backend and actual-capture owners assigned disjoint files in isolated world-palette-publication tree; no viewer/profiling/push. Source69085 remains immutable and unaffected.

- 2026-11-15 Backend/capture bridge integrated including forward alpha/glow/impostor paths and manual Bump/LegacyGlow capture. Batch grows one borrowed record pointer; adoption pins real shared record, GPU commands retain native arena bytes. Static cohort/page/group families explicitly reject the attachment-only field. Correctness/lifetime and96paired complete-cost cases authored and independently source-reviewed, execution pending. No shader or flags changed; equal-byte distinct-identity tradeoff remains measured, not concealed.

- 2026-11-15 observed verification: Source50 identity/lifetime paths retain successful22source48 scopes with exact production diff proof. Attachment complete costs225024assertions pass; CPU84/96wins, all48large110jointswins (median15.65% lower). Overall GPU/completion results mixed; no FPS claim. Owner verification remains pending; push not granted.

## PPD-244 — Pending-palette fixture creates a renderer behind its own queue gate

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Native-only new test, no upstream equivalent; actual ALMetalDevice command queue and Renderer::create initialization define the ordering contract. Source69085 does not contain this WIP fixture.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Create the probe renderer and builtin library before PaletteQueueGate; retain genuinely pending old/new native commands and the same lifetime/pixel assertions. Re-review before executing.
- Verification: Independent root/peer source reviews confirm new almetal_attachment_palette_test.mm constructs probe after the same-device event-wait command. Renderer::create uses commitAndWait for fallback texture uploads; it cannot reach gate.release. No hang was executed.
- Owner verification: pending
- Push approval: not_granted

The new lifetime fixture's CPU would wait for initialization uploads queued behind
its own unsignalled GPU event. The gate is released only after create returns.
This is a source-confirmed fixture deadlock, not a production viewer regression.
ALMetalDevice::makeCommandBuffer uses one device command_queue; ordinary render
submissions are nonwaiting, but Renderer::create has synchronous fallback uploads.

### History

- 2026-11-15 registered before repair/execution after root and independent peer identify the same ordering cycle in the frozen WIP test. No viewer, native test or affected build was run; source69085 remains unaffected.

- 2026-11-15 Author moves probe renderer/library setup before queue gate; same pending old/new palette, CPU expiration, pixel and abandonment assertions retained. Independent focused peer passes exact attachment-test SHAff5d3fae5e820592a0fffbc139b1a70b8f6175a4103806a2e8b4f8c508da81cf. No affected hang/execution occurred; native validation pending.

## PPD-245 — Extracted terrain guard changes the generic owner-line attribution

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Pre-extraction native capture.markIncomplete records pipeline.cpp owner lines; new actual-body include is a testing/diagnostic mechanism, not an upstream rendering change.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Capture the owner call-site line before the include and pass that numeric line into the identical failed-capture action. Add an actual-body attribution assertion; retain typed predicate/inl-file diagnostics.
- Verification: Peer source review confirms __LINE__ inside the new .inl resolves to the included file, even though expanding its text reconstructs the old source bytes. Root already labels the predicate implementation filename, but generic owner line should still identify its pipeline entry boundary. No affected runtime/build/test yet.
- Owner verification: pending
- Push approval: not_granted

Moving a guard into an include preserves C++ control flow but changes preprocessor
source coordinates. The generic first_incomplete_source_line previously identified
the pipeline owner; source-only reconstruction is insufficient proof for this field.
No geometry, terrain validation or GPU behavior is implicated.

### History

- 2026-11-15 peer identifies the __LINE__ attribution edge after the source extraction review. Root records it before changing line propagation. Source69085 is unaffected; no live capture was requested or run for this finding.

- 2026-11-15 Root captures pipeline call-site line before the local include and uses it for markIncomplete. Actual-body fixture now records/asserts that owner coordinate; predicate formatter also names its implementation file. No rendering/control-flow change; execution pending.

## PPD-246 — Attachment cost arms instantiate concurrent singleton font owners

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Existing llfontmetal_fixture.h NativeFont constructor requires gFontManagerp to be null and initializes the process-global font owner; this is a new cost fixture, not upstream viewer behavior.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Use one native fixture owner per configuration with honest per-arm producer refresh and separate renderer/FrameData accounting, or another source-proved nonnested owner design. Preserve rotating arms, producer sealing cost, equal-byte adverse cases and geometry-publication assertions.
- Verification: New cost Arm contains unique_ptr<AttachmentPalettePublication>, and setup constructs both before either dies. AttachmentPalettePublication inherits MaterialGroupPublication/Publication/NativeFontUI/NativeFont. The second NativeFont constructor must fail REQUIRE_FALSE(gFontManagerp). No affected test/build was executed.
- Owner verification: pending
- Push approval: not_granted

The benchmark's apparent independent per-arm state nests a process-singleton
fixture that cannot be instantiated twice. Native buffers may share the physical
Metal device, but that does not remove the font-manager ownership precondition.
This is a source-confirmed benchmark construction error, not a viewer defect.

### History

- 2026-11-15 root source review identifies the two simultaneous fixture owners before execution. Register before repair; keep baseline/candidate producer and arena costs symmetric when restructuring the harness. No successful benchmark or runtime failure is claimed.

- 2026-11-15 Cost fixture now uses one NativeFont host per configuration and separate per-arm FrameData/renderers. Both timed arms explicitly invalidate/reseal every producer record; stable bytes and churn packing scope remain symmetric. Equal-byte adverse cases, rotating arms, warm source-publication0 and producer-count checks preserved. Appendix SHA c473ec346c8fa6c477af7893327e80fabfd6020b141314860806f390e56b70a1 independently peer-reviewed; no cost execution yet.


## PPD-247 — Attachment fixture requests an unsupported tangent strider type

- Status: investigating
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Current LLVertexBuffer::getTangentStrider has LLVector3 and LLVector4a overloads; the original tangent layout is preserved. This is a new fixture API error, not a viewer rendering regression.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Use the existing LLVector4a tangent strider and rebuild; retain failed44receipt before native/cost validation.
- Verification: Source44 38732f4b25 doctor/configure pass, build fails after61.548s at new attachment appendix line30: LLStrider<LLVector4> cannot bind either tangent overload. No native tests or affected viewer execution.
- Owner verification: pending
- Push approval: not_granted

The new PBR fixture chose the color/weight vector type for a tangent accessor.
The renderer implementation is not implicated by this compile failure.

### History

- 2026-11-15 required44build fails exit1; exact commands/logs remain private in frozen-source-build-44.json and its build log. Registered before changing the fixture. Source69085 staged app remains intact.


## PPD-248 — Rigged PBR glow fixture omits its required emissive vertex stream

- Status: investigating
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Existing PBR glow producer requires TYPE_EMISSIVE and uses its offset; existing actual post-water PBR-glow fixture publishes that stream. No original shader or validation rule changes.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Allocate/populate/publish the emissive stream in the new PBR attachment fixture. Rebuild and rerun actual cases and the remaining native scopes; preserve the fail-closed production guard.
- Verification: Source45 build/static/strict pass; first14native scopes pass. Actual publisher67cases has66pass/1fail,17756/17757assertions. Family2 forward/glow test fails capture.complete because its buffer only has weight/tangent, while pbr_glow requires TYPE_EMISSIVE. Costs did not execute.
- Owner verification: pending
- Push approval: not_granted

The failure is a malformed test producer: requesting PBR glow without its authored
emissive attribute. The real source guard correctly rejects the incomplete draw.
No viewer crash or production rendering repair is attributed to this test failure.

### History

- 2026-11-15 native-correctness-45 actual publisher exits42; receipt/log retained. Register before correcting the fixture. Neither native tail nor costs ran after failure; no all-pass source45 claim.


## PPD-249 — Probe pass fixture counts borrowed wrappers as pooled acquisitions

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: ALMetalRenderTargetPool::acquire increments acquisitions only for reusable owned targets; borrowed-attachment descriptors are not reusable. Native pass timing windows count actual render passes separately.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Correct the new probe fixture's pool expectation to one owned acquisition for tile and two for separate passes; keep physical pass1versus2 assertion independent. Re-review before execution.
- Verification: New fusion test expects pool2versus3 including the borrowed framebuffer wrapper; source pool increments are inside reusableRenderTargetDescriptor, which excludes borrowed colors. No affected test/build execution or viewer behavior claim.
- Owner verification: pending
- Push approval: not_granted

The new assertion conflates CPU framebuffer wrappers, reusable target allocations
and physical render passes. Correcting that count must not weaken the independent
proof that eligible tile lighting replaces two render passes with one.

### History

- 2026-11-15 author reports the attribution error; root inspects actual pool acquisition code and records before repair. This WIP assertion is not in staged69085 or the source46 viewer under validation.


- 2026-11-15 observed verification: Source50 probe native7562/4 passes, including exact output/depth, one-vs-two physical passes and owned target acquisitions. Pool wrapper accounting agrees with implementation. Owner verification remains pending; push not granted.

## PPD-250 — Empty local/projector lists still submit accumulation render passes

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` deferred local/projector traversal skips empty light lists; current accumulateLocalLights and accumulateProjectorLights retain an empty color load/store owner.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Preserve full scene/GBuffer/target validation, then return successful no-work for zero accepted lights before allocations or command acquisition. Verify target bytes, invalid-input rejection, enclosing batch continuation and physical zero-pass behavior.
- Verification: Exact5c74 source audit identifies empty-list passes at both native accumulation owners. Existing conservative volume scissors are present; no fullscreen-overdraw or GPU-dominance claim follows from the CPU profile. No repair/test execution yet.
- Owner verification: pending
- Push approval: not_granted

A zero-light call has no color contribution but still creates a render encoder
and load/store pass. The optimization must retain error behavior for invalid
inputs and must not alter any nonempty light, projector cookie, shadow or filter.

### History

- 2026-11-15 registered before implementation after original/current source audit. This is avoided work, not proof of a measured FPS regression or dominant GPU shader cost.

- 2026-11-15 observed verification: Source50 empty native465/2 and complete-chain2948/1 pass: authored target preserved, invalid input rejected, no empty pass/submission; nonempty local/projector chain retained. Costs are same-source workloads, not a before/after timing claim. Owner verification remains pending; push not granted.

## PPD-251 — World attachment identities duplicate equal palette uploads

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: The preceding raw world path uses FrameData content interning; existing shadow identity publication intentionally skips that first-publication hash. PPD243 reused the shadow publisher for world inputs.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Retain O1 identity hits but content-intern a new world record once per producer, using the existing collision-safe byte interner and the exact returned native page. Preserve established shadow-only publication and pending-reader lifetime; rerun adverse equal-byte and ordinary complete costs.
- Verification: Source46 aa564/69089 all requested correctness passes.96paired attachment costs pass225024assertions. CPU improves85/96 configurations and48/48large-palette cases, but distinct equal records can inflate native published bytes55.02times. Example110joints/64draws/1view/no shadow/stable:74112raw versus4077888record bytes over12samples. CPU0.219083→0.174917ms and settle0.542855→0.516479ms still improve; native byte amplification is independently real.
- Owner verification: pending
- Push approval: not_granted

This is a measured publication inefficiency in the new world identity path,
not an owner-world FPS result or physical-memory allocation/leak claim. Distinct
producer identities with equal bytes should share a native slice while later
views/draws still use O1 identity lookup. Never assume that slice resides in the
last active page after content interning; retain the exact matched page.

### History

- 2026-11-15 adverse case was deliberately included before measurement. Source46 reveals the expected tradeoff; registered before repair. Preserve successful parity/lifetime evidence and raw costs, rather than declaring unconditional performance success.


- 2026-11-15 observed verification: Source50 full palette costs pass. Worst logical publication ratio record/raw is1.0 across96conditions. Original110joint/64draw/equal-record case falls from4077888bytes at46 to74112bytes at50, equal to raw74112;50paired CPU0.200458→0.169063ms and complete0.547208→0.514375ms. Native earlier-page alias and queued-lifetime tests passed48 and are explicitly reused. No physical-residency or viewer-FPS claim. Owner verification remains pending; push not granted.

## PPD-252 — Empty-light cost assertion omits the idle-barrier command

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: ALMetalDevice::waitUntilIdle creates and commits a real barrier command; new benchmark counters include it after settlement.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Count and label the one barrier in the cost frontier. Keep independent pre-wait zero-pass/no-work assertions and nonempty correctness checks.
- Verification: Source-only review finds expected views*2 read after waitUntilIdle; actual command debt is views*2+1. No affected test/build execution or production failure claim.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before fixture-only correction. The accounting change must preserve exact settlement inside the cost scope.

- 2026-11-15 observed verification: Source50 complete empty/nonempty cost suite2948/1 passes with one explicitly labeled settlement barrier included; no command debt excluded. Owner verification remains pending; push not granted.

## PPD-253 — Combined lighting fixture includes expose ambiguous helper names

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: C++ unqualified lookup after a using-directive; two new test appendices share the existing base-lighting translation unit. No upstream viewer equivalent.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Qualify EmptyLightPass::Fixture/Pixels in its three cases and remove broad using-directives before combining includes. Preserve all native assertions.
- Verification: Fusion includes the existing world-prepare fixture's anonymous Fixture/Pixels; empty-light tests import same-named nested types unqualified. Independent peer and author source review agree this is ambiguous only in combined translation unit. No affected compile/test executed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before the narrow combined-fixture correction. Production pass logic and staged69085/69089 are not implicated.


- 2026-11-15 observed verification: Required48/49/50 compile successfully; combined native probe and empty-light suites pass50. No broad using-directive workaround or production change. Owner verification remains pending; push not granted.

## PPD-254 — Old-library probe fixture passes unique ownership to a shared API

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: ALMetalShaderLibrary::loadFile returns unique_ptr; renderer factory takes shared_ptr<const ALMetalShaderLibrary>. Existing explicit-library callers transfer shared ownership.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Bind the newly loaded old library to an explicit shared const owner before renderer creation; rebuild without changing any rendering or fallback condition.
- Verification: Required47 atdfcfb911b0 doctor/configure pass; build exits1 after60.165s. New probe appendix217 passes an lvalue unique_ptr to the shared-library overload, so no matching create overload. No47native/cost execution occurred.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 compile failure retained in frozen-source-build-47 receipt/log. Registered before the ownership-type repair. Shader compilation and earlier compilation tasks are not whole-build success; source69085/69089 staged/evidence remain intact.


- 2026-11-15 observed verification: Explicit shared const library owner builds48/49/50; source50 preserved old-library native65/1 passes and selects the original separate passes. Owner verification remains pending; push not granted.

## PPD-255 — Probe tile light-map draw binds a nil depth/stencil state

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Original separate light-map pass has no depth attachment. The new same-pass draw shares the base-light depth attachment and must bind an explicit always-pass/read-only state, preserving all pixels and stored depth/stencil.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Bind the existing immutable always-pass/read-only depth state instead of nil, then rerun Metal API/GPU validation and exact tile parity before cost testing. Keep validation enabled.
- Verification: Required48 build/static/strict pass; first22native scopes pass including actual17897/70 and palette/preparation2492/16. New probe scope aborts with Metal API assertion depthStencilState must not be nil before first tile draw. No viewer launch/crash; later probe/empty/lighting checks and costs did not run.
- Owner verification: pending
- Push approval: not_granted

The shared render pass makes the first draw's depth behavior explicit. Disabling
validation or removing depth preservation would hide the error; the correct state
is always-compare, no depth writes and no stencil writes.

### History

- 2026-11-15 native-correctness-48 probe scope exitsSIGABRT(-6) under API/GPU validation; exact private log retained. Registered before repair. Earlier successful scopes remain valid evidence, not an all48pass claim.


- 2026-11-15 observed verification: Source49 explicit always-pass/no-write state eliminates API abort; source50 probe native7562/4 passes under Metal API/GPU validation, exact sky/geometry/depth parity retained. Owner verification remains pending; push not granted.

## PPD-256 — Retained static restoration stores an unused multipurpose batch

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Native group restoration is new; original LLDrawInfo/pass ordering and current typed-generation/material contracts remain authoritative. PPD203 already made batch slot storage lazy and shared when frozen.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md), [Implementation checklist](work/active/apple-first-architecture-status.md).
- Next action: Replace only owner-side idle static restoration metadata with a compact owner-free tuple; SIMPLE can reuse its existing immutable proof. Preserve renderer cohort descriptions, weak native-generation proof, exact active fields, warm OGroup transport, caps and failure/retry. Measure actual representation/copy costs, not just a cheaper estimate.
- Verification: Source48 audit finds fullBatch copies in Layout.Source, childWorldVariant and privateCohortSource. Layout restoration unnecessarily keeps224Bview matrices,256Bterrain hex arrays and avatar/impostor fields. Frozen Legacy384B/PBR1600B tables are shared, not repeated deep arrays; metadata estimate also conservatively charges a shared certificate twice. Rough9KBcharged perrow is not unique physical allocation or a demonstrated churn cause.
- Owner verification: pending
- Push approval: not_granted

The intended change removes real unused owner metadata, not a render backend or
warm-view adapter. Expand a material restoration tuple only on dirty republication
when the completed child/proof expired. Keep the private cohort's full immutable
renderer batch and all current source-generation checks. Do not raise cache caps
or claim a runtime speedup from correcting conservative accounting alone.

### History

- 2026-11-15 source audit registered before implementation. Existing lazy/shared slot work is retained; probe churn/performance attribution remains unproved until runtime reasons and complete costs support it.


- 2026-11-15 follow-up source audit: when private cohort proof expires but weak MaterialSource survives, expand creates authored slots before publish clones sealed slots again because snapshot identities differ. Candidate reuse of already-live sealed CPU snapshots requires existing exact producer/generation checks; no implementation or new retained GPU owner authorized yet, significance unproved.

## PPD-257 — Probe sky fixture omits the actual HDRI G-buffer marker

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Current HDRI G-buffer shader writes normal={0,0,0,1}; unchanged base-light sky transfer requires that family marker. The synthetic clear must reproduce it.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Set the synthetic normal attachment to the actual HDRI marker; retain exact tile/separate parity, both nonzero sky assertions and API/GPU validation. Rebuild and rerun the four targeted native scopes before costs.
- Verification: Required49 da8c24dbca/69092 doctor/configure/build0; static/strict pass. Probe scope3/4cases and7560/7562assertions pass. Only nonzero sky red/blue fail; tile/separate equality passes because both correctly reject the fixture normal alpha0. Source review confirms actual HDRI alpha1 and unchanged shader marker condition. Later native/cost checks did not run.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before fixture correction. Source48 API abort repaired by source49; source49 failure is distinct fixture input. Retain both failed receipts; no product sky defect or successful cost claim.


- 2026-11-15 observed verification: Source50 synthetic HDRI marker correction passes probe native7562/4, including exact parity and both nonzero sky checks. Production source unchanged. Owner verification remains pending; push not granted.

## PPD-258 — Compact-restoration fixture indexes a retired layout

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator / compact-restoration author
- Discovered: 2026-11-15
- Original reference: CpuLayoutResidency clears the superseded layout source vector when a successful reseal replaces it. Independently retained immutable source rows remain valid.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: In the new allocation-failure fixture, use its retained immutable row for the later expired-generation check, preserving all production and failure-retry behavior.
- Verification: Author source self-review finds old layout.sources[0] read after successful replacement; no affected build/test has executed. This is fixture authoring, not an observed viewer fault.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before narrow fixture repair. PPD256 implementation and complete-path costs remain pending; no performance or test pass claim.


- 2026-11-15 observed verification: Source51 actual publisher18617assertions/73cases passes under API/GPU validation. Fixture correction retains failure/retry, weak-generation rejection and production lifetime. Owner verification pending; no push approval.

## PPD-259 — Private probe cost driver selects the CPU publisher test binary

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Native probe and empty-light appendices are registered in almetal_tests; llrender_tests contains the separate producer/palette cost fixtures.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Point only private probe/empty-light cost drivers at the registered native app. Preserve source50 build, failed receipt and binary-hash guard; run under new retry labels without rebuilding production.
- Verification: Source50 required/static/strict and allfourtargetednative scopes pass. First probe cost command selects llrender_tests and exits2 without matching tests after0.025s; no cost samples ran. Subsequent costs did not execute.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before private driver correction. Production code/native correctness unaffected. Failed actual-probe-lighting-fusion-costs-50 receipt remains immutable; no successful cost claim.


- 2026-11-15 observed verification: Private drivers corrected to registered almetal_tests; source50 retry1 probe2448/1 and empty2948/1 costs pass. Failed no-test receipt retained; no production rebuild for driver-only repair. Owner verification remains pending; push not granted.

## PPD-260 — Compact-restoration fixture assumes completion handlers retired synchronously

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator / compact-restoration author
- Discovered: 2026-11-15
- Original reference: Metal Completed status permits subsequent completed-handler execution. Existing native source-owner fixtures wait for weak lease retirement separately.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: In the new fixture, wait with a bounded deadline for actual native lease retirement before asserting weak geometry expiry and stale-generation rejection. Preserve the final assertions and production lifetime behavior.
- Verification: Independent source review finds render/readback followed by eviction and immediate weak expiry assertion without a retirement token. No affected test has executed; transient failure is a source-confirmed test race, not an observed product fault.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before fixture retirement gate. Keep completion ownership strong until the real callback retires; do not shorten production lifetimes to satisfy this test.


- 2026-11-15 observed verification: Source51 actual publisher18617assertions/73cases passes under API/GPU validation. Fixture correction retains failure/retry, weak-generation rejection and production lifetime. Owner verification pending; no push approval.

## PPD-261 — Terrain patch capture republishes pass-common material constants

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator / terrain publication author
- Discovered: 2026-11-15
- Original reference: Native terrain pass capture resolves common texture/material inputs before its face loop. Original per-patch geometry, transforms, tessellation, paint/hex/overlay and query behavior remain authoritative.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Seal exact terrain surface inputs once per pass and share them across patch batches; publish common exact-view constants once while retaining independent patch geometry/factors and all validation. Gate with actual capture, native parity/generation lifetime and complete-path costs.
- Verification: Source50 audit identifies captureMetalWorldPBRTerrainBatch repeating four PBR slot values per patch; buildWorldTerrainConstants repacks896bytes of matrices/common/hex/PBR data and setVertexAndFragmentData uses default deduplicate=false. Patch offsets and six tessellation factors are separate. No measured viewer bottleneck or speedup attribution is established.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before implementation. This targets repeated pass-common allocation/publication, not removal of per-patch view-dependent work or a shader-quality change. Source49/50 probe repair and source51 compact restoration remain separately evidenced.


- 2026-11-15 observed verification: Source54/55full native and terrain5157/6 pass. Expanded55costs support sharing>=16; allfourcallers compiled/source-checked57. Actual helper44/2 plusnative5157/6 pass57. CompleteLLpool/runtime acceptance remains pending. Owner verification pending; no push approval.

## PPD-262 — Compact-restoration cost matrix omits expired-proof expansion

- Status: investigating
- Confirmation: reproduced
- Owner: Codex integrator / compact-restoration author
- Discovered: 2026-11-15
- Original reference: Cold new layout construction and retained warm/churn capture differ from restoring a retained tuple after child/private proof retirement. Correctness exercises expansion; the cost frontier must do so as well.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Add a focused complete-path cost condition using actual child/group eviction and real retirement, then capture/adopt/encode/reclaim/settle through actual admission against equivalent raw input. Report expansion/reuse counts explicitly and preserve all earlier cost data.
- Verification: Source51 required/static/strict/actual73cases and small-cost73578assertions pass. All1512small-cost raw samples total604tuplebuilds but zero tuple expansions and zero live-proof restoration. These results characterize construction/warm/churn only; they do not accept expired-proof repacking cost.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered after observed census and before cost-frontier correction. Native correctness demonstrates expired-proof numeric-buffer reuse; that does not supply a timing result. No viewer defect or speedup attribution is claimed.


## PPD-263 — World-preparation cost baseline omits owned-view adoption

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator / owned-preparation cost author
- Discovered: 2026-11-15
- Original reference: Current scalar production consumes ALMetalWorldViewInputs through renderOwnedInputs, while optional preparation consumes the same adopted packet. The earlier hidden cost matrix compares raw render(scene) with adoption plus preparation.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Add a paired complete owned-input cost comparison for scalar, serial preparation and credited two-range preparation. Include adoption, actual shared-frame admission, join, encoding, CPU reclaim and native completion in every arm, with explicit typed-source frontier and worker CPU accounting. Preserve old literal measurements and keep production selection disabled pending evidence.
- Verification: Source51 pipeline16549 adopts before either renderOwnedInputs or renderPrepared; existing hidden world-prepare-cost direct arm calls render(frame_scene) without adoption, while f.prepare adds it. Old results accurately describe those literal paths but cannot alone determine a current owned-path crossover.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before new fair cost fixture. This is a measurement-frontier limitation, not a demonstrated worker speedup or a viewer rendering defect. No automatic world-worker activation is authorized by source inspection alone.


- 2026-11-15 observed verification: Fair owned31595/1 cost suite passes54/55 with realcredit denial and allGPU samplesready. No usefulworker crossover found; productionworker control staysOFF. Owner verification pending; no push approval.

## PPD-264 — New matched terrain path relies on a replaceable batch resource owner

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator / terrain publication author
- Discovered: 2026-11-15
- Original reference: A matched immutable terrain publication proves exact texture generations; the encoded command must retain that publication's native-only resource lease independently of public mutable batch owner fields.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Matched terrain encoding retains terrain_pass.resourceLease independently; unmatched/raw retains existing owner behavior. Add pending-command coverage where the public owner is reset/replaced and CPU inputs retire.
- Verification: Author pre-freeze review finds new terrain path still retains only batch.terrain_texture_owner, which public callers can replace without changing matched static fields. No affected build/runtime execution; no viewer failure claim.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before narrow new-feature ownership repair. Preserve independent native completion retirement; do not retain the CPU pass record or viewer region to hide this gap.


- 2026-11-15 observed verification: Source55/57 terrain pending/cancel/CPUexpiry cases pass underMetal API/GPU validation, independent private selected-texture leases retained afterpublic ownerreset. Owner verification pending; no push approval.

## PPD-265 — Static source qualification omits the new terrain-pass owner field

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator / terrain publication author
- Discovered: 2026-11-15
- Original reference: Static cohort/page/group certificates exclude terrain and attachment owner fields so cached static records cannot retain unrelated resource graphs. The new shared terrain-pass field must obey the same contract.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Reject nonnull terrain_pass in static cohort/page/group qualification, independent of the terrain flag or replaceable terrain_texture_owner. Add an otherwise-qualified material certificate negative case and retain normal terrain behavior.
- Verification: Source261 adds shared_ptr terrain_pass to every batch. Existing static cohort/page/group gates reject terrain flags and old owner fields but not the new field. A public otherwise-static batch can carry this unrelated native-resource owner into retained static metadata. No affected execution or viewer regression observed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before narrow qualification repair. Primary terrain batches are already excluded by their terrain flag; the uncovered public-field combination remains a source-contract defect, not a measured memory or FPS attribution.


- 2026-11-15 observed verification: Source55fullcohort/pages/source/actual tests pass; native terrain otherwise-qualified owner-only rejection passes. Unused unrelatedowners stripped fromprivateprototype. No capchange. Owner verification pending; no push approval.

## PPD-266 — Actual terrain helper fixture confuses viewer facade with LLImageGL

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Existing Publication fixture exposes image as an LLViewerFetchedTexture facade and white as its real LLImageGL owner. The included terrain helper consumes LLImageGL pointers and native texture leases.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Use the fixture's actual white LLImageGL owner for native handles and helper input arrays; preserve every assertion and production source. Rebuild and execute all combined checks.
- Verification: Required53 00a90f4177 doctor/configure0, build1 after58.152s. Four errors in new actual terrain helper fixture: facade has no getMetalTexture and cannot convert to LLImageGL*. No native53 or cost53 execution occurred.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 failed build receipt/log retained before correction. Integrator used the wrong existing fixture member; no runtime viewer fault or successful combined build claim.


- 2026-11-15 observed verification: Corrected actual LLImageGL fixture compiles54/55/57; actual context44/2 passes57. Prior53failedbuild retained. Owner verification pending; no push approval.

## PPD-267 — Scalar cohorts repeat producer-invariant flag and common-constant preparation

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator / immutable scalar recipe author
- Discovered: 2026-11-15
- Original reference: Completed static cohort records already certify immutable fields. Scalar rendering rebuilds the400B common value and scans fixed material flags per draw even when adjacent source policies and the cohort view are identical.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Cache exact fixed flag/policy relationships at immutable publication; build common constants once per proven equal run and retain required slot rebinding after queries or special draws. Keep raw/pose-dependent paths and shader ABI unchanged; gate complete cold/warm/churn cost including metadata work.
- Verification: Source54 audit finds MaterialSource publication computes normal/specular flags once but scalar loop derives them again. bind_world_constants already reuses native GPU ranges on400B equality, so this defect concerns repeated CPU assembly/comparison, not repeated GPU uploads. No performance gain is established.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before scoped producer-derived preparation implementation. Charge actual metadata within existing caps and preserve bit-exact view/UV/flags. No worker or bank activation follows from this source observation alone.


- 2026-11-15 observed verification: Source55scalar recipe7835/3 passes underMetalAPI/GPU validation,136format/familytuples. Actualsingle265608/1,many1844512/1 and oldtags142080/2 pass; actualreusescounted. Warmbenefit supported, colddebt and cross-run/small-runlimits retained. Owner verification pending; no push approval.

## PPD-268 — Terrain sharing setup regresses single-patch passes

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Single-patch passes have no repeated terrain common work to remove. Raw native behavior remains valid; pass sharing must earn its creation/matching cost.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Extend the paired cost frontier to2/4/16patches, identify a supported selection boundary, and avoid sharing setup in passes below it. Preserve raw and shared output/lifetime paths.
- Verification: Source54 all27native scopes and all8cost suites pass. Single-patch CPU regresses4/4configurations2.55–4.09%; completion regresses3/4. All64/256patch CPU/GPU/completion8/8configurations improve. No whole-viewer FPS claim.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 measured before caller selection repair. Larger contained gains do not accept the one-patch regression. The new grid fixture permits partial valid grids for2patches; this changes no production geometry validation.


- 2026-11-15 observed verification: Expanded55matrix1/2/4/16/64/256 shows CPUlossbelow16 and CPU+completionall12winsat/above16. Source57fourcaller selection at16 builds/static/strict/focusedterrain pass; owner/runtime confirmation remains pending. Owner verification pending; no push approval.

## PPD-269 — New scalar-recipe cost geometry can occlude its required-positive query

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator / scalar recipe cost author
- Discovered: 2026-11-15
- Original reference: Query results are valid zero when authored geometry occludes them. A fixture asserting a positive result must place its query in a proven visible depth range.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Move only the new cost fixture's required-positive query volume in front of its authored triangles while retaining all query offsets, geometry coverage and exact parity assertions.
- Verification: Author pre-execution review finds new right-side triangles can cover inherited center(.35,-.5,.75); later queries can correctly returnzero despite CHECK(samples>0). No affected test or production execution occurred.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before fixture-only depth placement correction. Existing source54 evidence is unchanged; do not alter renderer query behavior or remove the positive primitive-coverage assertion.


- 2026-11-15 observed verification: Corrected frontquery costfixture passes55single265608/1,many1844512/1 with allgeometry/nonzero/query/MRT/depthparity assertions retained. Owner verification pending; no push approval.

## PPD-270 — Terrain caller-selection integration assumed three call sites instead of four

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Both classic/PBR main and probe paths have four terrain pass contexts. A caller policy is effective only when each existing context passes the selected pointer.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Wire all four contexts to the measured selection helper and rebuild. Validate edits before writing files and stop dependent shell steps on any failed command.
- Verification: Integrator edit guard expected3contexts, actual4; Python aborted after writing only the standalone helper. Subsequent shell commands incorrectly continued, producing da11a16ead with unused helper and starting56build. No caller selection or runtime success was established; source55cost evidence remains valid.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 partial intermediate commit retained before correction. Root fixes all four actual call sites, uses fail-fast execution, and records56 as superseded rather than a completed selection candidate.

- 2026-11-15 observed verification: Allfouractualpool call sites now use selectedpointer atsourceac2dfaeb6b/69102. Required57doctor/configure/build/static/strict andactual44/2/native5157/6 pass. Intermediate56successfulbuild remains unused-helper-only/superseded. Owner verification pending; no push approval.


## PPD-271 — GPU visibility retains CPU binding/submission for culled source rows

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex integrator / published-bound visibility author
- Discovered: 2026-11-15
- Original reference: The existing native GPU culler writes zero indexed arguments, while the owner still visits and binds each draw. The original CPU spatial-group culler remains intact; finer source bounds are already producer-published experimentally.
- Evidence: [Direct-cohort continuation](work/active/apple-first-direct-cohorts-20261114.md).
- Next action: Implement an experimental conservative CPU cohort/source frustum consumer over immutable qualified bounds, before native binding/submission. Preserve logical query offsets, unknown visibility and exact generations; gate complete cold/warm/churn costs before activation.
- Verification: Existing visibility cost frontier explicitly reports gpu_culling_eliminates_cpu_bindings=0. Native argument submission retains per-source CPU work even for zero index_count. This explains a limitation of that mechanism, not a measured cause of source69085 FPS or a rendering correctness defect.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before the alternative native CPU visibility implementation. No shader change, geometry reduction, cap increase or default activation is authorized by source inspection alone. CPU/GPU numerical disagreement must fail visible.


## PPD-272 — New source-cache lifetime fixture fails Catch INFO expression parsing

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Catch INFO streams its argument; an unparenthesized conditional expression binds after stream insertion.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md); `artifacts/metal/apple-first-persistent-native-scene-20261114/frozen-source-build-58.json` and build log.
- Next action: Parenthesize only the fixture diagnostic conditional, rebuild, then execute the unchanged cache lifetime assertions.
- Verification: Source4870b28117 doctor/configure exit0; build exit1 after19.792s at new fixture line83. No source58 native execution or successful combined build claimed. Production renderer unchanged.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 failure registered before fixture correction. Preserve source58 receipts and exact error; no viewer runtime defect inferred.

- 2026-11-15 observed verification: source60 required/static-retry/strict and relevant existing native scopes pass; actual21713/77 includes cache retirement and bounds promotion, common5496/41 includes residency counters. Earlier failures retained. Owner runtime acceptance/push approval pending.

## PPD-273 — New CPU visibility fixtures misread partial-union contract and omit third-mode parity

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex integrator / CPU visibility fixture author
- Discovered: 2026-11-15
- Original reference: Qualified union covers known rows only; renderer checks all-qualified before coarse rejection. Three-arm cost comparisons must include both retained and CPU visibility outputs against raw.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md); author pre-execution source review in published-cpu-visibility worktree, no execution yet.
- Next action: Assert known union and two-of-three qualification while proving unknown row remains visible; compare raw against BOTH other arms in the cost parity footer.
- Verification: WIP fixture-only errors identified before integration/build. No production fault, run failure or performance result claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before narrow corrections; preserve MRT/depth/query parity and numerical conservatism coverage.


## PPD-274 — Native source cache telemetry cannot attribute capacity withdrawal

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator / residency diagnostics author
- Discovered: 2026-11-15
- Original reference: Saved69085 NativeWorkingSet bytes reached3712MiB while entries stayed below8192; CPU layout entry limit is distinct. Existing aggregate census cannot separate byte/entry/age eviction and native touch misses.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), saved69085 frozen log and exact6af308d165 source policy.
- Next action: Count existing native rejection/eviction and touch-miss branches and report configured limits. Preserve all policy, resource accounting and reentrant publication semantics.
- Verification: Numeric-only35sample census confirms byte ceiling reached; individual miss causes remain unknown. No physical GPU-memory or FPS attribution.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before diagnostics-only implementation; no cap increase or new dependency/resource walk.


- 2026-11-15 observed verification: source60 required/static-retry/strict and relevant existing native scopes pass; actual21713/77 includes cache retirement and bounds promotion, common5496/41 includes residency counters. Earlier failures retained. Owner runtime acceptance/push approval pending.


- 2026-11-15 integrated69122 profile evidence: Numeric runtime counters now attribute quiet/club retain byte-cap rejection deltas2559646/4331675 and capacity eviction deltas1090835/1535092 inside sampled actualtrace intervals, cap3892314112bytes. Working-set retain is visible in CPU samples(3.452s/4.900s inclusive). Diagnostics work; total frame-time causality and policy quality remain unproved. Counts are occurrences and ledger estimates, not unique objects or physical allocations. No cap increase or fix. [Report](research/integrated-build-69122-quiet-club-20261115.md).

- 2026-11-15 Runtime-backed follow-up keeps numeric3,892,314,112B ledger cap and all ownership contracts unchanged. PPD295 is a separate bookkeeping candidate to avoid heap scratch growth before denial; no cache-policy or physical memory repair is established. Native retain inclusive3.452/4.900CPU-s does not support a dominant-frame attribution.

- 2026-11-16 preserved69151 whole-log activation census again records byte-only retention pressure: quiet522s/busy423s telemetry spans have retain false+4,348,078/+13,234,480, capacity evictions+2,019,845/+3,582,721; entry/both cap masks and allocation/invalid/conflict/exception counters remain0. Shared ledger cap3,892,314,112B derives from clamp(device recommended working set/16,256MiB,4GiB);8192entries/max_age2 frame units. Duplicate main/probe gauges are one global scope. These are invocation/estimate counters, not physical RAM, unique assets or five-minute CPU/GPU costs. PPD295 scratch repair does not fix cache turnover; no policy/cap change or frame-time cause claimed.

- 2026-11-16 exact69172/source350b803597 runtime final source-cache census:16730384retains,8104885success/new,8625499false(all bytepressure; other rejection buckets0),5673095capacity/892699ageevictions. Configured estimatedbytecap3892314112,retained3888122790;17629optional group failures allnative_retention withrequired raw/direct pathpreserved. These are logicalresource estimates and workcounts, not physicalUMAallocation orcausalFPScost. Correlate actual CPU/GPU beforechangingcaps/admission; priorallocationmitigations are not totalchurnrepairs. Private session analysis-warnings-review/summary/census-series preserve exactsource/logUTC.


- 2026-11-16 valid fresh5min evidence: near-window byte rejects1,988,956quiet/3,970,763club; capacityevictions1,276,671/2,288,000; groupnative-retainfallback835/5731. BusyactualCPUcacheunion13.356s/retain7.681s overlaps engine passes. Work is real, but a larger cap is not justified; sourceaudit confirms revision guard already prevents repeated dependencywalk on cachewithdrawal. PPD316 isolates only redundant same-Signal subscriptions.

## PPD-275 — New cache-lifetime fixture asserts retirement before releasing frame upload owners

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Actual LLVB upload uses frame-fence retention independently of the source publication/native-only tuple. Existing ownership fixtures release frame fences at the reader-retirement boundary.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md); native-correctness-59-source-cache-lifetime.log.
- Next action: Settle initialization and release real frame-fence ownership at the fixture retirement boundary, retain every source-reuse and native-resource lifetime assertion, then retest.
- Verification: Source59 required/static/strict pass; new lifetime case2516/2520 assertions pass, four final resource-retirement checks fail, one per control. Same-frame reuse/build census matches all expected controls. Missing fixture frame-owner retirement is source-confirmed; production leak not established.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 failure registered before narrow fixture repair. Preserve failed59receipt/log; no successful cache-lifetime claim yet. Source review also confirms LLVB destruction queues retired wrappers until the next ordinary flush; fixture now calls flushBuffers after native-reader reset, preserving collection policy.


- 2026-11-15 observed verification: source60 required/static-retry/strict and relevant existing native scopes pass; actual21713/77 includes cache retirement and bounds promotion, common5496/41 includes residency counters. Earlier failures retained. Owner runtime acceptance/push approval pending.

## PPD-276 — Warm cohorts never acquire newly requested bounds capability

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator / published visibility author
- Discovered: 2026-11-15
- Original reference: current/currentMaterial and seal/sealMaterial accept an existing unbounded group before source capability promotion. Live enabling of GPU visibility already had this limitation; the CPU experiment exposes the same issue.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), bounded integration source review.
- Next action: Stamp completed bounds-attempted capability during cold cohort publication, check it in warm lookup/seal when requested, and promote once. Unknown bounds must not cause repeated compilation.
- Verification: Fresh-start enabled requests are wired correctly. Existing warm unbounded groups remain fail-visible indefinitely until unrelated mutation; no missing geometry or owner FPS attribution.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before capability promotion repair; avoid qualified-count heuristics that would rebuild legitimate unknown rows forever.


- 2026-11-15 observed verification: source60 required/static-retry/strict and relevant existing native scopes pass; actual21713/77 includes cache retirement and bounds promotion, common5496/41 includes residency counters. Earlier failures retained. Owner runtime acceptance/push approval pending.

## PPD-277 — Pressure texture reductions bypass frame-wide native admission

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator / frame admission author
- Discovered: 2026-11-15
- Original reference: updateImagesCreateTextures drains native reductions before fetched uploads; scaleDown calls willWrite then preserveMetalScaleDown allocates and commits copy/mipgen/resample without the upload path's budget receipt.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), actual LLViewerTextureList/LLImageGL call path.
- Next action: Add explicitly typed bounded native texture-maintenance admission before mutation/allocation, retain credit to exact command completion and preserve pending queue work when denied. Maintain reduction progress under memory pressure and existing discard targets.
- Verification: Existing queued pressure reductions are governed by owner time scan, but not shared native in-flight bytes/work credits. No measured club-FPS or crash attribution.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before implementation; root owns integration and all execution. No viewer launch/profile or quality reduction authorized by this finding.


- 2026-11-15 observed verification: required62/static/strict and native maintenance+oldgeneration/mip843/18 pass underAPI/GPU validation, allthree real maintenance cases included. Common5585/44 and pressure174/23 passed61. Earlier fixture failures retained. Owner runtime acceptance/push approval pending.

## PPD-278 — New maintenance budget fixtures precede their helper declaration

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex frame admission fixture author
- Discovered: 2026-11-15
- Original reference: New C++ fixture cases call the existing small() policy helper before its declaration.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md); author pre-execution review in native-maintenance-admission worktree.
- Next action: Move only the three new fixture cases below the helper definition, preserving policy and assertions.
- Verification: WIP source issue found before integration/build; no executed failure or production defect claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before narrow fixture-order correction.


- 2026-11-15 observed verification: required62/static/strict and native maintenance+oldgeneration/mip843/18 pass underAPI/GPU validation, allthree real maintenance cases included. Common5585/44 and pressure174/23 passed61. Earlier fixture failures retained. Owner runtime acceptance/push approval pending.

## PPD-279 — New static verifier searches mangled symbols with a demangled method name

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: Existing verifier reads nm -m output without demangling. New stats marker incorrectly contains C++ :: syntax.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), validation-60-static-0.log and retained static-native-60.json.
- Next action: Use the exact emitted SourceWorkingSet5stats mangled-name substring and rerun static verification with new receipt/log paths before native checks.
- Verification: Source60 doctor/configure/build pass69108; verifier stops at missing marker although nm confirms __ZNK24ALNativeSourceWorkingSet5statsEv. No binary repair or source rebuild needed. Native/cost60 not started yet.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 false-negative harness failure registered before correction; preserve original failed receipts and tested binary unchanged.


- 2026-11-15 observed verification: source60 required/static-retry/strict and relevant existing native scopes pass; actual21713/77 includes cache retirement and bounds promotion, common5496/41 includes residency counters. Earlier failures retained. Owner runtime acceptance/push approval pending.

## PPD-280 — New maintenance fixture undercounts successful publication mutation events

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native maintenance fixture author
- Discovered: 2026-11-15
- Original reference: Signal::willWrite and didPublish each advance mutationRevision; successful scaleDown advances mutation twice and publishedRevision once.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md); new native fixture pre-execution source review.
- Next action: Correct only success mutation delta to2, retaining published delta1 and exact denial/no-mutation assertions.
- Verification: New fixture currently expects mutation delta1; no build/execution or production defect claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before fixture-only assertion correction.


- 2026-11-15 observed verification: required62/static/strict and native maintenance+oldgeneration/mip843/18 pass underAPI/GPU validation, allthree real maintenance cases included. Common5585/44 and pressure174/23 passed61. Earlier fixture failures retained. Owner runtime acceptance/push approval pending.

## PPD-281 — New mixed CPU visibility fixture requires a positive query under reverse-Z

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator / CPU visibility fixture author
- Discovered: 2026-11-15
- Original reference: Required-positive visibility volumes must use the native query depth convention for each depth mode; exact raw/optimized result parity remains mandatory.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), native-correctness-60-published-cpu-visibility.log.
- Next action: Trace actual query vertex/depth convention and author a visible query for both forward/reverse-Z without dropping nonzero coverage, MRT/depth parity or original query offsets.
- Verification: Source60 required/static-retry/strict pass; prior28native scopes pass including actual21713/77. New CPU visibility4cases:3pass, mixed case fails only raw-reference required-positive query at family0/format4,786/787 assertions pass. CPU optimized arm not yet reached for that format. Production fault not established; new CPU cost not run.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 failed reference fixture registered before investigation/repair; preserve exact60receipt/log.


- 2026-11-15 observed verification: repaired CPU visibility suite1780assertions/4cases passes61 underAPI/GPU validation with positive reverse-Z query and all parity assertions retained. Actual cost3674320/1 also passes. Owner runtime acceptance pending.

## PPD-282 — System-avatar view palettes lose immutable identity after composition

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator / native body palette author
- Discovered: 2026-11-15
- Original reference: Existing LLViewerJointMesh world-pose publication preserves original ordered joint/pivot calculation. Camera-specific buildJointMatrixPalette remains required, but main/probe/forward and shadow consumers expose its180floats only as raw pointers.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), exact9182fbeb7e body capture/adoption/validation/publication audit.
- Next action: Publish a privately sealed view-specific body palette, pin its CPU owner through capture/adoption, and use exact typed identity for validation/native frame publication. Preserve original180float shader layout and all per-view composition.
- Verification: Main capture15316/shadow14477 allocate raw180float arrays; ViewInputs snapshots them, validation rescans them and frame publication hashes them. Attachment proof excludes avatar_skinning, so prior243 does not cover body. No cross-view matrix reuse or measured FPS attribution.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before body ownership bridge implementation. Gate complete composition/capture/adoption/encode/retirement cost and resource-byte amplification; no shader/animation/quality/scheduler change.


- 2026-11-15 observed verification: Source65/9c2b48c23d required doctor/configure/build passes (29.166s), static/strict passes; repaired palette/preparation3150 assertions/18 cases passes under API/GPU validation, including raw/record body shadow parity. Independent64 body cases and24-condition actual cost remain valid: only fixture/docs changed65. Timings mixed/near parity, native-byte ratio1.0; owner acceptance pending.


## PPD-283 — New maintenance fixture uses authored-mip overload for a non-mip image

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: LLImageGL::createMetalTexture(span<MetalMipLevel>) requires mUseMipMaps and a complete authored chain. Non-mip images use the existing raw-pixel overload.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), native-correctness-61-native-maintenance-and-generations.log.
- Next action: Use the raw-pixel overload only for the new fixture's non-mip branch, preserve authored-chain branch and all native maintenance assertions, then retest.
- Verification: Source61 required/static/strict pass69109; common5585/44, pressure174/23 pass. Combined old/new texture scopes646/649assertions,15/18cases pass. Allthree new cases stop at non-mip fixture setup before exercising maintenance. Production defect not established; no CPU visibility/cost61 execution yet.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 failure registered before fixture-only overload correction. Preserve failed61receipt/log, no maintenance pass claim.


- 2026-11-15 observed verification: required62/static/strict and native maintenance+oldgeneration/mip843/18 pass underAPI/GPU validation, allthree real maintenance cases included. Common5585/44 and pressure174/23 passed61. Earlier fixture failures retained. Owner runtime acceptance/push approval pending.

## PPD-284 — Published CPU visibility repeats eight corner transforms for wholly visible bounds

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator / conservative visibility author
- Discovered: 2026-11-15
- Original reference: Current classifier transforms eight AABB corners and repeats component/plane checks. Qualified box support intervals can establish the same conservative frustum result with fewer operations.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), actual-published-cpu-visibility-summary-61.json.
- Next action: Replace repeated corner work with conservative affine/support intervals and preserve finite/subnormal/overflow/eye and rounding safeguards; gate exact native parity and the same complete actual cost frontier.
- Verification: Source61 CPU parity1780/4 and full48condition cost3674320/1 pass. Against retained direct, warm50%outside improvesCPU8/8median12.55%,completion8/8median9.21%; all-visible CPU loses8/8median4.41%, churn all-visible loses8/8median5.97%. No default activation or FPS claim.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 measured before classifier optimization. Preserve all61receipts; do not hide all-visible/cold debt or compare only against raw.

- 2026-11-15 observed verification: Source64 independent CPU GPU parity1780/4 and interval oracle137/3 pass, plus48-condition actual cost3674320/1. Warm half-outside CPU/completion medians19.987%/11.133% lower versus retained direct; all-visible warm CPU1.739% higher and cold debt remains. Control staysOFF; owner acceptance pending.


## PPD-285 — New body-palette fixtures misstate scene inputs and comparison policy

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex body-palette fixture author
- Discovered: 2026-11-15
- Original reference: Adopted scene must advertise both authored batches; negative mixed-proof fixture must retain a real shared attachment record using its actual API.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), body-palette worktree pre-execution source review.
- Next action: Set batch_count2 for the mixed scene and retain a live shared attachment record, preserving all negative proof assertions.
- Verification: WIP fixture only; no integration/build/runtime failure claimed. Author found a count1 scene and nonexistent owner() call on a temporary attachment record.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before narrow fixture corrections.

- 2026-11-15 pre-execution peer also finds shadow raw/record parity used different depth_clamp values. Registered under the same new-fixture authoring record before correction: select identical clamp policy within each pair and cover both policies across the existing format tuples. Retain positive, output/depth/query and ownership assertions; coverage is four HDR/normalHDR tuples with correlated reverse-Z, not eight independent tuples.


- 2026-11-15 build63/5258c8bf85 doctor/configure0, build1 after55.336s: new native appendix unqualified shadow(body) conflicts with the macOS MacTypes shadow enum. Registered under this same new-fixture record before qualifying the helper call. No native63/static63/cost63 execution; failedreceipt/log retained.

- 2026-11-15 required64/7308c43db9 builds69113(140.989s), static/strict pass. New mixed body fixture fails raw shadow reference: avatar=true with texture_mask coverage is invalid; existing validator requires avatar_mask/avatar_blend. Registered before changing only fixture coverage to avatar_mask and evaluating error after the render call. Previous17cases in palette/preparation scope pass; no production body-shadow regression inferred.

- 2026-11-15 observed verification: Source65/9c2b48c23d required/static/strict and repaired palette/preparation3150/18 pass, with avatar_mask and post-call diagnostics. Source63 compile and64 reference setup failures retained. No production shadow rendering fault established; owner acceptance pending.


## PPD-286 — CPU interval stress fixture omits its inside controls from result counts

- Status: ready_for_owner
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-15
- Original reference: The stress fixture classifies explicit underflow/ordinary-inside controls after its boundary corpus, but does not add those results to the non-vacuity counters.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), native-correctness-62-published-cpu-visibility-and-intervals.log.
- Next action: Count the existing additional control classifications without removing any finite-grid, float/FMA/FTZ or nonzero coverage assertion; retest.
- Verification: Source62 required/static/strict pass69111. Native maintenance/generation/mip843/18 pass. CPU visibility+interval1916/1917assertions,6/7cases pass; only counts[inside]>0 fails. Independent175616grid and invalid-case oracle plus four GPU parity cases pass. No false cull or production defect established.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before one-counter fixture repair; preserve failed62receipt/log, no fulloracle pass claimed yet.


- 2026-11-15 observed verification: Source64 interval oracle137 assertions/3 cases passes after counting the existing controls; CPU GPU parity1780/4 also passes. Earlier62 missing-counter failure retained; no false-cull defect established. Owner acceptance pending.


## PPD-287 — Persistent native scene rows exclude rigged animated surfaces

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex integrator / animated GPU source authors
- Discovered: 2026-11-15
- Original reference: Original Goal4 explicitly extends persistent geometry/material tables and GPU visibility/compaction to rigged and masked surfaces. Current static source eligibility rejects skin/weight inputs; shadow rigged ICB preparation still rebuilds per-view draw/constants and visibility arrays.
- Evidence: [Original route](research/apple-first-implementation-route.md), [execution ledger](work/active/apple-first-direct-cohorts-20261114.md), completion inventory at5258c8bf85.
- Next action: Owner evaluation of the separately verified69122 combined candidate; preserve optional animated modes OFF after nearparity/regressing complete costs. Follow the [run card](work/active/apple-first-owner-evaluation-69122.md). Runtime performance, feature and stability acceptance remain pending.
- Verification: Discovery described a missing regular-rigged native consumer. It is now implemented via separate animated sources with late pose/view data and conservative visibility; source72 full34 renderer scopes and four cost frontiers pass correctness/accounting checks. GPU completion costs regress and direct costs remain nearparity, so optional modes are disabled. Combined69122 staging is verified; original whole-viewer acceptance is unproved. Historical discovery and failed attempts remain below.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before required G4 animated-consumption implementation. Do not equate enabling slower pilots or merely constructing Metal4 objects with this capability.


- 2026-11-15 pre-execution source review: new animated factory used static-compact Draw(batch,&canonicalView) validation, which rejects rigged palettes. Registered under this feature implementation record before changing to a cold local canonical Batch and ordinary raw Draw validation. No per-view copy or executed failure claimed.

- 2026-11-15 pre-execution backend review finds new animated-cache entry insertion precedes potentially allocating native-asset ledger nodes, risking partial accounting on bad_alloc. Registered before staging all missing asset nodes/cache node, rolling back uncommitted zero nodes and committing reference/byte counts without allocation. Preserve old warm cache and all caps on failure; no executed fault claimed.

- 2026-11-15 pre-execution review finds newly authored scalar animated fallback calls nonexistent AttachmentPaletteRecord::floatCount; verified API is size(). Registered before one-call correction; no build/runtime fault claimed.

- 2026-11-15 Pre-execution resource review finds animated execution declares root allocations but misses exact linear/sRGB child texture views referenced by its table. Add native-only resource context and existing declareResources for each unique table before execution, preserving caps/completion ownership; no CPU source graph retained.

- 2026-11-15 pre-freeze backend review finds animated bank publication validates the binding cap but does not commit its selected binding limit into the shared command state. Register before committing that limit on successful publication, retaining all caps. Also count requested animated references during explicit prepared fallback calls (GPU0/fallbackN); production bypasses preparation for this mode. No executed failure claimed.

- 2026-11-15 backend peer review finds prepareAnimatedSourcesForCommand invokes the shared admission callback without first pinning its owner locally. Reentrant callback replacement can destroy the executing closure before its later captured-state access. Register before copying the shared callback locally for invocation, matching the existing paths and retaining epoch/denial checks. Source-only finding, no runtime incident claimed.

- 2026-11-15 observed verification: Source67/17ce73c68c builds69116 (805.967s), static/strict and all affected correctness pass: common5867/46, animated native3434/4, actual publisher23280/80, shadow98414/14, source admission2460/10, maintenance843/18, pressure174/23. Actual48-condition animated cost77438/1 passes output/query assertions but regresses warm owner CPU about18–30% and settlement8–25% at group means; cold/churn is worse. Exact1680raw/96summary rows retained. Arena bytes are lower, not a total-memory or speedup proof. Keep animated controlOFF and investigate concrete warm overhead; do not declare G4 performance accepted.


- 2026-11-15 observed verification: Source68/83c8b70ad7 required/static/strict pass69117 (37.145s); five animated native cases3604assertions pass, including numeric caps and pending retirement. Complete48-condition cost87446/1 passes with new working-set census; warm group CPU14.60–26.07% and settlement9.34–22.92% worse. Native source metadata/ICB caches are stable and bounded, not repeatedly recompiling geometry on warm hits. Next bounded experiment separates retained full-count commands from optional GPU visibility (separate cache mode prevents reuse of GPU-culled args) and removes redundant owned-view vector copying. Existing direct per-draw PoseView vertex binding already exists; do not invent a root-lookup shader repair.


- 2026-11-15 pre-freeze287 callback review finds public animated preparation snapshots views but continues reading caller-borrowed32B SourceGlobals after admission. A callback may mutate aliased flags/mirror data after bank-layout selection. Register before copying globals by value at owned entry before callbacks. Renderer-local globals are private and no viewer incident/performance cause is attributed.

- 2026-11-15 source69/d2e8b6d9ca builds69119 (164.224s); static/strict, native5029/5, actual23778/80, common5867/46 and command/resource/old-library checks pass. Separate explicit48costs pass92487/1 full-count and87447/1 visibility; both parsed48pairs/1680raw rows. Full-count reduces overhead but still loses warm CPU4.61–15.46%, settlement2.67–16.33% atgroup means; kernel mode stillworse. KeepbothcontrolsOFF. Next source-confirmed overhead is all-bank resource declarations repeated perquery/raster range and identical finite/owned-reference rechecks; optimize once per exact active G-buffer encoder with command/Batch ownership and preserve standalone API safety/prefix failure. No FPS acceptance or cause attribution beyond this contained frontier.

- 2026-11-15 source70/142390cc67 builds69120 and all relevant correctness passes (native5193/5, actual23814/80). Costs95743/1 fullcount and90703/1 visibility pass. Fullcount64 warm owner CPU improves4.52%/1.53% (one/three views), but settlement12.77%/12.33% and GPU19.51%/16.34% stillworse. Scopedclosurecount proves1declaration for2ranges; no performance activation. Next source-directed consumer keeps the same immutable SourceReference/pose/native tuple but uses original direct rigged shaders and bindings, avoids scratchBatch copies and conditionally reuses proved-equal400B constants. Split CPU certificate from optional GPU descriptor/bounds capability; default existing GPU seal remains superset, immutable promotion/fallback guards retained. This preserves shared architecture while selecting a faster M1 consumer, not a claim the unmeasured route is faster.

- 2026-11-15 pre-build direct-facade review finds optional static SourceRows can see an animated reference pose-free prototype with valid weight_offset0 and no prototype palette, and bypass its late pose. Register before explicitly declining animatedReference candidates in that static-row optimization, retaining ordinary direct rendering. Normal LLVB weight offsets are nonzero; no runtime attribution or feature removal claimed.

- 2026-11-15 pre-freeze direct-consumer review finds private animated references already excluded from material-argument-buffer requests/binds but still counted by a trailing resource fallback branch when that diagnostic flag is ON. Register before matching the final fallback guard to the same animated-reference exclusion. Preserve ordinary direct bindings and dedicated direct census; source-only accounting issue, no runtime performance attribution.

- 2026-11-15 source71/7d58e89d88 builds69121 (165.315s), static/strict and all34renderer scopes252278 assertion-executions/346case-executions pass. Direct/fullcount/visibility48costs100120/1,102464/1,97424/1 andall1680-row parsers pass. Direct route removes GPU tax (same shaders), but warm resultsnearparity: smallCPU0.65–2.44% lower,64CPU3.33%/5.37%higher, coldCPU7–11%higher. Noactivation. Next bounded source-proof work: trust already-sealed material proof in direct renderer, avoid repeated private-pose publication lookups within encoding, avoid temporary shared-owner copies in warm certificate identity checks, and use existing two-outer-frame stability admission before optional direct compilation. Preserve all public/borrowed/native and289guards, required raw rendering andper-source/framecredits; no capraise or changed shaders.

- 2026-11-15 observed source72 verification: exact beb6837b71 /26.4.0.69122 doctor/configure/build0/0/0 (165.459s), static native and strict source PASS. Nine affected scopes PASS,74802 assertion-executions/203 case-executions; animated7925/5 and actual publisher26034/80. Four explicit48-condition costs and all1680-row parsers PASS: stable direct, direct without stability, full-count GPU commands and GPU visibility. Stable direct warm64 CPU+0.20%/+0.27% and arena0.772x/0.755x(one/threeviews); this is near parity, not a consistent speed gain or physical RAM result. Full-count GPU warm64 settlement+11.56%/+8.96%; GPU visibility worse. All three animated controls remain OFF in the recommended combined policy. Cold/churn/loading/denial samples retained; no viewer FPS claim. Full34source72 regression and separate immutable staging are the final engineering checks; owner acceptance remains pending. Prior failed attempts retained.

- 2026-11-15 final source72 engineering checks: full34 renderer scopes PASS,254120 assertion-executions/346case-executions (161.571s), no earlier-scope reuse. New separate architecture-combined-69122 provenanceVERIFIED;8273 app inventory nodes identical, staged static signatures/ARM64/import/features and evidence SHA256SUMS PASS. Exact sourcebeb6837b71 and executableSHA101a327c6885… retained. Selected animated modes OFF for measured nearparity/regressions, retained static groups/stability ON. Bounded source audit finds no additional mandatory implementation callsite. Engineering delivery is ready; original owner performance/feature/stability acceptance remains pending, record not closed. No launch/login/profile/push; prior apps preserved.

- 2026-11-15 post-delivery dependency handoff: three consecutive audits confirm69122 verified with owner runtime acceptance absent. No remaining independent mandatory implementation/staging item found; original goal blocked on owner combined-viewer results or subsequent explicit runtime authorization. Full scope retained; performance/feature/stability acceptance unproved, record remains investigating. No launch/login/profile/push or defect closure.

## PPD-288 — New animated-source fixtures use invalid API and coverage assumptions

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex animated-source fixture author
- Discovered: 2026-11-15
- Original reference: Actual LLVB exposes TEXCOORD2/EMISSIVE striders and Weight4 input uses LLVector4a, not the authored convenience setters/vector type.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), animated-gpu-records worktree pre-execution fixture review.
- Next action: Use the verified existing strider interfaces and LLVector4a weights; preserve geometry, index uniqueness and intended pass/material coverage.
- Verification: Test-only WIP source issue; no integration/build/execution or production fault claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before fixture API corrections. Check real declarations/examples before adding fixture API calls.

- 2026-11-15 pre-execution native fixture review: inherited all-zero UVs sample the checker texture transparent first texel, so masked parity can be vacuous. Registered before authoring UV coverage over the checker and asserting positive/partial masked reference coverage against opaque control. Preserve fixed-UV positive GPU contract; changed UV explicitly exercises scalar fallback.


- 2026-11-15 pre-execution review also finds guessed abandonCommandBatch name (actual API cancelCommandBatch) and a local NativeScope that keeps generation owners alive during intended final-retirement checks. Registered before correcting the API call and explicitly resetting temporary/fixture owners; pending survival and final expiry must prove their distinct ownership boundaries.

- 2026-11-15 pre-execution fixture review finds a capturing lambda assigned to the existing function-pointer promotion seam. Use a captureless lambda with function-local static counter; preserve hook/count semantics and scope cleanup. No execution or production fault claimed.

- 2026-11-15 pre-freeze fixture review corrects provisional requested-draw denominator: requested counts emitted animated references, excluding cold CPU admission denials that retain required raw draws. Track captured-reference count, retain uncaptured raw count and require GPU+fallback=requested; no executed failure or production change.

- 2026-11-15 Pre-execution actual-fixture peer finds two assumptions: promotion hook intentionally runs before exact-transient guard, so throwing hook count must be1 while resource/cert/admission work remains0; and PBR masked flavor must match all owned materials ALPHA_MODE_MASK rather than inherited OPAQUE. Register before correcting only these fixture inputs/assertions; production order/guards unchanged.

- 2026-11-15 pre-execution native fixture review finds replacing FrameData on an already-used renderer does not reset its cached source context, making the intended cold-denial callback vacuous. Register before creating a fresh library/renderer with the fresh FrameData before first source use. Preserve denial assertions; no production setter change or runtime incident inferred.

- 2026-11-15 integrated66 native animated suite1340/1341 assertions,3/4 cases passes under API/GPU validation. Mixed-format case fails only raw reference required-positive query at family1/opaque/format4 (reverse-Z): expected.queries.front()==0, before optimized comparison. Register before tracing query convention and fixture repair; production animated rendering failure not established. Preserve positive coverage/parity and prior failed receipt.

- 2026-11-15 observed verification: Repaired reverse-Z raw-query fixture passes allfour animated native cases67 (3434assertions); eight formats and positive query requirements retained. Earlier66 raw reference failure remains in evidence. Owner runtime acceptance pending.


- 2026-11-15 pre-build direct-source fixture peer finds a ternary mixing float* matrices with float(*)[4] normal matrices. Register before flattening only the normal/rigged-normal branches with &matrix[0][0], retaining all five changed-matrix common-proof reset assertions. Test authoring issue only; no build/runtime failure or renderer cause claimed.

## PPD-289 — Optional animated promotion can reject a required reader after cache withdrawal

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex animated producer author
- Discovered: 2026-11-15
- Original reference: An exact reader-pinned transient may remain usable after residency withdraws State publication without changing producer revision. Existing optional promotion checks require that publication to remain current.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), new animated-capability path source review before enabling it.
- Next action: Decline optional GPU promotion while retaining only the exact required reader when state identity/revision, actual draw-info state, empty publisher and weak transient identity/control block all still match. Preserve all existing stale/new-publisher rejection guards and hook ordering.
- Verification: New animated request on exact withdrawn metadata would otherwise return an empty required lease when promotion cannot publish. No viewer/runtime failure claimed; GPU candidate must decline and raw native rendering remain available with the same pose.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before narrow no-promotion fallback branch. No cache cap bypass, resource/seal work or producer invalidation relaxation permitted.


## PPD-290 — Fixed-order animated CPU admission can repeatedly serve a failed source first

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex native admission author
- Discovered: 2026-11-15
- Original reference: Existing group caller sorts service-adjusted source priority before admission. New per-source animated factory traverses fixed pass order; CommonAdmission currently passes original pending age to budget even after recent service.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), pre-execution animated admission integration review.
- Next action: Pass existing service-adjusted unserved_since into budget acquisition while retaining original pending age and once-frame semantics; verify unsorted repeated failures cannot starve older sources and existing sorted admission remains valid.
- Verification: Under busy one-item progress, continuously failing first source can remain aged forever and consume each opportunity before later unserved source. This is source-confirmed new caller/policy integration, not a measured viewer regression.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before common policy repair; reuse existing state/fairness, no new queue, cap or requester/resource retention.


- 2026-11-15 integrated66/a2fc865fe8 doctor/configure/build0/0/0 (177.166s), static native and strict source pass69115. Frame budget suite5751/5753 assertions,43/45 cases passes; existing vanished-phase recovery and active-byte-capacity hint cases fail recovered/fits assertions. The new unsorted fairness case passes. Register this failed repair attempt before tracing service-adjusted pending-age interaction with phase hints. Preserve both existing recovery assertions/caps; no GPU66 or cost66 result yet.

- 2026-11-15 source diagnosis of66 failures: the290 repair passes service-adjusted age as both fairness rank and maximum-deferral eligibility, restarting genuine waiting age after each grant. Repair must keep original pending age for eligibility and separate service-adjusted age only for ordering hints. Register this narrow API/header split before implementation; retain original recovery/capacity and new fixed-order fairness assertions, no quota/cap changes.

- 2026-11-15 independent66 actual publisher23045/23046 assertions,79/80 cases passes; both new animated actual-producer cases pass. Existing cold-cohort recovery at frame10 fails compile(owners[0]) (line2612), consistent with290 genuine-deferral-age reset. Add to repair retest scope; no new GPU output fault inferred.

- 2026-11-15 observed verification: Source67 common5867assertions/46cases and actual publisher23280/80 pass, restoring all failed old recovery/capacity/cohort cases while retaining new fixed-order fairness and split-age assertions. Admission/shadow/maintenance/pressure suites also pass. Source66 failed repair receipts retained; owner acceptance pending.


## PPD-291 — Subnormal packed weights can invalidate cold animated bone bounds

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex animated source/bounds author
- Discovered: 2026-11-15
- Original reference: Existing object skinning falls back to first influence when GPU weight sum flushes to zero. CPU cold scan can instead see a positive subnormal fraction and bound a different bone.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), pre-execution animated source/shader peer review.
- Next action: Mark bounds unqualified for any nonzero subnormal encoded weight; retain normal native drawing/unknown visibility. Cover CPU/GPU FTZ fallback and possible visible first influence in focused parity tests.
- Verification: Example packed weights7.0/subnormal/0/0 can make CPU select joint0 while GPU FTZ selects first joint7; different poses could otherwise falsely cull. New code unbuilt/unexecuted; no owner rendering incident claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before source-factory guard repair; no shader layout or render-weight semantics change.

- 2026-11-15 Separate pre-freeze GPU enclosure review: a constant32*FLT_MIN floor does not bound intermediate skin-coefficient underflow amplified by large positions. Register before scaling world/clip underflow floors by both operand magnitudes (source extents/linear coefficients and point/MVP), retaining conservative relative and uncertain-W/cancellation guards. Rendering vertex math and shader ABI remain unchanged.


## PPD-292 — Idle animated command banks outlive expired source proofs

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex animated native command author
- Discovered: 2026-11-15
- Original reference: Existing idle-bank pruning follows weak membership proofs. New animated banks use native source closures but leave that membership proof null, so they are not retired by the same expiry path.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), animated backend lifetime/census peer review before execution.
- Next action: Add weak exact CPU-source/native-tuple generation proof for idle lookup/pruning; remove expired animated cache banks while prepared/queued views keep their independent native leases. Do not reject already prepared work merely because CPU metadata expired.
- Verification: Shared index/binding/asset caps already bound storage; no unbounded leak or COW-pin claim. Source-page census may release its accounting while idle bank closure still holds native assets; each census must describe its owner scope correctly.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before weak-proof idle-prune repair. Preserve actual native completion lifetime independently of CPU cache freshness.

- 2026-11-15 proof review also requires mutable AnimatedGpuView native lease to match the private captured weak owner/control block (and finite view) before warm binding. Register before adding matchesNativeLease alongside idle proof; no strong CPU parent or change to already prepared lease validity.


## PPD-293 — Animated cost evidence omits retained cache footprints and numeric cap coverage

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator / animated validation authors
- Discovered: 2026-11-15
- Original reference: Original Goal4 requires working-set and lifetime checks against direct native execution; bounded retained bytes remain part of the unified-memory goal.
- Evidence: [Execution ledger](work/active/apple-first-direct-cohorts-20261114.md), animated cost console and bounded SourcePages/Commands counter audit at17ce73c68c.
- Next action: Log existing per-arm retained metadata/assets/binding/index/ICB scopes outside the timed frontier, sampled once per shared FrameData cache. Add a focused native animated cap check using small injectable limits; retain required native rendering on denial.
- Verification: Current cost output reports FrameData arena_logical_bytes only. Existing four native cases test pending CPU/native ownership, cancellation and admission retirement, but do not assert numeric animated retained-byte caps. Source-enforced bounds exist. Metadata/assets/bindings/index scopes overlap and cannot be summed as physical RAM or compared to a zero raw-cache counter as total memory amplification. No measured memory leak or cap violation claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 missing working-set evidence registered before validation-only logging/coverage additions. Keep the first67 timing attempt and all prior receipts; do not add runtime policy or increase caps for this harness gap.

- 2026-11-15 observed verification: Source68/83c8b70ad7 required/static/strict and new native five-case suite3604/5 passes; tiny metadata/asset/ICB caps, required raw fallback control and actual pending byte/credit retirement all execute positively. Actual48-condition cost87446/1 and parser48pairs/1680raw rows passes. At64sources warm: page metadata256.3KiB; gross binding801.9KiB(one view)/930.4KiB(three); ICB64/192KiB; index12KiB. These scopes overlap and are not total physical RAM. All explicit cap assertions pass; actual physical process-memory behavior/owner acceptance remains pending.


## PPD-294 — Teleport cleanup logs remaining old-region map objects

- Status: investigating
- Confirmation: reproduced
- Owner: Codex Testing Integrated Build / Architecture Session handoff
- Discovered: 2026-11-15
- Original reference: Original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 and candidatebeb6837b71 have byte-identical LLViewerObjectList::clearAllMapObjectsInRegion bodies. After logging the remaining-object count, the function calls markDead on each region object.
- Evidence: [Integrated69122 two-scene report](research/integrated-build-69122-quiet-club-20261115.md); private busy log-review.json and source-warning-review.json.
- Next action: Retain as an observed cleanup anomaly; if investigating retention, trace actual object and native-generation retirement after cleanup. Do not infer a leak, new Metal regression or bypass cleanup from warning counts alone. No further live capture authorized by this handoff.
- Verification: Owner teleport completed in the300.725s club trace. Two WARNING rows report1045 and1529 remaining objects. Counts are not proven unique or additive; the log does not establish permanent retention. No ERROR/FATAL rows or viewer crash during the two recorded windows. No code change or repair attempted.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered observed warning state with unproved cause before any corrective implementation. Owner closed viewer after both recordings. Original cleanup behavior is preserved; no fix or closure claimed.


## PPD-295 — Native residency admission allocates rejection scratch for every candidate

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex Architecture Session / native admission implementation
- Discovered: 2026-11-15
- Original reference: This native cache has no upstream API equivalent. Original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 llspatialpartition.h drawmap_elem_t/draw_map_t retain LLDrawInfo and native-independent geometry ownership. Preserve draw order, producer revisions, required readers and actual completion lifetime; cache withdrawal is not a producer mutation.
- Evidence: [Integrated69122 report](research/integrated-build-69122-quiet-club-20261115.md), alnativesourceworkingset.cpp retain candidate.resources construction before capacity rejection, PPD240/274 runtime counters.
- Next action: Measure allocation-free bounded local resource staging against the exact current retain implementation; integrate only if admission/rejection/eviction/reentrancy/size validation remain equivalent and contained CPU/allocation costs support it. Keep all caps and admission policy unchanged.
- Verification: Every new source currently grows a heap vector of weak resource identities before byte admission, including2.56M/4.33M observed byte-rejection occurrences. Those counts do not establish total allocations or causal viewer FPS cost. Ready source lookups already use touch, so serial-only negative caching would not address the observed new-payload churn. Producer revision alone cannot safely key denial across resource-variant changes or recovered capacity. No repair/result yet.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered source-confirmed transient allocation opportunity before implementation. Current-view/queued/GPU readers, typed native identities, resource-size conflict checks, current-frame subset, reentrant callbacks and all numeric caps remain mandatory. An inline staging implementation must spill for arbitrary larger unions and preserve existing retained-entry storage/lifetime. No cap raise, negative-cache policy or quality change.

- 2026-11-15 initial documentation harness rejected the new record until the next-ID marker advancedPPD295→296; corrected metadata before implementation. A first metadata replacement assertion used an incomplete marker format and stopped before writing. No renderer/test execution failure.

- 2026-11-15 measured alternatives: Inline16 staging passes601/16 and all48+16 cost cells but cold one-resource admission regresses13.29%/16.41%(without/with duplicate input), so rejected for integration. Simpler reserve at first validated unique append passes the same601/16 semantic checks and48+16 parity/cost cells; selected as a partial allocation-growth mitigation. Actual-source retain-boundary2/3-resource cold CPU−15.28%/−26.26%, byte denial−16.05%/−28.04% without duplicates;4/16/17 denial−26.31%/−25.16%/−31.08%, scratch allocations3/5/6→1. Warm touch nearparity; one-resource denial+3.39%/+3.16% controls retained. These are retain-only nanosecond costs, not full factory/frame/FPS.

- 2026-11-15 selected source867fb2fb8bd2… plus testccd300e5f16a… frozen and copied exactly; root source/semantic review PASS. Reserve can retain input-span capacity for duplicate-heavy callers; current actual source and typed-group unions deduplicate before admission, while API stress costs retain this allocation-byte tradeoff. Existing caps, eviction order, revisions, callbacks, resource estimates and completion lifetime unchanged. One rejection allocation remains, so this is not a full allocation-free or cache-turnover repair; record stays investigating. First temporary compile lacked explicit SDK and failed; corrected SDK build and selected correctness601/16 exit0 retained in runtime-retention-295-20261115T220845Z. Full integrated build/checks next; no new viewer runtime or acceptance claim.

- 2026-11-15 integrated3841527bbd /26.4.0.69129 doctor/configure/build0/0/0 (34.712s), static native and strict source PASS. Full34 current-source renderer scopes PASS254316 assertion-executions/348case-executions (160.976s), includingcommon6063/48 andactual publisher26034/80; no older-scope reuse. Separate architecture-combined-69129 provenanceVERIFIED,8273 identical inventory nodes, staged static verification and SHA256SUMS PASS. ExecutableSHAe958bb09075957bd84d921459af7802d734d69f75a353d5a764b5805370c99b9. This validates the selected bookkeeping mitigation only; per-denial allocation and total cache turnover remain, owner acceptance pending. Tested69122 preserved; no new launch/login/profile/push.


## PPD-296 — Draw-source content revisions rebuild unchanged native dependency subscriptions

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex Architecture Session / native dependency author
- Discovered: 2026-11-15
- Original reference: Native dependency relays have no original API equivalent. Original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 llspatialpartition.h/LLDrawInfo owns geometry/material references in the retained draw map. Preserve true resource/material/layout invalidation and current reader lifetimes.
- Evidence: [69122 two-scene report](research/integrated-build-69122-quiet-club-20261115.md), privatebusy cpu-analysis.json observeMetalSourceDependencies9.521CPU-s inclusive/Signal::observe7.888CPU-s; llspatialpartition.cpp3763 revision guard and complete relay recreation.
- Next action: Preserve exact ordered signal identities across content-only revisions; reuse only the same State relay and identical current topology. Topology changes retain fresh subscribe-then-swap transaction. Share the production walker with dedicated tests and benchmark stable/content/topology changes at varying observer fanout.
- Verification: A texture/native-content publication advances State revision while dependency signal identities can stay unchanged; current code reallocates relay and resubscribes all signals. This cost is not automatically byte-cap churn: cache withdrawal does not advance producer revision. Current80-case publisher fixture explicitly stubs this walker and cannot prove the proposed change. No repair/runtime gain yet.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before scoped production-helper extraction/relay-reuse implementation. Include geometry, all image/material slots and all fallback dependencies; no layout-only keys, skipped notification, removed fallback, stale raw-pointer proof or cache-cap change. Test content changes, exact topology replacement/removal/duplicate identity, exception transaction, old observer expiry and independent readers.


- 2026-11-15 source296 extracted exact role visitor into shared production helper, weak State+ordered Signal-ID matching, captured pre-refresh revision, transactional topology replacement and five activation counters/logger. Initial vector-ID48-cell candidate correct/contentwins but low-fanout cold/topology+57–123%; retained as rejected storage attempt. One inline8 numeric-ID follow-up with arbitrary spill reduces that debt: content refresh−65.55..−99.83%,0alloc; low-fanout5-role cold+5.39%(7ns)/topology+9.73%(8ns);9/21role cold+14.62/+16.81%,topology+17.47/+11.96%;8192fanoutcold/topology+0.23..+1.30%. Stable-revision nearparity. These are isolated dependency-owner costs, not wholeviewerFPS or a measured runtime topology histogram.

- 2026-11-15 selectedsource296 CPUquick213old/227new PASS;48×9pairedblocks/1015commands complete0, expectedState/outcome/activationcounts matched. First temporary overescapedJSON pilot failure and original vector costs retained. Seven new native tests share production visitor and exercise distinct roles/fallbacks,8/9/64 identity sets, true content/topology changes, allocation failure, State identity, independent readers and reentrant epoch retry; old80-case source fixture remains unchanged and is not cited as visitor proof. SourceCPP43a7d1746d7e…/H114e72f82a44…/helperec12537fc96c…/appendixb3149da7f0a2…/mainincludefe82882407a0… frozen/copied. Root source review and earlier bounded peer review PASS; integration adds periodic logger call. Full build/native validation next, no owner closure.

- 2026-11-16 integrated source75/build69134 doctor/configure/build0, compiled topology diagnostic marker verified. Actual shared-helper native appendix378assertions/7cases PASS with Metal validation; existing80-case publication fixture26034assertions PASS separately. Full34 renderer scopes1130416/349 PASS; static native/deep signature/import/features and strict3595files/0blockers/0errors PASS. Exact measured production helper/CPP/H/test hashes match current source. No additional cost matrix or viewer run. Changed-content activation frequency and whole-viewer benefit remain unknown; cold/topology metadata debt remains. Automation does not close296; owner verification/push pending.

## PPD-297 — Native RGB byte expansion uses per-pixel dynamic copy work

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex Architecture Session / native upload author
- Discovered: 2026-11-15
- Original reference: Original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 LLImageGL upload uses declared format/type and unpack row layout. Native Metal RGB storage expands to supported RGBA/BGRA storage; preserve component byte order, opaque storage alpha, subregion offsets, row padding and swap semantics.
- Evidence: [69122 runtime evidence](research/integrated-build-69122-quiet-club-20261115.md), llimagegl_upload_native.inl prepare_metal_upload_pixels expansion loop; private69122 busy raw CPU export/derived shadow_copy_hotspot_busy.json identifies upload preparation among copy callers. Source-format mix is not recorded.
- Next action: Inspect optimized baseline code, implement a format-specialized U8 RGB/RGBA/BGRA expansion kernel with safe unaligned rows/tails, preserve wider/packed/float paths, and compare exact actual-helper output/costs with allocations included and retained.
- Verification: Current RGB expansion calls runtime-sized memcpy per pixel and writes alpha separately. Busy copy-like leaf samples include upload preparation, buffer writes and other scopes;15.355CPU-s memmove is not all shadow or all RGB8. The candidate's real contribution and byte-format frequency are unproved. No fix/FPS claim yet.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-15 registered before native upload specialization. Existing actual upload-layout tests cover RGB/RGBA/BGRA, opaque alpha, row padding/subregion offsets and swap/packed/float paths. Extend vector/tail/unaligned/page-edge coverage and benchmark realistic sizes; no quality, shader, decoder, upload-admission or cache-policy change.

- 2026-11-15 initial register harness rejected297 without a Markdown evidence citation; added the existing runtime-report link before implementation. No code/test failure.

- 2026-11-15 RGB297 source helperb453c0ba7895…/owningCPP27cc8ac2fa75…/test1f81205940c8… copied exactly before integration additions. Actual-inl optimized andASan/UBSan checks3376assertions/4cases PASS including guard pages. One serialized120cell/9paired-block cost matrix outputparity andallpairedmedians improve; fresh4096² median acrossformat/stride58.115ms→2.665ms, scopeCPUstage only. FirstwrongCatch2linkpath failure retained; correctedstandalonebuildPASS. Format mix/wholeviewer benefit remain unknown. Root adds activation counters outside measured purehelper at actualnativeupload wrapper and actualGPUreadback/counter tests; rows/mips/subregions are calls, notuniqueimages orGPUcommits. No per-pixel atomics or format/policy change. Full integrated build/nativechecks pending.

- 2026-11-16 integrated source75/build69134: native image/upload coverage6464assertions/34cases PASS, including actual RGB/RGBA/BGRA create/upload/readback, alpha255,17×2 vector+tail dimensions, swap false/true and exact cumulative counter deltas. Existing fixture disables upload workers, so exact deltas are coherent. Read-only independent integration review found no format/layout/counter blocker. Full34 renderer scopes1130416/349 PASS; doctor/configure/build, native static/strict PASS. RGB pure helper remains exactly the measuredb453c0ba7895… source; root wrapper diagnostics are outside its timing scope. Both new diagnostic strings occur in the exact executable. No runtime format mix, whole-viewer FPS gain, owner closure or push is claimed.


## PPD-298 — Final SSAO blur materializes a target with only one runtime reader

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native main-lighting integration
- Discovered: 2026-11-16
- Original reference: Inspect original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 light-map blur/base-light ordering before implementation. Preserve SSAO/sun-shadow math, RG8 conversion, alpha, sky, PBR/legacy/probe/projector/SSR and setting behavior. Native tile fusion is a new ownership mechanism, not a new quality algorithm.
- Evidence: [Whole-engine continuation](work/active/apple-first-direct-cohorts-20261114.md). Source3ed2540223 world.cpp23782–23839 renders horizontal and vertical blur to separate RG8 targets; pipeline17718 is the final-map runtime reader. MSL vertical blur13431–13483 filters only.g and retains center.r. Base geometry already accepts the resulting float2. Native debug, local/projector, sky/composition/exposure readers do not require a persistent final map.
- Next action: Current fusion candidate rejected by complete costs and restored to the separate implementation. Retain exact source/test/measurement history; do not activate or narrow the workload to favourable cells. Continue original owned-work/admission architecture; revisit this boundary only with a concrete new measured remedy.
- Verification: Exact native pixels14202/4 pass, but complete8-cell cost gate fails (exit42):1080p legacy/no-shadow GPU+11.09%,1080p probe/shadows+3.12%, CPU slower6/8cells, settled slower4/8. All80 parity checks pass. Candidate reverted; tile/allocation attribution remains unproved. No whole-viewer FPS claim.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before implementation. Independent reader audit confirms final target is consumed only by base light; sky never samples it, alpha stays0, modern sky follows, local/projector stages consume base color, and debug reads other attachments. Direct float inlining would remove RG8 quantization and is explicitly rejected. Exclusive author world.h/cpp/builtinMSL/new test in a separate worktree; root owns pipeline/CMake/docs/build/execution. No viewer/profile/push authorization change.


- 2026-11-16 root pre-build review of frozen author68eaa4ba5a finds a draft transaction gap: raw/horizontal work can enter an enclosing batch before base validation fails, but the combined wrapper lacks a guard covering those early returns. Add existing noexcept GBufferFailureScope around the complete new API and mark only successful returned targets; add an actual commit-count prefix rejection test before first77 build. This is source-confirmed in new unexecuted code, not an owner regression. Original unguarded author snapshot/hashes retained.

- 2026-11-16 registration initially failed the next-ID harness check because the marker was not advanced with298; corrected marker to299 and harness33 PASS before author implementation. Root caller draft now carries a pending main scene only for main+SSAO+RenderDeferredAtmospheric+capability, resets it at all matching capture boundaries, and records actual tile/separate encoded paths. New diagnostic is nonpersistent/defaultOFF until complete costs pass. Public separate light-map behavior and probe path remain intact. Obsolete GL-fallback wording at the touched failure paths is replaced with accurate native-encoding failure text; fail-closed behavior is unchanged.


- 2026-11-16 active-probe validation extension sourcec819c58bf454… replaces the legacy-only draft test while preserving the original seven cases, sky/malformed/prefix tests. Adds real packed-half SH/default+manual local radiance probes and a count2→1 RGB control before fused/reference exact parity. Cost frontier is8cells (2sizes×2shadow×legacy/probe), identical enclosing batching,2warmups+8alternating pairs; rows name environment. No execution yet. Root reviewed guard/caller ownership and freezes complete77 source next; no selection or FPS claim.


- 2026-11-16 exact77 source12ac914277/app69142 required build/static/strict and full34 renderer scopes PASS; main SSAO native pixels14202/4, private context392/5 and existing shadow/material/reload18390/29 PASS. The first complete cost matrix executes all8cells/160samples but fails one GPU regression CHECK (exit42;2343/2344assertions pass). All80 image-parity comparisons pass. GPU median wins5/8, CPU2/8, settled4/8;1080p legacy/no-shadow GPU+11.09%,1080p probe/shadows+3.12%. Warm candidate creates one additional memoryless texture wrapper per call. Allocation/tile pressure attribution is unproved. Reject activation and restore the previous separate implementation; do not cherry-pick favourable720p cases. Raw failed receipt and every sample retained in main-ssao-cost-77.* and summary. Initial summary parser assumed upper median; actual source averages middle values; corrected parser reproduces all emitted medians without rerunning cost.

- 2026-11-16 before reverting the draft runtime changes, preserve exact tested source and native test bundle/metallib under private main-ssao-rejected-77/provenance.json and local branch codex/apple-first-main-ssao-integration-tested. The source-supported reduction is not a universal complete-path improvement; extra MRT/tile pressure remains a hypothesis. Restore actual separate SSAO/base code and remove the unused new startup PSOs/diagnostic/caller, retaining independent native-context ownership, covered-reload checks and all earlier repair work. This is a rejected implementation, not a closed whole-engine goal or permission to lower quality.


- 2026-11-16 runtime source restore completed from pre-candidate3ed2540223 for world.h/cpp, builtinMSL, CMake, settings and pipeline, retaining accurate native failure messages plus independent context/packet/coverage and covered tests. The unselected SSAO test is removed from the active build/tree and retained in the rejected branch/artifact. New frozen core build will validate the restored source before any further integration. No existing viewer feature or earlier accepted optimization was removed.


## PPD-299 — Preparation credits ignore byte debt and startup job bounds

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex coarse native encoding integration
- Discovered: 2026-11-16
- Original reference: The shared native frame budget is new to this fork; original viewer work queues are the nearest producer/consumer boundary. Preserve render completeness with same-Metal owner execution when optional CPU jobs decline, while bounding admitted asynchronous CPU work and lifetime.
- Evidence: [Original architecture continuation](work/active/apple-first-direct-cohorts-20261114.md). At restored-coredd2a3ba0dd, Work::preparation job check is inside initialized-only branch in alnativeframebudget.cpp390; supplied logical_bytes never enter preparation stats/denial and Credit retirement only decrements job count. Shadow snapshots enable another CPU batch/query array while prior packet is retained. No measured physical-memory/COW incident is attributed to this source gap.
- Next action: Add an explicit32MiB default logical preparation packet capacity cap, checked job/byte admission even before frame initialization, outstanding-byte stats and exact CPU-completion retirement. Preserve CPU-busy preparation; deny nonzero additional metadata under actual memory pressure. Snapshot failure/denial must retain packet/original admitted command for existing native execution. Native resource residency is accounted separately and must not be double-charged as packet CPU bytes.
- Verification: Source inspection confirms missing enforcement, not a runtime performance result. Common/native helper tests and integrated build remain required.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before budget changes. Exclusive overlap author now also owns common budget h/cpp and its test file; root owns CMake/docs/execution. Test real byte denial, startup count, checked arithmetic, shared-credit retirement, frame transitions/drain/pressure and original-command preservation. No app/policy cap increase based on nominal64GB; this is a temporary CPU packet bound.

- 2026-11-16 preparation299 fix integrated with production overlap: startup job bounds and checked32MiB logical CPU snapshot debt are enforced, memory-pressure decline preserves direct native execution, and shared credit retires exact jobs/bytes once at CPU completion. Four new common byte-bound cases and actual native denial/fallback tests are frozen but unexecuted. Independent common-source review found no blocker. Legacy zero-byte preparation callers do not retain an additional asynchronous packet; this is not a claim that all existing producer slabs or native resources are bounded by this CPU metadata cap.

- 2026-11-16 pre-build independent integration review finds a PPD299 lifetime gap in the new unexecuted task: worker releases preparation credit after encoding, while its charged packet arrays remain owned until owner join/recycling. A completed-but-unjoined packet can therefore be uncharged while retained. Register before repair; retain byte credit through packet ownership/recycling rather than GPU completion, with a deterministic completed-before-join admission test. This is source-confirmed accounting debt, not a reproduced viewer memory fault. Frozen79 is deferred until this integration issue is corrected.

- 2026-11-16 narrow repair review finds a second retained-capacity edge before execution: recycle/restore accepts empty destination vectors with reserved storage; swapping them can leave that uncharged capacity in the packet after marking both arrays returned. Author must release any incoming empty allocations or preserve their debt, and cover reserved-empty destinations. This remains the same PPD299 lifetime repair; no build or native execution has started for79.

- 2026-11-16 PPD299 repair is now source-frozen: reservation follows the CPU packet outside GPU CompletionOwners, survives encode completion/Outcome moves and releases after both array capacities return or packet destruction. Incoming reserved-empty vector storage is freed before release. GPU-completion BODY pointer arrays are explicitly excluded from this CPU-only charge. Nonblocking Dispatch completion observation supports a deterministic completed-before-join denial/move/recycle/restore/destruction case, with GPU retirement independently gated. Current shared-admission log includes preparation_cpu_bytes_inflight. Independent narrow source review finds no remaining blocker; native tests remain unexecuted before79 build.

- 2026-11-16 frozen79 build69148 verifies preparation299 with actual49assertions/4commoncases and the native task completed-before-join/moves/reserved-empty/recycle/restore/destruction/GPU-independent retirement case within2424/5. Full34 includes native frame budget6112/52 and passes1130524/356 total assertion/case executions. Required build/static/strict PASS; no viewer OOM, physical-memory gain, owner closure or push claimed. Effective-flag query first used the default Ninja manifest and returned unknown target without mutation; corrected build-Release.ninja query verifies the actual three new/touched objects and retains that invocation failure in its receipt.

- 2026-11-16 preparation299 integrated frozen80/app69149 repeats actual byte49/4 and native task2424/5 PASS, full native-budget6112/52 within complete34renderer scopes PASS. Selected owner candidate includes packet-owned retirement repair and current byte-debt/denial census. Source-confirmed gap is repaired and contained native-verified; owner runtime verification remains pending, status investigating retained. No physical working-set/whole-engine benefit or owner closure inferred from logical-byte tests.


## PPD-300 — Frame admission cannot observe required-main native completion backlog

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex whole-engine scheduling integration
- Discovered: 2026-11-16
- Original reference: Original viewer display/probe ordering and GPU fences retain required presentation before ordinary probe work. Native scheduling must distinguish required-main completion from optional-group credits, CPU time, display/scan-out and summed GPU command durations.
- Evidence: [Integrated69122 engine evidence](research/integrated-build-69122-quiet-club-20261115.md). Current846f2781 source: ALNativeFramePlan fills main-thread CPU/interval and memory inputs only; ALNativeFrameBudget optional probe checks these, streaming and own outstanding credits. Window-owned ALMetalFrameScheduler has actual completed-handler retirement, pending/max and ordered submission serials, but these do not enter frame-plan diagnostics or optional admission. Quiet ordinary probes account for27.078CPU-s/31.7%main with high reported GPU-use; this does not prove harmful backlog or a scheduler cause of low FPS.
- Next action: Observe the integrated69151 census across owner whole-engine quiet/busy/loading evaluation, distinguishing normal pending presentation from harmful optional queue competition. Select any GPU-priority/deadline change only from actual queue/pass evidence; global GPU duration sums and ALU/utilization claims are not main-frame latency.
- Verification: Frozen81/source2038c60c36/build69151 required build/static/strict PASS. New real native census60assertions/1case verifies reserved-unsubmitted,3pending/full,retired,unknown and unchanged budget/timing/commit facts; existing outer-frame56/2,probe136/3,budget6112/52 PASS. App phase/logger wiring is source-reviewed. This repairs observation only; no harmful runtime backlog, GPU-priority policy or whole-viewer benefit is proved.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before census integration. Preserve all source80/69149 staged app and selected policy; root owns app-loop sampling, pipeline logger, CMake/docs/build. Helper author owns native-frame-plan census and dedicated contained native test. Sampling must not allocate, wait, submit, activate timing/validation, retain scheduler/window pointers, change admission/probe cadence or treat failed retirement as successful display. Counters are independently sampled, not a coherent submitted-minus-completed identity or scan-out measurement.

- 2026-11-16 implementation source frozen: value-only two-phase census reads scheduler stats, last-submitted serial and failure-inclusive retirement with no pointer retention, GPU wait, timing activation or admission mutation. Actual doFrame hooks sample after outer-plan begin and immediately before ordinary probes after display/required-CPU accounting; periodic frame-plan logging labels independent counters and non-scanout scope. New real-device test covers unsubmitted reservation, gated3-submission capacity, retirement, unknown scheduler and unchanged budget/timing/submit counts. Author/peer component review finds no blocker; integrated phase review and frozen81 build/checks next, no execution result claimed.

- 2026-11-16 frozen81 source2038c60c365b63d6b55e705cb0660efb781106b2/app69151 doctor/configure/build0 (32.297s), real native census60/1 and current owner/probe/budget checks PASS with API/GPU validation; native bundle/strict3597zero blockers/errors PASS. Exact scope whitelist proves observer/app hooks/periodiclogger/test-only changes and byte-identical existing FramePlan methods; all three native renderer/common test binaries match80 exactly. Prior80 full34renderer and worker costs are retained as explicit reused evidence, not new81 executions. New combined69151 stage is VERIFIED8273 identical inventory nodes; all19 SHA256SUMS rows pass, executablee7b29adc9214f0851a2089960edb1c422a6b6791e42f656f9a23f224967537c8. Existing69149 and earlier apps preserved. New sampling has no live-viewer performance measurement and no admission/policy/quality change. Owner runtime remains pending; no viewer launch/login/profile/push.


## PPD-301 — Early static GPU visibility lacks resident publication and later-consumer ownership

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex original G4 integration
- Discovered: 2026-11-16
- Original reference: Original LLSpatialPartition cull, stateSort/postSort pass/occlusion/area policy and ordered queries remain authoritative. This adds speculative native bounds work over existing immutable static producers; it does not replace simulation/selection/asset discovery or reorder alpha/query spans.
- Evidence: [Original architecture continuation](work/active/apple-first-direct-cohorts-20261114.md). Current5da3a2eb: camera matrices exist before main updateCull, and successful group seal/current publication owns complete static cohorts/native tuples. No weak typed resident-group index is exposed before CPU cull. Native Visibility makeView is built after draw capture; appliesToCohort requires late logical offsets and drawIndexed requires its original command token. Warm retained-direct capture already avoids rebuilding candidate Batches. Moving the old late dispatch alone would not create early GPU/CPU overlap or bypass CPU bindings.
- Next action: Keep RenderMetalEarlyGroupVisibility nonpersistent/defaultOFF after failed complete-cost selection. Integrate visibility with actual native ordered cohort command consumption so it removes per-row CPU submission; preserve query/raster splits, current owners/transforms and required-main publication priority. Inspect existing native bank cost failures before choosing a consumer. Do not repeat standalone early dispatch or force readiness waits. Whole-engine69151 runtime analysis is separate pending evidence; old apps remain preserved.
- Verification: Source9be33e2b3e/build69155 doctor/configure/build0; native early500assertions/4cases, actual producer75/2, existing visibility7521/13 and shader reload1248/6 PASS with API/GPU validation. Static native/strict3598files0blockers0errors PASS. Source83 complete34renderer1130599/358 is explicitly reused through unchanged105archives/objects/native libraries and exact source/test-scope proof, not reported as new84 execution. Complete24-cell480-row actual-owner cost passes195988/1 output/accounting but enabled policy is rejected:16/16 prefix-active owner medians slower16.61–86.59%;12/16 settled medians slower; no whole-viewer benefit established. Decision/every raw row retained in canonical evidence early-visibility-selection-84.json and early-visibility-cost-84-summary.json.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before implementation. Preserve69151 and every prior staged app/receipt. Native author owns visibility h/mm and dedicated test; registry author owns new early-registry helper and two successful seal hooks; root owns app/pipeline/renderer/settings/CMake/docs/integration/execution. Early GPU work is bounds-only and must release CPU/native geometry snapshot owners after encoding; speculative bounds must not become an idle resource pin or force unrelated backing COW. All command-slot trimming/reuse must honor the later reader. New flag stays nonpersistent/defaultOFF pending complete costs. Initial main-view scope preserves existing probe/hero/shadow transactions; original broader goal stays active.

- 2026-11-16 PPD301 source implemented for coherent82 review: successful-seal weak registry, before-main-cull copied region/camera bounds command, completed-success-only later reader, exact owner/control-block/view proof and sparse logical mapping; original same-command and slot pool checks remain. Early tickets shed CPU graphs/native geometry immediately after encoding. Root declines depth prepass and incompatible rendering modes before speculative submission. Real-device four-case native and two-case actual-producer correctness plus approved24-cell cost frontier are authored, unexecuted. Baseline is selected stable retained-direct, candidate factory is lazy/timed, real publication caps/debt and all pending fallbacks are reported. No GPU-owned discovery/CPU binding elimination, physical memory reduction, runtime gain or owner closure claimed; flag stays defaultOFF.

- 2026-11-16 pre-build PPD301 integration peer review finds new cumulative early-adoption counters omitted from commitShaderReload state transfer; a successful reload would reset counters and corrupt unsigned delta diagnostics. Registered before repair. Root carries early_visibility_stats beside existing cohort/CPU visibility statistics; renderer correctness and shader-reload cases must execute before cost work. No viewer-observed incident or successful retest claimed.

- 2026-11-16 frozen82/source1b3d1b3ca5 build fails compiling the new later visibility reader: ALMetalRenderEncoder::nativeCommandBufferForIndirect remains private and the new class lacks the narrow friendship granted to existing native argument consumers. Doctor/configure pass; native tests and costs not run. Register before repair; add only the new typed reader friendship, preserving private API and command-token check, then freeze83 and rebuild. Failed82 logs/receipt retained.

- 2026-11-16 frozen83/source6df64c8c22 doctor/configure/build PASS (179.850s). First native early scope executes4cases/263assertions with API/GPU validation:3cases262assertions pass, one REQUIRE at new fixture material publication line66 returns null. Validation correctly stops before static/full34/costs; receipt/log retained, no viewer operation. Register before repair: native author diagnoses the actual fixture publication ABI and will expose the precise publication error before changing it. No native-path correctness or performance pass claimed.

- 2026-11-16 PPD301 failed83 fixture cause source-confirmed: legacy native material prototypes forbid generic alpha_mask; legacy MASK uses material_alpha_mode2 plus material slot cutoff. PBR additionally needed its own slot cutoff to exercise intended mask coverage. Only the new native fixture now supplies correct family ABI/UV offsets and error context; all families, rows, queries and format coverage retained. Actual-producer75/2, existing visibility7521/13 and shader reload1248/6 independently PASS83 with API/GPU validation. Full34 executing on frozen83 while root-only test repair is reviewed; no cost measurement or viewer operation.

- 2026-11-16 frozen84/source9be33e2b3e627bdb49f1fdae65076b513911bf4f build69155 doctor/configure/build0 (29.584s). Corrected native early500/4, actual producer75/2, old visibility7521/13 and shader reload1248/6 PASS. Existing production34 scopes already PASS1130599/358 on83; only docs/new early-test fixture differ,105 production archives/other native-test objects/metallibs unchanged and llrender/common binaries identical, so explicit native-correctness-reuse-84.json retains actual83 execution provenance. Current static/signature/ARM64/imports/features and strict3598zero PASS. Both initial82 compiler failure and83 mask-fixture failure remain retained.

- 2026-11-16 one complete actual-owner84 cost run, no viewer/profile:24cells480samples,195988assertions/1case PASS output/query/accounting. Enabled early visibility REJECTED: all16 prefix-active conditions have higher owner-wall point estimates (+16.61..+86.59%),12/16 also higher settled medians. Measured8-source requested views adopt0/128;256-source adopt24/104, with all fallback/first factory/loading rows retained. Warm256/one-view prefix uses one of existing16 shared publication credits: baseline settles groups atsample4, candidate atsample5. This is contained admission contention and deadline misses, not a live-viewer bug attribution, harmful-main-GPU-backlog proof, changed cap, or hardware-wide verdict. CPU still performs cull/capture/bindings, so standalone bounds does not remove the work that dominates the owner. Mixed256 groups preserve inside/outside rows; wholly-outside one-row fixture groups cannot prove extra rejection benefit because real CPU cull already rejects their tested bounds. Flag remains nonpersistent/defaultOFF, tested source preserved as codex/apple-first-early-visibility-tested-69155; no new app staged/recommended and69151 remains owner candidate. Core original goal stays active/partial; no launch/login/profile/push or owner closure.


## PPD-302 — Static native bank submission has no integrated GPU visibility consumer

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex original G4 integration
- Discovered: 2026-11-16
- Original reference: Original1eb44bbd ordered LLDrawInfo/pass capture, indexed type/offset, raster and occlusion-query boundaries remain authoritative. Native compute-written retained commands are new; no original GPU command equivalent exists. Static opaque submission and material/lighting outputs must remain equivalent.
- Evidence: [Architecture continuation](work/active/apple-first-direct-cohorts-20261114.md). Current9a6bc0b92d static prepareCohortForCommand cold-encodes full-count calls and renderer skips actual CPU rows using ordered executeRange; scene.native_visibility explicitly declines banks. Animated preparation already uses same-command compute to update ordered ICB counts without resetting bindings. Source41/42/50 full warm bank hits still incur GPU penalties, so reducing metadata alone cannot establish useful native visibility/submission. PPD301 early classification does not remove CPU bindings and failed costs.
- Next action: Add a distinct visibility-qualified DIRECT bank mode with immutable cold bounds/index descriptors, current fixed-slot region/globals input and same-command GPU count updates. Preserve full-count banks, current native generations/base instances, sparse query/raster ranges, bounded publication and no CPU readiness wait. Compare against selected stable retained-direct with actual small/mixed groups, cold/warm/churn, all-visible/partial rows and complete GPU/owner/settlement costs before any enabled policy.
- Verification: Implemented and current88 native/actual-producer/full34 correctness PASS. Actual qualified bounds observed in92 cost samples (4816/4816 encodedrows). Complete cost does not establish an enabled advantage; keep modeOFF. No viewer or FPS claim.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before implementation. Root owns world.cpp/h, shared shader, pipeline/settings/CMake/docs/integration/build. Native command author owns sourcecommands h/mm and dedicated test; producer author owns sourcepages h/mm bounds certificate and exact encoder identity; cost author owns one actual-producer appendix/inclusion. New diagnostic stays defaultOFF/nonpersistent. No viewer launch/login/profile/push.

- 2026-11-16 pre-build302 author review finds an API lifetime gap in new unexecuted source: a borrowed ALMetalDevice wrapper pointer stored only to create later GPU metadata/pipelines could outlive its wrapper even though native MTLDevice remains alive. Registered before repair; new visible-preparation API now takes an explicit live ALMetalDevice reference, validating native identity like existing animated preparation, and stores no wrapper pointer. Native mechanism coverage also adds a clearly labeled synthetic-bounds probe to prove GPU zero-count writes independently of outside geometry producing the same pixels. Valid production-bounds parity tests remain separate. No native execution claimed.

- 2026-11-16 pre-build302 fixture review catches an incorrect unconditional positive-query assertion in the new actual-producer appendix: its deliberate one-row PBR-outside stress should produce zero samples. Registered before test repair; preserve the outside scenario and compare correct expected per-query visibility plus arm parity. No scenario removal, native execution, or rendering defect attribution.

- 2026-11-16 the pre-build actual-producer query concern above is DISPROVED by source trace before any assertion edit: ActualEarlyVisibility::addQuery uses a fixed visible proxy, and world encode_visibility_at draws that proxy independently of source geometry. Outside source rows therefore do not imply zero proxy samples. Existing positive-proxy controls and exact cross-arm parity are preserved with an explanatory comment. The separate native ranged-draw test, including synthetic bounds probe, directly observes zero-count ICB writes. No confirmed renderer or fixture defect is inferred from the withdrawn review finding.

- 2026-11-16 PPD302/303/304 source now peer-reviewed for coherent85 freeze: retained static DIRECT ICB counts are GPU-updated on the same command before rendering, cold64B source bounds/index facts plus current fixed-slot view data avoid N warm CPU writes; existing ordered query/raster range skip bypasses CPU scalar binds. Typed cold table-declaration certificate eliminates per-table command-retain loops with exact encoder lifetime identities and live/idle metadata charging. Four new native command, four certificate and two actual-producer cases are authored; complete24cell/720row three-arm frontier retains real budgets/loading/churn. MSL3.1 compiler-only preflight PASS; all native execution/build/costs still pending. Flag remains defaultOFF.

- 2026-11-16 frozen87/source9644aea425 build69159 doctor/configure/build PASS30.186s. Current static2810/4, certificate609/4, actualproducer2952/2, early500/4 and full34renderer1134160/364 PASS with API/GPU validation. Static exactapp/ARM64/signature/imports/retainedfeatures and strict3598zero PASS. Single24cell720row complete cost passes141123/1 parity/accounting but visible banks remain slower overall; large requestedwarm medians contain actualloading (nativebanksstartcoldsample5, truehits6–9), yet those4commonhit samples still show GPU penalties. No enabled selection made. Actual qualified producer bounds were not logged or asserted, so encodedrows do not establish cullingcoverage; register this observation gap before adding cold qualified-row census and actual-producer assertion. Source inspection expects valid bounds but execution has not yet observed them. No viewer/FPS inference or capchange.

- 2026-11-16 Apple official GPU-ICB sample and optimizeIndirectCommandBuffer documentation reviewed: optimization constrains executable range boundaries; indiscriminate fullbank optimization would violate retained query/raster-split execution. No optimization call is added on that assumption. Preserve exact spans/retained bindings; any future optimization requires an explicit valid-range contract and reset/re-encoding semantics, not just API activation. Sources: [Apple GPU ICB sample](https://developer.apple.com/documentation/metal/encoding-indirect-command-buffers-on-the-gpu) and [Apple optimize range contract](https://developer.apple.com/documentation/metal/mtlblitcommandencoder/optimizeindirectcommandbuffer:withrange:).

- 2026-11-16 PPD302 qualification observability now source-authored: cold Bank row loop caches qualified count; each successful compute dispatch adds that constant in cumulative encoded-qualified statistics (not visible/executed draws). Warm preparation adds no per-row host work. Native four-case counter tests and actual-producer warm8-row assertion plus24cell parser now require observed qualified inputs. GPU kernel, admission, lifecycle and range algorithm are byte-unchanged from87; only host census/log/tests differ. Freeze88 to test producer qualification before selecting or blaming the GPU route.

- 2026-11-16 qualification88 source24fe03/build69160: doctor/configure/build0/0/0; native2848/4, certificate609/4, producer3029/2, early500/4, full34renderer1134237/364, exactapp/static and strict3598zero PASS. Complete24cell720row cost and strict parser PASS;92 classified samples contain4816qualified/4816encodedrows. This rules out absent qualified producer bounds in the tested cost route, not all possible shader/culling failures. Independent native synthetic-bounds oracle proves actual zero-count writes. In common256warmhits6–9, visible owner medians about−43%, GPU+22.30%(allinside)/+20.99%(halfoutside), settled+6.97%/−0.05%; fullbank GPU+15.71%/+26.13%, settled+6.55%/+8.84%. Requestedwarm medians retain genuine publication debt; no quota override or untimedpreseal. OFF decision recorded static-visibility-command-selection-88.json. No further repeated cost run justified without a new algorithm or identified defect.

## PPD-303 — Source FrameView declaration proof uses a recyclable native encoder address

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native source publication
- Discovered: 2026-11-16
- Original reference: Every original encoder/pass must receive its resource usage declarations. Native argument-buffer resources and GPU-address roots require declarations on each actual encoder lifetime; an object address alone is not a lifetime proof.
- Evidence: [SourceFrameView binding](../../indra/almetal/almetalworldsourcepages.mm) and [native table declarations](../../indra/almetal/almetalworldresources.mm). Current almetalworldsourcepages.mm FrameView::bind compares view.declared_encoder raw id<MTLRenderCommandEncoder> to current native. A later encoder in the same allowed command can reuse that native object address and incorrectly inherit the previous encoder's declaration proof. Existing ALMetalRenderEncoder::declarationIdentity and animated command scopes already provide an exact inert lifetime identity. No live viewer fault or driver address-reuse incident has been observed.
- Next action: Replace FrameView raw-address declaration proof with the existing exact weak encoder-lifetime identity and cover fresh encoder invalidation/current command qualification without retaining the native encoder. Preserve every resource union/table declaration and normal binds.
- Verification: Source-confirmed proof weakness only; no reproduced live GPU fault or fixed test result yet.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before repair during302 review. Producer author owns sourcepages h/mm and dedicated tests; root owns register/integration. Do not change generic resource declaration semantics, cap or rendering policy to hide a missing declaration.

- 2026-11-16 producer review extends303 to ResourceTable raw native encoder marker used by bind/declareResources as well as FrameView. Exact weak encoder lifetime identity now authored for both; no observed driver object-address reuse is claimed. Certificate302/304 uses a separately owned native table-closure span and does not mutate per-table declaration proofs. Original1eb44 per-pass resource binding/flush is the nearest source contract; native resource declarations have no direct GL equivalent. Tests/build pending.

## PPD-304 — Cohort resource tables redundantly retain and declare an already-owned closure

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native source publication
- Discovered: 2026-11-16
- Original reference: Original accepted static pass owns every source resource through its submitted work. New native cohort certificates must preserve that lifetime and per-encoder resource declarations while avoiding repeated application-side discovery of immutable resources.
- Evidence: [SourceFrameView binding](../../indra/almetal/almetalworldsourcepages.mm) and [native table declarations](../../indra/almetal/almetalworldresources.mm). Current SourceFrameView::bind retains shared_from_this once, which owns CohortBinding tables; then visits each table through Resources::declareResources, redundantly retaining each table using the command's linear owner-vector search. T distinct tables cause quadratic owner comparisons. Native table/context validation and resource span submission also repeat despite immutable successful publication. Runtime dominance and whole-viewer attribution are unknown; old bank GPU penalties remain independent.
- Next action: Cold-publish one typed deduplicated cohort resource-declaration certificate, keep exact table/native owners and context identity, account allocated metadata, then declare its retained span once per exact encoder lifetime after command/FrameView qualification. Preserve ordinary noncohort resource behavior and all caps. Include unique-table, changed-generation, abandonment/pending-retirement and pixel/query correctness plus complete cost evidence.
- Verification: Source-confirmed redundant host work only; no measured saving, rendering repair or hardware-wide performance claim.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before implementation alongside302/303. Producer author exclusively owns sourcepages h/mm, resources h/mm and dedicated certificate/lifetime tests. No cap inflation or removal of declarations/native readers. This is support for the actual GPU-command consumer, not a substitute for that boundary.

- 2026-11-16 frozen85/sourceb7043e4efc doctor/configure PASS; build fails59.420s on new certificate test line160 using undefined viewer-only U64 in isolated almetal target. Native renderer/SourceCommands/SourcePages/Resources and MSL compile before this failure; no GPU tests or cost run. Register before root integration repair: use std::uint64_t in that test, preserving all cases and assertions, freeze86 and continue the same execution tree. Failed85 receipt/log retained.

- 2026-11-16 frozen86/source1c0035d301 doctor/configure/build PASS154.308s. Actual static GPU native4cases2810assertions PASS with API/GPU validation, including synthetic16/32 zero-count oracle, correct material/index/ranges and pending ownership. Certificate scope executes4cases583assertions:3cases582assertions pass, ordinary-table case line158 returns null during table preparation. No error context was printed; source/fixture cause unproved. Register before investigation/repair; producer author checks actual Resources preflight contract and adds context before any REQUIRE. No cost/full34/native completion claim for failed scope. Failed86 receipt retained.

- 2026-11-16 failed86 ordinary-table fixture cause is confirmed: its default ALMetalSamplerDescriptor had support_argument_buffers=false, correctly rejected by Resources::preflight; production SourcePages already enables that capability. Test-only sampler flag/INFO repair preserves4cases, renderer/caps unchanged. Actual-producer86 independently PASS2952assertions/2cases with API/GPU validation. No full34 or costs run; freeze87 for complete corrected native validation.


## PPD-305 — Detailed profiling template retains a short window despite five-minute recording

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex profiling workflow
- Discovered: 2026-11-16
- Original reference: Owner requests full five-minute quiet and busy CPU/GPU/log evidence. A recording time limit and successful CLI exit do not prove retained sample duration. The previously used Overview template has windowLimit0; installed detailed Game Performance has windowLimit5000000000.
- Evidence: [Performance evidence contract](PERFORMANCE.md). Exact69151/PID73806 captures return0 but TOCs report10seconds/Windowed5seconds; actual exported CPU ranges are only1.407quiet and1.166busy seconds. Full log/process streams remain preserved. Detailed template window mode was not preflighted before attaching; this is an agent capture-configuration defect, not a viewer defect. Quiet recorder also has221 DWARF cursor diagnostics, busy0; debug-map completeness is a separate limitation.
- Next action: Preserve both traces and all raw logs as limited evidence and retain the verified continuous template for a future specific measurement. Owner clarified additional recording was offered as help, not a prerequisite; continue architecture implementation without asking for another run. A future capture must verify retained coverage, exact executable and phase boundaries before use.
- Verification: Original capture0/TOC0/exports0 are genuine; five-minute CPU/GPU coverage FAIL. Derived CPU1.407/1.166s and GPU1.431/1.167s; raw CPU tails remain under2s. Copied installed detailed template changes only global windowLimit5000000000→0. Independent /usr/bin/yes20s test record/export exits0: Deferred mode,21.087841s metadata,12616 CPU rows spanning20.803002667s. retention-verification.json reports retention_pass=true; no viewer was launched. This proves controlled CPU retention, not a replacement five-minute viewer/GPU run.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before template repair. Build88 exactprocesssubtree remains SIGSTOPsuspended; no fresh viewer launched or settings changed. Owner closed69151 after requested busy measurement, so a new viewer launch requires owner action/fresh authorization. All original artifacts/recorder diagnostics and warning streams retained. Root owns capture repair and integration; agents own offline CPU/GPU/warning analysis only.

- 2026-11-16 PPD305 controlled nonviewer retention PASS (20.803s CPU span), original traces and all221 quiet DWARF diagnostics retained. CPU/GPU/warning reviews complete; no replacement viewer recording requested. Owner clarification keeps profiling optional for the active implementation goal. Build88 exact saved20process subtree resumed after full PID/args/ancestry verification, with receipt; no restart or viewer operation.

## PPD-306 — Texture preview requests absent controls and creates dummy controls

- Status: investigating
- Confirmation: reproduced
- Owner: Codex warning review
- Discovered: 2026-11-16
- Original reference: Original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 and owner candidate2038c60c365b63d6b55e705cb0660efb781106b2 have identical texture-preview XML and corresponding LLPreviewTexture requests for buttons_panel/aspect_ratio. Those names are absent from the XML. desc exists in XML and its generic floater lookup is a separate unresolved ownership question.
- Evidence: [Architecture execution ledger](work/active/apple-first-architecture-status.md); private artifacts/metal/architecture-69151-warning-review-20261116T072955Z/combined-source-warning-review.{json,md}, busy/warning-analysis.{json,md}; all186 wrapper warnings reviewed,0ERROR/FATAL.
- Next action: Audit live floater/control ownership before changing XML or lookup behavior; preserve texture preview functionality. This potential inherited contract issue is independent of the main renderer architecture and does not require another recording.
- Verification: Six dummy-control warnings observed in the full busy wrapper, including later activity. Source name mismatch is confirmed; visible loss of functionality, Metal regression and performance impact are unproved. No repair or runtime closure claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 source warning review retained per-phase counts and original callsites. HTTP/asset403/timeout/GLTF failures remain actual delivery failures with cause unproved (PPD122), embedded_items remains PPD127, map cleanup remains PPD294; none is dismissed merely as a harmless warning or attributed to the renderer without evidence.

## PPD-307 — Main G-buffer encoding cannot overlap independent point-light preparation

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex main light packet implementation / root integration
- Discovered: 2026-11-16
- Original reference: Original1eb44bbd ordered point-light limit, fades, frustum rejection, ordinary/projector separation and bounded/fullscreen grouping remain authoritative. Nearby-light updates and projector priority/cookie/shadow work retain their original traversal boundaries.
- Evidence: [Architecture implementation status](work/active/apple-first-architecture-status.md); current24fe03 endMetalWorldGBufferCapture seals owned inputs then synchronously calls renderOwnedInputs; point-value gathering in renderMetalWorldLocalLights runs later on the same owner. No independently owned early point packet or useful main encoding/owner-gather overlap exists. This is a remaining architecture boundary, not an observed crash or proved frame-rate cause.
- Next action: Separate exact CPU point-value gathering into an owned bounded packet. Optionally encode the preopened main command with one exclusive worker while the owner gathers values; join before any renderer statistics, sky, commit, query settlement or target publication. Preserve serial route and avoid OFF-route packet/task allocation. Decline active stage diagnostics; rebind original submission ownership; preserve renderer/TLS state and exception joins. DefaultOFF until actual producer/native correctness and complete cost justify selection.
- Verification: Source/design review only. Projector and native resource work stay serial. Early packet validates exact frame/camera/dimensions/light scale/count/attachment policy before reuse; deferred constants are consumed only at the original downstream boundary. No implementation execution, speedup or runtime acceptance claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before edits in isolated codex/apple-first-main-light-packet at24fe03. Main author exclusively owns prospective pipeline main-light/encoding hunks and new helpers/tests/settings; root terrain census hunks already committed separately3adb360d38. Root owns documentation, integration and sole executiontree builds. Preserve actual query/commit failure semantics and no replay after partial native encoding.

- 2026-11-16 PPD307 first scoped implementation integrated from8363194f57. One exclusive same-renderer main task joins before all sky/commit/query/target access; copied submission ownership is rebound, active stage diagnostics decline, unwind joins/cancels. Owner point arrays reserve budget before allocation and retain charge until consumed/destroyed; packet keys frame/camera/dimensions/scale/count/attachment policy. OFF route retains stack values without task/heap packet. Projector/resource work and nearby-light updates remain ordered. Authored five actual point-value/key/candidate cases, three native task/query/unwind cases and40cell800row full-frontier comparison including real producer, budget, allocation, dispatch, point consumer and original settlement. Exclusions and unselected defaultOFF retained. Source/peer review only; freeze90 build/native/cost still pending.

- 2026-11-16 first integrated90/source8eeb481 doctor/configure pass; build fails42.719s before native execution. Actual Light::drawable is LLPointer<LLDrawable>, so candidate helper const-auto-pointer deduction is invalid, though raw-pointer fixture accepted it. Register before repair: restore explicit borrowed LLDrawable* conversion as in original point gather and make candidate fixtures model an implicitly convertible handle, not only raw pointers. No app staged or test/cost claim; failed90 receipt/log retained.

- 2026-11-16 repair for90 uses explicit borrowed LLDrawable* conversion matching the original gather. Both new fixture Light fields now use implicitly convertible nonowning handles, preventing raw-pointer-only coverage from hiding this production type boundary; fixture stack/vector ownership is deliberately unchanged. No scheduling/shader/admission policy change. Freeze91 for build and actual native/producer tests before cost execution.

- 2026-11-16 sourcefd189 build91 doctor/configure/build0/0/0 (38.877s); point-value cases96assertions/5PASS. Native main3case suite has2PASS/1FAIL,589/590assertions: malformed nonempty batch adoption was rejected at the explicit native-resource-owner prerequisite, before intended native geometry validation. This is a fixture setup error, not a production rendering failure. Register before supplying an explicit owner lifetime shell to the deliberately malformed, unbacked geometry packet. Preserve actual native rejection/no-submission/no-replay assertions. Full34/static/cost91 were not run; native failure receipt/log preserved.

- 2026-11-16 source3876ff/build69167 (92) doctor/configure/build0/0/0, values96/5, native main596/3, full34renderer1135022/375 and exactapp/static/strict3602zero PASS. First cost92 fails42 after two zero-point startup rows: the fixture unconditionally opens/commits an empty third point-light command, whose completion may have unusable GPU timestamps. Actual accumulateLocalLights returns before command acquisition for zero lights (PPD250); three required timed receipts were an invalid fixture assumption. Preserve failedcost92 log/receipt. Register before repair: do not create an empty point command, still validate the empty native call; require two actual complete/timed receipts for zero-point controls and three for nonempty points, never ignore failed timing on real work. Positive point RGB contribution remains unobserved; cost can reject overhead but cannot select representative GPU shading without a positive control.

- 2026-11-16 sourcef04d025/build69168(93) doctor/configure/buildPASS41.529s; values96/5, main596/3, full34renderer1135022/375 and exactapp/static/strict3602zero PASS. Corrected40cell800row costPASS149364/1 and strict parserPASS. All320raw starts (256measured) have zero observed overlap; measured gather median0.000750ms/max0.0035ms, join median0.036917ms. Active-cell owner-wall paired medians worse31/32, range−0.43..+27.00%; settled−4.94..+9.90% mixed. Main CPU reductions do not select elapsed performance. Keep307OFF, preserve value-packet extraction for308. Synthetic getters/16×16 typed-clear frontier and unobserved positive RGB limit interpretation; no universal viewer/architecture/FPS failure claimed. Receipt/selection93 retain every startup/loading/denial/adverse pair and failed92.

## PPD-308 — Main lighting reopens render encoders on one accumulation target

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex main lighting render-pass author / root integration
- Discovered: 2026-11-16
- Original reference: Original1eb44 ordered base/sky/local/projector accumulation and later post-deferred haze-depth synchronization remain authoritative. Preserve currently accepted native order and arithmetic; do not opportunistically reorder light families.
- Evidence: [Architecture implementation status](work/active/apple-first-architecture-status.md); private lighting-renderpass-coalescing-source-review-20261116.{json,md}. Actual native main base, modern sky, point and projector calls share one RGBA16F accumulation target and command batch, but create separate render encoders. Point/projector do not sample accumulated color. Existing point/projector PSOs have no depth attachment; shared-pass variants require exact depth32FloatStencil8 compatibility.
- Next action: Implement one main-only compound native lighting operation receiving all ready scenes. Preflight cookie/shadow/BRDF work before encoder opening; encode the existing ordered bodies into one pass with explicit depth/no-write/state resets, preserving current standalone native APIs for other scopes and the comparator. Reuse joined307 point values. Preserve selected two-color memoryless probe fusion.
- Verification: Source-feasible only; no native execution or speedup claim. Haze remains outside because intervening forward/water draws and its current-depth snapshot require the later ordered boundary. No persistent encoder spans mutable pipeline gathering calls. Allocation/validation decline before prefix may use serial; post-prefix failure cancels the original transaction without replay/publication. DefaultOFF until actual native output/depth/query/lifetime parity and complete owner/GPU/settlement costs justify selection.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before implementation. Root owns docs/PPD/integration/build and preserved applications. Independent author owns prospective compound renderer APIs/body/PSOs, actual native tests/cost and pipeline ready-scene hooks, exclusively after307 author stopped editing. Existing command admission/caps, shader math, probe fusion and later post-deferred/haze boundaries remain unchanged. Need complete transaction/lifetime and representative-resolution cost evidence; removing explicit encoder boundaries alone is not proof of physical tile traffic or better FPS.

- 2026-11-16 pre-freeze308 peer review finds a source-only caller accounting bug: compound composition intentionally cancels on failure, but outer cached lighting_batch_active still attempts commit and increments the same failure counter again. Registered before gating commit by actual renderer commandBatchActive while preserving cleanup. No observed runtime incident or native test result claimed. Stage timing may represent one merged encoder; special base/workload diagnostics must explicitly decline or retain their requested semantics.

- 2026-11-16 pre-execution308 fixture review finds a31×27 continuation scene using an HDR-normal G-buffer without normal_hdr=true in the consumer. Registered before restoring the exact fixture format contract and adding explicit positive RGB lighting checks. Native preflight is preserved; no production relaxation or native failure/result is inferred.

- 2026-11-16308 coherent source imported from4da1017af8 with root CMake test registration/skip-PCH. Main ready-scenes collector consumes joined307 point arrays once, retains/rebases probe/hero/SSR/cookie/shadow owners and calls the actual compound renderer at composition. Shared native point/projector/modern-sky draw helpers preserve extracted body order, with lazy depth-compatible PSOs and explicit depth/state resets. Numeric307worker_exceptions and typed308 request/prepare/scope/diagnostic/input/preflight/prefix/serial counters are periodic. Both peer reviews find no remaining source blockers after registered fixes; five real native cases plus8cell cost authored, not executed. Positive RGB controls, borrowed depth/stencil/identity/query parity, one actual encoder and native-prefix cancel/retry remain gates for94. No new viewer/appstage/push.

- 2026-11-16 first308 integrated94/sourceb86c91 doctor/configure0; buildFAIL60.857s because ALMetalWorldOpaquePostDeferredScene has no normal_hdr member. Earlier source-review claim that this continuation required that field is DISPROVED: its actual format preflight reads dimensions/depth/color owner, not a per-scene normal flag, and the empty continuation has no normal-dependent draw. Register before deleting the invalid fixture assignment; production validation/API remain unchanged. Native94 not executed. Also register an evidence gap before adding untimed actual draw/commit census: frozen fixture observes4→1render encoders/positive RGB but does not yet report total draw or same-commit counts; no such gate has passed.

- 2026-11-16 test-only95 followup de75f30224 imported: removes the nonexistent continuation member; existing preflight preserved. Adds actual untimed normal/cost-parity encoder/draw/commit snapshots with paired scenario IDs before readbacks, same measured drawN and one commit, serial2/3/4→compound1 depending actual families. Instrumentation restores before timed samples; no invented census in timed rows. Five native cases retained;8cost cells/176raw timing rows authored. Peer source/diff checks pass; native execution and performance still pending95.

- 2026-11-16 native95/source39d4bf buildPASS160.817s, but new5case suite FAIL42 (400/409assertions). Nonempty compound calls decline with “Metal render pipeline requires a shader library from the same device”; dependent prefix-cancel assertions consequently never reach the prefix. Source confirms cloned existing pipeline descriptors intentionally clear library to avoid a borrowed wrapper pointer; new lazy depth-compatible point/projector PSOs failed to rebind the current renderer library. This is a real308implementation defect discovered before activation. Register before assigning mImpl->library.get() to both clone descriptors. Preserve all95failures; no cost, modern13, full34 or FPS result from95.

- 2026-11-16 sourceeb9f28/build69171(96) doctor/configure/buildPASS42.338s. Native5suite has2PASS/3FAIL,5523/5547assertions: eight positive-query fixture checks fail before any lighting API (proxy straddles near clip at z0), four post-continuation stencil checks assume preserved0x5A although existing post-deferred explicitly clears stencil0 at world.cpp20544, and twelve pipeline-membership checks use a timing window with frame_profile=false (recordPipeline intentionally returns without collecting identities). Actual serial/compound color/depth comparisons, positive lighting, canceled-prefix/retry and physical draw/commit/encoder census execute without reported mismatches; no all-correctness pass claimed. Register before test-only repairs: place positive proxy fully inside clip depth for both reverse modes, compare post-stencil to the unchanged native/original clear contract, enable identity capture only in untimed windows. Preserve positive occlusion and empty/outside controls, bit-exact pre-post depth/stencil guards and all source traces. No performance activation/cost run.

- 2026-11-16 fixture-only97 repair imported6b663e4d4c: positive query cube z.5±.2 stays inside clip depth for both modes, >0/outside0 controls retained. Untimed window explicitly enables frame_profile identities; timed measure untouched. Continuation compares existing serial/candidate clear0 and restamps0x5A between them because both borrow the same depth, so neither result can inherit the other's clear. Pre-lighting depth/stencil and color/query/positive-light oracles remain strict. Original view-bit context traced to1eb alwaterstencil.h32 and llviewerdisplay.cpp1065 deferred clear; current native post clear is unchanged. No production/API change or native97pass claimed.

- 2026-11-16 current97/source350b803597/build69172 doctor/configure/build0/0/0 (30.364s). Native main5627assertions/5cases, modern-sky242875/13, full34renderer1135022/375, exactapp/static/strict3602zero PASS. NativeAPI/GPU validation enabled for correctness. Actual same-API census is serial2/3/3/4→compound1 encoder, same2/11/6/15draws andonecommit; positive RGB, query, depth/stencil, native-prefix cancellation/retry all pass.8cell176row native-only cost/parity parserPASS; its exact command GPU medians decrease2.42–17.99% in this fixture, with all cold/warmup/adverse rows retained. This is diagnostic, excludes livecollection/Gbuffer/lightmap/assets andnormal inter-frame overlap, and does not prove viewerFPS/pacing.

- 2026-11-16 separately staged69172 VERIFIED8273 identical app inventory nodes/21checksum rows, executable b8e5596c3abcc69403a36d9bdb7935a991877edbc1f792b4513f2f161b41e9e4. Same app provides explicit308ON/OFF launchers; all other policy keys identical and307/banks/newvisibility remainOFF. Primary308ON is a provisional owner comparison, not an accepted performance policy. Source pinned local codex/apple-first-integrated-candidate-69172. No viewer launch/login/profile/push. Owner whole-viewer FPS/pacing, quality/functionality/loading and stability remain pending; mark ready_for_owner, not closed. Prior69151/69149/69134/69129/69122 preserved.

## PPD-309 — Runtime phase controller waits for a nonexistent recorder message

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex runtime collection
- Discovered: 2026-11-16
- Original reference: xctrace continuous20s preflight records successfully after printing Starting recording and Ctrl-C to stop; it emits no Recording started message.
- Evidence: [Instrumentation contract](INSTRUMENTATION.md); private architecture-69172-integrated-session-20261116T101436Z session/logs and prior controlled-record.log. Exact69172/PID65178 and xctrace65180 are live; CPU/GPU collection and full viewer/log observer are active.
- Next action: Keep the current recorder alive. Use a separate authoritative phase timer keyed to the observed CLI control prompt plus live login state; retain the original detector's startup label as invalid diagnostic metadata, without editing recorder output or restarting capture.
- Verification: Source check confirms the original artifact controller waits for literal Recording started, absent from the proven CLI log. Underlying recording/log collection remains active; phase timing, not trace retention, is affected. No viewer performance or rendering bug inferred.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before correction. Preserve current xctrace PID65180 and controller handle20469, source identity and all raw logs. Operator phase timeline will explicitly identify the alternate authoritative timer; actual retained CPU/GPU spans still require post-record verification.

- 2026-11-16 future collector artifact corrected to recognize the observed Starting recording/CLI control prompt and avoid sending SIGINT once automatic exit/Stopping recording/save/completion is already reported. Eight pure lifecycle cases and syntax pass against actual CLI evidence; future script has a fail-closed instrumentation qualification guard and was not executed. Active original controller/raw metadata remain unchanged; separate authoritative timer completed both300s intervals. This is not new live capture verification or closure. Evidence: private future-collector-logic-check.json and record-integrated-69172-corrected-not-executed.py.

- 2026-11-16 new bounded Overview controller uses per-scene recorder exit and actual saved TOC start/end as phase authority. Both300s scenes save/TOC0 with correct owner arrival markers; original faulty phase metadata remains only in discarded-attempt history. No fabricated recorder text or replacement of old evidence.

## PPD-310 — Integrated Instruments recording stalls during system-trace teardown

- Status: investigating
- Confirmation: reproduced
- Owner: Codex runtime collection
- Discovered: 2026-11-16
- Original reference: Exact69172/source350b803597 runtime; Apple xctrace continuous CPU/GPU template, owner-controlled quiet/club/logout session. This is a collector symptom, not a viewer crash.
- Evidence: [Integrated69172 assessment](research/integrated-build-69172-quiet-club-20261116.md); Private architecture-69172-integrated-session-20261116T101436Z xctrace-record.log, recorder-stop-sample-65180/65212.txt and recorder-raw-preservation.json. Full logs and LEAP histories are saved independently.
- Next action: Preserve the original recorder and raw data; investigate/recover offline without another viewer run. Verify actual CPU/GPU coverage and collection overhead before using any timing for acceptance.
- Verification: Viewer exited10:38:54UTC; recorder reports Target app exited / Stopping recording but no saved bundle or terminal exit as of10:43:51UTC. DTServiceHub waits in authorized_ktrace_stop/XPC reply; dtsecurity remains CPU-active. Raw269309939712-byte ktrace stopped growing10:38:55 and was APFS-cloned successfully. Adequate disk space remains. Cause and recoverability are unknown; no successful CPU/GPU coverage claim.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before recovery. Original recorder65180 and service65212 preserved; no repeated stop signal. Snapshot tools cannot sample protected dtsecurity65214 without privilege; noninteractive sudo denied and no credential requested. The large raw trace and active tracing helper also require an explicit measurement-overhead limitation.

- 2026-11-16 the original recorder resumed CPU-active processing after system-service teardown. Its raw attachment contains1261024more bytes than the early preserved snapshot; the extra bytes' nature and snapshot completeness are unverified. Duplicate offline import62938/PID71854 was reversibly suspended, then operator-canceled(exit1) to release analysis memory while preserving all raw data/receipts. This is not an independently failed recovery or proof of corrupt input. The original controller also sent its single SIGINT after xctrace already reported Target app exited/Stopping recording; retain this duplicate-stop risk without attributing the delay to it. Actual original save/CPU/GPU coverage remains pending.

- 2026-11-16 lighter recovery attempt starts11:22:50UTC from APFS-cloned later269311200736-byte attachment using installed Game Performance Overview(windowLimit0), handle44448/PID78586; original65180 preserved. Original progresses into DWARF symbol processing with cursor warnings; inspect actual symbol coverage after export, not warning counts alone. No new viewer or successful-retention claim. Prepared detailed parser qualifies only against short69151 fixtures and has sizing/RSS guards.

- 2026-11-16 owner explicitly stops recovery and orders old trace deletion. Jobs65180/78586 killed; original trace and both APFS raw snapshots deleted, receipts/logs retained. Fresh identical69172 viewer then records quiet300.826s/club300.737s with proven Overview template and390s/2GiB per-scene bounds: both record0/TOC0, final40.1/50.7MB. ActualCPU spans>300s and GPUmetric coverage100%/99.9469% verified. The failed detailed capture supplies no CPU/GPU assessment and must not be auto-resumed.

## PPD-311 — Live rigged skin data repeatedly requires nonfinite-matrix stabilization

- Status: investigating
- Confirmation: reproduced
- Owner: Codex integrated scene assessment
- Discovered: 2026-11-16
- Original reference: Original1eb44bbd1b llskinningutil.cpp multiplies inverse-bind and live joint matrices. Earlier native596bbc8ec1 stabilization predates the current architecture; current350b803597 bounded operand diagnostics identify invalid inputs/products.
- Evidence: [Integrated69172 assessment](research/integrated-build-69172-quiet-club-20261116.md); Private architecture-69172-integrated-session-20261116T101436Z analysis-warnings-review.md/summary.json and complete viewer log; llskinningutil.cpp217/235.
- Next action: Trace inverse-bind production/decoding and finite-product overflow with contained asset/matrix cases before choosing a repair. Preserve the guard against nonfinite GPU inputs and do not suppress the warning or claim bind-space fallback preserves intended mesh shape.
- Verification:70throttled notices represent at least6700palette calls by the last notice, not unique avatars. All16bounded diagnostics have finite world XYZ/W;10have nonfinite inverse XYZ and6finite inverse/world operands with nonfinite XYZ product. Asset/producer identity and numerical cause are unresolved. No measured FPS attribution or reported visible deformation in this session.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered the observed live quality fallback before repair. Busy-window55throttled notices are retained alongside transition/startup/shutdown evidence; source predates current architecture, which does not establish the origin of malformed inputs.

- 2026-11-16 unchanged fresh69172 has no nonfinite skin/operand/fallback warning. This does not close the earlier reproduced symptom or prove equal avatar/assets; no corrective source change was made.

## PPD-312 — PNG stderr warnings expose an unverified interlace/metadata path

- Status: investigating
- Confirmation: reported
- Owner: Codex integrated asset assessment
- Discovered: 2026-11-16
- Original reference: Original1eb44bbd1b and current350b803597 LLPngWrapper::readPng call png_read_info, normalization, png_read_update_info and png_read_image without png_set_interlace_handling.
- Evidence: [Integrated69172 assessment](research/integrated-build-69172-quiet-club-20261116.md); Private architecture-69172-integrated-session-20261116T101436Z viewer-stderr.log and analysis-warnings-review.md/summary.json. Structured viewer warnings alone omit these messages.
- Next action: Identify the emitting image path and exercise interlaced/noninterlaced PNG and metadata cases against the actual wrapper before a narrow API-order repair. Preserve original pixels and color semantics; no current image/FPS defect is proved.
- Verification: stderr contains one libpng interlace-handling warning and two gray-ICC/RGB-PNG mismatches. No timestamps or asset identities are present, so phase assignment and actual visual impact are unknown. Current/original wrapper call-sequence gap is source-confirmed; attribution of every stderr emitter remains unproved.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered all three stderr warnings separately from365structured viewer warnings. No application change or warning suppression attempted.

- 2026-11-16 unchanged fresh69172 again emits2grayICC/RGBPNG and1interlace stderrwarning. No asset/phase/emitter/visualeffect isolation; retain source/API-order concern without attribution to FPS.

## PPD-313 — Skin default and empty-input paths violate initialization/indexing contracts

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex integrated skin-source audit
- Discovered: 2026-11-16
- Original reference: Exact350b803597 and original1eb44bbd1b LLMeshSkinInfo constructors/fromLLSD/updateHash are byte-identical. llmodel.cpp1486–1510 leaves LLMatrix4a bind shape uninitialized; optional-field absence1549 precedes bind-pose composition1597. updateHash1687 indexes inverse[0] even when empty; llvoavatar.cpp10886 forms palette[0] after a possible zero-size resize.
- Evidence: [Integrated69172 assessment](research/integrated-build-69172-quiet-club-20261116.md); private analysis-skin-source-review.md/functions/excerpts/receipt preserve exact source comparisons and bounded callsites.
- Next action: Add actual CPU construction/default/missing-field/count-mismatch and zero-palette cases, then repair initialization/empty indexing without changing valid skin matrices, asset admission or current finite stabilization. Keep malformed numeric input semantics and PPD311 live cause separate.
- Verification: Source proves conditional uninitialized bind-pose input and empty-vector precondition violations. Inverse locals themselves assign all16lanes; bind-pose composition writes a separate vector, so missing bind shape is not an explanation for the observed nonfinite inverse coefficients. Normal inspected output/input owners do not alias. No executed reproduction, live occurrence, crash or FPS attribution for these subcases.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before repair. Bounded source comparison exits0; no tests/builds/viewer or application edits performed. Existing palette stabilization tests do not cover LLMeshSkinInfo decoding/default/empty contracts. Numeric hash uses lround then S32 narrowing; do not call a session hash collision or multiplication bug without focused evidence.

## PPD-314 — Multi-scene evaluation lacks continuous timestamped wall-frame telemetry

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex integrated frame-timing diagnostics
- Discovered: 2026-11-16
- Original reference: Exact350b803597 llviewerdisplay.cpp eligibility excludes teleport/logout/hidden/snapshot states; almetalbenchmark.cpp permanently invalidates a post-start ineligible sequence. The strict benchmark intentionally covers one controlled sequence, initializes once and has no UTC frame anchors/rearm API. This is a workflow coverage gap, not a defect in that strict contract.
- Evidence: [Integrated69172 assessment](research/integrated-build-69172-quiet-club-20261116.md); private analysis-frame-csv-audit.md/.json and analysis-frame-stats-report.md retain source and observed rolling-history limits.
- Next action: Add a small opt-in continuous wall-frame stream at the actual LLAppViewer frame boundary with measured steady/system timestamps and transition/focus status. Preserve strict benchmark semantics, normal rendering/loading, private local output and failure isolation. Keep this diagnostic work isolated until the current assessment is complete.
- Verification: Existing environment settings cannot provide complete UTC-aligned quiet/teleport/club/logout frame rows. LLStats histories lack frame IDs/timestamps and overlap; selected disjoint histories cover only232/250seconds of the two300s intervals. No completed new stream, runtime capture or whole-viewer improvement is claimed.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before implementation. Original69172/source350b803597 candidate and in-progress raw-data analysis remain frozen. Diagnostic correctness/component cost does not establish an architecture FPS gain; no new viewer run/push authorized by this record.

- 2026-11-16 implementation halted when owner rejects profiling rabbit-hole. Isolated codex/apple-first-continuous-frame-telemetry at254b74538a has uncommitted apphook/CMake/newmodule/test files. StandaloneCPU compile and119assertions/9cases passed; apphook compilation/fullbuild/runtime and peerreview remain unverified. Not integrated into69172; no further telemetry work or live jobs. Preserve files, do not represent as delivered viewer work.

## PPD-315 — Main-frame admission throttles three transactions as two commands

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex native main-frame admission integration
- Discovered: 2026-11-16
- Original reference: Exact350b803597 pipeline.cpp main G-buffer/sky17075–17399, light-map/lighting26449–26510 and opaque/optical8756–10337 use the same renderer/default2pending-command limit. almetalworld.cpp2434–2481 counts command entries, despite a comment describing two complete graphs. Original1eb44 uses end-frame pacing/resource reuse rather than this conversion-era intra-frame command cap.
- Evidence: [Verified69172 engine assessment](research/integrated-build-69172-overview-quiet-club-20261116.md); private fresh-session analysis-source-overlap-audit.md/receipt.json retain exact excerpts/hashes, CPU/GPU results and validation recipe.
- Next action: Implement explicit bounded main-frame groups with a finite invocation bound, preserving the three early commits, ordered native queue, completion-owned resources/credits and separate special-view policies. Event-gated actual native transaction/group, cancellation/error/output/lifetime checks before integration. Do not globally raise2to6 or merge the whole graph into one late commit.
- Verification: If current-frame G-buffer and lighting remain pending, beginning its post transaction waits for its own G-buffer. This source path inserts an avoidable CPU completion wait into normal frame overlap. Runtime incidence, magnitude and whole-viewer FPS benefit are unproved; Overview latency spans are not isolated GPU execution time.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before implementation after valid300s quiet/club evidence. Frozen69172/source350b803597 remains the baseline; no source repair/build/performance win claimed. Existing shadow ALMetalFrameAdmission{2,6} provides a local ownership precedent, not permission to change unrelated admission/drawable/upload limits.

- 2026-11-16 takeover independently re-confirms the normal three-transaction/two-command path. Native repair is isolated in codex/main-admission-takeover; root pipeline integration uses an explicit owner-local group serial and retains snapshot/probe/hero/diagnostic policies. Event-gated native validation and combined build pending. No live incidence, FPS benefit, owner acceptance or push.

## PPD-316 — Repeated dependency roles rescan the same Signal subscription

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex shared publication dependency optimization
- Discovered: 2026-11-16
- Original reference: Native llspatialpartition_native_dependencies.inl Relay::remember92–112 and refresh145–153 call Signal::observe for every role, although alnativepublication.cpp55–63 makes identical observer subscription idempotent. Original1eb44 current draw/resource selection and before-write invalidation behavior must be preserved; this Relay has no GL API equivalent.
- Evidence: [Verified69172 engine assessment](research/integrated-build-69172-overview-quiet-club-20261116.md); private fresh-session analysis-source-publication-review.md/receipt retain source and real-body validation conditions.
- Next action: Subscribe once per immutable Signal identity while retaining the full ordered role topology and existing ownership/inline-spill behavior. Validate repeated/distinct identities, producer mutation, transactional failure/reentrancy, cache withdrawal and actual subscription counts. No cap/retention policy or rendering/loading change.
- Verification: The real30-role fixture has3distinct Signals but performs30prune/observer scans; a correct dedup makes3actual subscriptions while retaining30ordered identities. Distinct30must remain30. Busy Signal::observe totals3.777CPU-s, only part redundant; no large or whole-viewer FPS gain inferred.
- Owner verification: pending
- Push approval: not_granted

### History

- 2026-11-16 registered before implementation. The apparent double world/shadow observation calls already have a revision guard and can be required after reentrant buffer publication; do not remove them. This bounded secondary candidate does not replace the primary frame-overlap work.

- 2026-11-16 takeover integrates reviewed local4050b02a88 as3c52f5dd29. Independent actual-body baseline has8assertion failures/4failed cases; candidate318assertions/9cases passes, including identical-unique-set duplicate order/multiplicity, invalidation, reentrancy and transactional failures. Full production test TU compiles. Private ppd316-takeover-validation receipts retained under successful69172 Overview evidence. Combined build and owner acceptance remain pending; no FPS claim or push.
