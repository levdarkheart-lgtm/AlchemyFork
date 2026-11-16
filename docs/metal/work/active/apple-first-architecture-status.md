# Apple-first architecture implementation status

Updated2026-11-16. The original whole-engine architecture is **active with runtime-backed follow-up work; acceptance remains partial**.
The owner has now supplied quiet-house and teleport/club runtime samples; see
[the two-scene analysis](../../research/integrated-build-69122-quiet-club-20261115.md).
Detached render-preparation counters stayed0 in both, so useful parallel-render
benefit is unproved despite implemented callsites. Cache turnover also needs
Architecture Session follow-through. Earlier blocked status below is historical. Goals1–5 landed ownership/admission foundations, but ordinary-scene useful parallel rendering was still missing in69122. The earlier
[source audit](../../research/apple-first-engineering-delivery-20261115.md) did not establish that outcome; treating the remaining work as only owner acceptance understated the implementation gap.
Producer refresh and upload follow-through is delivered in69134; production owned-view capture/encoding overlap is now integrated in source, still defaultOFF and awaiting the current build/correctness/complete-cost gate. No whole-viewer performance gain or completed release gate is claimed.

Owner-profiled source: **beb6837b71f0429f116fef8e2ad7cc5d166bc010**, build
**26.4.0.69122**, experiment branch `codex/apple-first-native-cohort-consumption`.
Doctor/configure/build, ARM64/signature/import verification and strict source
audit pass. All34 renderer scopes pass on these exact binaries:254120 assertion-
executions/346 case-executions. All four48-condition animated cost comparisons
pass their execution/parity/accounting checks; that does not mean they are faster.

| Boundary | Integrated behavior | Selected configuration / remaining acceptance |
|---|---|---|
| Shared scene/resources | Producer generations, immutable material/resource tuples, dirty subscriptions, retained static groups, exact native leases, shared main/shadow/probe capture. | GroupSources and two-frame stability ON; owner loading/teleport/rebase parity pending. |
| CPU preparation | Immutable owned inputs, bounded shared credits, joined large-shadow preparation, scalar recipes, fairness and recovery. | World workers OFF after measured completion regressions. Owner frame-tail benefit pending. |
| Animated ownership | Shared attachment records and view-specific system-body palettes; retained regular-rigged direct and GPU consumers preserve current poses, masks and queries. | Optional animated direct and GPU modes OFF: direct near parity; GPU commands/visibility cost more. Owner animation/stability acceptance pending. |
| Visibility/submission | Persistent static/animated tables, ordered native commands and conservative visibility; exact query/raster splits and completion ownership. | Optional banks and CPU/native visibility OFF after unfavorable or incomplete complete-path gates. Existing native shadow specialization retained. |
| Unified memory/admission | Changed-range/COW storage; shared CPU/native/decode/upload/probe/maintenance admission; completion retirement and bounded caches. | Owner physical working-set, loading and long-session evaluation pending. Cache ledger scopes overlap and are not total RAM. |
| Terrain/probe lighting | Terrain sharing selected at measured16-patch threshold; eligible probe/base lighting fusion and empty lighting-work avoidance. | Integrated; owner whole-engine evaluation pending. |
| Combined candidate | One source72 app includes these foundations and all experimental consumers. Separate architecture-combined-69122 staging is VERIFIED:8273 inventory nodes match the built app; signatures/ARM64/imports/features and evidence hashes pass. | Ready for owner evaluation through the exact launcher linked below; original apps and69102 checkpoint preserved. |

## Measured selection

- Accepted retained scalar/cohort path: source55 warm-many CPU8/8 cases faster,
  median28.05%; settled completion7/8 faster, median14.60%. Cold debt remains
  recorded. This is a contained path comparison, not an FPS improvement.
- Source72 animated direct: warm64 owner CPU+0.20%/+0.27% for one/three views;
  arena publication bytes0.772x/0.755x. Cold CPU−0.50%..+2.16%; resource churn
  +0.12%..+3.27%. This is near parity with reduced publication bytes, not a
  consistent net speed gain or a physical RAM measurement; keep OFF.
- Source72 full-count animated GPU commands: warm64 settled completion
  +11.56%/+8.96%; visibility adds further overhead. Keep both OFF.
- CPU static visibility helps outside-bound workloads but adds all-visible/cold
  overhead; world workers and small terrain sharing also lost their cost gates.
  Those decisions are retained rather than forcing more hardware activity.

All four animated comparisons retain1/2/4/64 sources, one/three views,
cold/warm/pose/resource churn, denials and loading samples.96 indices/source is
an explicit fixture limitation; saved club data does not establish per-draw
geometry sizes. Exact receipts and failed attempts remain in the
[execution ledger](apple-first-direct-cohorts-20261114.md) and PPD287–293.

## What remains

Finish and validate useful owned CPU/native overlap on the actual small-view workloads, with original command admission, query/projector settlement, lifecycle joins and retained CPU-byte bounds. Keep rejected small-family preparation and SSAO fusion evidence; select work only on complete-path results. Then evaluate the combined viewer with the owner across busy, quiet, loading, turning and teleport scenes: frame times/tails, input latency, physical memory, stability and retained SL features. Any reproducible failure creates a concrete repair item. Whole-viewer performance,
feature parity and `VALIDATION.md` release gates remain open. No viewer launch,
login, live profile, push or release was performed in this continuation.

## Exact delivered candidate

[Launch Apple First Combined.command](</Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-persistent-native-scene-20261114/architecture-combined-69122/Launch Apple First Combined.command>)
selects the measured policy on the separately staged26.4.0.69122 app.
`architecture-combined-69122/provenance.json` is verified;8273 file/mode/symlink
nodes match the built source app, executableSHA256
`101a327c68852a44e2ba435cd0744a9beaf4582e511fe16b239f171069c57428`.
Staged static verification and SHA256SUMS recheck pass. The app and launcher were
not executed. Evidence root is the canonical checkout’s ignored
`artifacts/metal/apple-first-persistent-native-scene-20261114/` directory.

[Owner run card](apple-first-owner-evaluation-69122.md) pins69122 and the previous
native69085 trial, settings/workload context and outstanding whole-engine evidence.

Post-delivery audits02/03 revalidated the clean3d06bdb708 worktree, verified
source72 candidate and missing runtime acceptance. No independent mandatory
implementation/staging work was identified. Completion is unproved; the full
objective is retained. Next action is owner combined-viewer feedback or
subsequent explicit runtime authorization under the existing goal restriction.
Do not rerun passing checks or invent additional phases to fill this dependency.

Latest owner-authorized samples:301.061s quiet and300.725s teleport/club, both
recorders/offline analyses exit0, owner closed viewer. No ERROR/FATAL rows,
24/129WARNING inactual windows; PPD294 cleanup observation nowregistered.
No source/app/settings change or push. Testing Integrated Build is handing the
report to Architecture Session for implementation decisions. Different scenes,
aggregate means and no old-build paired run do not prove an FPS gain or release.

## Active follow-up after69122 runtime evidence

The house/club recordings expose native-cache turnover240/274 and no exercised
contribution from the two detached renderer preparation mechanisms241. The old
blocked-runtime handoff above is historical; the analysis arrived as78edcee323.
PPD295 now investigates bounded local resource staging before native byte
admission. Caps, source generations, required readers and completion retirement
remain unchanged; this does not repair the whole cache policy or prove FPS gain.
Independent G3 source work examines coalescing already-owned small family inputs
within a view, preserving order and bounding all copy/job/join costs. No blind
threshold reduction or forced worker activation. Tested69122 stays immutable;
no further launch/login/profile/push authorized by the testing handoff.

## Previous implementation candidate —69129

Source3841527bbd944f9a6283b12ad45acdaa878d073a adds one native-resource vector
reservation at the first validated unique append. Selected after actual-source
retention-cost comparisons; inline staging rejected for its cold1regression.
The selected change reduces vector growth, keeps one scratch allocation on
denial, and can retain extra weak-vector capacity for duplicate-heavy callers.
It is a partial bookkeeping mitigation under PPD295, not a cache-policy repair
or whole-viewer FPS result. All caps and renderer settings are unchanged.

Doctor/configure/build, static native/strict and full34renderer scopes PASS:
254316 assertion-executions/348case-executions. New separate
`architecture-combined-69129` provenanceVERIFIED,8273 identicalappinventorynodes,
staged static verification andSHA256SUMS PASS. [Exact launcher](</Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-persistent-native-scene-20261114/architecture-combined-69129/Launch Apple First Combined.command>)
is unexecuted; profiled69122 app is preserved. Wholeengine/G3/performance
acceptance remains partial and active. Next priority is actual owned-work
granularity and retained-source churn, guided by the captured CPU scopes.
Probe tile fusion already covers all successful sampledprobeviews; no further
safe probe-pass removal was established. No new live capture is authorized.


## Latest integrated candidate —69134

Renderer source `a5ea338f22d2ef3b419e6ff47c3cc7cb625695ce` includes two
runtime-guided changes across scene publication and texture ingestion:

- PPD296 reuses dependency subscriptions when content changes but the exact
  State and ordered signal identities are unchanged. Actual notifications and
  source revisions remain authoritative. The48-cell component comparison shows
  changed-content savings65.55–99.83% with no allocations. Low-fanout cold and
  topology replacement remain slower; there is no universal speedup claim.
- PPD297 uses ARM64 interleaved vector loads/stores for U8 RGB8 storage
  expansion, preserving RGB/RGBA/BGRA bytes, opaque alpha, padding and tails.
  All120 paired component medians improve. Fresh4096² conversion changes
  58.115ms to2.665ms; this excludes viewer/GPU work and does not establish FPS.

Periodic diagnostics report topology refresh/reuse/subscription/failure counts
and RGB CPU-stage calls/pixels. The latter counts completed CPU staging before
GPU commit; retries, mips and subregions are not unique images. Both diagnostics
are compiled into the exact app; runtime frequency remains unknown.

Doctor/configure/build and static ARM64/signature/import/features checks pass.
Strict source:3595files,0blockers,0errors. Full34 renderer scopes pass with
Metal API/GPU validation:1,130,416 assertion-executions/349 case-executions.
Additional source-specific scopes pass: dependency378assertions/7cases,
image/upload6464/34, corrected segment876100/1. These scopes overlap and should
not be added as unique coverage. Optimized/sanitized kernel checks and original
failed attempts remain in the component receipts.

The corrected mixed-family matrix passes108 configuration triples/8748 raw
samples, with0/108 median wins for either aggregate preparation route against
current scalar preparation. Dispatch median overhead at867/1158draws is
45.16%/40.89%. No threshold/queue/feature change follows. Encoding/GPU and
compact-certificate validation savings are excluded; existing large-family
specialization is retained.

[Launch Apple First Combined.command](</Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-persistent-native-scene-20261114/architecture-combined-69134/Launch Apple First Combined.command>)
selects the same renderer policy as profiled69122 and staged69129. New69134
provenance is VERIFIED;8,273 app files/modes/symlinks match the built app,
staged native verification and SHA256SUMS pass. ExecutableSHA256
`6320a5c589bd55b0a72758c67d35d5d068c02e186cee405bed9882c1cfef41d2`.
The launcher is unexecuted; both earlier candidates remain unchanged.

Useful render CPU overlap remains a concrete open acceptance item. The bounded
source audit finds that `renderSpotShadow` consumes explicit native inputs,
but its renderer command, frame arena, material caches, stats and settlement
state are shared with subsequent capture. A batch-only worker would race.
An isolated non-query encoder context plus complete palette/source/target
ownership and owner-side settlement is needed before testing overlap. Borrowed
probe/hero commands and query views also have transaction dependencies. This
is an identified ownership boundary, not an implemented worker or a proven
profitable next path. No new live capture was taken.

Exact receipts under the existing private evidence root:
`frozen-source-build-75.json`, `native-correctness-75.json`,
`native-dependency-topology-75.json`, `native-image-75.json`,
`native-segments-75.json`, `shadow-segment-costs-75-summary.json`,
`combined-component-cost-gate-75.json`, `static-native-75.json`,
`strict-source-75.json`, `stage-75-sha256sum-check.json` and the staged
`architecture-combined-69134/provenance.json`. Owner whole-viewer feature,
loading, memory, frame-time and stability acceptance remains pending.
