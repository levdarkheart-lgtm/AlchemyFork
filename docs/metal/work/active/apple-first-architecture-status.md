# Apple-first architecture implementation status

Updated 2026-11-15 (active integration). The original whole-engine goal is **active and incomplete**.
Detailed source and evidence remain in the [execution ledger](apple-first-direct-cohorts-20261114.md).

Current built checkpoint: **ac2dfaeb6b / 26.4.0.69102**. Required doctor,
configure, build, static ARM64/signature/import/feature checks and strict source
audit pass. Focused actual terrain-owner and native terrain checks pass. Exact
diff evidence reuses source55's unchanged renderer checks: all28native scopes
and11cost suites passed. The explicit recipe tag repeats three cases also in
the cohort scope; execution totals are not unique-case totals.

A separate combined checkpoint is staged and statically verified locally at
`artifacts/metal/apple-first-persistent-native-scene-20261114/architecture-checkpoint-69102`
in the canonical workspace. Its launcher uses the same primary choices as owner69085. The existing69085/69065 apps remain unchanged.
No new viewer launch/login/live profile or push has occurred. There is **no
established whole-viewer FPS improvement**. The older69085 five-minute14FPS
recording is not a controlled before/after result.

| Boundary | Implemented and checked | Still required |
|---|---|---|
| Shared scene and resources | Producer generations, immutable material/resource tuples, dirty subscriptions, retained static groups, compact restoration and native leases. | Same-frame reproduction distinguishes CPU metadata expiry from retained native resources; add exact capacity/miss reasons before choosing a remedy. Finish specialized sharing gaps while keeping per-view transforms/order/culling current. |
| Animated data | Attachment identity shared through main/probe GBuffer and forward passes; pose ownership/parity/queued-reader lifetime checked. Equal-byte upload amplification removed. | Keep view-dependent system-body composition; qualify any additional animated visibility conservatively. Whole-engine benefit remains unproved. |
| Useful CPU preparation | Owned immutable packets, bounded shared credits and joined shadow work. New producer-derived scalar recipes avoid repeated400B assembly/comparison and fixed flag lookup. | World-worker tickets remain disabled: fair owned-input comparisons still regress complete time. Continue useful producer work rather than enabling idle worker infrastructure. |
| Visibility and native submission | Direct retained-group submission and scalar recipes work for small groups. Native bounds, GPU frustum and command-bank experiments are implemented. | Published CPU visibility is integrated for verification, default OFF: reject qualified static bounds before bindings, preserve queries and unknown draws. Native parity and complete cold/warm/churn costs pending. GPU visibility/banks remain disabled after mixed/worse results. |
| Unified memory and admission | Decode/upload/frame admission, native COW generations, CPU reclaim and native-only command-completion leases. | Investigate cache-capacity/lifetime interactions with actual current-frame work; no blind cap increases. |
| Terrain | Shared pass-common material/surface inputs, one native constant publication per view, prepared aliases and independent texture leases. Exact output/depth/query/lifetime checks pass. | Caller now selects sharing at>=16patches in allfourmain/probe paths; owner runtime confirmation and complete LL pool performance remain open. |
| Probe lighting | Eligible unblurred light map and base lighting share one tile-memory pass; sky/HDR/shadow/reverse-Z/depth/older-library behavior checked. Empty local/projector owners avoid empty work. | Whole-engine evaluation. No cadence or face-multiplicity regression was found againstG1. |
| Combined product | New checked build includes all accepted work above with exact source/evidence. | Finish remaining architecture boundaries and staged verification, then owner feature/stability/whole-frame performance evaluation. Full release gates remain partial. |

## Latest measured decisions

- **Terrain:**1/2/4patch sharing loses CPU in all12conditions.16/64/256patches
  improve CPU and completion in all12. The caller uses16as a measured boundary;
  shaders, patch geometry/tessellation and update cadence are preserved.
- **Retained scalar path:**many-group warm CPU improves8/8conditions versusraw,
  median28.05%; completion improves7/8, median14.60%. Cold compilation remains
  slower. These results include retained publication, not just the new recipe.
- **Narrow recipe comparison:**exact source54/source55 binaries use identical
  existing tests and shader bytes. Raw timing drift is substantial. Normalizing
  cohort/raw CPU ratios, allsix512-draw warm cases improve, median7.03%; small
  groups are mixed. This is limited cross-run evidence, not a frame-FPS claim.
- **World workers:**the fair owned-input benchmark includes adoption, credits,
  real worker join, reclaim and completion. Available-worker completion loses
  allnineconditions at source55. Workers remain disabled.
- **Restoration:**the revised cost frontier proves actual expired/live-proof
  restoration and native-buffer reuse. Forced tiny-group rebuilding costs more
  CPU than raw; warm reuse and cold debt must both govern compilation policy.

## Immediate next work

1. **Done:** separate69102checkpoint inventory/static verification,8272matching
   nodes,346ARM64Mach-O files and exact source/signature/import/feature checks.
2. Use a contained main→probe→probe cache-lifetime reproduction to distinguish
   same-revision metadata expiry from real producer changes and flavor extension;
   preserve caps until a measured remedy is supported.
3. Continue the useful visibility/submission and specialized sharing boundaries.
   Do not activate banks from owner CPU busy alone: existing completed-command
   sums overlap and lack whole-frame queue coverage.
4. Evaluate the combined viewer as a whole when the required implementation
   boundaries are ready. Keep performance and full feature acceptance partial
   until owner runtime outcomes exist.

## Source audits retained

Probe cameras are applied after source lookup; reads do not signal producer
changes, promotion preserves existing variants, and cube faces do not increment
outer frame age. Same-frame metadata withdrawal is reproduced in contained controls; its share of
saved-profile rebuild cost remains unknown. The69085census reached the3712MiB
native wrapper-byte ceiling and separate2048CPUlayout entry ceiling; native
entries stayed below8192. These wrapper estimates are not physical allocation totals. G1/source55 probe face schedules, hero rate selection and shadow calls
match; completion callbacks only retire credit/resources.


Current integration: PPD271 CPU visibility six production files + two new
native/actual-cost appendices are frozen and peer-reviewed, with default-OFF
setting, producer bounds request and periodic counters wired by root. Verification
pending; no activation or performance claim. PPD273 WIP fixture corrections included.

Required59/5785fb5761 builds69107, static/strict pass. Cache lifetime2516/2520
assertions pass, all same-frame reuse/build controls match; four final teardown
checks fail before releasing actual upload frame owners (PPD275). Fixture now
settles initialization/releases frame fences before asserting retirement; retest
pending. Source58compilefailure is retained separately (PPD272), corrected59.
PPD274 adds exact native capacity/age/touch branch census with policy unchanged.
No viewer launch/login/profile or push. Existing staged69102 remains intact.
