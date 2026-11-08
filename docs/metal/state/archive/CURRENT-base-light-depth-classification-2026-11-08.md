# Deferred base-light depth classification and benchmark warm-up — 2026-11-08

This checkpoint records a bounded M1 Max optimization and the evidence rule
exposed by its attended Ultra run. It is not the required paired 300-frame
OpenGL/Metal performance gate and does not close visual parity.

## Original behavior and implementation

The exact OpenGL reference is renderer commit
`1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` at
`/private/tmp/alchemy-reference-1eb44bb`. Its deferred-soften path draws a
far-plane triangle with ordinary depth coverage for geometry, rejecting clear
sky before the expensive fragment shader, then uses a separate equal/clear
depth sky shader. Metal had merged geometry and sky into one expensive
full-screen fragment path without a depth attachment.

Commit `c436b9fe6e` restores that classification at the existing Metal ownership
boundary. A far-plane vertex selects forward- or reverse-Z. One render pass
borrows the exact G-buffer D32F/S8 attachment and base-light output color;
ordinary coverage depth shades geometry and read-only clear depth transfers
the already selected WindLight/HDRI sky. No depth copy or persistent target is
added, and the prior sky-family marker, transfer and classic-mode semantics are
preserved.

Two source-led atmospheric experiments were measured and rejected before
commit: separate ordinary/on-water shader specialization produced 8.133
ms/present haze versus the retained 7.883 ms/present (+3.2%), while replacing
the constant fractional `pow` with square-root/reciprocal operations produced
8.190 ms/present (+3.9%). Both changes are absent from the committed source.

## Bounded GPU evidence

The preceding `68512` trace is documented in
`CURRENT-ultra-mirror-performance-diagnosis-2026-11-07.md`: 179 fused presents,
11.470 ms/present main base light and 14.738 ms/present haze. The source-exact
depth-classified trace is local at
`/private/tmp/alchemy-metal-base-classification-trace-20261108-1/metal-system.trace`
and contains 181 presents. Normalizing resolved GPU-channel intervals by those
presents gives:

| Main-world pass | 68512 ms/present | Depth-classified ms/present | Change |
| --- | ---: | ---: | ---: |
| Deferred base light | 11.470 | 2.688 | -76.6% |
| Atmospheric haze | 14.738 | 7.883 | descriptive only |

The base-light result is directly owned by the code change. The cross-run haze
difference is not attributed to it because the live scene, residency and trace
timing can differ. In the candidate trace, haze remains the next measured
shader bottleneck; ordinary local lights and projectors are 3.466 and 3.947
ms/present respectively.

The candidate GPU-interval, present, encoder-export and submission-summary
SHA-256 values are respectively
`9d4a613a3de892402c69da0c5a17097ef80f855d27b3df7a77ad6e5e3be9bfb7`,
`273122ac9f9a44c3455897968c669d7bcf361382629f9bb01662c70707fe0907`,
`2f9d25f2bec0f7d0d39eeed2e510336ff113d8ae701cfd00242aa4306421dc58`
and `35382cc044c5f5c571d577d3842529f22649588e359871a0ce04fd09027b2da7`.
The specialization and strength-reduction GPU-interval exports are retained at
the corresponding `/private/tmp/alchemy-metal-haze-*-trace-20261108-1`
directories with SHA-256 values
`4da20cd773dfab2b29f748b4d75b7d10d540a2cb3c2f8658ee72e889faee93be`
and `ab99903eb975ab627aecf18e8aaf150383e23b8fb9666accd8e09054f8cd9ba8`.

## Startup-to-steady-state finding

An attended working-tree `68516` run used the strict Ultra profile at `3456x2168`,
256 m, shadows 2, SSAO, 256 probes, mirrors at 1024/update rate 1, fixed
exposure 1.0, ACES 0.7, zero sharpening and no SSR/DoF. Without touching the
environment, FPS was 22.8 at 02:06:44, the six-face hero radiance published at
02:06:50, and FPS was 9.1 at 02:06:54 before settling at 8.2-8.9. Driver
resources rose from 1.87 GiB at renderer start to about 4.9 GiB at steady
state. Selecting Midday at the fixed camera then held 8.3-8.7 FPS for the next
45 seconds and did not cause a second collapse.

Therefore the observed startup burst is not admissible performance evidence:
it precedes complete scene/probe/mirror residency. Commit `1e962c84e2` requires
both warm-up thresholds in the opt-in collector: 120 eligible frames and 30
elapsed seconds by default, either explicitly configurable to zero. This stops
fast incomplete startup frames from consuming the frame-count warm-up before
the full workload activates.

The working-tree runtime gate passed the exact `68516` executable, target
coordinates and 19 Ultra controls. This run exercised the candidate changes,
but its bundle metadata was generated before the two commits and is therefore
runtime evidence rather than the final source-exact artifact. The
run/verification/log SHA-256 values are
`18ee6a422e7a647af14b8a73f900aacc20a183a1ec65f93dbeb9812137dfeabb`,
`164f1b168d9cce3210e7261c597983630c55da8ab40c4796831e3832b592e1f7`
and `f97348bfbd736789d9a411549c5092f72632739f15278212144da2da206175d6`.

## Build and remaining boundary

Source-exact commit `1e962c84e2dbf87939955a350dc52cddedc3338e` rebuilds as
Ninja ARM64 Release `26.4.0.68518` through `tools/metal/build.sh`; its executable
SHA-256 is
`764a0e8f5372e62357f7b67b354a0fbe1e50e53c25aed39c466caeb1a1fe3fde`.
The full Metal suite passes 52,798 assertions/57 cases, full viewer tests pass
99,002 assertions/749 cases with one platform skip, focused benchmark tests
pass 58 assertions/9 cases, harness validation passes, deep staging succeeds
and `verify_app.sh` reports 349/349 ARM64 slices.

The 24 FPS objective remains open. The next atmospheric-haze change must be an
architectural, quality-preserving prototype traced against the original; two
cheap shader rewrites already failed measurement. A provenance-complete paired
300-frame OpenGL/Metal run remains required before any product performance
claim.
