# Continue only owner-authorized work; viewer stays closed

Read AGENTS.md and CURRENT.md completely. Branch codex/apple-silicon-metal.
Renderer checkpoint7710ae4f977295d7c3539516a2d3814485a7db45; canonical Release
26.4.0.68648 built, verified ARM64 and deep-strict signed. All previous fixes
remain included. Viewer CLOSED, final process census empty. Owner explicitly
said DO NOT START THE VIEWER. Do not use cua.getApp; it can auto-launch an app.
No screenshot comparisons or live acceptance were performed for Rotate.
Broader porting goal and earlier20FPS campaign remain PAUSED.

Read work/active/object-rotation-repair.md: exact1eb original Debug shader uses
position-only uniform-color attributes. Native loader omitted MAP_VERTEX,
rejecting immediate rings/fans and invalidating authoritative UI captures.
Repair restores that layout and nonindexed cull propagation; no input/permission gating.
Live saved log corroborates202/102-vertex rejection counts, not exact click timing.
New actual native fixture passes62/1; same frozen test with prior cull code fails
4/62; restored candidate plus native UI/metadata regressions pass419/14 under
Metal API+shader validation. Fixture cannot link newview loader or prove mouse
handling; app compiles loader placement. Live object rotation remains open.

Doctor/configure/build/verify_app/deep codesign all0;349Mach-O files ARM64.
Private command/source/hash receipts:
artifacts/metal/private-object-rotate-audit-20261110-01/final-build-identity.json.
Executablec9620350...; metallib52d5aec4... . No viewer launch for validation.
Owner requested push of all accumulated fixes; confirm remote finalHEAD equals
localHEAD and keep receipt private. No force push, default merge or release.

Earlier repairs: classic-avatar W1, HUD/material/UI/pending textures, loading,
terrain/names/impostors/environment/probes and packaged LibVLC retained.
Owner raised animated limit11→40, confirmed responsiveness and geometry fixed
among roughly76avatars; full parity remains open. Self-arrow-turn lag stillopen.
Do not repeat intrusive LLDB sampling; actual presented age was not established.
Camera/voice source audits found no wrong port; actual voice/music and teleport
fly-in behavior require future owner-authorized functional evidence.

Known UNFIXED source defects: deferred PBR emissive OFF ignored; native Glow ON
callback checks GL-only completeness. Preserve alpha/HUD inline emission when
addressing that later. HiDPI restart and sceneHDR wired, physical outputSDR.
Lightbox/EEP and bounded AutoExposure math match original; bright/dark lighting
complaint remains open without matched activeEEP/Look/exposure evidence.
Do not tune brightness from screenshots. No broad goal resumption assumed.

Full parity/stability/noGL/performance/release gates remain open. Last controlled
FPS9.745,20FPSunmet. No private uploads or cache restore. Await owner direction
after completing the currently authorized feature-branch push.
