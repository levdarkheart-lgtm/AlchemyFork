# Whole-shadow parallel encoding: validated, measured, not admitted

Observed on the physical M1 Max/64 GiB/macOS26.5.2/Xcode26.6.
The Ultra20FPS goal remains ACTIVE and unmet; infeasibility is unproved.
No viewer was launched for this experiment. All prior viewer sessions remain
closed with their environment plans restored. Translator stays stopped.

## Restored normal viewer baseline

Renderer commit `06d16eb4cb20f4dfe47b4ce71e86e1420eb9e060` restores direct
covered-shadow binding by default after the preceding cache viewer trial failed
to demonstrate an FPS benefit. It retains the original non-argument-buffer
ordinary diffuse sampler and a separate lazy optional material sampler with
identical filtering. Both optimized native suites previously passed435462
assertions/102cases; their eleven per-file hashes match the restored source.

Normal build68606 completed doctor/configure/build/verify/signature, all exit0,
exec39343terminal0. Initial sandbox doctor failed1 on a denied hardware sysctl;
the native retry succeeded. The app has349 ARM64 slices and zero invalid slices.
C++23/O3/NDEBUG/arm64/min26.0/source recordingOFF remain verified.
Executable SHA256 `ac719b12a34964c6d34ac7850f24a0e5dc2a70a626216420ad7cac1b221615ec`;
metallib `3932018186338255b0523028441e5376393a51c2b3ddbad2ad538f1c530376c2`.
Independent source-header/executable/Info/45response-file/signature review passed;
identity JSON SHA256 `e614f1e311d51cb88738a21942205db24aed606c2e006fd585bc921da198e315`.
The actual app is preserved/deep-strict-signature0 at
`/private/tmp/alchemy-metal-68606-direct-default-preserved.app`.
It has not received a new live FPS trial.

## Implementation and original contract

The exact original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`,
`indra/newview/pipeline.cpp:12844 drawShadow`, uses strict depth testing and
ordered static/rigged, avatar/terrain and covered caster stages. The prototype
preserved the already captured native membership/order, reverse-Z, rawR32,
clamp, shader coverage, palettes/cloth, terrain and filtering.

A thirteen-file prototype added a native parallel-parent/two-child bridge, immutable
whole-shadow replay packets and one persistent CPU worker. Root prepared all
FrameData/palettes/factors/materials and retained native resources before opening
the parent. Workers only bound sealed packets and issued draws; root joined,
ended children/parent, aggregated counters and used the existing single command
commit/cancel. Child creation order retained GPU draw order. The default stayed
serial; no shipping producer selected the experimental modes.

Prepared-one and prepared-two modes covered current caster families. Native
order/blend, early-parent end, wrapper/resource lifetime, worker failure/retry,
invalid source, cancellation and actual child counts were exercised. Mixed
640-record streams covered eight reverse-Z/clamp/material combinations and
family-removal positive controls. Two distinct views in one uncommitted command
matched separate serial raw-depth references after temporary CPU palettes and
source vectors were destroyed. Flat terrain limits independent factor-lifetime
isolation; child allocation failure was not injected. No full viewer parity
claim follows from these fixtures.

## Actual native validation

All commands used the optimized Ninja Release executable and native API+GPU
validation, excluding the separate quiet CPU benchmark.

| Actual phase | Exit | Result |
| --- | --- | --- |
| First prototype build | 0 | New native bridge/replay and mixed fixture compiled. |
| `[parallel-encoder]` | 0 |100assertions/1case. |
| Focused raw-shadow fixture | 0 |2175assertions/1case before two-view addition. |
| First full `[almetal]~[frame-capture-gpu]` | 0 |435807assertions/103cases. |
| Two-view fixture rebuild/focused run | 0/0 |2210assertions/1case. |
| Benchmark-block rebuild/final full suite | 0/0 |435842assertions/103cases. |
| Preflight slot-cache rebuild/full suite | 0/0 |435842assertions/103cases. |

RelWithDebInfo was not run for this rejected prototype. Existing baseline
Release/RelWithDebInfo evidence remains separate. No experimental viewer app was
built or launched. Root preserved exact source maps/executable/library hashes.

## Complete precommit CPU measurements

Both quiet runs passed6268assertions, eight additional strict rawR32 pairs,
280samples with schedule/activation/timing checks and Nominal thermals. Each run used
ten cases, seven rotating ABBA/BAAB blocks and common repetitions/physical slice
within a pair. The serial comparator used the same compiled renderer and input.
Preparation, validation, publication, pinning, declarations, encoder creation,
emission, worker join and pass ending were all inside the precommit clock.
Commit/wait/callback-poll/diagnostic drain were separately recorded. Producer
capture, PSO creation and persistent renderer teardown were outside the clock.

| Revision / source records | Serial to prepared-one CPU | Serial to parallel-two CPU | Parallel-two paired range |
| --- | --- | --- | --- |
| Initial /640 |+21.26% |+19.20%,0/7faster |+14.39% to+24.16% |
| Initial /4096 |+22.55% |+13.39%,0/7faster |+5.30% to+21.45% |
| Cached validation /640 |+13.13% |+9.71%,0/7faster |+2.05% to+16.30% |
| Cached validation /4096 |+14.45% |−4.45%,5/7faster |−6.12% to+9.32% |

Positive percentages mean more CPU wall time. The bounded second revision
cached native identities per validation slot, while retaining first/new identity
device checks, unique strong owners and null/count failures. Its apparent4096
gain overlaps SAMEparallel control drift:−5.24%, range−11.32% to+9.43%.
Neither revision establishes a general benefit. GPU controls were variable;
initial4096 SAMEparallel GPU spans differed+19.16%. Do not infer GPU improvement,
whole-frame timing or FPS from these small32×32 synthetic streams.

All140short640sample rows per run were retained despite falling below the8ms
calibration band at the8-repetition cap. Generation advanced per shadow call,
while the viewer normally shares generation across views. Generic resource-owner
callback retirement was not independently observed; the diagnostic wall scopes
are not a shipping CPU-lifecycle total. Cold first calls were uncounterbalanced
diagnostics, not cold performance comparisons.

Decision: reject production admission and further variants of this experiment.
Root froze both complete tested patches and reverse-applied the final13-file
patch. The shared `indra` tree is exactly commit06d16; its source hashes match
both previously completed baseline optimized suites. Normal68606 is unchanged.
Ninja artifacts still contain the rejected prototype and MUST be rebuilt before
any future source-exact test claim. No unmeasured code was enabled in the viewer.

## Private evidence preservation

The initial nine raw packets were copied and SHA256-verified into ignored local
`artifacts/metal/private-whole-shadow-encoding-20261110/`; see preservation.json.
Subdirectories initial-validation/initial-benchmark/initial-audit and
cache-validation/cache-benchmark/cache-audit retain raw commands/exits/logs.
Final frozen patch SHA256
`63636129baa76c4fcff046ac196800e93078eb4064cacf349ffb005bc8d7c2c7`.
Initial complete tested patch SHA256
`2ce2c74d833855a34604c92c8f633da3223b3eb5e79eecb061ba10e41c867d12`.
Initial independent summary SHA256
`23d07670bad949623e060fa4460af63fe102c94294dba3192b24a5c020943624`;
cache summary SHA256
`ef9d5ffcfd8c9a157bdc35a028749fb41411bcd81185165deb327d39bdce35f9`.
No private captures or runtime data were uploaded or committed.

## Next GPU attribution boundary

A query-only native host compiled/ran0 on this M1 Max, confirming timestamp/
GPUTimestamp and stage-boundary sampling only. Draw/dispatch/tile/blit sampling
and utilization/statistic sets are absent. It created no GPU commands or counter
sample buffers. Packet counter-capability is preserved with the other evidence.
This REVALIDATES the November8 observation (including its four-sample allocation)
in CURRENT-ultra-goal-experiments-2026-11-08.md; it is not a newly discovered
capability. No current renderer source uses stage-counter attachments.

The next useful gate is actual encode/resolve/calibration/pixel-equivalence for
two distinct physical passes using fresh sample indices. A private standalone
fixture is being prepared; no renderer hook or actual timestamp samples exist
at this checkpoint. Preserve raw GPU ticks and calibrate with paired CPU/GPU
samples. Metal CPU samples are already nanoseconds; use the Mach timebase only
to cross-check raw Mach clock brackets, avoiding an erroneous41.67× conversion
on this host. Reject unavailable/error values and do not sum overlapping
vertex/fragment/pass intervals into a claimed GPU busy time. The units and
conversion follow [Apple
documentation](https://developer.apple.com/documentation/metal/converting-gpu-timestamps-into-cpu-time?language=objc).


## Subsequent actual stage-timestamp gate

After the preceding checkpoint, root compiled and ran the standalone native
fixture with API and GPU validation, both actual exit0. Both validation layers
reported enabled. Source SHA256
`926da7bdeb9fbe2a3c8dc9bc81cb89d5e0ad1b0760800ca0d3a2394f132a1a36`;
binary `424df4992dd3e0f43e82a27bc6a19acfd380d95b5947d4ac540f132216afc0af`;
result `3833ed6662412a9a88872fe47c9475aaf9df357c310bc2b4d8c7371b2205a8b6`.
Actual encode/resolve produced eight valid samples across two physical passes
with indices0–3/4–7. Counted images matched both uncounted references and every
expected RED/BLUE pixel; the uncommitted cancellation retained prior output.
Paired CPU-nanosecond/GPU calibration and independent converted Mach brackets
passed. This device's measured conversion factor was1; raw Mach timebase was
125/3 and was not applied to Metal CPU samples. No global stage order, interval
union, callback retirement or viewer performance was inferred.

Packet `/private/tmp/alchemy-metal-stage-timestamp-validation-run-20261110.veybvjku`
is SHA-verified as the tenth preserved private packet, stage-timestamp-validation.
This advances beyond the earlier capability/allocation-only evidence. No viewer
renderer hook exists yet. The next work is a minimal explicit bounded timing
window/helper and tests, followed by default-off native-encoder and exact
frame/view/face integration under the post-formal-ready gate. Preserve all
rendering and avoid extra waits/submissions in the viewer.
