# GL-independent native texture-pressure checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `66581835e8` (`Keep native texture pressure alive without GL`)

## Replacement-first boundary

Already-native fetched textures now remain pressure-manageable when the global
GL manager is disabled. `LLViewerTextureList::updateImagesCreateTextures()`
drains Metal-only downscale work before the GL-disabled return and calls the
established native `LLImageGL::scaleDown()` implementation, so the converted
resource keeps its memory-pressure behavior without a GL name, scratch target,
copy shader or live GL manager.

The upload queue and GL/dual-backed downscale branch remain behind the existing
GL-disabled guard. They have not been deleted or silently treated as complete;
their side effects remain fail-closed until fetched upload and each residual
consumer have native replacements.

The native queue policy is factored as a small ownership-neutral scheduling
helper rather than a graphics abstraction. It inspects only the entries that
were queued at entry, consumes eligible native work, and preserves the original
relative order of retained and unexamined work. This also fixes the former
timeout rotation, which could move already-inspected retained entries behind
unexamined entries.

## Exact build and validation evidence

- The new focused queue suite passed 12 assertions across 3 cases. It proves
  native consumption, stable retained order, timeout/resume behavior and the
  minimum-scan rule.
- The complete `newview_tests` binary passed 98,990 assertions across 747
  cases; the established non-reference CRT generator case skipped.
- With Metal API/GPU/shader/load-store validation enabled, the focused physical
  `[LLImageGL][almetal]` slice passed 1,972 assertions across 20 cases and the
  complete renderer binary passed 2,683 assertions across 100 cases.
- Ninja Release compiled the changed test and production source warnings-as-
  errors, linked the full viewer and packaged `alchemy-bin`.
- `tools/metal/build.sh` exited 0 at committed source `66581835e8`. The
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68243` (`26.4.0.68243`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `893e130a20a8117243c34373b2cb16aa09f393ad1f2b836d3fc647bbe5ba26a3`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory scanned 2,407 files and reported 3,220 findings / 2,909
  unique path/kinds/snippet records, expected exit 1. The additional owned file
  added no GL finding. Report SHA-256:
  `1f0f70ac302c12f62f75c1cc66bc1e8450ad202a5203f8a1bf7b21c630053104`.
- Both OpenGL and Metal remain linked. Static, linked and runtime no-GL release
  gates remain open.
- No viewer executable was launched, driven, photographed or compared.

## Next executable boundary

Give ordinary fetched-image creation a native Metal publication path that does
not generate or require a GL name. Preserve format/swizzle, mip, alpha/pick,
threaded publication and accounting behavior; keep unsupported texture kinds
and residual GL consumers on explicit fail-closed recovery until replaced.
