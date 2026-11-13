# Active goal — complete the Apple-first renderer architecture implementation

Status: doing. Owner authorization: 2026-11-13, continue original architecture
through all remaining goals, with one combined viewer test afterward. This
supersedes the previous per-goal owner-runtime stopping points. No agent viewer
or native-window/audio-device launch, login or new interactive profile/capture.
No push, publication or silent feature/quality reduction. Target next morning;
do not substitute that target for actual completion evidence.

Branch `codex/apple-first-architecture-g2-g5`, isolated worktree
`/private/tmp/alchemy-apple-first-architecture-g2-g5`. Baseline
`5836d41753fc3e09533d638748ac9756f89ac357` includes G1 immutable attachment
records and the confirmed `fef1e756fd` exception-ABI repair. Owner reports the
crash gone and another chat is preparing a five-minute G1 profile comparison.
Its checkout `/private/tmp/alchemy-apple-shadow-records-g1` is read-only here.
Preserve that session's files, profiling app, captures and baseline receipts.

## Deliverable and real execution boundaries

Implement the [remaining architecture route](../../research/apple-first-implementation-route.md)
in actual viewer producers/consumers, not test-only scaffolding or an alternate
engine. All application-owned rendering stays direct native Metal, ARM64/macOS
only. Retain Second Life scene semantics, main/probe/hero views, rigged bodies
and attachments, PBR/alpha/terrain/trees, queries, UI, media/voice and asset work.

| Goal | State | Concrete acceptance |
|---|---|---|
| G2 retained native source inputs and single per-view admission | doing | Generation-qualified native geometry/material/coverage inputs; one ordered admitted view list replacing duplicate count/source discovery; exact family/query/completeness/failure semantics; mutation/unload/origin/pose lifetime tests and complete-cost comparison. |
| G3 bounded detached CPU preparation | todo | Immutable job inputs with explicit view constants and resource leases; useful chunks on bounded Dispatch; no live gGL/sCull/avatar getters or shared mutable arena/cache on workers; ordered owner publication, cancellation/epoch tests, latency/overhead comparison. |
| G4 persistent GPU-produced visibility/indirect execution and useful Metal 4 | todo | Production native persistent tables/draw inputs, GPU visibility/compaction including rigged/masked families, conservative bounds/order/query preservation; capability-tested Metal 4 work where supported and measured; direct-vs-new GPU output/lifetime/CPU+GPU checks. |
| G5 UMA publication and frame/pass scheduling | todo | Changed-range immutable shared native publication with CPU queued/encoded and actual GPU completion leases; no redundant world snapshots; bounded ready/in-flight work/bytes; measured pass-lifetime/load-store/pacing changes with correctness and cancel/reload/resize tests. |
| Integrated delivery | todo | One separate Release app with all retained changes enabled, exact source/flags, doctor/build/ARM64/signatures/native imports/exception ABI and headless correctness+comparisons; preserve every adverse result and state runtime gates unverified until owner tests. |

Success requires source-path integration and measured bounded correctness/cost,
not nominal CPU/GPU utilization or a new API object merely being constructible.
Existing direct Metal specializations remain when hardware/API constraints
require them; no fallback graphics backend, Vulkan/translation layer or generic
cross-platform renderer is introduced. Existing rejected parallel-encode,
matrix-memo and draw-joining trials remain evidence against repeating them.

## Ownership, status and evidence

Root owns `pipeline.cpp`, shared CMake, source admission/integration, canonical
state/ledger/register and final build. Subagents receive disjoint helpers/tests
and explicit interfaces before editing shared classes or shaders. Begin with
source/API audits, then implementation immediately; preserve independent work.

Evidence root:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-architecture-20261113/`.
Record commands, exit codes, clean code SHA or exact diff hashes, compiler flags,
input parameters, output parity, counters and raw timing data. Validation-enabled
correctness and validation-disabled timing are separate. No headless sample is
a measured viewer frame or proof of in-world FPS, latency or full scene parity.

PPD-089 single-use cost remains open for actual admission/reuse policy work.
PPD-090/091 repair is retained; owner now confirms the crash is gone. Record that
confirmation here without modifying the other session's checkout. Register any
new defect before repair, preserve failed tests/reverts and do not infer push
permission from owner runtime confirmation.

Next action: implement G2 at the concrete `countMetalShadowBatches` /
`captureMetalShadowScene` source boundary while independently establishing
native store leases and the G3/G4 input contracts. Continue across subsystem
boundaries until the integrated app is ready; do not stop for another prompt.
