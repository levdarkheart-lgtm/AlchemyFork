# Archived engineering state — unmatched authored-mirror checkpoint

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Renderer checkpoint:** `7b27e5d62e`

## Erratum

The live recovery state previously reported an authored-mirror residual using
cross-backend SSIM/PSNR scores from profiles that were assumed equivalent. A
later effective-settings audit proved the assumption false: 25 of 386 captured
`Render*` settings differed, including quality `3` versus `0`, mirror resolution
`512` versus `256`, anisotropy, texture resolution, LODs, local-light count and
probe update settings. The old image scores below are historical diagnostic
output only and must not be used as parity evidence or to justify renderer work.

## Superseded observation

The invalid pair had been described as improving an upper reflected band from
SSIM `0.710348` / PSNR `12.456081 dB` to `0.806706` / `17.472924 dB`, and a
mirror-interior crop from `0.859180` / `17.934595 dB` to `0.874149` /
`22.417276 dB`, while retaining an apparent blurred-floor residual. That
interpretation is withdrawn.

## Corrective boundary

The pair was repeated through actual Computer Use with identical effective
settings. The corrected evidence and current product assessment are in
`docs/metal/state/CURRENT.md`. The native data-contract fixes, build 68012,
physical suites, signing and ARM64 verification remain valid independent
evidence; only the unmatched image comparison and its inferred next priority
are superseded.
