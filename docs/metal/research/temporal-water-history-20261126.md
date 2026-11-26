# Experimental water/history review 69523 — PPD623–627

Three source-contract defects are corrected and rendered regressions pass.
**The original water complaints are not established as fixed.** Coordinate/input
correctness improved; stationary water-pattern variation is essentially flat.

| Owner region | Actual producer/contribution | Disposition |
| --- | --- | --- |
| Blue background water outside the pool | Dedicated world water versus object/material is unconfirmed | PPD624 remains open |
| Warm broken pool streaks under the lights | SSR, environment reflection, refraction or material lighting is unconfirmed | PPD625 remains open |

The primary reference is the owner's `WaterJigglingLightsJiggling.mov` (referred
to as mp4), retained privately. A video does not identify material/draw families.
No account login or new Home/Club recording occurred. Authored fixtures use the
actual dedicated-water producer and BASE SSR; that does not attribute the video.
Owner-confirmed glass repair and successful foliage behavior are preserved.

## Corrections and source boundaries

**PPD623 — paired SSR history.** `ALMetalWorldScreenSpaceReflectionHistory` in
`indra/almetal/almetalworld.h/.cpp` owns the raw color/depth pair, accepted
publication command and exact projection/inverse/modelview/origin/source identity.
The viewer stamps this metadata alongside owned G-buffer inputs. Consumer rays
use storedModelview × inverse(currentModelview), then the stored projection;
the depth inverse and dimensions belong to those same textures. Skipped/rejected
publication retains the previous complete generation. Known GPU errors hide
failed history; origin changes fail closed until coherent publication. Reset
preserves the monotonic generation serial. No previous-application-frame assumption
or CPU wait is introduced. Existing lower-row storage orientation is unchanged.

**PPD626 — paired water raster/inverse jitter.**
`alMetalApplyTemporalWaterJitter()` in `almetaltemporalworldhistory.cpp` applies
Pj=J×P and inverse(Pj)=inverse(P)×inverse(J) together to copied field/haze inputs.
The motion inverse remains unjittered because its consumer subtracts jitter first.
The original reference is upstream/develop `408fd6c014e6` waterHazeV/F: homogeneous
clip4 interpolation and its matching inverse are preserved (including PPD013).
The rendered counterexample concerns full-XYZ haze reconstruction. Ordinary
projection jitter changes XY, while current refraction rejection compares Z;
no separate visible refraction-Z gain is claimed.

**PPD627 — actual visible-water inputs.** `prepareTemporalWaterInputs()` and the
water boundary in `almetalworld.cpp` capture actual water device depth/coverage
in R32F/R8 targets after water/exclusion depth-stencil testing, before later alpha.
The extraction samples a distinct depth snapshot, avoiding attachment feedback.
`almetalworld_motion.inl` and textures42/43 seed actual water depth/motion and
prevent hidden opaque replay from replacing it. Mask-zero pixels do not use the
old infinite-plane proxy. Valid full foreground replaces hidden water; partial
coverage selects foreground depth/motion but retains the visible water's unsafe
confidence; zero/invisible foreground preserves water. Invalid poses stay unsafe.
Animated water remains rejected; non-camera wave motion is not modeled.

Inputs are prepared before opaque MetalFX/Aurora work, owned by the existing world
transaction and retained through ordinary completion/abandonment. Settled reuse
is verified. Active Temporal water adds one depth/stencil copy and one extraction
pass plus 5 input-pixel bytes (16.66 MB at2304×1446). Existing refraction snapshot
storage is reused; nonrefracting water additionally requires that snapshot.
Native/Spatial null paths add no water targets/passes. No new queue or normal-frame
CPU wait; Aurora algorithm, mip bias, sharpening and graphics defaults are unchanged.

## Matched rendered results

Baseline is preserved69520 source `2af7ebba82b6bfd15886069f1eb9613f34fcef18`.

| Check | Baseline | Corrected | Meaning |
| --- | ---: | ---: | --- |
| SSR isolated-history RGB oracle MAE | .002828779531 | 0 | Exact stored-coordinate sampling |
| SSR current-raster RGB oracle MAE | .003965496845 | 0 | Current and stored jitter remain distinct |
| Water raw/output inverse-oracle maximum mean error | .000115113436/.000097885440 | 0/0 | Paired inverse correctness |
| Fixed-water successive RGB variation, 6,542 output texels | .003109148333 | .003108803499 | −.01109%; essentially flat |
| Same-region horizontal RGB detail | .016823110470 | .016824930287 | No material detail change |
| Actual-water proxy-depth error, recorded ROI | .009043594822 | 0 | Actual surface replaces opaque proxy |

The paired input-policy run has bit-identical output RGB in its 54-texel water
interior and 3,098-texel eroded clear interior over36 worlds. This is **not whole-frame
equality**: the maximum whole-image policy MAE/pixel differences are .000264225/
.258545. No positive-depth dry-hole output pair was retained; the missing-cell
input regression proves correctness, not a missing output-quality benefit.
A test-only localized rejection override reduced one interior variation by9.30%,
with identical inputs; it does not justify globally trusting animated water and
is not enabled in production.

SSR tests use real asymmetric bright geometry, the production history publisher
and BASE SSR consumer, with active-contribution guards. Final four cases pass
1,987 assertions, including asymmetric depth rows, negative row-flip controls,
skipped/rejected publication, noncommuting matrices, origins, resize and reset.
Water/pane visibility passes4,311 assertions: maximum depth/blend error
9.94653e-7/.000427246. Settled8warm+8measured worlds allocate0 new textures,
reuse56 pooled targets, add0 dependency deferrals and retain509,952 cached bytes.
These are fixed-descriptor component observations, not total GPU memory or FPS.

All19 selected Metal suites pass API/GPU validation: SSR, water, Temporal
history/motion/visibility/presentation/orchestration/scaler, glass, Aurora inputs,
Spatial, resource planning/resources and material sampling. Finalizer CPU passes.
Glass remains8 cases/68,498 assertions. Existing PPD610 quality failures remain.

Earlier invalid marker-order/cube-format fixture attempts are retained/excluded.
First valid water baseline failed306 analytical and two rendered oracle bounds.
A later empty-water fixture read an empty vector and caused the macOS GlassFixture
crash notice; this was not the viewer. Readback/size guards are corrected; its
focused retest passes102,659 assertions. The normal water suite subsequently passes.

## Review evidence and playback

Private `artifacts/metal/temporal-water-20261126/ssr-review-evidence` and
`water-review-evidence` retain authored PNGs, 1:1 crops, clips, exact metrics,
source/build receipts, failed attempts and hash manifests. No owner footage or
account data is pushed. Water inputs96×64→144×96 use18 production jitter phases,
72 warm worlds,18 stationary,18 moving,36 stopped and18 restored normal-animation
worlds. Clips play at6 or12 fixture worlds/s as labelled, not viewer cadence.
Matched phase17 stills remain; no mature stopped-water reference is claimed.
The ZIP preserves these authored comparisons and exact application source/diff.

## Delivered identity and verification

Branch `codex/aurora-native-temporal-20261126`; start checkpoint
`bc09268714d7f4cb851310b202c73c989502fa76`.
Application source **c72fafcd3b6191134e861f37939e21f975eac938**, version **26.4.0.69523**.
Later delivery documentation is separate from this pinned application source.

| Identity | SHA-256 |
| --- | --- |
| Executable | `826181006b635768029dc3aa4b999b3d477ff5972c100a3744c82993c7eb2b3a` |
| almetal_builtin.metallib | `175f26640fbc3ebf52972b93ec0281e1daaf44c244ffae939ecb90ad23a585a1` |
| Embedded Aurora sources.h | `9dd3a205ff55b2e401f7d3c4b4faba17c62051bd94f39401806b65c95d9f8070` |

Full Release exits0. The builtin library exactly matches rendered validation;
all four unchanged Aurora MSL source blocks are found byte-for-byte in the
executable. Deep/strict ad-hoc signing and346 ARM64 checks pass. macOS26.0 deployment
target; **not notarized**. Full-screen login-only smoke reaches69523 login, then
quits with no STATE_STARTED. All18 original preference files/24 entries and
restorable metadata are verified restored; viewer remains closed.

Mounted DMG matches8,293 app entries, exact executable/shader hashes, signatures
and346 ARM64 checks. ZIP CRC/all93 regular-file hashes pass. Preserved69520
DMG/ZIP/checksum hashes are unchanged. All three iCloud Desktop files are confirmed
uploaded, not uploading, no error, matching hashes at **2026-11-26T22:51:34Z**.
Initial22:49:57UTC error4355 cleared on the second bounded unchanged-file recheck;
no account/service change, recopy or rebuild (PPD618).

- `Alchemy-Aurora-Water-Experimental-69523-AppleSilicon.dmg`
- `Alchemy-Aurora-Water-Experimental-69523-Review.zip`
- `Alchemy-Aurora-Water-Experimental-69523-SHA256SUMS.txt`

DMG SHA-256 `1e52bb71e739b75f04fb1bbb0424b22836277bd442d14f78605b514a3e78034f`.
ZIP SHA-256 `fb16067717b9617cac307467eb17bd785fe8a00ea69344cbced161db3507b2cf`.
The compact ZIP comparison predates upload confirmation; final delivery receipts
are retained privately and this current report records the completed gates.

## Owner review

Advanced Graphics → World resolution → **67% (Aurora Temporal · Experimental)**;
confirm Active identifies Aurora, not Pending/fallback. Native stays default;
Spatial/MetalFX, output-based shadows and native UI remain intact; combined main
lighting staysOFF. Use macOS screenshots/recording, not independent snapshots.
Compare original background water and pool highlights separately, with normal
animation, camera movement/stop, shoreline/exclusions and accepted glass/foliage
controls. Their actual producers and material shimmer improvement remain the
specific unresolved boundary. No live quality acceptance or FPS improvement claimed.
