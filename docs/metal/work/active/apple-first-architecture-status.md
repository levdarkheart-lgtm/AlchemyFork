# Apple-first architecture implementation status

Updated 2026-11-15. The original whole-engine goal remains **active for owner
acceptance**. The named Goals1–5 mechanisms are implemented and integrated; a
bounded [source audit](../../research/apple-first-engineering-delivery-20261115.md)
found no additional required implementation callsite. This does not establish
a whole-viewer performance gain or complete the product release gates.

Exact source: **beb6837b71f0429f116fef8e2ad7cc5d166bc010**, build
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

Evaluate the combined viewer with the owner across busy, quiet, loading,
turning and teleport scenes:
frame times/tails, input latency, physical memory, stability and retained SL
features. No new speculative architecture or shadow-only work is a prerequisite.
Any reproducible failure creates a concrete repair item. Whole-viewer performance,
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
