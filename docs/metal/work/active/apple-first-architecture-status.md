# Apple-first architecture implementation status

Updated 2026-11-15. The original whole-engine goal is **active and incomplete**.
Source and exact commands/receipts are retained in the [execution ledger](apple-first-direct-cohorts-20261114.md).

Latest built source: **d2e8b6d9ca / 26.4.0.69119**. Doctor, configure/build,
ARM64/signature/native-import verification and strict source audit pass. No new
viewer launch, login, live profile or push has occurred. Existing owner apps and
separately staged69102 remain unchanged. There is **no established whole-viewer
FPS improvement**.

| Boundary | Implemented and checked | Remaining |
|---|---|---|
| Shared scene/resources | Producer generations, immutable geometry/material tuples, dirty subscriptions, retained static groups, compact restoration, exact native leases and cache-reason counters. Same-frame lifetime reproduction passes. | Integrate retained animated GPU records; owner runtime evaluation. |
| CPU preparation | Owned immutable inputs, shared admission credits, useful joined shadow work and producer-derived scalar recipes. | World workers remain disabled after measured completion regressions. Integrate the service-adjusted admission fairness repair needed by unsorted animated callers. |
| Animated ownership | Attachment palettes shared across views; system-body view composition now publishes immutable records instead of raw capture plus adoption copies. Actual publisher22267 assertions/78 cases passes. | Body-shadow fixture repair now passes3150 assertions/18 palette/preparation cases65. Do not reuse view-dependent body matrices across cameras. |
| Persistent visibility/submission | Retained direct groups/scalar recipes, static bounds and native GPU experiments exist. CPU support intervals pass GPU parity and independent boundary/rounding controls. | Required G4 rigged/masked GPU records are source-reviewed, integrated and builds66. Five native cases including byte caps pass68; actual producer80 cases pass67. Complete costs regress. ComputeOFF/ON and owned-vector handoff pass69; fullcount warm CPU4.6–15.5% worse. Reviewed encoder-scoped resource declarations and redundant owned-validation removal are next70; controls OFF. Late pose/view inputs and conservative current-pose visibility must pass correctness and complete costs. |
| Unified memory/admission | Frame-wide CPU/native admission, decode/upload coordination, native COW generations, CPU reclaim and command-completion retirement; maintenance843 assertions/18 cases passes. Native and CPU cache caps remain distinct. | Animated bank expiry, exact view leases and CPU/GPU compilation admission must pass integrated tests. No blind cap increase. |
| Terrain/probe lighting | Terrain pass-common publication selected at measured16-patch threshold; eligible probe/base lighting fusion and empty local/projector avoidance pass contained checks. | Owner whole-viewer feature/stability/performance evaluation. |
| Combined viewer | Earlier69102 checkpoint separately staged and statically verified; later69113 builds. | Integrate G4, run targeted GPU/actual-caller costs and relevant regressions, then stage one exact combined candidate for owner evaluation. |

## Latest measured decisions

- CPU visibility64: warm50%-outside versus retained direct improves CPU8/8
  conditions (median19.99%) and completion8/8 (11.13%). All-visible warm CPU
  costs1.74% median; all-visible churn3.25%; cold also costs more. Control stays
  OFF pending selection. These are contained costs, not viewer FPS.
- Body palette64: complete composition/capture/adoption/encode/reclaim/retirement
  frontier passes24 configurations. Timings are mixed and near parity; maximum
  logical native-byte ratio1.0. Ownership improvement is established, speedup is
  not. CPU allocator/control-block and physical resident memory are not implied.
- Terrain: small1/2/4-patch sharing loses CPU;16/64/256 improve CPU/completion in
  all measured conditions. The caller uses16, preserving shading and geometry.
- World workers and previously slower GPU banks remain disabled. Implementing an
  architecture capability does not justify enabling a measured regression.

## Concrete remaining sequence

1. Done: body-shadow fixture retest65 passes; prior failures retained.
2. Done in root: integrate reviewed animated source/backend/shader/frontend (PPD287),
   its fallback/fairness/lifetime fixes (289–292), root settings and build wiring.
3. Repair67 frame-budget recovery (290) and reverse-Z query fixture (288), then run native GPU parity/lifetime/admission and actual LL draw-caller
   cold/warm/pose/resource-churn costs, including small draws and multiple views.
4. Repair actual failures; select supported paths; verify and separately stage the
   combined app with exact source/evidence for owner whole-engine evaluation.

No new shadow-only investigation is a prerequisite. Special body/cloth/query or
unsupported draws retain correct native rendering; new retained animated shadow
submission is not claimed by the main/probe animated GPU implementation.
