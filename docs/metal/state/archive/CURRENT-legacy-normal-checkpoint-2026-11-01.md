# Archived engineering state — legacy normal-data checkpoint

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer checkpoint:** `7b27e5d62e`

This record preserves the superseded recovery snapshot facts that preceded the
controlled authored-mirror comparison. The live recovery state is
`docs/metal/state/CURRENT.md`.

## Implementation evidence

- `80924cdae2` preserves raw tangent-space material normal maps in opaque,
  lit-alpha and impostor-alpha native owners.
- `7b27e5d62e` preserves raw standard/generated legacy bump maps in the native
  G-buffer owner, matching `LLDrawPoolBump::bindBumpMap`; colour textures retain
  colour decoding.
- The production-boundary negative control held bump bytes constant and changed
  only the storage declaration. Before the correction, an sRGB-backed standard
  bump asset changed opaque G-buffer half channels from the raw result:
  `56820 != 65280` and `15337 != 32895`.
- Corrected focused RelWithDebInfo validation passed 1,322 assertions. Complete
  Debug and RelWithDebInfo physical suites each passed 44,865 assertions across
  50 cases with Metal API, GPU and shader validation enabled.

## Build and neutral-gate evidence

- Homebrew Bison 3.8.2 was selected in the Ninja cache. The build-local Python
  3.14 environment supplied repository-declared `llsd 1.2.4` and `PyYAML 6.0.3`,
  enabling the LL script compiler and its generated test inputs.
- The neutral non-manual/non-GL CTest gate passed 484/484 tests.
- Warnings-as-errors RelWithDebInfo build 68012 completed, repeated with
  `ninja: no work to do`, passed deep signing and verified 349/349 ARM64 slices.
- Executable SHA-256 was
  `de6165c5a7a79236f13a8828414987b96b6e8ae431581df99628b8c37e936cd0`.
- At this archived boundary no controlled live viewer run had yet been counted
  as parity evidence.

## Open release work at the boundary

M-008 remained active. Advanced-family parity, the full feature and stability
matrix, strict viewer-owned OpenGL elimination, independent build/review
evidence and paired quality-equivalent M1 Max performance measurements remained
open. No product or release gate was claimed complete.
