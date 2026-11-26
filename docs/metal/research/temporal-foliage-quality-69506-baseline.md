# Temporal 69506: frozen quality baseline

This records **twenty failed assertions in one fixture run, not twenty separately diagnosed defects**. Values are frozen from the retained `fixture-clean-final-08` run; later candidates must be compared against this table without rewriting its observations or bounds. No rendering was rerun to extract it. The owning defect is PPD-610; material-history correctness PPD-611 is separate.

## Provenance

- Fixture source: `f969bd53245a26b79df3abcb41c047f28558f455`; test source SHA-256 `372b39097c4e0b215ce9554927c72ba8452b9f902ad9ef3c1247774e97e04c50`. The extraction checkout was `f2f841305c88023d11fba8e51673d57f280d9d1b`.
- Retained run: 2026-11-26 00:05:03.020490–00:05:06.186602 UTC; `[foliage-stability]`, Metal API and GPU validation enabled; exit 42. Test executable SHA-256 `98dd3fdddadbbd459eb0e811cd7428caf0b9b145f0f073a334e76a29c2e3fd16`. This is the test executable, not the packaged 69506 app executable.
- Catch2 totals: 6201 assertions,6181 passed,20 failed; one failed test case. The packaged 69506 application source is the same pinned correction commit; no claim of twenty new app defects follows from this count.
- Artifacts under `artifacts/metal/temporal-foliage-20261125/`: `fixture-clean-final-08.log`, `fixture-clean-final-08-receipt.json`, and `fixture-clean-final-08/{regions,frames}.csv`. Full-precision observations below come from Catch2 expansions; CSV summaries are rounded and only corroborate them. Exact hashes and parsed rows are retained privately in `artifacts/metal/temporal-foliage-20261125/quality-baseline-69506/baseline.json`.

## Fixture and metric scope

Actual production color/depth/motion/rejection/scaler paths are exercised by independently authored offscreen geometry:256×128input→384×192output, Temporal 2/3, bias −1.58496,18 jitter phases. The main stream warms 36 worlds, measures 54 stationary worlds, moves 18 worlds and stops 36 worlds. `frames.csv` records one initial reset, valid previous state thereafter and all 144 main-stream worlds. These are fixture results, not Home FPS measurements or identification of live palms from their appearance.

All reported RGB differences are **linear RGB** from the actual reconstructed output texture, not encoded presentation-luma metrics. Stationary means use the 84×84 interior of each 96×96 ROI. Edge-band means use the fixed native-reference edge neighborhood. Recall and area compare recovered foreground coverage with an independently rendered coverage reference; recall allows the existing one-pixel neighborhood. Stopped errors are measured at the 18th/36th stationary world after motion, on changed pixels, against an independently warmed Temporal reference at the final camera and the same jitter phase. They are not Native absolute-error assertions.

## Failed bounds

Every row includes the immutable source assertion line (`T`) and retained log line (`L`). Source links target the pinned commit, so later edits cannot silently change the referenced test.

### MASK paths — 16 failed assertions

| # | Region / material path | Metric / assertion | Observed | Required | Evidence |
|---:|---|---|---:|---:|---|
|1|`generic_mask_sky` — Generic opaque alpha-mask; published classic-tree consumer flags, not LLFace producer traversal|mean adjacent edge-band RGB change; `edge_change<=1./255.`|0.00431148953450549|<= 1/255|[T852](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L852), log L19|
|2|`generic_mask_sky` — Generic opaque alpha-mask; published classic-tree consumer flags, not LLFace producer traversal|minimum coverage recall; `m.minimum_recall>=.90`|0.85402309287561229|>= .90|[T859](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L859), log L26|
|3|`generic_mask_sky` — Generic opaque alpha-mask; published classic-tree consumer flags, not LLFace producer traversal|minimum recovered/reference coverage area; `m.minimum_area>=.85`|0.72128308059733415|>= .85|[T860](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L860), log L33|
|4|`generic_mask_sky` — Generic opaque alpha-mask; published classic-tree consumer flags, not LLFace producer traversal|18-world stopped phase-matched RGB error; `stopped[1][i].phase_matched_error<=1./255.`|0.00663084148082352|<= 1/255|[T867](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L867), log L40|
|5|`legacy_mask_opaque` — Legacy material MASK, opaque background|mean adjacent RGB change; `change<=1./255.`|0.00430316200028507|<= 1/255|[T851](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L851), log L48|
|6|`legacy_mask_opaque` — Legacy material MASK, opaque background|mean adjacent edge-band RGB change; `edge_change<=1./255.`|0.00505461367666763|<= 1/255|[T852](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L852), log L55|
|7|`legacy_mask_opaque` — Legacy material MASK, opaque background|minimum coverage recall; `m.minimum_recall>=.90`|0.89310152396239684|>= .90|[T859](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L859), log L62|
|8|`pbr_mask_sky` — PBR MASK, sky background; authored grass texture|minimum coverage recall; `m.minimum_recall>=.90`|0.85761618631501346|>= .90|[T859](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L859), log L70|
|9|`pbr_mask_sky` — PBR MASK, sky background; authored grass texture|minimum recovered/reference coverage area; `m.minimum_area>=.85`|0.73585134880575009|>= .85|[T860](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L860), log L77|
|10|`pbr_mask_sky` — PBR MASK, sky background; authored grass texture|18-world stopped phase-matched RGB error; `stopped[1][i].phase_matched_error<=1./255.`|0.01847008152703998|<= 1/255|[T867](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L867), log L84|
|11|`pbr_mask_sky` — PBR MASK, sky background; authored grass texture|36-world stopped phase-matched RGB error; `stopped[2][i].phase_matched_error<=1./255.`|0.00607027091314644|<= 1/255|[T868](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L868), log L91|
|15|`grass_sky` — Generic opaque alpha-mask grass, sky background; not BLEND|minimum coverage recall; `m.minimum_recall>=.90`|0.85817597908358423|>= .90|[T859](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L859), log L121|
|16|`grass_sky` — Generic opaque alpha-mask grass, sky background; not BLEND|minimum recovered/reference coverage area; `m.minimum_area>=.85`|0.73641728538047047|>= .85|[T860](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L860), log L128|
|17|`grass_sky` — Generic opaque alpha-mask grass, sky background; not BLEND|18-world stopped phase-matched RGB error; `stopped[1][i].phase_matched_error<=1./255.`|0.01795483555317869|<= 1/255|[T867](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L867), log L135|
|18|`grass_sky` — Generic opaque alpha-mask grass, sky background; not BLEND|36-world stopped phase-matched RGB error; `stopped[2][i].phase_matched_error<=1./255.`|0.00583019928665798|<= 1/255|[T868](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L868), log L142|
|19|`dense_mask_opaque` — Generic opaque alpha-mask dense foliage, opaque background|mean adjacent edge-band RGB change; `edge_change<=1./255.`|0.00417986980182676|<= 1/255|[T852](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L852), log L150|

### BLEND path — 3 failed assertions

| # | Region / material path | Metric / assertion | Observed | Required | Evidence |
|---:|---|---|---:|---:|---|
|12|`blended_leaf_sky` — Generic forward/post-water fullbright standard-alpha, sky; depth-only companion|minimum coverage recall; `m.minimum_recall>=.90`|0.89056031102389499|>= .90|[T859](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L859), log L99|
|13|`blended_leaf_sky` — Generic forward/post-water fullbright standard-alpha, sky; depth-only companion|minimum recovered/reference coverage area; `m.minimum_area>=.85`|0.84450735858118853|>= .85|[T860](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L860), log L106|
|14|`blended_leaf_sky` — Generic forward/post-water fullbright standard-alpha, sky; depth-only companion|18-world stopped phase-matched RGB error; `stopped[1][i].phase_matched_error<=1./255.`|0.01005504958646609|<= 1/255|[T867](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L867), log L113|

### Controls — 1 failed assertion

| # | Region / material path | Metric / assertion | Observed | Required | Evidence |
|---:|---|---|---:|---:|---|
|20|`straight_edge_control` — Generic unmasked opaque straight edge|18-world stopped phase-matched RGB error; `stopped[1][i].phase_matched_error<=1./255.`|0.00462346703749525|<= 1/255|[T867](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/f969bd53245a26b79df3abcb41c047f28558f455/indra/almetal/tests/almetal_temporal_foliage_stability_test.mm#L867), log L159|

## Interpretation and retained controls

- MASK failures: 4 stationary RGB/edge-change bounds,7 coverage/detail bounds and 5 post-stop phase-matched error bounds. Generic `grass_sky` is an opaque MASK fixture even though the live foreground grass includes BLEND paths. The classic-tree flag match does not traverse the viewer's LLFace tree producer.
- BLEND failures: coverage recall,minimum coverage area,and 18-world settling. Its stationary mean/edge variation passed; that does not erase the coverage or settling failures.
- Controls: `straight_edge_control` fails 18-world settling. `opaque_texture_control` has no failed assertions. No region fails 36-world old-history-fraction,stationary motion or depth-agreement bounds; passing those cannot establish complete image stability.
- Every `edge_band_frame_p95` bound passed. Coverage precision/maximum-area,detail-energy floor,Native mean-error and other passing guards remain in the test. This table lists failures only; full CSV/JSON retains all eight regions. Do not weaken a bound to relabel a candidate as corrected.
- No observed or threshold values are missing. Catch2 records more precision than `regions.csv`; exact scalar strings and source/log locations are preserved in the private extraction. A new candidate must report its own source/library identity and a separate comparison rather than replacing this baseline.
