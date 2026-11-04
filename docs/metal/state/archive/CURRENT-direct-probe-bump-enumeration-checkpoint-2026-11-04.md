# Direct native reflection-probe bump-enumeration checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `131f446829233e356393d9ee8fb0882123c2c16c`
(`Enumerate probe bump batches without GL binds`)

## Replacement-first boundary

Legacy bump selection previously happened inside `bindBumpMap`, so direct
Metal scene enumeration could not obtain brightness, darkness or standard
normal images without also mutating GL texture-unit state. Commit `131f446829`
extracts `resolveBumpMap` as the single backend-neutral selector. The retained
GL binder consumes that result; the direct Metal traversal consumes it without
binding a GL shader, texture or target.

The probe-only CPU render-map traversal now covers static and rigged bump
geometry. It preserves the existing opaque/alpha-mask split, alpha cutoffs and
sorted-run normal lookup reuse, then feeds the existing Metal bump batch
contract. Cutout classification asks for GPU residency rather than GL-specific
residency, so native-only textures retain their known RGB/alpha semantics.

Exact `LLDrawInfo` identities suppress a duplicate native batch when the GL
pool subsequently renders the complete transition face. Missing native normal
images, tangents, vertex/index buffers or rigged skin palettes invalidate the
entire native face; they do not weaken or skip the GL fallback.

This is feature replacement, not OpenGL deletion. Indexed generic masks and
pool/face-owned sky, terrain, water, tree, avatar and post-deferred families
remain. `beginMetalReflectionProbeFaceCapture` still requires live GL
resources, `display_cube_face` still executes and the GL cube array remains.

## Exact build and validation evidence

- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- The changed `alchemy-bin` and `newview_tests` targets compiled and linked
  warnings-as-errors. Full `llrender_gl_fresh_tests` passed 3,594 assertions
  across 106 cases with Metal API/GPU validation enabled.
- Full `almetal_tests` passed 52,526 assertions across 56 cases with Metal
  API/GPU validation enabled. Full `newview_tests` passed 98,990 assertions:
  746 cases passed and the established non-reference CRT case skipped.
- `tools/metal/build.sh` exited 0 at renderer source `131f446829`. The exact
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68272` (`26.4.0.68272`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `0a90914c658b180ea43f0b969261e5b8e9269b0f8aaec6aae90633b086e1348c`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory scans 2,407 files and reports 3,214 findings / 2,903 unique
  path/rule/snippet records, expected exit 1. No rule or suppression changed.
  Exact report `artifacts/metal/audit-gl-131f446829.json` has SHA-256
  `7f4462e368544c9a008f35972cbfe36150eb4be448434a05fcdfd8c362693815`.
- `otool -L` still reports both OpenGL and Metal. Static, linked and runtime
  no-GL release gates remain open. No viewer was launched, driven,
  photographed or compared.

## Complete-product effect and next boundary

This advances M-007 and M-011 by making legacy bump feature semantics and
probe batch production independent of GL binding. The next source boundary is
a direct producer for pool-owned tree faces with duplicate suppression,
followed by explicit terrain, sky, water and avatar inputs. Only complete
native face production permits removal of the GL resource gate,
`display_cube_face` participation or the live GL cube array. Visual probe
parity remains queued for the owner-attended daily review.
