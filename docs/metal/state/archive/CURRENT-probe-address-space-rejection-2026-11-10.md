# Probe-buffer address-space experiment — rejected

Observed on the physical M1 Max/64 GiB/macOS26.5.2/Xcode26.6, 2026-11-10.
Retained renderer checkpoint is `f549a796d2`; integration base is
`7772da0422a9d589f1bcbd8fd5e3805531e4a5d6`. This experiment produced no viewer
FPS result and was removed after a slower GPU result.

## Mechanism and original contract

The original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`
`class3/deferred/reflectionProbeF.glsl` sampleProbeTaps/probeMix/tapHeroProbe
path was inspected before editing. Preserve first owner, neighbor traversal,
32 accepted probes, automatic/manual/default blending, SH/radiance and hero
ordering. The native address-space choice has no OpenGL equivalent.

The candidate cloned eight helper functions and the actual base geometry
fragment. Only names and the probe parameter's `constant&` to `const device&`
address space changed. Reversing those transformations proved exact source
equality; the complete original MSL prefix stayed unchanged. Both arms bound
the same immutable49,216-byte buffer at fragment1. No new upload, allocation,
layout, resource lifetime, selection or arithmetic was introduced.

A default-off scene flag selected a lazy HDR/non-HDR PSO, restoring the owned
library pointer when copying the original descriptor. The water specialization
was preserved. No viewer producer enabled this experiment.

[Apple's shader-core guidance](https://developer.apple.com/videos/play/wwdc2020/10632/)
supports testing device access for fragment-dependent indices, while uniform
reuse can favor constant access. This mixed payload therefore required actual
measurement; the guidance did not predict a gain.

## Actual commands and failures

Root-owned integration packet:
`/private/tmp/alchemy-metal-main-probe-device-integration-20261110.zpiz1zci`.
The sandboxed doctor/build failed1 on denied sysctl/compiler-cache access.
The host doctor completed0 and confirmed M1 Max, unified memory, Apple7 and
58 GiB recommended working set. Host Release build completed0 using:

```text
cmake --build build-Darwin-ninja-os-arm64 --config Release --target almetal_tests -j4
```

Current recipes recorded O3/NDEBUG/ARM64/C++23 and Metal3.1/fast-math.
The first API+GPU preflight failed42 before comparisons: the fixture omitted
normal_hdr=true while asserting/uploading RGBA16 normals. The renderer correctly
selected RGB10A2 independently of hdr. The format assertion was retained.

Corrected packet `...normal-hdr-20261110.56rfkoff` built0, then failed42 after
32 ordinary pairs and its first active hero pair. Its hero-off control supplied
an invalid nonnull hero pack with count0. The control was corrected to pass a
null hero pointer, then restore the active pointer. Both failures remain raw
evidence; neither was an A/B output mismatch or a renderer repair.

Final packet:
`/private/tmp/alchemy-metal-main-probe-device-valid-controls-20261110.jftrhxxe`.
Its `run.py build-host`, `preflight-host`, and `quiet-host` each exited0.
Corrected test source SHA256:
`2acc25297200e673b433b5e4457e6a0c793d08b42d0c61cb5d464776a3ba3eb4`.
Executable `9aaad59e358068b19c3ebfe88438b8f883626218b08fa8658e8e632bf1816205`;
bundled offline metallib `ff4dc5f4386f607d11b3c7e30d21a2945bb448f7116c21d2cb4b52e6b8aa27be`.
Both processes used identical source, executable and library hashes.

The API+GPU preflight passed3319 assertions/1 case, with actual enabled-layer
preambles, 40 small strict pairs and six1024-square pairs. HDR color bytes,
raw depth/stencil and all four immutable input MRTs matched. Mixed legacy/PBR,
forward/reverse-Z, classic/modern, SSAO, default/dense, observable late64th
neighbor/manual box and active box/sphere hero controls were covered. Positive
SSR, directional cube/mip variation and non-HDR output remained unverified.

## Quiet cost and decision

The separate validation-disabled process passed5521 assertions/1 case. A
process inventory confirmed no viewer/profiler/other test was running. The
runner scrubbed validation/capture/HUD/stage environment variables. It retained
all168 samples: six cases, seven rotated ABBA/BAAB blocks, four samples/block,
fixed common repetition4, nominal thermal state. Complete renderer CPU
precommit and whole command-buffer GPU time were measured separately from
commit, wait, callbacks and diagnostic cleanup.

| Dense case | Original GPU ms/invocation | Device GPU ms/invocation | Mean change | Paired-block range |
| --- | ---: | ---: | ---: | ---: |
| Modern | 6.120295 | 6.438832 | +5.205% | +2.063% to +11.268% |
| Classic | 5.038742 | 5.334813 | +5.876% | +1.759% to +8.538% |

Both dense cases were slower in all seven blocks. SAME-original mean drift
was−0.143%, SAME-device+0.893%; individual control blocks ranged−2.450% to
+5.459%. Sparse apparent gains changed sign between blocks and used short
commands. CPU precommit differences were noisy and did not establish a gain.
The8–20ms command-duration target was missed by162/168 samples (56 below8ms,
106 above20ms); every row was retained. This limits precise generalization,
but provides no reason to admit a variant consistently slower on dense work.
The synthetic logical probes shared a physical cube/SH row; physical target
identity and generic owner-retirement behavior were not fully controlled.

Independent audit actual0, SHA256
`8df2480a2a79ec8a19b36965c45410ff4619ded702fb136de0b28f60750b89c7`,
agrees with the root's independent arithmetic and recommends rejection. Final
findings SHA `d2807f7b5af9315b016f2869bb3dcc549d0800ebce4885e621e8528ff8492bc1`.
No viewer trial or retry was run to seek a favorable result.

All four experimental source/test files were restored byte-for-byte to7772,
including the original MSL SHA080fba9e and retained water fixture SHAe68cb9fc.
Normal viewer68610 was untouched. Ninja test artifacts still contain the
rejected experiment and MUST be rebuilt before reuse. All jobs are terminal.
57 raw source/command/failure/result/audit files are hash-preserved locally
under ignored `artifacts/metal/private-main-probe-device-rejection-20261110/`.

## Product advancement and next boundary

This closes an implemented and measured GPU hypothesis without regressing the
retained viewer. It does not improve FPS or establish a hardware limit. The
latest retained viewer result remains9.785FPS versus a fresh9.257FPS control,
with the previously recorded live-workload caveat. The20FPS goal remains active.

A separate source audit found no justified small haze change: its timed encoder
also contains later alpha, the math/order match original1eb, and its depth
snapshot preserves real read/write dependencies. Do not remove that copy or
revisit rejected arithmetic variants on the aggregate timing alone.

Next inspect coverage of the already successful probe-shadow function constant
in other actually used shader families. Root owns implementation/GPU; agents
perform read-only source and existing-evidence audits. Full parity, multi-scene
performance, stability, strict no-GL and independent build review remain open.

At the owner's request, stable7772 (including water optimizationf549) was pushed
to the existing GitHub feature branch. GitHub CLI lacked authentication and the
browser was signed out; the connector cannot dispatch workflows. The owner
chose to trigger `Tag a Build` from another device, with Project/Metal Alpha3/
26.4.0.68611-alpha3 on codex/apple-silicon-metal. No packaging run was observed
as of the last read. The pushed harness run42799555537 passed; it is not an app
package. No private evidence or experimental code was uploaded.
