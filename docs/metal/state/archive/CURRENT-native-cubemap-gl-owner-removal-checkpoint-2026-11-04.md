# Native cubemap GL-owner removal checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `f86d87a100` (`Remove superseded GL cubemap storage`)

## Replacement-first boundary

`LLCubeMap` already had an authoritative native Metal cube on Darwin. Its live
world consumers obtain that cube through `getMetalTexture()`, while the Darwin
legacy `initGL()` implementation was inert and the legacy texture-slot binding
had no face to bind, cleared stale state and returned false. Commit
`f86d87a100` therefore removes only the superseded GL allocator, per-face
`LLImageGL` storage and GL-name API after proving the native replacement.

The existing CPU face transform remains authoritative before native upload.
Physical coverage checks the transformed bytes of all six faces, the cube
descriptor, native readback, device-loss invalidation and retry generation. The
product is macOS/Apple-Silicon-only, so unused non-Darwin GL cube construction
is not a retained product feature. No sampling, upload, readback or retry
behavior was deleted.

`LLCubeMapArray` is separate. It still owns live reflection/hero-probe GL array
storage and is not authorized for deletion by this checkpoint. Its complete
Metal allocation, write/copy, binding, consumer and failure lifecycle must be
implemented and validated first.

## Source trace before selection

The preceding UI-texture trace found no hidden GL requirement at the bounded
callback boundary: `LLUIImageList::initFromFile` merges declarations and
preloads; `onUIImageLoaded` uses decoded, full and original dimensions; and the
callback uses backend-neutral discard level. A failed native create retains raw
data and pending state. That trace did not justify deleting a live fallback
texture owner, so the cubemap boundary was selected instead because its Metal
replacement and inert target-side GL route were already provable.

## Exact build and validation evidence

- Focused physical `[LLCubeMap]` coverage passed 24 assertions under Metal API
  and load-action validation. Full `llrender_gl_fresh_tests` passed 3,594
  assertions/106 cases under Metal API validation.
- Full `almetal_tests` passed 52,526 assertions/56 cases under Metal API
  validation. Full `newview_tests` passed 98,990 assertions: 746 cases passed
  and the established non-reference CRT generator case skipped, 747 total.
- Ninja RelWithDebInfo rebuilt and linked `llrender_gl_fresh_tests`,
  `almetal_tests` and the complete `alchemy-bin` warnings-as-errors target.
- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- `tools/metal/build.sh` exited 0 at committed source `f86d87a100`. The exact
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68264` (`26.4.0.68264`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `b9393f820e7dfa25a27f09094a06e4e41ba5fb983c035a254773a6b600fe36db`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory scans 2,407 files and reports 3,214 findings / 2,903 unique
  path/rule/snippet records, six fewer than the preceding checkpoint, expected
  exit 1. No audit rule or suppression changed. Exact report:
  `artifacts/metal/audit-gl-f86d87a100.json`; SHA-256
  `e431bee7369b77e9bc3ed693d0a5cf1133a93870761651f11307d5382723706b`.
- The remaining `llcubemap.cpp` audit hit is the name-based
  `LLImageGL::getMetalDevice`, not a GL call. Actual cube-array GL ownership
  remains visible in `LLCubeMapArray` and is an open implementation boundary.
- `otool -L` still reports both OpenGL and Metal. Static, linked and runtime
  no-GL release gates remain open. No viewer was launched, driven,
  photographed or compared.

## Complete-product effect and next boundary

This advances M-004 and M-011 because a fully replaced resource no longer
retains duplicate GL authority. The six-finding audit reduction corroborates
that source change but is not its purpose or a release claim.

Next trace `LLCubeMapArray` reflection/hero-probe allocation, write/copy,
binding and consumption, including the existing Metal mirror/fill paths. Add
deterministic lifecycle coverage, implement complete native authority, and only
then retire the corresponding live GL owner. Any visual question goes to the
owner-attended daily queue; Codex does not drive comparison viewers.
