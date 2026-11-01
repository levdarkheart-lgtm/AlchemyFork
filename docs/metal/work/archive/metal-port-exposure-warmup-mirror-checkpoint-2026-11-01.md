# Exposure warm-up and authored-mirror execution record — 2026-11-01

**Renderer checkpoint:** `d3cff63d0d`

The next M-008 acceptance replay exposed a timing-dependent exposure ownership
gap. On a cold isolated profile, GL adapted before the asynchronously created
main-world Metal renderer became available. The native gate therefore remained
correctly fail-closed even though the authored-mirror diagnostic was requested.

The bounded implementation restarts both exposure owners from fixed EV 0.
Native history is invalidated immediately at renderer completion; the GL clear
is deferred to `generateExposure()` so it occurs at the safe ping-pong boundary
before that texture becomes previous-frame history. An asymmetric physical
control distinguishes restart from stale-history adaptation.

Validation passed the 63-assertion focused case, 44,875-assertion complete
physical suite, 484/484 neutral tests, warnings-as-errors build 68020, deep
signing and 349/349 ARM64 inspection. The executable hash is
`42717a577f42cfd5e553d8e09e4ece86dc88ad4e283aa49eedfb24d4cbe71763`.

Actual Computer Use then completed candidate and pinned OpenGL-reference runs
sequentially. Both held the same SLURL/camera, fixed Midday, 2048x1476 PNG
Snapshot settings and 416/416 effective visual settings before and after the
runs. Candidate logs prove the EV reset, scoped authored-mirror admission and
native readback without GL fallback. Both viewers exited semantically and no
viewer/media process remained.

The sequential avatar pose changed, so whole-frame metrics are descriptive.
The stable mirror interior reached SSIM `0.985163` / PSNR `34.480 dB` and direct
inspection found no material residual for this fixture. Exact provenance is
pinned for both intentionally different binaries. M-008 remains active; the
next boundary is the first demonstrated gap in the remaining post or alpha/PBR
graph, while broad parity, stability, strict no-GL and performance gates remain
open.
