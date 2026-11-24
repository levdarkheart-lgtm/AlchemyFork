# Active Metal work — optional MetalFX spatial prototype

Commit `3df7055a9d` is the clean implementation base; feature changes pending.
PPD591 is the owner-requested active scope on `codex/metalfx-spatial-20261124`,
pinned from usual-branch7911e382ce. Control f384e2c220 and implementation base
3df7055a9d have identical gated quiet-output observations. Earlier research stays
separate. Native remains default; combined lighting OFF; no push authorized.

## Immediate executable action

Explicit fixed-half scene/output/shadow-reference planning is implemented and its
CPU coverage passes1942assertions/28cases. Native MetalFX integration and source
review are underway. Correct final presentation acquisition before opaque MetalFX
encoding and include both used lazy PSOs in staged reload. Finish clean candidate
build and focused GPU tests, then qualify actual Native/Half-linear/MetalFX output,
net world throughput, still/motion quality, UI, resize/native return and existing
snapshot paths. Preserve original global controls/shadow budget and restore settings.

The [implementation record](../../research/metalfx-spatial-20261124.md) owns concise
results; [CURRENT](../../state/CURRENT.md) records observations. No new live result is
claimed. Previous maintenance below is retained history, not this feature's proof.

## Remaining engineering

| Priority | Requirement | Concrete next boundary |
| --- | --- | --- |
|1 | PPD319 required GPU shading | Optimize measured main lighting/forward-alpha cost while preserving output. Include preparation/upload and complete pass/frame cost. Queue-depth and tiny adoption trials are closed. |
|2 | General service | Measure category queue age and establish ownership before separating file/audio/CPU work. Invalid widths are already repaired. |
|3 | Geometry/scene preparation | Separate immutable preparation from required lifecycle work before imposing a combined deadline. API renaming did not enforce one. |
|4 | Safe main replies | Correlate queue age with required-main and end-pacing waits; consume at a safe owner boundary. A live setting did not unify wait accounting. |
|5 | Upload preparation | Identify conversion/alpha/mask cost, denied credits and backlog age while preserving the single upload owner and memory/progress bounds. |
|6 | Pipeline/archive | Remaining serialized compile/archive ownership; required point pipelines are already eager. |
|7 | Retained incidents | Current recurrence and cause for002/015/311/319 and loading/appearance reports. Accepted maintenance does not establish every incident resolved. |
|8 | Product qualification | Reference-backed feature/visual parity, prolonged stability, representative performance, runtime/foreign no-GL and reproducible release. |

## Completed boundaries and retained history

PPD582/585/586/587/588/589 production repairs and PPD590 existing-harness repair
are implemented and qualified, with owner acceptance
separate from code/test completion. PPD584 reconciles current fields against source
and accepted history. Accepted Maintenance69445 and Scheduling69450 remain preserved;
completed preference, asset and abandoned-path repairs are not active requests.

The [truth audit](../../research/register-truth-audit-20261122.md) retains the original
211field corrections/44scoped accepted-removal closures and74specific missing-proof
actions. Those74 are evidence/acceptance gaps, not74 newly confirmed source bugs.
The [register](../../POST_PORT_DEFECTS.md) retains all IDs, failures and owner decisions;
resolve each remaining gap only from source and actual evidence.

[Review findings and measurements](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/a6089d691c113b9e6bc66a09f6b65b020e70960f/docs/metal/research/review-home-admission-20261122.md)
remain historical evidence. New native telemetry, input/memory/admission helpers,
metric manifests and study-specific tools/tests remain review-only. PPD583 is
excluded by owner direction, not an active usual-branch implementation task. Failed
experiments are not production capabilities or future tasks to reactivate.

Home FPS is not fixed. The [published PPD567 record](../../research/ppd567-performance-20261122.md)
retains mixed cadence and adverse Club p95; lower redundant CPU work alone does not
establish a viewer FPS benefit.

## Complete-product checkpoint gate

| Product ID | State | Remaining boundary |
| --- | --- | --- |
| M-008 | doing | Broad feature/visual acceptance and representative performance. |
| M-011 | doing | Runtime/foreign-process and reproducible release qualification. |
| M-012 | complete | Maintained harness/build/verification tools. |
| M-013 | doing | Retained features and prolonged stability. |

## Non-negotiable open gates

Cancellation/request identity, generations, native completion retirement, memory
bounds and retained features/quality remain invariants. Owner publication approval
is scoped to this maintenance; it does not waive product or release gates. Required
independent build/review evidence remains. A bounded candidate never implies product completion.
Do not mark feature parity from build/test success alone.
