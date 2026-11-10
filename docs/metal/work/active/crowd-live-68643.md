# Owner-run crowded-region responsiveness trial — 68643

2026-11-10. Exact canonical Release26.4.0.68643 /renderer2c60e47b0e, PID48817;
executable SHA256421fc603583e07f54a397baa5d38d51c814c18303bfd13d3a92fba5f13170790.
Owner logged in/traveled and reported many avatars, choppy UI/self movement and
coarse nearby-avatar movement despite top-right22–23 then24–25FPS.

Owner discovered max animated avatars11, increased it to40, then confirmed nearby
avatars moving and finally **Everything is responsive now**. This is owner-run
functional evidence, not an agent-controlled before/after benchmark or proof that
this setting caused every earlier UI/walking symptom. Original impostor animation
throttling explains limited animation; self avatar bypasses that throttling.
No source/render/settings/location/camera change by root, no restart or FPS-goal
resumption. Viewer remains running for owner. No new artifact/geometry pass claim.

## Diagnostic capture

Root attached Time Profiler for20seconds. Actual trace23:06:37.683–23:06:59.826UTC,
record/export/parser exit0,59393 CPU rows. DWARF analysis warnings were emitted;
symbolized frames remain present, with some address-only/deduplicated symbols.
Main Running19901samples represent19.901sampled CPU seconds over21.297seconds
between its first/last samples. Main sun-shadow ancestry8.997s, including native
shadow renderer3.907s; these are inclusive/overlapping. Ordinary probe1.410s,
strict hero0 in this window. No whole-frame GPU duration or formal FPS measured.
Running-only samples do not classify all remaining wall time as GPU waits.
This shows substantial CPU work during the capture, not proof of the reported
input symptom or a newly introduced avatar-animation defect.

Native FPS/UI publication can advance over retained pre-UI world imagery; these
counters alone do not prove fresh world motion. Existing logs show some retained
world selection but cannot quantify presentation age per frame. Do not infer
all-world freeze or shader correctness from either a high FPS label or quiet logs.

## Separate logged correctness concerns

Before this trace,1477 expected-cloud-layer warnings ended23:05:52. They set sky
availability false, not capture.complete=false, and alone do not prove world
publication aborted.16 genuine bad inverse-bind XYZ diagnostic rows and2 palette
fallback rows occurred23:01:12. Inverse XYZ was nonfinite; world matrix was finite.
Original parser/lifetime code matches1eb; malformed asset versus later overwrite
is unproved. No speculative clamps, geometry omissions or new palette guard.

Strict trace interior23:06:38–58 contains zero cloud/palette/terrain/G-buffer
failure signatures, three selected warnings (HTTP/circuit), and retained-UI
receipts. Older cloud/bad-inverse events are not temporally tied to this capture.
Rows are log receipts, not frame counts; repeated once-logging is coalesced.
Remote networking/input delivery and actual world-image age remain unmeasured.

Private exact commands/exits/trace/XML/CPU/log summaries:
`artifacts/metal/private-68643-busy-crowd-20261110-01/`.
preflight.json pins app/code/PID; owner-update.json preserves reported resolution.
No raw account/location/capture data uploaded. Current choppiness incident is
resolved by owner report; separate warnings remain unclosed correctness evidence.

## Follow-up: left/right turning remains under investigation

Owner subsequently clarified a distinct symptom: LEFT/RIGHT ARROW turning makes
name tags move with the expected turn, while self body appears delayed then
catches up. This reopens movement correctness despite the earlier general
responsiveness report. Owner also reports roughly76avatars and that previous
geometry corruption appears fixed; this is positive live shader-repair evidence,
not full reference/candidate parity. CUA diagnostic observation of exact68643
shows a crowded scene/78 nearby count; own body is partly obscured. No agent
movement/camera/settings input or controlled turn comparison performed.

Seven original self orientation/root/update functions match1eb. Self still uses
period1/full skeletal update; classic palettes rebuilt per mesh, rigged cache
keyed by frame. Original body yaw smoothing intentionally permits some lag.
That does not establish the reported effect is normal. Name tags use current
camera/pose while retained pre-UI world can contain an older body/camera image;
current source_frame evidence is required before attributing this turn to that
path. Missing native impostor quartet can invalidate all world batches, unlike
original per-avatar readiness handling, but no such event was logged in this
turn window. No speculative smoothing/cache/geometry-gate change made.

Runloop export analysis completed:929 main calls over21.2451s, intercall p95
54.406ms, maximum447.714ms, seven gaps>100ms and none>500ms. Continued runloop
service does not prove input delivery; gaps lie outside the OS runloop call and
are not classified as GPU waits. Logs lack per-frame retained-world age.

One identity-pinned LLDB snapshot executed, with only process memory reads,
no expressions/JIT/breakpoints/inferior calls/writes. Exact loaded module UUID
verified; snapshot_ok and detach_ok true. Root independently confirmed PID48817
running afterward. Attachment through detach took21.717s; stopped data reads
took1.985ms. This debugger pause is diagnostic interference and must not be
reported as a spontaneous viewer hang or normal frame-time result.

At that instant: gFrameCount24524, presentation_frame24524, retained source24524,
last submitted UI24523, world_ui_frame invalid, SSRwarmup0, failed frames0.
A current rendered world backing exists. The last submitted UI's world source
is not preserved separately in these fields, and the UI result may still be in
progress; this is not proof of the actual visible drawable's age. It does NOT
establish retained-world lag during the owner's earlier bad turn, nor rule out
intermittent stale-world selection. No controlled turn happened at the
snapshot. Do not repeat disruptive attaches for continuous sampling; a bounded
frame/pose census in a future diagnostic build is preferable if reproduced.
Private frame-age-snapshot.json, frame-age.json/log and source-audits pin exact
values/script/layout. No movement/camera setting or renderer policy changed.

Final pose-cache audit found no pointer-only reuse: frame-data palette/constants
dedup hashes content and verifies length/bytes; generations clear entries and
completion-pin pages. G-buffer constant reuse compares all400bytes including
MVP/model-view within one render. Changing yaw bytes produces fresh data. This
negative source result does not close the intermittent owner-reported turn lag.
