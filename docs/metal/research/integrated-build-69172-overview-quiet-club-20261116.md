# Build69172 — verified quiet/club engine assessment

Both fresh recordings contain the requested five minutes. This assessment uses
only the successful Overview recapture, not the earlier discarded detailed trace.
No old/new FPS improvement is established: these are two different live scenes
using one unchanged candidate, with no same-binary baseline-policy comparison.

## Evidence and scope

- Source350b803597e48a8c0e6b26b1050a47a5ae41e2ee; build26.4.0.69172;
  PID82260; executable SHA256
  b8e5596c3abcc69403a36d9bdb7935a991877edbc1f792b4513f2f161b41e9e4.
- Private evidence:
  `artifacts/metal/architecture-69172-overview-recapture-20261116T120552Z`.
  Exact source/binary/settings, launch arguments, owner markers, complete logs,
  per-scene record/TOC receipts, raw exports, analysis scripts and hashes retained.
- Game Performance Overview template SHA256
  acb55044577f59565776ae995a12b6b7007f202587b5467e02914b78ae989938,
  retention window0. Each300s recording was bounded390s/2GiB raw; neither bound
  fired. Quiet/busy final trace sizes40,123,783/50,709,469B. Both recorders,
  TOC exports, CPU exports/parsers and all14GPU-table exports exited0.
- Quiet12:07:39.721–12:12:40.547UTC,300.826s; club12:13:58.900–12:18:59.637UTC,
  300.737s. CPU samples span0.257–300.826s and0.271–300.736s. GPU metric buckets
  cover100%/99.9469% respectively, including the explicitly missing final~101ms
  of the club window and small bucket-boundary gaps. These are actual sample
  ranges, not an inference from recorder duration.
- Owner drove login/teleport/logout; process closed normally, Goodbye12:20:37.
  No competing build/native test during measurement. No renderer, quality,
  resource/loading policy or source change between these two recordings.

## Whole-frame interpretation

| Metric | Quiet | Club |
|---|---:|---:|
| Mean layer cadence |14.849FPS|20.706FPS|
| Mean reported frame interval |67.352ms|48.186ms|
| Main running CPU |85.941CPU-s|186.827CPU-s|
| Main running CPU / capture wall time |28.57%|62.12%|
| Worker running CPU |57.979CPU-s|151.427CPU-s|
| Last60s cadence |14.797FPS|20.875FPS|
| Median latest triangle counter |1.391million/frame|12.943million/frame|

Average main CPU work is approximately19.2/30.0ms per reported frame, below the
67.35/48.19ms cadence. This does not establish pure GPU saturation or eliminate
short CPU-critical bursts; it rules out treating sustained one-core saturation
as the complete explanation. At club arrival actual main-thread CPU approached
0.96, then subsided. There is useful work on several threads, not a reason to
maximize every core's utilization regardless of frame dependencies.

GPU associated-command in-flight spans116.691/84.416ms and associated-frame
envelopes126.472/84.867ms overlap across frames. They are queue/latency evidence,
not isolated GPU execution time and cannot be added to CPU time as a frame cost.
The reported GPUUsage ratio is a layer in-flight ratio, **not hardware occupancy**.
There are approximately18.84/20.92command buffers per reported frame across the
observed work; count reduction alone is not a performance criterion.

As club content arrives,0–15s averages26.858FPS with12.715mainCPU-s and54.93ms
GPU envelope;15–30s falls19.705FPS despite mainCPU falling9.441s while the GPU
envelope rises88.07ms. Later cadence remains~20–21FPS after most decoding settles.
This is a coupled preparation/submission/GPU/presentation problem. More workers
or a shadow-only CPU fix is not justified as the complete solution.

One concrete pacing incident occurs12:14:12.540–12:14:13.543UTC:7.978FPS and
12reported skipped frames. Eligible frame-interval statistics omit some gaps;
both individual-frame tables are empty. Consequently this capture does not
provide trustworthy whole-scene raw-frame p50/p95/p99 or a worst-gap bound.
The independent LLStats analysis selects disjoint histories rather than pooling
overlapping200-period responses; its period-mean quantiles are not those gates.

## CPU work across the engine

These mutually exclusive main-stack owners sum to each main CPU total. Ordinary
probes include their own shadow, geometry and lighting work.

| Main stack owner | Quiet CPU-s | Club CPU-s |
|---|---:|---:|
| Ordinary reflection probes |27.132|22.700|
| Sun shadows outside probes |20.559|56.359|
| Deferred geometry outside prior owners |14.273|31.708|
| Deferred lighting outside prior owners |7.163|29.785|
| Simulation idle outside prior owners |4.241|16.320|
| UI/HUD outside prior owners |4.673|10.091|
| Post-deferred outside prior owners |0.813|1.158|
| Hero probes |0.002|0.001|
| Other |7.085|18.705|

Cross-cutting club costs overlap those owners: publication29.456CPU-s,
cache13.356, encoder binding18.363, resource-list work9.427 and palette4.764.
Copies consume8.300CPU-s exclusively. These repeated preparation, ownership and
submission operations cross main/probe/shadow views; they should be treated as
shared architecture work, not added together as disjoint frame costs.

Decoder workers use60.754CPU-s at the club:55.357in the first45s,3.918in45–120s,
only1.479in120–300s. Late main work remains112.860CPU-s, including18.650publication,
9.186cache and23.521shadow-capture CPU-s (inclusive/overlapping categories).
Arrival decoding cannot explain the persistent engine costs.

## Loading and unified-memory context

Decoded textures awaiting main-thread creation peak3830, independently of the
near-zero submitted GPU-publication count. The decode ceiling of2 follows that
real downstream queue, not an inference that GPU/frame latency is CPU pressure.
The CPU-pressure predicates use actual thread/system CPU. The create queue clears
around12:14:45–47 and the limit recovers3/4. No safe thread/admission-policy change
is established; repeating the rejected eight-worker burst is unsupported.

The native-source cache repeatedly reaches its3,892,314,112estimated-byte cap.
Near-window quiet/club endpoints count1,988,956/3,970,763byte rejections and
1,276,671/2,288,000capacity evictions. Optional group native-retain fallbacks
are835/5731; required raw/direct rendering continues. Source retained-byte
estimates are not physical unified-memory allocations. CPU costs confirm real
work, but a larger arbitrary cap is not established as the fix.

Desired texture discard bias stays1in all38/52resource samples. Driver allocation,
process physical footprint, RSS and image-wrapper estimates measure different,
overlapping ownership domains and must not be summed. GPU process-peak footprint
is~11,170MiB quiet/~21,079MiB late club; this is neither residency nor a leak proof.
Thermals reportNominal and host observationsAC; GPU clocks/watts/bandwidth are
unavailable. Six club CPUInstructions numeric buckets are invalid/inconsistent
and excluded; CPU sampling supplies attribution. Main named-leaf coverage is
96.08%/96.85%, with unresolved shares explicitly retained.

## Active paths, warnings and acceptance limits

Lighting fusion encoded15240times with zero logged decline/preparation/prefix/
serial/input failures. Large-shadow overlap submitted jobs in both scenes;
owner-join sums8.465/11.621s are not automatically lost frame time. The opaque
indirect path selects0runs because no eligible run reaches2048draws; preparation
and fallback failures are0. Direct rendering is intentional at these sizes.
Main light-values workers, newer command banks/visibility and animated GPU paths
remain disabled. The entire architecture is not performance-accepted.

All255structured warnings/47callsites plus3libpng stderr warnings are classified
in the private warning report. There are0ERROR/FATAL lines, not a correctness
certificate. Three incomplete shadow captures occur only at startup; neither
measured window adds one. No nonfinite skin warning occurs in this fresh run;
that does not close PPD311 from the earlier unchanged-build session. PPD125's
specific repaired null-buffer case is not demonstrably exercised. A single
remaining LOD1volume reference recurs at shutdown(PPD126). Image interlace/ICC
warnings remain PPD312. Failed texture/material retrievals and transport timeouts
affect available content and loading; they are not blanket-dismissed as network
noise. No pixel, mirror, all-functionality or loading-parity acceptance is claimed.

## Concrete architecture follow-through

**PPD315:main admission counts commands where its policy intends complete frames.**
The same renderer commits G-buffer/sky, light-map/lighting and opaque/optical post
as three normal transactions. Its local deque admits only two pending command
buffers. Starting post can therefore CPU-wait for that same frame's G-buffer,
even though their ordered native queue preserves the GPU dependency. The source
proves this path; runtime incidence and its FPS cost remain unmeasured.

Implement bounded logical-main-frame admission, retaining all early commits and
waiting on older groups rather than the current normal graph. Keep two pending
frame groups plus a finite invocation bound, completion-owned resources/credits,
failure/cancellation semantics, UI/drawable pacing and distinct special-view
owners. Do not globally change2to6 or delay the whole graph into one late commit.
Event-gated actual native three-transaction/two-group tests and output/lifetime
checks precede integration. This directly addresses normal CPU/GPU overlap.

**PPD316:repeated roles resubscribe the same dependency Relay to the same Signal.**
Deduplicate only actual subscriptions using immutable IDs while keeping every
ordered topology identity. The30-role/3-Signal case should make3subscriptions;
30distinct Signals still make30. Preserve invalidation, reentrant revision and
transactional failure behavior. This is a small bounded CPU improvement:
Signal::observe totals3.777CPU-s at the club, only part redundant. It is secondary
to frame admission and cannot be presented as the FPS solution.

Do not remove the apparently duplicated world/shadow dependency-observation
calls: their revision guard already avoids a repeated walk, and the later call
handles intervening producer changes. Do not raise residency/decode limits or
force rejected tiny-worker/indirect variants from counters alone. Implement and
verify the coherent candidate, then assess whole-viewer cadence, quality and
loading. Additional owner profiling is useful, not a prerequisite for every edit.

The discarded269GB capture and raw copies were deleted at the owner's request;
its failure history/independent logs remain separate. PPD314continuous timing
code is held in an isolated, uncommitted worktree (119assertions/9CPU cases,
app-hook/build/runtime unverified), not integrated into this tested candidate.
