# Opaque fullbright parity checkpoint — 2026-11-02

**Observed host:** physical Apple M1 Max, 64 GiB unified memory, macOS 26.5.2,
Apple7-family Metal support.
**Source checkpoint:** `ae2b43644211a73802d4197efa39a361ad5614bb`.
**Outcome:** ordinary and alpha-masked fullbright now match their retained GL
owner's opaque, RGB-only presentation state. Legacy alpha-pool BLEND,
stained-glass and broader mirror parity remain open and fail-closed.

## Source-derived defect

The matched-mirror investigation first eliminated cube orientation with the
preceding 96-texel physical oracle, then traced the conspicuously absent wall
art/screens through their forward owner. `renderDeferredLighting` enters
`renderGeomPostDeferred` with `GL_BLEND` disabled. `LLDrawPoolFullbright::
renderPostDeferred` selects blend factors but does not enable blending, while
the outer post-deferred owner uses RGB-only color writes.

`app_settings/shaders/class1/deferred/fullbrightF.glsl` documents the essential
data meaning: outside the alpha pool, streamed vertex alpha carries shininess,
not coverage. The Metal fullbright descriptor instead enabled source-alpha
blending for ordinary fullbright. A zero-shiny textured poster could therefore
be erased, and any intermediate shininess could incorrectly attenuate it.

This diagnosis is bounded to ordinary `POOL_FULLBRIGHT` and
`POOL_FULLBRIGHT_ALPHA_MASK`. It does not assign legacy alpha-pool BLEND or
stained glass to the same state and does not by itself prove live mirror parity.

## Implementation and physical regression

Commit `ae2b436442` disables blending for the native ordinary/masked fullbright
descriptor and applies an RGB-only write mask. The final-color fullbright
pipelines inherit the corrected descriptor; impostor coverage remains handled
by its established coverage-stamp owner.

The physical GPU regression renders ordinary non-masked fullbright with the
fixture's 0.5 vertex alpha over an existing lighting target. It requires the
full green texel rather than a half-strength source-alpha blend and requires the
destination alpha to remain 0.5 rather than rising to 0.75. It then restores
the alpha-mask fixture for the remaining established assertions.

Under `MTL_DEBUG_LAYER=1`, load-action validation and physical Metal execution:

- focused RelWithDebInfo case: 8,022 assertions, one case, pass;
- complete RelWithDebInfo suite: 52,190 assertions, 52 cases, pass;
- complete Debug suite: 52,190 assertions, 52 cases, pass;
- complete Release suite: 52,190 assertions, 52 cases, pass;
- offline MSL and Objective-C++ test targets compiled in all configurations.

## Source-exact Release evidence

The exact command below completed at the committed checkpoint, followed by an
immediate exit-0 `ninja: no work to do` rerun:

```sh
env AL_METAL_PRESET=ninja-os-arm64 \
  AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 \
  AL_METAL_CONFIG=Release AL_METAL_JOBS=8 \
  tools/metal/build.sh build
```

CMake reported revision from Git and produced Development build
`26.4.0d68099` (`26.4.0.68099`).

- `tools/metal/verify_app.sh <app>`: 349 Mach-O slices checked, zero invalid
  non-ARM64 slices, exit 0;
- `codesign --verify --deep --strict --verbose=2 <app>`: valid, exit 0;
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C Release -LE
  'manual|gl' --output-on-failure -j 8`: 484/484 passed in 36.60 seconds;
- executable SHA-256:
  `ea063150c286acf7498a3c6e065cbe367e140b401ef3682536d1771d357f602a`;
- source and packaged MSL SHA-256, byte-identical:
  `d263315d1ea0c2c834e9e5a5d37109e9ec5d56325153220dedc1b2a9ab2ffac9`;
- compiled metallib SHA-256:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.

The executable still links both Metal and OpenGL. This checkpoint is not a
native-only release claim, and the refreshed 3,351-finding strict GL inventory
still exits 1.

## Product boundary and next action

This correction advances real retained-Alchemy rendering: opaque fullbright
content can no longer disappear as a function of shininess alpha. It also adds
a regression at the render-target boundary, including the GL RGB-only alpha
side effect. The uncontrolled private dry pair remains inadmissible as a parity
pass because cameras/content differ and its candidate/reference logs contain
64 versus 24 fetch/missing-resource matches.

Next, trace the distinct legacy alpha-pool BLEND/stained-glass owner from
source and isolate it with a controlled, content-converged fixture. Keep normal
mirror authority closed and continue strict OpenGL inventory reconciliation
independently.
