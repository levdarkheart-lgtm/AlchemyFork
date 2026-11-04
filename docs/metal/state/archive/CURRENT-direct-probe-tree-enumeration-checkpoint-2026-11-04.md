# Direct native reflection-probe tree-enumeration checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `d4270ed56c09e9da5f77fa5d1a68458197442b61`
(`Enumerate probe tree faces without GL pools`)

## Replacement-first boundary

Tree geometry is owned by `LLDrawPoolTree` faces rather than the deferred CPU
render maps used by simple, material, PBR and bump batches. Commit `d4270ed56c`
extends the direct reflection-probe scene producer across that ownership
boundary. It enumerates live tree pools and submits their faces to the native
world-G-buffer capture before the retained GL face executes.

Native tree capture resolves the face's vertex/index buffer, source texture or
the established default-texture substitute, 0.5 alpha cutoff and drawable
region transform directly from CPU-owned state. Model-view, projection and
inverse-normal matrices are constructed from the region matrix and current
probe camera; there is no GL shader/texture bind, GL matrix readback or GL draw
in the direct producer. The existing source-texture statistics side effect and
`sRenderTextures` behavior remain intact.

Exact `LLFace` identities suppress only duplicate Metal capture when the GL
pool subsequently renders the complete transition face. An unbuilt face or a
missing drawable region, buffer, image or native resource invalidates the
native face; it does not skip or weaken the full GL fallback.

This is feature replacement, not OpenGL deletion. Terrain, sky, water, avatar,
indexed generic masks and post-deferred inputs remain incomplete for direct
probe production. `beginMetalReflectionProbeFaceCapture` still requires live
GL resources, `display_cube_face` still executes and the GL cube array remains.

## Exact build and validation evidence

- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- The changed `alchemy-bin` and `newview_tests` targets compiled and linked
  warnings-as-errors. Full `llrender_gl_fresh_tests` passed 3,594 assertions
  across 106 cases with Metal API/GPU validation enabled.
- Full `almetal_tests` passed 52,526 assertions across 56 cases with Metal
  API/GPU validation enabled. Full `newview_tests` passed 98,990 assertions:
  746 cases passed and the established non-reference CRT case skipped.
- `tools/metal/build.sh` exited 0 at renderer source `d4270ed56c`. The exact
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68274` (`26.4.0.68274`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `c337436a8375ebb1f817d8cd8cb8cadde0ef40d4d6f022272efad495a835f9a3`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory scans 2,407 files and reports 3,214 findings / 2,903 unique
  path/rule/snippet records, expected exit 1. No rule or suppression changed.
  Exact report `artifacts/metal/audit-gl-d4270ed56c.json` has SHA-256
  `ecb14bc141bce116e81981bfd5ce4258f14869f79a4c87a49d85937c7ab0b8a0`.
- `otool -L` still reports both OpenGL and Metal. Static, linked and runtime
  no-GL release gates remain open. No viewer was launched, driven,
  photographed or compared.

## Complete-product effect and next boundary

This advances M-007 and M-011 by replacing the probe tree feature's dependency
on GL pool execution with a direct Metal producer while preserving its texture,
cutout, transform and statistics behavior. The next source boundary is a direct
producer for pool-owned terrain faces, followed by explicit sky, water and
avatar inputs. Only complete native face production permits removal of the GL
resource gate, `display_cube_face` participation or the live GL cube array.
Visual probe parity remains queued for the owner-attended daily review.
