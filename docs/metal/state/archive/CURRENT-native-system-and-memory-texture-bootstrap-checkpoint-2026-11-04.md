# Native system and in-memory texture bootstrap checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `3984366670` (`Publish in-memory fetched textures natively`),
including `8a4c9aee97` (`Publish system textures natively without GL`)

## Replacement-first boundary

When the GL manager is disabled on Darwin, the viewer now publishes its null,
black, default, checker and other raw local textures directly to native Metal
storage. The default fetched placeholder also uses direct Metal publication.
The resources retain their established dimensions, component expansion, exact
pixels, mip chains, categories, logical texture-list ownership and teardown
accounting without creating a GL name or backing allocation.

Decoded in-memory fetched textures now enter through the same manager factory.
This gives glTF/data-URI and other memory-backed local images native Metal
publication when GL is disabled while retaining UUID, preview boost, discard
and texture-list behavior.

The active fail-closed renderer still requires exact GL recovery pixels if a
Metal presentation cannot commit. Therefore both factories deliberately retain
their established dual-backed construction while GL is active. Removing that
recovery owner now would replace black/default/checker pixels with the white
sentinel on a fallback frame. No feature or recovery behavior was deleted to
lower the OpenGL inventory; later GL removal remains conditional on complete
native startup and presentation coverage.

## Exact build and validation evidence

- The focused physical system-texture case passed 53 assertions under Metal API
  validation. It proves exact null, black, default and checker RGBA pixels,
  dimensions, categories, mip counts, native residency, zero GL backing and
  allocation teardown.
- Full `llrender_gl_fresh_tests` passed 3,363 assertions across 105 cases and
  full `almetal_tests` passed 52,526 assertions across 56 cases under Metal API
  validation. Full `newview_tests` passed 98,990 assertions: 746 passed and the
  established non-reference CRT generator case skipped, 747 total.
- Ninja RelWithDebInfo compiled and linked the test targets and complete viewer
  warnings-as-errors, then staged `alchemy-bin`.
- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- `tools/metal/build.sh` exited 0 at committed source `3984366670`. The
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development build `26.4.0d68258` (`26.4.0.68258`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  deep strict code-sign verification passed.
- Viewer executable SHA-256:
  `4466b1a298175e3ebc6e4624b72e14ffab77b79037835af00333545d11f657c4`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory scans 2,407 files and reports 3,220 findings / 2,909 unique
  path/snippet records, expected exit 1. The count is unchanged because active
  recovery was intentionally retained; no audit rule or suppression changed.
  Exact report SHA-256:
  `65c7ca71d1e23307df6e031bf38932e309c52c331ec246d71b70a8ac9de70124`.
- Both OpenGL and Metal remain linked. Static, linked and runtime no-GL release
  gates remain open. No viewer was launched, driven, photographed or compared.

## Complete-product effect and next boundary

This advances M-004 and M-011 because concrete startup and decoded-memory
resource producers retain their behavior through native publication when GL is
disabled. It does not claim that all startup textures are native-ready or that
GL recovery can yet be retired.

Next trace local-file preload readiness (`white.tga`, missing asset, flat normal,
default irradiance and the UI image list) and the transition from fetched-queue
`setGLTextureCreated(true)` state to native-only publication. Convert the next
producer only after its consumers and failure behavior are accounted for.
