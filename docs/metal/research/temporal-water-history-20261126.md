# Temporal water and reflection history — PPD623–PPD627

Start: `codex/aurora-native-temporal-20261126` at
`bc09268714d7f4cb851310b202c73c989502fa76`; preserved69520 renderer source
`2af7ebba82b6bfd15886069f1eb9613f34fcef18`. Owner confirms the glass fence is fixed;
preserve that correction and successful foliage behavior. Aurora algorithm remains unchanged.

The two reported regions remain separate: blue background water outside the pool,
and warm bright broken pool streaks provisionally described as reflected/specular
highlights. A video alone cannot establish their material or contribution paths.
Primary local reference is Desktop/WaterJigglingLightsJiggling.mov, located by filename
search (owner referred to the attachment as mp4). No viewer session is active.

Source audit confirms current-jitter SSR constants are assembled independently
of retained color/depth publication. Pair exact stored projection/inverse/camera
metadata with successful history generation; include skipped/failed publication,
row orientation and no CPU waits. Inspect fixed-animation and naturally animated
water separately, including the water-haze inverse-projection pairing. Rendered
baseline, corrective results and delivery will be recorded here as obtained.


## Source findings before rendered verification

PPD626: water field/haze matrices originate as a matching unjittered pair in
ALWaterSurface::fillMetalWaterHaze (alwatersurface.cpp), reused by the dedicated
water producer. The Temporal color scene jitters field/haze MVP but previously
retained the unjittered inverse used by haze depth reconstruction and above-water
refraction rejection. A shared water-jitter helper now composes both. The motion
inverse stays unjittered: its shader first subtracts the known sample offset.
PPD013's homogeneous clip4 interpolation and earlier row-orientation correction
are preserved. This is source-confirmed independently of scene attribution.

SSR history is now a typed owner for the raw color/depth pair and exact raster
projection/inverse/modelview/origin/source identities. The source snapshot is
captured with owned GBuffer inputs and transfers only on accepted history
publication. Consumer rays use stored_modelview * inverse(current_modelview),
then that stored projection. A skipped/rejected publisher retains the preceding
pair, not an assumed preceding application frame. Origin changes fail closed
until coherent publication; known GPU errors hide failed history without waiting.
The actual publisher/consumer regression now verifies that contract below.


## Retained rendered checks before final qualification

SSR baseline69520 and corrected native tests use genuine asymmetric raw history
publication plus the production BASE SSR consumer. Isolated-history raster RGB
MAE against its exact-coordinate oracle is.0028287795313623673→0. With actual
current receiver raster jitter, it is.003965496844887561→0. Baseline and corrected
oracles match; no-hit guards exclude empty SSR. Final asymmetric color/depth row,
negative depth-flip, skipped/rejected publication, matrix/order, origin, resize
and reset recovery tests pass1987assertions/four cases with API/GPU validation.
Private matched PNGs/crops and two18-phase6fps clips are in ssr-review-evidence.
These are shader-output/coordinate results, not owner-scene attribution or FPS.

Water baseline03 is the first valid rendered baseline. It has306analytical
inverse-pair failures and two rendered oracle failures (max mean rawRGB
.0001151134363, reconstructedRGB.00009788544044). Earlier marker-order and invalid
legacy-cube-format setup failures are retained and excluded. Correct inverse
composition matches whole-jittered-projection inversion exactly in raw and
reconstructed output; genuine normal animation remains active.

That coordinate repair alone does **not** materially stabilize the reproduced
fixed water: successive RGB variation.0031091483→.0031088035. A test-only localized
unsafe-confidence override, with identical color/depth/motion, reduces variation
9.30% (.0034223493→.0031042177) and slightly increases detail. It does not justify
shipping relaxed water history: it initially retained underlying opaque inputs.
The override is confined to the test; no reconstruction algorithm is changed.

PPD627 therefore captures actual current water depth/coverage after real field,
exclusion and depth testing, before subsequent alpha. The frame-owned R32F/R8
pair seeds camera/surface proxy depth and prevents hidden opaque geometry from
overwriting it. Animated water remains maximally unsafe; non-camera wave motion
is not claimed modeled. Non-water holes use actual opaque inputs instead of a
hypothetical infinite water plane. This adds one depth/stencil copy and one
stencil-filtered fullscreen pass in active Temporal water views,5input-pixel bytes
of target storage (about16.66MB at2304×1446). The existing refraction-depth snapshot
is reused; nonrefracting water additionally needs that snapshot. Native/Spatial
null paths add no targets/passes.

The first new empty-water test crashed on a fixture-only empty vector read. Its
readback condition is corrected and size requirements precede pixel assertions;
the focused retest passes102659assertions. The viewer was never launched. Current
actual-water input policy comparison retains all water rejection and has identical
raw color; it does not establish a material stationary-water variation gain.
The background-water and pool producers in the primary video are still unknown:
older retained water_present observations cannot join those exact pixels.


The water field inverse is also used by refraction depth reconstruction. For
ordinary perspective jitter, only reconstructedXY changes while that consumer
currently comparesZ; no separate visible refraction-Z improvement is claimed.
The fullXYZ haze reconstruction is the rendered inverse-pair counterexample.

## Actual-water visibility and reuse

The actual-input comparison keeps raw color, color depth and jitter identical.
In its fixed-water interior the old proxy and actual-input arms have identical
successive RGB variation **.00384100768794** and gradient **.0207678607781**.
Correct inputs therefore do not demonstrate a water-pattern quality fix.

The post-water transparency regression passes4311assertions under Metal API/GPU
validation. It checks actual generic forward color, depth, motion and rejection
for zero, partial and fully covering panes, a pane behind water, a dry control,
invalid pose and recovery. Maximum water/pane depth error is9.94653e-7 and maximum
blend error is.000427246. A zero-contribution or invisible pane preserves water;
partial transparency selects the nearest surface but retains the visible water's
unsafe confidence; valid full coverage supplies the foreground and can accept
history. No unconditional water-history acceptance is introduced.

With completion settled, the same8warm+8measured worlds allocate **0additional
textures**, reuse56pooled targets, add0dependency deferrals and retain509952cached
bytes. This is an allocation/visibility check, not a timing or FPS result.
The glass suite still passes68498assertions/eight cases with the new library.

The macOS GlassFixture crash notice came from the earlier empty-vector fixture
failure, not the viewer. Its failed run remains retained; the corrected empty-water
test and the subsequent pane test complete normally. No owner account was used.

## Qualification boundary

PPD623 and PPD626 have rendered coordinate counterexamples and corrections.
PPD627 repairs actual-water input visibility and passes targeted ownership checks.
**PPD624 and PPD625 remain unresolved owner-scene image-quality reports.** The
source and authored fixtures do not identify which producer creates the two video
regions, or establish that both share a cause. In particular, maximum rejection
is retained for genuinely changing water; non-camera wave motion is not modeled.
The experimental package must not be described as a demonstrated water-shimmer
fix. Owner-scene review and a localized producer/input observation remain the
specific missing evidence. Preserved69520 is the comparison build.

## Correctness qualification before Release

The normal Release almetal_tests binary (not the private GlassFixture runner)
passes all18selected suites with Metal API and GPU validation in102.60seconds:
SSR paired history/isolation; water inputs/haze/normals; Temporal glass, history,
motion, visibility, presentation, orchestration and scaler; Spatial scaler and
presentation; resource-plan CPU/native, world resources and material sampling.
The actual viewer finalizer CPU suite and two Aurora borrowed-transaction/opaque-input cases also pass (19selected Metal suites total). Exact commands and complete
logs are retained privately under artifacts/metal/temporal-water-20261126.
No broad profiler, scene login, quality-setting change or FPS claim is involved.
