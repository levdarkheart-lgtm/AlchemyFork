# Direct native reflection-probe water-enumeration checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `ff2fd5dbee2ba8f3b0bbd68e7afc039082b3d5fd`
(`Enumerate probe water before GL pool`)

## Replacement-first boundary

Commit `ff2fd5dbee` adds the reflection-probe water surface to the direct native
producer at its existing post-deferred ordering boundary. The producer runs
after water exclusion, water haze and pre-water alpha have established their
inputs, but before the retained GL water pool draws. The ordinary world hook
and direct probe producer share one native snapshot builder rather than
maintaining divergent water implementations.

The snapshot retains current/next normal-map choice and blend, double-precision
global wave phases and lattice placement, map means, CPU camera modelview,
normal and environment matrices, far/rim state, normal scale, glint roughness,
Fresnel scale/offset, blur, light direction, hextile mode, above/below-water
refraction scale, exclusion state and the selected shader's shadow behavior.
Reflection cubes retain the viewer's existing flat-field contract: probe faces
do not draw wave patches. Probe water does not refract the clipped cube face,
and underwater faces still require the correctly ordered native water haze.

The builder reaches each normal map's Metal mirror through the existing image
owner but does not bind a GL texture, read a GL matrix or issue a GL draw. A
successful direct snapshot suppresses only the later duplicate Metal hook; the
complete GL face still renders as transition fallback. Missing normal maps,
field/cell data, expected field draw count or ordered haze marks the whole
native face incomplete, so a waterless cube is never published as success.
This is feature replacement, not feature deletion or OpenGL-count reduction.

Avatar, indexed generic masks and remaining post-deferred inputs are not yet
complete for direct probe production. The live GL resource gate,
`display_cube_face` traversal and GL cube-array owner therefore remain.

## Exact build and validation evidence

- `tools/metal/build.sh` exited 0 at renderer source `ff2fd5dbee`. The exact
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68280` (`26.4.0.68280`).
- The source-exact Ninja build linked `almetal_tests`,
  `llrender_gl_fresh_tests`, `newview_tests` and `alchemy-bin` warnings-as-
  errors. With Metal API/GPU validation enabled, `almetal_tests` passed 52,526
  assertions across 56 cases and `llrender_gl_fresh_tests` passed 3,594
  assertions across 106 cases. `newview_tests` passed 98,990 assertions: 746
  cases passed and the established non-reference CRT case skipped.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `0db65d7f2a9d5842a46591d2f7159ca35fdea02ef5452b2e1183fad313338260`.
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
  Exact report `artifacts/metal/audit-gl-ff2fd5dbee.json` has SHA-256
  `e4f7bbe1186d737fe10d9c6d48aa0eab4e341b986bc7c5e31949463dfff0afae`.
- `otool -L` still reports both OpenGL and Metal. Static, linked and runtime
  no-GL release gates remain open. No viewer was launched, driven,
  photographed or compared.

## Complete-product effect and next boundary

This advances M-007 and M-011 by making native Metal the direct producer for
probe water while retaining the complete surface, environment and ordering
contract. The next source boundary is direct avatar production, followed by
indexed masks and remaining pass-owned inputs. Only complete native face
production permits removal of the GL resource gate, `display_cube_face`
participation or the live GL cube array. Visual probe parity remains queued
for the owner-attended daily review.
