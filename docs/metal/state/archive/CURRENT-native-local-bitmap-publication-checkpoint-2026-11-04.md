# Native-only local-bitmap publication checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `66e0a60931` (`Publish local bitmap previews natively`)

## Replacement-first boundary

Local bitmap refresh now publishes decoded BMP, TGA, JPEG, JPEG2000, PNG,
WebP and AVIF pixels directly into an authoritative private Metal texture with
the requested generated mip family. The producer no longer allocates a GL
texture. File modification tracking, decode retries, local tracking/world IDs,
texture-list ownership, prim/material/sculpt replacement, GLTF-material
tracking, wearable updates and avatar rebake requests remain unchanged.

The cutover also repairs two consumers that equated a missing GL name with a
missing image. Brightness/darkness bump geometry now obtains the current texel
size from backend-neutral GPU residency. Volume alpha picking recognizes a
native-only texture and continues to read the CPU pick mask that Metal
publication builds from the decoded alpha channel. Transitional GL traversal
can bind its established sentinel, while native world/UI capture consumes the
real Metal resource.

No local-preview, material, selection or avatar feature was removed to reduce
an OpenGL count. A failed native publication retains the existing warning and
texture-list/ID propagation boundary, now identifying the Metal failure.

## Exact build and validation evidence

- The focused physical case passed 17 assertions in one case under Metal API
  validation. It proves backend-neutral residency with no GL name, exact
  dimensions, a complete four-level mip chain, transparent CPU pick-mask
  behavior and both slow/fast transitional binding paths.
- The complete physical `llrender_gl_fresh_tests` binary passed 2,781
  assertions across 103 cases under Metal API validation.
- The full `newview_tests` binary passed 98,990 assertions: 746 passed and the
  established non-reference CRT generator case skipped, 747 total.
- Ninja RelWithDebInfo compiled the changed production/test files and complete
  viewer warnings-as-errors, linked and staged `alchemy-bin`.
- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- `tools/metal/build.sh` exited 0 at committed source `66e0a60931`. The
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development build `26.4.0d68251` with short version
  `26.4.0.68251`.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `026e1dc67006cf35d7ec997d67dd779daf9dcbff734dbb202702b3e09adc61d3`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory remains 2,407 files / 3,224 findings / 2,912 semantic
  unique records, expected exit 1. The removed member-call allocation is
  outside current patterns; semantic comparison to the preceding report is
  unchanged. Exact report SHA-256:
  `735d09412d60378f472f686024133b62968a87d163623434a9173c2d905ebf68`.
- Both OpenGL and Metal remain linked. Static, linked and runtime no-GL release
  gates remain open.
- No viewer executable was launched, driven, photographed or compared.

## Complete-product effect and next boundary

This advances M-004 and M-011 because local bitmap previews retain their real
user-facing decode, refresh, material, object and avatar behavior through
native Metal publication, while native-aware selection and bump consumers no
longer depend on a GL name.

The next bounded producer is the abuse-report screenshot preview. Replace its
GL publication with the established native path while preserving native
snapshot capture, JPEG2000 upload/cache data, generated asset identity,
texture-picker display and failure reporting.
