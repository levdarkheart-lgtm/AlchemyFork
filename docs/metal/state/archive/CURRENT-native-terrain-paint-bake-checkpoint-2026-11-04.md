# Metal-authoritative terrain-paint bake checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `aaf3bbcd2a` (`Make terrain paint baking Metal authoritative`)

## Replacement-first boundary

The advanced local-terrain-paint command remains functional. It creates a
linear RGB8-style native destination, evaluates every grid point in every
region patch, builds the same two-triangle cells with 32-bit indices, samples
the preloaded two-dimensional alpha ramp, applies the configured 1–8-bit paint
weight precision, generates the complete mip chain and publishes the result to
the established local terrain-material owner. Creation failure still installs
the existing blank/null paint map, and the delete command still disables and
clears local paint.

Metal is now the production owner rather than a mirror. CPU geometry is written
directly to short-lived shared Metal buffers, the established native terrain
shader renders a separate target, and authoritative adoption occurs only after
successful encoding and command submission. The GL scratch render target,
shared GL matrix/viewport mutation, PBR terrain-bake GLSL draw, framebuffer
copy and GL mip generation are removed. No terrain feature was removed to
lower the OpenGL inventory.

## Exact build and validation evidence

- The focused physical Metal terrain-bake case passed 289 assertions under
  Metal API validation. It proves indexed viewer-style buffer offsets, exact
  quantized RGBA pixels, GL-compatible vertical storage orientation and the
  final generated mip.
- Full `almetal_tests` passed 52,526 assertions across 56 cases under Metal API
  validation. Full `newview_tests` passed 98,990 assertions: 746 passed and the
  established non-reference CRT generator case skipped, 747 total.
- Ninja RelWithDebInfo compiled the changed Metal/newview production files and
  complete viewer warnings-as-errors, linked and staged `alchemy-bin`.
- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- `tools/metal/build.sh` exited 0 at committed source `aaf3bbcd2a`. The
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development build `26.4.0d68255` (`26.4.0.68255`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  deep strict code-sign verification passed.
- Viewer executable SHA-256:
  `3090d9ea9eff67d705a30826c2f78ef5603b4b6d5537b5a7ba21998f241a8ef9`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory scans 2,407 files and reports 3,220 findings / 2,909 unique
  path/kinds/snippet records, expected exit 1. This is four findings and three
  unique records below the prior checkpoint; no audit rule or suppression
  changed. Exact report SHA-256:
  `36dd346df62e10ec6d19eeecee4b2e31f56e01b2cc59fd27ae23ac952dad2f9a`.
- Both OpenGL and Metal remain linked. Static, linked and runtime no-GL release
  gates remain open. No viewer was launched, driven, photographed or compared.

## Complete-product effect and next boundary

This advances M-007 and M-011 because a real terrain-authoring feature now
retains its production behavior through an authoritative native Metal pass,
then retires the duplicate GL owner.

The next live texture candidate is default/fetched bootstrap publication.
Trace its default-image, missing-asset, transitional fallback, world and UI
consumers before changing authority; the cutover must preserve those behaviors
rather than merely remove its remaining direct GL allocation.
