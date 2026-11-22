# PPD-567: completion-owned forward-probe publication

Status: local implementation, component checks, three qualified Club pairs, two
qualified Home pairs, bounded census and CPU/queue additions complete. The owner
explicitly approved committing and pushing this scope on 2026-11-22; visual and
performance acceptance remain pending. This report does not declare the
product release gates met. The measured redundant CPU cost is removed; an isolated
sustained FPS improvement is **not established** by these changing live scenes.

## Implementation and ownership

`renderOpaquePostDeferred()` already freezes a 49,216-byte forward-probe pack.
Its old slot1/slot3 consumers repeatedly submitted that same pack to the data-page
deduplicator, paying its content hash before buffer-binding caches could help.
The local patch publishes lazily at the first real consumer and binds the resulting
slice at every existing consumer and encoder restart within that invocation.

Deduplication remains enabled. Pack bytes, shader ABI/math, draw ordering,
per-view isolation, external FrameData and standalone command-completion epochs
are preserved. There is no global pointer cache, graphics switch, shader variant,
backend abstraction or worker-width change. Empty views do not publish a pack.
Original Alchemy `1eb44bbd1b` updates one current reflection UBO and binds it across
draws; Metal retains its immutable completion-owned pages rather than copying
the original mutable GPU implementation.

A read-only `LLAgent.getCamera` query is common to both new comparison builds.
It supplies actual request-time pose, axes, matrices, projection controls and mode;
it is not an atomic captured-frame record and adds no per-frame hook. Native
reverse-Z can have an infinite projection far plane. The private finite-only
reader preserves a partial snapshot for that one filtered scalar; all other
finite fields and the actual reverse-infinite matrix are checked.

## Builds and component verification

Both apps are Release 69452, built from ancestor `d219d538ab` plus explicitly
hashed local patches. The accepted Scheduling 69450 app is preserved separately.
The control contains the unchanged renderer plus the common camera observer;
the candidate adds PPD567. Do not identify either new executable as accepted 69450.

| Arm | Staged app under `~/Alchemy-candidates` | Executable SHA256 |
| --- | --- | --- |
| Control | `Alchemy Metal Probe Control 26.4.0d69452.app` | `381c23ff409992515792c447960fd2dedb635fa22a85839786d663882b680fe2` |
| Candidate | `Alchemy Metal Probe Publication 26.4.0d69452.app` | `1ded8f254087832c2d28d71084854eb1ff07474e545deaf211c8d890fffe099e` |

Full builds, strict/deep signatures, app verification of346 ARM64 Mach-O images
and matching dSYMs pass for both arms. Both use the same native metallib hash
`6fe71e2833331703acc4912f83f21c9e6ff0a14b7ef5fad70b1f75dccd688793`.
Actual startup logs identify OpenHTJ2K0.27.0 as the pixel decoder.

The new GPU case tests differing pending packs, both binding slots, caller-storage
reuse, same/separate commands, reverse commit order, external/standalone pages
and candidate/fallback paths against rendered controls. The old renderer fails
exactly eight repeated-full-pack reuse checks; the fixed renderer passes 51,279
assertions in that case. With API/Shader Validation, the transparency invocation
passes 1,825,343 assertions / 12 cases and related checks 581,147 assertions / 40 cases.
These are separate test invocations, not an assertion that every case is unique.

The tool suite passes 185 tests with one optional retained-fixture integration
skipped. Initial Python environments lacked PyYAML or NumPy; both failed runs are
retained. An isolated local test environment inherits the installed scientific
packages and adds pinned PyYAML6.0.3; existing environments are unchanged.

## Controlled comparison method and qualification

The protocol uses three Club pairs in AB,BA,AB order and two Home pairs in AB,BA
order. Every arm enters native full screen before login, preserves the same 66
effective controls, settles 120 s after the first startup-state24 FRAME and records
90 s of Overview CPU/GPU data. The fixed comparison window is world-age 150–200 s.
The initial 130–190 s declaration and its startup-margin amendment are retained;
the amendment preceded comparative cost inspection. There are no builds,
GPU frame captures, state queries or analysis jobs during measured intervals.

The first Club run is an excluded setup pilot: its requested pose matched but
visual qualification faced the adjacent storefront. The corrected entrance view
was qualified before comparisons. Actual camera matrices/pose are compared within
each scene; avatar landing position is recorded independently.

Qualification is per measure. Final command telemetry is lossy, so it cannot
support a complete command-stage budget. CPU/GPU exported timestamp coverage and
explicit consecutive FRAME iterations are checked independently. Native age and
cadence endpoints are exact Mach intervals; CPU/GPU selection uses UTC/header
mapping with observed clock-pair sensitivity, not microsecond frame attribution.
Overview GPU Active/Begin-to-End metrics overlap and are not occupancy or
additive frame costs. At least 300 native periods and drawable associations are
required for each final arm.

PPD577 preserves a premature-quit collector failure and an 18 s export timeout.
Separate offline exports subsequently qualify that raw recording with 90 s budgets;
the original failed session stays failed. PPD578/579 preserve and repair private
workload-summary and lambda-label errors. Corrected result02 retains identical
CPU/GPU/FRAME values while counting every observed metric row and using honest
inclusive labels.

PPD581 records the premature all-ten-qualified statement: reducer exit0 meant
analysis completed, while Club candidate03 explicitly failed cadence. Iteration6258
is missing inside the fixed window. Both arms of that third pair are retained and
excluded from final paired cadence; replacement04 uses unchanged rules. The final
comparison requires explicit qualification with no reasons, and rejects the actual
failed03 record. No app stall or exact telemetry-drop cause is inferred.

Replacement04 qualifies explicitly: 815 control and 803 candidate native periods,
zero window FRAME gaps, and sufficient CPU/GPU drawable associations. The final
gate accepts exactly six Club and four Home arms, each with Boolean qualification
true and an empty reason list. The original excluded pair and incorrect statements
remain in the measurement history. No rejection rule was weakened.

## Repeated results and limits

All values below use the fixed 50 s window. CPU-ms/period divides sampled CPU by
the count of eligible native periods; boundary-count approximation and sampling
make it an accounting estimate, not causal CPU attribution to individual frames.
The hash category deliberately includes other opaque-post constant/palette hashes.
The old setter/publish symbols can be inlined away; their absence is not used as
evidence of savings. The GPU byte-counter/output tests independently prove that
the full frozen pack is no longer republished per consumer.

| Pair | Control periods | Candidate periods | Main CPU ms/period, A → B | Broad hash CPU ms/period, A → B |
| --- | ---: | ---: | ---: | ---: |
| Club 1, AB | 756 | 807 | 65.49 → 61.21 | 2.881 → 0.0186 |
| Club 2, BA | 855 | 844 | 57.88 → 58.59 | 2.193 → 0.0213 |
| Club 3 replacement, AB | 815 | 803 | 60.64 → 61.42 | 2.940 → 0.0237 |
| Home 1, AB | 797 | 798 | 20.35 → 19.07 | 0.380 → 0.0175 |
| Home 2, BA | 798 | 794 | 19.82 → 19.26 | 0.449 → 0.0113 |

The broad hashing subtree drops 99.0–99.4% in the Club pairs and 95.4–97.5% in
Home. Residual palette/other-constant work is retained rather than called zero.
Missing main-thread stacks are explicitly accounted: at most 1 ms in any final
Club arm over its 50 s window, and zero in these Home arms. Inclusive categories
overlap and are never added into an optimization budget.

| Pair | Native mean period ms, A → B | Mean change | Native p95 ms, A → B |
| --- | ---: | ---: | ---: |
| Club 1 | 66.06 → 61.87 | −6.35% | 81.52 → 74.45 |
| Club 2 | 58.43 → 59.19 | +1.30% | 66.11 → 66.30 |
| Club 3 replacement | 61.22 → 62.21 | +1.62% | 67.93 → 75.92 |
| Home 1 | 62.63 → 62.58 | −0.09% | 75.06 → 74.66 |
| Home 2 | 62.55 → 62.80 | +0.41% | 76.19 → 76.28 |

Positive period change means slower. Club 1 improves, while Club 2 and replacement3
do not. The replacement candidate's p95 is 11.76% higher; this adverse result is
retained, not averaged away. Home mean cadence stays within 0.5%, with visually
consistent foliage, shadows, avatar and UI at the checked view. These are
instrumented app-loop results, not monitor scanout or an unprofiled FPS guarantee.

Camera, controls and build identity are controlled; world workload is not frozen.
In replacement3 the retained cumulative vertex-counter span changes from 44,465
full / 15,110 partial uploads to 55,185 / 30,340, and logical upload bytes from 2.765 GB to
3.598 GB, about 30% more. These counters count logical full/dirty-range CPU writes,
not unique geometry or GPU transfer operations. Mean observed point-light counts change 88.38 → 90.88 while
configured probe count stays 19. Texture worker-upload deltas instead change
669 → 349. These are first-to-last observed counter spans, not complete 50 s totals,
DRAM bandwidth or a causal normalization. They demonstrate unequal activity;
they do not prove which activity caused the slower tail or rule out regression.

The justified conclusion is narrower than a throughput win: the source contract
is coherent and its repeated hash work disappears, while live whole-frame effects
remain mixed. Retain owner testing as a separate acceptance step; do not claim
product FPS targets or compensate by changing quality, pool widths or budgets.

The final preservation audit checks 20 pre/post snapshots for the ten selected
arms: all 66 controls and per-scene finite camera matrices/pose/scalars match exactly.
Saved 18 explicit / 48 absent whitelisted controls have zero differences. FullScreen is
absent from both saved XMLs and reads 0 live; native full screen is independently
observed through AX and actual 3456 × 2168 FRAME dimensions. Whole settings XML hashes
change through session activity, so byte-for-byte preference preservation is not
claimed. Each app's post-run executable/metallib hashes match its own recorded
expected identity; control and candidate executables intentionally differ, while
their metallib is identical. Accepted69450 remains separately preserved.
AC power and 100% battery and sampled OS thermal state 0 are observed; this is not frequency, power,
temperature or continuous no-throttling evidence. Cross-clock observer sensitivity
is retained in each result; native cadence remains independent of that mapping.

## Bounded Home census and decision

An actual M1 Max API/Shader Validation kernel consumes the retained 55-probe pack,
candidate masks/grid, base constants, specular/normal/depth textures and packed
integer SH resource from the identified Home BASE draw. Verbatim current helper
bodies and a source-restoration audit retain traversal semantics. Deterministic
32×32 visible-pixel strata produce 1,024 records, 966 covered receivers, zero invalid
ambient outputs and zero ambient bit mismatches against the unchanged helper.

Nondefault accepts average6.740. Positive SH taps including default average7.740,
median8, maximum13. Default contributes to all 966 receivers. Of 25,049 containment
calls, 18,538 reject and 6,511 accept. There are no sampled manual contributions,
32-tap saturation or zero-weight/scale opportunities. Radiance uses a labelled
dummy: this is not full-lighting parity or a shader-performance benchmark.

Decision: **stop GPU kernel variants after this census**. It identifies work,
but no new redundant operation with a demonstrated beneficial transformation.
PPD353's corrected predecode regressions remain contrary evidence. No production
shader change or dormant experiment was added; PPD319 remains open.

## Exclusive Club CPU budget and worker progress

The separate accepted 69450 late Club window has43,429 main samples/43.429 CPU-s,
848 drawable associations and 51.213 ms sampled main CPU per association. Each sample
is assigned once to the nearest reviewed owner; the 21 categories reconcile exactly.
Registry/container work is 7.809 ms, encoding 7.044 ms, publication/source caches 5.751 ms,
shadow-owner remainder 4.802 ms, avatars 4.232 ms and geometry 2.822 ms. The old frozen-pack
hash leaf is 2.167 ms. These exclusive categories must not be added to earlier
inclusive scopes or treated as removable wall time. This is older baseline evidence,
not the new fixed-camera A/B control.

| Boundary | Evidence | Remaining decision |
| --- | --- | --- |
| General | One worker; 5 ms sampled CPU in the old strict late window | Category queue age and wall occupancy unknown; widening is not justified by nominal idle cores |
| Decode | Six named workers execute OpenHTJ2K; loading ticket age reaches 31.9 s, then clears | Late snapshots have zero waiting/active tickets; retain loading-versus-steady distinction and cancellation-during-permit-wait residual |
| Cache/fast cache | Normal cache completions route to fetch; bounded 64-slot asynchronous lookup retains main publication | Fallback and completion-to-consumption ages remain unmeasured |
| HTTP/ready results | HTTP callback servicing remains frame-triggered; ready decode identities have a bounded priority drain | Source dependency is established, critical-path delay is not; measure request generations across handoffs |
| Main replies | Live regular allowance plus safe pacing work exists | Queue age and useful work around both waits still need correlation |
| Native upload | One ordered owner executes real worker uploads/commits | Preserve ordering; measure denied credits/queue age before separating CPU preparation or changing budgets |

Keep pool widths, retained-data budgets and encoding ownership unchanged in this
patch. Next CPU work should select a measured registry/encoding/publication
suboperation with immutable inputs and include dispatch, synchronization and
publication in its full cost. PPD534 already removed eager owning Use construction
on registry hits; that repair must not be diagnosed again as missing. For asset
latency, reuse current decoder age evidence and add only missing generation-aware
handoff timestamps for a demonstrated late request. No architecture change is
selected merely because a worker exists.

Private reproducible evidence is under `artifacts/metal/ppd567-20261122`:
`identities`, GPU/build logs, `census`, `analysis-cpu-queues`, corrected comparison
outputs and preservation audits. Failed attempts and excluded runs are retained.
The active ledger and defect register own subsequent retests and next actions.

## Owner review and publication authorization

After receiving this report, the owner explicitly requested committing and pushing
all changes in the reviewed scope. This authorizes source/report publication; it
does not establish visual acceptance or a sustained FPS improvement. Private
recordings, account data and unrelated local worktrees are excluded.

The exact candidate above is running normally in native full screen at Club for
owner testing, with no recorder, LEAP helper or automatic GPU capture. This final
launch selected **My last location** as the owner instructed and the actual Club
region was visually verified. Home regression evidence is the qualified comparison
series above, independent of this normal launch.
