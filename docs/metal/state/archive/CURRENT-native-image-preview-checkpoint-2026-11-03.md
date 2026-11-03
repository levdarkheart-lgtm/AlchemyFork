# Native image-upload preview feature checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `7f3bbc58a0` (`Make image upload previews Metal-authoritative`)

## Implemented native feature owner

- Darwin `LLImagePreviewAvatar` and `LLImagePreviewSculpted` now schedule
  without retained targets, start with native Metal backing and publish complete
  renders through `LLImageGL::adoptMetalRenderTextureAuthoritative()`.
- A scoped CPU camera transaction restores the prior viewer camera, current
  camera and viewport metadata after every render attempt without issuing the
  old retained matrix/depth/blend transaction.
- The avatar producer keeps the selected body or clothing mesh, the actual
  preview `LLViewerTexture` Metal backing, a 15-joint scalar palette, preview
  lighting, transparent/opaque tint, camera controls and the existing alpha
  cutoff. `updateLOD()` prepares dirty mesh geometry before extraction.
- The sculpt producer keeps the sculpt-map-derived geometry, 1.25 model scale,
  camera controls and established three-light solid-preview convention.
- Both paths render through compatible linear attachment views while retaining
  sRGB sampling descriptors. This preserves the already encoded RGB bytes
  written by the former retained targets instead of applying a second sRGB
  conversion.
- The former targets were logical RGB. Surviving avatar fragments therefore
  publish alpha 1 after the existing 0.2 alpha test, preserving UI composition
  while retaining rejection of cutout pixels.
- Complete results become sole authoritative texture backings. Missing geometry,
  resources or publication leaves the previous valid result intact and the
  preview dirty for retry.
- The Darwin background GL draw, avatar/sculpt GL draw, retained camera/matrix
  setup and framebuffer-copy publication were deleted after native production
  was established. Non-Darwin retained behavior remains isolated.

This preserves avatar and sculpt image-upload preview functionality through
Metal; it does not disable those preview modes to reduce the GL inventory.

## Deterministic and physical evidence

- `tools/metal/doctor.sh` exited 0 on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 and Metal compiler
  available.
- `tools/metal/build.sh` exited 0 at committed source `7f3bbc58a0`. The
  warnings-as-errors Xcode Release build linked, signed, validated and staged
  Alchemy Metal Development `26.4.0d68211` (`26.4.0.68211`).
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` exited 0 and checked 349/349 bundled Mach-O files as
  ARM64. `codesign --verify --deep --strict --verbose=2` exited 0.
- Viewer executable SHA-256:
  `dbe8db4893d4c5cd647fc72b8b439dd3d55a79a5b4ab432a59b90c0ef93a0b64`.
- Source and packaged `almetal_builtin.metal` are byte-identical at
  `de17600b1fec7059e4b6417737d58dc6c47757b9ddee1f9d244096caa920c4a1`;
  packaged metallib SHA-256 is
  `f0832637526fad05fa96fe8aed5aa3216e2c88b5b09f9f74e71d8f352b13cda7`.
- The exact Ninja tree reconfigured at build 68211 and linked
  `llappearance_gl_tests`, `almetal_tests`, `llrender_gl_fresh_tests`,
  `alrender_tests`, `newview_tests` and `alchemy-bin` warnings-as-errors.
- With Metal API/GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,447 assertions across 55 cases. The image-preview
  case physically proves a linear storage view on an sRGB destination, exact
  background bytes, solid sculpt lighting, 15-joint avatar skinning, encoded
  source color without double conversion, alpha cutoff and opaque output for a
  surviving partially transparent source pixel.
- Under the same environment, `llappearance_gl_tests` passed 13,207 assertions
  across 3 cases and `llrender_gl_fresh_tests` passed 2,612 across 96.
- `alrender_tests` passed 557,312 assertions across 137 cases.
  `newview_tests` self-reported build 68211 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- `python3 tools/metal/validate_harness.py` and the 27 harness/tool tests pass;
  `git diff --check` passes.
- Exact packaged symbol inspection finds both targetless image-preview methods,
  `LLViewerCamera::setPerspectiveWithoutRenderState()` and authoritative Metal
  publication.

## Strict and binary boundary

- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-audit-image-preview-cutover-strict.json` exited 1 as
  required while findings remain: 2,408 files / 3,244 findings. Report SHA-256
  is `07b221b3fccbab07c30cb11c086711b9a4b519c1db67ab4271b13e52b47f3eaf`.
- The raw count and 2,933 unique path/kinds/snippet record count are unchanged
  from build 68209; one wrapper snippet changed with the refactor. The three
  remaining image-preview findings are wrapper/API references rather than
  direct retained producers; no audit rule or suppression changed.
- `otool -L` still finds both OpenGL and Metal. Linked/runtime no-GL and all
  remaining feature/parity gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. Avatar
and sculpt preview presentation is queued in
`docs/metal/work/active/visual-review-queue.md` for an owner-attended daily
review. Physical GPU evidence proves geometry/resource, lighting, alpha,
color-storage and publication behavior, not final visual parity.

## Next executable action

Replace the retained-target producer for Darwin `LLPreviewAnimation` with
targetless authoritative Metal output. Preserve playback, camera interaction,
system-avatar rigid/skinned meshes, cloth/alpha behavior, default texture,
publication and retry semantics before deleting its retained draw.
