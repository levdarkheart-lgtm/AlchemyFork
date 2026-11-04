# Native Metal texture-eviction checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `0789086de1` (`Make texture eviction Metal-aware`)

## Replacement-first boundary

The viewer-facing texture lifecycle now names and performs the actual operation:
`LLGLTexture::destroyGPUTexture()` releases owned GPU residency whether GL or
Metal is authoritative. Its former wrapper was gated on a live GL name, so a
native-only texture silently retained Metal storage and accounting.

Sculpt-only conversion now recognizes GPU residency and can release a Metal-only
image. Normal fetched eviction and fetched-result clearing use the same
contract. Native brightness/darkness bump cleanup also releases the temporary
Metal images after their recent-use window. The non-Darwin media recreation
call retains its equivalent GL behavior through the neutral API.

The underlying `LLImageGL` owner already releases its Metal and GL stores,
resets discard/creation metadata, invalidates pending analysis and reconciles
the corresponding process-wide charge. Direct low-level
`destroyGLTexture()` calls remain for that owner's explicit detach/lifecycle
implementation. No feature, raw-image behavior or transitional recovery path
was deleted.

## Exact build and validation evidence

- The first broad Ninja compile found two legacy-bump wrapper callers omitted
  from the initial source edit. They were migrated to the same full-residency
  contract; the resumed cold Release warnings-as-errors build then compiled
  `llrender_gl_fresh_tests`, linked the full viewer and packaged `alchemy-bin`.
- With Metal API/GPU validation enabled, the focused physical
  `[LLImageGL][almetal]` slice passed 1,972 assertions across 20 cases; the
  complete renderer binary passed 2,683 assertions across 100 cases.
- The new physical case creates only Metal backing through `LLGLTexture`, then
  proves destruction clears GPU/GL residency, the native pointer, discard
  level, wrapper state and the exact process-wide byte charge.
- `tools/metal/build.sh` exited 0 at committed source `0789086de1`. The cold
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68241` (`26.4.0.68241`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `ce2bf076c43f68e5ad0218602ba775d921652d3aea4f8a701e88ee6a37a4a28a`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory scanned 2,406 files and reported 3,220 findings / 2,909
  unique path/kind/snippet records, expected exit 1. No audit rule or
  suppression changed; the count is informational, not feature evidence.
  Report SHA-256:
  `0fd3f423a7552de947b09fcad4685f9e6ac650a2715c1965c96675e97af3d9b5`.
- Both OpenGL and Metal remain linked. Static, linked and runtime no-GL release
  gates remain open.
- No viewer executable was launched, driven, photographed or compared.

## Next executable boundary

Move native downscale queue draining ahead of the global GL-disabled return.
Already-native pressure work must remain operational without a live GL manager,
while upload and GL/dual-backed branches remain fail-closed until their native
replacements are complete. Add deterministic scheduler coverage.
