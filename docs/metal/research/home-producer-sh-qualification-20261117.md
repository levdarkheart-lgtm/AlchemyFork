# Producer SH representation qualification — 2026-11-17

Both seven-wide coefficient representations are **rejected for performance**. Their precision-preserving forms pass strict output checks; neither demonstrates useful consumer savings, and both add producer cost. Best combined viewer69231/source7060967 remains unchanged. No new viewer, in-world recording, production publication path or push was used.

The [whole-frame map](home-whole-shader-map-20261117.md) established growing source demand on actual Home receivers:1→7.326SH entries per receiver, with45.73million late entries. Captured normal data subsequently classifies5,309,162of6,242,661late depth-selected receivers as PBR. Exact CPU classification also records933,499legacy receivers,94,134with positive legacy environment intensity. This is a source-input distribution, not a GPU branch counter or an alpha-material census.

## Frozen source and preservation contract

Baseline MSL SHA256`b8dc1b0e473906cb785bb2613f0d37cdb0f839b4dc44486d983f3c3bf42cb6a7` matches the current engine. Baseline private library`d71835ecac1d43aede08111300805f254dfbd18b0b3094399d23eec8796f9c11` is separately compiled with the exact production options: Metal3.1, fast math, macOS26.0, line tables and recorded sources. Its source is identical to the viewer source; its library hash differs from the packaged viewer and is not claimed to be that artifact.

The original1eb44 packing/SH contract and actual native ownership are audited privately. Derive only from the final native clamped, half-packed row; preserve partial/final rounding, coefficient order, parallax, scale/weights, default/manual precedence, traversal and all probes. Current ordinary publication accepts matching queued SH and copies radiance plus packed row in one ordered command. It must not acquire a CPU-ready/readback barrier. Packed default-preview and old-library behavior remain authoritative. Current validation accepts RGBA32Uint width>=4; that alone cannot distinguish layouts. No publication/lifetime/representation integration exists in these prototypes.

## B: exact decoded bits, differing full-shader result

Private B source`4d388ab840cb1352a8bd34cc3c40529813c8d4c6b3289860722daa800eab5d49` expands four packed uint4 texels into seven uint4 texels containing27float32 coefficient bit patterns and one zero. It changes nominal sampled bytes64→112per row, retains the authoritative packed texture and adds112bytes of retained derived payload per row. Actual allocation and cache traffic are not inferred from that arithmetic.

GPU coefficient qualification on M1 Max passes28cases and12host preflight controls. Independent integer IEEE expansion, actual compute output and a fragment conversion bit-reference agree for all63,488finite half encodings in each27channels (1,714,176components), mixed/tail cases and3,510actual captured coefficients. Signed zero, subnormals, padding and rounded1Ddispatch tails are covered. Actual compute/reference shader validation reports Enabled1, with API/GPU-validation logs. This checks representation and conversion stages, not the entire production optimizer context.

The frozen consumer fixture first passes36shadow-alpha cases and one actual-row helper case, then fails the actual-row0base case: pixel50,0blue changes0x29d1→0x29d2, +1half ULP (0.000030517578125). The other10,239components match. Saved decoded texture bytes match CPU expansion and both earlier GPU bit outputs exactly. Catch abort exits42 after38comparisons;1,065planned comparisons remain unrun in B. PPD353 retains the failure. No B cost was measured and no tolerance was relaxed.

## C: conversion-provenance control

One control retains the seven reads, reconstructs14explicit half2→float2 pairs, and uses the exact original l0…l8assembly and SH expression. These finite values already came from half, so the conversion is a mathematical identity. Source`21f73f19d33f787f4e0cf5279604e0f98ec3a6aa1b0f5690152bb43c1fd5cac2`, library`38ab090408800107d1802a2db1acc4f036ed993e938a49a9e7a1c6e8dfd1d834` compile/link0.

C passes the entire1,103comparison matrix:670helper,173base,172simple-alpha and88shadow-alpha comparisons,24,821assertions/four test cases, API/GPU-validation logs. All RGBA16F bits, finite/written-output checks and eligible depth/stencil checks pass. Its actual kernel separately passes the28case coefficient gate. This supports sensitivity to full-shader precision provenance/code generation; it does not prove a particular FMA difference or a Metal compiler defect.

PPD352 records a separate fixture premise caught before running:75captured rows have all-zero coefficients and legitimately produce black at SHscale1. The corrected fixture requires exact black plus written-alpha/finite/nonclear checks for those controls, while retaining positive-output guards elsewhere. All75execute in C. All28active captured main probe layers have nonzero coefficients. Classic-mode sampling bypass returns the supplied ambient rather than an all-zero result; that review correction is retained too.

## C cost: every measured workload regresses

Six1024×576production-shader workloads pass all12strict large prechecks with actual shader validation Disabled2 on all11fixture graphics/compute PSOs and empty stderr. Independent source replay of saved constants checks644receivers per cell plus continuous geometric bounds. Actual SH entries per receiver are1,8,8,8,5,32; these are not guessed from configured probe counts. The manual cell retains accepted indices1,2,3,5,7and suppresses the default; the cap cell accepts32with no default.

Each workload/mode retains150alternating ABBA/BAAB measured blocks,300samples per arm, plus10warmup blocks. Reuse derives once outside timing; combined mode includes the full130-row producer in the consumer command. All7,990observed command intervals are completed, positive and nonoverlapping within the recorded sequence:7,200measured consumer observations,480consumer warmups and300+10producer-only observations. No hardware-exclusive busy-time or fixed-clock claim follows.

| Workload / SH entries | Reuse paired median | Decode + consumer paired median | Adverse reuse /150 | Adverse combined /150 |
| --- | ---: | ---: | ---: | ---: |
| Default PBR /1 |+3.79%|+12.80%|108|134|
| PBR base /8 |+6.97%|+10.35%|126|137|
| Legacy base /8 |+6.14%|+9.80%|120|138|
| Simple High2/pair /8 |+6.44%|+10.74%|113|132|
| Manual PBR /5 |+6.64%|+9.46%|131|138|
| Capped simple High2/pair /32 |+6.68%|+9.57%|126|140|

Both order subgroups are adverse in all24subgroups. Producer-only median GPU envelope is4.999microseconds; owner encode/commit/wait wall is213.167microseconds, a different metric. Separate modes have different baseline distributions; subtracting their medians does not isolate producer cost. AC power/charged and no recorded thermal/performance warning are retained; actual GPU MHz is unmeasured.

Larger payload, additional reads/conversions and changed generated code can contribute, but their individual costs are not isolated. The result rejects **this representation**; it does not reject producer-owned derived inputs generally. No whole-viewer FPS prediction follows from these fixtures.

## H: native-half representation also has no useful net gain

The private H source stores the original27half coefficient bit patterns in seven RGBA16Float texels,56bytes per row. It keeps the original half2→float2consumer assembly and removes C's float→half round trip. Source`9cc3ff27a6d3850216523f1943b7260b0a19aa3fc6c990aa6a3a9665b2286ac0`, library`98602e55814011c09f8056c9c94102c76f50e7009fd2c53edcbc6c541098a2d6` compile/link0. Only the new raw half-shuffle kernel,11SH consumer parameter types and load representation change; exact reversal recreates the baseline. Packed project/reduce/preview stay unchanged.

H passes28coefficient cases/12host controls, covering the same1,714,176finite components and3,510actual coefficients. Direct raw half stores/reads preserve all27original coefficient bit patterns and positive-zero padding; both the H typed-texture fragment conversion and original packed fragment reference agree with independent CPU expansion. All three physical PSOs report shader validation Enabled1. The complete strict consumer matrix passes1,103comparisons/24,825assertions/four test cases, including all75black controls. This covers shared PBR evaluation and production base/simple/shadow paths; it does not directly draw every forward PBR/material caller.

The six workloads pass12strict1024×576prechecks with actual validation Disabled2 on all11cost PSOs. Fresh saved inputs and independent source census again establish1/8/8/8/5/32SH entries. All7,990completed GPU intervals are positive and nonoverlapping within the recorded sequence. The same150ABBA/BAAB blocks,300samples per arm, declared warmups and two modes are retained without outlier removal.

| Workload / SH entries | Reuse paired median | Decode + consumer paired median | Adverse reuse /150 | Adverse combined /150 |
| --- | ---: | ---: | ---: | ---: |
| Default PBR /1 |−0.11%|+7.20%|73|116|
| PBR base /8 |+1.41%|+4.37%|91|111|
| Legacy base /8 |+1.60%|+4.42%|96|120|
| Simple High2/pair /8 |+2.03%|+5.85%|96|111|
| Manual PBR /5 |+0.88%|+4.08%|102|120|
| Capped simple High2/pair /32 |+1.93%|+7.72%|92|133|

Default reuse is neutral; the other reuse deltas are modest and do not establish uniformly strong regressions against clock/noise variation. They demonstrate no useful saving. Producer-only median is4.875microseconds GPU envelope and212.563microseconds owner encode/commit/wait wall. Both order subgroups are adverse for every combined cell and nondefault reuse cell. Saved56byte rows match all original half bits. Smaller nominal storage does not prove reduced hardware texture traffic; register/spill/cache/clock effects remain unmeasured.

H remains unselected and unintegrated. Stop coefficient-layout and direct-blit variants: removing producer overhead does not supply the missing demonstrated consumer gain. This rejects the tested representation, while preserving the broader producer-owned publication architecture.

## Next evidence-based specialization

All28active probes in the captured main pack, including default0, have priorityw==0: automatic spheres. A new private source retains the original four-wide packed SH, all taps, traversal/caps, half arithmetic and sphere weights, but uses optional Boolean function constant5 to remove unreachable manual/box/family paths only for an eligible bound pack. Mixed/invalid packs must retain generic behavior; eligibility must follow each actual view's immutable pack. Water still rejects nondefault automatic probes when its helper passes automatic=false. This is a data-invariant specialization, separate from earlier failed accumulator/ambient rewrites.

Source`a123856bfeff36edc287deec982ce23f91b7cee94e0e94e3d1df46e756c52342` and library`f27d1b5f907dbc03e21994dfe40ae489350beebefc7d7f098c2d06ad3e4dcb35` compile/link0 with production options. The sole warning is the existing unused SKY_MAX_METEORS constant. The all-caller strict run then exits42 after17passing comparisons and one failed water-helper comparison (2,797planned comparisons unrun). All40executed pipeline metadata rows pass; eligible true mode is established. Water gloss differs132RGB half components across129pixels, every one+1half ULP; maximumabsolute0.000244140625. Other MRTs/alpha are exact. All70automatic local probes are rejected by water, leaving default0only; the cap label does not mean32water taps. Inputs/radiance identity match, while optimizer cause remains unknown (PPD355). A separately scoped direct-production fixture compiles/links0 but has not run. No cost or host selection/publication implementation exists; no spill/FPS benefit is claimed.

Private evidence is under artifacts/metal/home-producer-sh-decode-20261117: frozen B/C/H sources, coefficient-harness, parity-harness/failure-analysis, diagnostic-half-provenance, ownership-audit, cost-harness/analysis and native-half/{coefficient-harness,parity-harness,cost-harness/analysis}. Automatic-sphere source/contract/offline-compile receipts are under home-automatic-probe-specialization-20261117. Exact argv, exit codes, raw planes, samples, hashes and failed attempts remain local. Best69231 and all earlier private evidence are preserved; PPD319/343/353 and broader architecture/product acceptance remain open.

Owner subsequently redirects work to implementing missing GPU-side architecture before another runtime measurement. [Current implementation group](../work/active/probe-candidate-preparation-20261117.md) prepares conservative published per-view candidates, shared native consumers and completion ownership. All coefficient/AutoSphere experiments above remain private/unselected; no further performance measurement is running.
