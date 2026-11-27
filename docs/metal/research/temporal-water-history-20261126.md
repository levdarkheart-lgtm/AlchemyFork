# Primary water SSR cycle repair — PPD624/628

## Current disposition — owner accepted69531 (2026-11-27)

The owner reviewed69531in the affected scene and explicitly accepted the reported
water blinking as fixed. PPD624and its receiver-feedback correction PPD630are
verified for that accepted reference. This establishes appearance in the tested
scene, not correctness for every possible water scene or a performance result.

- Application source: `f971d11de29daf60233f9f5875cf957ca069e95c`.
- Executable SHA256: `fd5228bbce87246903e631ed6570d77059f393dc453fedc58668d0a864f4f999`.
- Builtin library SHA256: `86ebf38d847cd9ef9b37afbbaff5981d1bdf57309e8e195f6ff83da0765e12a5`.
- Aurora embedded-source SHA256: `9dd3a205ff55b2e401f7d3c4b4faba17c62051bd94f39401806b65c95d9f8070`.

These identities also match the owner's running mounted69531app. Keep accepted
tiles, glass, foliage, normal water animation and reflections unchanged. The
previous mixed/confounded comparison and receiver-band approximation below remain
limitations. The owner explicitly instructs **no further patchA investigation or
visual changes solely to improve its metric**. Revisit only after a reproducible
visual defect. Earlier open/pending/rejected checkpoints below are historical;
this scoped owner acceptance supersedes their current disposition.69530remains
withheld and preserved, not an accepted reference.

A brief readback-free controlled GPU cost comparison is not possible using the
existing setup: the owner's mounted69531is running without a timing/control
connection, earlier sessions used ROI readbacks, the existing target-size cost
harness measures BASE rather than water, and the production water fixture reads
color/depth/motion/masks every world even when export is disabled. A new harness
or disruptive relaunch is outside this conditional check. No measurement was run,
no settings/rendering changed, and no concurrent build occurred. **Actual GPU
cost stays unmeasured**, distinct from the prior+25.1%depth-fetch work count.
No material GPU cost or FPS claim follows from that count. Owner visual acceptance
is unaffected. Future measurements must explain any concrete material impact
before replacing the accepted implementation.

Owner authorizes fast-forward integration of the retained reconstruction chain
into `codex/apple-silicon-metal` and the existing Alpha prerelease workflow.
The immutable69531reference is not relabelled as the future hosted artifact.

## Historical69527 receiver-coordinate candidate (2026-11-27)

The reproduced stationary water cycle comes from **rough-ray selection**, before
Aurora. Primary water SSR hashes refracted UV; quarter-pixel raster/normal changes
scramble that hash, redirect the sparse lobe, and change both accepted hits and
which part of the reflector they sample. At fixture pixel65,35 in detailed/soft
phases2/14, the normal changes0.40degrees, but individual rays change15.7–25.3degrees;
accepted rays fall3→2 and blend alpha.24623→.16320. Mean hits across the measured
water region range2.38–3.10/4. Accepted rays hit the authored bright reflector,
not water. The actual per-ray fields include coefficients, directions, terminal
UV/depth/delta, sampled color, iterations and termination reasons, joined to the
exact published source generation/projection/inverse/modelview. Private inspection
preserves the original HDR/native/presented bytes across all108stationary worlds.

The correction separates the coordinates in `almetal_sample_ssr_pattern()`:
**unrefracted receiver screen UV selects the rough-ray pattern; refracted UV still
supplies edge fading**. Ray count, lobe/roughness footprint, normal shading, hit
validity, reflected-radiance estimator and blend weighting are unchanged. Primary
world water uses this coherent receiver-pixel seed in all modes; ordinary material
SSR retains its previous seed and the tile minimum-four policy. The far-rim mirror
is unchanged. No extra pass, texture, history owner or production CPU wait is added.

| Stationary rendered water,108worlds/six18-phase cycles | 69525 shader | Receiver seed | Retained original32-ray comparison |
| --- | ---: | ---: | ---: |
| Presented temporal luma RMS | .007548012 | **.002939398** | .002208 |
| Presented mean luma | .270027512 | **.267975454** | .269993992 |
| Presented adjacent RGB variation | .005079480 | .005743160 | .001489 |
| RGB MAE against32-ray comparison | .004885823 | **.007996786** | 0 |

Temporal modulation falls **61.06%**; mean brightness stays within **0.75%** of
the common high-sample comparison and reflected green/red contrast within0.98%.
This does not fade or blur the water: adjacent variation rises13.1%, and untouched
background/reflector pixels are byte-identical across108worlds. It **does retain
static low-ray error/grain**; its higher MAE against32rays is not concealed as
exact reflection fidelity. The32-ray result is a common sampling comparison, not
a physically perfect reflection oracle. Owner-scene acceptance remains pending.

The globally constant-seed diagnostic was rejected: although steadier, it biased
mean brightness+4.29% and changed the reflected appearance. Freezing a coherent
color/depth/coordinate packet did not remove the original cycle. These completed
diagnostics stay private; neither ships. Original post-water history publication
is also present in pre-retirement Alchemy cd612d0136d1, so water-inclusive history
alone is not diagnosed as a translation defect. Accepted paired-coordinate and
water-depth repairs remain intact; Aurora's reconstruction/mip/rejection policies,
normal water animation, glass, foliage and the shared tile correction are unchanged.

The new production-path regression uses genuine advancing water-inclusive SSR
history and a persistent bright reflector. Its342worlds comprise72warm,
108fixed-animation stationary,72normally animated stationary,18moving and72stopped;
normal animation continues through movement/stopping. One skipped publication
during movement proves that the next world consumes exactly the retained packet,
including color/depth/coverage and coordinate identity. No history reset is used.
Frozen bounds require temporal RMS<.003, mean within3% of the retained comparison,
nonflat reflected contrast and nonnegligible SSR contribution. The69525 shader
fails exactly the temporal bound; the receiver candidate passes. It also passes
the unchanged earlier water-SSR noise bounds (MAE.005172/RMS.009021). Both cases
pass113,240assertions with API/GPU validation. The source-pinned Release and all24selected checks pass:20Metal suites with
API/GPU validation and4CPU suites (161.60s). These include water/SSR coordinates,
glass/visibility, Native/Spatial presentation, history/cancellation and resource
reuse. Full-screen login-only smoke passes and restores18original settingsfiles/
24entries; viewerclosed. Mountedapp verification matches8293entries, signatures and346ARM64 checks pass;
ZIP CRC/all127selected hashes pass. All three iCloud uploads are confirmed
**2026-11-27 08:01:28UTC**, with no error and matching destination hashes.

**Cost boundary:** retained owner ROI runs cannot establish a matched ordinary-view
cost for the shared four-ray tile candidate: A0 has no qualified quiet stream;
B/A1 contain6,480diagnostic readbacks each, and later tails lack matching controls.
No live review session is running, so this conditional comparison is deferred.
The existing **+2.438ms** dense target-size BASE result remains a separate component
measurement. The tile change affects eligible rough materials across Native,
Spatial and Temporal modes, within the configured cap, and remains experimental.
The new water seed adds no rays or allocations; no zero-cost or liveFPS claim is made.

Private raw fields, coherent source packets, source/compiler identities, failed
counterexample, matched PNGs and clips remain under
`artifacts/metal/water-ssr-detail-20261127/`. Clips use12exportedworlds/s deliberately,
not viewerFPS. Normal-animation/movement qualification uses matched final342world sequences: intended
wave motion remains nonzero and every untouched background/reflector pixel is exact
over270savedworlds. No old frozen-animation reference is substituted for move/stop.
Packaging follows separately.

## Source-pinned 69527 delivery candidate

Application source `5d44ab6411a47bad3e437ed2da78179b7121e978`, branch
`codex/aurora-native-temporal-20261126`, Release26.4.0.69527. The code starts from
reviewed app0a94/69525 plus documentation checkpoint4d8e; no later source changes
were present at task start. Preserve69525 and accepted glass/foliage packages.

| Identity | SHA-256 |
| --- | --- |
| Executable | `bac2cd70dc98ec8d483051ed3c8244872349e1aca1b75180cd41db01ce3ffbe9` |
| Built-in metallib | `0e3114b033e28998f138d35a6706d2cf1b97da492ee9808e100200e387cb7aee` |
| Aurora generated MSL compiler input, unchanged | `9dd3a205ff55b2e401f7d3c4b4faba17c62051bd94f39401806b65c95d9f8070` |

The final CMake-built and app-bundled library is byte-identical to the qualified
candidate. No privatebuffer8/inspection shader, global fixed seed, frozenhistory,
omission, mask override or new setting ships. Existing bounds and all PPD610
failures remain unchanged. Actual original-scene water acceptance is pending;
the demonstrated source cause/improvement belongs to the authored production-path
fixture. No live scene login was performed in this follow-up.

The app is ad-hoc signed, not notarized, with the existing macOS26 target.
Select **67% (Aurora Temporal · Experimental)** and check its effective status.
Keep combined lighting OFF. Use macOS window screenshots/recording rather than
independent viewer snapshots for displayed Temporal quality. Package/source/diff,
raw ray fields and paired source packets, matched clips and verification remain
private. The package verification succeeded; the initial cloud metadata at07:58:09UTC
reported the retained PPD618 transient4355condition. The second bounded
unchanged-file recheck confirms all three uploads at08:01:28UTC. No account/service
change, recopy or rebuild was needed. `delivery-complete.json` owns that receipt.

The ray observer counts27.230→27.521 depth evaluations per measured receiver
(21pixels×18phases, about+1.07%). This is algorithm work, **not GPU time**. Selected
rays can change trace duration despite the same ray/pass count; a net timing or
zero-cost claim is not made.

| Private package on iCloud Desktop | SHA-256 |
| --- | --- |
| `Alchemy-Aurora-Water-SSR-Experimental-69527-AppleSilicon.dmg` | `95afe006f274fd84ee5d7f31108abbb723d1ba74a7c3533664004508583c28d0` |
| `Alchemy-Aurora-Water-SSR-Experimental-69527-Review.zip` | `30904db1518260b2751e8ca871e9cdad931d627708b5890d665894cb25250ff4` |
| `Alchemy-Aurora-Water-SSR-Experimental-69527-SHA256SUMS.txt` | `6b82cb72bf0a188ae188719e9b5d79d46ec8e9d0d4e1404a3da7478e1c66080b` |

## Preserved69525 tile candidate and unresolved-water checkpoint

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

## Preserved69525 source-pinned sampling candidate

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

## Current owner-pulse correction — PPD624, 2026-11-27

**Current disposition:** the owner rejected build69527/source`5d44ab6411a47bad3e437ed2da78179b7121e978`:
background water still flashed. The tile repair is owner-accepted (PPD628 verified),
and the glass/foliage repairs remain preserved. The local corrective source is
`136ce1849331f968d7808ae18eaf0f2e95145b19`. Release, regression and packaging gates
are pending; this section does not declare another review application ready.

### Actual contributing operation and controlled response

Owner-session03 joins64 submitted worlds to raw HDR, immediate Aurora, final-world
ROIs and the executed water shader's per-ray records. Camera matrices are fixed,
coefficients remain stable and no history reset occurs. The inherited coarse SSR
condition `abs(source_depth - sampled_depth) < .1` terminates the entire ray before
its hit test. Same-world/history-generation checks and previous actual-water depth
readbacks identify the rejected first samples as the receiver's water neighborhood.
Observed one-evaluation depths agree with those readbacks within5.15e-6 eye units.
One phase7→8 example changes from gap−.059753/one-evaluation termination to
first-water gap−.208549 followed by a21-evaluation reflected-geometry hit. This is
an executed traversal boundary, not an inference from the video's nine-frame pulse.

A primary-water-only coarse advance in live04 reduces the pulse; restoring the
original library in live05 restores it. The narrowed candidate in live07 uses a
receiver-normal band derived from the tracer's existing axial intersection
thickness. Within the near-source branch, only a qualifying sample advances the
existing step and spends its ordinary iteration budget. Refinement rejection,
25-iteration cap, ray count,
receiver seed, ordinary material/far-rim SSR and Aurora reconstruction are retained.
The policy also applies to primary water at exact gloss; the far-rim mirror is separate.
There is no new CPU wait, submission queue, scaler history or resource owner.

The band is **not exact surface identity**. Geometry inside the existing SSR
thickness can be indistinguishable;32epsilon accounts for arithmetic allowance,
not a claimed Metal raster-precision guarantee. Private GPU controls execute the
production helper against rasterized depth: on-plane acceptance, a separate4cm
surface outside the band rejected, zero normal rejected, and a separate1cm surface
inside the configured band accepted. The last case is a documented limitation,
not a hidden passing occlusion claim. Water MVP/inverse-projection jitter was also
checked: the production color-scene copy already applies both. Earlier original-
scene metadata was unjittered; adding jitter again was rejected without an edit.

### Rendered regression and retained negative evidence

The valid authored case uses384×256→576×384, real production water, an above-water
reflector visible in the published scene history, production normal phases/mips,
and continuous history. It records112,520 rays changing from first-water rejection
to later wall hits; source inputs and random coefficients are exact across arms.
The six frozen quality bounds retain detail and brightness alongside phase metrics.

| Authored raw-HDR water ROI | Original69527 shader | Guarded candidate |
| --- | ---: | ---: |
| Phase-fold luminance range | .01026275 | .00343221 |
| Mean RGB gradient detail | .00593086 | .00783284 |
| Mean luminance | .143028 | .139605 |

The original fails the pulse bounds; the candidate passes all six. This demonstrates
a rendered fixture correction, not confirmation of the owner's final image quality.
The original96×64 lookalike did not enter the same self-depth branch, and an earlier
distant reflector was outside the sampled scene image. Both remain failed
counterexamples, not qualification. Strict independent input retracing also remains
unqualified: recorded-UV sampling matches72,951 RGB components exactly, but retracing
differs at55 pixels, maximum.0017395. That diagnostic parity gate is not waived or
used to supply a counterfactual image. Live intervention and production rendering
provide the separate causal evidence.

### Narrowed live07 result and limits

Live07 has64/64 native main-post and observer joins,2,560 collected ROI copies,
zero drops/resets and a fixed camera. The balanced54-world, always-water interior
comparison has these immediate-Aurora phase-fold ranges:

| Water patch | Restored live05 | Guarded live07 | Reduction |
| --- | ---: | ---: | ---: |
| A | .002751 | .002010 | 27% |
| B | .008373 | .001822 | 78% |
| C | .005733 | .002113 | 63% |

This is an observational comparison with confounds: live07 has48 configured probes
in52 recorded frames and49 in11, versus37 in live05 (one07 BASE metric is missing).
Normal animation times also differ. Water mean luminance changes−7.50/−9.35/−12.35%
for A/B/C versus control+.54%; gradient detail changes+19.38/−9.57/−7.87%, control−.93%.
Neither a brightness-preserving live gain nor equal whole-scene inputs is claimed.
The remaining worst positive native transitions are.000720(A6→7),.000706(B0→1),
and.001052(C16→17). A16→17 still falls−.001503, essentially unchanged from live05's
−.001508; the lower overall range does not establish removal of every pulse.
More coarse evaluations may cost GPU time; actual-view cost is unqualified. The tile's separate+2.44ms synthetic BASE result is not this repair's cost.

Private evidence: `artifacts/metal/water-pulse-20261127/owner-session-03/` through
`owner-session-07/`, `boundary-analysis/first-step-depth-results.json`,
`boundary-analysis/retained-arm-comparison-with07.json`, its review Markdown/per-world
CSV, `receiver-guard-control/`, and the authored visible/dense fixture outputs.
Live07 clips use64-world UI-selection timing spanning4.305443333s; they are texture
exports with a current-world UI association, not monitor recordings. Final source,
full validation, cost disposition and package identities must be added after their
gates complete. PPD624 stays open for owner-scene quality review.


The new permanent receiver-band GPU test first exposed PPD629: compiling the
complete builtin through the SDK-default runtime dialect rejects the existing
`coherent` local identifier, although the packaged Metal3.1 library compiles.
The test now explicitly uses Metal3.1, fast math and invariant preservation,
matching the shipping compiler contract; no unrelated production shader or
runtime-compiler policy changes are made here. A test-only crash formatting a
nil NSError string was independently identified from its `strlen`/Catch stack
and corrected with non-null diagnostic strings. Initial failures remain private.
The original library now fails exactly three pulse bounds in the permanent
production test; the candidate passes all six unchanged bounds (range.00343185,
detail.00783295, mean.139605). Additional receiver-control/regression checks and
final delivery gates remain in progress.


### Release69530 verification checkpoint

Application source is `136ce1849331f968d7808ae18eaf0f2e95145b19`; later test and
report changes do not change shipping renderer code. The normal Release build
completes, all20 selected Native/Spatial/Temporal/SSR/resource/shader regressions
pass with API/GPU validation, and all ten focused water cases pass across split
runs (21,051,715 assertions). The permanent receiver-band test now passes after
matching the offline dialect and guarding optional NSError messages. Original
failed diagnostics remain retained; no image-quality bound was relaxed.

The full-screen login-only smoke reaches the login screen with automatic login
disabled, then closes. All18original preference files/24entries and restorable
metadata match the initial snapshot. Architecture verification passes346Mach-O
checks. Ad-hoc signatures are used; the application is not notarized.

- Executable SHA256: `d2c727d7f67466b4ff99847817d0c10f2c2134bdcc23666400064eec0db26565`.
- Builtin library SHA256: `12c1041744f8947d4967bdf3d79a92685abc63e071c1524ea5f450b922b2f789`.
- Embedded Aurora compiler-input SHA256: `9dd3a205ff55b2e401f7d3c4b4faba17c62051bd94f39401806b65c95d9f8070`.

The experimental review package retains69527 for comparison. Select Advanced
Graphics → World resolution → **67% (Aurora Temporal · Experimental)** and verify
that the effective status names Aurora rather than a fallback. Keep combined
main lighting OFF. Review Midday water detail/brightness, normal animation,
camera movement/stop and the accepted glass/foliage/tile controls using macOS
window capture; the independent viewer snapshot is not displayed-output evidence.
Mounted package/content and upload receipts are kept with the private delivery.
PPD624 remains open, including the remaining A transition and actual-view cost
qualification; no whole-viewer FPS improvement is claimed.


The normal Release `almetal_tests` module also passes the198-world pulse case
with API/GPU validation (716321assertions); its builtin library is byte-identical
to the delivered application's `12c10417…` asset. Separate matched media export
uses the identical renderer and only CPU crop writes after the existing completion
wait:72warm →72stationary →18moving →36stopped worlds. Both normal-animation
sequences are retained at18worlds/s preview (0.6× authored time), not live FPS.


### Final safety rejection — 69530 WITHHELD

The uploaded69530candidate is **withheld**, not ready for owner review. A final
ray-safety check finds4266coplanar accepted hits, including2077from downward rough
rays. Coplanarity alone is not identity, but3190of those samples fall within the
exact named preceding world's retained water ROI: all have coverage255 and match
actual water depth within4.38µm. This proves receiver-water feedback for that
subset. Other positions remain unassigned. The previous pulse/detail bounds did
not catch this validity defect (PPD630).

The69530files are preserved in the clearly labelled private iCloud Desktop folder
`WITHHELD-69530-DO-NOT-TEST`; original69527is unchanged. Continue only after a
water-only production counterexample and a correction that rejects wrong-side
rays/receiver samples while retaining genuine reflected geometry. No successful
handoff or owner acceptance is claimed. The final current state supersedes the
earlier Release/package checkpoint above.


The PPD630 water-only counterexample now fails69530 in32of36measured worlds,
with64RGB checks failing, maximum per-world MAE.00577471 and maximum channel
error.0995483. Both arms use72warm worlds, advancing genuine water-inclusive
history and the same normal animation/geometry/probes; neither submits the
above-water reflector. Zero RGB/alpha difference from inactive SSR is an
independent validity oracle, fixed before the revised candidate. Reset checks
use authoritative counters (the initial sparse-snapshot default was not a real
reset and its invalid assertion is retained). The public regression preserves
that oracle; the existing genuine-reflector quality bounds remain unchanged.


### Revised receiver validity — component qualification

PPD630 is corrected in the candidate by treating receiver-surface samples as
empty throughout coarse and refined tracing, and rejecting primary reflection
directions that leave toward the wrong side of the receiver. Camera side is
cached in current eye coordinates before history reprojection; the sign test
is unchanged if the geometric normal is reversed. The same nearest depth fetch
feeds ordinary linear-depth reconstruction and the receiver-band test. Existing
trace thresholds, total iteration/ray budgets, receiver seed, material paths,
far-rim mirror and Aurora algorithm remain unchanged. Separate geometry inside
the existing intersection-thickness band is still ambiguous, not exact water
identity. No CPU wait, new render pass or resource is introduced.

The exact water-only counterexample now passes all1,532,232 assertions with
API/GPU validation: maximum RGB MAE and maximum channel difference are both0,
versus69530's.00577471/.0995483 (64failed RGB assertions in32of36worlds).
The independent zero oracle and original failed baseline remain unchanged.
The genuine-reflector198-world sequence passes its frozen bounds with normal
animation and moving/stopped camera: range.000673403, jumps8→9.000170559 and
17→0.000257832, detail.008124224, mean.140210183. Original69527 values were
.010262750/.008130452/.004024902/.005930859/.143027696 respectively.
These are fixture results, not owner-scene acceptance or a performance result.
Private receipts: `receiver-validity/compile-frozen-receipt.json`,
`water-only-candidate.log`, `visible-quality-result.json` under the retained
`artifacts/metal/water-pulse-20261127/` root. Complete production water suite and
exact-view candidate safety/quality joins are the next gates.


### Sourcef971 /69531: receiver-validity repair and retained quality limit

Application source `f971d11de29daf60233f9f5875cf957ca069e95c` supersedes the
withheld69530renderer. All11production water cases pass with API/GPU validation
(22,609,127assertions), including the exact-zero feedback counterexample,
198-world genuine-reflector frozen bounds, normal animation/move-stop, skipped
history publication, visibility/exclusion and pool reuse. Test-only export of
the unchanged198-world sequence produces matched stationary and move-stop media
(716,701assertions). Previews are18worlds/s,0.6×authored animation time, not FPS.

The saved Midday camera was reproduced through the same69527host plus private
observer instrumentation and the revised shader, not mislabelled as a69531live
executable. Since69527, shipping viewer changes are only the builtin shader;
CMake/test changes are test-only. After30main frames at49probes,64worlds and
64observer packets join exactly; all2560ROI readbacks complete, no ROI/observer
drops, no history resets. Native quiet stream has302contention drops (none
budget/full); this is not lossless general timing or a performance comparison.
The actual decoded coverage is86,460water pixels /345,840rays. All2333wrong-side
rays terminate before stepping and have zero hit/terminal data. All68,796hits
point upward; none is within1mm of the receiver plane or below it. Exact named
source-world checks find no accepted water-coverage sample;67,610locations outside
retained source ROIs and1169missing-first-source cases remain semantically
unassigned. A plane-height classification is not object identity.

The original first-depth near-source test terminated the whole ray on the water
receiver. Continuing a valid outgoing ray fixes that rejection; testing every
coarse/refined sample also prevents the69530feedback counterexample. The new
rule does not alter receiver seeds, reflection blend strength, normal animation
or Aurora. It does not fix every source of visual variation.

Common eroded water interiors, first54worlds =three complete18phasecycles:

| Region | Immediate Aurora phase-mean range, baseline→candidate | Final-world range, baseline→candidate | Immediate mean /detail change |
| --- | --- | --- | --- |
| A | .00275143→.00225581 (−18.0%) | .00538178→.00589685 (+9.57%) | −8.52% /+33.0% |
| B | .00837274→.00290594 (−65.3%) | .01912146→.00662771 (−65.3%) | −2.57% /−7.08% |
| C | .00573252→.00092602 (−83.85%) | .01188772→.00301068 (−74.67%) | −11.61% /−4.01% |

Baseline05has37configured probes; candidate09has49. Animation time differs.
These are descriptive scene comparisons, not identical-input quality estimates.
The non-water control mean changes+.96% immediate /+.85%final. A's former
phase16→17 immediate drop−.00150782 becomes+.000653934; other variation remains,
and its final range is worse. Thus neither all-pulse removal nor original
owner-scene acceptance is established. PPD624 stays open; reflected brightness,
detail and residualA require owner review. PPD630moves only to ready_for_owner.

Recorded per-valid-water-ray loop entries rise19.796→24.780; derived depth
fetches19.785→24.760 (+25.1%). These are shader work counts, not GPU time.
No pass, texture, buffer or CPU wait is added. Actual-view rendering cost is
unqualified: this targeted capture ran alongside the Release CPU build and
used readback observers. Do not transfer old dense tile +2.44ms into this view.
The shared minimum-four-ray tile correction remains owner-accepted.

Private evidence: `receiver-validity/live09-complete-analysis/analysis.json`,
`live-ray-safety-09.json`, `live-water-feedback-proof-09.json`,
`retained-arm-comparison-05-vs09.json`; `fixture-validity-comparison-media/`.
Session08used an unsupported512MiB diagnostic cap and was closed without capture;
09uses the supported256MiB cap, verified at startup. Both sessions restore all
18original settings files/24entries and restorable metadata; all viewers close.
Normal69531Release/regression/smoke/package gates remain pending at this checkpoint.


The09quiet-native join also resolves64/64frames4155–4218 to64unique successful
and GPU-timed main-post commands, exact Aurora/output resource generations and
current-world UI selections, with0join exclusions. Clock calibration/units are
retained in `receiver-validity/native09-joins/`. This is current-world UI selection,
not independently observed drawable presentation; logical-group/admission IDs
are not recorded here. General quiet losses remain302contention events as above.
Three single-pixel generated-reactivity observations occur in A; B/C/control
are zero. No reset occurs; those isolated pixels do not establish a cause of
A's residual variation. Matched-phase actual-view clips are retained privately
at fixed18worlds/s, not liveFPS; owner recordings are not published to GitHub.


Responsible shader locations at applicationf971: `almetal_ssr_ray()` in
`indra/almetal/shaders/almetal_builtin.metal`, current-camera-side test line6963,
coarse receiver exclusion line6999, refinement exclusion line7055. The reused
nearest-depth/receiver-band helper is immediately above. Generic material and
far-rim callers pass zero receiver normal and retain their previous semantics.
The permanent regression cases are `[water-ssr-self-depth]`,
`[water-ssr-receiver-band]` and `[water-ssr-no-feedback]` in
`indra/almetal/tests/almetal_temporal_water_test.mm`.


### 69531 Release and exact-application verification

Normal `tools/metal/build.sh build` completes at sourcef971 (2486build targets);
PPD615's known incremental-cache recovery is retained, without a code/build
workaround. `doctor.sh` identifies the M1Max and required OpenHTJ2K0.27 decoder.
All21affected Release CTest modules pass with API/GPU validation in181.85s,
including11water cases /22,583,776assertions in the actual shipped-library module.
Its real-reflector range is.000673392, detail.00812429, mean.140210; water-only
RGB MAE/max remain exactly0. All original quality bounds remain unchanged.

The staged review app is `Alchemy Aurora Water Validity Review69531.app`
(filename has spaces around69531). Executable SHA256:
`fd5228bbce87246903e631ed6570d77059f393dc453fedc58668d0a864f4f999`.
Shipped builtin SHA256:
`86ebf38d847cd9ef9b37afbbaff5981d1bdf57309e8e195f6ff83da0765e12a5`.
Its compiler input SHA256 is the verified revised source
`cad104596abc63d1e44fe979ef5de76a2d9c62ec097d587db2831e7d2a236896`;
the full production Metal3.1/fast-math/invariance/min26compiler command is retained.
The private media library05bd… has identical source/options and distinct binary
identity; it is not mislabelled as the packaged86eb… asset. Normal Release tests
exercise86eb… directly. Embedded Aurora sources remain9dd3a205… unchanged.

Deep/strict ad-hoc signatures and346ARM64 checks pass. The exact application
reaches the native-full-screen login screen with AutoLoginFALSE and no account
login; it closes normally. All18original settings files/24entries and restorable
metadata match the initial snapshot. No private login screenshot is published.
The application is not notarized. Mounted package/ZIP/upload checks are the
remaining delivery gates; their receipts will follow. PPD624 remains open.


### 69531 private delivery complete —2026-11-27 22:22:18UTC

The mounted DMG matches all8293application entries; deep/strict ad-hoc signatures
and346ARM64 checks pass. All50selected review-ZIP files match their manifests
and ZIP CRC checks. The DMG, source/evidence ZIP and SHA256SUMS are confirmed
uploaded on iCloud Desktop, with no remaining upload error and matching
destination hashes. The initial4355account-access status clears without any
account/service change.69527is preserved;69530remains explicitlyWITHHELD.

- `Alchemy-Aurora-Water-SSR-Experimental-69531-AppleSilicon.dmg` —377163731bytes,
  SHA256 `1b0ad6fcfc6fc5deacc228acaa330935a7d88f505a54c67fd925ce754c9135a7`.
- `Alchemy-Aurora-Water-SSR-Experimental-69531-Review.zip` —39414037bytes,
  SHA256 `6aab0e1ef70fbd43025de445d2e85336123091f94074b1efc939045b548e6523`.
- `Alchemy-Aurora-Water-SSR-Experimental-69531-SHA256SUMS.txt`.

Immutable application source staysf971 /69531. Final report/register/state
publication is documentation-only after packaging; it does not relabel the
application source. The ZIP preserves the source pin, actual executable/library
identities, source diff, correctness logs, frozen bounds, synthetic and selected
private water-only ROI before/after media. Original owner movies, raw recordings,
account data and scene asset files are excluded; owner-scene media is not pushed
to GitHub. The application's login screenshot stays private and excluded.

Select Advanced Graphics → World resolution →67% (Aurora Temporal ·Experimental)
and confirm effective Aurora status; combined main lighting staysOFF. Native
remains default, and Spatial/MetalFX remain available. Review Midday outside-pool
water, reflection brightness/detail, movement–stop, shoreline and accepted
glass/foliage/tile controls with macOS screenshots/recording, not independent
viewer snapshots. No routine CPU wait, extra pass or allocation was introduced.
Actual-view GPU cost remains unmeasured; observed tracing work rises25.1%.

**Disposition:** ready for owner review of a source-confirmed SSR receiver-validity
correction, not a completed water-quality fix. PPD630ready_for_owner; PPD624open
with residualAfinalrange+9.57%, B/Cimprovements, mean/detail/probe confounds and
owner acceptance pending. Do not report all blinking removed or a viewerFPS gain.

Final documentation harness initially catches the ledger retaining69527as its
primary checkpoint while CURRENT identifies69531. Both primary checkpoints now
name immutable application sourcef971; the69527reference is explicitly preserved
as historical. This documentation correction does not rebuild or relabel69531.
