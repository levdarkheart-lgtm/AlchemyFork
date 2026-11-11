# Architecture and direct-metal rewrite boundaries

This document is an **engineering hypothesis grounded in upstream source**, not authority over the checkout. Verify references at fork HEAD before changing code. Capture actual call graphs and scheduling contracts in work notes as you learn them.

## Retain from Alchemy
- `indra/llcommon`, `indra/llmath`: data types, threading, math, shared foundations.
- `indra/llcorehttp`, `indra/llmessage`: networking/Second Life protocol.
- `indra/llfilesystem`, `indra/llimage`: asset cache and CPU decode; OpenJPEG remains usable.
- `indra/newview` scene, camera, avatar, appearance, editing and viewer lifecycle logic **where not GPU-coupled**.
- `indra/llui`: UI model/event logic; change rendering integration only.
- existing audio, media, WebRTC, V-HACD and SDL3 input code, subject to tested boundaries.

## Reimplement as native Metal (no fallback)
| Source/area | Existing coupling | Required native behavior |
| --- | --- | --- |
| `indra/llwindow/llwindowsdl.cpp`, `llsdl_macos.mm` | SDL_GL 4.1 context, swap | SDL3 events with `CAMetalLayer`, drawable lifecycle, resize, HDR/color semantics, present/fences |
| `indra/llrender/llrender.cpp`, `llgl.cpp` | `gGL` mutable state, GL entrypoints, compute | Metal command encoding, state/pipeline resolution, per-frame lifecycle, explicitly ordered compute/render |
| `indra/llrender/llvertexbuffer.cpp`, `algpubuffer.cpp` | GL VBO, Apple-specific GL VBO pool | `MTLBuffer` allocation, upload, index/vertex layouts, persistent resource lifetime and heap use if beneficial |
| `indra/llrender/llrendertarget.cpp`, `llimagegl.cpp` | FBO, GL textures, binds | `MTLTexture`, attachment load/store, MSAA resolve, depth/stencil formats, readback and aliasing |
| `indra/llrender/llglslshader.cpp`, `llshadermgr.*` | GLSL variants, uniforms, `LLGLSLShader` | MSL shader libraries, pipeline state cache, argument/buffer/texture/sampler binding, variant and error diagnostics |
| `indra/newview/llviewershadermgr.cpp` and `app_settings/shaders/` | GLSL program assembly, shared snippets | Native `.metal` implementations plus deterministic variant/binding metadata; preserve algorithms, validate output |
| `indra/newview/pipeline.cpp`, `lldrawpool*.cpp`, `llviewerdisplay.cpp` | GL pass graph, render state, material/avatar draw calls | Direct Metal rendering and compute passes, PBR, avatar, transparent objects, probes, shadows, HDR and presentation |
| `indra/alrender/` | Resource/kernel policy plus retained transient prewarming; the duplicate GL depth-pyramid owner is removed | Retain pure policies; native owners control actual GPU allocation/commands and synchronization |
| `indra/llrender/llfontgl.cpp`, `indra/llui/` render call sites | Glyph/UI drawing using GL-style interfaces | Native Metal text/2D/widget/selection rendering without replacing widget logic |
| media texture and snapshot/picking paths | GL texture upload/readback | Metal texture ingress, CPU-GPU readback, color/alpha and synchronization correctness |

## Verified upstream observations (2026-10-24)
- `llwindowsdl.cpp` sets macOS SDL GL version 4.1 and `SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN`.
- `LLPipeline::shaderInputsFromSettings` gates GL compute behind GLSL >= 4.30.
- Water depth-pyramid build/cull is owned by `ALMetalWorldRenderer`; the dormant duplicate GL compute owner has been removed.
- `ALRenderResourcePlan` describes Main, Probe and Hero target packs. It is largely pure policy and may survive the backend replacement.
- `llvertexbuffer.cpp` has `LLAppleVBOPool` specifically for Apple OpenGL buffer handling; it is not a Metal resource manager.
- `pipeline.cpp` is large and GL-coupled; owning `LLRenderTarget` semantics or keeping method signatures is not proof a line-by-line substitution is valid.
- Upstream uses SDL3; **do not rewrite the entire UI in SwiftUI** unless a narrow integration specifically requires it.

## Current fork observation (2026-11-06)
- At `22def49efb`, Darwin creates a plain high-DPI SDL/Cocoa window, attaches
  the sole opaque `CAMetalLayer`, initializes only the native renderer and has
  no compiled window-object import for SDL/GL context APIs. The application
  still links OpenGL through other owned renderer objects; this is a completed
  lifecycle boundary, not the final no-OpenGL gate.

## Current fork observation (2026-11-11)

At `d2d0380309`, strict production source3508files has zero API blockers/errors.
The fresh ARM64 viewer and both owned media hosts have no direct forbidden
graphics imports/frameworks; all346bundled Mach-O files and strict signatures
pass. Foreign CEF/screen dependencies and runtime context creation remain
separate open gates. See [current evidence](work/active/native-finalization-and-media-closure.md).
Compatibility class/state names do not establish retained OpenGL execution.

The native finalizer owns exposure, lens, callbacks, scopes and typed SSR/
reference histories. Eight displaced fixed targets are retired. Pure
ALLightMapBlur kernel policy remains; the retained transient pool still
allocates unused Metal targets during prewarm and is being retired after its
live Bloom shape dependency is replaced. The separate ALMetalRenderTargetPool
remains a live native GPU owner.

## Key semantic traps
- GL state caches are global/implicit; Metal pipeline state and command buffers are explicit. Implement actual pass/state semantics, not a superficial GL-call shim.
- Framebuffer binds ≠ Metal render pass begin/end. Coordinate attachment lifetimes, load/store actions, memoryless attachments and barriers.
- Depth convention, clip space, face winding, framebuffer origin, color space/sRGB, MSAA, alpha behavior, tangent space, glTF materials and sampling must be verified.
- Render-to-texture (HUD, snapshots, mirrors, reflections) may use distinct camera projections, depth conventions and target packs.
- Compute may unlock paths absent on macOS GL, but the corresponding caller assumptions and synchronization must change too.
- Be precise about owned process versus embedded browser/media subprocess. Do not claim no GL system-wide just because the viewer rendering path is native.

## Development boundary
The *final fork* contains no fallback GL renderer. During development the clean upstream executable can run as an **external reference process**, not as a second backend in this fork. Codex is free to refactor data contracts **when necessary** for correctness; avoid speculative abstraction frameworks. Once the target render path is correct, remove dead GL code and build/link dependencies.
