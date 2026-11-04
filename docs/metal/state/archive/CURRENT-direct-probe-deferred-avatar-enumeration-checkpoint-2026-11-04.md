# Current engineering state — direct probe deferred-avatar enumeration checkpoint

**Observed:** 2026-11-04 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `6183613ef0` (`Enumerate probe deferred avatars directly`)

## Implemented boundary

- Reflection-probe capture now enumerates ordinary-avatar and control-avatar
  pools directly from CPU scene state before the retained GL pool traversal.
  Pool type and deferred pass ordering remain explicit.
- Pass 0 produces native impostor billboards when retained rendering selects an
  impostor. Pass 1 produces rigid eye geometry. Pass 2 produces opaque skinned
  body geometry and preserves the existing optional cloth state.
- The direct producer retains the existing fully-loaded, friends-only, mute,
  impostor/appearance, attached-animesh, self/head, baked-texture, jelly/UI and
  render-type decisions. Joint traversal follows the same active LOD hierarchy
  and resolves the same composite/default diffuse sources.
- Avatar and impostor matrix capture now derives model-view/projection state
  from the viewer camera and joint world matrices. It no longer reads the GL
  matrix stack. The impostor billboard and normal-rebase calculation is one
  shared CPU helper used by both direct Metal and retained rendering.
- Exact `LLViewerJointMesh` and `LLVOAvatar` identities suppress only the later
  duplicate native hook after successful direct production. Retained GL draws
  continue to run as complete transition fallback.

## Failure and feature-preservation contract

- Missing native textures, vertex/index buffers, skin palettes or coherent
  four-attachment impostor bakes invalidate the entire native probe face.
  Native impostor bake absence schedules the established retry path.
- An incomplete-avatar immediate-mode diagnostic sphere, a manual GL-only test
  texture, an unexpected non-viewer joint or requested debug-impostor outline
  also fails the native face closed. None is silently omitted.
- No avatar feature was removed. Transparent hair and skirt remain at the
  ordered post-deferred avatar boundary and still reach native capture through
  the retained draw hook; they are the next direct-production boundary. Generic
  indexed masks and remaining pass-owned families also remain open.
- Therefore this checkpoint does not permit removal of the GL resource gate,
  `display_cube_face`, retained GL avatar traversal or the live cube array.

## Exact validation evidence

- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- `tools/metal/build.sh` exited 0 at committed source `6183613ef0`; the
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68282` (`26.4.0.68282`).
- The source-exact Ninja build linked `almetal_tests`,
  `llrender_gl_fresh_tests`, `newview_tests` and `alchemy-bin` warnings-as-
  errors. With Metal API/GPU validation enabled, `almetal_tests` passed 52,526
  assertions across 56 cases and `llrender_gl_fresh_tests` passed 3,594
  assertions across 106 cases. `newview_tests` self-reported build 68282 and
  passed 98,990 assertions: 746 cases passed and the established non-reference
  CRT case skipped.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `66509c200d8158d561f53b1f665b18f2b0096ecf57b3d734b02e5b6baabb2674`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains default-on at `1`.
- Harness structure validation passed for 32 required files and all 30
  harness/tool tests passed. `git diff --check` passed before this archive.
- Strict inventory scans 2,407 files and reports 3,214 findings / 2,903 unique
  path/rule/snippet records, expected exit 1. No rule or suppression changed.
  Exact report `artifacts/metal/audit-gl-6183613ef0.json` has SHA-256
  `fa79f0a6ff915838bfc769b37d1a2b8049fcfb112ce3b482015371286d6c08de`.
- `otool -L` still reports both OpenGL and Metal. Static, linked and runtime
  no-GL release gates remain open. No viewer was launched, driven,
  photographed or compared.

## Complete-product effect and next boundary

This advances M-007 and M-011 by replacing GL-dependent discovery and matrix
state for deferred probe avatars with a direct native Metal producer while
retaining the complete avatar selection, LOD, skinning, cloth, impostor and
failure contract. It does not treat deletion as progress and removes no
feature. The next source boundary is direct transparent hair/skirt production
at the ordered post-deferred avatar boundary, followed by indexed masks and
remaining pass-owned inputs. Visual probe parity remains queued for the
owner-attended daily review.
