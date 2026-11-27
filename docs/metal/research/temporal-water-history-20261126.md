# Water detail and rough-SSR sampling review — PPD624/628

The tile defect has a demonstrated source and reversible improvement. The sampled
tiles are opaque PBR, around .20 roughness; the old glossy budget4 supplied one
randomized SSR ray. The owner confirms SSR-off removes the grain in both Aurora
and Spatial75%. A cap-respecting four-ray minimum reduces actual-scene final
spatial grain RMS by53.2–53.4%, with less than .47% restored-control variation.
Residual grain remains. This is a sampling-quality correction to the shared
upstream policy, not evidence of a Metal-only port mismatch.

| Actual tile stage, normalized RGB high-pass RMS | Original A0 | Four-ray B | Restored A1 |
| --- | ---: | ---: | ---: |
| Opaque HDR | .04892055 | .02424715 | .04914586 |
| Pre-Aurora HDR | .04837234 | .02398046 | .04859516 |
| Immediate Aurora | .02070341 | .01108163 | .02069114 |
| Final world | .03139691 | .01462076 | .03125021 |

Each arm has180worlds, ten18-phase cycles, identical camera/projection and
phase-matched albedo/ORM/normal/depth. Mean final luminance changes+.44–.54%,
rather than fading the reflection. All18phases improve. B/A1 each join180worlds
to successful main-post completion, exact output resource/generation and current
UI selection. Their same-frame bound film grain is0, dither1/8bits/animated.
A0's quiet capture was disabled by a rejected512MiB request; its own completed
producer tokens and parity/resource identities support stage comparison, but no
native-command/UI join is invented. Subsequent captures use the accepted256MiB
limit. This is one reversible visual comparison, not independent per-frame trials
or an uninstrumented FPS measurement.

**Water is not qualified as fixed.** The affected background patch is confirmed
dedicated world water. Its repeating detail response already exists in HDR and
persists when normal animation is frozen. No resets occur; generated reactivity
stays0 and accumulation mature. Clearing composition does not qualify: it worsens
movement/stop variation and settling in the controlled fixture. Four-ray SSR
reduces high-frequency reflection energy in the actual water, but also contrast;
normalized stability does not improve beyond restored-control variation. Preserve
PPD624 open. Normal animation, primary SSR and other water-shading contributions
must remain distinct. A final controlled cut holds the same normal animation
phases fixed and omits only primary water SSR: phase-cycle RGB temporal standard
deviation drops raw .014832→.005139, immediate .005512→.001136 and final
.011465→.002326 (79.7% lower final). All phase-matched material/depth/motion/mask
inputs remain exact; opaque shaded color drift is .0000625 MAE versus .018997
water intervention MAE. This identifies the primary SSR contribution as the
largest demonstrated component of this frozen-normal cycle, not all natural
water animation or a measured exclusive function cost. Residual cycling remains;
SSR omission changes lighting and is diagnostic only. No frozen animation,
rejection override or shader bypass ships. The accepted paired-coordinate, actual-water-depth, glass and foliage
repairs remain intact.

The bounded source check also excludes double-applied water jitter: capture starts
with logical P/P^-1, local Temporal copies receive J once, refraction snapshots
share that raster's coordinates and paired inverse, and motion subtracts jitter
once against the unjittered inverse. This closes that specific contract question,
not every source-shading cause of the remaining water cycle.

The new tile rendered regression fails three original-library bounds and passes
with the candidate. The water SSR regression fails two source-noise bounds on the
original library (.011795meanMAE/.020513centeredRMS), versus .005615/.010009 with
four rays. All180water high-budget/off controls and72tile high-budget/off controls
remain exact across variants. These higher-budget references are fixed common
sampling references, not perfect reflection or Native-quality oracles.
At2304×1446, full-BASE synthetic GPU medians A0/B/A1 are1.6814/4.1190/1.6801ms:
**+2.438ms** is a material rendering cost. No extra steady textures, pool deferrals
or ownership system are added. This is not SSR self-time or a viewer FPS result.

Private evidence: `artifacts/metal/water-cycle-tiles-20261126/` retains authored
fixture comparisons, exact source/library/compiler receipts, raw cost samples,
failed setup/no-hit attempts, actual-scene ROIs and joined records. Owner images,
locations and account data are excluded from Git. Public source carries only the
common sampling rule, reusable read-only graph observations and focused tests;
completed diagnostic overrides and exclusive test helpers are removed.

Current local-source verification: full Release build exits0, then refreshed test
targets and20selected Metal suites pass with API/GPU validation (157.60s). Four
CPU input/resource/finalization checks pass. The cleaned water SSR counterexample
fails exactly two frozen bounds on original175f library and passes57,501assertions
with candidate361b; tile counterexample fails three bounds and candidate passes.
These are rendering/ownership/sampling regressions, not closure of PPD610's
existing quality failures. New source pin/build identity, login-only smoke and
private package delivery follow separately; the69523 section below is historical.

## Source-pinned sampling candidate 69525

Application source `0a94aa6fe2f28e2ad544aa9cded56c8526be6278` is pushed only to
`codex/aurora-native-temporal-20261126`. The final Release build exits 0. Its
built-in shader exactly matches the rendered candidate; Aurora's embedded shader
source is unchanged. Later delivery/documentation commits are distinct from this
application source.

| Identity | SHA-256 |
| --- | --- |
| Executable | `e9ff71e51aac809e48b008a1408e6e0de5f1d04d704088fef3bb131226ec5bfc` |
| Built-in metallib | `361b9f91fcbffd633c40c46688cf5e245f2e62ef01bac1c83cfeb5fc7f34a92f` |
| Embedded Aurora generated MSL compiler input | `9dd3a205ff55b2e401f7d3c4b4faba17c62051bd94f39401806b65c95d9f8070` |

The built-in shader diff against 7c25 is only the eight-line shared SSR sampling
change. Read-only graph observations and focused rendered tests carry the
evidence; no private pipeline ROI hook or completed water causal override ships.
Mirror/transparent sampling and existing sample caps remain. Twenty affected
Metal suites with API/GPU validation and four CPU suites pass at this same code
state before version metadata was pinned; no rendering source changed afterward.
Original PPD610 bounds remain unchanged.

The exact copied app passes deep/strict ad-hoc signature checks and all 346 Mach-O
ARM64 checks. Its full-screen, automatic-login-disabled smoke reaches
STATE_LOGIN_WAIT, confirms OpenHTJ2K 0.27.0 pixel decoding, and closes normally
without world login. All 18 original settings files and 24 entries are restored,
including restorable metadata. Package verification receipts retain mounted app
entry comparison, signatures, architecture and library checks; cloud upload
confirmation is recorded separately after the immutable DMG/ZIP are created.
The app is not notarized and retains the existing macOS 26 deployment target.

This is an experimental tile-grain candidate with a measured material cost;
water cycling is not repaired. Preserve 69523/69520 and owner-accepted glass and
foliage. Select **67% (Aurora Temporal · Experimental)** under Advanced Graphics
world resolution and check the effective status beside it. Native remains default;
combined main lighting remains OFF. Use macOS window screenshots/recording for
live review, rather than the independent viewer snapshot route.

## Private package verification

The immutable69525 DMG is mounted and compared against the exact smoke-tested
app: all8,293entries match, signatures and346ARM64 checks pass. The review ZIP
passes CRC and all110selected-file hashes. Full source/focused diff, selected
actual-scene ROI metrics/joins/crops/clips, synthetic regressions and raw component
cost samples are private; no raw settings or login screenshots are included.
The first cloud observation at01:26:30UTC reports the retained PPD618 transient
4355condition. Destination hashes match, but remote readiness is not inferred from
that local copy. At **2026-11-27 01:28:25UTC**, the third bounded read-only recheck
confirms all three unchanged files uploaded, no upload in progress/error and matching
hashes. No account/service change, recopy or rebuild was needed. The private
`delivery-complete.json` receipt owns that final upload observation.

| Package on iCloud Desktop | SHA-256 |
| --- | --- |
| `Alchemy-Aurora-SSR-Sampling-Experimental-69525-AppleSilicon.dmg` | `54c2555bb2876530da0d7a7408ea41fa9c9d4ffec918c6b9bc625a8cf20dbf93` |
| `Alchemy-Aurora-SSR-Sampling-Experimental-69525-Review.zip` | `84f25f696cfff3c8d1962ca4cd3be2541ba547e91853e1f33a192ebde25fba82` |
| `Alchemy-Aurora-SSR-Sampling-Experimental-69525-SHA256SUMS.txt` | `cbd9390fd2355ceb964633ec8607206ece45a7d3de2f88bbce73396fbd98ed2c` |

## Preserved 69523 water/history report


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

## Continued water-cycle and tile-grain task — PPD624/628

Start7c25d0b389 (reviewed app69523/sourcec72fafcd3b), clean and remote-matched.
Two new private recordings define the symptom: aurora-water-slow-flicker.mov and
Screen Recording2026-11-26at22.59.04.mov. Their mode/stream relationship is being
checked before treating them as independent populations. Owner reports water
detail collapse/recovery while silhouettes stay fixed, and steadier tile grain
in both Aurora and Spatial75%, apparently dependent on viewing angle.
Source inspection and bounded multi-cycle production fixtures precede correction;
no jitter-wrap reset, dedicated-water identity or SSR attribution is assumed.

### Follow-up observations before selecting a water correction

The extended414world fixture (72warm,108fixed,108animated,18moving,108stopped)
records no reset after first use and immediate/resolvedRGB equality. R8state
reconstructs update_accumulation within1LSB. Composition is255 throughout the
water ROI, but accumulation still varies with generated/dilated shading change.
Locks and luma instability are0. A test-only generated-reactivityOFF arm makes
accumulation mature but loses animated/stopped detail; a constant-high arm also
does not remove the dominant animated trough. Neither is selected as a fix.
The original new static-input oracle failed5phase-zero comparisons because
.02f*18 differs from literal.36f by1ULP; only motion differed (3.04e-6inputpixels),
while HDR/opaque/depth/unsafe were exact. The plateau now uses the same expression
as the lastmoving camera, with an equalityassertion; no tolerance was relaxed.
Baseline/cuts then pass. Retained data, including the failure, stayprivate.

The authored constant-material PBR tile case places pepper in rawBASE SSR, before
reconstruction/presentation. Four-ray minimum within the existing budget improves
residualgradient29–58%;72frozen high-budget and SSRoff frames staybitexact. This
is a source quality/cost candidate, not yet attribution to the owner's tile.
Current privateROI diagnostic hooks exist only to join the exact scene stages;
they are not shipping code and must be removed before final source pin.


## Owner-view boundary check — 2026-11-27 (PPD624/628)

Private diagnostic69524 used7c25d0b389 plus the retained ROI patch, executable
`e3c454d07fe80a6d227ca040c380be42ba42155a177e6cbf8fa5045d8f9068d5`,
with unchanged69523 builtin library175f2664. The owner logged in and positioned
the affected view. At2304×1446→3456×2168,180 consecutive submitted Aurora worlds
cover ten complete18-phase cycles. All6,480 bounded asynchronous ROI copies
collected with producer status completed; zero drops. Camera matrices are
bit-identical, jitter repeats every18worlds, parity alternates and all179 adjacent
accumulation/previous-accumulation transitions match exactly. No history reset or
GPU failure occurs. Private hooks are removed from shipping source afterward.

The ordinary quiet stream reached its64MiB limit before arming. Consequently
these data do **not** have a native-command-ID or final UI-selection join.
Their own world/ticket/parity/resource-generation and completed producer tokens
support same-world stage comparison, not presentation cadence or an FPS claim.
No association is filled from averages. The diagnostic application is closed;
its original18preference files/24entries and restorable metadata are restored.

**Water:** the sampled background patch is actual dedicated water: coverage255
at every texel/world, selected depth equals water depth, camera motion is zero.
Unsafe confidence255 selects Aurora's binary composition/maximum-reactivity
policy. Generated transparency reactivity is **zero** throughout this patch;
next accumulation remains255, while small shading changes affect pre-increment
accumulation. Hence the authored fixture's0↔.9 generated-reactivity cycle and
history resets do not explain this particular owner capture.
Raw-HDR spatial-detail CV is9.88%, immediate Aurora4.11%, final world3.72%.
Phase accounts for88.7%/45.6%/34.7% respectively. Some input variation is expected
from jitter; even after a bilinear jitter-alignment diagnostic it remains phase
related. At matched phases the opaque input is effectively unchanged, but water
HDR changes as normal phases and SSR history advance. Thus modulation is already
present before reconstruction; this does not yet identify normal sampling, SSR
or their interaction as the defective operation. Water waves=false disables
geometry displacement, not scrolling normal animation. No water-policy fix is
selected from these observations alone. Preserve PPD624 open.

**Tiles:** all516,060 captured G-buffer texels are opaque PBR, median roughness
.20075, metallic.4. At glossy budget4 this gives one stochastic SSR ray. Pepper
is present in the opaque HDR snapshot (pre/post-transparency high-pass correlation
.999999786). Motion, unsafe and generated reactivity are zero; accumulation1.
All162 matched-phase albedo/ORM/normal comparisons are exact. Final grain RMS
is~7.98display code levels; presentation dither is bounded by one code level.
The earlier bound-noise record has grain0/dither1/bits8, but predates the ROI and
is **not** represented as a same-frame noise binding. On the owner's explicit
live SSR-off check, conspicuous tile pepper disappears in **both Aurora and
MetalFX Spatial75%**. SSR is therefore the demonstrated shared contribution,
not an Aurora-specific instability or an inferred film-grain setting.

A bounded common rough-SSR correction raises the minimum to four samples within
the user's existing cap; mirror rays and counts already≥4 retain their behavior.
The footprint, random sequence, hit estimator, material maps and all histories
are unchanged. Frozen authored tests fail three quality bounds on the original
library; candidate retest is pending. Existing controlled results show residual
gradient reductions29–58%, not complete elimination or owner acceptance.
At2304×1446, retained full-BASE synthetic cost A0/candidate/A1 medians are
1.6814165/4.118979/1.6800625ms (300samples each,360warm, validation off),
+2.43824ms versus mean controls. This is a material cost, not SSR self-time or
viewer FPS. All arms add zero steady textures and reuse300 pooled targets.

Source observations, invalid no-hit water-SSR attempts, composition-only and
reactivity-only diagnostics remain distinct from accepted fixes. Only a valid
rendered signal and controlled response can justify another water policy.
