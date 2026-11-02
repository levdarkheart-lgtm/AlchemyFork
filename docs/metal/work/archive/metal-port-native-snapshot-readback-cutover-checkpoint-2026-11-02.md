# Metal-port execution — native saved-output snapshots — 2026-11-02

## Outcome

Commit `d1df906f44` removes `rawSnapshot`'s last GL color/depth row fallbacks.
Color, depth, depth24, cutout, no-post, UI/HUD and tiled output now require the
established exact native producer; request or tile failure rejects the complete
snapshot rather than publishing partial or GL-owned pixels.

Commit `857493de1a` removes the now-dead synchronous
`LLRender::readPixels` wrapper. Production has zero `gGL.readPixels` callers,
and strict inventory falls by one. Direct avatar bake/morph-mask GL reads remain
as the next explicit owner boundary.

## Evidence

- Source-exact warnings-as-errors Development build `26.4.0d68119` passed at
  `857493de1afda0a08147c2fdc6cee3b14ef6803a`, completing all 1,330
  invalidated actions.
- Physical M1 Max Metal validation passed 52,209 assertions in 52 cases with
  API/load-action validation enabled.
- `newview_tests` passed 98,973 assertions across 743 cases with one expected
  platform skip; focused `alrenderstate_gl` passed 194 assertions/12 cases.
- Bundle verification passed 349/349 ARM64 Mach-O slices.
- Executable SHA-256:
  `cd0cb8a1c24d3cf57de9579b973342c1c8d4218a7695352ed2e1338383b292ad`.
- Strict inventory is 2,408 files / 3,265 findings, expected exit 1.
- The executable still links OpenGL and Metal; native-only product completion
  is not claimed.
- No viewer launch, snapshot or comparison occurred.

## Resume action

Map `LLTexLayer::readBackAlpha` and
`LLBakingTexLayerSetBuffer::midRenderTexLayerSet` to their targets, formats,
subregions, row orientation and process ownership. Add exact native readback
before removing either direct GL call.
