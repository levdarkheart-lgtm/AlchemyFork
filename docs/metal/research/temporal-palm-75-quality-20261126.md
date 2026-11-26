# Temporal 75% PBR palm quality comparison

**75% does not qualify as a correction for the remaining palm defect.** It improves recovered recall and detail, but still fails three unchanged target bounds: stationary edge-band change, minimum recovered area and 18-world settling. The comparison stops here; no extra quality tweak, variant or replacement viewer/DMG is justified. PPD-610 remains open.

## One actual 75% input comparison

| Configuration | Retained 67% | Fresh 75% |
|---|---:|---:|
| Actual input→output |256×128→384×192|288×144→384×192|
| Saved mode / production mip bias |Temporal 67% /−1.5849624872207642|Temporal 75%,mode 7 /−1.4150375127792358|
| Production jitter phases |18|15|
| Warm-up worlds |36 (two cycles)|45 (three cycles)|
| Measured stationary frame indices (count) |36–89:54 (three cycles)|45–89:45 (three cycles)|
| Moving / stopped frame indices |90–107 /108–143|90–107 /108–143|
| Independent final-camera reference |54 worlds,36 warm|60 worlds,45 warm|

Both modes rasterize the same authored production PBR MASK palm at the same 384×192 output extent, material/coverage rules and camera path.75% uses the centralized existing resolution/mip policy and actual15-phase history; it is not an upscaled67% capture or a forced18-phase replay. Input-buffer sizes and 144 metadata rows confirm the new dimensions. Standalone 67 replay was not used for 75.
The actual camera-shift metadata agrees for all 54 moving/stopped worlds. Each mode is scored against its own independently warmed final-camera reference at the actual jitter phase. At frame index 125 (world ID 126),67% uses phase 17/reference index 53 while75% uses phase 5/reference index 50; frame index 143 (world ID 144) uses phase 17/reference index 53 versus phase 8/reference index 53. All frame/reference indices are zero-based; submitted world IDs are one-based. No phase IDs are equated merely because frame numbers match.
This compares production modes with different input size,bias,jitter count and warm-up age; it does not isolate pixel count alone.75% receives nine more warm-up worlds. The original acceptance thresholds remain fixed despite those differences.

## Unchanged bounds and observed result

Values below are the recorded 17-digit C++ scores. Four targeted67% failures become three75% failures; this is not twenty defects or a passed suite.

| Metric |67%|75%|Existing bound /75% status|
|---|---:|---:|---|
|Mean adjacent RGB change|0.0036729858417721907|0.003668965601935041|<=1/255,passed|
|Mean edge-band change|0.004314364957619261|0.0043095348865212265|<=1/255,**failed**|
|Edge-band frame p95|0.0045685057890792174|0.0046924812333725193|<=2/255,passed|
|Minimum recovered recall|0.85446920949441629|0.9043314498700209|>=.90,passed|
|Minimum recovered area|0.72172860622982393|0.75008171745598695|>=.85,**failed**|
|Mean output edge energy|0.20455696902988851|0.22267468779468313|≥50%native-reference energy,passed|
|After 18 stopped-world phase error|0.0059885653451804913|0.0048588025873222843|<=1/255,**failed**|
|After 36 stopped-world phase error|0.002298313084378386|0.0016502992057410375|<=1/255,passed|
|After 18 old-history fraction|0.00043351592909049132|0.0010124810294345234|<=.02,passed|
|After 36 old-history fraction|0.00017885918770396472|0.00030904561061914946|<=.02,passed|

`quality75-01.log`:9584 assertions,9581 passed,3 failed; exit42. All remaining coverage/raster/rejection checks pass with zero mismatches/rejected foreground; maximum analytical motion error is 9.7900629043579102e−5 inputpixels. Mean reference error 0.045743625618763314,coverage change 0.00012282690854119424,minimum precision 0.99989551401883603 and maximum area 0.76278618000764287 pass their existing guards. Passing old-history bounds is not a general no-ghosting guarantee.
Mean edge variation barely changes; frame p95is slightly higher. Output edge energy increases 8.86%, and area-weighted signed-green contrast rises 73.7611%→75.9588% of actual input. These support retained-detail improvement rather than a broad blur substitution, but do not establish stable thin detail or Native-quality parity. Descriptive contrast integrates fractional input-pixel boundaries at 75%; it is not a new acceptance threshold.

## Oracle and comparison limits

Native-detail and shifted-Native RGBA32F references are byte-identical over the entire output. The scored 84×84 PBR coverage ROI and full 96×96 PBR cell are also byte-identical. The full coverage map has 1173/73728 different float components outside the PBR cell, all in the BLENDreference cell, maximum 5.960464477539063e−8. That rounding-sized difference is preserved and disclosed; it was not replaced, ignored as global identity or investigated as another material task.
Only the target PBR MASK case is evaluated here. The higher recall/gradient is not proof of recovered physical detail, and recovered area is not a missing-geometric-leaf count. Native0 remains a detail reference, not an exact anti-aliased output oracle. Different phase counts, warm-up ages and test executables remain explicit; no frame sample is treated as an independent experimental repetition.

## Cost, media and decision

The actual input grows 32768→41472 pixels, **+26.5625%**, with output size unchanged. That arithmetic is not a measured GPU-cost or FPS result. No viewer login, Home/Club performance run or live-quality acceptance was collected.
Media uses existing reconstructed output textures at the same output size. Common-offset still pairs verify both recorded jitter components:67% frame indices 72/79/86 versus75% frame indices 75/82/89 (common phases0/7/14). Stationary clips retain each mode's complete measured cycles,54versus45 worlds; neither is coerced into the other's phase schedule. Motion clips preserve 54 submitted worlds at frame indices 90–143, with the matched camera path and distinct jitter. Lossless files/PNGs are authoritative; nominal playback and compressed previews are not viewer FPS.
Common-offset crops show slightly stronger/cleaner frond contrast at 75%, while both modes remain softer than the binary Native detail reference. Move-and-stop output still converges over time; the clips do not establish completely stable or trail-free foliage.
Keep Native as default and the existing Spatial alternatives available.75% Temporal can retain more detail but is **not recommended as the demonstrated stationary-foliage remedy**, because every target bound must pass and three do not. A bounded next selection gate could compare Aurora's reconstruction algorithm on this one palm with valid algorithm-specific inputs,the same output and unchanged quality bounds before considering full integration. Do not assume it will improve quality or cost; no such comparison or integration occurred here.
A final test-source edit only generalized the inherited command label. `final-source-validation.json` records a nondumping validation with an exact match to the collected metric line; it is not another comparison arm. Three targeted sizing/history/presentation-glow regressions passed. The original collected source/executable identities below remain the evidence provenance.

## Provenance and private evidence

- Baseline:`../temporal-palm-native-isolation-20261126/direct-final-02/native-isolation/`; source/scoring scope is retained in [native isolation](temporal-palm-native-isolation-20261126.md).
- 75% evidence:`artifacts/metal/temporal-palm-75-20261126/quality75-01.{log,json}` and`quality75-01/temporal75/`. Starting HEAD`9e093806d790267ec63dd7ec0dee25ca3bbf19b7` plus recorded fixture edits; source SHA-256`f430d02d769370b86736a37dd4076c62d89b59952be36df3bb42b48052db5348`.
- Test executable:`7c5b19b8e1d58f2a63b2ae5920161c8a81c4532d2d15b6ee608d7a4be6cec8fe`; production shader unchanged:`5d10cbcaefd65d98e50fb2894870a3364e30527b28b7dfd6addc7c7436c09e8a`. Metal API/GPU validation enabled. This is not a new application build identity.
- `offline-comparison/{comparison.json,authoritative-metrics.json,reference-audit.json,compare.py}` retains exact frame/reference joins, source hashes, weighted contrast, guard results and all exclusions. The completed `media-review/export-receipt.json` preserves 211 hashed sources and 38 image/video outputs, exact source frame/world/jitter identities and the pixel-transfer rules. Separate stationary67/75 clips retain54/45 frames; the paired motion clip retains54 frames. Lossless first decoded frames match their source panels. See `media-review/README.md`; no additional measurement was collected.


Private owner delivery is complete: `Temporal-Palm-67-vs-75-Quality-20261126.zip` (30263429bytes,
SHA256`3f7540c5d3e1e56215dfd3116c80f1725f4c56ef6d7307a9b0b1795090dd3c73`), separate private Apple
feedbackDRAFT and checksums are on iCloud Desktop. All68bundle manifest entries verify; Foundation reports
uploaded=true/notuploading/noerror at2026-11-26 11:48:54UTC. The draft is excluded from Git and has not been
submitted to Apple. This is evidence/media delivery, not a viewer build or DMG. Exact receipt:`final-handoff.json`.
