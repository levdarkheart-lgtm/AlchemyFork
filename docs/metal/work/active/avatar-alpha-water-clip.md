# Legacy avatar alpha water clipping — retained source finding

Rechecked 2026-11-12 against committed `8599bfce5195efb301b22a44113850a38f93932b`.
Reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`. Uncommitted renderer
changes were excluded. No implementation, build, test or viewer action occurred.

## Original contract

Original `indra/newview/app_settings/shaders/class2/deferred/alphaF.glsl:114–118`
excludes `waterClip(pos.xyz)` under `IS_AVATAR_SKIN`. `llviewershadermgr.cpp:2832` sets that permutation for
`gDeferredAvatarAlphaProgram`; the avatar post-deferred pass binds it and
renders transparent legacy avatar meshes. Ordinary rigged-object alpha retains
its separate water-clipping contract.

## Current reachable mismatch

At committed `8599bfce5195`, `LLDrawPoolAvatar::renderPostDeferred:460` calls
`captureMetalPostDeferred:395`, then `LLVOAvatar::captureMetalPostDeferredTransparent:5923`
submits eligible legacy skirt, eyelash and hair meshes. Pipeline avatar capture
sets `avatar_alpha=true` and `pre_water_simple=true`, without `post_water_alpha`
(`pipeline.cpp:15537–15582`). Avatar PSOs select generic simple/shadow fragments
(`almetalworld.cpp:4838–4841,17801`), which unconditionally discard one water-plane
side in `almetal_world_pre_water_simple_surface` (`almetal_builtin.metal:8253–8261`).
The live environment supplies sign ±1 (`pipeline.cpp:7795,7838`); only post-water
batches invert it. Sign -1 and positive signed distance therefore discard where
the original avatar permutation continues. Scalar-weight/cloth paths are reachable.

## Evidence, limits and next action

Prior receipt: `/private/tmp/alchemy-black-coat-water-consumer-diagnosis-20261111/audit-receipt.json`,
SHA256 `611965f233b70ac1b51534eb4ccbf01f2a514f722bcfb66049af5be15ba71b8e`.
This is source-confirmed coverage divergence, not an observed cause of the coat,
beard-light or turning reports. Failing-frame material/constants remain unknown.
Retain it in the defect register. Next is a bounded both-sides avatar coverage
oracle with ordinary rigged clipping controls; no fix or test is authorized here.
