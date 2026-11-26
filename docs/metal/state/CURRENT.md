# Current engineering state — experimental native Aurora prototype

Observed 2026-11-26 on Apple M1 Max /64 GiB /macOS26.5.2 /Xcode26.6.
Branch `codex/aurora-native-temporal-20261126` starts at evaluation checkpoint
`318f0ac8b6a8073fe8d22d0848c84c282ad8b212`. The owner authorizes implementation,
offline qualification, feature-branch push and private iCloud delivery. No live
login or whole-viewer FPS claim. Native remains default; combined lighting OFF.

## Active implementation and qualification

**Renderer code checkpoint:** `b6f18c24f4e32e5e8897f782c0e4b65bda521f8f` — experimental build69516 source.
The [native report](../research/aurora-native-temporal-20261126.md) owns the source
map, qualification and delivery. New saved ID8 /effective4 selects exact2/3 Aurora
OFF. IDs0–7, output-sized shadows and native UI remain unchanged. The component
uses existing motion/poses, jitter, main-post transactions and retirement. Genuine
opaque/final HDR drives upstream reactivity; the final resolve preserves glow.
No runtime Rust/wgpu, new history scheduler or normal-frame CPU wait is added.

PPD610 stays open: the owner’s palm/grass scene has not been reviewed with this
prototype. All original67/75 and Aurora quality failures remain. The native port
matches **195,084,288 RGB components across468main+414mature worlds exactly**.
Edge variation.0008673, contrast.820113, recall.936028; area.821401<.85 and stop18/36
errors.0158863/.0123194>1/255 still fail. Matched previews use fixture worlds at12fps,
not live Home footage or viewer throughput.

PPD614’s initial port mismatch is corrected: Naga renamed two pyramid entry points;
wrong names yielded accepted vertex-only pipelines. Generated reflection and
mandatory vertex/fragment validation now prevent silent clear-only stages. Failed
outputs and language-version countercheck remain retained. No quality bound changed.

Actual-size native component:2304×1446→3456×2168,360warm/360samples, GPU median/p95
**10.925625/12.392208ms**, including opaque copy, adapters, complete graph, depth and
alpha resolve. CPU encode.294167/.720458ms; submit.011084/.022000ms. AC power, nominal
thermal state, identical18input fingerprints. Owned allocations and physical device
footprint plateau. The [earlier evaluation](../research/aurora-reconstruction-evaluation-20261126.md)
measured ~11ms WGSL and ~4.14ms MetalFX; this is not a matched native speedup or FPS result.

Twelve component cases pass API/GPU validation. Two actual renderer capture/borrowed
transaction cases pass204assertions, including bloom, pre-display rejection,
unsubmitted cancellation, retained presentation and recovery. Glow-edge error
.000813611<.002; owned payload returns to zero at teardown.198tool tests pass with
one existing skip; seven translation/analysis checks pass.

## Current executable action

Release 26.4.0.69516 is complete at application source b6f18c24f4. All three CPU
and fifteen affected Metal suites pass, including the PPD616 test-only oracle
correction. PPD615 retains the incremental-cache issue; it does not block the
verified full build. Deep/strict ad-hoc signing and 346 ARM64 checks pass.
The full-screen login-only smoke reaches STATE_LOGIN_WAIT and closes normally.
PPD617 limits its evidence to shipped defaults/session overrides after the private
launcher’s cloned-settings path failed; actual owner XML matches retained baseline.
No account login, permanent graphics change or live rendering claim is made.
Mounted-package verification is complete: all 8293 app entries match; signatures,
346 ARM64 checks, ZIP CRC and selected hashes pass. All three iCloud Desktop files
are confirmed uploaded at18:16:49UTC, with no error and matching destination
hashes. PPD618 preserves the transient initial account-access error and recovery.

Build directory: `build-aurora-native-release`; OpenHTJ2K0.27 is the sole pixel decoder.
Private full outputs/failed attempts/receipts: `artifacts/metal/aurora-native-20261126/`.
Compact synthetic evidence is linked from the native report. One Ninja owner at a
time includes metadata commands; dependency-log recovery remains a separate follow-up.
DMG, review ZIP and checksum file are ready on iCloud Desktop. The new feature
branch is published; only final delivery documentation follows application b6f.

## Preserved branches and previous repairs

- Evaluation remains `codex/aurora-reconstruction-evaluation-20261126` at318f0ac8b6.
- Temporal remains `codex/metalfx-temporal-20261125` at86a7654fb1. The69507 application
  source was b73abe1acd; later tests/docs differ. The [Temporal report](../research/metalfx-temporal-20261125.md)
  and [69507 review](../research/temporal-foliage-69507-review.md) retain corrections,
  all original20 failed quality bounds and private delivery identities.
- Spatial remains `codex/metalfx-spatial-20261124` at41e90aaadb. Build69464 source was
  a5142ae5c2. Its [quality review](../research/metalfx-quality-20261124.md) retains
  historical live results; those are not measurements of this new executable.
- The usual branch and all existing viewer packages remain unchanged.

Submission-only pose history, semantic material equality, blended foreground depth/
motion, visibility-aware rejection, sky motion, cancellation and pooled-target
pre-acquisition are preserved. MetalFX wrapper/pool/history/resolve isolation is
[closed for the retained case](../research/temporal-palm-native-isolation-20261126.md);
do not reopen it without a new demonstrated integration defect.

PPD591–600 retain Spatial sizing, mip, preset, fallback and native snapshot decisions.
The existing snapshot route is independent and does not represent displayed Temporal
quality; owner review must use macOS screenshots/recordings. PPD608/609 retain the
allocation/cancellation repairs. Transparency/water/sky/content-animation limitations
remain explicit; no rejection, bias or sharpening tuning is part of this prototype.

## Complete-product advancement assessment

This prototype advances owner-reviewable reconstruction quality, not release
acceptance. PPD610, PPD319 Home performance, broad feature parity, prolonged
stability and third-party/release qualification remain open. The owner will review
actual stationary palms/grass, moving avatars, camera stop/settling, transparency
and whole-viewer performance. Native, Spatial and MetalFX remain comparison options.

| ID | State | Remaining boundary |
| --- | --- | --- |
| M-008 | doing | Broad feature/visual acceptance and representative performance. |
| M-011 | doing | Reproducible release and strict no-GL qualification. |
| M-012 | doing | Maintained tools and meaningful automated/offline evidence. |
| M-013 | doing | Retained features and prolonged stability. |

Required independent build/review evidence and release gates remain open.

## Exact next actions

Owner launches build69516 and selects 67% (Aurora Temporal · Experimental), checking
the effective Active status. Review stationary palms/grass, move-stop response and
whole-viewer performance; use macOS screen capture, not the snapshot route.
No implementation, offline qualification or upload work remains for this handoff.
Live acceptance remains unclaimed; no broader profiling is requested.
