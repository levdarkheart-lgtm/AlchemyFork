# Native texture-pressure accounting checkpoint — 2026-11-08

## Finding and classification

The claimed Metal texture-pressure scope mismatch is **confirmed** at the
pre-fix checkpoint. The automatically detected streamed-image capacity is a
deliberately bounded fraction of the device recommendation, but
`LLViewerTexture::updateClass()` compared that small target with the maximum of
the viewer's texture/vertex estimate and `MTLDevice.currentAllocatedSize`.
The latter covers the complete device resource set, including render targets,
buffers and staging resources. This made unrelated Metal allocations capable of
driving fetched-image discard bias.

The separate claim that this causes repeated decode/upload cache churn or a
specific frame-time cost remains unproven. The pre-fix runtime demonstrates
maximum discard bias and substantial generic resource create/destroy activity,
but the existing counters do not attribute that activity to fetched-image
decode/upload transactions.

## Exact arithmetic and live settings

The M1 Max reports a recommended working set of `62,277,025,792` bytes, or
`59,392 MiB`. `ALRenderCapabilities` divides that by 16 and clamps the result to
2–8 GiB, yielding a streamed-image capacity of `3,712 MiB`. The registered and
live `RenderTextureVRAMDivisor` is `1`; `RenderMaxVRAMBudget` is `0`. The
controller's 20-percent/512-MiB headroom rule therefore produces:

```text
min(3,712 - 512, 3,712 * 0.8) = 2,969.6 MiB = 2.90 GiB
```

The `2` in the `LLCachedControl` constructor is only a missing-setting fallback,
not the registered or observed value. The implementation now also clamps a
saved divisor of zero to one.

The owner-attended Release `68532` process was still running from PID `14685`
while this finding was investigated. From 14:13 UTC onward, 86
`#MetalMemory#` samples recorded:

- driver allocation `4,807.58–4,931.12 MiB`;
- `LLImageGL::getTextureBytesAllocated()` (`budget_accounted`) of
  `2,947.87–3,066.48 MiB`;
- discard bias fixed at `4.0` in every sample;
- a reused `321.548 MiB` target pool with zero evictions.

The old controller consequently treated the total device allocation as roughly
62–66 percent over the `2,969.6 MiB` streamed-image target even though it was
only about eight percent of the full `59,392 MiB` device recommendation. The
new controller instead sees texture pressure near the actual target and no
global-device overage. This runtime proves the pre-fix mechanism was active; it
does not substitute for a fresh post-fix run.

The raw local evidence remains in
`artifacts/metal/qa-runs/candidate-resource-churn-68532-20261108-01/viewer.log`
and `leap-latest.json`. Those files include private login/session data and must
not be uploaded.

## Exact-original contract and conversion history

Exact original commit `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`
computes pressure as tracked texture bytes plus the deliberately 2x-biased
vertex estimate and compares that estimate with `gGLManager.mVRAM` after the
texture divisor. Both sides describe the same dedicated/video-memory pool. The
exact-original `indra/newview/llviewertexture.cpp` blob SHA-256 is
`27b94a757e9cdbba99d3a5b2b07d6ab592455c4546e5191b6bad5c323b806c61`.

Conversion commit `ab98aae4a9` introduced the mismatched Metal maximum. The
pre/post source SHA-256 values for `llviewertexture.cpp` are respectively
`4d0a16e81375fda4ffd43d700d96e05317f34b82d7df1fc47d45af77c0ac7303`
and `7c633f7a42a1b6366674831a1b9972bc2a5b4962cc4df8c2355df98b7b0be8cd`.

Apple's Metal headers describe `recommendedMaxWorkingSetSize` as an
approximation of the total texture, buffer and heap allocation for good
performance, and `currentAllocatedSize` as the current size of all resources
allocated by the device. They form a coherent global pair, not a global-usage
versus streamed-image-target pair.

## Repair

Commit `7cd1993d8ed7a3ba04beb200e7dc007ecae19e0c` (`Separate Metal texture and
global memory pressure`) introduces explicit backend-specific accounting:

- Metal compares `LLImageGL::getTextureBytesAllocated()` with the existing
  bounded streamed-image target.
- Metal separately compares `MTLDevice.currentAllocatedSize` with the full
  recommended working-set budget exposed by `ALRenderCapabilities`.
- The effective pressure is the worse of the two only when a real global
  device overage exists; system-memory pressure remains an independent
  emergency signal.
- OpenGL retains the exact-original tracked texture plus biased vertex estimate.
- The 2.90-GiB streamed-image policy remains intact; textures were not granted
  the full 58-GiB device allowance.

The new pure helper and tests live in
`indra/newview/altexturememoryaccounting.h` and
`indra/newview/tests/altexturememoryaccounting_test.cpp`. Source SHA-256 values
are `a84c61b63318a8d7132d95e47b1db15a3dd3169ef7c793daefb75d91cea0e778`
and `bc7cc49cdc8e7cef458264a4dde29ce41c88dc3c1bc3cb8861082ef0272ca433`.

## Validation

- Focused `newview_tests '[texture][pressure]'`: 22 assertions / 6 cases pass.
- Full `newview_tests`: 99,012 assertions / 752 cases; 751 pass and the known
  CRT case is skipped.
- `tools/metal/build.sh build`: passes warnings-as-errors build/staging.
- `tools/metal/verify_app.sh`: 349/349 inspected slices are ARM64.
- Deep strict code-sign verification: passes.
- Source-exact Release `26.4.0.68535` executable SHA-256:
  `160852c8679a3d26cd0cd31d0d833dec0f9b445820739223a3713c22faf97286`.
- Packaged metallib SHA-256 (unchanged):
  `4f4778e6d6558654b52986ac4caa69b42add1ec1fe940bed331844a9f1ad97bb`.

The old `68532` process remains alive at the owner's request even though its
bundle path has since been rebuilt in place as `68535`; it cannot validate the
new code. The next owner-attended action is a fresh `68535` Ultra launch using a
dedicated profile, followed by settled `budget_accounted`, driver-allocation and
discard-bias sampling. If cache-churn attribution is required, add fetched-image
decode/upload/downscale counters rather than inferring it from generic Metal
wrapper churn. Under a genuine future global overcommit, reusable-target/cache
trimming before visible-image degradation remains a separate policy change.
