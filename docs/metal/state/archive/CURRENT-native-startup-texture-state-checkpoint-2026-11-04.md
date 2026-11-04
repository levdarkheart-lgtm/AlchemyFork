# Native startup-texture state checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `9cd0f05401` (`Keep native startup texture requests
GL-free`)

## Replacement-first boundary

Local-file and UUID fetched-image requests no longer claim a GL-created
texture on Darwin when the GL manager is disabled or was never initialized.
That bit is legacy GL restoration state, not logical image existence. Decoded
pixels still flow through the fetched create queue and publish to the existing
native Metal resource path; the active transition renderer keeps the old bit
while GL is initialized so fail-closed recovery behavior is unchanged.

The exact shipped startup assets `missing_asset.tga`, `white.tga`,
`flatnormal.tga`, `default_irradiance.png` and `alpha_gradient.tga` were decoded
from the repository and physically uploaded on the M1 Max. Native readback
matches every decoded base-level byte, including opaque RGB expansion and the
alpha-only R8/swizzle contract. Dimensions, mip counts, boost categories,
zero-GL backing, false GL-created state and allocation teardown are covered.

No feature was removed and the remaining preload/UI images continue through
the same fetched decode/publication lifecycle. Their complete startup-readiness
trace remains the next boundary before any live GL context removal.

## Exact build and validation evidence

- Focused `[startup-textures]` physical coverage passed 107 assertions under
  Metal API/load-action validation. Full `llrender_gl_fresh_tests` passed 3,470
  assertions across 106 cases; full `almetal_tests` passed 52,526/56 under the
  same validation.
- Full `newview_tests` passed 98,990 assertions: 746 cases passed and the
  established non-reference CRT generator case skipped, 747 total.
- Ninja RelWithDebInfo reported the changed tests and complete viewer targets
  up to date after their warnings-as-errors build/link.
- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- `tools/metal/build.sh` exited 0 at committed source `9cd0f05401`. The
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68260` (`26.4.0.68260`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `e2a40e6d1c57ca0ca8ab02361586752db9aadc0cb4b7127fc8ce4a48e347a9ab`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory scans 2,407 files and reports 3,220 findings / 2,909 unique
  path/rule/snippet records, expected exit 1. No audit rule or suppression
  changed. Report SHA-256:
  `2cb09cfa75c4dfd25091ee5a99c87975be86a5b2b6672fbdaf68babe12d97398`.
- `otool -L` still reports both OpenGL and Metal. Static, linked and runtime
  no-GL release gates remain open. No viewer was launched, driven,
  photographed or compared.

## Complete-product effect and next boundary

This advances M-004 and M-011 because startup requests now expose truthful
native-only state while preserving decoded assets and active-GL recovery. It
does not prove every UI/preload texture is ready without GL or authorize
removing the live context.

Next trace the remaining preload assets and `LLUIImageList` callback/clip/scale
readiness through the GL-disabled main-thread create queue. Add deterministic
coverage for the complete declared lifecycle, then cut the next GL owner only
after its consumers and failure behavior have a native replacement.
