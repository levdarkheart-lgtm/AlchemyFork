# Native-only HDRI publication checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `5985911fab` (`Publish HDRI textures natively with exact accounting`)

## Replacement-first boundary

HDRI EXR loading now publishes its decoded RGBA32F values directly into an
RGBA16F private Metal texture with the complete generated mip family. The
producer no longer allocates a parallel GL texture. The existing full-HDRI and
split-HDRI Metal sky owners consume the same `LLImageGL` Metal resource and
retain exposure, rotation, split composition and failure handling; decode or
native publication failure still frees the TinyEXR buffer and clears the
published image.

This removes duplicate backing only after the feature's native consumers and
float/mip upload implementation exist. GL traversal can temporarily bind its
established native-only sentinel during conversion, while authoritative Metal
capture reads the real image. No HDRI feature, precision mode or mip behavior
was deleted to reduce an OpenGL count.

The new physical regression exposed a second correctness defect before the
cutover was accepted. The native allocation and process-wide residency charge
were 344 bytes for an 8x4 RGBA16F mip family, but per-texture memory reported
168 bytes using legacy source-channel geometry and omitted the final 1x1 mip.
All native-authoritative create, staged publication, render-target adoption and
scale-down boundaries now set per-texture memory from the actual Metal
descriptor, matching the already descriptor-based process charge.

## Exact build and validation evidence

- The first focused physical run correctly failed one of 47 assertions:
  `getTextureMemory()` reported 168 B instead of the 344 B descriptor storage.
  After the production repair, the focused case passed all 47 assertions under
  Metal API validation.
- The regression proves RGBA32F-to-RGBA16F conversion with exact representable
  values, all four mip levels including the 1x1 tail, no GL name/backing,
  actual per-image/process accounting and teardown to the starting baseline.
- The complete physical `llrender_gl_fresh_tests` binary passed 2,776
  assertions across 103 cases under Metal API validation.
- The full `newview_tests` binary passed 98,990 assertions: 746 passed and the
  established non-reference CRT generator case skipped, 747 total.
- Ninja RelWithDebInfo compiled the complete viewer and changed tests with
  warnings-as-errors, linked and staged `alchemy-bin`.
- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- `tools/metal/build.sh` exited 0 at committed source `5985911fab`. The
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development build `26.4.0d68249` with short version
  `26.4.0.68249`.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `e5ca4a9eaca8537e48d2b33d9fe3cbaff0cbfd11e525e4d0ce5dd33ac978e2b7`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory remains 2,407 files / 3,224 findings / 2,912 unique records,
  expected exit 1; the member-call cutover is outside its current patterns.
  Exact report SHA-256:
  `fc9eaa4069c11f04e72ca54c7f18f16214e3887a6f11e577a89b6a047ac5256e`.
- Both OpenGL and Metal remain linked. Static, linked and runtime no-GL release
  gates remain open.
- No viewer executable was launched, driven, photographed or compared.

## Complete-product effect and next boundary

This advances M-004 and M-011 because HDRI loading retains its real native sky
feature while shedding duplicate GL residency, and native memory reporting now
matches actual Metal allocation for every authoritative lifecycle boundary.

The next direct producer is local-bitmap refresh. Replace its GL upload with a
native Metal publication while preserving file decode/retry, generated mips,
world-ID replacement, material and avatar rebake propagation, texture-list
ownership, failure reporting and actual residency accounting.
