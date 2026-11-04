# Direct native reflection-probe sky-enumeration checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `23146eeeaa729cf00992da8708c9072e262db3a1`
(`Enumerate reflection-probe sky directly`)

## Replacement-first boundary

Commit `23146eeeaa` adds the reflection-probe sky pool to the direct native
scene producer before retained GL traversal. When HDRI is authoritative, the
producer captures the full HDRI dome with its exposure and rotation.
Otherwise it captures WindLight atmospheric haze, rainbow/halo and weather
parameters, then the eligible sun/moon bodies. Clouds are captured on
radiance faces and remain excluded from irradiance faces as before. Ordinary
probes still exclude the sun while hero-mirror capture includes it.

Stars, meteors and aurora remain deliberately excluded from reflection cube
snapshots by the retained viewer behavior; those features were not deleted.
The direct path reconstructs the probe camera translation, sky-dome rotation,
scale, camera-height offset and projection on the CPU. It does not bind a GL
shader/texture, read a GL matrix or issue a GL draw.

Exact dome-buffer and heavenly-body identities suppress only duplicate Metal
capture during the subsequent complete GL fallback traversal. Missing or
invalid native inputs clear the staged identity set, leaving the retained GL
hook as capture authority. This is feature replacement, not OpenGL deletion.
Water, avatar, indexed generic masks and post-deferred inputs remain
incomplete for direct probe production. `beginMetalReflectionProbeFaceCapture`
still requires live GL resources, `display_cube_face` still executes and the
GL cube array remains.

## Exact build and validation evidence

- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB unified
  memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and Metal compiler.
- `tools/metal/build.sh` exited 0 at renderer source `23146eeeaa`. The exact
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68278` (`26.4.0.68278`).
- The source-exact Ninja build linked `almetal_tests`,
  `llrender_gl_fresh_tests`, `newview_tests` and `alchemy-bin` warnings-as-
  errors. With Metal API/GPU validation enabled, `almetal_tests` passed 52,526
  assertions across 56 cases and `llrender_gl_fresh_tests` passed 3,594
  assertions across 106 cases. `newview_tests` passed 98,990 assertions: 746
  cases passed and the established non-reference CRT case skipped.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `19636a263a2df538778a86a64ec5ca75dcd568de812fbc59bd2e6e8018654352`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains default-on at `1`.
- Harness structure validation passed for 32 required files and all 30
  harness/tool tests passed.
- Strict inventory scans 2,407 files and reports 3,214 findings / 2,903 unique
  path/rule/snippet records, expected exit 1. No rule or suppression changed.
  Exact report `artifacts/metal/audit-gl-23146eeeaa.json` has SHA-256
  `810665755c82d2f18ad85cd998cc6a955d2ab7629505b49536997905ef9e27b4`.
- `otool -L` still reports both OpenGL and Metal. Static, linked and runtime
  no-GL release gates remain open. No viewer was launched, driven,
  photographed or compared.

## Complete-product effect and next boundary

This advances M-007 and M-011 by replacing reflection-probe sky traversal with
direct Metal production while preserving the viewer's HDRI/WindLight,
heavenly-body, radiance-cloud and cube-exclusion semantics. The next source
boundary is direct water production, followed by avatar and remaining pass
inputs. Only complete native face production permits removal of the GL
resource gate, `display_cube_face` participation or the live GL cube array.
Visual probe parity remains queued for the owner-attended daily review.
