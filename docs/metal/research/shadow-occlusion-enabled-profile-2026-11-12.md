# Enabled directional-shadow occlusion profile — 2026-11-12

The missing native shadow-camera query path is implemented and was observed
producing queries for all four directional cascades. This first enabled run
**does not demonstrate a performance improvement**. Combined directional and
projector shadow processing still accounts for about 41% of main-thread sampled
CPU. Texture decoding and the image working set were substantially busier than
in the [earlier capture](busy-region-profile-2026-11-12.md), so the two sessions
cannot establish the change's causal FPS effect.

## Candidate and live state

- Alchemy Metal Shadow Occlusion **26.4.0.68849**, Release, LTO off, ARM64 only.
  Base `65bc4e110f6243246f1b9008d2247123f9068ba8` plus local implementation patch
  SHA256 `f60e5508e085f5e75063088029ee075bc024385ce9a361b1116bc9ea7de514ea`.
  Full production-source hashes and exact commands are in `source-receipt.json`
  under the private artifact root below. Changes remain local; no push approved.
- Executable SHA256
  `4d04fdd723f9cebb3433e8ac0ed59cf69c5b929f75f1803f79e59b16c1959b88`;
  UUID `225F5551-4B76-3596-8403-3E5BD20628DC`; captured PID **31451**.
- Full build exit 0; app inspection exit 0, 346 Mach-O files/zero non-ARM64;
  deep strict signature verification exit 0. Focused Metal API/GPU validation:
  6,229 backend assertions/three cases; 1,424 query assertions/11 cases.
  Actual-body frontend probe: 52 checks. Independent source review found no
  actionable integration defect. These do not establish visual parity.
- Owner manually logged in, chose the busy scene and graphics, retained the
  warm disk cache and readied the recording. No preset or destination override.
  Window inspection was unavailable because Computer Use access to Alchemy was
  not approved; no agent screenshot or visual-regression claim.
- Session overrides: `AlchemyRenderShadowOcclusion=true`,
  `RenderShadowCullMode=0`, `UseOcclusion=true`. LEAP verified them after login,
  immediately before capture (sample age 1.83 seconds), and during capture.
  Live `RenderShadowDetail=2`, far clip 64, shadow resolution scale 1,
  projector cap 2048, screen-space reflections 0.
- One-time native receipts recorded nonzero query batches in cascades 0–3.
  This establishes query production, not a census of zero results or how many
  caster submissions were avoided. Projector queries remain excluded as in
  the original; this feature does not disable directional rendering indoors.

## Capture, recovery and symbol evidence

Command: `xcrun xctrace record --template 'Game Performance Overview' --attach
31451 --time-limit 3m --no-prompt --output <private>/shadow-occlusion-enabled-3min.trace`.
There was no rolling-window option. The timer reached its limit, and the owner
was immediately told it was safe to log out.

Instruments stalled during finalization. Its raw kernel trace was APFS-cloned
before terminating only the stalled finalizer; original record command exit 1
is retained honestly. Importing the preserved raw file with the same Overview
template exited 0. The recovered trace reports **10:38:46.913–10:41:48.194 UTC,
181.281 seconds, Deferred**. CPU sample timestamps span 181.280997 seconds;
all 37 five-second bins are populated. The requested three minutes survived;
a repeat login was not needed for the retained data.

The import targets All Processes, so every analysis explicitly filters PID31451.
All 724 layer-count, 5,792 layer-duration and 150 process-metric rows belong to
that PID; one `ALMetalOverlayView` layer. CPU export has 448,210 target samples,
including 163,771 main samples. Raw/imported files remain unchanged.

The import lost image associations, leaving CPU frames as addresses. dSYM
symbolication of the trace itself failed with exit55, no relevant associations.
Names for the exact main executable were recovered separately:

1. SDK-native KCDATA validation identifies the target task container, PID31451
   and main TID97826234. Its `LIBRARY_LOADINFO64` array directly records this
   candidate UUID and load address `0x100210000` at file offset `0x3a5d50`.
2. The executable and generated dSYM UUIDs match. `atos` uses that recorded
   load address and exact dSYM; no baseline ASLR address or guessed slide.
3. A separately labeled derived CPU XML replaces 22,739 candidate frame names,
   preserving original addresses, samples, timestamps and weights. Reanalysis
   reproduces all raw totals exactly. Other-library leaves remain unresolved:
   67.920 CPU seconds/41.47% of main CPU; missing backtraces 0.002 seconds.

Application ancestor unions below can include unresolved driver descendants.
They are useful ownership evidence; the recovered run cannot support a new
fully symbolicated driver-leaf or memcpy breakdown. Sampled Running weights are
CPU estimates, not elapsed frame time, GPU execution or blocked-time measures.

## Observations

| Metric | Earlier 68841 run | Enabled 68849 run |
|---|---:|---:|
| Trace wall duration | 301.302 s | 181.281 s |
| Presented FPS, duration-weighted | 35.570 | 28.284 |
| Main sampled CPU / wall | 83.50% | 90.34% |
| Viewer sampled core equivalent | 1.63 | 2.47 |
| Combined shadow share of main CPU | 41.26% | 41.03% |
| Combined shadow CPU / wall | 34.45% | 37.07% |
| Native shadow renderer share of main CPU | 17.50% | 19.96% |
| ImageDecode workers, core equivalent | 0.436 | 1.175 |

Inclusive categories overlap and cannot be added. `generateSunShadow` includes
both directional and projector processing; **67.192 CPU seconds** in this run
must not be labeled directional-only. Native shadow rendering accounts for
32.691 CPU seconds. The named occlusion union across all views is 0.416 CPU
seconds; that is not an isolated measurement of this feature's entire cost.

FPS observations cover 180.608 seconds across 181 intervals. Interval median
is 28.569; p10/p90 25.000/31.999; range 17.000–35.734. These are interval
aggregates, not individual-frame tail latency. Presented FPS also does not
measure how often a fresh world scene was rendered.

Both ImageDecode workers accumulated **213.000 CPU seconds** in 181.281 wall
seconds, with recovered OpenJPEG decode symbols. The prior run used 131.486
CPU seconds over 301.302 wall seconds: roughly **2.69 times** the decode activity
per wall second now. Main-thread texture creation also consumed 11.023 CPU
seconds under `updateImagesCreateTextures`. Warm disk cache did not eliminate
in-session decoding/texture creation. This evidence does not identify why each
asset was decoded, or prove that decoding caused the whole FPS difference.

Follow-up examination of the decoder timeline shows the recording began with
both decode workers almost continuously occupied: 59.478 CPU seconds in the
first 30 wall seconds (1.983 cores), falling to 8.168 CPU seconds in the last
complete 30-second window (0.272 cores). A further burst occurred at120–150s,
so the decline was not monotonic. This strongly suggests the texture workload
had not settled when capture began. User-visible readiness was accepted without
a decoder/texture-memory settling gate; that is a capture-protocol gap.

Keeping the disk cache does not preserve every decoded image or Metal texture
across viewer exit. Source `lltexturefetch.cpp:1346–1358` explicitly routes a
successful cache read into `DECODE_IMAGE`; `1932–1945` queues decoding. The small
raw-image fast cache can supply reduced-resolution previews; it does not replace
full requested-resolution decoding (`lltexturecache.cpp:2145–2170`). The2.69x
figure measures decode CPU activity per second, not downloaded bytes or texture
count. Without per-asset history, initial warmup versus new arrivals/resolution
changes cannot be fully separated. A future steady-scene comparison should
verify low/stable decoder activity and texture-memory growth before recording,
then change only the query gate within the same running session.

Instruments' reported peak footprint rose from 22.64 to 32.82 GiB. Native log
image-wrapper estimates rose from 5,753 to 16,885 MiB, versus buffers from
2,833 to 3,218 MiB. Driver and wrapper estimates are different gauges and cannot
be added as a footprint breakdown. Their trends supply a concrete texture
working-set lead. Device thermal state was Nominal throughout.

## Meaning for the implementation

The optional port omission is repaired, but this evidence does not establish
that it materially reduces the club workload. The persistent shadow CPU share
keeps the prior preparation/submission findings relevant. A causal feature
comparison would keep one running scene/camera/settings and alternate only the
query gate after settling, while counting actual culled groups and world frames.
That additional experiment has not been performed.

A source audit found that completed query result batches retain native command
buffer objects until discard or the existing 64-frame pool history retires them.
At the observed presentation rate this suggests about 2.3 seconds of history
**if viewer frame progress tracks that rate**; it is not measured retention time.
Main-world queries use the same policy. Query buffers use eight bytes per query,
with one batch shared by its tickets. No unbounded ownership chain was found.
Whether a completed command object retains every referenced GPU resource needs
an actual native-resource lifetime measurement; the 10 GiB footprint increase
cannot be attributed to these tickets from source inspection.

## Evidence and next action

Private root: `artifacts/metal/shadow-occlusion-implementation-20261112/`.
The `profile-3min/` directory contains the recovered `.trace`, untouched raw
recording/CPU export, derived symbolicated XML, exact dSYM, `comparison.json`,
`overview-audit.json`, `symbol-recovery-audit/receipt.json`, launch/live settings,
all command/exit receipts and the failed-finalizer record. No private capture
was uploaded. Implementation details and further owner testing are tracked in
[the active work ledger](../work/active/shadow-camera-occlusion.md).
