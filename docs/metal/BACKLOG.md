# Work prioritization — Codex-owned backlog

This is a *dynamic engineering backlog*, not a user approval phase plan. The active work ledger is `work/active/metal-port.md`; Codex keeps this document consistent with actual code and measured blockers.

Priority order should be driven by dependent implementation paths, risk, independent parallelizability and product value. Candidate workstreams (re-evaluate against checkout):

- Native ARM64 reproducible baseline: build and run reference; note original functionality and non-system dependency slices.
- Native Metal app presentation: integrate `CAMetalLayer` with existing SDL3 window/input lifecycle; complete present/resize and frame synchronization.
- Real renderer resource semantics: persistent and transient buffers/textures; stream uploads; explicit attachment plan, pipelines and command encoding.
- Native UI/fonts sufficient for actual viewer flows; keep existing widget model.
- Direct MSL implementation: deterministic shader census, shared logic and permutation binding schemas; test material output.
- Full in-world drawing: geometry, terrain, PBR/glTF, avatars, transparency, sky/water, shadows, probes, mirrors, HUD and editing.
- Metal compute: features currently disabled by GL 4.1, plus measured optimization only.
- Media/voice/texture ingress and readback (picking/snapshot), stability and repeated session tests.
- Hardware performance: real M1 Max GPU trace, CPU bottlenecks, storage modes, frame pacing and regression measurements.
- Repository simplification: remove owned GL code and dependencies, ensure native-only packaging, separate identity, licence notices, release gates.

Every workstream ends only with observed proof. Codex may parallelize independent source areas but must serialize shared renderer integration. Do not claim any of these implemented when only their names are listed here.
