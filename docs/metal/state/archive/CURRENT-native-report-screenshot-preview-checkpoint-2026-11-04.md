# Native report-screenshot preview checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `85036920be` (`Publish report screenshot previews natively`)

## Replacement-first boundary

The abuse-report floater continues to capture the current frame through the
existing native snapshot path, optionally save the previous screenshot as PNG,
convert the captured RGB pixels to JPEG2000, create the upload transaction and
asset identity, write the cache file, populate the texture picker and execute
the established upload callbacks. Only its preview backing changed: the same
post-conversion RGB image now publishes to an authoritative private Metal
texture with generated mips instead of allocating GL storage.

The native publication is checked. Failure produces a Metal-scoped warning
that includes the generated asset UUID rather than silently presenting stale
preview pixels. No abuse-report, screenshot, upload, cache or UI behavior was
removed to reduce an OpenGL count.

## Exact build and validation evidence

- The focused physical case passed 529 assertions in one case under Metal API
  validation. It proves 16x8 RGB expansion to exact RGBA8-sRGB bytes, five
  generated mip levels, backend-neutral residency, no GL name, exact native
  memory accounting and cleanup to the process baseline.
- The complete physical `llrender_gl_fresh_tests` binary passed 3,310
  assertions across 104 cases under Metal API validation.
- The full `newview_tests` binary passed 98,990 assertions: 746 passed and the
  established non-reference CRT generator case skipped, 747 total.
- Ninja RelWithDebInfo compiled the changed production/test files and complete
  viewer warnings-as-errors, linked and staged `alchemy-bin`.
- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- `tools/metal/build.sh` exited 0 at committed source `85036920be`. The
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development build `26.4.0d68253` (`26.4.0.68253`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  deep strict code-sign verification passed.
- Viewer executable SHA-256:
  `d4946b03c64941ab7ffba22c6b36be248e3dd9a1d3f3de503fb53067b7d20811`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory remains 2,407 files / 3,224 findings / 2,912 semantic
  unique records, expected exit 1. The removed member call is outside current
  patterns and the report is byte-identical to the preceding audit at
  `735d09412d60378f472f686024133b62968a87d163623434a9173c2d905ebf68`.
- Both OpenGL and Metal remain linked. Static, linked and runtime no-GL release
  gates remain open. No viewer was launched, driven, photographed or compared.

## Complete-product effect and next boundary

This advances M-004 and M-011 because a real user-facing report workflow keeps
its asset and UI semantics while its preview resource becomes Metal-native.

The next bounded owner is local terrain-paint baking. Its Metal renderer and
physical pixel/orientation/mip coverage already exist, but production still
runs a GL bake first. Make Metal authoritative only after preserving region
geometry, alpha-ramp weighting, bit-depth quantization and failure semantics.
