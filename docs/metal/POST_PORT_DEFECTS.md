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

<!-- next-defect-id: PPD-007 -->

## PPD-001 — Projector preflight can block the entire world frame

- Status: open
- Confirmation: source_confirmed
- Owner: Codex integrator
- Discovered: 2026-11-12
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, `indra/newview/pipeline.cpp`, `LLPipeline::setupSpotLight` and the deferred-light traversal.
- Evidence: [Source comparison and failed-session investigation](work/active/busy-turn-local-investigation.md).
- Next action: Record the exact projector rejection during the reported freeze; then specify and verify original-equivalent per-light failure behavior before implementing a repair.
- Verification: No corrective implementation verified; source comparison establishes the failure-scope mismatch.
- Owner verification: pending
- Push approval: not_granted

Native projector collection returns before completing the entire lighting stage
for some UI/protocol-admitted inputs. For example, independent light radius
1 metre and prim Z scale 2 metres produce `1.5 * radius - scaleZ = -0.5`;
FOV zero is also an admitted input. The resulting projection can be degenerate
or inverted: this record does not claim its individual light should shade
correctly. The confirmed mismatch is that the original continues the remaining
lights/world, while native composition and later world publication are withheld.

Camera-dependent light selection can expose this path. Attribution to the
owner's busy-region freeze, PPD-002, remains unconfirmed. Missing native cookie
or shadow data and other projector validation failures need separate diagnosis.

### History

- 2026-11-12: Source comparison confirms the whole-stage failure-scope defect.
  Analysis only; no correction selected or approved.

## PPD-002 — Busy-region world freezes while nametags keep turning

- Status: investigating
- Confirmation: reproduced
- Owner: Codex integrator
- Discovered: 2026-11-10
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, `indra/newview/llviewerdisplay.cpp`; current world and current-camera UI precede the same swap.
- Evidence: [Initial crowded-region report](work/active/crowd-live-68643.md), [failed 68845 retest and revert](work/active/busy-turn-local-investigation.md).
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

- Status: ready_for_owner
- Confirmation: source_confirmed
- Owner: Codex texture decoder workstream
- Discovered: 2026-11-12
- Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, `LLAppViewer::initThreads` sizes CPU workers before window/vendor initialization; inherited heuristic also exists in pre-fix `d537f10611`.
- Evidence: [Worker sizing fix, actual SL codec benchmark and local candidate](work/active/texture-decoder-workers-and-codecs.md).
- Next action: Owner tests local26.4.0.68855, confirms four ImageDecode workers and evaluates warm-disk relaunch texture settling/frame pacing; keep local pending explicit push approval.
- Verification: Actual old/fixed sizing bodies:16 cases, old9 failures/fixed0; M1 Max selects2/4 workers. Source64f10455ce build/346 ARM64 files/strict signature and56 tools tests pass. Isolated OpenJPEG CPU batch time falls46.30% full/48.31% mixed; no viewer relaunch/FPS gain or owner acceptance inferred.
- Owner verification: pending
- Push approval: not_granted

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
