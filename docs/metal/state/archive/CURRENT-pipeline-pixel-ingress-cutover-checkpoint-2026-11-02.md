# Pipeline pixel-ingress cutover checkpoint — 2026-11-02

## Observed source state

- Branch: `codex/apple-silicon-metal`.
- Final renderer checkpoint: `35082663e5`.
- Host: physical M1 Max, 64 GiB unified memory, 32-core GPU, macOS 26.5.2,
  Xcode 26.6.
- No viewer was launched, driven, photographed or compared. Visual questions
  remain in the owner-attended daily queue.

## Implemented cutovers

1. `bdefc6b2d7` removes the avatar-impostor GL attachment/PBO fallback. A
   complete native bake moves its four private Metal textures into the retained
   per-avatar generation. Missing native output requests a fresh bake and makes
   the Metal world transaction incomplete; it does not import legacy pixels.
2. `60c9ee55f2` removes directional and projector `glGetTexImage` fallbacks.
   Consumers require complete native R32F array slices plus validity for every
   built cascade/channel. Fully unbuilt directional shadows retain the defined
   null-pack behavior.
3. `bc79f48aca` removes the hero-radiance framebuffer/readback/upload fallback.
   Consumers require the native cube's device, owner UUID, owner epoch,
   generation, format, dimensions and mip count to match the live hero owner.
4. `35082663e5` removes the blurred RG base-light-map bridge. Base lighting now
   requires the native directional-shadow/SSAO light-map target; encode failure
   leaves the native frame incomplete.

No global draw/traversal suppression was added. These changes remove duplicate
pixel crossings only; the retained legacy renderer still supplies unported
owners and remains the visible authority where the native exactness gates do
not publish.

## Static audit

Command:

```sh
python3 tools/metal/audit_gl.py --mode strict --json \
  --output artifacts/metal/gl-strict-pipeline-pixel-cutover.json
```

Expected exit status is 1. The audit scanned 2,408 files and reported 3,279
findings, down from 3,337 before this sequence. The 58-findings delta is 7 from
impostor publication, 16 from shadow depth, 27 from hero radiance and 8 from
base-light input. The same artifact reports zero `gl_function` findings for
`indra/newview/pipeline.cpp`. Shader/state/traversal findings, SDL/CGL context
ownership, the linked OpenGL dependency and the global strict gate remain open.

## Source-exact build and physical validation

The following command passed with warnings as errors at `35082663e5`:

```sh
AL_METAL_PRESET=ninja-os-arm64 \
AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 \
AL_METAL_CONFIG=RelWithDebInfo AL_METAL_JOBS=8 \
tools/metal/build.sh build
```

It produced Development build `26.4.0d68108` (`26.4.0.68108`).

- `tools/metal/verify_app.sh` checked 349 Mach-O slices; all were ARM64.
- Executable SHA-256:
  `10086bb6ef963c3168658a5d83a2bb208bae1728bfe3d30fd32dcf02189e47bc`.
- Source and packaged MSL SHA-256:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
- Packaged metallib SHA-256:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- The complete physical RelWithDebInfo Metal suite passed 52,190 assertions in
  52 cases under API and load-action validation. Existing cases cover direct
  impostor texture movement/replay, directional/projector shadow filtering,
  hero cube generation/LOD validation and native base-light composition.
- Harness validation passed and all 27 harness unit tests passed.

The earlier unfiltered 574-entry CTest result remains 568 passes, six legacy
GL-host failures and five skips. It was not rerun for this Darwin pipeline-only
sequence and is not represented as green.

## Next executable boundary

Trace the two GL/PBO face readbacks in `llreflectionmapmanager.cpp` against the
existing native reflection-probe face and radiance owners. Remove only a
source-proven duplicate crossing, preserving fail-closed generation semantics.

