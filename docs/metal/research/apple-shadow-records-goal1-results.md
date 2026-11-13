# Goal one result — reusable native attachment poses

**Runtime update, 2026-11-13:** owner reports G1 died shortly after busy-club
arrival. Runtime acceptance has failed. [PPD-090 investigation](../work/active/g1-arrival-crash.md)
tracks the ImageDecode exception/rethrow in the existing log; underlying cause
and relationship to shadow records remain unknown. Headless results below are
retained as historical evidence. Goal two remains stopped.

**Engineering milestone complete; headless mechanism gate passed. Owner viewer
assessment pending. Goal two has not started.** This result supports the pose
ownership seam; it does not establish better in-world frame time or validate
the rest of the [Apple-first implementation route](apple-first-implementation-route.md).

Branch: `codex/apple-shadow-records-g1`. Unchanged source baseline:
`18e576cec0a05923632188ec8d7b2224657ffbfa`. Implemented/tested app source:
`e21fe9c21832e5af88971ae0cb4f1ec2d892c36e`. The accepted Alpha6 renderer,
applications and build directories were preserved. No viewer/utility launch,
login, screenshot, interactive profiler, push or publication occurred.

## What is implemented in the viewer

- `LLVOAvatar::MatrixPaletteCache::getMetalAttachmentPalette()` lazily seals
  the current packed attachment pose into owned immutable bytes. The existing
  once-per-frame producer clears that handle when it rebuilds. Failed creation
  is cached only for that revision; already captured records remain valid.
- `captureMetalSpotShadowOpaqueBatch`, `captureMetalSpotShadowTexturedBatch`
  and `captureMetalSpotShadowPBRBatch` pass this record into actual shadow
  submission. This is enabled in the delivered app, not a test-only path.
- `ALMetalAttachmentPaletteRecord` validates joint limits, packed shape and
  finite values once. `renderSpotShadow` still checks record/raw consistency,
  geometry, weights, coverage and other admission rules. Body/cloth palettes
  retain their existing view-dependent path. Shader math and draw order stay.
- `ALMetalWorldFrameData::publishPalette()` indexes immutable record identity
  instead of hashing/comparing its floats for every batch/view. It registers
  **every** consuming command completion, including cache hits. Frame rollover
  clears identity ownership before retiring pages; pending or abandoned command
  lifetimes determine when storage can recycle.

This establishes one real producer-to-native-consumer ownership boundary that
later detached jobs can consume. There is no worker dispatch, geometry/material
record migration, GPU visibility or Metal 4 command migration in goal one.

## Measured comparison

Apple M1 Max/64 GiB, macOS26.5.2, Xcode26.6; Release `-O3 -DNDEBUG`, LTO off.
One declared run: 300 frames per arm in each of six fixtures, 3,600 samples total,
ten alternating ABBA/BAAB blocks. Native opaque/legacy-mask/PBR-mask submissions,
both index widths, distinct view matrices and new producer records each frame.
Definitions and thresholds were committed before timing in the
[goal](../work/active/apple-shadow-records-goal1.md).

CPU total sums preparation/encoding, command commit and same-frame input/index
cleanup plus autorelease drain **per sample** before calculating nearest-rank
p50/p95. Creation, finite validation, the extra copy, shared ownership and native
publication/binding are included. GPU completion wait is excluded and retained
separately in raw data. Compilation and other validation were stopped for timing.

| Fixture: views / draws per view / poses / joints | Raw p50 / p95 ms | Record p50 / p95 ms | p50 saving |
|---|---:|---:|---:|
| Light: 1 / 32 / 4 / 32 | 0.038708 / 0.043416 | 0.029584 / 0.033875 | 23.57% |
| Unique: 1 / 32 / 32 / 32 | 0.038375 / 0.042876 | 0.039916 / 0.044751 | **−4.02%** |
| Repeated: 4 / 256 / 8 / 64 | 0.979333 / 1.040708 | 0.402208 / 0.439458 | 58.93% |
| Repeated: 6 / 1024 / 16 / 110 | 8.304126 / 8.408375 | 2.314500 / 2.386417 | 72.13% |
| Unique per view: 4 / 256 / 256 / 64 | 1.010208 / 1.060624 | 0.567458 / 0.612833 | 43.83% |
| Unique per view: 6 / 1024 / 1024 / 110 | 8.370125 / 8.507125 | 3.336250 / 3.443916 | 60.14% |

The two repeated fixtures exceed the predeclared 10% saving and the light case
stays within its 5% regression ceiling. Their ten-block saving ranges are
57.99–59.13% and 71.97–72.22%, respectively. The single-view unique case is
consistently slower: −5.79 to −0.83% across blocks, about **1.54 microseconds**
extra at p50. Registered as [PPD-089](../POST_PORT_DEFECTS.md#ppd-089--immutable-attachment-records-cost-more-for-single-use-poses).
No owner waiver or repair is implied. Multi-view unique cases reuse each pose
across views; they are not zero-reuse frames.

An independent raw-data check confirmed all sample counts and per-sample CPU
sums; published GPU arena bytes are identical between arms in all six cases.
CPU-owned record payload adds 6,208–5,423,104 bytes per fixture frame, excluding
object/allocator overhead. This is not zero copy or a whole-process memory
measurement. The source cache and GPU arena still have separate storage.

## Correctness and delivery evidence

All commands exit0 unless the retained setup history explicitly says otherwise:

- `tools/metal/doctor.sh`; isolated configure; `AL_METAL_CONFIG=Release
  AL_METAL_JOBS=8 AL_METAL_BUILD_DIR=<isolated-build> tools/metal/build.sh build`.
  Feature flags match Alpha6, including OpenHTJ2K ON with its exact source,
  archive and receipt. Initial OFF setup was caught and corrected before app
  delivery/timing. Sandbox cache failure and deliberate build interruptions
  remain in receipts; they are not successful full-build claims.
- Validation-enabled CTest subset: nine file entries, 25 cases, **53,601
  assertions**. Includes the new record tests, full-cost fixture parity, world
  data, shadow capture/gather/indirect/materials, classic body pose and reload.
  Native R32 output matches across raw/record paths, pose changes and alpha
  discard; malformed/non-finite data, metadata mismatches, frame rollover,
  multiple commands, cancellation and resource lifetimes are covered.
- Actual-source producer-cache CPU probe: 18 assertions; strict compile and run
  pass. Its extracted bodies/hashes are pinned to the implementation commit.
- `tools/metal/verify_app.sh <app>`: **346 ARM64-only Mach-O files**;
  `codesign --verify --deep --strict`: pass. Static receipt verifies embedded
  source/version, isolated identity, native Metal/owned-host import checks and
  retained music, voice, media, OpenHTJ2K and OpenJPEG symbols/payloads.
- `python3 tools/metal/audit_gl.py --mode strict`: 3,540 files, zero blockers or
  scan errors. `python3 tools/metal/validate_harness.py`: 33 required files pass;
  `git diff --check`: pass. No tools were changed.

App: **Alchemy Metal Alpha G1, 26.4.0.68987**. Durable local staging:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-shadow-records-g1-20261113/Alchemy Metal Alpha G1.app`.
Copied with `ditto`; staged signature and main-executable hash rechecked.
Main SHA256: `278f8ea009fe71976a3c509fb1afffe635ff9d3ab135ebd6478ebfd9aec76594`.

All raw commands, logs, manifests, samples, analysis and copy receipt are in that
staging directory's `evidence/`. Original receipt directory:
`/private/tmp/alchemy-apple-shadow-records-g1-evidence/`.
Raw `benchmark.log`: 1,398,781 bytes, SHA256
`cce9b3dc096ba174d699d86550397c4e4427df096a9193aac4fd5e27491bf113`.
Test binary SHA256:
`8623df119d0abaa2896faa410bea152d9684f0a6fbacf2da4ab082601a61353c`.

To repeat the comparison, build `almetal_tests` in Release, first run the nine
file names recorded in `evidence/validation.json` with API/shader validation,
then run the test executable with filter `[attachment-palette-cost]` while
`MTL_DEBUG_LAYER`, `MTL_SHADER_VALIDATION`, capture and HUD flags are absent.
Exact argv/environment and source/binary/metallib hashes are in
`evidence/benchmark.json`; the committed benchmark source implements the fixtures.
Before/after power snapshots show AC/80% and no recorded thermal/performance
warning. These are snapshots, not continuous temperature measurements.

## Assessment and next decision

**Keep this experiment for owner testing.** It demonstrates a substantial
reduction in the repeated native shadow-input work at unchanged raster output,
with a small, explicit single-use cost. It justifies evaluating this ownership
seam in the viewer. It does not prove that the entire architecture is faster.

The control is the retained raw path in the same modified optimized executable,
including its new null-record branches. It is not a separately compiled untouched
Alpha6 binary. The fixture uses produced palettes and indexed pose association;
it excludes the actual producer-cache getter, unchanged SL culling/pose solving,
scene traversal and view-matrix computation. Private D32 output, GPU busy time,
total native-wrapper retirement, full visual parity and in-world FPS remain
unmeasured here. No synthetic frame is reported as a Second Life viewer frame.

The owner assessment is a sequential comparison of this exact staged app with
accepted Alpha6, using the same region, camera, graphics/exposure/shadow settings,
resolution, avatar density and warm cache. Check attachment/body shadow coverage,
masked/PBR materials, turning and teleport, then actual frame cadence. The app
retains the existing `AlchemyNext` settings/cache namespace; none was reset here.
If behavior or cadence worsens, retain the result and stop/revise. If the owner
accepts it, goal two is the retained geometry/material source and single admitted
shadow-list work specified in the route, with a policy for the single-use case.
**Goal two has not been started and nothing has been pushed.**
