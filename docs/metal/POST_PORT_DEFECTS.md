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

<!-- next-defect-id: PPD-161 -->

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
