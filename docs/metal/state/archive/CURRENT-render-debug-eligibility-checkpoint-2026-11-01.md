# Render-debug eligibility audit checkpoint — 2026-11-01

## Observed result

Commit `9e28d64932` (`fix(metal): narrow debug fallback to world output`)
replaces the broad `mRenderDebugMask != 0` native-presentation veto with an
explicit `RENDER_DEBUG_GL_WORLD_OUTPUT` mask. Source tracing separates all 33
defined render-debug settings by their actual presentation owner.

Twenty settings still emit uncaptured world pixels or alter geometry captured
for native publication and therefore remain fail-closed:

- Composition, BBoxes, Octree, Wind Vectors, Occlusion and Points;
- Texture Animation, Lights, Batch Size, Raycast and Shadow Frusta;
- Avatar Volume, Avatar Joints, Agent Target and Update Type;
- Physics Shapes, Normals, Texel Density, Impostors and Reflection Probes.

Nine settings feed only LLHUD/debug text, which is drawn after native world
publication, so they no longer force a GL world fallback: Texture Priority,
Texture Area, Face Area, Particles, Avatar Draw Info, Sculpted, LOD Info,
Triangle Count and Probe Updates.

Four defined settings have no production consumer and likewise do not veto:
VERIFY, GLOW, ALPHA_BINS and NODES. No visual owner was waived. Adding a future
world-pixel consumer requires adding its bit to the explicit mask until native
capture is proven.

The linked OSS pathfinding implementation remains a nonfunctional stub. Its
separate latent eligibility guard remains intact.

## Validation

- Warnings-as-errors RelWithDebInfo `alchemy-bin` compiled, linked, staged and
  signed.
- The neutral gate passed 484/484 in 35.39 seconds:
  `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure -j 8`.
- The committed-head source-exact command
  `env AL_METAL_CONFIG=Release AL_METAL_JOBS=8 tools/metal/build.sh build`
  produced Development `26.4.0d68049` and exited zero.
- Deep strict signing passed. `tools/metal/verify_app.sh` inspected 349 Mach-O
  slices with no non-ARM64 slice; the main executable reports arm64.

SHA-256 provenance:

- executable:
  `f8dd26047e89afd0aca67de34de3eb64a2bc2ae986caad4f7d189288c14c7691`
- source and packaged `almetal_builtin.metal`:
  `dd51886cc8cc33a8b19ac69206fed5045421c8905bb51c7d596fdeaefb467214`
- packaged `almetal_builtin.metallib`:
  `b5ccb104ec22fade3cf1822595924c751128b6ca14e0ab188276890339357e58`

The previously validated native wireframe renderer and its 45,147-assertion
three-configuration physical Metal evidence are unchanged. The executable
still links OpenGL during migration; the 20 visual debug modes remain a real
strict-no-GL boundary.

## Persistent acceptance prerequisites

- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers before capture. Off/on is invalid and both-off is isolation only.
- Exact RLVa parity is deferred M-013 nice-to-have. Exact comparison requires
  RLV enabled and saved in both viewers followed by restarting both; only enough
  native RLVa behavior to unblock Metal/no-GL cutover is mandatory.

## Next executable boundary

Implement Develop **Wind Vectors**, the smallest bounded remaining visual
owner. It is a fixed live 16x16 simulator lattice: 256 one-pixel green points
and 256 red line segments. Remove only its mask bit after physical transform,
color, count, depth/order and off/empty proof.
