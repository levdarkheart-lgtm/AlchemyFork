# Hardware-aware adaptive decoder concurrency

2026-11-12. Owner reports quicker HUD/world loading in six-worker68876 and is
continuing to visit places. They explicitly request implementing hardware-aware,
reactive decoder concurrency while their current viewer remains running, with
profiling afterward. This is a local experiment, not push approval or a proven
cross-Mac optimum. Base `f0fa305dd50a866b985db98a42c7dff87f068609` on
`codex/apple-silicon-metal`; accepted OpenHTJ2K behavior remains unchanged.

## Implementation contract

- Query enabled cores and enumerate performance tiers; preserve EmulateCoreCount
  and bounded fallback when topology queries are unavailable/inconsistent.
- Start with conservative hardware-dependent normal/maximum admission. M1 Max
  observed8performance/2efficiency cores: minimum2, normal4, maximum6. A budget
  is not a core reservation or affinity. Other layouts require measurement.
- Keep a bounded worker pool. Gate new decode starts; never interrupt running
  decodes or hold scheduler locks during decoding/completion callbacks.
- Increase admission for sustained waiting work with busy allowed workers;
  reduce with hysteresis under CPU contention, thermal/power constraints, or
  sustained quiet. Raw FPS/GPU-associated intervals are not CPU-pressure signals.
- Count both queued and gate-waiting jobs, retain request ownership/IDs/callbacks,
  reject new submissions during drain and wake/drain accepted work before join.
- Record anonymized queue/service timings, active/waiting counts and transition
  reasons. Include a fixed-maximum comparison setting without a codec change.

Root owns startup, worker integration, CMake/settings, canonical docs and build.
Independent peers own the new scheduler/tests, existing Darwin info helper and
worker tests. Shared renderer files remain outside scope. No parallel viewer.

## Validation and next action

Evidence root: `artifacts/metal/adaptive-decoders-20261112/`.
`baseline-before.json` pins the existing six-worker binary/hash and live process.
Preserve68876, accepted68860 and projector68869 app bundles. Build a distinct
`Alchemy Metal Adaptive Decoders` app, then verify ARM64/signature and source
audit. Run deterministic policy/bounds tests and real threaded admission,
callback, exception/post-failure and shutdown/drain regressions. Exercise actual
Darwin queries on the M1 Max. Doctor/build/verify/test commands and failures must
be recorded. Do not launch/restart or profile until the owner is ready.

## Implemented policy and measurements

`LLGetDarwinCPUInfo` reads enabled logical cores and bounded numeric physical
performance-tier counts. Incomplete/inconsistent topology remains unknown.
The initial maximum is fastest-tier cores minus2, bounded by effective total
and2–16. Normal admission is ceil(2*maximum/3), at least2. Missing topology
falls back to total minus4; missing counts use2. EmulateCoreCount limits the
effective total/fastest tier; the inherited minimum2 is retained even for an
emulated1. This is a conservative starting policy, not a measured optimum on
other Macs or a claim that lower tiers have no useful decode capacity.

`ALImageDecodeScheduler` keeps workers alive and gates admission under a
condition variable. The main thread evaluates pressure at most twice/second:

- Raise by1 with at least500ms evidence: >=95% occupied permitted slots,
  saturated waiting backlog for>=90% of that interval and oldest wait>=50ms.
- After5s completely quiet, reduce by1/second toward normal admission.
- Sustained system CPU busy>=90% AND main-thread CPU/wall>=85% reduces by
  1/second after1s, down to2. Other-process CPU load alone is insufficient.
- Fair thermals cap at normal admission; serious/critical thermals or Low
  Power Mode cap at2. Existing jobs finish. Recovery has a2s hold and advances
  gradually toward normal before backlog can raise it further.
- `ImageDecodeAdaptive=false` holds the same detected maximum, intentionally
  bypassing adaptive pressure limits for a matched fixed-worker comparison.

These are initial empirical thresholds. System and main CPU ratios come from
actual Mach CPU-time/tick deltas; first/unavailable samples are unknown, not0.
The sampler uses availability-guarded NSProcessInfo thermal/power state and
releases Mach port rights. It does not interpret slow GPU frames as CPU pressure.

Worker ownership/IDs and the original processRequest→finishRequest sequence
are retained. Unexpected codec exceptions still follow the original WorkQueue
main-loop rethrow path; RAII releases admission without reporting corrupt data.
The pending count now includes queued, gate-waiting and active requests.
Failed posting cancels the registered ticket. Shutdown rejects new requests,
opens admission and drains accepted jobs before joining, including destruction
without another update tick. No changes to ThreadPool's general implementation.

Private `Decode admission:` logs report the limit/reason, waiting/active counts,
oldest age, request counts, occupancy, CPU/thermal operands and timing sums/maxima.
Queue wait is submission→admission; decode wall ends immediately after
processRequest; service wall also includes completion callbacks. These are wall
durations, not sampled CPU time or time-to-visible/full-resolution textures.
A bounded5s window uses500ms buckets. Completed counts include retired jobs,
not just successful texture reconstructions. No asset IDs or names are logged.

## Observed implementation checks

Doctor exits0. Darwin helper strict ARM64/macOS11 compile/probe passes, including
15 topology cases, actual10active/[8,2], first/foreign-thread unknown samples,
idle/busy sampling and2000calls without extra Mach references. Pure C++17
controller tests pass239assertions/13cases normally, with ASan/UBSan, and with
ThreadSanitizer. Preliminary production llimage compile exits0.

Review caught and corrected strict ticket-order gating that could deadlock an
out-of-order producer post, and unsigned unknown thermal state becoming a false
serious-pressure value. Both have regressions. Unexpected-exception semantics
were retained after review identified the risk of evicting valid cached input.
The first app compile exits1 for unsigned→LLSD assignment ambiguity; explicit
LLSD Integer conversion corrects it. Failure and retry receipts are preserved.

Production worker/codec checks pass:44unique cases/1222assertions, including
10worker/247assertions,13policy/239,7image/38,11layout/642 and3OpenHT/56.
The initial7CTest entries pass; a subsequent worker-only rerun passes its2
entries after adding fixed-maximum coverage. Real-queue tests cover blocked
callbacks,2→3→4 admission, thermal reduction while jobs continue, shutdown
without another tick, direct pool-close/post-false ticket cancellation, original
mainloop exception propagation and fixed-mode physical/admitted width4.
Tested production input hashes match the implementation; test variants overlap
and are not added to the unique count. The external accepted-validation cache
rejects current-source configure due to its old /private/tmp source path; the
existing current-source main build is used instead, with target-only tests and
OpenHT enabled. Both the failed request and successful replacement are retained.

Preliminary full viewer build retries successfully(exit0) after the LLSD fix.
Final source-exact configure/build exits0/0. Candidate26.4.0.68880 is built from
`98f41e652beb6150d9012cd25af102bd6c5e76df`, Release/O3/LTO off, in
`build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/Alchemy Metal Adaptive Decoders.app`.
Executable SHA256 `243b5787b6b3d9789953a34b58e6b58615c859861c41c29dfe19c1400b5cd3d4`;
the exact source SHA is present in the binary. Verify_app exits0:346Mach-O,
zero invalid non-ARM64 slices. Strict deep codesign verification exits0.
Production source audit exits0:3516files/912informational/0blockers/0errors.
All three older viewer executable hashes are rechecked unchanged: fixed6/68876,
projector68869 and acceptedOpenHT68860. Private final-build/configure receipts,
build-identity.json, preserved-apps-after.json and validation.json bind evidence.

Exact final build commands:

```sh
cmake -S indra --preset ninja-os-arm64 -B build-Darwin-ninja-os-arm64-shadow-occlusion '-DAL_CHANNEL:STRING=Alchemy Metal Adaptive Decoders' -DAL_BUILD_TESTS=OFF
AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64-shadow-occlusion AL_METAL_CONFIG=Release AL_METAL_JOBS=3 nice -n 10 tools/metal/build.sh build
tools/metal/verify_app.sh 'build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/Alchemy Metal Adaptive Decoders.app'
codesign --verify --deep --strict 'build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/Alchemy Metal Adaptive Decoders.app'
```

Next: owner-coordinated launch of this exact app, retain the selected existing
cache via equal CacheLocation/NewCacheLocation without purge/migration, and
start recording before the target arrival. Verify startup topology/normal/max
and live ImageDecodeAdaptive, then correlate admission logs with CPU samples.
Use ImageDecodeAdaptive=false in the same binary for a later matched fixed-max
comparison. No adaptive viewer has been launched, owner confirmation/push
approval remains pending, and no adaptive loading gain is observed.
Original68876 log shows normal owner-session shutdown at17:42:22Z; no agent
closed it. Its binary remains unchanged; cache contents remain preserved.

## Owner-triggered runtime measurements

Owner authorized launch and two destinations. Exact68880/hash243b5787 starts
PID36734 with adaptive1/normal4/max6, retaining the previous fixed-six cache
(30813files/4522534437logical bytes before launch, no purge/migration). Live
LEAP history works from logged-in samples; critical observed controls remain
stable within each capture. Both recordings save before heavy analysis begins.

[Full two-destination analysis](../../research/adaptive-decoder-profiles-2026-11-12.md):
first180.840185s includes login, second181.057662s clips teleport onset by
2.664s. Both save normally(exit0), and CPU/metrics/admission analyses pass.
Decoder CPU95.182/214.378s; peak5s3.607/5.901core-equivalents. Heavy first15s
after finished callback averages5.847cores; last30s decoder CPU0.235s.
Admission rises4→5→6 and later returns toward4. Heavy inner observed mean
queue wait2.484s versus17.931ms process wall; sampled backlog peaks1860.
No cancellation; conservation checks pass. Occasional small jobs delay the
continuous5s-quiet reduction, without evidence idle permits consume CPU.
Late presentation8.226/20.967FPS and GPU-associated spans188.272/51.695ms
describe different workloads, not a controlled adaptive speedup.

Owner additionally reports black world/visible UI at first login. PPD-008
[source/log investigation](login-black-world-investigation.md) finds native
renderer warm-up starts after STARTED and lasts6.60176s while UI submissions
continue over a possible cleared background. Pixel attribution remains
unverified; no rendering fix applied. Next source boundary is early async
prewarm preserving all scene-publication guards, then pixel/readiness proof.
Keep this issue separate from decode-policy acceptance. All changes stay local.

Owner requests deeper explanation of mean2.484s queue wait. The [queue audit](../../research/decoder-queue-wait-forensics-2026-11-12.md)
reconstructs12005 admissions and29818200ms wait;7120 early admissions account
for97.555% of it (mean4.086s), later4885 average149ms. An interval admits272/s
against477/s arrivals, reaching1860 pending while six workers are CPU-busy.
The captured ramp is brief; reductions happen after the backlog. FIFO lacks
decode priority/cancellation, but obsolete/low-value work prevalence is unknown.
Read-only analysis/plot passes; no viewer/source change. Next evidence is a
matched6/8 burst test with frame impact and queue usefulness telemetry.
