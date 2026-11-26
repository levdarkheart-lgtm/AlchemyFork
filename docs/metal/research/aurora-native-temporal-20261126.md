# Experimental native Aurora Temporal review

This owner-authorized prototype starts at evaluation checkpoint
`318f0ac8b6a8073fe8d22d0848c84c282ad8b212` on the separate
`codex/aurora-native-temporal-20261126` branch. PPD610 remains open; the
[quality/cost decision](aurora-reconstruction-evaluation-20261126.md), all
original failed bounds and closed MetalFX integration-isolation result remain intact.

## Integration contract

Only saved choice8 is new: **67% Aurora Temporal (Experimental)**, exact2/3 scene
scale and no sharpening. Effective method4 means successful Aurora execution.
Native0 remains the default and existing IDs0–7, output-sized shadow budgets,
independent native snapshots and native UI remain unchanged. Main material mip
bias is log2(actual scene width/output width)-1, selected before rasterization.
Early unsupported views render unjittered linear fallback with zero added bias;
late failures cancel that world and retain the preceding display.

The component uses Aurora's actual WGSL port of FSR3.1 at
`ee458fbf1d95e98c5b3a55c0027a523bb28d7e7f`, reproducibly translated offline with
pinned Naga. It executes native Metal only; no Rust/wgpu runtime is linked.
The surrounding alpha/glow reference is original Alchemy `upstream/develop`
`408fd6c014e6`, pipeline.cpp10712 onward: sharp linear HDR for SSR/metering,
reduced DoF, bloom/glow and then tone mapping/exposure. Aurora is new.

Linear HDR exposure1, reverse-Z perspective device depth and existing RG16
previous-minus-current input-pixel motion feed the adapter. Jitter is separate,
input-pixel Y-down; motion is negated/divided into upstream current-minus-previous
UV in.zw. Camera reprojection is Y-flip conjugated; reverse-Z is already[0,1].
Opaque HDR is genuinely preserved after opaque-forward draws and before the
first transparent/water/haze compositing boundary, including opaque-only worlds.
Generated reactive masks compare that image with final HDR. Existing invalid-pose,
animated/unsafe contribution rejection maps to the upstream composition marker;
that is a documented producer adapter, not reuse of MetalFX's reactive semantics.

Reconstruction precedes bloom/tone mapping. The final resolve preserves native
RGB and Alchemy's jitter-adjusted bilinear current-world glow alpha, rather than
upstream nearest alpha. DoF retains its existing reduced-depth contract before
reconstruction; remaining depth consumers retain their established normalized
sampling. The pinned display-depth pass is included and validated, not assumed
from reconstructed color. DoF may conservatively increase generated reactivity
because preserved opaque color precedes that optical transform.

Existing world history owns camera, object, attachment, avatar and cloth poses
and submission-only jitter. Aurora supplies its documented phase count to that
owner (18 at2304→3456; actual rounded widths govern odd sizes). Parity, previous
jitter and frame index advance only on successful enclosing submission. Resize,
mode/camera/origin discontinuity, GPU error and interrupted publication reset the
stream. Commands retain exact resource generations through completion. Ordinary
opaque tracking, pre-acquired downstream targets and frame-data constant pages
remain; no normal-frame CPU wait or new admission queue is introduced.

## Rendered port qualification

The corrected native component matches the retained Aurora OFF reference
**bit-for-bit across all195,084,288 RGB components**:468chronological main worlds
and414independent mature-reference worlds, with Metal API/GPU validation enabled.
This covers complete history building, stationary/moving MASK and movement-stop.
The same frozen scorer produces exactly the same metrics and failures:

| Quantity | Retained WGSL OFF | Native Metal OFF | Bound |
| --- | ---: | ---: | ---: |
| Stationary edge variation |.000867307430|.000867307430|≤.003921568627|
| Contrast retention |.820112736074|.820112736074|Original metric retained|
| Minimum recall |.936027846714|.936027846714|≥.90|
| Minimum recovered area |.821401135595|.821401135595|≥.85 **fails**|
| Stop18 own-mature error |.015886268101|.015886268101|≤.003921568627 **fails**|
| Stop36 own-mature error |.012319430645|.012319430645|≤.003921568627 **fails**|

These are authored-fixture results, not acceptance of the owner's palm/grass scene.
Matched crops and stationary/move-stop previews use actual native output;12fps
playback labels count worlds, not real viewer seconds. Native remains an imperfect
unjittered detail reference. PPD610 remains open.

PPD614 preserves the initial wrong-port evidence: world0exact, then468-world
MAE.002101218529/max.293335. Language-version alignment did not change it.
Two-frame intermediate comparison found two Naga-renamed pyramid entry points
whose wrong lookup yielded nil fragment functions. Metal accepted vertex-only
pipelines, silently clearing those targets. Generated reflection now supplies
all names and preparation explicitly requires both shader functions. Three
reflection/embedding regressions retain this counterexample; final full histories
prove the correction. No threshold relaxation or algorithm tuning was used.

Full private outputs, initial failures and receipts remain under
`artifacts/metal/aurora-native-20261126/`. No live login or FPS claim is made.

## Actual-size component cost

M1 Max, AC power, nominal thermal state; optimized Release, validation off.
360 warm worlds precede 360 retained samples at2304×1446 →3456×2168.
All18 resident input fingerprints match the earlier cost workload. The native
command includes opaque-HDR preservation copy, format/motion/composition adapter,
all upstream reconstruction passes, display depth and Alchemy glow-alpha resolve.
Uniforms use the existing completion-owned frame pages. Input production/uploads,
file I/O, readback and deliberate benchmark waits are outside GPU/encoding clocks.

| Component | GPU median | GPU p95 | Evidence scope |
| --- | ---: | ---: | --- |
| Native Aurora OFF |10.926ms|12.392ms|New full native command, including integration passes|
| Retained Aurora WGSL OFF |10.996ms|12.846ms|Earlier Metal-backend stage envelope, adapter included; uniform transfer separately bounded|
| Retained MetalFX controls |4.128–4.147ms|5.702–5.773ms|Earlier native command including glow resolve|

These are component costs, not viewerFPS or a matched native-versus-wgpu speedup.
The native implementation remains near the anticipated11ms budget, substantially
more expensive than retained MetalFX evidence. Live extra tile store/load cost at
the opaque-preservation boundary is scene-dependent and not isolated by this
component workload. No one-world-outstanding policy is copied into production.
CPU encode median/p95=.294/.720ms; submit=.011/.022ms, wall time with waits separate.
[Summary](media/aurora-native-20261126/metrics/native-cost-summary.json) and
[all360raw timestamps](media/aurora-native-20261126/metrics/native-cost-raw.json)
retain timing boundaries, thermal state, fingerprints and resource observations.

Explicit component payload:389,904,768texture bytes +43,296,768storage-buffer bytes.
Integration adds26,652,672bytes of opaque HDR preservation and59,940,864bytes of
resolved output; the live HDR/depth/motion inputs and shared constant arena already
serve existing rendering. The benchmark's1,580,679,168-byte device footprint
includes its18-phase resident input pool and compiler/driver allocations.
After warm-up, owned texture/buffer creation and device footprint growth are zero;
one shared constant page is reused719times. Driver-private transient activity is
not fully inventoried by the typed counters.

## Focused correctness evidence

- Twelve actual-component cases pass Metal API/GPU validation: generated reactivity,
  jittered glow edges, output depth, fixed-descriptor reuse, abandon/recover,
  invalid inputs, resize and three submitted worlds before completion.
- Actual viewer renderer dispatch passes204assertions/two rendered cases: genuine
  pre-transparency capture and the borrowed-main Aurora/bloom/display transaction.
  Required display eligibility rejection submits no affected command, preserves
  presentation/pose/parity/jitter, settles once and resets on recovery.
- Glow resolve RGB error=0; jittered alpha-edge error=.000813611<.002 against an
  independent bilinear oracle. Alpha-only glow reaches the actual bloom consumer.
- Owned texture/buffer payload returns to zero at teardown. Existing Native/Spatial,
  motion/history/resource regressions and full application/package checks follow.

[Port identity/parity](media/aurora-native-20261126/metrics/port-verification.json),
[frozen quality](media/aurora-native-20261126/metrics/native-quality.json),
[component checks](media/aurora-native-20261126/metrics/component-checks.json),
[teardown](media/aurora-native-20261126/metrics/component-teardown.json), and
[matched preview manifest](media/aurora-native-20261126/visuals/manifest.json)
make the scope reviewable. The preview clips are not live Home recordings.
