# Native Metal texture-pressure scheduler checkpoint

**Observed:** 2026-11-04 on the physical Apple M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `ebd07151a3` (`Route texture pressure through native Metal`)

## Replacement-first boundary

Fetched-texture lifecycle and pressure decisions now recognize actual GPU
residency rather than requiring a GL name. A Metal-authoritative image can
participate in shutdown precache, redundant-upload avoidance, post-create
downscale and `LLViewerLODTexture::scaleDown()` after its GL name retires.

On Darwin the pressure scheduler first examines the bounded current downscale
queue for images whose Metal texture is authoritative. Those images execute the
already-validated native `LLImageGL::scaleDown()` route directly. This path does
not create or bind a GL name, scratch target, copy shader, framebuffer, screen
triangle, blend state or color state.

GL and dual-backed images are requeued in their original relative order for the
existing fail-closed path. If the native scan consumes the time budget, the
scheduler returns before an unexamined native entry could fall through to GL.
The existing pending-flag lifecycle is preserved. This checkpoint implements
retained pressure-management behavior on Metal; it does not delete a feature or
globally disable transitional GL recovery.

## Exact build and validation evidence

- A cold Ninja Release warnings-as-errors build of
  `llrender_gl_fresh_tests alchemy-bin` exited 0, linked the complete viewer and
  packaged the app.
- With Metal debug/validation enabled, the focused physical
  `[LLImageGL][almetal]` slice passed 1,960 assertions across 19 cases; the
  complete renderer binary passed 2,671 assertions across 99 cases. The two new
  assertions prove a Metal-only `LLGLTexture` wrapper reports GPU residency and
  does not claim GL residency.
- `tools/metal/build.sh` exited 0 at committed source `ebd07151a3`. The cold
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68239` (`26.4.0.68239`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  `codesign --verify --deep --strict --verbose=2` passed.
- Viewer executable SHA-256:
  `f2660391e4bbc98632509ad3bbff106e57b7f8b162cd85e7ae7df229722bcbe1`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  `ALMetalWorldPresentation` remains default-on at `1`.
- The strict GL inventory was intentionally not changed: this slice adds a
  Metal implementation and keeps GL/dual-backed recovery until its remaining
  consumers have native replacements. Both OpenGL and Metal remain linked;
  static, linked and runtime no-GL release gates remain open.
- No viewer executable was launched, driven, photographed or compared.

## Next executable boundary

Make destruction backend-neutral only where the lifecycle contract is to drop
all GPU residency. Native-only sculpt storage must be evictable without
pretending it has a GL name; explicitly GL-only detach operations must remain
separate. Add physical lifecycle coverage before retiring any superseded owner.
