# Hero-cube camera orientation checkpoint — 2026-11-02

**Observed host:** physical Apple M1 Max, 64 GiB unified memory, macOS 26.5.2,
Apple7-family Metal support.
**Source checkpoint:** `1518b533ce48b68a13fdcb0d483bcd7df8e070b4`.
**Outcome:** the GL cube-camera basis, native face publication and Metal
texture-cube sampling convention agree. No production transform correction was
justified, and normal mirror authority remains fail-closed.

## Source-derived convention

`LLViewerWindow::cubeSnapshot` renders faces in `+X, -X, +Y, -Y, +Z, -Z`
order. For framebuffer coordinates `(u,v)` its camera ray is
`forward + u * (forward x up) + v * up`, using these up vectors:
`-Y, -Y, +Z, -Z, -Y, -Y`. This expands to the six directions already used by
the GLSL `cubeDirection` helper and MSL `almetal_world_cube_direction` helper.
`LLCubeMapArray` uses the same face order.

The native world path deliberately flips clip-space Y. Existing presentation
and readback evidence establishes that row zero in the raw Metal target matches
row zero of the GL lower-left framebuffer convention; the CAMetalLayer boundary
performs the single presentation flip. `ALMetalWorldGBufferRenderer::
updateHeroProbeFace` therefore must copy the authored 2D face into the
identically numbered cube slice without a second row, column or face transform.

## Physical regression

Commit `1518b533ce` adds a test-only nearest cube-sampling kernel and extends
`almetal: native hero faces publish filtered cube radiance`. The test fills all
six production hero-probe faces with asymmetric face/row/column values, calls
the production `updateHeroProbeFace` publication path, then samples the
resulting cube at all 96 texel-centre rays derived from the exact
`cubeSnapshot` forward/up camera basis. Every RGBA channel must equal the
corresponding source texel. The regression fails on a face swap, row or column
flip, or 90-degree rotation.

On the physical M1 Max under Metal API and load-action validation:

- focused RelWithDebInfo case: 589 assertions, one case, pass;
- complete RelWithDebInfo suite: 52,184 assertions, 52 cases, pass;
- complete Debug suite: 52,184 assertions, 52 cases, pass;
- complete Release suite: 52,184 assertions, 52 cases, pass;
- Release CTest target 59 also passed.

Offline MSL and the Objective-C++ test binary compiled in all three
configurations. No production renderer code changed because the source-derived
oracle passed.

## Source-exact Release evidence

`AL_METAL_PRESET=ninja-os-arm64 AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64
AL_METAL_CONFIG=Release AL_METAL_JOBS=8 tools/metal/build.sh build` completed;
an immediate source-exact rerun reported `ninja: no work to do` and exit 0.
Development bundle version is `26.4.0d68097` (`26.4.0.68097`).

- `tools/metal/verify_app.sh <app>`: 349 Mach-O slices checked, zero invalid
  non-ARM64 slices, exit 0;
- `codesign --verify --deep --strict --verbose=2 <app>`: valid, exit 0;
- Release non-manual/non-GL neutral gate: 484/484 passed in 38.39 seconds;
- executable SHA-256:
  `06c4e8f7b258dc24a64090a464da908fc4543a4819306a07d885a9dca7c57909`;
- source and packaged MSL SHA-256 (byte-identical):
  `d263315d1ea0c2c834e9e5a5d37109e9ec5d56325153220dedc1b2a9ab2ffac9`;
- compiled metallib SHA-256:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.

The executable still links both Metal and OpenGL. This is not native-only
release evidence.

## Live-evidence correction and next boundary

The replacement private dry candidate/reference pair remains diagnostic only:
its cameras are not pixel-exact and mirror contents differ. Log inspection also
found 64 candidate versus 24 reference fetch-failure/HTTP/missing-resource
matches. That unequal content convergence means the pair cannot causally assign
its missing surfaces to a renderer owner.

This checkpoint closes the proposed face/row/rotation hypothesis and protects
the correct direct-copy convention. It does not close mirror parity. The next
comparison must use source-led forward/material batch and texture-ownership
evidence with matched cache/content convergence, or a controlled fixture that
proves equivalent assets before capture. Keep the mirror guard closed while
the strict 3,351-finding GL inventory proceeds independently.
