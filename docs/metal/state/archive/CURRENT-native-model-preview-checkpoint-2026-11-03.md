# Native model-upload preview feature checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `b3f6124664` (`Make model upload previews Metal-authoritative`)

## Implemented native feature owner

- Darwin `LLModelPreview` now requests native Metal backing, schedules without a
  retained target and publishes complete output through authoritative Metal
  adoption.
- CPU camera scoping retains framing, yaw/pitch, pan and zoom without the old
  GL camera/matrix transaction.
- Static preview batches retain per-face diffuse material colors and available
  diffuse texture mirrors, optional edge overlays and depth-tested lighting.
- Physics preview retains triangle meshes, convex-decomposition hulls, explode
  transforms, depth-prepass/blended-hull behavior and ground reference.
- Rigged preview retains palette skinning, joint overrides, collision volumes,
  bones, joint bounds and continuous refresh.
- Degenerate-triangle reporting retains the legacy test threshold while drawing
  original-size geometry as 3-pixel red loops and 8-pixel red points. The
  shared Metal renderer now supplies screen-pixel wide-line and point pipelines
  instead of dropping these diagnostics.
- Clear-only output, UI-state updates and static/physics demand-driven refresh
  remain. A failed native attempt leaves the prior result authoritative and the
  preview pending for retry.
- `LLVertexBuffer::ScopedMetalOnlyUpload` is nestable and thread-local. Model
  preview buffer generation uses it so Metal stores receive the authored bytes
  without generating or binding GL array/index names.
- The Darwin retained draw and framebuffer-copy publication are bypassed only
  after the native producer owns the feature. Source census confirms every
  known `LLViewerDynamicTexture` subclass now has a targetless Darwin path;
  `LLModelPreview` was the final subclass.

This preserves model-upload functionality through Metal. It does not delete
static, physics, rigged or diagnostic modes merely to reduce OpenGL use.

## Deterministic and physical evidence

- `tools/metal/doctor.sh` exited 0 on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 and Metal compiler
  available.
- `tools/metal/build.sh` exited 0 at committed source `b3f6124664`. The
  warnings-as-errors Xcode Release build linked, signed, validated and staged
  Alchemy Metal Development `26.4.0d68215` (`26.4.0.68215`).
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` exited 0 and checked 349/349 bundled Mach-O files as
  ARM64. `codesign --verify --deep --strict --verbose=2` exited 0.
- Viewer executable SHA-256:
  `f2286f343e6dae25e090dbe18519ebec0fffecc4014dfffc82df0dddb09395f5`.
- Source and packaged `almetal_builtin.metal` are byte-identical at
  `6fcd3271ea0fed0a7dde4fec6e66ce71a8ec3fd1b43d4637f850ac8717af8110`;
  packaged metallib SHA-256 is
  `024aba5a70e01ce501c884535ad53f438d626d756c7c2148ee08a1a7a2aacc2d`.
- The exact Ninja tree reconfigured at build 68215 and linked
  `llappearance_gl_tests`, `almetal_tests`, `llrender_gl_fresh_tests`,
  `llrender_gl_shared_tests`, `alrender_tests`, `newview_tests` and
  `alchemy-bin` warnings-as-errors.
- With `MTL_DEBUG_LAYER=1`, `MTL_SHADER_VALIDATION=1`, load-action validation
  and `MTL_DEBUG_LAYER_ERROR_MODE=nslog`, `almetal_tests` passed 52,468
  assertions across 55 cases. The expanded lit-preview case physically covers
  wide-line and point pixel widths, clear-only execution and invalid-batch
  rejection in addition to its established geometry/texture/depth coverage.
- Under the same validation environment, `llappearance_gl_tests` passed 13,207
  assertions across 3 cases and `llrender_gl_fresh_tests` passed 2,612 across
  96.
- The focused shared-state test `llvertexbuffer_gl: a Metal-only upload fills
  native stores without creating GL names` passed 17 assertions. It verifies
  exact vertex/index bytes, absent GL names/bindings and nested scope lifetime.
- The complete `llrender_gl_shared_tests` diagnostic invocation exited 42 with
  8,352/8,357 assertions: 182 cases passed, 4 failed and 39 skipped across 225.
  Four failed assertions are in legacy/headless frame-fence timing cases; one
  is a retained depth-format expectation. The newly added upload case passed
  under the same environment. This failure is recorded and is not represented
  as a green gate.
- `alrender_tests` passed 557,312 assertions across 137 cases.
  `newview_tests` self-reported build 68215 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- `python3 tools/metal/validate_harness.py` validated 32 required files;
  `python3 -m unittest discover -s tools/metal/tests -v` passed 27 tests;
  `git diff --check` passed.
- Exact packaged symbol inspection finds `LLModelPreview::renderMetal()`,
  `LLModelPreview::renderWithoutTarget()`,
  `LLModelPreview::canRenderWithoutTarget()` and targetless `postRender()`.

## Strict and binary boundary

- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-audit-model-preview-cutover-strict.json` exited 1 as
  required while findings remain: 2,408 files / 3,247 findings. Report SHA-256
  is `4a4a348cce830a532e5f8ea3bccecb5a8c162c72747322e59366cdae815f433b`.
- The report contains 2,934 unique path/kinds/snippet records. The increase from
  3,244 findings / 2,933 unique records reflects new scoped-upload source/test
  vocabulary; no audit rule or suppression changed.
- `otool -L` still finds both OpenGL and Metal. Linked/runtime no-GL and all
  remaining feature/parity gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared.
Model-upload preview presentation is queued in
`docs/metal/work/active/visual-review-queue.md` for an owner-attended daily
review. Physical GPU evidence proves the bounded rendering/resource behavior,
not final visual parity.

## Next executable action

Delete the now-replaced non-Darwin/retained model-preview GL body and helpers
reachable only from it, then rerun source-exact build/tests and strict audit.
Continue with the next application-owned GL producer only after its complete
Metal replacement is implemented.
