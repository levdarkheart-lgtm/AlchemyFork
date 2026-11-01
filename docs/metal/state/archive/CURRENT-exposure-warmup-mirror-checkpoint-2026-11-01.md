# Exposure warm-up and authored-mirror replay checkpoint — 2026-11-01

This append-only record preserves the detailed evidence compacted out of
`docs/metal/state/CURRENT.md` after renderer checkpoint `d3cff63d0d`.

## Observed defect and implementation

A fresh-profile authored-mirror candidate run reached `STATE_STARTED`, then
completed asynchronous main-world Metal renderer warm-up only after GL had
already adapted dynamic exposure. The Snapshot gate correctly rejected the
native transaction with `dynamic_exposure=false`: Metal had no exact previous
EV and could not safely join the already-advanced GL sequence. The scoped
authored-mirror diagnostic itself was active; no image was accepted from that
run. Its post-run graphics manifest still matched the reference 416/416.

Source tracing found a startup-ordering defect, not exposure shader math.
Commit `d3cff63d0d` marks a pending GL reset when the main-world renderer
finishes warming after non-fixed exposure. It advances the native history epoch,
invalidates the old Metal target and makes the next native update start from
fixed EV 0. Because the completion callback may run while another GL target is
bound, it does not bind an FBO there. `generateExposure()` clears the current
exposure target at the established pass boundary before the normal swap makes
that EV-0 texture the previous frame for GL. Metal and GL then advance from the
same boundary in the same frame. Resource release and ordinary history reset
clear the pending flag.

The physical exposure regression first adapts a bright frame from an existing
history, then repeats it with no previous texture and proves the restarted
result differs asymmetrically and matches the fixed-EV response. No presentation
gate, development-only waiver or GL authority rule was relaxed.

## Build and automated validation

- `tools/metal/doctor.sh` passed on macOS 26.5.2, Apple M1 Max, Xcode 26.6 and
  the available Metal compiler.
- The focused exposure case passed 63 assertions under Metal API/load-action
  validation.
- The complete physical Metal suite passed 44,875 assertions across 50 cases.
- Warnings-as-errors RelWithDebInfo Ninja build 68020 linked, staged and signed.
- Build 68020 executable SHA-256 is
  `42717a577f42cfd5e553d8e09e4ece86dc88ad4e283aa49eedfb24d4cbe71763`.
- Deep strict signing passed and `tools/metal/verify_app.sh` inspected 349/349
  ARM64 Mach-O slices with zero invalid slices.
- The architecture-neutral gate passed 484/484 tests in 240.06 seconds.

## Actual Computer Use acceptance

Preflight compared the isolated candidate and reference profiles before login:
all 416 effective visual settings matched, with zero missing, mismatched or
one-sided settings. Exact-path preflight then attested candidate 68020, its
hash, singleton PID and isolated profile.

Actual Computer Use logged in through the real viewer UI, reached the requested
authored-mirror fixture, selected fixed Midday and retained the persisted camera.
The log recorded `STATE_STARTED` and
`Native main-world renderer warm-up restarted GL and Metal exposure history
from their fixed-EV boundary`. Snapshot UI state was Disk, PNG, colors,
2048x1476, NoFilter, post enabled, cutout/HUD/interface disabled. Refresh logged
the development-only authored-mirror admission and native 2048x1476 color
readback; no GL-readback fallback marker appeared. The saved candidate is
`/private/tmp/alchemy-mirror-candidate-68020.png`, 4,196,657 bytes, SHA-256
`ef84432dae05b48cc04f0762639c98ce53e7ba26a71ee47d08a5fc4cb8e56078`.

After confirmed semantic logout and complete process drain, pinned ARM64
OpenGL reference 67607 ran sequentially with the same SLURL, camera, Midday,
window and Snapshot contract. Its executable SHA-256 is
`fac5225435df2c096d10b856f6e0f186b37e41446576398ee8f4842b884ef28d`.
The saved reference is `/private/tmp/alchemy-mirror-reference-67607.png`,
4,015,981 bytes, SHA-256
`4d218fd4bcf7abf63c526bcde72babc1e81640c341c27512c35237a6c8edbe71`.
Both 2048x1476 PNGs were visually inspected. Both semantic exits reached
`Goodbye!` and `status: stopped`; the final viewer/media census was empty.

Post-run manifests again matched all 416 effective visual settings. The exact
binaries differ by design, so the record pins both source artifacts rather than
calling them source-identical. The sequential avatar idle pose changed, making
whole-frame SSIM `0.933043` / PSNR `23.364 dB` descriptive only. A stable
780x560 door/wall crop at `(700,120)` reached SSIM `0.968123` / PSNR
`35.375 dB`. The 110x250 mirror-interior crop at `(380,790)` reached SSIM
`0.985163` / PSNR `34.480 dB`; direct and side-by-side inspection found no
material mirror-shape, orientation or sampling residual for this fixture.

Authenticated-world images, logs and manifests remain outside the repository.
This accepts the repaired lifecycle and one mirror fixture only. Broader
post/alpha/PBR/probe/advanced-view parity, stability, strict no-GL, independent
review and qualifying M1 Max performance evidence remain open.
