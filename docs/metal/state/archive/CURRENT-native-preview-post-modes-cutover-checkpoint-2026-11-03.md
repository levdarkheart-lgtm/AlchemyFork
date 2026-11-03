# Native material-preview post-mode cutover checkpoint — 2026-11-03

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer commit:** `f5513b7d3fc839847262d1e855a8b5af6248dfe9`

## Product movement

Modern Darwin PBR material previews no longer fall back to retained OpenGL
merely because HDR is disabled or post processing is suppressed. The native
renderer now names whether exposure/tonemapping is enabled, applies the legacy
gamma curve when the active sky requires it, and reproduces non-HDR 8-bit
dithering in encoded display space.

HDR post-suppression disables preview exposure, tone mapping, bloom, chromatic
aberration and metering rather than switching renderers. Non-HDR preview output
likewise skips exposure/tone mapping and bloom, performs the ordinary display
transfer, and applies the current static/animated dither contract. The output
works for ordinary RGBA8 storage and sRGB attachments without double-encoding.

Classic-mode lighting remains deliberately ineligible: its retained shader has
a distinct compile-time atmospheric/direct-lighting model that this checkpoint
does not approximate. Native failures and incomplete textures, geometry,
environment or destination state also retain the explicit fail-closed fallback.
Consequently retained preview bloom/program reachability is narrower but not
dead and must not yet be removed.

## Source changes

- `ALMetalPBRPreviewScene` now carries explicit tonemap enablement, legacy-gamma
  exponent, dither amount/animation state and frame identity.
- The native post shader bypasses exposure and tone mapping when requested,
  applies legacy gamma after the display transfer, and shares the production
  triangular dither implementation. An sRGB render attachment converts the
  final encoded value back to linear for hardware re-encoding.
- The renderer suppresses bloom, exposure metering and chromatic displacement
  whenever the tonemap/post transaction is disabled. Invalid gamma and dither
  parameters fail closed.
- `LLGLTFPreviewTexture` now permits modern non-HDR and suppressed-post scenes,
  maps the active sky and dither controls into the native scene, and keeps only
  `LLRender::sClassicMode` as a mode-level eligibility exclusion.
- Physical GPU regression coverage proves exposure bypass, legacy-gamma output,
  deterministic dither bounds/channel preservation and invalid-state rejection.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and the Metal compiler.
- An incremental warnings-as-errors Ninja compile/link and built-in MSL compile
  passed before commit. `tools/metal/build.sh build` then exited 0 at exact
  source `f5513b7d3fc839847262d1e855a8b5af6248dfe9`. The warnings-as-errors
  Apple Silicon Xcode Release build produced Development build `26.4.0d68185`
  (`26.4.0.68185`), linked, signed, validated and staged.
- Exact-source Ninja RelWithDebInfo test targets rebuilt successfully. The
  separate warnings-as-errors appearance-utility target rebuilt and staged at
  build 68185.
- Focused lit/PBR material-preview GPU coverage passed 447 assertions across
  two cases with Metal API and shader validation enabled.
- With those validation layers, full `almetal_tests` passed 52,402 assertions
  across 55 cases. `alrender_tests` passed 557,312 assertions across 137 cases;
  its focused resource-plan matrix passed 3,376 assertions across 37 cases.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases.
  `llrender_tests` passed 1,513 assertions across 203 cases: 202 passed and the
  established single-palette font case skipped.
- `newview_tests` passed 98,973 assertions across 743 cases: 742 passed and one
  platform CRT case skipped. Compiled and runtime build numbers were 68185.
- `llrender_gl_shared_tests` reproduced known non-gating exit 42: 8,335 of
  8,340 assertions passed; 181 of 224 cases passed, four failed and 39 skipped.
  The failures remain three frame-fence pacing/budget cases and one depth-format
  case. This run is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` exited 0 and checked 349/349 bundled Mach-O slices as
  ARM64.
- Viewer executable SHA-256:
  `41eb926793007e4f5864f7dc65bb48740cebc7726a62fb60221261235ecb9a8a`.
  Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `177f5548a11f65cd33c6a5fcaa268a5267eb9ec83b3610a7d7ce722771967574`;
  packaged metallib SHA-256 is
  `4cfc9ab99b6d500abfa0b8e76297d3cabed87b75a5efe11b34cf1634318662c3`.
- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-native-preview-post-modes-cutover.json` exited 1 as
  expected at 2,408 files / 3,262 findings. Report SHA-256 is
  `101f742754564f2c9c8a7b394e58df271ccf2da025eb1cb87d5da2663ef8e32c`.
- `file`, `otool -L` and `nm -u` confirm the primary binaries are arm64, the
  viewer has no undefined `CGLTexImageIOSurface2D`, and Apple OpenGL and Metal
  remain linked. The strict no-GL gate is not met.
- Source inspection confirms modern non-HDR/post-suppressed configurations no
  longer participate in the eligibility gate; classic mode and incomplete or
  failed native state still take retained fallback.
- `python3 tools/metal/validate_harness.py` validated all 32 required harness
  files. `python3 -m unittest discover -s tools/metal/tests -v` passed all 27
  harness/tool tests.
- No viewer executable was launched, driven, photographed or compared. Visual
  material-preview questions remain in owner-attended queue row 17, advanced
  only to exact build 68185.

## Next executable boundary

Implement the retained classic-mode material-preview atmospheric, direct-light
and probe conventions in the native shader with independent physical reference
tests. When classic mode becomes native and failure-only retained fallback is
the sole remaining path, separate recovery policy from production reachability
before removing retained preview bloom/program resources.

The product remains incomplete. Classic preview lighting, failure recovery,
other dynamic textures, application-owned GL context/traversal/resources,
complete parity and stability, paired profiling, independent review and strict
release gates remain open.
