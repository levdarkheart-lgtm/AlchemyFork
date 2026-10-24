# Product contract — Alchemy Metal

## Goal
Deliver a usable, performant, self-built Second Life viewer derived from Alchemy, **native ARM64 macOS with native Metal rendering**, using Codex/frontier models as the principal engineering implementation resource. Preserve the existing functionality of Alchemy's **open-source** macOS build. The owner should not have to direct each subsystem or approve a sequence of experimental phases.

## Immutable constraints
1. **Platform:** arm64-apple-darwin only; M1 Max / 64 GiB is the primary physical acceptance device. Support later Apple Silicon by capability testing rather than arbitrary family assumptions. No x86_64 binary slices in our application-managed executables/libraries.
2. **Rendering:** Metal APIs (`MTLDevice`, command queues/encoders, `CAMetalLayer`, MSL, Metal compute) directly; no application-owned OpenGL, Vulkan, MoltenVK, ANGLE, Rosetta, translation-based graphics, or shipping fallback backend. Prefer removing obsolete GL implementation once displaced.
3. **Application:** retain Alchemy's Second Life connectivity, inventory, outfits, avatars, rigging, animation, attachments, PBR/glTF materials, terrain, water, sky, shadows, alpha/transparency, mirrors/probes where supported, selection/editing, uploads, UI, fonts, media, audio, voice, scripting/interaction and logout/recovery. Test and document *specific* exceptions rather than silently dropping them.
4. **Source/legal:** a separately identified LGPL-compliant fork using documented open-source dependencies. Do not use or fabricate proprietary SDKs or private Alchemy registry credentials. Existing open-source build is the functionality reference; proprietary-only feature parity is not assumed. Honour Second Life Third Party Viewer Policy and avoid implying official affiliation.
5. **Efficiency:** achieve real measured advantages on representative workloads. Use Apple GPU tile-based architecture, explicit resource residency, compute, and unified-memory-aware transfers where appropriate; avoid unnecessary CPU copies, pipeline recompilation, full-frame synchronization and oversubscription. Performance must include quality and functional correctness.
6. **Scope discipline:** no general cross-platform graphics engine, dual backend, unrelated application rewrite or speculative Neural Engine integration. Existing SDL3 event/windowing is acceptable with native Metal presentation. AppKit may be used where required, not as a mandatory UI rewrite.
7. **Security:** never commit credentials, tokens, unredacted user data, SL session keys or sensitive captures. Do not run authenticated actions automatically without necessary access.

## Completion (ALL required)
- Independently build and launch the **release-candidate fork** on real arm64 macOS; inspect Mach-O architecture and non-system runtime dependencies.
- All application-owned world, UI, fonts and media display use Metal; no GL context is created in the viewer process. `audit_gl.py --strict` passes after resolving documented false positives; runtime/link inspection corroborates it.
- Feature matrix has no unknown/unverified core feature and captures the open-source Alchemy reference result vs Metal result. User-visible regressions are fixed or explicitly accepted by the owner; no agent unilateral waivers.
- Repeatable performance runs on M1 Max compare identical camera/scene, resolution, graphics quality, avatar density, texture cache warm state, temperature/power state and build configuration. Meet criteria in `PERFORMANCE.md`; explain every regression with captures.
- Stability checks include teleport, region changes, camera motion, prolonged sessions, device/surface resize, media, voice, asset loading, shutdown and recovery.
- Independent review of lifecycle/synchronization, shader correctness, licensing and security completed with evidence.
- Installation/runtime identity distinct from upstream Alchemy, release build reproducible, no private dependencies.

## Not assumptions
- Metal alone **does not guarantee** an FPS improvement, higher visual quality or less CPU time. Benchmark and profile.
- Unified memory is not universally zero-copy optimum; choose Metal storage modes by usage.
- Apple Neural Engine is not a programmable graphics accelerator. Only use supported Core ML workloads if an actual candidate workload, end-to-end win and quality parity are proven; absence of ANE use is **not** a failure.
- A successful compilation or a displayed triangle is not evidence of product completion.

## Owner versus Codex
Owner controls objectives, major scope exceptions, public distribution and external access. Codex owns technical analysis, task allocation, engineering changes, integration, tests, profiling and recorded handoffs. Do not require owner to manage agent microtasks.
