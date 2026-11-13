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

<!-- next-defect-id: PPD-054 -->

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
- Push approval: not_granted

The application accepts next_url without retaining HTTPS; HTTP redirects also lack an HTTPS-only restriction. Both layers need the final upstream guard. No downgrade or credential exposure was reproduced.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

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
- Push approval: not_granted

A remote name is reused for the local cache target and cleanup; short names can also reach suffix substrings with invalid positions. Upstream generates independent local temporary names and bounds suffix tests.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

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
- Push approval: not_granted

Declared size is passed beyond a bounded copy, and session/inventory text is consumed as an unbounded C string. Keep copied byte length and bounded NUL searches together.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

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
- Push approval: not_granted

The predicate computes distance modulo 24 bits but the loop compares against the original 32-bit input. A high-bit number is unreachable by that wrapping loop. This is separate from PPD-002's unproved render-freeze cause.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

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
- Push approval: not_granted

Retained subcases: static name-value buffer overflow/stale fields, particle/TE/extra-param/motion length and type checks, fixed object-data buffers, patch-size and bitstream bounds, partial wind/land patches, and short initial Xfer headers. See the detailed general-review rows for the complete corrective sequences; not every malformed input was independently executed.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

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
- Push approval: not_granted

Retained subcases: an influence read can consume up to three bytes after a one-byte check; missing trailing vertices leave skin weights uninitialized; face/per-face/LOD index counts are allocated without upstream work caps. These production trees are identical to the fork base.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Independent final review identifies another inherited decoder subcase: nonempty Normal/TexCoord arrays shorter than the Position count are SIMD-read past their ends. Upstream 2a599a405c has the minimal size predicates; add compressed-input/ASan controls before the combined candidate is finalized. High-valence optimizer cost dc0f4b478c remains outside this minimal malformed-array repair.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

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
- Push approval: not_granted

Disconnect deletes LLWorld while later current-frame texture/UI work dereferences it. Related scene-monitor visibility paths also assume an existing singleton. Preserve both retained-world cleanup and monitor guards; no live crash was reproduced.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

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
- Push approval: not_granted

getCurrentTime and generate share lazy initialization, time and sequence fields without complete synchronization. Upstream reproduced duplicate IDs under concurrency; no collision benchmark was run on this Mac. Deterministic combine/name-based asset ID behavior must remain unchanged.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

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
- Push approval: not_granted

A full ring overwrites an older packet after it was acknowledged. Related final upstream corrections handle duplicate ACKs, copy windows, queue impairment and peers that never ping growing reliable history. These are protocol mechanisms, not an attribution to a rendering incident.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

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
- Push approval: not_granted

Retained subcases: map clearing leaves raw cache entry references/cursors; removal during a cache read needs deferral; cache objects and extras are not paired when no glTF override ever made extras dirty. Upstream tests are not current-Metal runtime verification.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending. PPD-026 covers cache-removal lifetime prerequisites here; disk/extras pairing remains later.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

The restriction check uses escaped params[0] while opening resolves an unescaped name. The two stages can disagree on the allowed floater. No restricted floater was opened during this audit.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

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
- Push approval: not_granted

The per-thread painter is reused without hb_raster_paint_clear. Failure after painting begins can leave a larger root surface for the next smaller glyph, allowing out-of-bounds compositing. Native font upload does not remove this CPU painter mechanism.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the First upstream security/crash batch. Local backport and regression validation in progress; retain original findings and keep owner verification/push approval pending.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

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
- Push approval: not_granted

readBackRaw fatally rejects dimensions above 2048 before reaching native readback although accepted images extend to MAX_IMAGE_SIZE (4096). Upstream uses the accepted limit and nonfatal failure for invalid requests.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

A plain initialized bool guards sorting of a shared string, so concurrent first calls can race. Retained companion subcases: malformed percent escapes lose/change text, an empty query returns an array, and leading-colon scheme handling differs. No current race reproduction was run.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

Retained subcases: failed/empty unzip result interpretation, single-line deserialization, empty XML keys and malformed XML/DOCTYPE acceptance, quoted URI/control serialization, nonfinite real formatting, date fraction/timezone round trips, separator/CR removal, and empty-target replace looping. The last is latent: upstream found no current empty-target caller. These are CPU contracts; no exploit or live corruption is asserted.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

An infinite or overflowing squared magnitude passes the positive threshold, producing zero/NaN components instead of the identity fallback. This is independent of Metal and has not been assigned to a current visual incident.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

When a loaded mesh gains faces, old code copies the last existing face despite earlier wire data describing the new faces. TE, material and glTF override can all be wrong until another update. Full viewer material assignment was not runtime tested.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

A child insertion can ignore its own capacity after a parent fills. Bring legal capacity/minimum-size clamps and retain the intentional exemption for unsplittable tiny nodes. Dense-geometry omission is a source-level risk, not a reproduced scene symptom.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

Provisional no-mask results may select a blend pass, but a later mask answer does not notify owners to rebuild; reverse transitions and provisional answers that were consumed also need handling. The asynchronous CPU path remains live here.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

Fetched-only slots discard dynamic local composites; PBR base-color bake body hiding, refresh and actual-bake shadow invalidation are missing. Retain final base-color-only/HUD exclusions and coalesced body visibility update. No owner appearance incident is assigned.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


## PPD-037 — Native HDR bloom exposure and probe paths lack complete finite boundaries

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-13
- Original reference: Upstream base 65c39b7bad928e165f1acdc0cbf39478e017ef1d; landed correction #595; 07094f82fe, 977e51afe9, 74589add5f, 9c451dc4a2, b929675d9c. Current audited fork 5a403168af62b5960126bf5bcd3c410ca791e3b4; almetal_builtin.metal local/projector lighting, base output, bloom extract, exposure meter/history, probe downsample/SH.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Source-only upstream/current comparison; no corrective implementation, runtime reproduction or owner retest in this audit.
- Owner verification: pending
- Push approval: not_granted

Native radiance clamping is partial: later multiplication/addition can overflow, bloom can multiply infinity by zero, exposure history can retain nonfinite values, and probe/SH writes lack final upstream protections. No flash/NaN incident was reproduced or tied to the owner's reports.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

Placed probes are rejected like automatic probes using occlusion state; off-frustum queries and preview mutation also remain. Native results are completion-gated, so upstream GL flicker latency is not a measured Metal fact.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

Default sun/moon construction passes altitude first to an azimuth-first API. Upstream deliberately changes visible default sun/moon positions when correcting the call; this is not a lighting-neutral update.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

Retained subcases: permanent temporary-use boosts, stale face-walk maxima, ineffective media decay, face area counted as visibility, and short/floor fetch results retried without upstream floor handling. Retirement/purge is a larger adaptation candidate, not a measured memory/FPS fix. Preserve current decoder admission.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

Retained reported subcases: object roster/orphan/region overrides; inventory IDs/counts/cycle guards; friends/mutes/group replies; notecard counts; SLURL/RLVa parsing; login/maturity/local-teleport/progress; Script Studio analysis/publish; camera steering; run notices and graphics-preset key omissions. These known upstream corrections remain candidates pending focused current-source/behavior tests; optional new features are excluded.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

Upstream reports coroutine deadlocks from thread locks and stale process/builtin table publication. The current HTTP mutex still uses the old type; this audit did not independently exercise every coroutine call graph or deadlock. Do not replace native per-thread locks indiscriminately.

### History

- 2026-11-13: Retained during the owner-requested upstream audit through 408fd6c014. No fix applied, existing incident attribution, defect closure or push approval inferred.


- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.

- 2026-11-13: d7b1bbbf1c imports independent snapshot/coroutine publication fixes. Keep this record scoped/partially addressed; no broad38-file lock refactor or unsupported precise waiter imported.


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
- Push approval: not_granted

This inherited final normalization path is distinct from PPD-022's byte-array bounds. The short-array repair intentionally selects the original absent-attribute fallback, exposing zero normalization in the retained older tangent implementation. Upstream's replacement uses unit3_or_zero; transplant that semantic boundary without importing the geometry rewrite.

### History

- 2026-11-13: Registered failed fixed-candidate public-API regression before correcting the zero-normal boundary. No visible incident, runtime owner verification or push approval is inferred.

- 2026-11-13: Combined candidate1f635/68914 passes build, affected tests and static bundle checks; [final validation](research/upstream-first-batch-validation-2026-11-13.md). Five unrelated pre-existing HTTP cases remain skipped. Owner runtime verification and push approval remain pending.

- 2026-11-13: Remaining allocation review flags the legacy createTangents normalize3fast loop as a possible unhandled zero-normal fallback. Requested an actual production-path probe before broadening the First Mikk-boundary result; do not infer complete zero-normal path coverage from the earlier three-vertex decoder tests.


- 2026-11-13: Reopened after actual frozen51d53a6dee direct createTangents and65535-vertex cacheOptimize fallback controls produce NaN normals (15failedchecks/17assertions/2cases, normal child42 underLLDB; no fault). First201/4 Mikk decoder proof remains valid but did not cover the legacy fallback. Register this confirmed remaining path before repair.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

Keep this reported cost mechanism separate from PPD-022 memory safety and PPD-002 owner freezes. No measured M1 CPU stall or speedup is asserted.

### History

- 2026-11-13: Retained after independent final review; the first malformed-array batch does not claim to resolve all algorithmic decode costs.

- 2026-11-13: Owner authorizes the remaining applicable upstream fixes. Reconfirm current source and integrate required corrections under [remaining batch](work/active/upstream-remaining-batch.md); optional replacement/performance projects require applicability evidence. No First-batch runtime acceptance or push approval inferred.


- 2026-11-13: Independent review found the optional budget returned success for invalid no-tangent indices. Production asset callers request tangents and already reject them, but the public cacheOptimize(false) contract needs unconditional structural/index admission before the cost guard. Register this correction before repair; retain zero-work success.

- 2026-11-13: Actual retained cacheOptimize(false) fan control exceeds incidence budget100040000>2^26; baseline reorders, fixed preserves all30000indices (3checks). Fixed public compressed decoder201/4 and unconditional empty/bad-index/nontriangle admission3/1 pass underASan/UBSan. No viewer freeze cause or FPS improvement demonstrated. PPD-048 separately tracks inherited allocation hazards.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

This is a new local backport-integration issue, not an inherited viewer incident. No published branch or First candidate is affected.

### History

- 2026-11-13: Registered independent review finding before correcting CMake test ownership.

- 2026-11-13: 4e230f49ad removes the unnecessary test implementation unit; independent no-dead-strip Debug probe passes. No Release-only linker inference used.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

The owner screenshot names almetal_tests. Both actual crash reports identify the scratch test app, and the source/stack confirms the two fixture defects. These are unexpected failures, not successful red controls. Raw diagnostics and private screenshot stay outside Git; source/log summaries are retained.

### History

- 2026-11-13: Owner reports two pop-ups. Runner paused, both reports independently inspected, implementation work continues without launching further GPU tests. New fixture fixes must pass review and corrective execution before readiness is claimed.

- 2026-11-13: Null-safe diagnostics and production-equivalent function-constant specialization reviewed. Final isolated LLDB HDR1230/2 and existing243759/19 pass with actual child exit0/MetalAPIvalidation; earlier faults/interruption are not counted as passes. No viewer launched.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

Effective scoped priorities do release correctly. Keep this legacy state invariant separate from PPD-040 priority correctness and unmeasured native resource/accounting consequences.

### History

- 2026-11-13: Retained from independent material/texture review; no speculative unpin or accounting redesign applied.

- 2026-11-13: Full source reader audit finds only state/profile bookkeeping, avatar debug logging and setActive refusal. Actual native pressure/LOD/downscale/reference deletion uses other owners/flags; scoped boost priorities release correctly. Remains unmodified and no measured functional consequence is claimed.


## PPD-048 — Retained mesh optimizer allocation failures can lose buffers

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex integrator and geometry reviewer
- Discovered: 2026-11-13
- Original reference: Retained LLVolumeFace::cacheOptimize Mikk re-weld/index shuffle; upstream dc0f4b478c also repairs allocation failure handling in its replacement implementation. The local incidence-budget backport covers cost only.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [active work](work/active/upstream-remaining-batch.md).
- Next action: Owner tests exact Alchemy Metal Upstream Fixes26.4.0.68936 candidate for this record’s affected behavior; no closure or push without owner verification and explicit approval.
- Verification: Required/optional allocation failure controls pass293/8 normal+ASan/UBSan and217/4 private aligned/null/new/fallback; exact old source fails both allocation cases. Ordinary fingerprint unchanged004fd052b5f3a793. Native LLFace4calls/preview1call and intersection now propagate false before raw tangent reads; final build/math/native checks pass. No physical OOM scene test.
- Owner verification: pending
- Push approval: not_granted

Keep this allocation/lifetime issue distinct from PPD-022 decoder bounds and PPD-044 optimizer cost. No complete dc0f4b478c allocation-safety equivalence is claimed.

### History

- 2026-11-13: Registered independent source findings before repair investigation; full candidate build running at2a5af4fdac. Existing hazards predate the new budget guard.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

### History

- 2026-11-13: Registered unintended scratch fixture failure. Direct failing launches stopped; subsequent tests require LLDB containment and actual child exit/state verification. Do not treat a setup failure as product evidence.

- 2026-11-13: Original09:58:08 OS report and contained stale-datapacker stack/link control preserved privately. Initial noticebar/SIGBUS association was provisional; the cache fault is independently traced to stale datapacker ABI, with corrective isolated-source proof. Canonical full build recompiles both.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

### History

- 2026-11-13: Registered platform-dependent test expectation before correction; root canonical target building. PPD-049 retains prior scratch linkage/SIGBUS history separately; no product attribution inferred.

- 2026-11-13: Portable test correction tested in isolated worktree; production layout unchanged. Integrator will cherry-pick after current frozen-source build completes.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

### History

- 2026-11-13: Registered independent applicable upstream admission change before repair; no renderer or asynchronous framework dependency.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


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
- Push approval: not_granted

### History

- 2026-11-13: Registered producer/consumer findings before implementation. Full geometry and GL asynchronous readback architecture are unnecessary prerequisites for this correction.

- 2026-11-13: Final combined sourceaf5c263988 /26.4.0.68936 Release/LTOOFF: doctor/configure/build,197CTest entries,22contained UI processes, native texture/HDR245071assertions/25cases,346ARM64files/signature/source/import/features pass.13existing skipped cases and19legacy geometry expected-failure cases retained unchanged; scoped evidence above remains authoritative. No viewer launch, owner acceptance or push.


## PPD-053 — Retained geometry suite has19 expected-failure contracts

- Status: investigating
- Confirmation: reproduced
- Owner: Codex geometry integration
- Discovered: 2026-11-13
- Original reference: Existing indra/llmath/tests/llvolume_test.cpp explicitly marks19 generation contracts !shouldfail; the file is unchanged fromb04d9bf6ef through this remaining batch. Corresponding normal/stitching/cap/hollow/flexible/degenerate repairs are in the larger upstream geometry stack.
- Evidence: [Remaining batch validation](research/upstream-remaining-batch-validation-2026-11-13.md), [geometry audit](research/upstream-geometry-review-2026-11-13.md).
- Next action: Evaluate and port the coherent geometry/tangent/dependency/uploader stack separately with original-output mapping, native integration and scene checks. Keep the19 known failures visible; do not remove expectations or claim complete geometry correctness from green CTest.
- Verification: Finalaf5c263988 llvolume_test returns0 under its existing !shouldfail policy:34cases,15pass/19fail as expected;3963assertions,3940pass/23fail as expected. Separate new safety tests311/10 pass. No new unexpected failure or owner visual incident.
- Owner verification: pending
- Push approval: not_granted

Contracts include cylindrical/mirrored/creased normals, S/T seams, caps, zero-area tangent contribution, normal length, hollow walls, flexible partial rebuilds and placeholder normals. This is a grouped register of existing expected-failure evidence, separate from repaired decoder bounds/cost/allocation/legacy zero/sculpt copy paths. It is not a newly introduced regression or full-stack port claim.

### History

- 2026-11-13: Registered known retained geometry contracts after observing actual isolated and final combined suite output. Current batch preserves all existing test expectations and records the larger project separately.
