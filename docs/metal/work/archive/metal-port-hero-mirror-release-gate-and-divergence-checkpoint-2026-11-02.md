# Metal port execution — Development mirror gate and current divergence — 2026-11-02

## Corrected diagnosis

The retained pre-signal sample from build 68089 contains 40/40 active main-
thread renderer stacks across directional/projector shadows, native world
encoding, resource upload and deferred lighting. The validation-heavy viewer
continued at roughly 5--7 FPS and later completed `SIGTERM` shutdown in about
one second. The preceding semantic observation timeout is not an established
renderer deadlock, and its watchdog shutdown failure did not reproduce.

Source inspection exposed the real diagnostic blocker: the signed Development
app uses the Release configuration, so `LL_RELEASE_FOR_DOWNLOAD=1` compiled
`ALMetalHeroMirrorSnapshotValidation` to hard false. Commit
`38b89cfd58d93bc6ec7d0841a8cdf67b705926dd` now admits the setting only when
the immutable compiled channel is not `Alchemy Release...`. Public Release,
normal live presentation and the authored-mirror guard are unchanged.

## Build, test and runtime evidence

The exact Release wrapper build produced Development 68093 at the committed
source SHA. Doctor passed on the physical M1 Max. Deep strict signing passed,
349/349 Mach-O slices verified ARM64, and the executable SHA-256 is
`144f256b4d87a35825ac0029f751bf5056ab5af9c1ba8c7cfd4c80fe2a3d00ce`.
The non-manual/non-GL Release CTest gate passed 484/484 in 35.57 seconds.

Exact-path candidate/reference preflight passed. The two isolated graphics
manifests matched all 416 captured visual settings. Reference 67607 and
candidate 68093 both used the authored-mirror fixture, fixed exposure,
quality 3, probe level/detail 2, 256-square hero probes and
`RenderShadowDetail=2`.

The candidate published native radiance from all six mirror-clipped faces,
entered the scoped Development Snapshot diagnostic and completed 2048x1476
native readback. Its image SHA-256 is
`8e62b12f97af70c3a0f077d5c10b1ce1aad6c2396979c011266263c8e9eaea92`;
the reference SHA-256 is
`b5721310ac7a0cb8a80abc0e5192bf70bf246e7f6454495a03944c33f1927a88`.
The viewer then shut down cleanly.

Whole-frame raw SSIM is `0.855629`, average PSNR is `19.210972 dB`, and
half-size sigma-8 structural SSIM is `0.938252`. The fixed door/wall crop is
SSIM `0.846400` / PSNR `21.528820 dB`; the mirror-interior crop is SSIM
`0.888406` / PSNR `18.393056 dB`.

## Containment and next executable action

The current Metal image visibly drops patterned/stained door glass, wall
media/art and other forward content, replacing some surfaces with gray
translucent or blurred rectangles. The vanity reflection also differs. Keep
the production guard closed.

Trace capture and replay ownership for the missing door/wall forward categories
from current source, implement only the bounded missing owner, and repeat the
same matched diagnostic. Continue strict GL classification independently. Any
shadow comparison remains valid only after Graphics Preferences enable/save in
both viewers and restarting both viewers. Exact RLVa remains deferred M-013
with its separate Preferences/save/restart rule.
