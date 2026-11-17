# Shared probe preparation and transparency — group 1

Owner activated the four-step hotspot-led architecture plan on2026-11-17. This first coherent group corrects derived-mask cache dependencies and extends shared preparation into the expensive simple transparent lighting consumer. Per-pixel shader cost, completion-aware fresh optional-probe admission and additional tile-local work remain subsequent groups. Keep all required lighting contributions and full original functionality; no whole-viewer gain is inferred from source changes.

## Reference and ownership

Pinned original1eb44 class2/deferred/alphaF.glsl and class3/deferred/reflectionProbeF.glsl own forward eye-position lighting, ordered bucket/neighbor traversal, manual/default precedence,32accepted cap, SH/radiance math and blending. The native mask mechanism has no original equivalent: it can omit only a candidate whose original containment test must fail. Current simple alpha uses separately computed MV eye_position and MVP raster position; no identity between those two rounded paths is assumed.

Root exclusively owns almetalworld.cpp/.h, MSL, pipeline.cpp, CMake and canonical docs. Architecture peer owns publication-test file only. Evidence peer owns the new transparency-test file only. GPU peer reviews the independent projective mapping proof without shared edits. Sources/builders and all prior apps/captures are preserved; builder remains clean647dc369d1 until reviewed integration.

## Dependency and correctness contract

- PPD358: GPU masks are functions only of exact4112B bounds/config packet and derived grid under one kernel library/epoch. Current full lighting pack remains separately published and validated for every consumer. On a miss retain a full borrowed-input snapshot around reentrant admission, with all native-prefix failure rules intact. Reuse only same producing command or successful completed producer; keep32records/32MiB accounted storage and completion-owned work credit.
- Transparent consumer requires its own owned forward-view inputs. Candidate lookup must follow the exact interpolated eye_position actually used by containment, independently of MVP/MV/pose/raster alignment. A virtual projection lookup is under proof; unsupported/nonfinite/overflow/near-W/outside cases must use the original generic walk. BASE inverse/depth bounds are not an alpha certificate.
- Preserve current generic entry points, High2 and pair-selector repairs, texture/alpha/water/clip/discard/derivative behavior, all blend/depth/coverage variants, traversal/taps/math and optional old-library behavior. No draw-time PSO creation, new command queue/commit/wait/readback or retained live scene pointers.

## Acceptance and next checkpoint

Correctness includes actual compiled tile lookup with zero containment omissions, authored full-renderer exact color/depth/stencil comparisons with positive/color-changing probes, rigid/skinned/mismatched transforms, main/probe contexts, manual/neighbor/default/cap and unsupported cases. Publication tests require genuine distinct derived keys, completed lighting-change reuse without admission/allocation, reload/abandonment/reentrancy and resident accounting. Run exact native Release compiler flags and appropriate existing renderer tests. Preserve any failures without tolerance relaxation.

Run doctor/build/verify_app and static source checks, stage one separately identified combined viewer and bind source/library/executable evidence. Then pause for the owner Home login/loading/settlement followed by club teleport/loading/settlement checkpoint. No new in-world recording before this group is ready; no push. Mask reuse and rejection do not remove valid accepted SH/radiance evaluations, so their whole-path value remains to be established.


## Source integration and independent lookup gate

PPD358 persistent key is now exact packet+kernel library/epoch; full current validation precedes hits and full borrowed pack/P/I snapshots guard reentrant miss admission. Actual lighting is freshly copied/published independently. New forward scene owns a projection value and snapshots the full current probe pack once. Virtual lookup projects the exact interpolated eye position with bounded finite inputs; no MVP/MV equality or opaque depth is assumed. Unsupported/all-full/admission-denied cases stay generic. Standard simple-alpha rigid/skinned, shadow/no-shadow, High2 and exact pair-selector variants are prebuilt; custom/avatar/depth/PBR/material owners retain their original paths. No queue/commit/wait/readback was added. Last successful actual prepared draw counts are observable independently of timing.

Doctor and offlineMSL compilation pass0 with production metal3.1/fast-math/macOS26.0/source-recording flags. Private actual-GPU oracle passes28cases/203936qualification checks, zero containment omissions/guard/output/expectation errors and unchanged inputs. It includes22072GPU-held eye/probe pairs,8820virtual lookups,281071excluded pairs and16nontrivial cases. Both actualPSOs report shaderValidation1; stderr only expected API/GPU notices. This proves the bounded mapping/containment contract, not full scene output or speed. Private compile-01 receipts retain all28raw outputs and hashes.

Publication test commit627f59f5b1 adds true derived keys, lighting-only completed reuse, native-library/epoch isolation and reentrant full-input guards. Transparency fixture0fb1078d92 supplies five cases/76planned renderer arms with exact output and positive controls; not executed yet. PPD359 retains two corrected precompile fixture premises (native stencil clear0 and supported simple-alpha sidedness); PPD360 retains private invalid-offset raw-evidence correction. None is called an owner-verified production fix. Combined build/native execution/staging are next; no viewer or profiling has started.
