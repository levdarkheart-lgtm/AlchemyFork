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

<!-- next-defect-id: PPD-010 -->

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

- Status: investigating
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference: 1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 indra/newview/llviewerdisplay.cpp628 begins world traversal at STATE_PRECACHE; llstartup.cpp2727/2760 and llprogressview.cpp retain a1s normal curtain fade without native readiness. Candidate pipeline.cpp2130 delays asynchronous world-renderer warm-up until STARTED. No original runtime comparison completed.
- Evidence: [Owner report and capture context](work/active/login-black-world-investigation.md).
- Next action: Investigate early asynchronous renderer/PSO prewarm independently of STARTED scene admission, then verify curtain/first-real-world publication timing and pixels without bypassing startup ownership guards.
- Verification: Original/current source comparison and frozen log confirm late asynchronous renderer preparation lasting6.60176s while native UI submissions continue and cleared loading background is available. Fits the owner's5–10s report; actual curtain branch/black pixels/first on-glass world frame remain unverified. No fix applied.
- Owner verification: pending
- Push approval: not_granted

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

## PPD-009 — Startup media indicator active while parcel music is silent

- Status: investigating
- Confirmation: reported
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, `indra/newview/llstatusbar.cpp`, `llvieweraudio.cpp`, `llviewermedia_streamingaudio.cpp`; aggregate all-media control and stream transport/fade ownership.
- Evidence: [Startup media-state investigation](work/active/music-startup-indicator.md).
- Next action: Implement owner-requested music-primary status when parcel music is enabled/present, preserve all-media actions and pending-stream cancellation, test the first-click path and build a local candidate.
- Verification: Source confirms unrelated active MOAP can select Pause while parcel music is stopped, making the first click stop media instead of starting music. Owner explicitly rejects this UX and requests correction. Exact incident/backend failure remains unproved; no backend retry or autoplay override planned.
- Owner verification: pending
- Push approval: not_granted

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
