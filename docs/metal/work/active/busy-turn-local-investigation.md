# Busy-region turning — owner failed retest and requested revert

2026-11-12. Owner explicitly reports that 68845 did not fix the brief whole-world
and own-avatar freeze while current-camera nametags continue to move smoothly.
The source-tested missing-impostor repair did not establish the live cause.

Owner requested reverting the unapproved push and prohibits future fix pushes
until they test, agree the fix works and explicitly approve the push. This
supersedes generic feature-branch push permission. Build/test success alone is
not approval. Keep investigation and proposed changes local.

## Completed revert

Normal revert `0f7b488264fc60e177e30d445502c8008d4fd37a` reverses `5ed2133fd4`
and `2c11d05d53`. Remote feature branch was verified at that revert. Its tracked
tree exactly matches pre-push `24515e9cdf`; no history rewrite, release change,
workflow dispatch or viewer operation. Revert harness passes.

Local branch `codex/local-busy-turn-investigation-20261112` preserves the failed
candidate source and the independent profiling commit `ade10da76d`. The local
integration retains the profiling documents and five untracked research files;
its subsequent records and approval rule are not authorized for upload.

## Observed diagnostics and next local work

The owner-run session produced 62 rate-limited older-world receipts. All had
first-incomplete line zero. In 61, the current G-buffer, light map and base light
exist, but composition and subsequent post products do not. Retained world age
reached 216 display frames in one sampled receipt; this is not a frame-time or
performance measurement. The remaining receipt reflects ownership transfers
and must not be diagnosed as a missing base-light producer.

The chain therefore narrows to local/projector-light readiness or execution
before composition. Those failures do not call markIncomplete, and per-frame
counter resets can suppress their thresholded warnings. A concrete source
candidate is the native projector's whole-stage return when its derived range
is nonpositive: independent supported radius/scale inputs can produce that
range. Original `1eb44` setupSpotLight does not abort the remaining world frame.
Missing cookie/shadow readiness and native accumulation failures can produce
the same product signature. No offending live light or exact guard is known.

Next local action: preserve explicit local/projector stage outcomes and their
first error without changing lighting admission, then obtain an exact failure
receipt before attributing the owner's freeze to a proposed repair. Inspect the
original behavior before changing range/asset failure semantics. No additional
implementation, runtime test or publish is claimed here.

Private evidence remains in `artifacts/metal/busy-turn-world-freshness-20261112/`
and `/private/tmp/alchemy-busy-turn-lighting-gates-20261112.json`.
