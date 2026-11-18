# PPD-399: wearable and avatar appearance warnings in 69258

The recorded sequence is normal outfit initialization and duplicate suppression,
with misleading wearable diagnostic wording. A valid first wearable of a type
can trigger the message claiming an invalid type, then be inserted normally.
All recorded stale-message warnings concern equal versions, not older updates.
The redundant bake request returns before HTTP submission. These findings do
not establish missing clothing, a visible avatar regression or an FPS cause.

## Scope and evidence identity

Investigation only, on isolated `codex/appearance-warning-investigation-20261118`
at source `47dd1fc5a6f9e65d85810264525067ce2287a71a`, worktree
`/private/tmp/alchemy-ppd125-startup-cause-20261118`. Exact recorded builder:
`afc9278d23d7fa5e800e322a7e1fa913e21d1070`; production:
`cfc20af4ba19c7df9d1cf406cc07a5eba4c7eda6`, build69258.
Original reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
Local upstream reference: `408fd6c0` (resolved full identity in source proof).
The original [checkpoint](shared-shadow-main-admission-checkpoint-69258-20261118.md)
retains the broader workload and capture limits.

The performance integrator reserved PPD-399 for the inherited misleading
wearable diagnostic; canonical388–398 remain separate and are not copied or
renumbered in this older isolated branch. The private reservation snapshot
records the actual canonical entry.

Only documentation and private contained evidence are produced here. No viewer
source modification, app build, launch, network request, settings change or GPU
work occurs. Earlier mirror and terrain fixes remain separate. The concurrent
performance worktree and its existing evidence are not modified.

Private evidence root:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/appearance-warning-investigation-20261118`.
`timeline.json` retains input hashes, line numbers, UTC times, numeric versions
and hashed messages, without exporting avatar identities or server URLs.
`source-proof.json` records exact function comparisons and source identities.
The contained fixture is described below; none of these is a visual QA result.

## What the three logs show

All UTC timestamps below are on 2026-11-18. COF means Current Outfit Folder,
the inventory folder whose version identifies the outfit being baked.

| Event | Normal, uphzzchc | Default, 77ib0k3e | Late180, sowsq5_l |
| --- | --- | --- | --- |
| Self appearance24621 accepted | line564,03:07:42 | line494,03:19:09 | line499,03:26:20 |
| Holding pattern complete,9 resolved wearables | 728–729,03:07:43 | 657–658,03:19:10 | 668–669,03:26:22 |
| Eight invalid-type lookup warnings | 731–738 | 660–667 | 671–678 |
| Already-satisfied bake request declined | 739–740 | 668–669 | 679–680 |
| Duplicate appearance messages dropped | 744,1177 | 671,982,1055 | 682,1053 |
| Self reaches fully-loaded lifecycle state | 1106,03:07:55 | 1069,03:19:28 | 1128,03:26:40 |

Each holding pattern reports done1 and timed-out0. Accepted appearance precedes
these warnings in STATE_INVENTORY_CALLBACKS. The outfit initialization and
request decline occur in STATE_PRECACHE. The first duplicate is also during
precache; later duplicates arrive after STATE_STARTED. Each log records one
accepted self appearance version,24621. No actual bake-submission emission or
Appearance Failure warning appears in these logs.

## The invalid wearable lookup is an absent-bucket diagnostic

At exact builder `indra/llappearance/llwearabledata.cpp`:

1. `LLWearableData` constructs an empty `mWearableDatas` map (35–38).
2. `pushWearable()` checks for the same wearable pointer using
   `getWearableIndex()` before creating a bucket for a new type (94–110).
3. `getWearableIndex()` obtains the wearable type and looks for its map bucket.
   If absent, it logs the invalid-type warning and returns false (215–227).
   This branch does not perform an enum-range check.
4. That false result is the normal first-insertion path: `pushWearable()` creates
   the bucket, appends the wearable, then calls `wearableUpdated()` (110–121).

Therefore an ordinary first insertion of a valid type deterministically emits
this warning. Missing bucket is not equivalent to invalid type or invalid asset.
The insertion is not rejected by this diagnostic.

The startup caller is holding-pattern completion
(`llappearancemgr.cpp:927–929`) through `updateAgentWearables()`
(2451–2458,2519) to `LLAgentWearables::setWearableOutfit()`
(`llagentwearables.cpp:1040,1140–1157`). Body parts use `setWearable()` which
can call `pushWearable()`; clothing calls it directly. All three eight-warning
bursts immediately follow the outfit-start row. Eight first-seen types would
explain the pattern; the old messages omit type values and stacks, so exact
per-row type attribution is not recoverable.

Constructor/driver setup is not a competing direct explanation for these rows.
Wearable import checks the enum before assigning it (`llwearable.cpp:339–347`),
and `setType()` assigns before cloning parameters (489–493,145–178).
Cross-wearable driver lookup uses `getTopWearable()`/`getWearableCount()`, which
handles an absent bucket without this warning. The default WT_NONE constructor
performs no index lookup. This does not prove all possible callers always pass
valid input; it explains why this message cannot diagnose invalid input.

A proper diagnostic correction would distinguish a valid unregistered type from
an invalid enum and retain duplicate-pointer checks, type/capacity rules and
wearable update behavior. Broad warning suppression or pre-filling every bucket
without checking the other map consumers would hide rather than resolve the
semantic mistake. No such correction is implemented in this investigation.

## The redundant request never reaches HTTP

`LLAppearanceMgr::serverAppearanceUpdateCoro()` yields, then rereads the current
outfit version and last received/requested versions. At exact-builder
`llappearancemgr.cpp:4174–4181`, `cofVersion <= lastRcv` logs and returns.
Request construction, the actual bake-submission log, assignment of
`mLastUpdateRequestCOFVersion`, and `postAndSuspend()` are later in the function.

All three messages report requested24621, last-received24621,
last-requested−1. The matching appearance was accepted earlier. This attempt
was already satisfied and cancelled before POST. It is not evidence of repeated
server baking or wasted HTTP requests. The coroutine's initial yield makes
rechecking those versions necessary even after an earlier enqueue decision.

## Every stale drop is an equal-version duplicate

`LLVOAvatar::processAvatarAppearance()` checks
`mLastUpdateReceivedCOFVersion >= thisAppearanceVersion` before replacing
`mLastProcessedAppearance` or applying parsed appearance values
(`llvoavatar.cpp:10610–10665`). Rejecting duplicates and older messages preserves
monotonic appearance state.

All seven recorded drops are received24621 versus previously-received24621.
The wording about rolling back is imprecise for these equal-version duplicates;
no actual older update is recorded in this group. The guard should remain.
Later fully-loaded messages support successful lifecycle progression but do not
independently prove every wearable layer, attachment or rendered pixel correct.

## Inherited behavior and separate recovery questions

The compared `getWearableIndex()`, `pushWearable()`,
`serverAppearanceUpdateCoro()` and `processAvatarAppearance()` bodies match in
original reference, recorded builder, this investigation source and the locally
available upstream. These warning paths were not introduced by Metal.
The wearable duplicate check is present in RLVa merge `ce27315819a` (2025-08-14).

Two source properties deserve precise limits rather than additional attribution:

- The last-requested version is a persistent high-water mark. A failed request
  at unchanged COF cannot pass the inclusive request guard again. Upstream
  deliberately restored this policy in `8006d0e628`, following `1edf48a0c7`
  (do not rerequest the same bake). The failure path requests a self appearance refresh,
  waits, and rereads canonical inventory COF state before attempting the guarded
  retry; corrected state is not guaranteed to have arrived. This is a recovery-policy question,
  not proof of a newly discovered bug or the cause of these log entries.
- The outstanding boolean is set when a queued coroutine begins and cleared
  when its local BoolSetter is destroyed. The packaged AIS pool has20 workers,
  also observed in the logs. If different-version requests overlap, a later
  coroutine can potentially clear the shared boolean while an earlier one
  remains active. Version guards still suppress equal-version submissions.
  Scheduler reproduction and an observable consequence are needed before
  treating this as a confirmed appearance defect. These logs show no such POST
  overlap. No concurrency or retry policy is changed here.

The confirmed issue is misleading wearable diagnostics during valid first
registration. The request and stale-message guards perform their intended
checks in the recorded sequence. None of this establishes the cause of a visible
avatar problem or the continuing FPS decline.

## Qualification and next action

Source tracing and the sanitized three-log timeline are the investigation's
primary evidence. The private fixture exercises extracted production lookup and
insertion bodies with explicit stubs:17 checks pass, compile and run exit0,
with no compiler diagnostics. First valid shape and first valid shirt each emit
a warning and insert/update; a second distinct shirt inserts at index1 without
another warning. Null lookup remains false/silent. Admission, notifications and
logging are stubbed; real capacity rules and avatar propagation are outside this
fixture. Its results, exact commands and limits are recorded in
`wearable-first-insertion-receipt.json` (SHA256
`d35e28a48d6a9a601fdfda74207a34f04dfdc74c320bd8771aea4ba7779f7e76`). It is not a full outfit, network or viewer test. Documentation
qualification uses `python3 tools/metal/validate_harness.py`; the initial pass
validates33 required files, exit0, and final amended-document qualification is
retained separately. No app build is necessary for this documentation-only
investigation.

If correcting the wearable diagnostic, retain range validation and existing
insertion/update semantics, and test normal first registration, repeated lookup,
duplicate insertion, invalid type and body-part/layering limits. Keep appearance
version guards intact. Establish a concrete failing recovery or visual case
before changing request scheduling or retry policy. No live visual acceptance,
functional repair, performance improvement or push is claimed here.

## Retained evidence hashes

- `timeline.json`: `88af1aead5251099fde047ebcdf968e7e6272b9b1cc86157a68b0fb48c7fe8f0`.
- Timeline extraction `receipt.json`: `4dab5603a485e2fbc2693f9fdc5f2a2c26b48211ee96d2df398126253105ff89`;
  corrected extractor exit0, exact expected counts/input hashes. Initial parser
  attempt exit1 and its correction remain retained; neither is a viewer failure.
- `source-proof.json`: `355beff9f68595dfe683bbc11bab8ba46baa6b4edeb22ac624c265770387238f`;
  generation exit0, all four compared bodies equal across all four revisions.
- `wearable-first-insertion-receipt.json`: `d35e28a48d6a9a601fdfda74207a34f04dfdc74c320bd8771aea4ba7779f7e76`;
  exact source-body fixture compile0/run0,17checks.
- `docs-harness-final.receipt.json` retains exact final command/exit, documentation
  hashes and base source identity. The first final-docs attempt exit1 exceeded
  CURRENT/ledger line limits by one each; its receipt/output remain retained as
  `docs-harness-final-01.*`. One blank line per snapshot was removed without
  discarding evidence/history, then qualification was repeated.
  Private scripts and raw command output are
  retained alongside their receipts; no user avatar identities are exported.
