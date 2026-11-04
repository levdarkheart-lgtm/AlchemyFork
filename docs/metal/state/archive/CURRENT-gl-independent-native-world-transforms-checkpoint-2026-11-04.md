# GL-independent native world-transform checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `e618640417d3f940bf0893a759f1cf5fc7a67427`
(`Decouple native world transforms from GL state`)

## Replacement-first boundary

Metal world-batch construction previously copied model-view and projection
matrices from `gGL` after a GL draw pool had applied each `LLDrawInfo` model
matrix. That made the native producer depend on mutable GL traversal state even
though the authoritative draw model matrix and current camera matrices already
exist on the CPU.

Commit `e618640417d3` reconstructs the exact draw transform from
`LLDrawInfo::mModelMatrix` and `LLViewerCamera::getCurrent()`. Generic,
material, PBR, bump, water-exclusion and legacy-glow Metal batches now own
their model-view, inverse model-view and model-view-projection inputs without
reading the GL matrix stack. The multiplication order was checked against
`LLRenderPass::applyModelMatrix` and `LLRender::multMatrix`.

This does not bypass culling, draw pools, shader selection or GL fallback.
Those still drive enumeration, so the reflection-probe GL-resource gate and
live cube array remain. The change is a prerequisite for a direct native scene
enumerator; it does not remove or weaken a rendered feature.

## Exact build and validation evidence

- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- The changed `alchemy-bin` and `newview_tests` targets compiled and linked
  warnings-as-errors. Full `llrender_gl_fresh_tests` passed 3,594 assertions
  across 106 cases under Metal API validation.
- Full `almetal_tests` passed 52,526 assertions across 56 cases under Metal API
  validation. Full `newview_tests` passed 98,990 assertions: 746 cases passed
  and the established non-reference CRT generator case skipped, 747 total.
- `tools/metal/build.sh` exited 0 at renderer source `e618640417d3`. The exact
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68268` (`26.4.0.68268`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `4360751dc9d9c938ab76b11bb5fdf16556db83d21460cdb87dd7107f54f1f137`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory scans 2,407 files and reports 3,214 findings / 2,903 unique
  path/rule/snippet records, expected exit 1. No audit rule or suppression
  changed. Exact report `artifacts/metal/audit-gl-e618640417d3.json` has SHA-256
  `7851308f708501627c5200a84e18cb9890768a6303bc38c5a3240e91ed7bb0a2`.
- `otool -L` still reports both OpenGL and Metal. Static, linked and runtime
  no-GL release gates remain open. No viewer was launched, driven,
  photographed or compared.

## Complete-product effect and next boundary

This advances M-007 and M-011 because the core native world batches now have
an explicit CPU-owned transform contract suitable for invocation outside a GL
draw. A complete Metal reflection-probe producer still requires direct scene
culling/enumeration plus explicit sky, terrain, water, avatar and remaining
pass contracts. Implement and prove that producer before removing
`mGLResourcesLive`, `display_cube_face` participation or the live GL cube
array. Visual questions remain in the owner-attended daily queue.
