# Matched mirror and native reference-still checkpoint — 2026-11-01

This append-only record preserves the evidence compacted out of
`docs/metal/state/CURRENT.md` after renderer checkpoint `011ede6cf9`.

## Matched authored-mirror correction

The first build-68012 cross-backend pair was invalid: 25 of 386 effective
`Render*` settings differed, including quality `3` versus `0` and mirror
resolution `512` versus `256`. Its metrics are superseded and are not parity
evidence.

Actual Computer Use repeated the fixed 2048x1476 view. Post-run manifests
matched 416/416 settings with zero mismatches: quality `3`, mirrors enabled and
mirror resolution `512`. The full pair scored SSIM `0.987409` / PSNR
`39.185075 dB`; mirror frame `0.988105` / `37.793470 dB`; mirror interior
`0.991375` / `41.100904 dB`. Visual and amplified-difference inspection found
no material mirror-region residual. This rejects the suspected large blur for
one fixture only; broader probe/mirror parity remains open.

Build 68012 passed warnings-as-errors linking, deep signing and 349/349 ARM64
checks. Its executable SHA-256 was
`de6165c5a7a79236f13a8828414987b96b6e8ae431581df99628b8c37e936cd0`.
Both sessions logged out semantically and left no viewer processes.

## Native Lightbox reference owner

Source audit found the final Metal presentation shader already implemented
reference wipe/side-by-side, but production forced an active reference to GL
because the still itself had no coherent native owner. Commit `011ede6cf9`
added an exact-format, exact-size GPU copy at the post-AA/CAS boundary. Pending
copies join the renderer transaction, are usable in the same batch and publish
persistently only after commit. Replacement, dimension/format mismatch,
allocation failure, explicit Clear and command-batch failure invalidate the
appropriate native state.

Focused physical coverage passed 1,850 assertions; the complete Metal suite
passed 44,871 assertions/50 cases. The neutral CTest gate passed 484/484 in
242.67 seconds. Warnings-as-errors build 68016 passed deep signing and 349/349
ARM64 checks; executable SHA-256 was
`ce435cf3030c93b179b24d5db08d03d5994c4e1b2b66ffb58000d06f29aec9d0`.

Exact-path Computer Use preflight attested build, hash, singleton process and
isolated profile. Semantic login opened Lightbox, Grab enabled a 0.50 wipe,
side-by-side visibly duplicated the frozen/live view, and Clear disabled the
reference controls. The original isolated-profile Look was restored. Confirmed
Quit produced `Goodbye!`; no viewer/media process remained. The log contained
no native reference-capture warning, Metal validation error or command-buffer
error. No credential values or account/world-bearing image was retained here.

No performance improvement, broad M-008 completion, feature parity or no-GL
claim follows from this checkpoint.
