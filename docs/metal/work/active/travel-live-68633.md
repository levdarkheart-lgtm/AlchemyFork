# Owner-driven travel test — failed live QA, 2026-11-10

**Running code:** `97dd825aaf428da4a50e6b8bf1659d5b60d62eda`, build68633,
PID95871, executable5695b820fcd7a7a3252d1dedb4e91dd8cd90a0595a4afb4ec13417e58ffbac83.
Later documentation commits do not identify different running renderer code.
Owner logged in, traveled, resized and attempted movement. Codex did not move
camera/avatar, change preferences, rebuild or restart during this observation.
Owner subsequently offered to close the stuck viewer; Codex said captures were
sufficient and the owner could close it. No further live captures are required.

## Observed failure

Owner reported strange teleport display, missing avatar-name text until arrival
progressed (a screenshot shows a4m distance label without that name), repeated UI/
movement/resize unresponsiveness, and giant corrupt rigged triangles in another
screenshot. Resize initially helped, then no longer helped. This build FAILED
this live correctness check. Passing backend tests are not proof of live parity.
No controlled FPS comparison was attempted, and no speedup claim is supported.

Both owner screenshots are preserved/hash-pinned under ignored
`artifacts/metal/private-avatar-parity-owner-try-20261110-01/log-watch/`.
The full travel-observation.log and summary are private; they contain account,
region and session information and must not be uploaded/committed.
Read-only native UI inspection later observed changed location/camera controls,
current clock and many avatar clouds/name tags. That does not invalidate earlier
intermittent failures or prove keyboard delivery at the reported failure time.
All CUA inspection phases are completed; no controls were operated.

## Log facts

A teleport was initiated21:05:24UTC. Between21:05:27–32,399 warning rows identify
first native G-buffer invalidation at pipeline.cpp16548 with zero retained batches.
That is a classic terrain RESOURCE-readiness guard: fetched detail textures,
alpha ramp/optional overlay or generated height/composition map is absent. It is
not a malformed geometry/tessellation check. The log does not identify the member.
Native publication-success messages resume later. Subsequent warnings identify
pipeline16208, the missing native impostor quartet reader. Final21:18:51 snapshot
contains99 such warning rows. These are LOG ROWS, not counts of failed frames.
The generic warnings do not identify main versus probe view; do not assign all
terrain or impostor warnings to main-world publication.

Thousands-seen coalesced warnings report non-finite rigged-joint matrices replaced
by avatar-root bind-space fallback. That is object/attachment skinning, not the
classic body45float4 palette. The warning does not identify inverse-bind versus
live-joint operand or a particular corrupted draw. Neither it nor the screenshot
alone proves which producer generated the stretched geometry.

New policy receipts prove same-layout default-probe retention was selected and
both modern ambiance2 and legacy effective-ambiance0 display branches encoded.
That is branch evidence, not visual verification. Zero native avatar/attachment
profile-completion rows were observed; actual request/policy activation was not
verified, so do not claim the live ART producer has passed or failed from zero.

## Captured main-thread evidence

`unresponsive-210742.sample.txt` actual exit0 contains only3 observations after
DWARF analysis warnings. It shows shadow encoding but is not adequate for timing
percentages or an earlier/later sustained-hang diagnosis.

`unresponsive-2110.trace`: actual Time Profiler10-second attach,21:11:02.195–13.143
UTC, exit0. Export/parser commands, exits and hashes in
`unresponsive-2110-analysis-manifest.json`; README16a54e31 and manifest18db2338.
The capture has7,339 CPU rows. Main thread5,580 Running samples represent5.580
sampled CPU seconds over10.703seconds of main sample span. generateSunShadow3.728s
and endMetalSpotShadowCapture2.074s are INCLUSIVE/overlapping sampled ancestry.
The Running-only sampler cannot classify the remainder as GPU wait or CPU idle.

Runloop export contains345 complete main-runloop calls, with26–55 calls in each
trace second and max intercall gap82.18ms. No continuous single-stack main deadlock
was captured in that window. No keyboard dispatch samples is inconclusive at1ms
sampling. Runloop progress does NOT prove visible frame publication or input
handling at every time the owner reported a hang. Potential-hangs table is empty;
that is not proof that no intermittent hang occurred.

## Confirmed code discrepancies, not yet repaired here

Reference for each is exact `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.

1. Teleport state/progress code matches original. Native UI publication instead
   requires a COMPLETE CURRENT world presentation. A failed world can therefore
   suppress updated progress/controls along with the world, whereas original
   continued drawing/swap-publishing UI. Direct UI replacement is disabled for
   world display, so the absent composition transaction has no separate native
   presentation owner. A repair must retain compatible PRE-UI color/depth and
   render fresh ordered UI independently, without reblending a previous UI image
   (ghosting), stale-device/size sampling, or a GL fallback. This is a confirmed
   bad failure path, not proof it explains every reported freeze.
2. Fetched classic terrain textures bypass original bindSampled/default-image
   semantics. Original immediate-updates then substitutes Default/Null; native
   directly rejects a missing native requested image and clears the whole view.
   Preserve raw-vs-sRGB slow-fallback sampler distinctions. GENERATED height/
   composition maps are different and must not receive guessed fetched defaults.
   Add per-member receipts to identify the actual live missing input first.
3. Native impostor publication is void; generateImpostor clears needs-update and
   success timestamps unconditionally even if attempted/valid/complete/target
   guards failed and no native cache record was published. Logical render-target
   isComplete is not a native success receipt. Shared cache/device/HDR ownership
   appears consistent; the failed producer/scheduling context is unobserved.
   Only successful fresh native publication should clear the update request.
4. The port introduced per-joint identity-at-avatar-root replacement for bad
   skinning matrices, absent from original. Mixed valid/replaced influences can
   be finite while stretched. Valid-data weight decoding, column layout and root
   addition match original; general CPU transform sources also match. Capture
   the bad product operands/joint/producer before choosing a correction; do not
   call a blanket finite fallback a geometry-correctness repair.

Name-tag creation/visibility/positioning and llhudnametag/llhudobject/llhudrender
match exact original; no fully-loaded gate or tag-specific wrong port found.
Original name-value arrival, apparent bounds/angle, LOD/distance/policy and native
text/publication remain candidate boundaries. Current screenshot is not enough
to select one. No new source change, build or attempted symptom-hiding workaround
was made during this log-watching task.
