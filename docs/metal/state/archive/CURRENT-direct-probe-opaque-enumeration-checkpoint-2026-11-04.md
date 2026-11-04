# Direct native reflection-probe opaque-enumeration checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `9db17b5bdc87a78a2b6d093d31034e2d1569a48a`
(`Enumerate probe opaque batches without GL pools`)

## Replacement-first boundary

Reflection-probe Metal batches previously existed only because each GL draw
pool called a capture hook while rendering its own batch. Commit `9db17b5bdc`
adds a probe-only traversal of the authoritative CPU `LLCullResult` render maps
before GL pool execution. It directly constructs native batches for static and
rigged simple geometry, scalar alpha masks, grass, all twelve legacy material
families and their rigged variants, plus static/rigged opaque and alpha-masked
PBR geometry.

The traversal records exact `LLDrawInfo` identities. The subsequent GL pools
still render the complete fail-closed face, but their capture hooks do not add
the already enumerated native batch a second time. Missing draw data, Metal
buffers, textures, skin palettes or invalid material state invalidates the
entire native face; it does not suppress or weaken GL fallback.

`LLDrawPoolMaterials::DEFERRED_PASSES` is now the single description of legacy
material render-map and shader feature semantics. Scalar GL, indexed GL and
direct Metal traversal share its alpha, normal and specular mapping instead of
maintaining independent tables.

This is an opaque-enumeration cutover, not a completed probe producer. Indexed
generic alpha-mask batches and bump resolution still rely on their pool paths;
sky, terrain, water, tree, avatar and post-deferred families retain their
existing owners. `beginMetalReflectionProbeFaceCapture` still requires live GL
resources, `display_cube_face` still executes, and the GL cube array remains
until every required native producer, consumer and failure path is complete.

## Exact build and validation evidence

- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- The changed `alchemy-bin` and `newview_tests` targets compiled and linked
  warnings-as-errors. Full `llrender_gl_fresh_tests` passed 3,594 assertions
  across 106 cases with Metal API/GPU validation enabled.
- Full `almetal_tests` passed 52,526 assertions across 56 cases with Metal
  API/GPU validation enabled. Full `newview_tests` passed 98,990 assertions:
  746 cases passed and the established non-reference CRT case skipped.
- `tools/metal/build.sh` exited 0 at renderer source `9db17b5bdc`. The exact
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68270` (`26.4.0.68270`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `19859d121e5217d5b75edfe34b94239d039eb743193b98a06ce4804bce52c952`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory scans 2,407 files and reports 3,214 findings / 2,903 unique
  path/rule/snippet records, expected exit 1. No audit rule or suppression
  changed. Exact report `artifacts/metal/audit-gl-9db17b5bdc.json` has SHA-256
  `d47a8f65d2729470dc028a1caecf91ab0961503fe1052b470c9cf5ef8ee2968d`.
- `otool -L` still reports both OpenGL and Metal. Static, linked and runtime
  no-GL release gates remain open. No viewer was launched, driven,
  photographed or compared.

## Complete-product effect and next boundary

This advances M-007 and M-011: core reflection-probe opaque batches now have a
direct native producer independent of GL draw-pool execution. The next source
boundary is a backend-neutral bump-texture resolver and direct bump-map
enumeration, followed by explicit pool/face-owned sky, terrain, water, tree and
avatar inputs. Only complete native face production permits removal of the GL
resource gate, `display_cube_face` participation or the live GL cube array.
Visual probe parity remains queued for the owner-attended daily review.
