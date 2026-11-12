# PPD-008 — black world during login with visible UI

Owner report2026-11-12 during the first adaptive68880 destination test: the
world area was completely dark/black for approximately5–10seconds while the
UI remained visible, then the world appeared and continued loading. The owner
recalls this once before, with no established cause. Timing is approximate;
this does not establish a new adaptive-decoder regression.

Retain this as PPD-008, separate from busy-turn PPD-002 and source-confirmed
projector PPD-001. No rendering fix is authorized by a source assumption.
Investigate with the original-viewer reference workflow before any repair.

Candidate source98f41e652beb6150d9012cd25af102bd6c5e76df/build68880, PID36734,
SHA256243b5787b6b3d9789953a34b58e6b58615c859861c41c29dfe19c1400b5cd3d4.
Private evidence is the first180.840185s recording under
`artifacts/metal/adaptive-decoders-20261112/runtime-180348/new-destination/`.
Capture18:05:45.035–18:08:45.876UTC includes login/world entry; initial movement
is+18.965s and STATE_STARTED+25.965s. The owner's black interval has not been
time-aligned to a specific frame or these events. Per-frame Overview tables
are empty; aggregate presentation cadence cannot prove pixel content.

## Original mapping and observed native interval

Read-only comparison against original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45
uses eight successful Git source reads; exact argv/hashes are retained in
`runtime-180348/black-world-source-commands.json`. Candidate renderer/startup/
progress/background files byte-match98f41e652b. No original viewer was launched.

Both displays begin ordinary world traversal at STATE_PRECACHE (original
llviewerdisplay.cpp628, candidate1679). Native pipeline.cpp2130 additionally
withholds G-buffer capture and renderer preparation until STATE_STARTED;
canCaptureMetalWorldPresentation also requires STARTED. The guard deliberately
protects partial startup water/alpha/environment ordering.

pipeline.cpp2678–2809 posts renderer creation to General and its completion
back to mainloop. WorkQueue::postTo is asynchronous, not waitForResult. When
there is no real pre-UI world, pipeline.cpp10969–10983 uses
ALMetalRetainedBackground::makeCleared; almetalretainedbackground.mm249–286
clears RGBA(0,0,0,1). Original/current llstartup calls setStartupComplete before
STATE_STARTED; normal no-intro llprogressview behavior fades its curtain in1s,
with no native-world-readiness predicate.

| Capture event | UTC / trace seconds |
| --- | --- |
| Login cleanup |18:05:57 /+11.965 |
| Initial world movement |18:06:04 /+18.965 |
| Enter PRECache, continuous native UI messages |18:06:05 /+19.965 |
| STARTED and asynchronous renderer warm start |18:06:11 /+25.965 |
| Continued UI submissions during warm-up |18:06:12–17 /+26.965–31.965 |
| Warm complete, measured6.60176s; first logged final BGRA8 world available |18:06:18 /+32.965 |

This is a source-confirmed readiness difference and a strong timing match to
the owner's5–10s report. Actual curtain visibility, exact black pixels and the
first world present on the display remain unobserved. UI publication flags can
include the cleared loading lease and are not sufficient real-world readiness
proof. Zero-Overview buckets0–9.529s occur before login cleanup and do not
establish this interval. PPD-003 diagnostic suppression remains relevant; absence
of warnings does not prove every world transaction succeeded.

Next: decouple early asynchronous renderer/PSO prewarm from scene-capture
admission, retaining device/owner generation and STARTED ordering checks; then
measure curtain visibility and first coherent native-world source/present on
one clock. A readiness-qualified curtain is another boundary to assess. Do not
simply remove the partial-startup capture guard. No rendering repair, pixel
reproduction or owner verification is claimed; PPD-008 remains investigating.
