# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `PRODUCT.md`, not a demo. **Baseline:** pin actual fork HEAD in first session. **Evidence:** use `VALIDATION.md` and `PERFORMANCE.md`.

## Work board (Codex manages and expands)
| ID | Work | Paths / integration | Depends on | State | Evidence / next action |
| --- | --- | --- | --- | --- | --- |
| M-001 | Verify host/fork, reproduce ARM64 OSS build, run reference and record baseline | build preset, docs/state | none | doing | Release configure/build/login-screen smoke pass; 461/461 architecture-neutral tests pass; 349/349 bundled Mach-O files arm64. Next: authenticated feature/perf reference when authorized. |
| M-002 | Enumerate GL calls, shader/variant graph, pass/resource read/write graph, non-system runtime slices | `llrender`, `newview`, `alrender`, `llwindow` | source checkout | doing | 3,080 findings/135 files recorded with raw artifact hash and top owners. Next: deterministic shader variants and pass/resource edges. |
| M-003 | Native Metal drawable/presentation and app lifecycle | SDL3 macOS window, `llviewerdisplay` | M-001 | doing | Device/queue and tested SDL `CAMetalLayer` lifecycle implemented; viewer smoke selects Apple M1 Max and reaches login. Next: attach production surface and remove `SDL_GL_SwapWindow` after converted passes produce the drawable. |
| M-004 | Metal buffers, textures, render targets, uploads, readback, synchronization | `almetal`, `llrender`, `alrender` GPU resources | M-002 | doing | Implement typed resource descriptors, storage policy, upload/readback and frames-in-flight on the proven queue; extend physical-GPU tests. |
| M-005 | Native MSL and explicit pipeline/resource-binding system | shader source and manager | M-002, M-004 | todo | Correct shader permutations, captured outputs |
| M-006 | Existing UI/fonts/login, media texture ingress and HUD drawing through Metal | `llrender` fonts, `llui`, `newview` | M-003..005 | todo | Real viewer login/UI and media, no GL |
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

## Active integration / next action
Codex on M1 Max: M-001/M-002 have a locally proven reference. M-003 has production device/queue ownership and a tested surface abstraction, while the viewer still presents through its temporary GL migration context. Advance M-004 resources and wire the surface into the window lifecycle as soon as converted passes can own the drawable. Current architecture-neutral suite: 463/463 enabled tests green. Record branch, SHA, tests and errors at each checkpoint.

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
