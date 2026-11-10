# Continue avatar/HUD correctness; performance goal stays paused

Read AGENTS.md and CURRENT.md completely. Branch codex/apple-silicon-metal.
Renderer checkpoint `d18641c3557996807de03d99a105419161e95dfb`; final68626 built/verified/launched PID43249.
Root owns shared source/build/runtime/docs; agents idle. Source changes are
committed; this handoff records the completed build/runtime checks. Do not resume the earlier 20 FPS investigation.

All previous fixes were retained, as explicitly requested by owner. New work:
547daf9c31 authoritative world/UI HUD capture;
082075249b original HUD alpha/depth/blend, explicit sampler/activity and direct
opaque/MASK PBR HUD shader/dispatch;
916ad7e79d negative-test setup correction;
08bc61af05 failed native VB/IB retry queue and teardown;
d18641c355 pending HUD default/Null placeholders.

Doctor/configure/Release build/verify/signature passed. UI763/1 + selection4732/1
API+shader validation passed; native VB93/2 passed after real pre-fix failure4/47.
These do not imply complete visual parity. Owner said previous fixes were OK;
latest placeholder correction now included in68626. Four owner screenshots and
all logs/exits/hashes remain private under ignored artifacts/metal/ paths named
in CURRENT. No private uploads or publication. Current runtime has API validation
only, no benchmark flags, no forced location/profile/camera/outfit. User drives.

Final startup check19:24UTC:26 success/publication log entries, zero missing HUD
diffuse or VB/IB upload errors, zero API assertion errors. Generic UI buffer/texture
mirror warmup was logged twice (through ONCE10) then publication resumed; do not
claim every startup wait fixed. Viewer remains running for owner review.

Next, if owner reports another missing attachment or spikes,
trace that actual object's CPU geometry→draw map→native palette/batch; compare
exact original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45. Re-Wear had already fixed
this occurrence, so cause is unknown. No production upload error connected it
to the retry defect. Shared attachment routes/flush/query invalidation matched
reference in the audit; no speculative reclassification/culling workaround.

Earlier 20 FPS stop remains in archive/CURRENT-20fps-owner-stop-2026-11-10.md.
No further optimization/profiling unless owner resumes that goal. Translator must
remain fully stopped; Xcode replay stopped. No avatar travel by Codex. All CUA
phases completed. Product parity/stability/performance release work stays open.
