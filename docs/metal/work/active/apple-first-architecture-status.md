# Apple-first architecture implementation status

Updated 2026-11-15. The original whole-engine goal is **active and incomplete**.
Exact source, commands, failures and receipts are in the [execution ledger](apple-first-direct-cohorts-20261114.md).

Latest verified build: **142390cc67 / 26.4.0.69120**. Required build,
ARM64/signature/import verification, strict source audit and relevant native GPU
checks pass. No new viewer launch, login, live profile or push has occurred.
Owner apps and staged69102 remain unchanged. **Whole-viewer FPS improvement is
not established.**

| Boundary | Implemented and checked | Remaining |
|---|---|---|
| Shared scene/resources | Producer generations, immutable material/resource tuples, dirty subscriptions, retained static groups, compact restoration, native leases and exact cache-reason counters. | Validate the new retained regular-rigged direct consumer; owner runtime evaluation. |
| CPU preparation | Immutable owned inputs, bounded shared credits, useful joined shadow preparation and scalar recipes. Budget recovery/fairness5867 assertions/46 cases passes. | World workers remain disabled after measured completion regressions. |
| Animated ownership | Shared attachment records and immutable view-specific system-body palettes, preserving required camera composition. Raw/record world and shadow parity passes. | Owner whole-viewer acceptance. |
| Visibility/submission | Persistent animated sources, late pose/view inputs, conservative GPU visibility and ordered native commands; mode switches, cancellation and pending native ownership pass. Encoder-scoped declarations remove repeated query-range walks. | GPU commands remain disabled after worse complete costs. Source71 feeds retained references through the original direct shaders to preserve CPU savings without the indirect GPU cost; unbuilt/unmeasured. |
| Unified memory/admission | Changed-range/COW storage, frame-wide CPU/native/decode/upload coordination, explicit completion retirement; maintenance and pressure checks pass. Numeric animated cap/retirement tests pass. | Owner loading/teleport/stability and physical working-set evaluation. Cache ledger scopes overlap and are not total RAM. |
| Terrain/probe lighting | Terrain sharing selected at measured16-patch threshold; eligible probe/base lighting fusion and empty lighting-work avoidance pass contained checks. | Owner whole-engine evaluation. |
| Combined candidate | All accepted foundations build together. New direct consumer is integrated and peer-reviewed. | Build/test source71, measure direct/full-count/visibility routes, choose supported paths, then separately stage one verified combined app. |

## Latest measured decisions

- Source70 full-count animated commands improve owner CPU at64 draws by4.52%
  (one view) and1.53% (three views), but completion is about12% worse. GPU
  visibility is worse again. Both controls remain OFF.
- Source71 retains the same producer/capture ownership, but uses ordinary direct
  rigged shaders and bindings. It removes scratch Batch copies and reuses400B
  common constants only when the actual view and immutable fields prove equal.
  Direct sealing skips GPU descriptors and bone scans. This is an implemented
  hypothesis, not a successful performance result.
- The cost frontier retains1/2/4/64 draws, one/three views, cold/warm/pose/resource
  churn, denials and loading samples.96 indices/source is a known limitation;
  saved club data does not establish a per-draw geometry-size distribution.
- CPU static visibility wins with many outside bounds but adds all-visible/cold
  overhead; it remains OFF. Body ownership timings are near parity. Small
  terrain sharing and world workers remain disabled after measured losses.

## Next concrete steps

1. Required source71 build/static/strict verification and full native regression
   scopes for the changed Draw facade, including all retained ownership guards.
2. Three separate48-condition cost runs: direct records, full-count commands,
   and GPU visibility. Keep the same raw baseline and explicit route census.
3. Repair actual failures and select paths supported by complete costs.
4. Separately stage the exact combined app and launcher for owner whole-viewer
   feature, stability, latency, memory and performance evaluation.

No shadow-only investigation is a prerequisite. Product release gates remain
open; contained tests do not establish full viewer performance or feature parity.
