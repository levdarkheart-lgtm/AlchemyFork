# Native post-history Snapshot checkpoint — 2026-11-01

This append-only record preserves the evidence compacted out of
`docs/metal/state/CURRENT.md` after renderer checkpoint `f9cc59d963`.

## Defect and implementation

A controlled nondefault post run used SMAA quality 3 with predication, dynamic
exposure and lens flare at 2048x1476. The native Snapshot gate correctly failed
closed with `lens_flare=false`. Source tracing and temporary runtime census
showed two lifecycle defects rather than a shader-math error:

- the main-world Metal renderer warms asynchronously after `STATE_STARTED`,
  while the already-enabled GL temporal flare can advance beyond the only
  exact no-readback bootstrap value (black); and
- the first reflection/hero capture swaps in an independent capture owner but
  its device initialization cleared the main-world exposure/flare readiness.
  Auxiliary render-finalize passes also advanced the global GL flare history
  even though color correction applies that flare only to the main world.

Commit `f9cc59d963` restarts the main-world GL and Metal flare histories together
at the established safe clear boundary when asynchronous renderer warm-up
finishes. Independent probe-owner initialization no longer invalidates the
main owner's temporal readiness, and non-main passes now hold the global flare
history. Missing or divergent history still fails closed; the change does not
make native presentation authoritative or suppress GL traversal.

## Controlled runtime evidence

Actual Computer Use authenticated an isolated build-68018 session, waited for
both main-world and probe warm-up, opened Snapshot, selected Disk, refreshed
and saved a PNG. The accepted log contains `Native color snapshot readback
completed at 2048x1476` and no GL-readback fallback marker. UI state confirmed
PNG, colors, 2048x1476, NoFilter, post-processing enabled and HUD/interface
disabled. Confirmed Quit ended with `Goodbye!` and `status: stopped`; the final
viewer/media process census was empty.

The candidate PNG remained outside the repository because it contains
authenticated-world imagery. Its SHA-256 is
`d3b3d3c7e09a2a5fdedbece91e0afb74b15151349e9400b3fda09ae31a777cdf`.
Preflight and post-run manifests each matched all 416 effective visual settings
against the reference, with zero mismatches, missing settings or unilateral
settings.

FFmpeg reported SSIM `0.588572` and PSNR `15.294873 dB` against the earlier GL
reference. Those values are descriptive only and are not parity evidence: the
reference and candidate used different source checkpoints and were captured
about 25 minutes apart while region lighting and avatar pose visibly changed.
The settings invariant passed, but the scene/time invariant did not.

Discarded attempts are not evidence: one malformed command placed two
name/value pairs after one `--set` and the viewer parser rejected it; one cloned
profile failed the automated preflight with 44 visual mismatches; and two
diagnostic Snapshot runs retained GL while the lifecycle defects were being
isolated.

## Build and regression evidence

- Warnings-as-errors incremental RelWithDebInfo build 68018 linked, staged and
  signed the exact app.
- Executable SHA-256:
  `0df89b25c134eea1199b61c7255121971aae98cf37ec595edf16c855aa2098f7`.
- Deep strict signing passed; `verify_app.sh` inspected 349/349 ARM64 slices.
- Complete physical Metal validation passed 44,871 assertions across 50 cases
  with Metal API and load-action validation enabled.
- The architecture-neutral gate passed 484/484 tests in 245.95 seconds.

This is bounded M-008 progress. Broader post/alpha/PBR/probe parity, the full
feature and stability matrices, strict no-GL, independent review and qualifying
M1 Max performance evidence remain open.
