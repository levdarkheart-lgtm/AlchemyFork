# Native saved-output snapshot readback cutover checkpoint — 2026-11-02

## Observed source state

Commit `d1df906f44e8a79377a5b68f9fc3f58cfd0a6b3b` removes the final two
`gGL.readPixels` call sites from `LLViewerWindow::rawSnapshot`. The supported
snapshot layer enum contains color, scalar depth and packed depth24; ordinary
outputs use RGB, isolated cutouts use RGBA, and the existing native producer
already owns clean-plate/no-post, UI/HUD, single-pass and tiled output.

Every request now arms the exact native world-snapshot transaction before
rendering. An unsupported request, unavailable producer or failed tile returns
false through the common restoration path. A tiled failure leaves partial raw
storage private to the failed call, so callers cannot encode or publish it and
there is no whole-image GL rerender. Depth linearization and depth24 packing
remain unchanged after native D32F readback.

After the last production consumer disappeared, commit
`857493de1afda0a08147c2fdc6cee3b14ef6803a` removes the synchronous
`LLRender::readPixels` wrapper and its wrapper-only GL test. Other legacy
GL-only tests retain direct test reads where they still verify unrelated state.
Production source now has no `gGL.readPixels` callers.

Remaining direct production `glReadPixels` consumers are outside Snapshot:
`LLTexLayer::readBackAlpha` extracts avatar morph-mask alpha in the main
appearance library, and `LLBakingTexLayerSetBuffer::midRenderTexLayerSet` reads
RGBA in the separate appearance-baking utility. They remain explicit next
owners rather than being silently deleted or described as completed.

## Validation evidence

- The required warnings-as-errors RelWithDebInfo wrapper completed all 1,330
  header-invalidated actions at exact source
  `857493de1afda0a08147c2fdc6cee3b14ef6803a`, producing Development build
  `26.4.0d68119` (`26.4.0.68119`).
- With `MTL_DEBUG_LAYER=1`, shader validation disabled, load-action validation
  enabled and errors routed to `nslog`, the physical M1 Max Metal suite passed
  52,209 assertions across 52 cases.
- The source-exact `newview_tests` binary passed 98,973 assertions across 743
  cases: 742 passed and one platform CRT case skipped as expected.
- Focused `alrenderstate_gl` coverage passed 194 assertions across 12 cases
  after the dead wrapper-only case was removed.
- `tools/metal/verify_app.sh` passed 349/349 bundled Mach-O slices as ARM64.
- Executable SHA-256:
  `cd0cb8a1c24d3cf57de9579b973342c1c8d4218a7695352ed2e1338383b292ad`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,265 findings, expected exit 1. The exact
  ignored report is
  `artifacts/metal/gl-strict-native-snapshot-readback-cutover.json`.
- `otool -L` still reports both Apple OpenGL and Metal in the main executable.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; avatar morph-mask and appearance-utility bake readbacks
remain; complete parity, prolonged stability, independent build/review
evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Trace the main-viewer morph-mask and separate-utility bake readbacks from target
creation through consumers. Preserve exact subregions, RGBA-to-alpha behavior,
row orientation, cache lifetime and bake output while assigning each a native
Metal texture/readback owner before removing the direct GL calls.
