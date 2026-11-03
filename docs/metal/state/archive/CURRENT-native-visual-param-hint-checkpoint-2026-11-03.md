# Native visual-parameter hint feature checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `18a8b6924f` (`Make visual parameter hints Metal-native`)

## Implemented native feature owner

- Darwin `LLVisualParamHint` now schedules without a retained target, starts
  with native Metal backing and publishes a complete render through
  `LLImageGL::adoptMetalRenderTextureAuthoritative()`.
- CPU-only preview camera setup preserves the existing aspect, field of view,
  yaw/pitch and target calculations without mutating retained renderer state.
  The prior camera and viewport metadata are restored after every attempt.
- `LLPipeline::prepareMetalAvatarPreview()` performs the preview render-mask,
  visibility and state-sort work required to rebuild avatar and attachment
  draw metadata without issuing the old retained impostor draw.
- The native scene retains the selected system-avatar body meshes, joint
  skinning, cloth deformation, rigid/rigged attachments, attachment alpha
  ordering, legacy/PBR material modes, fullbright/mask behavior and preview
  background.
- Appearance hints keep their sRGB sampling descriptor while rendering through
  a compatible linear attachment view. This preserves the encoded display
  bytes produced by the preview shaders instead of applying a second sRGB
  store conversion.
- A complete result becomes the sole authoritative texture. Incomplete
  geometry, resources or publication leave the previous valid result intact
  and the hint dirty for retry.
- The Darwin background GL draw, GL camera transaction, depth/blend setup,
  `generateImpostor(..., true)` draw and framebuffer-copy publication were
  deleted after the Metal producer was established. `LLVisualParamReset`
  likewise has a targetless CPU reset path when instantiated.

This preserves the appearance-editor preview feature through Metal; it does
not remove body, clothing, attachment or material behavior to reduce the GL
inventory.

## Deterministic and physical evidence

- `tools/metal/doctor.sh` exited 0 on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 and Metal compiler
  available.
- `tools/metal/build.sh` exited 0 at committed source `18a8b6924f`. Its
  warnings-as-errors Xcode Release build recorded 175 cacheable tasks, linked,
  signed, validated and staged Alchemy Metal Development `26.4.0d68209`
  (`26.4.0.68209`).
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` exited 0 and checked 349/349 bundled Mach-O files as
  ARM64. `codesign --verify --deep --strict --verbose=2` exited 0.
- Viewer executable SHA-256:
  `70723d54a7afc5ed798d0683a22a430014e5476c797a41a00a11ad38a48ccf04`.
- Source and packaged `almetal_builtin.metal` are byte-identical at
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`;
  packaged metallib SHA-256 is
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- `env AL_METAL_PRESET=ninja-os-arm64 tools/metal/build.sh configure` exited 0
  at build 68209. The exact Ninja tree linked `llappearance_gl_tests`,
  `almetal_tests`, `llrender_gl_fresh_tests`, `alrender_tests`,
  `newview_tests` and `alchemy-bin` warnings-as-errors; the repeat build had no
  work and exited 0.
- With `MTL_DEBUG_LAYER=1`, `MTL_SHADER_VALIDATION=1`, load-action validation
  and `MTL_DEBUG_LAYER_ERROR_MODE=nslog`, `almetal_tests` passed 52,427
  assertions across 55 cases and explicitly reported Metal API/GPU validation
  enabled. The added physical case rejects rigged rendering to an sRGB target
  without a raw-storage view, then proves native geometry and background bytes
  are preserved through the compatible linear view.
- Under the same environment, `llappearance_gl_tests` passed 13,207 assertions
  across 3 cases and `llrender_gl_fresh_tests` passed 2,612 across 96.
- `alrender_tests` passed 557,312 assertions across 137 cases.
  `newview_tests` self-reported build 68209 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- `python3 tools/metal/validate_harness.py` exited 0, and
  `python3 -m unittest discover -s tools/metal/tests -v` passed all 27 tests.
  `git diff --check` exited 0.
- Source inspection confirms the former direct hint producer calls are absent.
  Exact packaged symbol inspection finds
  `LLVisualParamHint::renderWithoutTarget()`,
  `LLPipeline::prepareMetalAvatarPreview()`,
  `LLViewerCamera::setPerspectiveWithoutRenderState()` and authoritative Metal
  publication.

## Strict and binary boundary

- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-native-visual-param-hint-68209.json` exited 1 as
  required while findings remain: 2,408 files / 3,244 findings. Report SHA-256
  is `3a91085405bf2dad63efa126dd0723aba15226fb018b277a6829c9b9b1d55e68`.
- Relative to build 68207, both the raw count and all 2,933 unique
  path/kind/snippet tuples are unchanged. The removed direct drawing uses were
  not individually classified by the current audit patterns; no new classified
  GL use, audit rule or suppression was added.
- `otool -L` still finds both OpenGL and Metal. Linked/runtime no-GL and all
  remaining feature/parity gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared.
Appearance-editor hint presentation is queued in
`docs/metal/work/active/visual-review-queue.md` for an owner-attended daily
review. Physical GPU evidence proves resource, color-storage and publication
behavior, not final visual parity.

## Next executable action

Replace the retained-target producers for Darwin avatar-image and sculpted-image
upload previews with targetless authoritative Metal output. Preserve camera,
interaction, mesh/texture selection, lighting, alpha and retry behavior before
deleting their old GL draws.
