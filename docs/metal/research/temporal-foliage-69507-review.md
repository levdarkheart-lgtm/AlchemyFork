# Temporal 69507 experimental review: visibility repaired, foliage quality unresolved

**This build does not demonstrate a correction of the remaining stationary foliage defect.** The final original-eight-ROI fixture still fails all twenty frozen quality bounds. Its 144 output PPMs and eighteen raw stationary output textures are byte-identical to the 69506 fixture08 baseline. The new visibility repairs fix separate, reproduced motion/depth/rejection correctness errors; they do not turn the remaining MASK contrast/settling failures into passes. PPD-610 remains open.

## Source and verification scope

- Review application source: `b73abe1acdb65f96239ce70ac4cdf4c0ca83827a`, branch `codex/metalfx-temporal-20261125`, build 69507. Later documentation commits are distinct from application source.
- Final quality/correctness test executable SHA-256:`8af3b7c7683cb72ea4820221382065b9498bc99c0ed675f70f36772bce1d80ae`; shader library:`5d10cbcaefd65d98e50fb2894870a3364e30527b28b7dfd6addc7c7436c09e8a`. Receipts record parent HEAD `be565c16` plus the working corrections subsequently committed as `b73abe1a`; they are not clean-parent execution claims.
- Metal API/GPU validation was enabled. No new Home/Club login, live image-quality or FPS acceptance is claimed. Native remains the default; existing Spatial/Temporal identifiers, quality settings and admission policy are preserved.
- Clean Release build,346 ARM64 checks, deep/strict signature verification and a fullscreen login-screen-only smoke passed. Automatic login was disabled; all 17 touched settings XML files were restored and the viewer left closed. Application executable SHA-256:`fefb2ca48f997c232966ef191ef0e50308d4747018d958af9395960a60c271f8`. DMG/delivery receipts are separate from these source and test identities.
- Artifact names below are under private `artifacts/metal/temporal-foliage-20261126/`; the immutable baseline is [temporal-foliage-quality-69506-baseline.md](temporal-foliage-quality-69506-baseline.md).

## Implemented visibility corrections

`almetalworld_motion.inl` now gives motion/rejection replay its own memoryless depth attachment, seeded from pristine opaque depth, and follows the actual forward color draw order plus read/write depth policy. This keeps hidden layers from changing visible Temporal inputs while preserving depth-only occluders. The independent motion/depth proxy and main post-processing depth retain their separate roles.
`almetal_temporal_motion_fragment()` matches the applicable water-plane clip, material/vertex-alpha and discard rules. Exactly zero standard-alpha contribution retains the represented inputs; a reliable full-alpha layer replaces preceding color/history state; fractional layers preserve rejection from unsafe visible contributors. Motion agreement uses equality in the stored RG16 representation, avoiding a chain of individually small tolerances accepting incompatible layers. An opaque/MASKdraw does not receive an unrelated forward-alpha discard rule. No global acceptance of unreliable history or quality-policy reduction is used.

### Reproduced counterexamples and expanded final coverage

| Pre-repair checkpoint | Failed assertions / total | Failed cases / total | Evidence |
|---|---:|---:|---|
| Original 69506 forward visibility |72/5264|4/4|`forward-visibility-original/run-02.{log,json}`; library `f9f6735c…`|
| Opaque forward blocker, intermediate checkpoint |32/674|1/1|`opaque-forward-blocker-original/run-01.{log,json}`; library `dd453fa2…`|
| Opaque/MASK alpha-policy counterexample, intermediate checkpoint |24/1098|1/1|`opaque-mask-original/run-01.{log,json}`; library `3087ca29…`|
| Zero-alpha contribution, intermediate checkpoint |24/894|1/1|`zero-alpha-original/run-01.{log,json}`; library `cf7b8e4d…`|
| Final expanded visibility suite |0/19777|0/10|`visibility-final-04.{log,json}`; final library `5d10cbca…`|

These checkpoints use explicitly different binaries/libraries and evolving test sets. They are not summed or presented as one matched assertion denominator. The final suite covers all retained counterexamples plus additional qualification guards. `correctness-final-02.{log,json}` passes 15/15 affected suites, including Temporal, Spatial, resource planning and snapshot contracts; image-quality gates are reported separately below.

### Transparency and resource cost

Exact agreement in stored RG16 motion is deliberately conservative: partial layers with different represented motion retain rejection; animated textures, unreliable poses and unqualified material/blend families remain conservative. This does not promise stable glass, hair or particles in arbitrary motion. Full-alpha/zero-alpha rules have explicit counterexamples rather than globally accepting transparent history.

The compatible Temporal input pack gains one memoryless D32 visibility attachment. Motion constants gain 80 bytes/draw (64 for current modelview,16 for water plane). Forward motion replay changes from two replays to one; no extra encoder,command buffer or normal-frame CPU wait is introduced. These are source/ownership changes, not a measured Home cost claim. In the 64×48→128×96 ownership fixture,12 settled worlds with display/presentation/opaque snapshot/visibility pack record51 pool acquisitions,45 reuses,0 deferrals,3 textures created,0 new textures after warm4 and150528 cached bytes. The fifteen-suite receipt and `correctness-final-02-ctest-detail.log` retain this scope; it is not a full-resolution memory budget or live FPS measurement.

## Exact original twenty quality bounds

Every row remains failed. Before values retain the original Catch2 precision; final values use 17-digit metric exports. RGB change/error values are linear RGB; recall/area are dimensionless. Original test/oracle definitions and source/log locations are frozen in the baseline document. Differences of at most 1.51e−8 in coverage summaries are not rendered improvements; actual saved output pixels are identical.

| # | Region / path | Metric |69506 before|69507 after|Required|
|---:|---|---|---:|---:|---:|
|1|`generic_mask_sky` / MASK|edge-band change|0.00431148953450549|0.0043114895345054928|<=1/255|
|2|`generic_mask_sky` / MASK|minimum recall|0.85402309287561229|0.85402309283127875|>=.90|
|3|`generic_mask_sky` / MASK|minimum area|0.72128308059733415|0.72128308085187198|>=.85|
|4|`generic_mask_sky` / MASK|stopped18 error|0.00663084148082352|0.0066308414808235183|<=1/255|
|5|`legacy_mask_opaque` / MASK|mean RGB change|0.00430316200028507|0.0043031620002850733|<=1/255|
|6|`legacy_mask_opaque` / MASK|edge-band change|0.00505461367666763|0.0050546136766676337|<=1/255|
|7|`legacy_mask_opaque` / MASK|minimum recall|0.89310152396239684|0.89310152396239684|>=.90|
|8|`pbr_mask_sky` / MASK|minimum recall|0.85761618631501346|0.85761618657265937|>=.90|
|9|`pbr_mask_sky` / MASK|minimum area|0.73585134880575009|0.73585135059851559|>=.85|
|10|`pbr_mask_sky` / MASK|stopped18 error|0.01847008152703998|0.01847008152703998|<=1/255|
|11|`pbr_mask_sky` / MASK|stopped36 error|0.00607027091314644|0.0060702709131464395|<=1/255|
|12|`blended_leaf_sky` / BLEND|minimum recall|0.89056031102389499|0.89056031948289083|>=.90|
|13|`blended_leaf_sky` / BLEND|minimum area|0.84450735858118853|0.84450737362167694|>=.85|
|14|`blended_leaf_sky` / BLEND|stopped18 error|0.01005504958646609|0.010055049586466089|<=1/255|
|15|`grass_sky` / MASK|minimum recall|0.85817597908358423|0.8581759802064588|>=.90|
|16|`grass_sky` / MASK|minimum area|0.73641728538047047|0.73641728636933557|>=.85|
|17|`grass_sky` / MASK|stopped18 error|0.01795483555317869|0.017954835553178686|<=1/255|
|18|`grass_sky` / MASK|stopped36 error|0.00583019928665798|0.0058301992866579798|<=1/255|
|19|`dense_mask_opaque` / MASK|edge-band change|0.00417986980182676|0.0041798698018267622|<=1/255|
|20|`straight_edge_control` / Control|stopped18 error|0.00462346703749525|0.0046234670374952465|<=1/255|

`quality-final-01.{log,json}`:6972 assertions,6952 passed,20 failed; exit 42. The count is twenty failed assertions, not twenty newly diagnosed defects. All original thresholds remain unchanged. Exact before/after strings, source assertions, metrics and artifact hashes are in `final-quality-review-support/final-matrix/original20-matrix.json`. Generic `grass_sky` is an opaque MASK fixture; live grass also includes BLEND paths. The original `pbr_mask_sky` uses an authored grass texture, so the added explicit PBR palm fixture is separate.

## Remaining MASK result and oracle limits

`mask-final-01.{log,json}` adds an explicit PBR MASK palm at 67% across normal/reversed depth and clear/finite backgrounds. All four arms retain stationary edge-change, recall, area and 18-world settling failures:16 failed bounds/23684 assertions. Stationary mean/edge/coverage values are the same across these arms: edge change 0.004314364957619261, recall 0.85446920949441629, minimum area 0.72172860622982393. This weakens an explanation confined to reversed Z or the clear background; it does not prove a vendor limit or close all depth-contract questions.
The explicit PBR contract records maximum analytical motion error 8.7022781372070312e−5 input pixels, zero color/proxy coverage and raster mismatches, zero rejected foreground and zero paired input mismatches. Retained history remains valid after first use with no repeated reset. These checks exclude the tested coverage/pose/motion mistakes at their stated scope; they do not equate small vector error with complete image quality.
An independent offline audit of the 69506 raw input/output cycle finds output foreground green contrast only 72.9% of actual input for generic MASK,74.4%for PBR/grass MASK and 84.9%for BLEND; opaque controls retain 99.4–99.6%. That calculation uses signed linear-color excess over measured background, without a leaf classifier, alpha unmixing or clamp. Source depth coverage agrees with matched-LOD supersampled reference within 0.08 percentage points. Thus the problem includes real contrast attenuation after the input. Recovered coverage remains an appearance proxy: it does not establish that geometry or main depth disappears. See private `../temporal-foliage-20261125/quality-oracle-audit-69506/{FINDING.md,compact-result.json}`.
The vendor color-input-alpha diagnostic forced alpha to 1 while preserving RGB and downstream alpha. All 144 output PPMs and 18 raw stationary outputs remained byte-identical; original twenty bounds were unchanged (`fixture-mask-alpha-12/comparison.json`). This was not an alpha-only depth-selection experiment. Forced vendor history reset worsened the original fixture from 20 to 42 failed bounds: generic edge change 0.00431148953450549→0.04049735172625727 and minimum area 0.72128308059733415→0.57205518096939101. It met seven original settling bounds by discarding accumulated detail while introducing other failures;13 of the original twenty still failed (`fixture-mask-reset-13/comparison-and20matrix.json`). Negated jitter was also rejected. These diagnostics were not shipped and do not establish an intrinsic MetalFX limit.

## Review media and decision

Media receipts: `fixture-review-preparation/final-eight-ROI-media/assembly-receipt.json`, `fixture-review-preparation/final-byte-identity-receipt.json` and `fixture-review-preparation/final-PBR-palm-media/assembly-receipt.json`. Stationary clip uses18 matched worlds36–53; movement clip uses18 moving+36 stopped worlds90–143. Corresponding lossless RGB FFV1 files and1:1 PNGs/crops are primary; MP4 is a compatibility preview. `lossless-verification.json` verifies decoded panel pixels against sources. All source frame IDs/hashes remain reviewable.

The private before/after fixture stationary and move-stop exports preserve actual texture output and exact frame/jitter identities. Because the existing fixture outputs are byte-identical, those clips honestly show unchanged residual quality, not a new stability improvement. PNGs are the authoritative pixel crops; compressed MP4 previews and nominal playback rate are not viewer FPS. The explicit PBR arm has18 stationary outputs and four sampled move/stop states; sparse samples are not fabricated into a continuous motion recording. Native-reference crops show sharper but binary/jagged thin tips, while warm Temporal output is smoother and lower in fine-detail contrast. Native0 is a detail reference, not an exact anti-aliased output oracle; recovered area is not a count of missing geometric leaves.
Keep 69507 **experimental**. Retain the verified visibility corrections, but do not select Temporal as the default or call PPD-610 fixed. Native remains the direct detail reference and Spatial remains an available reconstruction alternative for owner review; no new comparative quality/FPS claim is made for either. The next acceptable Temporal change must improve the explicit MASK contrast/edge/settling gates while preserving accurate motion/occlusion and the passing visibility cases. Keep the current quality thresholds and do not mask the defect with blanket rejection, added blur, a jitter freeze or relaxed coverage scoring. Remaining real-scene foliage/motion acceptance belongs to owner review after this bounded handoff.


## Completed private delivery

At2026-11-26 08:39:33UTC, Foundation reports all three iCloud Desktop files uploaded=true,
uploading=false and no uploaderror: `Alchemy-MetalFX-Temporal-Experimental-69507-AppleSilicon.dmg`,
`MetalFX-Temporal-Experimental-69507-Review.zip`, `MetalFX-Temporal-Experimental-69507-SHA256SUMS.txt`.
DMG377560963bytes/SHA256`d92686ca1d767499e8f1bdbb57092044ad9851be1344538bec0516dcc4743c64`;
ZIP84363956bytes/SHA256`f72eac530b2a25b962de776fc26f1883c544bd7ad5663b61adf5dc2a841e6e18`.
Read-only DMG matches all8272appentries; ZIP verifies all1057manifestedfiles. Signing is ad-hoc,
not notarized; AppleSilicon/macOS26.0+. Actual runtime decoder identifies OpenHTJ2K0.27.0.
Private `delivery-69507/final-handoff-receipt.json` retains source/executable/library/upload details.
Application source remainsb73abe1a; the source publicationf3d9fdd1 and this later delivery-note commit
are distinct documentation checkpoints. The ZIP contains the pinned application source and final evidence;
this completion note does not change its executable, shaders or quality disposition. Prior69506 is preserved.
Select AdvancedGraphics→Worldresolution→67%MetalFXTemporal and inspect effective status. Use macOS
screenshots/recording for live review; independent viewer snapshots do not reliably represent Temporal output.
