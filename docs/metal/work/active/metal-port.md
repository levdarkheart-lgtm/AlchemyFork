# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `PRODUCT.md`, not a demo. **Baseline:** pin actual fork HEAD in first session. **Evidence:** use `VALIDATION.md` and `PERFORMANCE.md`.

## Work board (Codex manages and expands)
| ID | Work | Paths / integration | Depends on | State | Evidence / next action |
| --- | --- | --- | --- | --- | --- |
| M-001 | Verify host/fork, reproduce ARM64 OSS build, run reference and record baseline | build preset, docs/state | none | todo | First: doctor, configure, build, launch, test; store command/exit/SHA |
| M-002 | Enumerate GL calls, shader/variant graph, pass/resource read/write graph, non-system runtime slices | `llrender`, `newview`, `alrender`, `llwindow` | source checkout | todo | Inventory in docs, no unsupported counts |
| M-003 | Native Metal drawable/presentation and app lifecycle | SDL3 macOS window, `llviewerdisplay` | M-001 | todo | Real app launch/resize/present without GL context |
| M-004 | Metal buffers, textures, render targets, uploads, readback, synchronization | `llrender`, `alrender` GPU resources | M-002 | todo | Metal validation + focused tests |
| M-005 | Native MSL and explicit pipeline/resource-binding system | shader source and manager | M-002, M-004 | todo | Correct shader permutations, captured outputs |
| M-006 | Existing UI/fonts/login, media texture ingress and HUD drawing through Metal | `llrender` fonts, `llui`, `newview` | M-003..005 | todo | Real viewer login/UI and media, no GL |
| M-007 | World scene rendering and material correctness | draw pools, `pipeline.cpp`, shaders | M-003..005 | todo | Scene geometry, terrain, avatars, glTF/PBR, alpha |
| M-008 | Advanced views/effects and compute | shadows, water, sky, probes, mirrors, HDR, compute | M-007 | todo | Reference-equivalent captured frames and hazards |
| M-009 | Feature regression validation, stability and native-only dependency cleanup | tests, packaging, scripts | M-006..008 | todo | Matrix and strict native/GL gates passed |
| M-010 | M1 Max profiling and optimization, full benchmark evidence | perf tools/renderer/scene | working render | todo | Measured speed and no quality regression |

Rows encode technical dependencies, **not** user-supervised phases. Multiple independent rows may proceed concurrently with exclusive source ownership. Break down work as needed; update on real evidence, don't leave items indefinitely in `doing`.

## Recorded observations
- 2026-10-24: harness authored independently of fork; no local Mac build or performance run. Upstream observations in `REPO_FACTS.md` require fork verification.

## Active integration / next action
Codex on M1 Max: start M-001 and M-002 immediately; implement M-003/M-004 as soon as dependencies are established. Continue driving the full target across work items. Record current branch, SHA, tests, errors and file ownership at each handoff.

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
