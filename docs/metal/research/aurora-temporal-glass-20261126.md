# Temporal through-glass stability — PPD619–PPD622

The corrected production-path fixture is steadier through legacy and multi-slot
PBR glass while retaining more detail. It also exposed and corrected exact-static
motion errors in layered surfaces and sky reprojection. These are rendered-fixture
results, **not acceptance of the owner's original glass-fence view or a live FPS
claim**. The actual fence's material family remains unconfirmed. PPD610's original
palm recovered-area and settling failures remain recorded.

Work began at `codex/aurora-native-temporal-20261126` / `74c07274bb908df84648610b948259bb88c55004`.
The preserved baseline renderer/application source is
`b6f18c24f4e32e5e8897f782c0e4b65bda521f8f` (69516). The intermediate coverage/content
checkpoint is `bf90c0dd728bc642247ce863ea1ca96a41a5ea54`; PPD622 follows it.
No live login or owner-scene capture was required. Final application and package
identities belong to the delivery receipt; this report does not assign them.

## Causes and repairs

### Coverage and material identity: PPD619–PPD621

The baseline excluded legacy material BLEND and multi-slot PBR from reliable
standard-alpha history. The valid static fixture reproduces full unsafe-history
rejection for both, despite valid previous poses and zero independently generated
Aurora transparency reactivity in the measured pane region. Single-slot PBR was
already qualified and is the unaffected control.

Legacy color coverage is `max(diffuse.a, glare) * vertexAlpha`, including reflected
specular glare, rather than raw diffuse alpha. Original `upstream/develop`
`408fd6c014e6`, `class3/deferred/materialF.glsl` confirms that contract. The repair:

- Shares the actual legacy shading/coverage helper between color and Temporal
  replay, including normal/specular UVs, light/probe/shadow/SSR/hero inputs,
  minimum-alpha and water discards. A zero-diffuse-alpha texel with visible glare
  supplies the foreground inputs rather than disappearing from the replay.
- Qualifies validated PBR BLEND slots with matching per-slot transformed UVs and
  streamed vertex alpha. Both UV and alpha selection clamp to the populated slot
  prefix, matching color. The rendered selector5/two-slot counterexample now
  matches selector1; generic32-texture semantics are preserved (PPD620).
- Carries the existing authored `Payload::producer_revision` into transient
  `temporal_material_revision` and exact material-history equality. In-place
  published image changes invalidate the affected draw while equivalent
  republication remains eligible, without inventing a second history owner
  (PPD621).

The policy retains exact-zero contribution, full-coverage replacement and
conservative handling of genuinely mixed visible motion. Partial layers require
identical representable RG16 motion; no rolling tolerance is introduced. The
shared R8 signal is named unsafe-history confidence. MetalFX consumes it as a
reactive weight; Aurora's adapter maps it to the pinned algorithm's composition
marker. Aurora separately generates transparency reactivity from genuine
opaque-before-transparency and final HDR color. Those signals are not conflated.

A localized **test-only** rejection override was the causal control. With all
other inputs retained, it reduced legacy stationary variation from.000901808 to
.000686735 and increased detail from.028473880 to.035373061. The shipping repair
is the matched coverage/identity policy, not that override.

### Exact stationary motion: PPD622

After repairing coverage, a settled overlapping-layer counterexample still
rejected980/1008 contributing pixels. Isolating the farther layer showed nonzero
motion at958/1008 pixels despite valid history and bit-identical current/previous
MVPs. The ranges were x−7.7486e−7..7.1526e−7 and y−8.3447e−7..4.1723e−7 input pixels.
A nearer layer had exact-zero motion, so the strict RG16 coherency check correctly
detected the supplied disagreement. Weakening that check would conceal the faulty
producer and permit genuinely different visible motion.

A separate static modern-sky probe reproduced the same issue in camera motion:
23296 nonzero pixel observations across128×64/18jitter phases;4714 across127×63/two
phases; maximum≈1.85e−6 input pixels. Identical camera matrices left tiny
inverse/multiply residue, and the UV/clip round trip added numerical motion.
Clear/non-sky background was already exact zero.

The repair preserves mathematical zero only with an exact producer proof:

- [Motion replay](../../../indra/almetal/almetalworld_motion.inl) checks valid
  prior state, identical MVP, joint-palette bytes and relevant cloth inputs.
  Stable resource generations/history validity remain prerequisites. A shader
  flag emits literal zero for those unchanged positions; it grants no history
  eligibility by itself.
- [World history](../../../indra/almetal/almetaltemporalworldhistory.cpp) retains
  all existing invertibility/product validation, then canonicalizes identical
  camera MVPs to exact identity. Only that exact identity enables zero camera
  motion in the background shader.
- [Shader resolve](../../../indra/almetal/shaders/almetal_builtin.metal),
  `almetal_temporal_resolve_surface` and
  `almetal_temporal_camera_motion_fragment`, preserve invalid-pose, reset,
  dynamic-sky and water rejection. Genuine small camera/geometry motion stays
  represented. Jitter is still supplied independently; it is neither frozen nor
  counted as surface motion.

No reconstruction algorithm, opacity, mip policy, quality threshold or motion
agreement tolerance is changed. The unqualified69519 build was stopped before
packaging when the overlap defect was discovered.

## Matched rendered quality result

The authored fixture renders128×64→192×96 at rational2/3 scale with the production
mip policy and18-phase jitter sequence:108 warm worlds,54 stationary worlds,
18 camera-movement worlds and36 stopped worlds. The same chronological scene and
camera sequence is retained. RGB means measure successive output change through
partially covering tinted panes; they do not compare the tint against an
unobstructed image. Detail is measured independently so blur cannot qualify.

The final comparison uses retained `baseline-output-03` and `qualified-output-01`.

| Path | Covered RGB change, before→after | RMS change, before→after | Edge detail, before→after |
| --- | ---: | ---: | ---: |
| Aurora legacy BLEND |.000901808→.000710947 (−21.16%)|.001645373→.001172545|.028473880→.034480436 (+21.09%)|
| Aurora multi-slot PBR BLEND |.000900528→.000707896 (−21.39%)|.001658335→.001169021|.028634528→.034682217 (+21.12%)|
| Aurora single-slot PBR control |.000710783→.000710781|.001175347→.001175349|.034622804→.034622798|
| MetalFX legacy BLEND |.012875387→.001980066 (−84.62%)|.015695999→.002861568|.026212885→.034899263 (+33.14%)|

Aurora's unobstructed control remains.001000411; all216worlds retain identical
[control-region RGB pixels](media/aurora-glass-20261126/output-control-parity.json).
Single-slot PBR changes only at numerical noise scale, rather than receiving the
legacy/multi-slot improvement. Both Aurora baseline/candidate already satisfy the
existing1/255 mean-change bound; that bound alone did not detect this defect.
The regression additionally checks the real history mask, output/detail agreement
with the localized diagnostic and actual input coverage.

**Final input parity is not wholly bit-exact.** Saved stationary world118/phase10
has one changed input pixel at(x37,y25), shared across the four arms. The largest
channel difference is opaque green.5205078125→.52685546875. Maximum full-image RGB
MAE is2.58287e−7 and covered-region MAE1.39203e−6; alpha and the uncovered input
control remain identical. All other saved phases are identical. The earlier
candidate01 comparison had exact input parity; that stronger claim must not be
transferred to the final run. The cause of this narrowly scoped difference is not
attributed. [Per-pixel scope](media/aurora-glass-20261126/input-difference-scope.json)
retains the observations.

[Exact metrics/per-world observations](media/aurora-glass-20261126/glass-comparison.json),
[artifact identities](media/aurora-glass-20261126/manifest.json),
[stationary legacy comparison](media/aurora-glass-20261126/legacy-production-stationary54-12fps.mp4),
[multi-slot comparison](media/aurora-glass-20261126/pbr-multi-production-stationary54-12fps.mp4)
and [legacy move-stop sequence](media/aurora-glass-20261126/legacy-production-move18-stop36-12fps.mp4)
provide the matched output. Baseline is left, corrected output right. Clips play
fixture worlds at12fps: stationary54worlds=4.5seconds; moving18+stopped36worlds=
4.5seconds, with the stop at1.5seconds. This is review playback, not measured viewer
cadence. Full-size and1:1 PNG crops are included; preview RGB is HDR-clamped[0,1]
and gamma1/2.2. Private float textures remain authoritative.

## Movement-stop tradeoff

The corrected image is steadier between adjacent stopped worlds, while useful
history gradually restores detail. It does **not** establish faster settling.

| Aurora path | Adjacent RGB change, stop1–18 before→after | Stop19–36 before→after | Same-phase first→second stopped-cycle drift, before→after |
| --- | ---: | ---: | ---: |
| Legacy |.001108394→.000702759|.000889519→.000675414|.002108630→.005128708|
| Multi-slot PBR |.001105941→.000701738|.000887392→.000674463|.002325282→.005200547|

Corrected legacy detail rises.028125917→.030419141 between stopped cycles, below
its stationary.034480436. Multi-slot detail similarly rises.028185011→.030484745.
The old forced rejection was persistently softer and noisier, with less history
recovery. The36 stopped worlds contain no independently mature final-camera
reference: cycle drift is not mature settling error or a measured old-history
fraction. These records do not establish absence of ghosting. Existing palm
recovered-area/settling failures remain unchanged; live owner review must judge
fine detail, pane edges, reflections and stop response.

## Focused verification and retained failures

The rendered cases are in
[almetal_temporal_glass_test.mm](../../../indra/almetal/tests/almetal_temporal_glass_test.mm).
They cover actual legacy/PBR forward color, immutable opaque-color capture,
production motion/depth/rejection and native Aurora; MetalFX uses the same inputs.

| Check | Recorded result |
| --- | --- |
| Historical full glass/history qualified run |30/31cases pass,74131/74132assertions. The only failure is the new sky fixture's missing active command batch before opaque-depth capture, before its measurement. Production glass, overlap, content-history and motion cases pass.|
| Final linked69520 tests |All31cases pass:68498assertions/eight rendered glass cases and2099assertions/23history cases, with API/GPU validation. Viewer and test Metal libraries are byte-identical.|
| Corrected sky fixture retest |1076assertions/one case pass with API/GPU validation; ordinary batch starts before capture/motion and commits before readback/history advancement.|
| Exact static surface/input targeted run |8684assertions/two cases pass with API/GPU validation. Three families×18phases require literal zero motion; a genuine.0001m camera step still produces motion/rejection.|
| In-place image publication |131assertions/two cases pass with API/GPU validation. Real in-place GPU pixels change the source epoch without changing allocation identity/generation; equivalent republication remains valid.|
| Backend regressions |18selected suites pass, including Aurora inputs, Temporal visibility/presentation/orchestration/motion/history, Spatial presentation/scaler, resource ownership/planning and material sampling.|
| CPU settings/resource regressions |Three suites pass: resource planning, native graphics preferences and preset round trip.|
| Offline analysis |Six tests pass for input/output stride, ROI, detail, phase/sequence completeness and zero-denominator handling.|

[Qualified run](media/aurora-glass-20261126/qualified-glass-history.log),
[sky retest](media/aurora-glass-20261126/static-sky-retest.log),
[surface/input tests](media/aurora-glass-20261126/static-motion-inputs.log),
[publication tests](media/aurora-glass-20261126/content-publication-tests.log),
[backend regressions](media/aurora-glass-20261126/gpu-regressions.log) and
[analysis tests](media/aurora-glass-20261126/analysis-tests.log) retain the results.
The [baseline fixture receipt](media/aurora-glass-20261126/baseline-fixture-identity.json)
and [sky-retest receipt](media/aurora-glass-20261126/sky-retest-fixture-identity.json)
identify those exact test executables, archives and bundled libraries; they are
not final viewer-package identities.

Earlier failed attempts remain private and excluded from quality attribution:
nonzero unused weight/cloth offsets violated the batch contract; then a Y-sign/
winding error culled the scene. Mandatory foreground-contribution/background-detail
gates now reject those empty fixtures. Valid baseline03 fails14assertions.
Candidate01's three overlap-oracle failures incorrectly demanded rejection across
an ROI only62.17% covered by the new layer. The oracle now compares actual
same-jitter visible contributions. The subsequent settled-overlap failure was
real numerical motion (PPD622), and was corrected without relaxing that oracle.
The later sky fixture batch setup failure is separately retained above.

## Cost and remaining scope

Legacy coverage reuses the lighting-dependent helper during the existing motion
pass. It adds shading work for visible legacy glass, two pipeline variants and
constant bindings, **no new render pass or full-frame target**. Its live cost is
unmeasured. No Home FPS improvement or precise cost claim is made.

Native/Spatial and existing MetalFX modes/defaults, original output-based shadow
budgets, native UI, submission/cancellation ownership and the pinned Aurora
algorithm remain intact. This repair does not make independently moving layered
transparency representable by one vector. Animated UVs, invalid poses, image
content updates and genuine disocclusions retain conservative treatment. The
owner fence's actual family is still unconfirmed. The bounded fixture improvement
supports an experimental owner-scene review, not a claim that the live complaint
has already been resolved. The prior69516 artifacts remain preserved.


## Corrected application identity and handoff

Release **26.4.0.69520**, application source
`2af7ebba82b6bfd15886069f1eb9613f34fcef18`, is a separate
`Alchemy Aurora Glass Experimental Review.app`. The full Release build passes;
346 Mach-O files are ARM64, and deep/strict ad-hoc signing passes. It is not
notarized. The full-screen login-only smoke reaches the login screen, closes
normally without login, and verifies restoration of every original settings byte
and restorable metadata.

- Executable SHA-256: `210e71ca74849ebacc889304d1ce3c35435808e9e805d1b982b4d760b0c34f72`.
- Builtin Metal library SHA-256: `47e1500f1717ad3c5c19a569b7316a2d45dcdea4ba3d7b4f29825f301d3e1b55`.
- Embedded pinned Aurora MSL source-header SHA-256: `9dd3a205ff55b2e401f7d3c4b4faba17c62051bd94f39401806b65c95d9f8070`.

All four complete Aurora source blocks are present in the executable. The viewer
and rendered-test builtin libraries match, and the final actual-production glass
cases execute successfully. [Exact linked identities](media/aurora-glass-20261126/final-linked-identities.json)
and [final test output](media/aurora-glass-20261126/final-linked-glass-history.xml)
prevent confusing this build with an earlier inactive candidate.

Select **World resolution →67% (Aurora Temporal · Experimental)** and verify
**Active:67% Aurora Temporal (Experimental)**. A fallback reason or Pending status
is not Aurora execution. Keep combined lighting OFF. Use macOS window screenshots
or screen recording for the original fence/palm/grass comparison; independent
viewer snapshots do not reliably represent displayed Temporal output. Check
stationary detail, pane edges, moving content, camera movement then stopping and
overlapping panes. The live fence family and owner-scene result remain pending.

The delivery workflow creates newly named69520 DMG/review ZIP/checksums on iCloud
Desktop and includes mounted-content/signature/architecture verification. Its
archive and upload completion receipts are recorded separately after packaging;
this implementation evidence does not substitute for those checks.69516 is retained.


### Verified private delivery

At **2026-11-26 20:57:47UTC**, Foundation metadata confirms all three iCloud
Desktop files uploaded, not uploading and without error; destination SHA-256
matches local verified packages. The initial4355 transient and successful
unchanged-file recheck remain in PPD618. No account/service changes were made.

| iCloud Desktop file | SHA-256 |
| --- | --- |
| `Alchemy-Aurora-Glass-Experimental-69520-AppleSilicon.dmg` | `57e4c2d764b02dfebd43c1610461ed025b566bb38f435d2b0636a0503e218673` |
| `Alchemy-Aurora-Glass-Experimental-69520-Review.zip` | `1e22eb729b2d40be8eb3731716f8a9e49b8102306678fd90ec127d9f341020c5` |

The matching `Alchemy-Aurora-Glass-Experimental-69520-SHA256SUMS.txt` is alongside
them. The mounted DMG matches all8293app entries; deep/strict signing and346ARM64
checks pass for source and mounted apps, and ZIP CRC/per-file verification passes.
The ZIP contains the pinned source archive, focused diff, rendered evidence,
identities and application-validation snapshot. That snapshot predates archive
creation; included DMG verification and this final delivery record cover the later
steps. Source remains2af7ebba82; subsequent branch commit contains only evidence
and documentation. Viewer is closed and original preferences are restored.

Build command: `AL_METAL_BUILD_DIR=build-aurora-native-release AL_METAL_JOBS=4 tools/metal/build.sh build` (exit0),
after `cmake -S indra -B build-aurora-native-release -DAL_CHANNEL:STRING="Alchemy Aurora Glass Experimental Review"` (exit0).
Final API/GPU check: `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 ctest --test-dir build-aurora-native-release -C Release -R '^almetal_tests/almetal_temporal_(glass|history)_test$' --output-on-failure --output-junit artifacts-glass-final.xml` (exit0).
Private raw build/delivery logs remain under `artifacts/metal/aurora-glass-20261126`.
