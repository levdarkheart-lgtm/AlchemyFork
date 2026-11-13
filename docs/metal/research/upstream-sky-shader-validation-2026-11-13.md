# Native sky shader evidence — 2026-11-13

Final native implementation evidence. The focused physical GPU suite passed; whole-app validation and owner acceptance are separate root-owned gates.

## Source identity and ownership

The native worktree is `/private/tmp/alchemy-sky-shader-20261113`, branch `codex/sky-shader-20261113`, base `2144fa7040384d40f0ea97d4ca54a98ddc2471be`. Only `indra/almetal/almetalworld.cpp`, `almetalworld.h` and `shaders/almetal_builtin.metal` were edited. Root owns canonical documentation, registration, CMake and viewer integration; the CPU agent owns ALSky and pure alrender policy, and the test agent owns focused native GPU tests. No canonical edits, viewer launch, push or source acceptance by this agent.

Commits, in order:

| Local commit | Change |
| --- | --- |
| `27a948adc9e32f2b2a7e336c082b8c9e166f7441` | Modern view-ray sky, compact stars, common water dome, native cube and depth metering |
| `975b6cec178e6354c4c4afe50c5987f31ee3d5df` | PPD-067: skip inactive optics before sampling |
| `f55c53c589d67f134acf973d5dadcb0187d969f5` | PPD-071: correct CPU/MSL payload layout, static checks and GPU probe; PPD-070: scene-taking meter overload |
| `2f2129bd8fc487522c7d9bbe3538a4b956572615` | PPD-074: preserve the original direct meter kernel ABI beside the new depth kernel |

The initial contract is `/private/tmp/alchemy-sky-native-contract-20261113.md`; actual source declarations are authoritative. This report retains failures instead of hiding them with the final code.

## Original implementation map

The reference is landed #584 (`e9d2331376`) and its applicable tails through pinned upstream `408fd6c014e6e3b2b90dfe61265f34df0628f450`. No unmerged topic branch is included.

| Reference family | Original behavior and native boundary |
| --- | --- |
| `496e3edef4`, `dc7cf338e3` | `windlightDomeF.glsl`: intersect the view ray with the sphere, raise the shading point 50 m, apply the retained atmospheric equations. Shared MSL dome replaces mesh facets/skirt. |
| `167cb8b88e`, `c005f5beb5`, `d464042e3d` | Water haze evaluates that same dome directly, including values above 1; the half-resolution SkyBehind target disappears. Preserve the water stencil split and the 1/256 convergence-weight floor. |
| `bec44d6f50`, `826d8c1d40`, `8248f131dd`, `666714d3e8`, `e75ed4cf80`, `92f07a3808` | CPU deterministic star field/layout/culling; one packed vertex per ordinary point and six per spiked quad; per-star math in the vertex stage; write only the selected star attachment. |
| `418a79c400`, `d78b76e91a`, `806d581b60`, `ae00c45736`, `81837f6d42` | View-ray sun/moon/bloom/cloud/aurora functions and packed per-pixel meteor streaks. |
| `50daef6661` | Probes-off environment cube: GPU dome-only bake, 64 texels per face, seven mip levels, complete neutral fallback. |
| `ba9da2e58b` | Exposure distinguishes far depth and water over cleared G-buffer normals, rather than old sky flags. |
| `5e919e7ad2`, `d5bbdc7362`, `2cfa6eed92` | Composite every sky layer in registers into the lit screen at clear depth, after geometry base lighting and before local lights. |
| `cdb4531ada`, `3cc20b4971` | Final ray-quad and projected meteor degeneracy handling, imported by the CPU owner. |
| `07094f82fe`, `9c451dc4a2`, `b929675d9c` | Final HDR finite limits, meter/history safety and probe transport. Existing native finite safeguards are retained rather than overwritten. |

Inspected final shader files include `windlightDomeF.glsl`; `skyCompositeV/F/MainF`, `skyBodiesF`, `skyCloudsF`, `skyAuroraF`, `skyMeteorsF`, `skyCubeF`, `starsV/F`, `valueNoiseUtil`, `luminanceF`; and class3 `hazeF`/`softenLightF`. ALSky view admission, star draw, composite binding, dome binding and cube ownership were traced through actual pipeline ordering. Read-only snapshots are under `/private/tmp/alchemy-sky-shader-audit-20261113/`.

## Final native contract

`ALMetalWorldModernSkyConstants` is 816 bytes. It embeds bare 192-byte dome and 240-byte cloud payloads. The meteor block is a separate 3072-byte constant slot; neither exceeds Metal's setBytes limit. Both C++ and MSL assert the sizes and all major member offsets. The renderer copies scalar values into command encoding; the CPU NativeView retains texture and buffer owners.

The G-buffer producer clears only the selected star attachment to black **before opaque geometry** through `sky_star_attachment` (-1, 0 or 3). `renderModernSkyStars` runs after opaque geometry. It borrows only that attachment, preserves every other color attachment, depth and stencil, and selects point or quad pipelines from compact 16-byte vertices. Vertex work includes moon masking, extinction, twinkle, chromatic variation, glare and output gain; fragments evaluate only the star profile. The CPU owns frustum and brightness selection. An eligible zero-draw star view keeps its black clear and composite bit; root skips the empty draw call.

`compositeModernSky` borrows the lit color and scene depth, uses depth EQUAL without writes in forward and reverse Z, and writes RGB in linear light with alpha zero. It unprojects at depth 0.5, since the infinite far plane is not a valid unprojection point. Layer order is dome plus stars, bloom alpha, sun alpha, moon alpha, plus meteors, plus aurora, then cloud alpha. Derivative-taking bodies and clouds run before the opaque-cloud branch. The remainder is skipped under an opaque cloud deck. Final output is bounded to 65504. HDRI stands alone or splits against the dome, retaining values through 65024 before classic gain. The authored star brightness chain, including its intentional second decode during composition, is preserved. Body, cloud and optics maps use raw storage views where upstream explicitly performs display-space math.

Modern water haze receives the same scene's dome snapshot and optics maps through `modern_sky`; no SkyBehind target is needed. The old map route exists only for retained fixtures. Exposure receives the **completed post-water depth**, and, when water was drawn, the same target as authoritative `water_stencil`. It copies original G-buffer normals under water surface bit 2 into a zeroed mask, then classifies `farDepth || mask.a > 0.9`. `exposure_depth` forces shader-readable output even with waves and SSR disabled. Both adaptive and fixed/classic production meter callers use the scene-taking API. Old no-depth fixtures retain their original behavior.

`createNeutralModernSkyCube` defines every face and mip as RGB 64/255, alpha 1. `bakeModernSkyCube` renders the common dome at half display-encoded intensity into a six-face strip, copies it into native cube slices and generates mips. Root publishes a new cube only after a successful independent command submission, retaining the previous/neutral cube on failure. No old live sky renderer or OpenGL mechanism is introduced.

## Compilation evidence

Fresh actual `almetalworld.cpp` compilation uses matching canonical dependencies read-only and the worktree's actual header. The exact argument array is `/private/tmp/alchemy-sky-shader-checks-20261113/world-compile.json`; logs are `world-compile.log`, `world-abi-compile.log` and `world-meter-compile.log`. Final compile status is 0. An early private compile caught a clear block inserted into the wrong function; it was corrected before runtime, and the actual production clear is in G-buffer renderInternal.

MSL was compiled with:

```
xcrun -sdk macosx metal -std=metal3.1 -ffast-math -mmacosx-version-min=15.0 -c indra/almetal/shaders/almetal_builtin.metal -o <scratch>.air
xcrun -sdk macosx metallib <scratch>.air -o <scratch>.metallib
```

Both stages passed. ABI and meter followups also passed the exact fast Metal compile. One unused SKY_MAX_METEORS warning remains; the named constant is retained for CPU/shader contract checks. `git diff --check` passed before each local commit.

## Actual runtime evidence and retained failures

The independent test agent ran these on physical M1 Max under LLDB with actual child status inspection. Fault-stopped children were backtraced and killed, never resumed or passed a signal. No viewer was launched.

| Run | Observed result / evidence |
| --- | --- |
| Old production HDRI, finite texel with exposure 1, 2 and 1e6 | 404 assertions passed, child 0. M1 packing stayed finite. Physical overflow was **not** reproduced; do not attribute an owner incident to the old source limit alone. |
| Initial new modern shader ABI | PID84613 stopped Metal validation: CPU 816 bytes versus MSL 1008 bytes. Child was killed without resume. `/private/tmp/alchemy-sky-modern-27a.log`. |
| Corrected actual layout, source-compiled library | 19 assertions / one case passed; PID85385 child 0, state 10. `/private/tmp/alchemy-sky-layout-green.log`. |
| Corrected layout, exact offline metallib | 19 assertions / one case passed before any further rendering. Test owner retains the exact offline log. |
| Corrected ABI, first six actual modern GPU cases | 132523 assertions / six cases passed, child 0 with Metal validation. HDRI finite/range/far depth, G-buffer/depth/stencil preservation, zero-star selected clears, point/quad stars in both Z conventions and HDR/albedo paths, and all 32766 neutral cube texels. `/private/tmp/alchemy-sky-modern-abi-green.log`. |
| **Final modern suite, all native source through 2f** | **242875 assertions / 13 cases passed**, PID86489 child 0, state 10, MTL_DEBUG_LAYER=1 and exact offline metallib. `/private/tmp/alchemy-sky-modern-permanent.log`. |
| Actual modern MSL, two explicit bounds removed in private control | 12300 failed / 53405 assertions (41105 passed), one failed case; PID86788 child42/state10, no fault. Ordinary input cases passed. `/private/tmp/alchemy-sky-hdri-unbounded-red.log`. |
| PPD-067 fixed-ABI old-optics control, without 975 | Zero-strength NaN optics maps changed an ordinary positive dome to black. One assertion failed, 46 passed; PID85700 child 42, state 10, no fault. `/private/tmp/alchemy-sky-optics-red.log`. |

PPD-071 was a new port defect: I accidentally embedded old raster wrapper types (288/336 bytes) into the modern MSL payload. Separate bare types in f55 preserve the public 816-byte API. The GPU layout probe checks sizes, eleven offsets and five representative typed sentinel reads. Compilation alone had not established layout equality.

PPD-067 was registered before the guard repair. Final upstream branches before fetching a zero-strength optic; the old native shared helper fetched first and multiplied by zero. The repair preserves ordinary finite active behavior and applies to the main sky, water and cube through the same helper. The real negative control is retained; the corrected case passes in the final 13-case offline GPU suite.

PPD-070 and PPD-072 were source-confirmed integration findings reported to root before repair: fixed/classic metering still used old flags, and the caller mistakenly treated optional layer mask zero as no dome. Root now uses the scene meter overload and separate captured sky eligibility. PPD-069 covers the valid empty star draw. PPD-073 is CPU/root-owned dome-only cube capture, which avoids advancing optional layer clocks or building stars during a bake.

PPD-074 was found by source review before executing the retained HDR fixture: it directly binds the original meter with 16 bytes and three textures. The new extended kernel would have required 32 bytes and five textures. Commit 2f restores the old kernel ABI and adds `almetal_world_depth_exposure_meter` for every production scene with completed depth. Both share grid math; no old fixture was edited to hide the regression. The new production depth kernel and its complete water-provenance path pass in the final 13-case suite. Root retains the old direct HDR finite fixture as the separate 16-byte ABI check.

Earlier fixture failures omitted required base-light reflection inputs and failed admission before meaningful sky behavior; they were fixture errors, not product defects. The test agent corrected its inputs before drawing.

## Final focused GPU coverage

The permanent test fixture is `/private/tmp/alchemy-sky-tests-20261113/indra/almetal/tests/almetal_sky_modernization_test.mm`, committed independently by the test owner as `798ec032c4`. It invokes actual production native renderer APIs and the actual shader library. Its thirteen cases cover:

- Actual CPU/MSL layout and typed sentinel reads before drawing.
- Clear-depth HDRI composition without changing opaque G-buffer stores.
- Configured zero-star views, selected black preclear, and valid dome-only mask zero.
- Compact point and quad stars, unrelated attachment/depth/nonzero-stencil preservation, moon masking, HDR/emissive and raw-albedo paths, forward and reverse Z.
- Full finite HDRI range, negative/NaN/Inf inputs, exposure, classic gain and split-screen direction.
- Inactive rainbow/halo no-read behavior.
- Neutral cube completeness and baked cube basis, faces and every mip.
- Body/cloud/HDRI precedence and complete meteor/aurora night layers.
- Completed post-water exposure depth and original normal provenance with both normal formats, both Z conventions, waves and SSR disabled.
- Water-only convergence to the same bright, unclamped ray dome sampled by the main sky.

The exact passing command, preserved in `/private/tmp/alchemy-sky-modern-permanent.log`, is LLDB batch launching `/private/tmp/alchemy-sky-tests-build-20261113/Release/sky_modern_tests.app/Contents/MacOS/sky_modern_tests` with filter `[modern-sky] --reporter compact`, `MTL_DEBUG_LAYER=1`, and `process launch --shell-expand-args false`. Its containment script prints the actual child exit/state, backtraces and kills a fault-stopped process, then quits. Final child 86489 exited normally with status 0 and state 10. No signal was continued or passed.

## Remaining gates

The private bound-removal control uses the same full production MSL and actual API/output, changing only two explicit bounds. Negative input stored -1/-1.099609375; amplified 65504 input saturated physically at65504 on this GPU,480 above the expected65024 before classic gain. It did not establish physical infinity. The command uses Catch2 --abortx12300 and exits normally with42 after reaching that assertion-failure limit. The control is not a CPU-equivalent replacement shader and is not the old application shader. Root owns the retained direct HDR finite fixture, whole-app integration, full build, verifier and comprehensive tests. Same-location reference visuals, GPU captures, measured performance and owner acceptance remain unproved. No FPS, scene acceptance, release readiness or push approval is inferred from these checks. Native source remains clean at `2f2129bd8fc487522c7d9bbe3538a4b956572615`; no source edits are planned after this validation handoff.
