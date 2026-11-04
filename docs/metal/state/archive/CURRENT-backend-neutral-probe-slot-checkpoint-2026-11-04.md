# Backend-neutral reflection-probe slot checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `eadfbc3e4f4ebadc923da7798801cce084ffbda1`
(`Decouple reflection probes from GL cube ownership`)

## Replacement-first boundary

Each `LLReflectionMap` formerly stored a shared pointer to the manager's GL
`LLCubeMapArray`, even though the object needs only a logical cube slot. Commit
`eadfbc3e4f` removes that false ownership and removes the unused cube-array
pointer and cube index from `LLViewerWindow::cubeSnapshot`. The map retains
`mCubeIndex` as a backend-neutral logical slot; `LLReflectionMapManager` is now
the sole backend-resource owner.

Native reflection-probe radiance and SH production already publish through the
Metal probe-array mirror into that slot. The live GL cube array still supports
the fail-closed GL renderer and GL probe-generation path. It is intentionally
not removed: `LLPipeline::beginMetalReflectionProbeFaceCapture` currently
requires `mGLResourcesLive`, and native capture mirrors the scene traversal
driven by `display_cube_face`. Removing that gate or resource before a complete
native scene producer exists would delete functionality.

No render output, scheduling policy or fallback behavior changes here. The
strict inventory remains 3,214 findings, confirming that this checkpoint
separates ownership without using source deletion to improve the metric.

## Exact build and validation evidence

- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- Ninja Development build 68265 rebuilt and linked the changed complete viewer
  and tests warnings-as-errors. Full `llrender_gl_fresh_tests` passed 3,594
  assertions/106 cases under Metal API validation.
- Full `almetal_tests` passed 52,526 assertions/56 cases under Metal API
  validation. Full `newview_tests` passed 98,990 assertions: 746 cases passed
  and the established non-reference CRT generator case skipped, 747 total.
- `tools/metal/build.sh` exited 0 at committed source `eadfbc3e4f`. The exact
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68266` (`26.4.0.68266`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `f939a3578617ee287bcc2fd33d067d726f4d99e590f4efcaf1d06ac3291c37d1`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory scans 2,407 files and reports 3,214 findings, expected exit
  1. No audit rule or suppression changed. Exact report:
  `artifacts/metal/audit-gl-eadfbc3e4f.json`; SHA-256
  `34006085589d7e409582047bf55cc3f3dc3efdd98068e59e759653b5a464d201`.
- `otool -L` still reports both OpenGL and Metal. Static, linked and runtime
  no-GL release gates remain open. No viewer was launched, driven,
  photographed or compared.

## Complete-product effect and next boundary

This advances M-004 and M-011 by making logical reflection-probe state
independent of GL resource identity while retaining both the native producer
and complete fallback. It does not claim GL cube-array retirement or parity.

Next implement a complete native reflection-probe scene producer independent
of `mGLResourcesLive` and `display_cube_face`, with deterministic failure and
lifecycle proof. Only after native producers, consumers and fallback semantics
are covered may the live GL cube-array owner be removed. Visual questions stay
in the owner-attended daily queue; Codex does not drive comparison viewers.
