# Developer menu native admission — 2026-11-11

Source audit compares original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`
with native62d99552aa and the subsequent integrated fixes. This is source
admission evidence, not live menu/feature parity. No viewer/menu/window or
screenshot was executed. Preserve all non-rendering menu behavior.

| Menu family | Observed native or CPU consumer |
| --- | --- |
| Render type/pool masks | Pipeline capture masks and hasRenderType retain ordered native pool selection. |
| Wireframe / RLVa permission | set_use_wireframe retains the RLVa gate; scene wireframe selects real Metal fill modes. |
| Bounds/normals/octree/occlusion/raycast/shadow frusta | renderDebug and typed capture owners feed actual native debug scene geometry. |
| Batch/update/texture animation/texel density/avatar joints/volumes/targets | Retained CPU producers and typed scene/debug captures; no GL program completeness gate. |
| Texture priority/area/face area/LOD/triangle/sculpt/particles/probe updates | Original CPU debug text reaches native HUD/text rendering. |
| Reflection probes/impostors/wind | Explicit native reflection-manager and pipeline captures. |
| UI/selection/dynamic textures/flexible geometry | Display UI, native selection and dynamic producers plus retained CPU flexible updates. |
| Shadows/SSAO/alpha/cloth/texture animation/full resolution/attached lights and particles/glow | Existing typed scene and CPU producer consumers remain. |
| Buffer visualization0–6 | Native G-buffer, actual spatial-meter and SMAA intermediate owners, in original post order. |
| Frame Profile | Actual Metal encoder timestamp window and pipeline/MSL identities; unavailable hardware counters stay unavailable. |
| Texture benchmark | Original fixed workload now owned by ALMetalTextureBenchmark with real visibility counts and completed GPU timing. |
| HDRI | Native texture publication into reflection-map ownership. |
| Next-run validation / driver crash | Metal validation selection precedes device creation; explicit crash path enters the native API. Neither action was invoked. |
| Far plane/axes/look-at/point-at/debug views | Native world overlay or ordinary UI line capture. |
| Fog | Retains original CPU updateFog semantics. |

## Concrete defects and completion state

Enable Shader Cache retained its control but lost its executable reader after
the displaced GL compiler/cache was removed. Original llviewershadermgr passed
RenderShaderCacheEnabled to initShaderCache and keyed binaries by app/source
identity. A direct device-owned Metal binary-archive port is being implemented
in `/private/tmp/alchemy-native-pipeline-cache-20261111`, with separate root
policy wiring in `/private/tmp/alchemy-native-cache-policy-20261111`.
Do not claim this menu functional until its actual persistence/hit/disable/
purge regressions and integration build pass. System Metal caches and the
bundled metallib are not app-owned cache-purge targets.

Randomize Framerate and Periodic Slow Frame inherited a real original bug:
callbacks changed unused globals while LLAppViewer's loop read cached settings
named `Randomize Framerate` and `Periodic Slow Frame`. c5261cecae binds existing
CheckControl/ToggleControl callbacks directly to those settings, removing the
orphan handlers;4ce389dd57 removes the dead globals. Normal loop timing/math is
unchanged. XML parses; actual Release callback syntax passes in
`artifacts/metal/feature-completion-20261111/timing-menu-syntax-final.log`.
The first attempted syntax check selected an obsolete Debug PCH and failed;
the corrected check explicitly selects Release and textual headers. No delay
action or viewer loop was run.

Primitive query plots emitted fabricated zero when the native backend had no
primitive counter. d397b548ca checks actual support; the real profiler-enabled
owner fails two baseline assertions and passes7/1 after repair. The separate
native water draw plot now counts actual accepted field/patch/haze encodes,
including replay, instead of reading the displaced GL increment.124aaec10a
collects an optional synchronous result only for an armed Tracy interval;
failed renders publish no partial count, fullscreen haze/compute are excluded.
Validated native water fixture passes31516assertions/1case, including13new
counter assertions, in native-water-draw-count-tests.log. Counts are encoded
geometry calls, not measured GPU execution or generated primitives.

ShowTangentBasis, GLTFNodes and Highlighted/FootShadows/FRInfo have no executable
consumer in either the original reference or audited native source. They are
inherited inert controls, not proven displaced native consumers. The registered
ReloadVertexShader callback has no XML entry and its helper was empty in both
sources. Do not invent original behavior or label these as verified functional.

## Evidence and remaining gates

Native image/label/profiler/draw filter passes67773assertions/47cases after
full dependent rebuild; wrapper regressions pass66108/18 llrender,552467/29
CPU water,33323/8 native water/typed compute/shadow-indirect assertions/cases.
Historical filters overlap; do not sum them into a new aggregate.
Receipts: `artifacts/metal/feature-completion-20261111/`.
Latest verified app remains696c43ba78/26.4.0d68677. Newer source requires a
frozen canonical build, imports, architecture and signature checks.
Full original feature parity, stability and release gates remain open.
