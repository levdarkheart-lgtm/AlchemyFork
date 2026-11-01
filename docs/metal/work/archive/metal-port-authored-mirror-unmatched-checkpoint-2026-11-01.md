# Archived execution checkpoint — unmatched authored-mirror comparison

**Renderer checkpoint:** `7b27e5d62e`
**Superseded:** 2026-11-01 by the matched-settings checkpoint in the active
ledger.

## Erratum and decision

This checkpoint directed work toward hero-probe LOD, mip and sampling analysis
after an apparent reflected-floor blur. The comparison was invalid: 25 of 386
effective visual settings differed across the reference and candidate profiles,
including the quality preset and mirror resolution. Its SSIM/PSNR values and
the inferred mirror-blur priority are retained only to explain the decision
history; they are not product evidence.

Actual Computer Use repeated the pair at identical quality `3`, mirrors enabled
and mirror resolution `512`. Post-run manifests matched all 386 settings and
the mirror-interior result was SSIM `0.991375` / PSNR `41.100904 dB`, with no
material residual under visual and amplified-difference inspection. The active
ledger therefore returns to the broader M-008 matrix instead of continuing the
unsupported blur hypothesis.
