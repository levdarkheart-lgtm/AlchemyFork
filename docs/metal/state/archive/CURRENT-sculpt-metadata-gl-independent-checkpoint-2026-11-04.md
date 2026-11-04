# GL-independent sculpt metadata checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `c3d0a6c7ed` (`Publish sculpt metadata without GPU backing`)

## Replacement-first boundary

The sculpt-only fetched-image branch no longer calls
`createGLTexture(..., false)` to publish decoded dimensions and discard state.
`LLImageGL::setImageMetadata()` now performs that operation explicitly: it
validates the decoded image, resolves its logical full size and format, updates
discard/component state, and releases obsolete GL or Metal backing without
allocating either kind of GPU resource. `LLGLTexture` mirrors the resulting
full geometry, component count and texel count for viewer consumers.

This fixes behavior rather than merely deleting a GL-named call. The former
compatibility route returned before updating metadata when the GL manager was
disabled. Sculpt data could therefore lose its newly decoded dimensions and
discard level before the raw image was released. The explicit metadata route
works without a live GL manager and retains raw-image teardown plus the
existing sculpt LOD invalidation performed after publication.

`createGLTexture(..., false)` delegates to the same operation before its GL
manager checks, preserving any remaining compatibility caller while the named
owner is converted. Ordinary texture upload still requires a real Metal or GL
publication and is not weakened into metadata-only success.

## Exact build and validation evidence

- The physical focused regression passed 15 assertions in one case under
  Metal API validation. It starts from real Metal residency, disables the GL
  manager, publishes 4x2 RGB at discard 2, and proves logical 16x8 geometry,
  three components, no GL/Metal backing, zero memory, unchanged texture state
  and restored process accounting.
- The complete physical `llrender_gl_fresh_tests` binary passed 2,729
  assertions across 102 cases under Metal API validation.
- The full `newview_tests` binary passed 98,990 assertions: 746 passed and the
  established non-reference CRT generator case skipped, 747 total.
- Ninja Release compiled the changed renderer/viewer/test sources with
  warnings-as-errors, linked the complete viewer and packaged `alchemy-bin`.
- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- `tools/metal/build.sh` exited 0 at committed renderer source `c3d0a6c7ed`.
  The warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and
  staged Alchemy Metal Development build `26.4.0d68247` with short version
  `26.4.0.68247`.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `09f2f1d7c238f47a865f2968693083b484339c1330462a6a622de3a4b0815b64`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory scans 2,407 files and reports 3,224 findings / 2,912 unique
  path/kind/snippet records, expected exit 1. Report SHA-256:
  `b09dbd88c0c76799d27cd07873183d6b4456a5930e964207379cbbb3ba6a2fd8`.
  The sole new unique record is the backend-neutral `setImageMetadata` method
  in the legacy-named `LLImageGL` class, not a GL API call.
- Both OpenGL and Metal remain linked. Static, linked and runtime no-GL release
  gates remain open.
- No viewer executable was launched, driven, photographed or compared.

## Complete-product effect and next boundary

This advances M-004 and M-011 because sculpt metadata publication now works
after GL shutdown while retaining the feature's CPU data, dimensions, discard
state, LOD notification and no-GPU-allocation contract. No feature was removed
to reduce the audit.

The next bounded producer is HDRI EXR upload. The native full/split HDRI sky
consumers and RGBA16F sampling path already exist, but `load_exr()` still makes
a paired GL/Metal image. Convert that Darwin producer to Metal-only RGBA16F
publication with generated mips, exact float pixels, lifecycle accounting and
failure cleanup before retiring its GL allocation.
