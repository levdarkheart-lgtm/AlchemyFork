# Current engineering state — production maintenance consolidation

Observed 2026-11-22 on Apple M1 Max /64 GiB /macOS26.5.2 /Xcode26.6.
Usual branch: `codex/apple-silicon-metal`; production base `d191bb062f`.
**Renderer code checkpoint:** `ec696aa615` (unchanged rendering implementation).
The owner authorized committing and pushing the production fixes and task
reconciliation to the usual branch, excluding experiment code, one-off tools and
new instrumentation. Publication does not establish visual/performance acceptance.

## Retained production changes

- PPD587/588/589: bounded HTTP completion servicing wakes the existing fetch owner
  independently of rendering; nonthreaded service is exact-once and exceptions
  retain response ownership. Measurement-only timestamps/histograms are excluded.
- PPD582: the existing GPU regression suite now covers actual above-water fragment
  slot3 and alpha1→water3→alpha1 restoration across pending immutable packs.
- PPD586: fixed octree diagnostic live-child bounds and overflow rejection; ordinary
  and macro-enabled regression coverage are retained.
- PPD585: camera API documentation states request-time, non-atomic values, radians
  and potentially infinite projection far distance. Behavior is unchanged.
- PPD590: existing harness distinguishes external evidence URLs from local paths
  and commands; three regression cases preserve local path/executable checks.
- PPD584: current task/register fields reconciled against source and accepted
  history; accepted completed/removal work is retired from implementation lists.
  Original IDs, failures and histories remain. Missing proof is identified by ID.

The [maintenance record](../research/production-maintenance-20261122.md) owns scope
and validation; [canonical work](../work/active/metal-port.md) owns remaining work.
The [truth audit](../research/register-truth-audit-20261122.md) preserves the broader
reconciliation. The separate [review branch](https://github.com/levdarkheart-lgtm/AlchemyFork/tree/a6089d691c113b9e6bc66a09f6b65b020e70960f)
retains experiment evidence, failed trials and research-only tools. They are not
installed capabilities or outstanding implementation requests on the usual branch.
No new device/frame/input/presentation/memory telemetry, extended capture limits,
metric manifest or Python analysis bundle is included. Main-group count and adoption
order are unchanged. No shader, graphics-setting or worker-width experiment is added.

## Runtime and validation

The previous review candidate was closed through normal UI Quit at owner request;
PID98258 is absent. Keep the viewer closed while the owner is away. That candidate
contained review-only instrumentation and is not the cleaned production build.
OpenHTJ2K0.27 remains the sole texture pixel decoder; OpenJPEG header parsing and
encoding remain separate. No new decoder or runtime collection is requested.

Clean Release build, doctor, strict owned-GL source audit and app verification all
exit0. All346bundled Mach-O components are ARM64. Focused native CTest passes6/6,
including real water coverage with Metal API+Shader Validation, queued-thread, HTTP
and normal octree tests. Full maintained tool suite passes188tests/1optional skip;
the harness passes33required files. All589prior histories remain intact; register
now includes590/next591. Independent source/harness reviews found no material issue.

Clean build: `build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/Alchemy Metal Scheduling.app`.
Executable SHA256 `8f8f59babd642d40f8797184cd2f34212041fc5d347575c053a294fb2b1fe99f`;
UUID `7C472792-D5A5-388D-8071-C9FE2B0914EE`. It was built from the production base
plus the17-path source patch, not launched or staged over the review candidate.
The exact commands/results and source hashes are in the maintenance record/private
`production-maintenance-20261122` evidence. Prior macro-enabled octree ASan/UBSan
proof applies to the identical retained source; ordinary CTest is macro-off.

## Performance disposition

Home FPS remains unresolved (PPD319). The measured third-group trial worsened tail
cadence and moved blocking; earlier adoption showed no material repeated frame gain.
Both are excluded. The HTTP measurements support lower handoff-stage delay; unequal
live workloads do not establish whole-loading or FPS benefit. Detailed evidence is
preserved on the review branch rather than copied into an active experiment plan.

## Complete-product advancement assessment

| ID | State | Remaining boundary |
| --- | --- | --- |
| M-008 | doing | Broad feature/visual parity and representative performance. |
| M-011 | doing | Runtime/foreign-process, reproducible release and strict no-GL qualification. |
| M-012 | complete | Maintained harness/build/verification tools. |
| M-013 | doing | Retained Alchemy/RLVa/media behavior and prolonged stability. |

Required independent build/review evidence and release gates remain open. Local
component success does not establish product completion.

## Exact next actions

This maintenance scope is implemented, independently reviewed and validated for
the owner-requested publication on `codex/apple-silicon-metal`. No experiment merge
or viewer launch is required. Next engineering is output-preserving required
lighting/forward-alpha GPU work (PPD319), including complete affected-pass/frame
cost. Do not reactivate the closed queue-depth or tiny adoption trials.
