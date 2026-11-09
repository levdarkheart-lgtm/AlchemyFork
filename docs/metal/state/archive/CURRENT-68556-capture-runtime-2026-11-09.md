# 68556 native capture checkpoint — observed runtime, 2026-11-09

Renderer d1966efd09654659e83c0f2df926a36e8d7f023c, product source
cf398b2fabd40fbebb24e1d77ba63fe814af9d17 (docs-only descendant). Native early
shadow admission, obsolete probe duplicate-set removal and opt-in provenance.
No product shader changes. Physical M1 Max/64GiB, macOS26.5.2/Xcode26.6.

## Product and setup evidence

- tools/metal/build.sh configure exits0; host build exits0, Release26.4.0.68556.
  Initial restricted Xcode attempt exits65 on DerivedData permissions, before
  compilation. Host log `/private/tmp/alchemy-metal-20fps-product-build-host-20261109.log`.
- Executable SHA256 `264e4fe2d7f2169195471be7cb6d415e8945d196e93f77ea62830a367c87f1aa`.
  Metallib `5bd02961a77fdfcef9d28410e9f92fc86d1de08197aec113660e0cd0ccd779d1`,
  identical to68554/68550. verify_app349 ARM64 slices; deep strict signature pass.
  Main binary links Metal and no OpenGL/Vulkan/MoltenVK framework.
- Full Release/RWDI source-checkpoint suites each pass66,962 assertions/79 cases;
  llrender2030/229 (one Noto palette skip), benchmark423/13. Native API/GPU
  validation on actual host. Logs `/private/tmp/alchemy-metal-20fps-{release,rwdi}-*`.
- Current location unchanged, Ultra quality6,3456x2168, six hero faces1024,
  original shadow cadence. Native fullscreen via CtrlCmdF before login; Return
  logs in using the remembered account. Avatar not moved.
- CUA world controls and screenshots timed out after login. Runs01/02 are
  diagnostic-only:01 could not confirm environment/preset;02 had3416x1876 and
  a Python binary header rejected by native LLSDSerialize::fromBinary.
- Run03 uses the existing startup loaders: all numeric Preset123 settings plus
  name/type3 in a temporary graphics preset, and session environment persistence
  with a headerless binary map selecting exact Midday asset
  c46226b4-0e43-5a56-9708-d27ca1df3292.28 live settings/location/identity checks pass.
  Native log confirms file load. Graceful exit later rewrites the exact Midday
  asset ID, confirming the effective saved local sky at exit; the release log
  does not positively timestamp the asynchronous asset-completion callback.
- The temporary environment file was absent originally; original absence and
  persistence preference were restored after graceful exit. Payload, proof and
  native exit evidence are private under
  `artifacts/metal/private-startup-setup-plan-20261109.json`.
- Camera snapshots remain unchanged through warmup/sampling, and hero/HUD
  camera restoration checks all pass. Intentional infinite reverse-Z far clip
  serializes null and makes finite_camera_values=false; all actual vectors and
  matrices are finite. This is not evidence of a malformed camera.
- No screenshot/visual-parity success is claimed. Startup settings and native
  rendering provenance establish the measurement configuration; image parity,
  asset-completion timing and source-isolated attribution have limits.

## Frame measurements

Accepted run `artifacts/metal/qa-runs/candidate-68556-startup-20261109-03`, PID23144
(now exited).120frames plus300seconds warmup, then300 consecutive exact-native
frames2892–3191, zero GL fallbacks, every presentation state submitted.
CSV `artifacts/metal/private-68556-startup-20261109-03.csv`, SHA256
`2516022d2a5937f424630fca79630fdedd15c27b4cb7389d820d7ae3571d126a`.

| Metric |68554 |68556 |
| --- | ---: | ---: |
| Mean ms |102.678943 |100.613020 |
|1000/mean FPS |9.739095 |9.939072 |
| p50 ms |101.875709 |98.968667 |
| p95 ms |119.073875 |114.090125 |
| Median physical native draws |55,647 |52,400.5 |

Observed throughput+2.05%, p50−2.85%, p95−4.19%;20 FPS is not reached.
Do not attribute the entire small change to source. Early coalescing can merge
short/scalar runs previously excluded by late bulk thresholds, plausibly
reducing physical calls without implying scene geometry was removed.

perf_compare.py --require-provenance passes row metadata but not performance
targets or required multi-scene coverage. The historical raw scenario label is
pbr-interior; run03 used private-ultra-scene-01 for the same private location.
A derived CSV changes that label only, explicitly recorded with both hashes;
raw rows remain untouched. Comparison/alias proof are in the metrics directory.

## Separate CPU and GPU traces

Raw/exported data `/private/tmp/alchemy-metal-68556-metrics-20261109.0MBFBe`.
30s TimeProfiler followed by separate30s MetalSystemTrace, after unprofiled CSV;
no builds/prototypes/exports overlapped those captures.

Main Running28.772s/30.621s≈93.96% of ONE core. Disjoint hero14.378s49.97%,
ordinary1.730s6.01%, remainder12.664s44.02%. Overlapping family unions:
shadow capture/finish/replay9.675s, culling5.115s, G-buffer4.732s,
texture/sampler1.954s, arena/data setters1.882s. Do not sum these families.
Fixed-window totals are close to68554; no large CPU-family improvement proven.
Unresolved symbols include linker-folded trivial getters; source supports a
buffer-length alias under setters, not an invented unrelated crypto cost.

| Traced metric |68554 |68556 |
| --- | ---: | ---: |
| Presents/s |7.998 |8.291 |
| Target-viewer GPU active union ms/present |86.5874 |86.7047 |
| Whole-device active union ms/present |88.8236 |88.7509 |
| Encoder CPU ms/present |44.0523 |43.9326 |
| Submissions/present |29.3755 |29.3386 |
| Main base labelled ms/present |17.194 |17.218 |
| Main atmospheric encoder aggregate |16.940 |17.139 |
| Hero aggregate |35.193 |35.547 |

GPU unions remove all cross-channel/nesting overlaps; encoder-labelled spans
are not additive frame costs. CSV metal_gpu_ms sums command-buffer spans and
is not wall GPU frame duration. Profiling changes pacing; compare unprofiled
CSVs for FPS. Both traces thermal Nominal,9 cached targets/292.966MiB,
driver4.4045–4.5276→4.3942–4.4659GiB. Sparse memory samples retain discard bias4.

Current initial main opaque segment103 records costs about0.301ms/present.
The atmospheric encoder includes528 records:136 alpha_simple,24 alpha_fullbright,
33 alpha_material,230 alpha_pbr,104 glow and one haze marker. Its17ms aggregate
does not isolate haze arithmetic. Full post input644 records; accepted CSV
snapshots at2973/3100 have identical composition and64/67 packed probes.
Historical20ms initial-opaque swings lack this fingerprint; unresolved.

Main queries in sample boundary interval (299 frame-index intervals):196253
acquired/attached,167971 unique zero,28241 nonzero; no expired/pending/duplicate
polls;118422 spatial-group skips. One new OCCLUDED transition is consistent
with state established in warmup. This proves actual main-camera rejection,
not exact saved draw count or hero-face occlusion.

Two settled resource intervals reconcile successful source/physical counts.
Early joins remove nominal10357 main and12327.5 probe source records per main
frame. Old late counters cover a different eligible set; no exact causal
percentage or identical source-membership claim follows.

## Evidence hashes and next boundary

- CPU comparison `fb94f3f54fcda791dc8657fcb352bd4e59d6fa81bcc6fe2cddd33585d678fe84`.
- Coalescing summary `e27a4514b0bd27dd8ca5a3f79e9b248351cc76518ddbb6923a911e710eec350a`.
- GPU comparison `00762e9760007957a06d10bea0c2a828168d5cb510bd6a094deb33f74de51faf`.
- GPU union detail `3a080bda6487f04533b0c5dd383eea094780ee20902c484104fab35ed20bb314`.
- Post/resource summary `522cf2b48d5b593b22c21e92bf17978542dc837a96b46675574c0b621fc70b72`.

Goal remains active. Next candidates are measured local400B constant reuse and
ordered probe-candidate skipping; always-on mask and named accumulators are
rejected. Preserve every required Ultra view/caster and exact selection/tap
semantics. Build/test/commit successful candidates before another actual run;
do not infer20 FPS from mechanism tests or sum overlapping timing channels.
