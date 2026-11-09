# Next session — active Ultra20 FPS optimization goal

Continue the ACTIVE goal in this chat. The owner explicitly requests general
native Metal optimization toward at least20 FPS on Ultra and permits automatic
quit/relaunch/login/measurement. Stop with evidence if infeasible. LIVE testing
must remain at the current private location with Preset123; do not move the
avatar or visit other regions. Use Mac shortcuts before mouse fallbacks, verify
the result, preserve all Ultra quality/views/casters. Synthetic coverage must
vary workloads so code is not tuned to this one place.

Read AGENTS.md, CURRENT.md, active ledger and
[campaign](../work/active/ultra-20fps.md). Use agent-continuity, original-viewer,
metal-port and gpu-validation skills. User authorization supersedes older
owner-attended-only handover wording; do not ask again for the same login cycle.

## Current source and artifact

- Root `/Users/dekstroza/Programming/AI/Secondlife/Alchemy`, branch
  `codex/apple-silicon-metal`, renderer d1966efd09654659e83c0f2df926a36e8d7f023c.
- Latest measured normal Xcode Release app is6c694df237/68554. Executable SHA
  `223bdc1054b9cdf73c336a26530431ded5eb4ca001de0339c74390ec41af93db`;
  metallib `5bd02961a77fdfcef9d28410e9f92fc86d1de08197aec113660e0cd0ccd779d1`.
- Preserved full baseline `/private/tmp/alchemy-metal-68554-goal-baseline.app`;
  prior68550 `/private/tmp/alchemy-metal-68550-measured-baseline.app`.
- Physical M1 Max/64 GiB, macOS26.5.2, Xcode26.6, Apple family7; doctor passes.
- Viewer is currently CLOSED for isolated prototypes. Old PID89607 is historical.
- 6c694 production is native direct static shadow packets with slab constants,
  strict compatible index-range merging and populated ordinary/material/PBR
  texture binding. ICBs are optional tests, not production. Old shaders restored.
- Current d1966efd09 Release/RWDI full Metal suites each pass66,962/79 with
  API/GPU validation; llrender2030/229 each (one unrelated font-palette skip),
  benchmark423/13 each pass. Product build/signature/runtime remain pending.
  Logs `/private/tmp/alchemy-metal-20fps-{release,rwdi}-*`. Baseline68554 product
  signature/349 ARM64 slices passed before this goal's changes.

## Current ownership and next build

- shadow_contract completed the early shadow helper/source and independent
  review. Both real GPU suites pass. CPU admission saves0.157ms/10k sources at
  70%joins; heterogeneous nonmerge cases show no large observed penalty. This
  is not a viewer FPS result.
- trace_recheck completed query/provenance and obsolete probe-set cleanup.
  Default-off unique outcomes/actual skips, exact main-camera matrices and
  private JSONL; CSV unchanged. Twelve non-owning duplicate-suppression sets
  were unreachable under native face early returns. Semantic alpha/water flags
  remain. Both suites, pipeline syntax and independent cleanup review pass.
- draw_cpu_costs: layered and named-accumulator candidates rejected after paired
  GPU regressions. No product shader change. Conservative spatial probe-mask
  prototype may be prepared in ignored tmp only; no GPU/timings until root
  releases the product-build/live-measurement window.
- Root: world renderer/shaders/CMake/docs/integration and opaque-pass census.
- Files are committed/frozen as d1966efd09. Next: docs-only handoff commit,
  configure/build source-exact product, verify/sign, then controlled same-location
  login/300-frame test. No heavy work during any live measured window.
  Post census uses127-call samples with explicit source frame/view kind/face;
  count/descriptor hashes are diagnostic, not pixel or all-face parity proof.

## Latest measured result and essential caveat

Pinned full-screen-before-login/Midday/Preset123,3456x2168,21 controls,
warmup120frames+300s and300 exact-native samples: fresh68550→68554 mean frame
118.916→102.679ms, p50117.437→101.876, p95133.511→119.074; observed8.409→9.739 FPS.
Main thread94.28% of one core; hero50.44% of main samples; selected static draw
coalescing69.86%;55,647 median physical draws/frame remain.

Main opaque-post labelled GPU cost swings0.3–20ms across unchanged-shader
runs, including the same68550 executable. Do not assign all+15.8% throughput
to code or blame the old base-light experiment. New camera/pass/query census
must distinguish actual workload, state and scheduling. Encoder CPU savings
exclude pre-encoder preparation; GPU channels and inclusive CPU scopes overlap.

Current private evidence:

- 68554 `/private/tmp/alchemy-metal-68554-direct-shadow-metrics-20261108.jz1nvbtv`.
- Fresh68550 `/private/tmp/alchemy-metal-68550-baseline-rerun-20261108.92x7iims`.
- Tests/builds `/private/tmp/alchemy-metal-shadow-indirect-tests-20261108.HGz01r`.
- Launch config `artifacts/metal/private-ultra-68554-direct-shadow-qa.json` and
  run `artifacts/metal/qa-runs/candidate-68554-direct-shadow-20261108-01`.
- [Full runtime record](archive/CURRENT-68554-direct-shadow-runtime-2026-11-08.md).

## Exact original and preserved fixes

Reference behavior is1eb44bbd1bdc76d0100c75699bad1fb337e8cb45. External validation
HEAD734afd3179 adds diagnostics only. Approved OpenGL app is
`/tmp/alchemy-reference-1eb44bb/build-Darwin-ninja-os-arm64/newview/Release/Alchemy Metal Development.app`,
version67607, executable SHA
`de8fc8f817c0afb2063f641e9c24cbd58054c32c3053f4d0e863232498bb1797`.
Do not use /Applications/Alchemy Beta.app or incomplete67607 backup.

Native occlusion is implemented(e52de13acc); literal Ultra4x anisotropy is
implemented(f799262971); do not redo these as missing features. Main-camera
effectiveness remains unmeasured; cube captures disable occlusion as original.
Preserve retired geometry generations, bounded lights, direct retained shadow
slices, shared/borrowed G-buffer depth, six Ultra faces/two sun slices per face,
main-owned projector sharing and raw-depth PCSS/clamp/reverse-Z/alpha behavior.
Texture-pressure separation7cd1993d8e remains reverted at the owner baseline;
do not restore it without controlled evidence. No release authorization exists.

## Resume commands

Check git status/HEAD and get_goal, read the current owners' messages. Source
and both test configurations are ready. Continue source-exact build→verify→
fixed-location300-frame loop, then separate CPU/GPU traces and inspect new camera,
query and pass provenance. No query-ring bug found:64 result frames; zero
rotations per cube face, one per normal display, matching exact original.
Run tools/metal/doctor.sh, tools/metal/build.sh and tools/metal/verify_app.sh
on the real host. Run validate_harness.py for documentation, tool unit tests
for tool changes. Keep CURRENT<=160 and ledger<=180 lines. Raw captures stay
local. Never declare goal/product complete from a prototype or API-only result.

Prior detailed history remains in the
[archived handover](archive/NEXT_SESSION-before-ultra-goal-2026-11-08.md).
