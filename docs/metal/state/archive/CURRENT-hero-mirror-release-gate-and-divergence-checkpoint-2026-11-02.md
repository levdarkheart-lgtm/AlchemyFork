# Development mirror gate and current divergence checkpoint — 2026-11-02

This archive corrects the classification of the preceding authored-mirror
attempt, makes the scoped Development diagnostic reachable in the normal
optimized build, and records a source-exact current native image. It does not
change public Release behavior or justify native authored-mirror authority.

## Correction to the preceding timeout record

The pre-signal sample from candidate `26.4.0.68089` is retained at
`/private/tmp/alchemy-m009-hero-mirror-ui-timeout.sample.txt`. All 40 sampled
main-thread stacks were active in frame work, including
`LLPipeline::generateSunShadow`, direct Metal spot-shadow encoding,
`LLPipeline::renderGeomDeferred`, native world rendering, texture upload and
deferred lighting. The log continued at roughly 5--7 FPS under full Metal API,
shader and attachment-action validation. That evidence does not support a
renderer deadlock.

The sampled PID subsequently received `SIGTERM` and reached `Goodbye!` plus
`status: stopped` in about one second. The earlier watchdog shutdown failure did
not reproduce. The bounded classification is therefore a semantic observation
timeout while the validation-heavy renderer remained active, not an established
render-loop or shutdown hang.

## Reachability defect and bounded correction

The actual reason the requested native Snapshot path did not run was a build-
configuration gate. `ALMetalHeroMirrorSnapshotValidation=true` was accepted,
but the packaged Development application is intentionally compiled with the
optimized Release configuration, where `LL_RELEASE_FOR_DOWNLOAD=1`. The helper
therefore returned false before considering the saved setting.

Commit `38b89cfd58d93bc6ec7d0841a8cdf67b705926dd` gates this diagnostic on the
immutable compiled viewer channel instead. A channel beginning `Alchemy
Release` remains ineligible; a command-line channel override cannot spoof the
compiled value. This changes diagnostic reachability only. It does not change
mirror rendering, normal live presentation, public Release behavior or the
fail-closed authored-mirror guard.

## Source-exact build and host gates

On the physical Apple M1 Max host, `tools/metal/doctor.sh` passed with macOS
26.5.2, 64 GiB unified memory, Xcode 26.6, an available Metal compiler and
Apple7-family support. The exact wrapper command was:

```sh
AL_METAL_PRESET=ninja-os-arm64 \
AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 \
AL_METAL_CONFIG=Release \
AL_METAL_JOBS=8 \
tools/metal/build.sh build
```

It produced signed Development build `26.4.0d68093` (`26.4.0.68093`) with the
full source revision embedded. Deep strict signing passed and
`tools/metal/verify_app.sh` verified 349/349 Mach-O slices as ARM64. The main
executable SHA-256 is
`144f256b4d87a35825ac0029f751bf5056ab5af9c1ba8c7cfd4c80fe2a3d00ce`.
Packaged MSL remains
`73955209f611038a5e4fe0305642614fefee30766dcd26fd03b7df7d3933df20`;
the compiled metallib remains
`fd64dfedceb9056e85090916b7e0dab2d8e17c553cc4ffe63ea1b5e67647c297`.

The neutral Release gate:

```sh
ctest --test-dir build-Darwin-ninja-os-arm64 -C Release \
  -LE 'manual|gl' --output-on-failure -j 8
```

passed 484/484 tests in 35.57 seconds.

## Current admissible diagnostic pair

Exact-path preflight passed against pinned reference `26.4.0.67607`, executable
SHA-256
`fac5225435df2c096d10b856f6e0f186b37e41446576398ee8f4842b884ef28d`,
and source-exact candidate `26.4.0.68093`. The isolated profiles are:

- `/private/tmp/alchemy-mirror-reference.5uT356`
- `/private/tmp/alchemy-mirror-candidate.buhQH0`

Their captured graphics manifests compare all 416 visual settings with zero
mismatches, one-sided values or missing values. Both sessions used the same
authored-mirror location, quality 3, mirrors and reflections enabled, probe
level/detail 2, fixed exposure, `RenderShadowDetail=2`, and 256-square hero
probes. This inherits the standing shadow rule: any shadow-parity claim also
requires enabling and saving shadows in Graphics Preferences in both viewers,
then restarting both viewers.

The reference control is
`/private/tmp/alchemy-m009-mirror-reference-shared-256.png`, 2048x1476,
SHA-256
`b5721310ac7a0cb8a80abc0e5192bf70bf246e7f6454495a03944c33f1927a88`.

The candidate reached `STATE_STARTED`, published hero radiance from six complete
mirror-clipped native Metal faces at generation 149 and max LOD 7, logged the
Development-only authored-mirror Snapshot route, and completed native color
readback at 2048x1476. The captured file is
`/private/tmp/alchemy-metal-67956-depth24_2026-11-02_06512301.png`, SHA-256
`8e62b12f97af70c3a0f077d5c10b1ce1aad6c2396979c011266263c8e9eaea92`.
The candidate then exited cleanly with `Goodbye!` and `status: stopped`.

## Measured and visible result

The current pair measures:

- whole-frame raw SSIM `0.855629`, average PSNR `19.210972 dB`;
- 1024x738 sigma-8 structural SSIM `0.938252`;
- fixed door/wall crop `(700,120,780,560)`: SSIM `0.846400`, PSNR
  `21.528820 dB`;
- mirror-interior crop `(380,790,110,250)`: SSIM `0.888406`, PSNR
  `18.393056 dB`.

Visual inspection identifies a concrete forward/material loss: patterned or
stained-glass content on the central and right doors becomes gray translucent
or blurred rectangles, and wall media/art plus nearby forward content is
missing. The vanity mirror/reflection also differs. These are visible route
divergences, not metric-only noise.

## Decision and next boundary

Keep normal authored-mirror native presentation fail-closed. The source-exact
current pair invalidates any proposal to open the guard from the older
one-fixture 512-square result. Isolate the missing forward categories and their
ordered replay from current capture/source, beginning with the door glass and
wall media, then repeat the same matched diagnostic before changing authority.

Exact RLVa correctness remains deferred non-release nice-to-have M-013; only
minimum support needed for Metal/no-GL cutover is mandatory. Any exact RLVa
comparison must enable RLV in Preferences in both viewers, save and restart
both. Strict viewer-owned OpenGL elimination remains independently open.
