# Metal port execution — Normals checkpoint — 2026-11-02

## Completed bounded owner

Commit `08973b83dccad57d39fa0386c4f8463385761d2a` implements native Develop
**Normals** replay.

- Volume faces retain the exact CPU-generated yellow normal and cyan tangent
  line pairs, including inverse-scale normalization and live transforms.
- Non-volume drawables retain zero-copy Metal vertex/index buffers; native
  u16/u32 variants expand the same per-index normal and optional tangent lines
  previously produced by the GL geometry shader.
- Replay preserves one-pixel raster, disabled culling, standard alpha blend,
  RGB-only writes, read-only LEQUAL depth and forward/reverse-Z behavior.
- Volume HUD lines remain owned by ordered native immediate-mode UI capture;
  unexpected non-volume HUD use fails closed.
- Validation rejects invalid storage, values, kind, color, ranges, buffers,
  index width, transforms, tangent contract, state/order and owner collisions.
- Only the Normals veto bit was removed; Texel Density is now the sole explicit
  render-debug world-output veto. The latent pathfinding guard remains separate.

## Validation evidence

- RelWithDebInfo, Debug and Release physical Metal suites: 51,416 assertions in
  52 cases each under Metal API/load-action validation.
- Neutral non-manual/non-GL gate: 484/484 in 35.58 seconds.
- Warnings-as-errors integration and source-exact Release build pass.
- Release Development `26.4.0d68087` (`26.4.0.68087`) at full SHA
  `08973b83dccad57d39fa0386c4f8463385761d2a` followed a 161-step wrapper build.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `039fbe6e2af93d5c9fc63d8c94b79a574323aa908a4a27a08c7a16c6807f445d`.
- Source/packaged MSL SHA-256:
  `73955209f611038a5e4fe0305642614fefee30766dcd26fd03b7df7d3933df20`.
- Metallib SHA-256:
  `fd64dfedceb9056e85090916b7e0dab2d8e17c553cc4ffe63ea1b5e67647c297`.

The executable still links both Metal and OpenGL; this is not a native-only
release claim. Controlled live Normals parity remains unclaimed.

## Durable constraints

- Any shadow reference/candidate pair requires shadows enabled in Graphics
  Preferences in both viewers. Off/on is invalid and both-off cannot prove
  parity.
- Exact RLVa parity is deferred M-013 nice-to-have. Exact RLVa tests require
  Preferences enable/save/restart in both viewers; only cutover-minimum behavior
  is mandatory. Remaining exact work stays in
  `docs/metal/work/deferred/rlva-parity.md`.

## Next owner

Audit `RENDER_DEBUG_TEXEL_DENSITY` in full before changing the final explicit
visual veto. Preserve every geometry producer, shader input, transform,
texture/state dependency and side effect as one coherent owner.
