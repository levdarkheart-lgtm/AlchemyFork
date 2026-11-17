# Remaining Home GPU cost and spill map — 2026-11-17

The remaining Home decline is completion limited in the healthy runs. The main thread spends more time waiting for already-running GPU work as scene inputs populate. Current saved-frame evidence puts growing base/probe and transparent lighting ahead of another point-light shortcut. It does not establish one exclusive cause or identify another confirmed remaining spill in build69231.

Best staged69231/source7060967e3069f85d808d7b7ac6cca2bfef836dd2 is unchanged. The [point/projector proposals](lighting-rejection-qualification-20261117.md) remain unselected after accepted-light regressions and a strict specialization precheck failure. Original architecture acceptance stays partial; PPD319/343/344 remain open. No viewer or new in-world recording was used for this map.

## Whole-frame costs and compiler fields

A fresh Xcode profile of the retained late69228 working copy finishes at121.91ms. Its complete Shaders table has138rows:70fragment,62vertex and6compute. **62report0spill bytes,75are blank/unknown, and1reports nonzero.** Row ordinals, stages, pipeline labels, allocated/high GPR and raw spill fields are retained without merging same-name variants. A blank cannot inherit zero from another descriptor, even with the same label/GPR. Current metadata does not establish executed spill traffic.

| Late shader path | Sampled replay share | Allocated GPR | Spilled Bytes |
| --- | ---: | ---: | --- |
| Simple transparent, High2/two-resource selector |21.71%|214|blank/unknown|
| Main base geometry |21.27%|214|blank/unknown|
| Main projector, High2 |13.17%|188|blank/unknown|
| SSAO bilateral blur |8.11%|83|0reported|
| Main PBR transparent, High2 |5.39%|254|blank/unknown|
| Main point light |4.76%|86|blank/unknown|
| Shadow/SSAO light map, High2 |3.63%|188|0reported|
| **Old serial** exposure history |3.55%|50|560reported|
| Legacy-material transparent, High2 |1.50%|242|blank/unknown|
| Actual atmospheric haze |0.57%|142|blank/unknown|
| Probe single-tap PBR transparent |0.33%|228|0reported|

These are one profile's sampled shares, not isolated shader durations or live FPS. Earlier early/late counter profiles remain separate; their shares must not be spliced to this new profile. The old exposure row belongs to librarya40/build69228. Build69231's parallel resolver already has separate exact-output and useful component-cost evidence; its new GPR/spill allocation is still unmeasured. Historical PBR32B/material0 and controlled selector304→0 readings do not replace current blank cells.

Xcode's Call Stack view adds a useful localization. Main base displays25.94ms and attributes an inclusive69.89%of that shader's samples to almetal_sample_base_probes; simple transparent displays26.46ms and attributes54.46%to the same helper. Nested tap/SH/weight/traversal subtrees are retained privately. They overlap and **must not be added**. Inlining/source mapping includes surprising surface/oct-decode children, so exact leaf subtraction or a literal call graph is not qualified. The evidence supports investigating shared probe evaluation, not declaring SH arithmetic alone responsible.

## What grows while FPS falls

Healthy normal-mode A2/B2, with all selected controls equal and dynamic exposure enabled, both decline during loading. Early0–10s main completion waits are13.63/12.60ms per eligible period; late80–120s waits are46.38/43.56ms. Late main running CPU is~29%of one core and decode CPU is0. The wait targets are already-running older GPU work, with only~0.53/0.51ms between GPU end and wait return. This supports GPU completion pressure; it is not a measurement of exclusive hardware busy time or clocks.

In the separate69228 early/late frame pair, the fused main base/sky/point/projector encoder grows25.053→48.574ms and the alpha-containing haze-labelled encoder11.567→30.058ms. Main configured probes grow1→28. Alpha vertices grow5.8× while fragment invocations fall and fragment ALU grows; material/coverage and per-pixel cost change together. Fixed main local-light areas do not fix material/cookie/BRDF cost. Probe views also change, and are not matched faces. The actual haze shader is small; the encoder label includes subsequent alpha draws.

SSAO blur remains~9.89ms in both serialized counter profiles despite a reported zero spill field. It is a substantial fixed cost, not the dominant measured growth. The older exposure resolver is also a floor already addressed separately. Changing the CPU threading model cannot be credited with removing these GPU costs; bounded preparation still requires published-input and dependency proofs.

## Actual receiver diagnostic

The next discriminating question is **candidate tests and accepted probe/SH taps per real receiver**, rather than configured probe count. Source retains the original32accepted/64neighbor bounds, depth buckets, manual precedence and default fallback. A zero weight or fade does not itself skip SH evaluation; only a nonpositive SH scale bypasses that evaluator's fetch/decode. No reduced lighting quality is proposed.

From actual main base draws36081early and38083late, Xcode exports the bound49,216byte probe packs and336byte base constants using Editor→Export→Raw. Depth-attachment Actions→Export→KTX yields3456×1916 DEPTH_COMPONENT32F with S=r,T=d orientation. These files support reconstruction of real receiver positions and a CPU replay of the source traversal. They do not contain alpha varyings or all material/radiance inputs and cannot measure shader execution time.

The initial combined depth-stencil resource export yields STENCIL_INDEX8 instead. It is retained as late-main-depth.ktx and explicitly rejected as depth. The corrected depth-aspect files pass header/size checks. No values are reconstructed from screenshots or PNG normalization. Exported files, hashes, exact draw/binding identities, analyzer and limitations are private under artifacts/metal/home-whole-shader-map-20261117.

The first executed64×40sample grid gives the following source-semantic demand. Geometry-qualified receivers number2430early and2424late. Actual reverseZ1/classic0/legacy-environment0 confirms that this main base path enters modern probe evaluation. All28active late probes, including default0, are automatic spheres with positive SH scales and fades.

| Receiver positions | Probe pack | Mean local accepted taps | Mean positive-scale SH entries, including default | Mean containment tests |
| --- | --- | ---: | ---: | ---: |
| Early, actual |Early1|0|1|0|
| Late, actual |Late28|6.3094|7.3094|22.9703|
| Early, counterfactual |Late28|6.3123|7.3123|22.8687|
| Late, counterfactual |Early1|0|1|0|

The pack swap holds receiver positions and their geometry qualification fixed within each control. It demonstrates that the increase in source-level probe demand persists independently of those receiver geometry changes. It does not replay different lighting or measure a GPU/FPS speedup. The late actual grid spans1–13positive-scale SH entries, with8the most frequent; no manual or zero-weight taps occur. This supersedes configured-count-only reasoning for these sampled opaque receivers, but does not measure actual simple-alpha tap incidence.

The private analyzer passes13deterministic traversal/decoder/vector cases and also decodes an older real v2 GPU-published packet, kept explicitly separate. CPU sequential float32 math is not a bit-exact Metal fast-math/FMA oracle. An8EPS boundary sensitivity flag finds no near-boundary cases on these grids; it is not a rigorous error bound. An independent float64 reconstruction/walk crosscheck reproduces every ordered tap and containment count on both grids; maximum eye-position difference is1.183e-5. That review is limited to the scalar grid snapshot.

A bounded vectorized automatic-sphere path first matches scalar reconstruction, counters, terminations and boundary flags on both grids, then processes each full6,621,696pixel depth image in65,536pixel chunks. Full early analysis takes1.95s CPU wall and late12.04s; these are analysis costs, not viewer/GPU timings.

| Actual main receiver census | Early | Late |
| --- | ---: | ---: |
| Depth-selected receivers |6,259,843|6,242,661|
| Local accepted source taps |0|39,489,146|
| Positive-scale SH source entries, including default |6,259,843|45,731,807|
| Mean SH entries per receiver |1|7.32569|
| Containment tests |0|144,101,229|
| Near-boundary flagged receivers |199|286|

The late full histogram still spans1–13SH entries, with8the mode. The286flags include199depth-bucket integer and87sphere-boundary cases;0.0046%of late receivers are flagged. Five late positive-scale SH entries have zero blend weight, so skipping zero weights is not a useful broad explanation here. Counts remain source-semantic demand with CPU/FMA uncertainty, not GPU counters or an exact45.7million executed-instruction claim.

## Producer experiment and subsequent decision

A producer-GPU decode experiment tests the existing half-quantized SH publication as exact float coefficients shared by consumers. It preserves projection's half rounding, probes, parallax, coefficient order, weighting and blending. No CPU readback/wait is introduced. The private float prototype packs27decoded coefficients into seven texels:112bytes versus64packed bytes per row; retaining both adds112payload bytes per row. Extra fetch/cache and producer cost can outweigh saved repeated unpack/conversion, and real allocation overhead remains unmeasured.

Actual late draw38083texture7,4×130RGBA32Uint, is exported as raw KTX1 RGBA32UI/UNSIGNED_INT with8,320payload bytes and a verified hash. Subsequent [physical qualification and cost](home-producer-sh-qualification-20261117.md) rejects both112byte float and56byte native-half representations. Each precision-preserving control passes exhaustive coefficient and1,103strict consumer comparisons, but neither demonstrates useful consumer savings. Decode+consumer costs regress9.46–12.80%for float and4.08–7.72%for half in the six bounded workloads. The first float variant's one-half-component failure remains PPD353; tolerances were never relaxed. No publication integration or viewer source changed.

The subsequent private experiment specializes the actual all-automatic-sphere metadata invariant with a Metal function constant. It keeps the original packed representation and every probe/tap, while making manual/box paths unreachable only for eligible per-view packs. Generic/unset/false fallback and water automatic=false semantics require strict tests before cost or selection. Native spill changes remain unknown; compile/source simplification is not performance evidence.

## Evidence and continuation

Private growth-audit/report.md retains the phase/source map and11input manifest; spill-evidence/report.md and inventory.json retain the full138-row census and older transcriptions separately. root-xcode-inspection contains UI census, inclusive call-stack observations and raw exports. probe-walk-census owns the receiver diagnostic; next-target-review contains the independent scope challenge. The source840→706 MSL difference affects only exposure. Subsequent candidate-preparation source adds a probe-walk mask path and new entry points; this historical profile/compiler census is not a measurement of that new source. Captures/profiles remain diagnostic populations, separate from healthy live cadence and owner visual acceptance.

Root verification: python3 tools/metal/validate_harness.py passes33required files; python3 -m unittest -v test_census passes13cases; git diff --check passes. No production shader changed, so no new viewer build is asserted. All replay sessions end with Stop GPU workload and observed Replay/Finished running. No production source or app is edited and no private evidence is uploaded. Continue from actual receiver incidence before choosing another probe implementation; preserve all earlier rejected ambient/scalar/PCF and source-inline trials rather than repeating them without a new question.

The owner then directs implementation of the missing GPU-side architecture, with no measurements now. [Current group](../work/active/probe-candidate-preparation-20261117.md) starts conservative per-view candidate preparation and integrated main/probe BASE consumers. No depth-only bounds may cull transparent receivers, and no valid lighting contribution may be removed. Runtime evaluation follows a complete verified/staged change group; architecture/performance acceptance remains partial.
