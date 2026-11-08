# Maximized 68547 CPU/GPU comparison — 2026-11-08

The owner requested login, Midday, camera preset `Preset123`, a residency wait,
measurement and comparison with the previously reported `68543` baseline. The
owner then requested window maximization before collection. No renderer or
graphics-setting repair was made as a result of this profiling run.

## Provenance and exclusions

- Branch `codex/apple-silicon-metal`; renderer source
  `18aedd2ecb9ca388cd0e04dffe0c4f32ece9e323`, documentation HEAD at launch
  `5c2b71732399ec55e66680cf8fa12e59357fd6a4`.
- Exact candidate is Xcode Release `26.4.0.68547` at
  `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
  Executable SHA-256:
  `6f942194c757fb8a3335970ed1f527b5cf815d4d177d71e1d5ed5ee270c62609`;
  metallib SHA-256:
  `5bd02961a77fdfcef9d28410e9f92fc86d1de08197aec113660e0cd0ccd779d1`.
- The owner approved closing both older viewer processes and launching this
  exact candidate. Native UI login succeeded; Midday was selected with the
  viewer shortcut and `Preset123` selected explicitly. Only the requested UI
  setup was performed. The automatic LEAP probe matched all 21 compared live
  Ultra controls, including mirrors, anisotropy 4, 256 m, shadow detail 2,
  hero 1024/update rate 1, SSAO, exposure and occlusion.
- Maximization generated the `1728x1084` point resize at 19:22:31 UTC and
  subsequent native capture logs confirm **3456x2168** pixels, matching the
  baseline. All primary recordings start more than 60 seconds afterward.
- Physical host remains the M1 Max/64 GiB acceptance Mac, on AC power. The
  Metal trace reports Nominal thermal state. Earlier build/GPU-suite proof is
  in [the implementation checkpoint](CURRENT-hero-cpu-encoding-checkpoint-2026-11-08.md).
- Raw evidence is local at
  `/private/tmp/alchemy-metal-68547-hero-cpu-metrics-20261108.Ol8EPh/maximized`;
  run logs/manifests remain in ignored
  `artifacts/metal/qa-runs/candidate-68547-hero-cpu-comparison-20261108-01`.
  Baseline raw evidence is
  `/private/tmp/alchemy-metal-68543-root-cause-20261108-1.U9wKEH`.
- Exclude every earlier `3456x1916` recording in the parent directory. Its
  Metal trace was terminated while taking excessively long to finalize. An
  initial maximized CPU trace overlapped that finalizer and is corroborating
  only; the primary **`time-profiler-clean.trace`** was recorded after that
  process was confirmed stopped. The primary maximized Metal trace completed
  normally. Do not mix these recordings.
- The opt-in collector produced its 300 rows before final camera/window
  setup, so that CSV is excluded. The new Metal trace has 208 presents and the
  old one 178: this is a bounded trace/log comparison, **not** the formal
  consecutive 300-frame product gate. Exact camera transform and per-view
  geometry/residency census were not captured; live-region content is not
  deterministic. These limits do not justify dismissing the owner's matched
  scene and fixed-camera setup.

## Matched-resolution trace results

Twenty clean ten-second log samples, after trace finalization and all exports
completed, establish a bounded **8.2 versus 7.1 median FPS** comparison:

| Log statistic | Published 68543 baseline | 68547 |
| --- | ---: | ---: |
| UTC window | 17:20:27–17:23:38 | 19:33:26–19:36:37 |
| Samples | 20 | 20 |
| Median FPS | 7.1 | 8.2 (+15.49%) |
| Mean FPS | 7.08 | 8.16 (+15.25%) |
| FPS range | 6.9–7.3 | 7.9–8.3 |

The old process later drifted to a 5.4 median over 19:03:11–19:06:23. That is
recorded separately in the local summary; the primary comparator remains the
previously published window paired with its CPU/GPU trace. This drift is an
additional reason to require repeated controlled runs before an isolated
product speedup claim. The historical `68532` run reached about 9.9 median log
FPS, so the new 8.2 is still below that earlier best.

The candidate Metal trace runs 19:28:17.154–19:28:47.788 UTC for 30.633711 s.
Baseline runs 17:14:07.260–17:14:37.894 UTC for 30.63432002 s. Frame-delivery
intervals below are Instruments present-request intervals, not the viewer's
formal per-frame CSV wall times. Values are normalized by all captured
presents consistently.

| Metric | 68543 | 68547 | Change |
| --- | ---: | ---: | ---: |
| Presents | 178 | 208 | — |
| Presents/s under Metal tracing | 5.8105 | 6.7899 | +16.86% |
| Presentation interval p50, ms | 153.769 | 128.592 | -16.37% |
| Presentation interval p95, ms | 204.120 | 156.513 | -23.32% |
| Command submissions/present | 31.5955 | 29.6731 | -6.08% |
| Render encoders/present | 181.2528 | 171.4856 | -5.39% |
| Total CPU encoder ms/present | 78.8906 | 65.6952 | -16.73% |
| Hero CPU encoder ms/present | 41.2189 | 31.4189 | -23.78% |
| Hero shadow CPU encoder ms/present | 24.1811 | 14.5808 | -39.70% |
| Nonhero shadow CPU encoder ms/present | 24.8302 | 19.2200 | -22.59% |
| Hero G-buffer CPU encoder ms/present | 13.3672 | 13.1287 | -1.78% |
| World G-buffer/sky submission CPU ms/present | 8.8107 | 11.1322 | +26.35% |
| GPU-active interval union | 57.255% | 62.136% | +4.88 percentage points |
| Metal allocated size, GiB | 4.452–4.592 | 5.214–5.323 | resize-history confound |

Encoder categories are portions of CPU encoding, not independently additive
whole-frame CPU budgets. Hero transactions remain approximately six/present
(5.9663 to 6.0144). Hero shadow passes fall from 13.7865 to 12.0288/present,
consistent with the retained two directional maps per face and removal of
redundant cube projector casters. Nonhero shadow passes fall from 8.0112 to
5.8558/present; this label includes directional and projector shadows, not
projectors alone.

## What the clean CPU profile establishes

The clean candidate profile contains 28,961 ms of running main-thread samples
over a 30.365-second sample span. No main backtrace is missing; 3.53% of leaf
samples are address-only, with symbolized ancestors. These are separate
statistical CPU windows: a similarly busy thread can complete more frames.
Do not interpret equal total sampled seconds as unchanged throughput.

| Main-thread sampled category, ms | 68543 | 68547 |
| --- | ---: | ---: |
| Running main thread | 28,785 | 28,961 |
| Strict hero-probe call tree | 14,590 | 14,473 |
| Sun-shadow generation | 16,536 | 14,131 |
| Generic shadow renderer | 10,212 | 7,546 |
| G-buffer renderer | 4,717 | 5,652 |
| Both fragment-texture binding paths | 5,409 | 2,651 |
| Indexed draw encoding | 4,373 | 3,901 |
| Metal/IOGPU resource-list self time | 2,274 | 1,408 |
| Batch `push_back` record-copy self time | 1,384 | 0 |
| Inline-byte copy self time | 821 | 4 |

Inclusive call trees overlap. The expected old copy paths are absent or nearly
absent; texture-binding samples fall 51.0%, generic shadow-renderer samples
26.1%, and resource-list self time 38.1%. Hero work still occupies about half
of active main-thread samples. The GPU is active only 62.1% of traced wall
time, so the CPU feeding bottleneck remains material alongside GPU work.

Current measured CPU targets are concrete:

- Draw-record construction accounts for 2,217 ms of leaf `memset` samples,
  **7.66%** of active main-thread samples: 1,529 ms shadow and 688 ms simple
  G-buffer. Large shared records still contain material/terrain/avatar arrays
  unused by many draws. Baseline already constructed the same member defaults
  in a local record before copying it. This is remaining construction cost,
  not proof that all 2,217 ms was introduced by emplacement; outlined
  constructor and instruction attribution also changed.
- Shared arena publication plus inlined hashing accounts for 2,779 ms,
  **9.60%**. It includes 845 ms leaf copying, 344 ms hashing and 250 ms equality
  checking. These aggregate immutable material and palette data, not palettes
  alone. Removing repeated buffers did not eliminate packing, hashing and
  copying. The 80-byte opaque shadow block is verified, but it occupies a
  256-byte-aligned arena range.
- These two disjoint sampled groups total **17.25%** of active main-thread
  samples. They do not explain all remaining CPU time. Native buffer creation
  still samples 531 ms, including 128 ms under arena publication; terrain
  factors, light lists and other owners still allocate.

The next bounded code options are compact common capture records with separate
optional payloads, producer-generation identities for immutable palette and
material reuse where correctness permits, less repeated constant packing, and
completion-safe reuse for remaining small allocations. View-dependent palette
data still requires an identity that distinguishes its actual bytes. These
are proposed follow-up repairs, not changes made during this measurement task.

## GPU work and memory limits

Labelled GPU intervals remain substantial: hero work 36.219, world base light
17.299, world haze 16.904, hero haze 7.032, hero above-water 6.164, nonhero
shadows 5.830 and light map 5.382 ms/present. Hero shadows fall from 5.796 to
4.984 ms/present, while base light rises from 13.769 to 17.299. These labels
overlap and are not whole-frame GPU duration; their summed 115.390 ms/present
cannot be used as a serial frame budget. Increased throughput does not mean
every GPU channel improved.

Total allocated size is higher, but this run resized after login. The target
pool contains 35 targets/1,125.54 MiB versus the contemporaneous baseline's
9/292.966 MiB; the pool increase is 832.574 MiB. The exact settled FPS windows
contain 12 baseline and 13 candidate memory samples. Driver medians are
4,634.865 versus 5,422.83 MiB, and buffer-wrapper estimates 241.418 versus
319.893 MiB. Retained previous-size
pooled targets are a confound; do not attribute the full allocation increase
to the arena. Discard bias remains 4; the texture-pressure separation is still
reverted. No fetched-image cache-churn cause or FPS cost was isolated here.

## Commands, evidence and next boundary

Both `xcrun xctrace record --template 'Time Profiler' --attach 17746
--time-limit 30s --no-prompt --output <clean.trace>` and the corresponding
`'Metal System Trace'` recording exit 0. `xctrace export` exits 0 for CPU,
present, encoder, GPU interval, GPU state, allocation and thermal tables.
`python3 tools/metal/trace_submissions.py <trace> --output <submissions.json>`
also exits 0. Power checks report AC attached and no recorded thermal or
performance warning. Raw captures remain local.
Documentation-only `python3 tools/metal/validate_harness.py` exits 0 (32
required files); `git diff --check` exits 0. No rebuild or renderer test rerun
is needed for these evidence-only documentation changes.

SHA-256 fingerprints:

- `time-profile-clean.xml`:
  `d93420deb2eb4ed4d28ff9282ee9c563f9410d46f2105dfe52e041e7a25177dd`
- `presents.xml`:
  `8556541f6a882c2180f1e0a9fb609986c3a9710d602034c1a9a94ca9ef052826`
- `encoders.xml`:
  `1d83ebf0352d3caf610c358f3fe4c8696db8a20f65f5dd87c2a8203e9a98a65d`
- `gpu-intervals.xml`:
  `3714b1b680645ab8e11900f0779eb1fa5898f1a950f7ac66f011dfd7c581df6a`
- `submissions.json`:
  `302939b6a06591108d37758e3d2614210c53590200c3efd4b2ee9aacbf85293a`
- `comparison-68543-vs-68547-maximized.json`:
  `47916010ee4f8ab9607e1be9ae88dac81dbea56f4b137f085a0b894033e0e945`
- `cpu-comparison-68543-clean.json`:
  `da04480c11bc152eec6c80cf50fdba54d23f4e423c205722eb4c82080055c51f`
- `settled-log-comparison.json`:
  `410e498e1c14608edba70365e8e577e685b6f140a4d9d00f708618bef831ee51`

This is measurable partial recovery against the previous `68543` report. It
does not close the historical best `68532`, 24-FPS objective, visual parity or
formal paired benchmark gates. Preserve this checkpoint and wait for the
owner's next implementation instruction before applying follow-up fixes.
