# Current engineering state — direct probe transparent-avatar checkpoint

**Observed:** 2026-11-04 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `b27df920fc` (`Enumerate probe avatar alpha directly`)

## Implemented boundary

- Reflection-probe post-deferred traversal now enumerates transparent ordinary-
  avatar and control-avatar geometry directly before the retained GL pool binds
  or draws. It reuses the same pool-level loaded/friend/mute/impostor/animesh
  eligibility helper as direct deferred avatar production.
- The direct producer retains skirt wearable/visibility, head/eyelash, baked
  hair and jelly/self/head rules. Joint traversal follows the same active LOD
  branch as retained rendering and resolves the same composite/default diffuse
  source, Metal buffers, 15-joint palette and optional cloth state.
- High-detail hair/skirt preserves three ordered submissions: unculled color
  with read-only depth, unculled depth-only, then unculled color with read-
  only depth. Low-detail geometry preserves the front-culled/read-only-depth
  inside pass followed by the back-culled/color-and-depth outside pass.
- Direct batches carry those cull, depth-write and color-write states explicitly
  and do not inspect or mutate GL state. Exact joint-mesh identity suppresses
  only the later duplicate Metal hook; the retained GL draws still execute as
  complete transition fallback.

## Failure and feature-preservation contract

- Missing native texture/buffer/palette inputs, a GL-only test texture or an
  unexpected joint invalidates the whole native probe face. The UI dummy mode,
  which bypasses the world transparent multipass contract, also deliberately
  fails closed rather than publishing an approximate face.
- A partial direct multipass cannot publish: if any required replay fails, all
  native post-deferred batches are cleared and the complete GL face remains.
- No hair, skirt, eyelash, cloth, LOD or alpha behavior was deleted. Deferred
  rigid/opaque avatars and impostors remain direct from checkpoint `6183613ef0`;
  this checkpoint adds their ordered transparent portion.
- Generic indexed masks and remaining pass-owned families still prevent removal
  of the GL resource gate, `display_cube_face`, retained scene traversal and the
  live cube array.

## Exact validation evidence

- `tools/metal/doctor.sh` remains green on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- `tools/metal/build.sh` exited 0 at committed source `b27df920fc`; the
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68284` (`26.4.0.68284`).
- The source-exact Ninja build linked `almetal_tests`,
  `llrender_gl_fresh_tests`, `newview_tests` and `alchemy-bin` warnings-as-
  errors. With Metal API/GPU validation enabled, `almetal_tests` passed 52,526
  assertions across 56 cases and `llrender_gl_fresh_tests` passed 3,594
  assertions across 106 cases. Existing physical coverage exercises the exact
  high-/low-detail avatar-alpha cull, depth and color-write combinations.
- `newview_tests` self-reported build 68284 and passed 98,990 assertions: 746
  cases passed and the established non-reference CRT case skipped.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `ed66dc697f5f0da16503418f7d5e5876c78f721cc3ceb22f0dd6c7c74a35aed5`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged `ALMetalWorldPresentation` remains default-on at `1`.
- Strict inventory scans 2,407 files and reports 3,214 findings / 2,903 unique
  path/rule/snippet records, expected exit 1. No rule or suppression changed.
  Exact report `artifacts/metal/audit-gl-b27df920fc.json` has SHA-256
  `f8e3c3ecd6883dadc934e686e312a5b11e6bb0167406e23e615831ae6a5a2d53`.
- `otool -L` still reports both OpenGL and Metal. Static, linked and runtime
  no-GL release gates remain open. No viewer was launched, driven,
  photographed or compared.

## Pause handoff

This advances M-007 and M-011 by making the complete ordinary world-avatar
probe family—impostor, rigid, opaque skinned/cloth and transparent hair/skirt—
directly discoverable and state-complete in Metal while retaining fail-closed
fallback. It removes no feature and does not yet authorize GL owner deletion.
Per owner request, implementation pauses at this coherent checkpoint for an
owner-attended visual-validation session. After explicit resume, the next
source boundary is indexed generic masks followed by remaining pass-owned
inputs; visual results must be recorded against exact builds and profiles.
