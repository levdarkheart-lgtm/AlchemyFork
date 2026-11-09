# Direct shadow and populated binding runtime — 2026-11-08

Renderer `6c694df23756ed9805b7d853fa83d9fc64212c04` is source-exact Xcode
Release `26.4.0.68554`. Both Release and RelWithDebInfo Metal suites pass
57,344 assertions/75 cases with API/GPU validation. The warnings-as-errors
product build, deep strict signature and 349/349 ARM64 inspection exit 0.
Executable SHA-256 is
`223bdc1054b9cdf73c336a26530431ded5eb4ca001de0339c74390ec41af93db`.
Packaged shader SHA-256 is byte-identical to the 68550 baseline:
`5bd02961a77fdfcef9d28410e9f92fc86d1de08197aec113660e0cd0ccd779d1`.
The complete candidate is preserved at
`/private/tmp/alchemy-metal-68554-goal-baseline.app`.

## Source and controlled setup

Production uses native direct static-shadow packets, one immutable constant
slab, strict adjacent full-triangle range coalescing and populated texture
bindings for ordinary/material/PBR G-buffer draws. Experimental ICB execution
is disabled in production. Original shading math, reverse-Z, clamp, alpha,
casters, six Ultra hero faces and directional shadow cadence remain.

Native CUA closed the predecessor, launched the exact candidate, entered full
screen before saved login, sent Mac Midday and camera shortcuts, explicitly
selected Preset123 and closed the panel. Fresh LEAP verification passed the
exact executable, world position and all 21 pinned Ultra controls. Resolution
was 3456 x 2168. Warm-up required both 120 frames and 300 seconds; setup
completed over a minute before measurement. The 300 measured candidate rows
are consecutive frames 2813–3112, source 6c694df237, Release, quality 6, exact
native 300/300, zero GL fallbacks. No compilation or profiling overlapped
those rows. Raw account/location/camera material stays local.

## Three runs and attribution limits

The same 68550 executable was repeated because its main opaque-post GPU cost
was not stable across sessions. No region change was made. Both old and fresh
baselines use source 175cd155dd and the same Ultra/preset/setup procedure.

| 300-frame CSV | Earlier 68550 | Fresh 68550 | 68554 |
| --- | ---: | ---: | ---: |
| P50 frame time, ms | 112.7755 | 117.4368 | 101.8757 |
| P95 frame time, ms | 133.6875 | 133.5108 | 119.0739 |
| Mean frame time, ms | 114.5774 | 118.9164 | 102.6789 |
| 1000 / mean frame time, FPS | 8.728 | 8.409 | 9.739 |

Against fresh 68550, measured throughput rises 15.8%, median frame time falls
13.25% and p95 falls 10.81%. These are observations for the captured windows,
not a source-isolated attribution of the entire improvement. Full main-camera
transforms and opaque-family counts were not recorded. Same-shader main opaque
GPU channel cost swings from 0.311 to 19.596 to 0.274 ms/present across earlier
68550, fresh 68550 and 68554. The intervening 68553 run records 22.086 ms.
Consequently neither that increase nor its disappearance can be assigned to a
particular optimization. Camera/pass/query diagnostics are the next control.

`perf_compare.py --require-provenance` exits 0 and verifies matching
build/display/quality metadata and sufficient samples. Its product target is
not met: one live scenario does not satisfy the multi-scenario performance
contract, nor does this run achieve 20 FPS.

## Draw and CPU evidence

Candidate native draw census is median 55,647 physical commands/frame. The
baseline's old zero draw counter is unusable for a total-count comparison.
A stable 111-second pre-profiler log window reconstructs actual selected
static-shadow source records from encoded plus coalesced counts:

| Selected static-shadow records | Main owner | Probe/hero owner |
| --- | ---: | ---: |
| Original source records | 14,391,000 | 16,042,359 |
| Physical encoded draws | 4,266,000 | 4,907,093 |
| Coalesced records | 10,125,000 | 11,135,266 |
| Coalesced share | 70.36% | 69.41% |

Combined coalescing is 69.86% of these eligible records; this is not all scene
draws. No packet-slot allocation occurs in that window; main/probe counts end
at 4/7 slots. Runtime ICB execution/preparation remains zero.

The 68554 CPU trace has 28,864 Running main samples over 30.616 seconds,
approximately 94.28% of one core, with no missing backtraces. Hero rendering
is 50.44%, ordinary probe work 6.19%, remaining main 43.37% (disjoint scopes).
There is no fresh-baseline CPU trace. Fixed-window comparisons against the
earlier 68550 trace therefore do not establish per-frame isolated savings.
G-buffer samples fall 5,902 to 4,679 ms, texture/sampler wrappers 3,044 to
1,918 ms and native resource-list leaves 1,582 to 1,298 ms. Entire shadow replay
is 7,426 to 7,233 ms; broader capture/replay is 9,467 to 9,591 ms. The remaining
logical capture/culling work is substantial despite fewer physical draws.
Native direct preparation/execution sample 495/538 ms; native ICB samples are
zero. Broad culling samples are 5,097 ms and overlap other categories.

## GPU/encoder evidence

Final 30.632-second Metal trace records 245 presents, 7.998/s versus fresh
baseline 7.003/s. Encoder CPU falls 60.772 to 44.052 ms/present; this excludes
pre-encoder packet construction and cannot alone measure complete CPU work.
Nonhero/hero shadow encoder CPU falls 17.609/14.100 to 11.267/10.809 ms;
main/hero G-buffer encoder CPU falls 8.598/12.776 to 5.915/8.823 ms.
Submissions remain about 29.3/present. GPU-active union is 75.06% to 71.04%;
allocation 4.395–4.519 to 4.405–4.528 GiB; both report Nominal thermals.
GPU-active wall per present is 107.189 to 88.824 ms. Labelled channels overlap:
main base 14.901 to 17.194, haze 16.394 to 16.940, nonhero shadows 6.046 to
5.026 and hero shadows 5.745 to 5.062 ms/present. Main opaque variability above
prevents attributing all throughput gain to code. No ALU/texture/occupancy
limiter counters were available in these traces.

## Evidence locations and hashes

- Candidate: `/private/tmp/alchemy-metal-68554-direct-shadow-metrics-20261108.jz1nvbtv`.
- Fresh baseline: `/private/tmp/alchemy-metal-68550-baseline-rerun-20261108.92x7iims`.
- Earlier baseline: `/private/tmp/alchemy-metal-68550-draw-payload-metrics-20261108.DnhpTn`.
- Candidate run: `artifacts/metal/qa-runs/candidate-68554-direct-shadow-20261108-01`.
- Candidate CSV: `artifacts/metal/private-68554-direct-shadow-20261108-01.csv`,
  SHA-256 `727e9fac88123e813d58800b3f39a38e1f991df6e5bab398f69eb81f03d60569`.
- CSV comparison: `dd4d204dc06282e37995c60a635966828e521a529983b955fc900b751c1fb72a`.
- CPU comparison: `66c1e44b75dc1ddf641fddcb2ad87ab176e98be1eb81a978f7cf0be256ac6d19`.
- GPU comparison: `e9e293c98a083bd6aaa3eac04f6a33c3ec6299c9805e7463876f79495f8a9570`.
- Final GPU intervals: `cb5c2afdfd30d42ac96bc9c2b7b221c231e1d8dce4a6604b73f632fc0a8f8e26`.
- Encoder XML: `148cf1ad1177919335ae039229ed998b879aca46829b0d6cf8b1f056974c49d8`.

## Next goal

The owner created an active at-least-20-FPS Ultra engineering goal, permits
quit/relaunch/login/measurement, requires general optimizations, and restricts
live testing to the current location and Preset123. Prefer Mac shortcuts before
mouse interaction. Broader coverage uses varied native fixtures and exact
original-source checks, without claiming unexecuted live multi-scene gates.
See [the active campaign](../../work/active/ultra-20fps.md).

