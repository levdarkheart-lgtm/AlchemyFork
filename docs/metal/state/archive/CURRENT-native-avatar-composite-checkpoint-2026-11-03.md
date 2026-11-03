# Native avatar-composite feature checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `e4d5c187a9` (`Make avatar composites Metal-authoritative`)

## Implemented native feature owner

- `LLViewerTexLayerSetBuffer` schedules targetlessly on Darwin and executes the
  existing layer graph inside `ALMetalUIRenderer` capture instead of requiring
  a retained GL render target.
- The capture consumes the same layer geometry, texture selection, sampler,
  uniform color, blend factors and minimum-alpha threshold. Its scoped state
  suppresses GL drawing, shader and texture side effects while preserving the
  renderer's feature decisions.
- Static alpha assets and processed alpha masks publish native Metal textures.
  Processed alpha retains multiply, domain and cache semantics.
- Morph-mask generation reads alpha from the live in-progress Metal capture,
  applies it to the avatar morph targets and resumes encoding. The readback
  supports an sRGB output descriptor backed by linear storage so encoded layer
  color and alpha remain coherent.
- A successful composite publishes the Metal render texture through
  `LLImageGL::adoptMetalRenderTextureAuthoritative()` as sole backing. Failure
  leaves any prior valid result intact and the update dirty for retry.
- Capture state is restored on every exit. Publication occurs only after all
  layers and morph masks succeed.

This preserves the avatar composite/bake feature through Metal; it does not
delete clothing, skin, alpha or morph behavior to reduce the GL inventory.

## Deterministic and physical evidence

- `tools/metal/build.sh` exited 0 at committed source `e4d5c187a9`. Its
  warnings-as-errors Xcode Release build processed 1,093 cacheable tasks,
  linked, signed, validated and staged Alchemy Metal Development
  `26.4.0d68207` (`26.4.0.68207`).
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` exited 0 and checked 349/349 bundled Mach-O files as
  ARM64. `codesign --verify --deep --strict --verbose=2` exited 0.
- Viewer executable SHA-256:
  `b66c59b1da4aff162c16cef62ca4b7d483f5ef5b3c1f4808da3827b6003f3357`.
- Source and packaged `almetal_builtin.metal` are byte-identical at
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`;
  packaged metallib SHA-256 is
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- `env AL_METAL_PRESET=ninja-os-arm64 tools/metal/build.sh configure` exited 0
  at build 68207. The exact Ninja tree linked `llappearance_gl_tests`,
  `almetal_tests`, `llrender_gl_fresh_tests`, `alrender_tests`,
  `newview_tests` and `alchemy-bin` warnings-as-errors.
- With `MTL_DEBUG_LAYER=1`, `MTL_SHADER_VALIDATION=1`, load-action validation
  and `MTL_DEBUG_LAYER_ERROR_MODE=nslog`, `almetal_tests` passed 52,414
  assertions across 55 cases and explicitly reported Metal API/GPU validation
  enabled. Coverage includes mid-transaction morph-alpha readback and encoder
  resumption for sRGB output using linear capture storage.
- Under the same environment, `llappearance_gl_tests` passed 13,207 assertions
  across 3 cases and `llrender_gl_fresh_tests` passed 2,612 across 96.
- `alrender_tests` passed 557,312 assertions across 137 cases.
  `newview_tests` self-reported build 68207 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- `python3 tools/metal/validate_harness.py` exited 0, and
  `python3 -m unittest discover -s tools/metal/tests -v` passed all 27 tests.
  `git diff --check` exited 0.
- Exact packaged symbol inspection finds
  `LLViewerTexLayerSetBuffer::renderWithoutTarget()`,
  `LLImageGL::adoptMetalRenderTextureAuthoritative()` and
  `ALMetalUIRenderer::readFrameColorRGBA8()`.

## Strict and binary boundary

- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-native-avatar-composite-68207.json` exited 1 as
  required while findings remain: 2,408 files / 3,244 findings. Report SHA-256
  is `52bf624a5d7bb582a4705bc35a2d354a00cdf8c55b8265a4f05bd81e04956b27`.
- Relative to build 68205, 13 unique capture-state/resource references were
  added and 2 older function signatures were replaced, for net +11. These are
  legacy `LLImageGL`/`LLGLSLShader` types at the translation boundary, not new
  GL drawing. No audit rule or suppression changed.
- `otool -L` still finds both OpenGL and Metal. Linked/runtime no-GL and all
  remaining feature/parity gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. Avatar
composite/bake presentation is queued in
`docs/metal/work/active/visual-review-queue.md` for an owner-attended daily
review. Physical GPU evidence proves capture/readback/publication behavior, not
final visual parity.

## Next executable action

Replace Darwin `LLVisualParamHint`/`LLVisualParamReset` retained-target and GL
impostor preparation with targetless state preparation and authoritative Metal
lit-preview output while retaining camera, body/clothing/attachment,
alpha/material and retry behavior.
