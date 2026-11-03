# Native-eligible material-preview retained-light-state checkpoint — 2026-11-03

## Observed source state

Source checkpoint `88f1677884181e0b8b3524175dab76c545cb2183` further confines
retained GL preview state to the recoverable fallback callback. A successful
native-eligible Darwin PBR material preview no longer changes
`RenderLocalLightCount`, forces the default probe through retained uniform
updates, calls `setupHWLights`, or mutates GL light zero's position/sun-primary
state. The native batch already carries its direct light explicitly and reads
the coherent native default-probe mirror, so those retained mutations are not
part of its result. If native eligibility or submission fails, the callback
performs the original changes before the GL draw and restores them afterward.

The prior checkpoint remains in force: an eligible preview also binds no
auxiliary GL screen or GL PBR shader, issues no `pushGLTFBatch`, takes no GL
post target, runs no retained post/final blit and copies no GL framebuffer.
Non-Darwin behavior and unsupported-mode fallback remain unchanged.

This is still not complete preview GL elimination. Dynamic-texture `preRender`,
color/depth clear and masks, depth/stencil/scissor state, auxiliary render-pack
selection, GL viewport/projection/modelview setup and `gGLLastMatrix` reset are
still shared before native eligibility resolves. Classic, non-HDR,
post-suppressed and incomplete/failed-native configurations retain their GL
draw and post path. The retained bloom lifecycle remains reachable.

No viewer was launched, driven, photographed or compared. The existing single
material-preview row in the owner-attended daily queue was advanced to exact
build 68171; no new comparison row was created.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 earlier in this continuous session on the
  physical MacBookPro18,2 acceptance host: M1 Max, 64 GiB unified memory,
  arm64 macOS 26.5.2, Xcode 26.6, Apple7-family support and Metal compiler.
- `tools/metal/build.sh build` exited 0 at exact source
  `88f1677884181e0b8b3524175dab76c545cb2183`, using the Apple Silicon Xcode
  Release preset with warnings as errors. It produced Development build
  `26.4.0d68171` (`26.4.0.68171`).
- The exact-source Ninja RelWithDebInfo test targets rebuilt successfully. The
  separate RelWithDebInfo `appearance-utility-bin` target completed at that
  source with no work pending.
- Focused PBR material-preview coverage passed 201 assertions with Metal API
  and shader validation enabled before the source commit; the tested native
  renderer and shader inputs are unchanged by the retained-state source move.
- With the same validation layers enabled, complete exact-source
  `almetal_tests` passed 52,381 assertions across 55 cases.
- `alrender_tests` passed 557,312 assertions across 137 cases.
- The CPU-only `llrender_tests` binary passed all 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases on the
  physical acceptance host.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped. Compiled and runtime build numbers both report
  68171.
- The broad `llrender_gl_shared_tests` run passed 8,335/8,340 assertions and
  181/224 cases, with the established three frame-fence pacing/budget failures,
  one depth-format failure and 39 skips. Exit 42 remains recorded as failed.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `fd10040bf4fc8c110f9cbbfc8b45d8f569256da5ba8f3c126baa9ddf8435ab5c`.
- Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory remains 2,408 files / 3,262 findings and exits 1 as
  expected. Exact report SHA-256 is
  `589b98fbc15cee351f34a264f75341e80b762d9bdd071958f469bdfc67967019`;
  ignored report:
  `artifacts/metal/gl-strict-native-material-preview-light-state.json`.
- The viewer and appearance utility remain ARM64 executables with no undefined
  `CGLTexImageIOSurface2D`; both Apple OpenGL and Metal are still linked. The
  strict no-GL gate is not met.
- Initial harness/tool validation correctly failed because the updated
  `CURRENT.md` had grown to 165 lines, above the 160-line recovery limit. After
  compressing the snapshot without deleting archived evidence,
  `validate_harness.py` passed all 32 required files and all 27 tool tests
  passed.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Shared preview viewport/matrix/clear state,
unsupported preview modes, application-owned GL context/traversal/resources,
complete parity, prolonged stability, independent review, strict no-GL and
measured performance gates remain open.

## Next executable boundary

Build the native preview camera/modelview/projection/light snapshot directly on
the CPU and defer dynamic-texture GL pre-render, clear/mask/state setup and
matrix loads into the retained callback. A successful native submission should
not mutate the GL context before publication. Preserve the callback until every
unsupported mode has a physically validated native owner.
