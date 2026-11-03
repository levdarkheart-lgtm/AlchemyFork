# Targetless scheduler cutover checkpoint — 2026-11-03

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer commit:** `74a7338a44fcdd84967079e9dec4a8c19d79296f`

## Product movement

`LLViewerDynamicTexture::updateAllInstances()` no longer returns before all
work merely because retained OpenGL is disabled. Dynamic textures now declare
whether the scheduler may ask them for a targetless transaction in that state.
The base contract is false; the Darwin PBR material preview opts in and may
execute its complete Metal geometry/post/publication path.

Retained-only items are skipped before their `needsRender()` methods can inspect
retained renderer state. If an opted-in targetless attempt reports
`RETAINED_FALLBACK` while GL is unavailable, it remains pending without shader
or vertex-buffer unbind, transient-target acquisition/bind/clear, retained
callback, or GL flush. The scheduler also no longer treats a targetless
completion as a dirty-GL-depth result: only a successful retained last-order
render makes the display caller clear GL depth.

Normal GL-available behavior is preserved, including same-frame retained
fallback and the preceding demand-driven resource plan. This is not evidence
that the full viewer already runs after `LLViewerWindow::stopGL()`: that current
teardown destroys broader pipeline and texture state as well as setting
`mIsDisabled`. The exact no-context runtime gate remains open.

## Source changes

- `LLViewerDynamicTexture::canRenderWithoutTarget()` defaults to false and
  documents the no-retained-transaction scheduling contract.
- Darwin `LLGLTFPreviewTexture` opts into targetless scheduling. Other platforms
  and all retained-only dynamic textures keep the default.
- The scheduler snapshots retained availability. Without it, non-opted-in
  textures are skipped before `needsRender()` and the on-demand target is
  constructed with no descriptor, preventing accidental acquisition.
- A targetless `PENDING` result or unavailable retained fallback performs no GL
  state or target transaction. A targetless `COMPLETED` result increments render
  accounting but not the retained last-order return value consumed by
  `llviewerdisplay.cpp`.
- With retained GL available, the original callback, post-render, flush and
  last-order return behavior remain unchanged.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and the Metal compiler.
- An incremental warnings-as-errors Ninja build first compiled and staged the
  complete viewer after the scheduler edit. `tools/metal/build.sh build` then
  exited 0 at exact source `74a7338a44fcdd84967079e9dec4a8c19d79296f`.
  The warnings-as-errors Apple Silicon Xcode Release build produced Development
  build `26.4.0d68181` (`26.4.0.68181`), linked, signed, validated and staged.
- Exact-source Ninja RelWithDebInfo test targets rebuilt successfully. The
  separate warnings-as-errors appearance-utility configuration reported build
  68181 and its target had no work pending.
- Focused lit/PBR material-preview GPU coverage passed 426 assertions across
  two cases with Metal API and shader validation enabled.
- With those validation layers, full `almetal_tests` passed 52,381 assertions
  across 55 cases. `alrender_tests` passed 557,312 assertions across 137 cases.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases.
  `llrender_tests` passed all 1,513 assertions across 203 cases: 202 passed and
  the established single-palette font case skipped.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped. Compiled and runtime build numbers both
  reported 68181.
- `llrender_gl_shared_tests` reproduced the known non-gating exit 42: 8,335 of
  8,340 assertions passed; 181 of 224 cases passed, four failed and 39 skipped.
  The failures remain three frame-fence pacing/budget cases and one depth-format
  case. This run is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` exited 0 and checked 349/349 bundled Mach-O slices as
  ARM64.
- Viewer executable SHA-256:
  `e7a73175e7edb851ab51862680dc5db07d10c4a1880fb41cceeba98becf9c8ef`.
  Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-targetless-scheduler-cutover.json` exited 1 as
  expected at 2,408 files / 3,262 findings. Report SHA-256 is
  `00d70a892b0de47b76ccc7996990f52d77c34ab4727d7bab6c60e7e279c200eb`.
- `file`, `otool -L` and `nm -u` confirm the primary binaries are arm64, the
  viewer has no undefined `CGLTexImageIOSurface2D`, and Apple OpenGL and Metal
  remain linked. The strict no-GL gate is not met.
- Exact-source inspection confirms the removed `mIsDisabled` early return, the
  default-false/Darwin-preview-true capability pair, the pre-`needsRender()`
  skip and the unavailable-retained fallback guard. The GL-disabled branch was
  not presented as a full-pipeline runtime test.
- No viewer executable was launched, driven, photographed or compared. Visual
  material-preview questions remain in owner-attended queue row 17, advanced
  only to exact build 68181.

## Next executable boundary

Split Darwin native-preview destination creation from the dynamic-texture
base's eager `generateGLTexture()` call. Successful native previews should own
only the logical/Metal destination, while an explicit retained fallback creates
its GL backing lazily and remains fail closed. Then widen native eligibility and
separately preserve native pipeline lifetime across full GL teardown before
claiming runtime no-context operation.

The product remains incomplete. Unsupported preview modes, other dynamic
textures, application-owned GL context/traversal/resources, complete parity
and stability, paired profiling, independent review and strict release gates
remain open.
