# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `PRODUCT.md`, not a demo. **Baseline:** pin actual fork HEAD in first session. **Evidence:** use `VALIDATION.md` and `PERFORMANCE.md`.

## Work board (Codex manages and expands)
| ID | Work | Paths / integration | Depends on | State | Evidence / next action |
| --- | --- | --- | --- | --- | --- |
| M-001 | Verify host/fork, reproduce ARM64 OSS build, run reference and record baseline | build preset, docs/state | none | doing | Release configure/build/login-screen smoke pass; 463/463 architecture-neutral tests pass; 349/349 bundled Mach-O files arm64. Next: authenticated feature/perf reference when authorized. |
| M-002 | Enumerate GL calls, shader/variant graph, pass/resource read/write graph, non-system runtime slices | `llrender`, `newview`, `alrender`, `llwindow` | source checkout | doing | 3,080 findings/135 files recorded with raw artifact hash and top owners. Next: deterministic shader variants and pass/resource edges. |
| M-003 | Native Metal drawable/presentation and app lifecycle | SDL3 macOS window, `llviewerdisplay` | M-001 | doing | Device/queue and tested SDL `CAMetalLayer` lifecycle implemented; viewer smoke selects Apple M1 Max and reaches login. Native UI currently renders to a shadow target. Next: bind it to the production drawable and remove `SDL_GL_SwapWindow` after the converted login pass is visibly equivalent. |
| M-004 | Metal buffers, textures, render targets, uploads, readback, synchronization | `almetal`, `llrender`, `alrender` GPU resources | M-002 | doing | Core resource policy, transfers, attachments and frame scheduling are GPU-tested. Production `LLVertexBuffer` and supported 2D `LLImageGL` owners now maintain native mirrors, including mip/BC/swizzle/threaded-upload paths; smoke proves real allocations. Next: render-target ownership, framebuffer-derived mutations and transient pooling. |
| M-005 | Native MSL and explicit pipeline/resource-binding system | shader source and manager | M-002, M-004 | doing | Runtime/bundled MSL library, immutable render/compute/depth/sampler states and typed render/compute encoders execute on M1 Max with readback. Production UI/solid pipelines and samplers are cached from live viewer state; fan/loop remapping is native. Next: complete binding census and world shader variants. |
| M-006 | Existing UI/fonts/login, media texture ingress and HUD drawing through Metal | `llrender` fonts, `llui`, `newview` | M-003..005 | doing | Live immediate and captured UI/font batches encode through Metal into an offscreen shadow target under API/GPU validation. Next: drawable presentation, complete login composition/glyph evidence, then remove GL UI presentation. |
| M-007 | World scene rendering and material correctness | draw pools, `pipeline.cpp`, shaders | M-003..005 | todo | Scene geometry, terrain, avatars, glTF/PBR, alpha |
| M-008 | Advanced views/effects and compute | shadows, water, sky, probes, mirrors, HDR, compute | M-007 | todo | Reference-equivalent captured frames and hazards |
| M-009 | Feature regression validation, stability and native-only dependency cleanup | tests, packaging, scripts | M-006..008 | todo | Matrix and strict native/GL gates passed |
| M-010 | M1 Max profiling and optimization, full benchmark evidence | perf tools/renderer/scene | working render | todo | Measured speed and no quality regression |

Rows encode technical dependencies, **not** user-supervised phases. Multiple independent rows may proceed concurrently with exclusive source ownership. Break down work as needed; update on real evidence, don't leave items indefinitely in `doing`.

## Recorded observations
- 2026-10-24: harness authored independently of fork; no local Mac build or performance run. Upstream observations in `REPO_FACTS.md` require fork verification.
- 2026-10-24: physical M1 Max reference Release builds and reaches `STATE_LOGIN_WAIT`; runtime is macOS GL 4.1 with no compute. Full-bundle arm64 and signature checks pass. See `docs/metal/state/reference-baseline-2026-10-24.md`.
- 2026-10-24: macOS ignored the documented disposable-profile environment variable. Added parity with Linux before continuing smoke/validation runs; the successful second run kept logs, settings, and cache under `/tmp` and exited normally.
- 2026-10-24: added strict built-in Apple Silicon device selection, one production command queue, command-buffer status/error handling, and an SDL3 `CAMetalLayer` lifecycle. Physical-GPU tests prove blit, render/readback and drawable presentation; the integrated viewer selects Apple M1 Max, reaches `STATE_LOGIN_WAIT`, and exits normally.
- 2026-10-24: added typed Metal buffers, textures and multi-attachment render targets; shared/private/memoryless storage policy; aligned private upload/readback; explicit load/store/MSAA resolve; and a three-frame scheduler that returns abandoned slots and accounts completion failures. M1 Max tests cover adversarial row pitches, transient MSAA/depth and frame-slot exhaustion.
- 2026-10-24: added native MSL library loading/runtime compilation, immutable render/compute/depth/sampler state and typed command encoders. M1 Max executes and verifies compute and raster pipelines; the first UI and solid-color shader math is translated. Offline `xcrun metal` remains optional because this Xcode lacks the separately downloadable Metal toolchain component.
- 2026-10-24: wired the production `LLVertexBuffer` owner to the Metal device. macOS vertex and index stores receive native shared `MTLBuffer` mirrors at allocation and upload; isolated viewer smoke proved a real 114,688-byte mirror, reached `STATE_LOGIN_WAIT`, exited 0, and logged no Metal allocation/upload failure.
- 2026-10-24: wired the production `LLImageGL` owner to private Metal texture mirrors at its existing full/partial upload boundaries. The mapping covers BC1/2/3, sRGB, normalized/integer/float storage, legacy alpha/luminance swizzles, RGB expansion, byte swapping, CPU-provided/generated mips, and off-thread publication. GPU tests prove swizzle sampling, generated mips and 7x5 BC1 transfers. Isolated smoke `/tmp/alchemy-metal-texture.iqImrW` reached `STATE_LOGIN_WAIT`, exited 0, emitted exactly one production texture activation marker and no unsupported-format or Metal allocation/upload warnings. The complete 463-test architecture-neutral suite remains green.
- 2026-10-24: added the first live production draw conversion. `LLRender` now mirrors immediate and deferred UI/font batches into a one-encoder native frame with cached pipeline/sampler state, exact production SoA offsets, retained image/sampler identity, matrices, viewport/scissor, blend/color-mask state, text shadows and solid color. MSL remaps triangle fans and line loops without transient buffers. The physical-GPU test verifies a textured production-layout batch by BGRA readback. Final isolated smoke `/tmp/alchemy-metal-ui-final.MtjPen` ran with Metal API/GPU validation, encoded a real 1-batch/6-vertex UI frame, reached `STATE_LOGIN_WAIT`, exited 0, and logged no native/validation failure. The complete 463-test architecture-neutral suite remains green; 349/349 Mach-O files are arm64 and deep strict signing passes. This output remains an offscreen shadow until drawable presentation replaces the visible GL UI path.

## Active integration / next action
Codex on M1 Max: M-001/M-002 have a locally proven reference. M-003 has production device/queue/frame ownership and a tested surface abstraction, while visible presentation still uses the temporary GL migration context. M-004's core resource and synchronization primitives are GPU-proven; production vertex/index and supported 2D image ownership have migrated in parallel with temporary GL uploads. M-005/M-006 now encode live production UI/font batches through cached Metal state into an offscreen shadow target. Next bind that renderer to the production drawable, prove complete visible login/glyph composition, and retire GL swap for the bootstrap/UI path; then move framebuffer-derived texture mutations with native render targets. Current architecture-neutral suite: 463/463 enabled tests green. Record branch, SHA, tests and errors at each checkpoint.

## Risks to monitor
- GL state semantics and Metal render-pass lifetimes;
- shader variant/binding consistency, PBR visual parity;
- UI/font bootstrap before full world rendering;
- Cocoa/SDL3 event/presentation interaction;
- WebRTC/CEF/VLC/media texture and readback;
- external third-party ARM64 slices, distinct viewer identity;
- scene CPU bottlenecks hiding Metal gains;
- agent edits colliding in `pipeline.cpp`/shader manager;
- stale running session losing GPU evidence or intent.
