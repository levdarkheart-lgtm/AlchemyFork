# Apple-first engine takeover — 2026-11-16

The persistent objective remains the integrated Apple-first Second Life engine,
with better whole-viewer performance and preserved behavior. Architecture and
performance acceptance remain partial. This is an independent takeover of the
experiment lineage at `467ae911cbeeba705ba37359565cec85855fc4a8`, retaining the
owner-tested `350b803597` / 69172 app and all existing evidence.

## Observed evidence and limits

The [successful Overview assessment](integrated-build-69172-overview-quiet-club-20261116.md)
contains two valid >300-second CPU/GPU recordings. The prior oversized trace was
owner-discarded; its jobs are no longer running. No recovery of the discarded trace is authorized. Initial takeover also prohibited
new viewer launch/login/profiling; subsequent owner checkpoint steering below
authorizes bounded runtime evaluation. Push remains unauthorized.

Independent audit confirms 14.849/20.706 FPS layer cadence, main running CPU
28.57%/62.12%, and persistent publication/cache/encoding work after club decoding
settles. CPU samples contain **Running only** (`all-thread-states=NO`); blocked
wall time is absent. GPU metrics are aggregate in-flight/layer observations,
without per-pass execution or drawable-wait tables. Consequently neither pure
CPU saturation nor pure GPU saturation, admission-wait incidence, frame-tail
percentiles, or a causal old/new regression is established.

The prearchitecture 68841 busy capture reports 35.57 FPS but lacks a matched
settings/resolution/avatar contract. The closer 68991 → 68995 → 68999 sequence
reports 20.3639 → 17.7729 → 20.3569 FPS, with source-capture CPU 2.221 → 7.367 →
6.832 ms/present and different live source populations. Preserve these as
historical evidence; do not claim a matched percentage regression or recovery.

## Actual architecture boundaries

| Boundary | Implemented | Remaining / implication |
| --- | --- | --- |
| Immutable scene/resources | Producer invalidation, shared source/cohort/NativeTuple ownership and static whole-range consumption across main/probe/shadow | Cold source sealing is still view-demand driven. Rigged, blended, animated UV and special paths still need per-view preparation. Native residency churn can recreate publication work. |
| CPU preparation | Bounded owned shadow encoding on large views, immutable inputs and completion-safe command settlement | Main light worker and newer general preparation are disabled; tiny-work tests did not justify activation. Core utilization is not acceptance. |
| Native visibility/commands | Native cohort/bank/visibility implementations and real correctness/cost checks | Actual runs are small; the opaque 2048 threshold selects zero runs. New banks/visibility remain off after contained regressions. GPU discovery is still downstream of CPU discovery. |
| Unified-memory scheduling | Shared optional upload/probe/preparation/publication/maintenance credits; completion-owned optional retirement | Required-main command admission is separate; main GPU debt does not feed the frame plan. Cache and arena byte budgets remain independent. |

The exact source map is in `pipeline_scene_publication_native.inl`,
`pipeline_group_publication_native.inl`, `pipeline_group_capture_native.inl`,
`pipeline.cpp`, `almetalworld.cpp`, `alnativeframeplan.cpp` and
`alnativeframebudget.cpp`. No feature or architecture boundary is declared
complete merely because a factory or test exists.

## Immediate implementation

**PPD-315**: three early main transactions currently share a two-command cap.
A pending G-buffer and lighting command can make post wait for the same frame's
G-buffer. Implement explicit main-only logical groups with two pending groups
and three committed transactions per group, preserving early commits, special
view policies, ordered queue execution, resource ownership and failure handling.
Use event-gated native tests to verify overlap, bounds and output/lifetime
semantics. This repairs a source-confirmed path; live incidence and FPS benefit
remain unmeasured. Add cumulative older-group wait diagnostics so a subsequent
normal owner run can distinguish this policy's waits.

**PPD-316**: subscribe one relay once per immutable Signal identity while keeping
every ordered topology role. Independent actual-body CPU red/green gives eight
baseline assertion failures and 318 assertions / nine passing candidate cases;
the production test translation unit also compiles. The complete Signal::observe
club cost is only 3.777 CPU-seconds / 300.737 wall-seconds, of which only part is
redundant. This is bounded secondary work, not an FPS explanation.

Separate clean takeover worktrees protect unfinished earlier-session edits.
Root owns pipeline/CMake/integration/docs; native admission and dependency work
have exclusive file ownership. Existing staged apps and private evidence remain
untouched. Further work targets shared dirty-publication/restoration costs and
useful small-run command preparation rather than forcing rejected variants on.

## GPU diagnosis when subsequently authorized

Installed Xcode supplies Metal System Trace, Game Performance Overview and Time
Profiler. Existing `MTLCaptureManager` whole-device interval capture and bounded
stage timestamps already exist in the viewer. No new capture framework is needed.
The M1 Max stage hook provides timestamps, not hardware occupancy/bandwidth.

Use a bounded 2–5-second detailed timeline to correlate thread states, CPU commit,
GPU begin/end and drawable/present waits, plus one complete-display `.gputrace`
including ordinary probes. Use Xcode pass/shader/counter inspection to diagnose
expensive work. Counter replay changes overlap; validate improvements with
uninstrumented matched whole-viewer measurements. Actual capture coverage and
finalization limits must be verified; a recorder's requested duration is not proof.
See Apple's [Metal performance workflow](https://developer.apple.com/documentation/xcode/analyzing-the-performance-of-your-metal-app/),
[programmatic capture](https://developer.apple.com/documentation/xcode/capturing-a-metal-workload-programmatically)
and [counter statistics](https://developer.apple.com/documentation/xcode/analyzing-apple-gpu-performance-using-counter-statistics).

The successful collector is still private/ad hoc; maintained tooling should
acquire explicit PID/source identity, bounded recording/finalization/export,
actual coverage and failure receipts. Existing continuous-frame work PPD-314 is
preserved in its isolated worktree; it has not been resumed or integrated.

## PPD-317 — shared CPU recipe publication follow-through

The native source cache currently discards CPU preparation facts together with
optional native leases. Interior same-revision-expired getter deltas are
5,626,666 quiet / 10,870,652 club, against 1,112 / 29,080 revision-change calls.
These are repeated calls, not distinct producers or physical allocations.
Club `world()`18.010CPU-s, `publish()`13.977, native `retain()`7.681,
`remove()`2.262 and `buildWorld()`2.177 overlap. Both source and measured stacks
justify a shared restoration boundary; no frame-time saving follows from their
sum. A bounded producer-owned CPU recipe should survive native-cache withdrawal,
then restore exact weak generations across main/probe/shadow without repeating
live material/texture walks. Producer writes, expired generations and unsupported
families must retain the existing current-input path. This work is independent
of the running frozen290c25ea60 admission/dependency build and remains unverified.

## Frozen first integration validation

`290c25ea60c46b2f522259ee8631b84a3897d98c` builds as 26.4.0.69181.
Doctor/configure/full build and exact static ARM64/signature/import/retained
feature/provenance verification exit0. Production dependency topology427/9,
main lighting5627/5 and common budget/working-set6112/52 pass. The four new
admission cases first execute with1136assertions/1failure: three cases pass;
reload fails its artificial five-second gate. Source review proves reload must
commitAndWait eager lookup/fallback uploads on the same queue. Corrected test
releases the artificial gate before reload and still verifies receipt transfer
and retirement. Corrected runtime execution remains pending. Preserve the
initial failure; no production workaround or expanded timeout is applied.
Private exact commands/hashes are under `artifacts/metal/engine-takeover-20261116/`.

Main admission ledger counts are pruned at later owner entry and can include
already-completed commands. They must not be interpreted as live GPU debt.
The independent frame-budget audit found no observed window-capacity-full
samples in69172, so no new probe/upload throttle is justified from those
counters. Shared producer recipes are the next supported implementation work.

### First recipe cost observation — broad SIMPLE route not selected

Seven actual-body cases pass332assertions under native validation. The initial
five-cell fixture reports native-denied4source/3view SIMPLE owner preparation
7.34795→8.39605µs (+14.26%). Its settled render/readback998.617→992.056µs
cannot establish a viewer gain. Cold/warm/protected-denial owner differences are
small; PBR producer churn50.0272→51.3166µs also needs qualification. Preserve
these adverse observations and narrow the candidate instead of enabling every
restoration path. Timing is elapsed owner wall, not sampled/thread CPU. Initial
cost1 retained medians only; raw per-iteration observations were discarded and
cannot be reconstructed. The already planned follow-up must persist them.

## Selected source and recurring owner checkpoints

Full world/material restoration proposal `eb7d7aac13c142ab57e57ff2ad7af64ee2dcadc2`
is retained on `codex/source-recipes-full-proposal-20261116`. The selected
follow-up `49a31097de6839ff9d1fd3e8c457caa3b260f48e` removes those unqualified
world/SIMPLE/opaque routes. Only covered shadows, including rigged variants,
use the new weak CPU recipe. Main/probe world publication keeps its existing
path and coexists with the covered recipe. Shared CPU bounds/group priority,
current producer/pose/UV/resource and native completion semantics remain.

Selected actual-body335assertions/7cases pass with native validation. Exact
selected byte-pressure qualification has192raw iteration rows and16counter
receipts: every arm72byte rejections/64residententries/262144logicalbytes,
without entry/oversize rejection or eviction. Owner wall5.99605→4.78750µs
(-20.156%,8/8paired-block wins); settled957.90205→932.22710µs
(-2.680%,6/8wins). This is a tiny contained native workload, not a viewer FPS
result. Earlier adverse, invalid and unselected runs remain separate. Exact
receipts/hashes: successful69172 Overview private `ppd317-source-recipes/`.

Owner steering now authorizes a repeated validation cadence after several
coherent implementation groups: pause code changes, verify/stage the combined
app, launch its exact executable, let the owner log in, then always sample quiet
in front of home → teleport/loading into the busy club → settled club after
one to two minutes. Keep lightweight CPU/GPU/log/position capture continuous
across the transition, with explicit timestamps and phase markers. Use brief
detailed GPU/thread timelines separately, with byte/time/finalization limits;
never an unrestricted multi-minute detailed recording. Owner drives login and
teleport. No push or private-evidence upload is authorized. Assess each group
before choosing the next. Architecture and performance acceptance stay partial.
