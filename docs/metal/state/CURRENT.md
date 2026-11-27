# Current engineering state — experimental native Aurora prototype

Observed 2026-11-27 on Apple M1 Max /64 GiB /macOS26.5.2 /Xcode26.6.
Branch `codex/aurora-native-temporal-20261126` starts at evaluation checkpoint
`318f0ac8b6a8073fe8d22d0848c84c282ad8b212`. The owner authorizes implementation,
offline qualification, feature-branch push and private iCloud delivery. The later
owner-authorized targeted scene login is recorded below; no whole-viewer FPS claim.
Native remains default; combined lighting OFF.

## Active implementation and qualification

**Active correction sourcef971 /69531:**11water cases pass22,609,127assertions
with API/GPU validation. Exact saved-view09joins64worlds, noROI/observerdrops;
all2333wrong-side rays reject before tracing, no68796accepted hit is coplanar
with receiver water.69530remainsWITHHELD. PPD630ready_for_owner; PPD624open:
B/Cfinal brightness range improves65/75%, A worsens9.57%; mean/detail tradeoffs
and37→49probe confound remain explicit. No all-pulse/FPS claim. Release build,21affected
API/GPU modules,346ARM64/signatures and full-screen login-onlysmoke pass. Mounted8293entries,50ZIPfiles and allthreeiCloud uploads are verified
22:22:18UTC;69531is available for owner review, not a complete water-qualityfix. Original
settings restored; preserve accepted tiles/glass/foliage and PPD629 follow-up.


**Preserved69527 receiver-seed investigation (2026-11-27):** start4d8e, app0a94/69525.
Per-ray inspection is HDR/native/presented-exact for108stationary fixtureworlds:
refracted-UV hashing redirects rays15.7–25.3degrees while normal changes.40degrees,
changing hit coverage and sampled reflector colors. No acceptedwaterfeedbackhit.
Receiver-screen-UV seeding retains the spatial distribution/roughness and lowers
final temporalRMS .007548→.002939 (61.06%), mean within.75% of common32ray reference.
Residual static4raygrain/error remains (referenceMAE .004886→.007997); noexact-quality
claim. Constantglobalpattern rejected forbrightnessbias; coherentfrozenhistorydoes
notremove originalcycle. Aurora/rejection/mips/normalanimation unchanged.
New342world regression keepsanimationthroughmovestop and skipsonepublication,
proving retainedpacketidentity. Original69525shader fails1frozenqualitybound;
candidate2cases/113240assertions API/GPU pass; oldwaternoisebounds unchanged.
Source5d44/69527 Release and24selectedchecks (20MetalAPI/GPU+4CPU) pass.
Full-screen login-only smoke restores18settingsfiles/24entries, viewerclosed.
Mounted8293entries/signatures/346ARM64 and ZIP127hashes pass; all threecloud
uploads confirmed08:01:28UTC withnoerror/matchinghashes. Initial4355cleared
withoutaccount/servicechanges. Owner originalscenequality review remainsnext. No productionnewpass/ray/
texture/wait. Tileminimum4 and owneracceptedglass/foliage are preserved.
Actual-viewtilecost isunqualified: ROIcaptures perturbcost, A0quietinvalid, no live
sessionavailable. Keep +2.438ms syntheticcost separate; no liveFPSclaim.

**Renderer code checkpoint:** `f971d11de29daf60233f9f5875cf957ca069e95c` —69531water receiver-validity candidate; Release,21API/GPU modules, login-onlysmoke, mountedpackage and allthreeiCloud uploads pass22:22:18UTC. Preserved69527/source5d44,69525/source0a94 and69523/sourcec72fafcd3b remain unchanged.
Preserved glass69520 source `2af7ebba82b6bfd15886069f1eb9613f34fcef18` remains owner-accepted.
Preserved69516 source remains `b6f18c24f4e32e5e8897f782c0e4b65bda521f8f`.
The [native report](../research/aurora-native-temporal-20261126.md) records the preserved69516 port;
the glass report owns corrected69520 qualification and delivery. New saved ID8 /effective4 selects exact2/3 Aurora
OFF. IDs0–7, output-sized shadows and native UI remain unchanged. The component
uses existing motion/poses, jitter, main-post transactions and retirement. Genuine
opaque/final HDR drives upstream reactivity; the final resolve preserves glow.
No runtime Rust/wgpu, new history scheduler or normal-frame CPU wait is added.

PPD610 stays open: the owner reports substantial live foliage improvement with
69516, and separately reports glass instability tracked by PPD619. This is useful
live feedback, not acceptance of all quality bounds. All original67/75 and Aurora
quality failures remain. The native port
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

## Preserved69516 delivery

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

Owner reviews69531in the originalMidday water view: residualpatchAfluctuation,
brightness/reflecteddetail, movement–stop and acceptedglass/foliage/tile controls.
PPD624remainsopen; do not treat passed rayvalidity as complete visible acceptance.
PPD630ready_for_owner; PPD629runtimeMSLdialect and PPD615buildcache remain
actionable follow-ups. No more livecapture/generalFPS work is authorized by this
bounded repair. Preserve69530WITHHELD and priorpackages. Actual-view cost remains
unqualified, with measured depth-fetch work+25.1%, not GPUmilliseconds.
