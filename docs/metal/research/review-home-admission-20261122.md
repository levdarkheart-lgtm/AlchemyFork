# Review fixes and measured Home admission findings — 2026-11-22

The maintenance fixes are implemented locally. **Home FPS is not fixed.** Increasing
main queue depth from two groups to three removes that named admission wait, but
moves blocking elsewhere and produces worse frame-time tails in the completed
comparisons. The normal candidate retains two groups and the original adoption order.

This report is the authoritative current disposition for this follow-through.
The defect register retains individual history and owner decisions; the current
snapshot and work ledger link here instead of serving as separate performance reports.
The collected data is analyzed and collection is closed; the normal candidate is ready for owner testing.
No new-scope push is approved.

## What changed in production and validation code

| ID | Implemented correction | Executed proof |
| --- | --- | --- |
| PPD-582 | Correct the old slot claim; exercise real above-water slot 3 with different pending probe packs and alpha 1→3→1 restoration. | Focused 3,805 assertions/1 case and integrated transparency 1,829,148/13 pass Metal API and Shader Validation. The initial invalid fixture remains recorded. |
| PPD-583 | Version reusable qualification, zero-tag, attribution, histogram and closed Home analysis with sanitized fixtures. | Integrated repository tool suite: 259 tests, one optional skip, exit 0. Public/private A02 numeric results and qualification exactly match after only null private-marker removal. The public numeric memory-window helper retains global misses and actual sample gaps; eight boundary tests pass. Its actual 250-sample reduction matches the reference, with at most one ULP of quantile interpolation rounding. |
| PPD-584 | Reconcile current status against source, accepted code and retained history. | 211 current-field corrections and 44 scoped accepted/removal closures; histories preserved. The audit lists 74 specific missing-proof actions, not 74 confirmed current bugs. |
| PPD-585 | Describe camera queries as request-time/non-atomic, FOV in radians, with possibly infinite far projection. | Description-only change; integrated build passes and actual query was used in qualification. |
| PPD-586 | Repair diagnostic octree child-array compilation and reject a ninth child before mutation/ownership transfer. | Original diagnostic compile failure retained; fixed normal/diagnostic compile passes. Normal 8,264 assertions/7 cases and ASan/UBSan 8,331/8 pass. |
| PPD-587 | Wake the existing fetch owner directly when HTTP replies arrive. Bound dispatch turns to 16 callbacks or 1,000 µs between complete callbacks; rearm remaining work. Preserve cancellation and synchronized notifier teardown. | Consistent core build passes; actual queued-thread 4,783 assertions/21 cases and HTTP 180/11 pass, including remainder, race, FIFO and exception cases. Live comparison appears below. |
| PPD-588 | Remove duplicate housekeeping in nonthreaded fetch mode. | Exact-once owner fixture and integrated application build pass. Normal threaded Home cost is not inferred from this repair. |
| PPD-589 | Release a borrowed response reference when a notifier throws. | Actual exception/lifetime fixtures in the rebuilt HTTP suite pass. |

Native admission/presentation diagnostics are opt-in and bounded. The public field
map versions 25 metric records. Diagnostic-OFF paths avoid the new clock/observer
work; existing renderer ownership, generation, completion and quality rules remain.
Startup-only recording limits are explicit (default 600 seconds, configurable up to
3,600; file allowance 64–512 MiB; fixed 8,192-record ring). Private cap3, early-adopt
and four-display capture experiments are restored out of canonical source/tests.

[Register truth audit](register-truth-audit-20261122.md) contains the exact reconciliation
and unresolved proof actions. Accepted Scheduling69450 and Maintenance69445 remain
accepted history; their completed asset, preference and abandoned-path repairs are
not reopened as implementation tasks. Working native graphics controls are not removed
because an inherited class or setting name originated in OpenGL.

## Answers to the five Home questions

### 1. What prevents the oldest required work from finishing?

The exact captured dependency is an older post command whose forward/alpha-containing
fragment encoder still executes while admission waits. Its resource bindings and probe
pack are identified, and native/MST/capture identities are independently joined below.
The measured pass is not inferred from an overall GPU-busy percentage. Separate live
pass-associated hardware counters characterize ALU, texture and occupancy behavior;
they do not assign an exclusive instruction cost to the captured encoder.

A second instrumented case has a substantial delay after GPU execution ends. It is
reported separately below; it cannot be blamed on shader work or a particular completion
handler from the available samples. Ordinary uncaptured main-admission lag is generally
about 0.2 ms, so the exceptional instrumented case is not substituted for that baseline.

### 2. What independent work is ready but withheld?

Captured input arrays are available before admission. The private early-adoption
arm validates and owns immutable inputs before the gate; canonical validation and
adoption remain after it. The experiment keeps two groups and submits no GPU work early. The actual GPU ownership fixture passes 87 assertions/1 case. In the first
live run, all 792 preparation observations qualify; 791 additionally prove matching
frame/group/epoch adoption ended before the actual admission observer. One missing gate
sample stays excluded from that stronger claim. Adoption averages 10.7 µs; gather averages
0.792 ms and encode 3.127 ms. These elapsed stage spans are not exclusive CPU samples.

The second run has 783 matched adoption spans and 782 complete before-gate proofs;
one missing gate sample remains explicit. Earlier next-frame vertex work is already
queued in the detailed timelines. Later lighting depends on the current G-buffer and
shadow results. Source generation predicates alone are not proof that every next GPU
resource is independently ready. The experiment measures the concrete ready CPU stage
rather than claiming all unencoded scene work can safely run concurrently.

### 3. Why is work withheld?

The ordinary main limit is a logical two-group policy, with up to three transactions
per group. The source-isolated cap3 arm changes exactly that one constant, preserving
all other resource, generation, queue, drawable and adoption rules. It admits three
main groups and independently records zero older-main-group wait increments.

That does not remove all dependencies. The separate shadow admission authority remains
two groups/six invocations. The detailed cap3 trace shows blocking there while older
main light/post work executes. Resource-reuse and publication rules remain necessary;
raising a group limit does not make their inputs independent or bypass their leases.

### 4. Does earlier admission or preparation improve the complete path?

The completed queue-depth comparisons do not support activating cap3. The initial two
cap2 means are 62.916/64.643 ms with p95 75.660/74.134 ms. Cap3 means are 65.029/65.927 ms
with p95 114.678/111.582 ms. The clean extended cap3 reverse run is 64.805 ms mean,
110.501 ms p95. It removes the named main-policy wait while leaving worse tails.
These are separate live workloads; asset/probe populations differ, so they are not
claimed as a perfectly isolated causal FPS estimate.

The first extended normal run is 63.251 ms mean/75.778 ms p95. Earlier adoption gives
63.108/74.672 ms in its first run. Moving a roughly 11 µs stage earlier is demonstrated;
the repeated difference is below 0.25% in mean period and does not justify a production reorder. The
second run and final normal control are included in the final comparison table below.
No extra production switch or dormant experiment is retained on this evidence.

### 5. What happens to input latency and memory?

Real native keyboard events are traced through camera processing, immutable view
adoption, main/UI submission and the first changed world presentation. Only complete
parent chains qualify. Event time is a bounded interval; uncertainty is preserved.
The final policy arm reaches 12 qualified trials. Repeated normal and early-adopt arms
have actual measured samples but do not both meet the investigator-imposed 12-trial
target. Collection was stopped after owner feedback about elapsed time. No exclusion
is promoted to a success and the original target is not declared passed.

Each main fixed window also has 250 independently sampled physical-footprint observations
scheduled at 200 ms, with actual bracket times and gaps retained. Sampled maxima are not
continuous peaks. Physical footprint, Metal current allocation, registry bytes and page
bytes overlap and are not added. Differences between live asset populations cannot be
called the marginal allocation cost of a third frame. Final observed values appear below.

## Final uncaptured cadence and response observations

| Run | Cadence use | Mean / p95 / p99 frame period (ms) |
| --- | --- | ---: |
| Normal L01 | Qualified | 63.251 / 75.778 / 82.977 |
| Normal L02 | Qualified | 63.913 / 74.784 / 82.694 |
| Earlier adoption L01 | Qualified | 63.107 / 74.672 / 80.927 |
| Earlier adoption L02 | Qualified | 63.759 / 72.509 / 79.829 |
| Three groups L01 | Excluded: analysis interference | 65.745 / 110.480 / 119.490 |
| Three groups L02 | Qualified | 64.805 / 110.501 / 118.726 |

These are application-loop start-to-start periods, not monitor scanout. Complete
eligible periods in the declared 150–200-second windows are retained; the excluded
window contributes no comparison. Earlier pilot runs are a separate cohort.

| Arm | Qualified / attempted input trials | Mean event→present bounds (ms) | Mean selected UI source→present bounds (ms) |
| --- | ---: | ---: | ---: |
| Normal | 6 / 8 | 188.78–210.12 | 127.87–127.87 |
| Earlier adoption | 10 / 18 | 186.86–209.73 | 124.19–124.19 |
| Three groups | 12 / 16 | 207.35–265.01 | 174.78–174.78 |

These means preserve per-trial uncertainty; they are not hardware-to-photon
latency or isolated policy effects. Normal/earlier adoption do not meet the original
12-trial target. The investigator stopped repetition after owner feedback about elapsed
time; the owner did not explicitly cancel the broader engineering task. The larger
selected-source delay with three groups is an adverse observation.

| Fixed window | Physical-footprint sample mean (GB) |
| --- | ---: |
| Normal L01 | 12.872 |
| Normal L02 | 13.344 |
| Earlier L01 | 12.751 |
| Earlier L02 | 12.860 |
| Three groups L02 | 12.917 |

Each window has 250 fully contained API brackets scheduled at 200 ms; actual jitter
and gaps remain explicit. Three-group L02 retains one global missed interval and a
maximum selected start gap of 295.9 ms. Observed samples are not continuous coverage;
different assets prevent marginal third-group memory attribution. Earlier adoption
moves the small CPU operation ahead of admission without a material repeated frame
gain, so canonical adoption remains unchanged.

## Exact GPU dependency, counter scope and policy migration

### Home GPU admission: established evidence

The strongest same-command evidence identifies a captured ordinary main post command that later became the oldest required group. Admission waited **29.685 ms**. That command's native GPU envelope was **27.517 ms**; its atmospheric-haze encoder had **19.889 ms of Fragment activity inside the wait**. Main-thread state was Blocked for **29.655 ms**. GPU end preceded the diagnostic completion observation by **0.298 ms**, and the observation preceded wrapper return by **0.069 ms**. This establishes substantial GPU work on that particular required path; the wait is not merely inferred from overall device activity.

The encoder label is broader than a single haze shader. The captured encoder includes one fullscreen draw and 120 indexed draw records, including subsequent forward/alpha work. Its 19.889 ms therefore cannot be assigned exclusively to atmospheric scattering, probe evaluation, ALU instructions or texture reads. Actual captured bindings include probe radiance, SH and an immutable probe-constant slice with **27 configured probes**. The captured shader library is byte-identical to the staged library, and the relevant renderer, pipeline and device source hashes match the staged identity.

Identity is supported by independent facts: native main-group/ticket records; complete creation/commit brackets inside the capture API window; unique calibrated GPU endpoint pairs; and the complete ordered four-post/18-render correspondence in the capture. A separate diagnostic validated 504 endpoint joins against independent lifecycle/static-label joins with zero contradictions. The failed initial full-command ordering assumption is retained: sorted capture records follow observed commit order, while the unsorted stream follows observed creation order. Neither captured pointers nor native resource serials are treated as Instruments object IDs.

The capture was **watchdog-cancelled after three display transitions**, although four main bodies were created and committed inside its actual capture window. Capture and stop-call perturbation mean these timings are not an ordinary cadence or FPS comparison. Both later counter-profiling attempts were stopped without accepted results.

Evidence: `home-admission-questions/current-capture4/FINAL.md`, `static-capture-ordered-bijection.json`, `raw-record-order-recheck.json`, `actual-bindings-and-byte-identity.json`, and `static-join-freeze-02.json`.

### Separate live limiter observation

The later live normal-run diagnostic yielded **779 unique native-to-Instruments command joins**, with no ambiguous matches. The observed common command/GPU/thread-state span was **4.450 s**. Main-thread state included **3.304 s Blocked** and **1.135 s Running**. Those are short instrumented observations, not a new throughput baseline. All 69 ordinary-main workload records in that span had **53 configured probes**, so the live workload is not interchangeable with the captured 27-probe frame.

The counter export exceeded its bound and remains incomplete. A separately validated interior subset contains **32,677 complete counter packets**: **653.54 ms of supplied intervals within a 1.302444 s outer span**, with gaps preserved. Only packets wholly contained in exactly one identified target Fragment interval were associated with that pass:

| Fragment interval present | Observed packet duration | ALU Limiter | Texture Sample Limiter | Fragment Occupancy |
| --- | ---: | ---: | ---: | ---: |
| Main atmospheric-haze encoder | 145.92 ms | 33.70% | 6.13% | 11.03% |
| Main depth-classified BASE encoder | 144.64 ms | 44.44% | 6.36% | 14.52% |

These are **duration-weighted device counters during the identified intervals**, not isolated shader costs. Other processes and concurrent Vertex/Compute activity are not excluded. Counter gaps are not filled; no whole-recording population, GPU-busy percentage, exclusive ALU bottleneck or FPS improvement is claimed. The counters give measured hardware context for the live pass sequence, while the earlier capture supplies the exact same-command resource/pass proof.

Evidence: `home-admission-questions/current-live-limiters-01/command-stage-join.json` and `counter-pass-temporal-association.json`; packet qualification and failed-export provenance are in `home-admission-questions/live-limiters-01/`.

### Completion delay and admission policy

The live recording also contains a contrary case that must remain visible. A required light-chain wrapper waited **72.435 ms**, but its GPU ended **52.561 ms before the diagnostic completion observation**; observation-to-return was only **0.015 ms**. The main thread was Blocked throughout that 52.561 ms interval. There were **zero CPU samples from any target thread inside it**; the surrounding target sample gap was 74 ms. This does not identify the responsible handler, lock or scheduling cause. The counter subset ends near this command's GPU completion and does not cover most of its later completion gap.

The diagnostic completion observation is emitted after handler entry, status/timestamp
property reads and producer bookkeeping; it is not native callback-entry time.
Source registers resource-plan retirement, retained-owner release, optional timing and diagnostic callbacks. Retirement already releases backing leases after unlocking the registry. Outside the exceptional gap, CPU samples show Metal completion machinery performing retirement and resource destruction on five non-main worker threads; those samples do not establish the cause of this particular delay. Ordinary uncaptured baseline completion lags, supplied separately by the cadence analysis, and the GPU-dominated captured wait above must not be merged with this instrumented outlier. Recording perturbation is possible but unproved. No new handler plumbing or source fix follows from this evidence.

The private cap3 experiment changes **one ordinary-main logical group limit from two to three**. Per-group width remains three. The independent shadow limit remains **two groups with six invocations**, and generation leases, resource planning, queue/drawable limits, adoption order and quality controls remain active. In the earlier cap3 diagnostic, three main groups were actually pending and main admission did not wait; blocking instead appeared at the independent shadow authority. One **48.485 ms** shadow wait contained only **12.542 microseconds** of its target's Fragment activity, while earlier main light/post stages overlapped **47.118 ms** of the wait. Source proves the shared queue construction, but overlap and submission order alone do not prove a particular resource hazard or FIFO dependency. Another 184 ms shadow wait retained a 140.890 ms GPU-end-to-diagnostic-observation gap.

These observations establish which admission path and pass were involved. They do not establish that extra queue depth is a production improvement, that immutable next-view inputs were already available before every shadow wait, or that a resource-reuse barrier can be removed. The separate controlled policy/earlier-adoption, memory and input-to-presentation comparisons remain the authority for those decisions.

Evidence: `home-admission-questions/current-live-limiters-01/wait-completion-delays.json`, `callback-full-stacks-and-gap-thread-states.json`, `callback-gap-thread-state-details.json`; `home-admission-questions/current-b02/queue-source-and-completion-delay.json`, `gather-shadow-ordering.json`; and `home-admission-questions/private-cap3/manifest.json`. Source landmarks: `almetaldevice.mm` callback registration and wrapper wait; `almetalresourceplan.cpp` retirement; `almetalworld.cpp` main and shadow admission authorities.

Apple's primary API contracts support the interpretation boundaries: [GPUStartTime](https://developer.apple.com/documentation/metal/mtlcommandbuffer/gpustarttime) and [GPUEndTime](https://developer.apple.com/documentation/metal/mtlcommandbuffer/gpuendtime) are host-time execution endpoints, distinct from the CPU-observed callback timestamp; [captureObject](https://developer.apple.com/documentation/metal/mtlcapturedescriptor/captureobject) selects capture scope. The installed Metal SDK headers additionally state that device capture includes commands created after capture begins and committed before it ends. These contracts do not make a command envelope a hardware-busy measurement or a diagnostic observation timestamp the GPU completion instant.

## Live HTTP handoff comparison

The same-budget frame-triggered control and independent-wake candidate are compared
at Home and Club with ordinary caches, fixed camera/quality and native full screen.
Only the inline independent wake differs between their 27-source manifests; both
retain the notifier, timestamps, 16-callback/1,000 µs bound and exception repairs.
The shutdown histogram is fetch-owned reply publication→handler entry, before the
worker lock. It includes the whole fetch lifetime (login, loading, fixed view and
shutdown); it is not network RTT, decode/upload latency, a world-only phase or FPS.
The deliberately held-main-poll component fixture measures 25.019666 ms versus
13.791 µs, demonstrating independent progress without requiring a live FPS claim.

### Actual HTTP reply-age observations

| Scene / arm | Callbacks | Observed p50 bounds (ms) | Observed p95 bounds (ms) | Age ≥8.192 ms |
| --- | ---: | ---: | ---: | ---: |
| Home, frame trigger | 21 | 8.192–16.384 | 16.384–32.768 | 71.43% |
| Home, independent wake | 33 | 0.256–0.512 | 4.096–5.519 | 0.00% |
| Club, frame trigger | 1,846 | 16.384–32.768 | 32.768–65.536 | 84.45% |
| Club, independent wake | 4,621 | 0.256–0.512 | 2.048–4.096 | 0.48% |

At Club, the control has 38.30% of callbacks at or above 32.768 ms; the candidate
has none. Observed maxima are 148.965 ms and 12.852 ms respectively. Both were
fresh processes with matching source except the wake, unchanged caches and the same
bounded dispatcher. Counts and world activity differ, so this is not an isolated
loading-time/FPS estimate. Home is sparse (21/33 callbacks); Club has 1,846/4,621.
The component hold test establishes the dependency directly, while these live
distributions show the repaired stage making materially faster progress in the
observed Club workload. No additional runs are planned.

## Collection failures and limits retained

The first extended cap3 cadence window is externally excluded: a reader missed the
quiet boundary and was paused during that window. The replacement reverse window
is separately qualified. The normal counter recording succeeds, but complete XML
value/model exports exceed their bounds; only validated complete interior packets
are used. Four-display counter replays stopped without accepted values. Initial
invalid fixture, missing native parents, failed readiness checks and postprocessor
false failure are retained with their corrective records. Successful tool exit is
never used as a substitute for a measure's qualification.

The investigation took too long because repeated attempts continued toward a
self-imposed input-trial target. Owner feedback ended that repetition. The actual
measurements and negative results are retained; no further queue-depth or shader
experiment is needed for this handoff.

## Remaining engineering work and owner boundary

Home's expensive GPU shading remains the main performance task (PPD-319). The
measured targets are the required main lighting and forward/alpha paths, with their
actual probe inputs and resource lifetimes. A future optimization needs retained
output and complete affected-pass/frame improvement; changed-lighting SH/radiance
replays are diagnostic controls, not shippable fixes. The earlier shader audit did
not find a safe missing early-alpha discard: G-buffer alpha also carries retained
emissive/shadow semantics.

The exceptional post-GPU completion tail remains an accurately scoped diagnostic
observation; its responsible handler/lock is unidentified. It does not justify
removing native waits, moving arbitrary callbacks into active render transactions
or attributing all normal Home delay to completion delivery.

The [canonical work list](../work/active/metal-port.md) preserves the other substantive
requirements: General job-category age, required versus deferrable geometry, safe
main-reply service around waits, upload CPU preparation and credit age, pipeline
archive/compile cost, retained owner incidents and broader product qualification.
The [audit](register-truth-audit-20261122.md) lists individual records whose proof or
owner acceptance cannot be reconstructed; they are not silently resolved.

The normal candidate retains two groups, original adoption, the fixed independent
HTTP owner service and the review fixes. New scope remains local until owner testing
and explicit push approval. No Home FPS fix or product/release completion is claimed.

## Validation, candidate and provenance

On Apple M1 Max /64 GiB /macOS 26.5.2 /Xcode 26.6, doctor and integrated/restored
full builds exit 0. Exact actual OpenHTJ2K 0.27 pixel decoding is verified in each
measured process; OpenJPEG header parsing/encoding is separate. The normal app
passes ARM64/signature verification (346 Mach-O components). Native GPU suites
and core fixtures are listed above. Final tools run 259 tests, one optional skip,
exit 0. Tests require the existing `tools/metal/requirements.txt` dependencies;
two incomplete interpreter environments are retained as failed attempts before
the qualified run. No rendering source was changed to address environment errors.

Normal owner candidate: `~/Alchemy-candidates/Alchemy Metal Review Candidate 26.4.0d69454.app`.
Executable SHA256 `7c4e45dce8e7066e7911d42dbee04291ad8cbccf29b09d5be6111ab965248dcd`,
UUID `C1C910DE-5E27-3C74-93D6-0FD739075A62`, matching dSYM. It was built from
`d191bb062f` plus the explicitly recorded 28-source patch; all source hashes match
the restored canonical code. The Metal library is
`6fe71e2833331703acc4912f83f21c9e6ff0a14b7ef5fad70b1f75dccd688793`.
No private cap3, early-adoption or multi-display-capture branch is present in that app.
The normal app is running as PID98258 at Club in native fullscreen for owner testing,
with diagnostics off and no LEAP helper or fixed-camera controller. The rendered
world and runtime decoder/executable identity were observed; owner acceptance remains.

Private immutable evidence root: `artifacts/metal/ppd567-review-20261122`.
`home-comparison-analysis/final-descriptive-comparison-01/comparison.json` pins the
six final runs and four separate pilot summaries. `http-final-comparison-01.json`
pins the four callback histograms. `identities/group2-final.json` records the app,
source hashes and symbols. `tools-tests-final-04` records the final tool suite.
Source, actual process/decoder, camera, controls, clocks, excluded intervals and
failed attempts remain explicit. Raw recordings, coordinates and account data stay
private; these public descriptions are not permission to upload them.
