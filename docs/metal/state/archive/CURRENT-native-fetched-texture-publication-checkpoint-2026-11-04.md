# GL-disabled native fetched-texture publication checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `2477a6aa28` (`Publish fetched textures natively without GL`)

## Replacement-first boundary

Ordinary fetched images now retain their upload and publication behavior after
the global GL manager is disabled. On Darwin the main-thread create queue
builds a staged `MTLTexture`, publishes it only after successful command-buffer
submission, and then retires any lower-resolution transitional GL generation.
The active-GL route remains unchanged while complete per-frame GL recovery is
still required during conversion.

`LLImageGL` now treats replacement as a transaction. The published geometry,
discard, component/format, alpha-stride and offset, alpha/pick policy, mip,
category, GL-created, allocation and memory-accounting state remain visible
while the pending Metal generation is prepared. Failure cancels the upload and
restores that complete view; success publishes the pending Metal resource,
alpha data and accounting together. A pending generation is never charged as a
second logical residency allocation.

The GL-disabled scheduler keeps fetched upload work on the main thread instead
of posting it to the GL worker. It processes that work before the historical
GL-disabled return. Native allocation failure retains decoded raw pixels and
requeues the texture for retry rather than silently finalizing a missing GPU
resource. This retry change is scoped to Darwin with GL disabled; active-GL
behavior and unsupported residual consumers remain fail closed.

## Exact build and validation evidence

- The focused staged-publication regression passed 31 assertions in one case.
  It starts with an 8x8 GL/Metal generation, forces a failed RGB native
  replacement, proves complete rollback, then publishes a successful 16x16
  RGBA replacement and verifies GL retirement, mip/mask state, accounting,
  readback and teardown.
- The focused physical Metal renderer slice passed 2,003 assertions across 21
  cases. The complete `llrender_gl_fresh_tests` binary passed 2,714 assertions
  across 101 cases under Metal validation.
- The full `newview_tests` binary passed 98,990 assertions: 746 passed and the
  established non-reference CRT generator case skipped, 747 total.
- Ninja Release compiled the changed production and test sources with
  warnings-as-errors, linked the complete viewer and packaged `alchemy-bin`.
- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- `tools/metal/build.sh` exited 0 at committed renderer source `2477a6aa28`.
  The warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and
  staged Alchemy Metal Development `26.4.0d68245` (`26.4.0.68245`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `75f9555af46d2aed423120dfa21b2f73178358101cb80c08cdfde4e27baa1e46`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  `ALMetalWorldPresentation` remains default-on at `1`.
- The documented `audit_gl.py --strict` alias is restored and covered by a
  regression test; all 30 harness/tool tests pass. Strict inventory scans
  2,407 files and reports 3,223 findings / 2,911 unique path/kinds/snippet
  records, expected exit 1. Report SHA-256:
  `c75a0f209d31b4d21651e49a9b585d7f99e8be84828a8878b9d4b298dcb7343c`.
- The inventory increase is exactly the new `cancelUpload` and
  `publishMetalTextureAuthoritative` method declarations in the legacy-named
  `LLImageGL` class. No new GL API call was added; the name-based audit records
  three findings/two unique records. Counts are not used as feature evidence.
- Both OpenGL and Metal remain linked. Static, linked and runtime no-GL release
  gates remain open.
- No viewer executable was launched, driven, photographed or compared.

## Complete-product effect and next boundary

This advances M-004 and M-011 because a real application texture producer now
keeps format, mip, alpha/pick, retry, publication and residency behavior while
running without a GL manager; no feature was deleted to reduce the audit.

The next producer boundary is sculpt-only fetched data. Its
`addToCreateTexture()` branch still calls `createGLTexture(..., false)` only to
publish metadata. Replace that misleading GL-owned metadata path with an
explicit backend-neutral operation while preserving raw-image teardown, sculpt
LOD invalidation and the absence of a GPU allocation.
