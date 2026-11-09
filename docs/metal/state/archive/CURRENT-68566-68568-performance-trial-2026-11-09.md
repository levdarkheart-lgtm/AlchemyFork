# CPU build repair and unsuccessful opaque-PBR viewer trial

Observed on physical Apple M1 Max / 64 GiB, macOS26.5.2, Xcode26.6,2026-11-09.
Branch `codex/apple-silicon-metal`. The20FPS goal remains active and unmet.
No repeatable FPS gain was established by this work. The owner questioned the
lack of progress after11hours; test counts and CPU relief do not meet that goal.

## Confirmed build defect and CPU result

`59177ae1df35a210e83122cb49cc5cf8d80a0a3e` repairs empty Darwin/Xcode Release
C/CXX compiler-cache defaults. Previously27of36targets compiled atO0; the
completed post-fix build contains1607actual compile actions across36targets,
allARM64/O3/NDEBUG. Seven guard cases and independent response-file review pass.
Nonempty custom flags and other configurations are preserved. Historical cause
of the empty cache remains unknown. See the [original/build record](CURRENT-67611-original-and-release-flags-2026-11-09.md).

Measured68566 CPU main-thread Running fell from91.476% to72.768% of one core.
Broad culling sampled time fell4.886→1.536seconds per roughly30-second profile;
inlining changes limit symbol-by-symbol comparisons. Draw encoding, shadows,
G-buffer preparation and texture/sampler calls remained substantial. This is
CPU relief, not an observed FPS improvement.

Clean matched GPU profile: viewer-active interval union109.245ms per present,
83.169%wall, nominal thermal state,4.389–4.457GiB allocation. Large inclusive
labels remain main base17.515, main haze17.027 and hero above-water21.648ms/p.
Labels overlap and include subsequent draws: they cannot be added into a frame
budget or treated as individual shader costs. No shader-line attribution exists.

Two earlier GPU attempts were excluded: a systemwide recording and a second
recording overlapping its finalization. They are preserved with an explicit
scope note. Only the later matched `--attach3810` trace was analyzed. Each
accepted recording/export completed before dependent analysis.

Private profile evidence: `/private/tmp/alchemy-metal-68566-metrics-20261109`.
FINDINGS SHA256 `545981b1e83336446bbf2ca3ac62a1b4339a0de7c8fd0bf90592533bb2ce7728`;
analysis-summary SHA256 `39030cf1db2137017a11f0e8eac1abfcf7d0b148bb8f04fc42cd444c86a6784d`.

## Retained forward binding repair

`62e2f9abea1cb6626be93961cc0f04c01b88ded5` binds only the populated1..8forward
material/PBR slots. Scalar map setter counts become4PBR/3legacy rather than32/24;
actual native calls depend on encoder caching. Texture views, slot offsets,
ordering and shader bounds remain unchanged. New GPU regressions cover1/2/8,
large selectors,rigid/skinned, stale tails, haze restarts and linear/sRGB views.
Focused8918assertions/1case and full391162/89 EACH Release/RWDI pass real Metal
API+GPU validation. Shader library remains bit-identical to68566.

Source-exact app68567 was built, verified349ARM64 and deep/strict signed. It was
not measured. Preserved `/private/tmp/alchemy-metal-68567-binding-only-baseline.app`;
executable SHA256 `ba2d397e36797ce3f2c11bfcb9582512843be892c3bc515dc1982bd26d7374da`;
metallib `8bbfe4c6f3ce45eabd4fb58049cec413178213da480b23020447f01ef864b943`.
Logs `/private/tmp/alchemy-metal-forward-prefix-{release,rwdi}-*-20261109.log`.

## Experimental opaque/MASK split, then rejection

Trial commit `d3d1393d97052418bf39bf80cc3ca007a4f00279` adds eight matching opaque
PBR G-buffer PSOs and a no-discard entry point, selected only for
`batch.alpha_mask == false`. Generic MASK body, binding math and output packing
stay unchanged. Exact original1eb44bb's `lldrawpoolpbropaque.cpp` and
`class1/deferred/pbropaqueF.glsl` select separate opaque/MASK programs and compile
the discard only for HAS_ALPHA_MASK. No Metal early-fragment override was added.

Private200-state correctness produced800MRT and200raw-depth matches. Initial
microtimings were confounded by separate libraries/targets. One corrected retry
used one combined library, the same actual MASK PSO and the same targets; the
identical MASK control still drifted. No reliable percentage or general benefit
was credited. Source and production-test patches received independent reviews.

The production regressions exercise actual renderer PSO selection across all
four MRTs and raw depth, rigid/skinned1/2/8slots, all four HDR/normal formats,
and alpha0/255 invariance versus always-passing MASK. Existing rejecting-MASK
coverage remains. Full Release/RWDI API+GPU suites each passed392926assertions/
89cases. They do not replace live scene correctness or prove performance.

Actual commands and exits, logs under `/private/tmp/`:

- `tools/metal/doctor.sh`: restricted1(sysctl denial); host retry0,
  `alchemy-metal-opaque-pbr-doctor-host-20261109.log`.
- An initial Xcode `almetal_tests` target failed65 because tests live in the
  configured Ninja tree; corrected immediately. No result was credited.
- `cmake --build build-Darwin-ninja-os-arm64 --config Release --target almetal_tests --parallel 4`:0;
  corresponding RelWithDebInfo:0.
- Both `almetal_tests` binaries with `METAL_DEVICE_WRAPPER_TYPE=1`,
  `MTL_DEBUG_LAYER=1`, `MTL_SHADER_VALIDATION=1`:0. Logs
  `alchemy-metal-opaque-pbr-{release,rwdi}-gpu-20261109.log`.
- `tools/metal/build.sh configure`, `tools/metal/build.sh build`:0.
- `tools/metal/verify_app.sh <exact68568app>`:0,349ARM64 slices;
  `codesign --verify --deep --strict --verbose=2 <exact68568app>`:0.

68568 preserved at `/private/tmp/alchemy-metal-68568-opaque-trial.app`;
executable `b2c5175b0c6c59eb06f56b13d39284a5fa44d4be9a80b6e5dbd7e5be598a9194`;
metallib `9c1e440c20b4bf2f4898150b8ec85c7630350278fa745f187d97327f57c2d60d`.

## Controlled viewer comparison

Full screen before login via Mac shortcut; exact binary identity pinned by QA.
Actual logged-in location unchanged, Ultra6,3456×2168,Midday,numericPreset123,
all43live controls pass. The world AX read after the Midday shortcut timed out;
the seeded environment and collected main/sun metadata match the baseline.
No screenshot was obtained; do not claim visual pixel parity. No additional
profile, build or GPU experiment overlapped the measured viewer window.

| Metric | Settled68564 | Optimized68566 | Combined trial68568 |
| --- | ---: | ---: | ---: |
| Mean frame ms |107.873381|112.806384|108.776442|
| p50 frame ms |106.484646|110.616500|108.786063|
| p95 frame ms |118.416|138.295291|113.496416|
| Throughput FPS |9.270128|8.864746|9.193167|
| Mean encoded draws |53867.017|54158.580|54392.410|
| Main visible probes |97|96|97|
| Sampled discard bias |not sampled|4 all300|4 all300|

68566frames6070–6369 and68568frames4968–5267 are exactly300consecutive frames,
all exact native/zeroGL, all256assigned probes complete/faded/current, stable120.
Trial has zero camera-change frames. Camera basis/projection match; vertical
position differs only0.0057mm. Environment and hero/HUD restoration records match.
A one-probe/draw-count difference does not establish a scene change.

Against68566, mean improves3.57%, median1.65% and p9517.93% in this one sequential
window. The candidate combines forward binding and the shader split, so neither
is isolated. It remains below the older9.27FPS sample and far below20FPS. No
repeatable end-to-end gain is established. The speculative split was reverted
by `d967552ae823ae6d1cc7c979c8528c4fe10fc5e8`; its patch/commit/app remain available
for future general testing. Default renderer sources now match62e2 exactly.
This is not proof that the split can never benefit other workloads.

Raw local files:

- `artifacts/metal/private-d3d139-opaque-ready-20261109-01.csv`, SHA256
  `5e661140e203e11ff491868e48bcbf90aef466e4540e37711b25cf5ecb2da246`.
- Matching `.provenance.jsonl`, SHA256
  `1fa5d6ed6c942503e6710b0f12cb951cb81b9f74d3bae7fd6b5bec44515d8513`.
- QA `artifacts/metal/qa-runs/candidate-d3d139-opaque-ready-20261109-01`.
- Official `perf_compare.py --require-provenance`:0; report
  `/private/tmp/alchemy-metal-d3d139-comparison-20261109.json`, SHA256
  `f6472e24dbe61ae713ff7d94220e636214274ae281b7ca367de832938fc5ba14`.
  Performance target remains false; three required scenes are untested by owner
  location constraint. Nearest-rank p95 follows this official tool.
- Independent `/private/tmp/alchemy-metal-68568-opaque-comparison-20261109/`:
  formal JSON `b29f3294fa6ad864cd59b2495845da3c7474adab5538da220fa11e7699f8f470`,
  FINDINGS `5438cb73d70b2cb76b3e66b7fc8e4736474d550e06128fbfd8b7ec0bb42e2e7b`.

PID44908 gracefully exited/Goodbye11:35:33UTC through the verified LLApp SIGTERM
quit handler; PID absence checked. Temporary environment/persistence restored
11:35:55UTC; `private-d3d139-startup-setup-plan-20261109.json` cleanupDONE.
No post-workload packet or new profile is claimed for68568.

## Product advancement and remaining work

A real Release compiler defect is fixed and CPU relief measured. Forward binding
work is reduced with production correctness checks. Neither has demonstrated
an FPS gain; the speculative shader candidate was not retained. Tests/builds
are not substitutes for the owner's20FPS objective.

GPU work remains substantial in both main and reflected views. Reaching50ms
from roughly109ms requires a much larger general improvement than this trial.
The existing evidence does not prove hardware infeasibility. Exact-original
quality parity, streamed-image/global-pressure accounting, full scene coverage,
feature/stability gates and strict no-GL release validation remain open. No
quality, view cadence, caster or location requirement was waived.

## Restored default build68569

Revert checkpoint `d967552ae823ae6d1cc7c979c8528c4fe10fc5e8` was configured,
fully built, verified349ARM64 and deep/strict signature checked: all exit0.
Logs `/private/tmp/alchemy-metal-d967-{product-configure,product-build,verify-app,signature}-20261109.log`.
App is the canonical `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`;
executable `0e4a0b138ceec2fd1d37ca72a85fa8c2a05d9a0dd1c734116b3074831951b4f4`;
metallib `8bbfe4c6f3ce45eabd4fb58049cec413178213da480b23020447f01ef864b943`.
Default app has not been launched or measured. Renderer/test sources match62e2;
that source's prior full391162/89 suites both passed. Current Ninja test binaries
still contain the trial and must be rebuilt before another source-specific claim.
No viewer/build/test/profile remains running. All temporary state plans are restored.

Final documentation checks: the initial CURRENT rewrite failed the harness on a
stale feature anchor, required headings/milestone table and invariant wording.
These were restored; `python3 tools/metal/validate_harness.py` then exited0
(32required files), and `git diff --check` exited0. No tool code was changed.
Independent final state/ledger review found no material factual blocker.
