# Current engineering state — remaining-feature port resumed

**Observed:** 2026-11-11 UTC, M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `bccbb7be48000fab91b43688bc585f2f9ab474cf` plus current feature-port changes.
The owner resumed RLVa, Debug/Developer and remaining OpenGL removal. This
supersedes the previous broad-port pause. The separate20FPS campaign remains
deferred. No viewer launch or screenshot comparison is allowed. No account,
cache, settings, camera, location or simulator state was changed.

## Current implementation

[Remaining-feature port and exact checks](../work/active/remaining-feature-port.md).
Original viewer source was inspected before each change; independent reviews
found no blocking contract defects.

- RLVa: sole native sphere owner; remove displaced GL program, shaders,
  framebuffer execution and generic effect-target dispatcher. Preserve ordered
  enabled overlays and five sphere modes. Remove erroneous frame-only policy/
  view-origin cache so each execution samples current state like the original.
- Developer: next-run Metal API validation, including legacy request migration
  and `--debugsession`, is selected before device creation. Standalone actual-
  device probe selects MTLDebugDevice; normal control selects AGXG13XDevice.
  Explicit Force Driver Crash now enters Metal with invalid CPU data and an
  abort fallback; this action was not executed and submits no GPU work.
- Correct deferred emissive OFF independently of HDR storage, preserving inline
  alpha/HUD emission and cached-impostor data for reenable. Correct Glow ON's
  native program-availability latch.
- Correct Build No Post-processing's stale native preflight rejection. The
  downstream native owner already applies the original available-floater gate
  and gamma-only/tonemap-bypass behavior.
- Remove unused ALDepthBounds/ALDrawnOnly code, GL-only test, CMake entries and
  obsolete setting. Native depth/stencil/sky/radius restrictions remain.

All previous renderer, avatar, editing, travel and music repairs remain.
Deleted legacy files remain recoverable through Git.

## Observed validation

Doctor/configure and native test builds exit0. Initial sandbox builds could not
access compiler caches; approved retries used. First app compile exposed an
implicit camera include after RLVa cleanup; explicit include fixed, final
Release app rebuild pending. Do not claim the previous app is this candidate.

Under Metal API and shader validation, native renderer tests pass910assertions/
17cases. Focused RLVa/Glow/emissive/G-buffer/UI tests pass38307assertions/5cases;
display-post/tonemap-bypass fixture passes1824assertions/1case. Emissive fixture
covers HDR/SDR, both depth conventions, classic modes, switch off/on and legacy
fullbright invariance against analytic GPU values.

These fixtures do not establish live menu clicks, settings persistence, RLVa
command/reply, touch, tween, restart or full feature parity. No viewer started,
no deliberate crash executed, no performance claim.

Ignored evidence: `artifacts/metal/feature-completion-20261111/`.
Strict source audit exits1 with1792findings/2406files (previous1815/2408).
Names account for many findings, but dormant GL function pointers and real dead
GL implementations remain. Earlier executable inspection found no linked
OpenGL framework or unresolved GL imports; runtime context proof remains open.

## Complete-product advancement assessment

M-008/M-011/M-013 are doing: native feature semantics corrected and displaced GL
execution removed, with bounded GPU evidence. Full parity, source/runtime no-GL,
prolonged stability, performance and independent release gates remain open.
M-012 stays complete; M-014 stays deferred.

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-008 Advanced views/effects | doing | Native settings and developer behavior repaired; debug buffers remain. |
| M-012 Native UI automation | complete | Prior exact-build flow retained; no new viewer launch. |
| M-013 RLVa parity | doing | Sole native ownership and current-view state; live policy parity unverified. |

Full parity, strict no-GL and independent build/review evidence remain open.

## Exact next actions

Next finish current Release build and run `tools/metal/verify_app.sh` on
`build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
Then implement native `RenderBufferVisualization`: preflight currently rejects
every selected debug buffer and the old GLSL path alone consumes modes0–6.
Trace attachment sampling, luminance mip8 and retained SMAA intermediates before
native scope/reference/UI composition. Afterwards remove the dormant GLSL
compiler/linker/binary-cache tail while preserving metadata/uniform contracts.

Keep unresolved self-turn body/name-tag lag, camera/voice and broad lighting
complaints recorded in the prior [rotation handoff](../work/active/object-rotation-repair.md),
[display audit](../work/active/display-settings-lighting-audit.md),
[crowd record](../work/active/crowd-live-68643.md) and
[camera/voice audit](../work/active/teleport-camera-voice-audit.md).
No speculative brightness, camera or voice policy changes. No release readiness.
