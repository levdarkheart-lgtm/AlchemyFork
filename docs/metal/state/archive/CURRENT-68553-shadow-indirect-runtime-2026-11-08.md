# Indirect shadow experiment regressed — 2026-11-08

Source-exact Release `26.4.0.68553`, renderer
`b1d198674de3037faf0377680423373b1f9a71d7`, was built, signed and verified
349/349 ARM64. Executable SHA-256 is
`47fbc7b514c629b64c1b9f8e7526a69c2a9447f569418e854d1061f2ffde74d4`;
packaged metallib is
`5ea78dde8f6fab2b915d1d984f4cbe5da2fb4c7517c1d9acfded240c2ec31ea5`.
The measured predecessor bundle is preserved locally at
`/private/tmp/alchemy-metal-68550-measured-baseline.app`, matching its original
executable hash. No release was published.

The owner-authorized native CUA flow closed `68550`, launched `68553`, entered
full screen before clicking saved Log In, sent Command-Shift-Y, explicitly
set camera `Preset123`, closed the camera panel and sent Midday again. Fresh
LEAP verification passed exact binary, position and all 21 Ultra controls.
Resolution is 3456 x 2168. Setup completed before the 300-second/120-frame
warmup expired; the 300 measured frames are consecutive and exact native.
Quality, mirrors, shadow coverage and six-face cadence are retained.

## Frame result: reject as a performance win

| 300-frame CSV | 68550 | 68553 |
| --- | ---: | ---: |
| Median frame time | 112.7755 ms | 124.3173 ms |
| P95 frame time | 133.6875 ms | 139.6973 ms |
| Mean frame time | 114.5774 ms | 125.2259 ms |

Median frame time regresses 10.23%; p95 regresses 4.50%. The new native census
reports median 73,968 encoded draws/frame; old zero draw counts are the dormant
GL counter, not proof of no work. Command submissions stay similar, mean
29.48 versus 29.3867/frame. Summed completed command GPU spans rise, but overlap
and must not be treated as whole-frame GPU time.

CSV rows span frames 2451–2750, require source/build/display provenance and
contain zero GL fallbacks. `perf_compare.py --require-provenance` exits 0 and
reports the regression; the product performance target is not met. This is one
live scene, not the required multi-scene acceptance gate.

## CPU work is reduced and redistributed

The 30-second Time Profiler capture has 28,566 Running main-thread samples over
30.624 seconds (93.28% of one core); predecessor is 94.68%. No backtraces are
missing; address-only leaves are 3.46% versus 5.34%.

| Inclusive sampled CPU category, ms | 68550 | 68553 |
| --- | ---: | ---: |
| Direct draw wrappers | 4,467 | 3,556 |
| Direct draw plus ICB execution | 4,467 | 3,891 |
| Entire shadow renderer including preparation | 7,426 | 6,976 |
| Shadow capture/finish/render union | 9,467 | 9,291 |
| Arena/data-setter union | 2,185 | 1,752 |

These fixed windows do not normalize per-frame throughput. Categories overlap.
Whole shadow replay drops 6.1%, but the broader family drops only 1.9% and
remains 32.5% of sampled main activity. Preparation occurs before render encoder
begin, so encoder-only savings would overstate complete CPU savings. New
helper/native work totals 1,180 ms; `IdentitySet::insert` alone is 447 ms
inclusive/368 ms self. Native ICB/proxy functions total 83 ms inclusive.
There are no native proxy-allocation samples, which is not proof of zero calls.
Arena memmove attribution rises 403 to 728 ms; bulk library copies can replace
inlined stores, so this is not an exact byte-volume measurement.

## Next implementation and pending GPU analysis

GPU recording is complete and finalization/export is pending. The full renderer
code is being revised toward native direct bulk shadow submission using the
same immutable slab, with normal non-ICB PSOs. Ordinary G-buffer batches also
need only their actual diffuse texture count: original `bindIndexedTextures`
does this and current forward owners already use count-only binding with the
same 32-wide signatures. Keep material/PBR contracts and linear/sRGB views.
The shader dead-work experiment is being reverted for the next clear comparison.
No net FPS claim is permitted until that candidate is built and measured.

Raw local evidence is
`/private/tmp/alchemy-metal-shadow-indirect-metrics-20261108.xcmz295i`;
run manifest is `artifacts/metal/qa-runs/candidate-68553-shadow-indirect-20261108-01`;
CSV is `artifacts/metal/private-68553-shadow-indirect-20261108-01.csv`.
Final CPU comparison SHA-256 is
`6e9f033bce469c3a9fe50b59abe49adee229dafc19c7f25097dcc70d9c7fc3b0`.
Build/test/verify logs remain in
`/private/tmp/alchemy-metal-shadow-indirect-tests-20261108.HGz01r`.
