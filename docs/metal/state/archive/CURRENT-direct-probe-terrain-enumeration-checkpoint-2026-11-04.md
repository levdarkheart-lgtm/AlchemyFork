# Direct native reflection-probe terrain-enumeration checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `914716ad0111f548eec10482af14be627a02fc6f`
(`Enumerate probe terrain without GL pools`)

## Replacement-first boundary

Terrain geometry and material preparation are owned by `LLDrawPoolTerrain`,
outside the deferred CPU render maps. Commit `914716ad01` adds that pool to the
direct reflection-probe scene producer before the retained GL face executes.

The direct legacy-terrain path preserves four detail textures, the default-
texture override, world-space detail scale/phase, native height/composition
surface-map refresh, alpha ramp, parcel overlay, planar/triplanar and hex
sampling, smoothing and distance/pinned tessellation. The direct PBR-terrain
path preserves four base-color/normal/ORM/emissive slots, missing-texture
defaults, local-material override, paint-map fallback, KHR texture transforms,
normal axes, material factors, mask cutoffs, PBR detail level, flat-normal
mode, triplanar/hex state, smoothing and tessellation.

Both modes reconstruct model-view, projection and normal matrices from the
patch region and current probe camera. They do not bind a GL shader/texture,
read a GL matrix or issue a GL draw. Exact `LLFace` identities suppress only
duplicate Metal capture when the GL pool subsequently renders the complete
transition face. Missing region, surface map, patch buffer, material/paint
texture or other native input invalidates the whole native face; it does not
weaken the terrain feature or skip the GL fallback.

This is feature replacement, not OpenGL deletion. Sky, water, avatar, indexed
generic masks and post-deferred inputs remain incomplete for direct probe
production. `beginMetalReflectionProbeFaceCapture` still requires live GL
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
- `tools/metal/build.sh` exited 0 at renderer source `914716ad01`. The exact
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68276` (`26.4.0.68276`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `242b7008cbf98cfdf56245b25beb1ade14e5c4fce724c8149314e638c57c16c8`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory scans 2,407 files and reports 3,214 findings / 2,903 unique
  path/rule/snippet records, expected exit 1. No rule or suppression changed.
  Exact report `artifacts/metal/audit-gl-914716ad01.json` has SHA-256
  `6a8c4364b406a4f3f555d6ad162c62d20097de70ad55cf24d78bd36cc142a1f5`.
- `otool -L` still reports both OpenGL and Metal. Static, linked and runtime
  no-GL release gates remain open. No viewer was launched, driven,
  photographed or compared.

## Complete-product effect and next boundary

This advances M-007 and M-011 by making full legacy and PBR terrain probe
production independent of GL pool execution while retaining material, paint,
overlay, surface and tessellation behavior. The next source boundary is direct
sky-family production, followed by explicit water and avatar inputs. Only
complete native face production permits removal of the GL resource gate,
`display_cube_face` participation or the live GL cube array. Visual probe
parity remains queued for the owner-attended daily review.
