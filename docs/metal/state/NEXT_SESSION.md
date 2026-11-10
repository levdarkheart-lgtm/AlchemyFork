# Continue correctness validation — native avatar repairs ready

Read AGENTS.md and CURRENT.md completely. Branch codex/apple-silicon-metal.
Renderer checkpoint `97dd825aaf428da4a50e6b8bf1659d5b60d62eda`.
Owner explicitly requested all four multi-avatar gap repairs; implemented and
committed, prepared build68633. All prior fixes retained. Earlier20FPS goal stays
PAUSED. Translator fully stopped; Xcode replay off. All CUA phases completed.
Root owns shared source/build/runtime/docs; agents finished owned work/reviews.

Owner subsequently explicitly authorized restart. Closed68626PID44312 viaCmdQ,
launched exact canonical68633; login screen observed inCUA. NewPID95871, path/
hash verified in private-avatar-parity-owner-try-20261110-01/launch.json. No launch
session/diagnostic overrides injected. Owner will log in and try different places;
do not drive login/travel. No crowd/region/settings/FPS observation yet.

Implementation details and exact limits:
docs/metal/work/active/avatar-crowd-parity.md.
- Native GPU completion receipt + ALGPUTimer now supply actual isolated avatar/
  attachment GPU times. profile requests queue from UI/autotune, dispatch one per
  main frame AFTER updateImpostors+main-camera restore; current shadow mirror is
  packed and coherent saved shadow/SSR inputs pinned. Reuses warmed renderer,
  separate targets, complete transaction, no visible impostor publication/wait.
- Shared avatar-ID cache spans main/probe owner swaps. Immutable quartet leases
  survive queued draws, replacement/destruction and actual GPU completion;
  failed publication preserves old, device/HDR/normal invalidation stays strict.
- Unloaded avatar skips where original skips. Original particles-disabled debug
  sphere still lacks a native producer, so that explicit case remains incomplete.
- Body texture requested/Default/Null ownership and raw-vs-sRGB sampling restored
  without UI-slot mutation, preserving alpha/depth/caster admission.

All focused GPU/API+shader checks passed: timing369/4, timer+UI+VB375/5,
cache395/2, avatar sampling31498/1, existing impostor126/1. Doctor/configure/build/
verify/signature exact-source records in
artifacts/metal/private-avatar-parity-repair-20261110-01/build-final.json.
No real crowd/viewer-manager/ART/visual comparison yet and no FPS gain claim.
Bounded max16 native avatar/attachment completion logs can verify actual samples.
Next owner-directed viewer run should test several avatars with full-detail limit
and profiling enabled, main/mirror impostors, loading/default-to-ready transitions,
and the earlier Midday/Sunset/Midnight fixes. Do not launch just for source audit.

Previous cold empty-cache evidence/archive:
docs/metal/state/archive/CURRENT-cold-start-environment-repairs-2026-11-10.md.
Original8978files/1703059096bytes cache remains preserved at ignored
artifacts/metal/private-empty-cache-start-20261110-01/original-cache;
do NOT restore over live cache. Account/settings untouched. Earlier sky/gamma/
probe continuity repairs68630 are ancestors; full visual result still unverified.
Terrain/water/shadow screenshot and random shoe/outfit spikes remain unresolved.
Private artifacts/captures/cache must never be uploaded without authorization.
