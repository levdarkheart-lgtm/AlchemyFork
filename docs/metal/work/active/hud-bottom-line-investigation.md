# PPD-016 — lower HUD line while translating its position

Owner reports2026-11-12 while testing the accepted Water Haze Fix candidate
(sourcea6f4e8ff41, launchedPID49892). Three cropped owner screenshots21:50:15,
21:50:51 and21:51:50 show a thin black line below the compact blue HUD controls.
It moves with the HUD; at some positions it spans the full frame, at others
its middle disappears leaving two end segments. Edit selection outlines a
large mostly transparent rectangular frame whose lower edge aligns with it.
This establishes the report, not a new viewer-rendering cause. Owner says it
could be the HUD itself. The suggested above-horizon absence was explicitly
withdrawn; do not use it as a constraint or derive world-depth attribution.

Private artifacts/metal/hud-bottom-line-20261112/ retains all three PNGs with
SHA256s in owner-screenshots.json. No upload, new capture or viewer controls.

## Original/native contract audit

Original1eb44bbd fullbrightV/F and native HUD vertex/fragment agree on MVP,
texture matrix, raw display-encoded tint, texturealpha*vertexalpha and original
.004 alpha cutoff. Ordinary HUDs deliberately use AnisoWrap in both versions;
forced double-sided particles use AnisoClamp. A blanket clamp/raised cutoff
would change baseline semantics without proving this report.

Current captureMetalHUDBatch(lldrawpool.cpp561) sends indexed geometry to
LLRender::drawMetalHUDIndexedBatch; AnisoWrap selection is near727.
ALMetalUIRenderer::drawHUDIndexed(almetaluirenderer.cpp2168) preserves live blend/
depth/sampler state, explicitly sets backface culling/winding and draws triangles.
No stale wireframe fill-mode mutation found in this renderer. The fullbright
fragment uses alpha-product cutoff only for the alpha pool; opaque fullbright
intentionally does not discard. Texture wrap/filter and opaque edge-on side
geometry therefore need to be distinguished using actual face inputs.

Existing actual indexed-HUD GPU coverage is inside test
"almetal: production UI renderer consumes the viewer SoA layout" near
almetal_gpu_test.mm33614/34495. It covers tint/alpha products, transforms, winding,
particle double-sided handling and compressed-sRGB views, but not exact live
HUD geometry/atlas mip borders and fractional-pixel edge translation.

## Next evidence

Asked the owner whether it appears in original Alchemy or Firestorm; answer pending.
A nearly edge-on real side face may legitimately gain/lose subpixel coverage
as it moves. Alternatively a front quad could sample nonzero atlas/mip alpha
at a boundary. Neither attribution is proved by the screenshots. If native-only,
record offending batch/face render pool, texture identity, projected triangle
area/height and UV bounds, then replay its exact vertices/matrix across pixel
phases with original alpha/sampler policy. No speculative renderer fix yet.
