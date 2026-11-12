# Alchemy Metal — Codex repository instructions

This repository is an Apple Silicon-only, macOS-only fork of Alchemy Viewer. Its product objective is a **native Metal-rendered Second Life viewer with retained open-source Alchemy functionality and demonstrably better performance**, not a demonstration or an API-translation exercise.

## Authoritative map (read only what is relevant)
- `docs/metal/PRODUCT.md` — immutable target, exclusions, release criteria.
- `docs/metal/ARCHITECTURE.md` — verified source map and implementation constraints.
- `docs/metal/APPLE_SILICON.md` — hardware-specific requirements and claims that require measurement.
- `docs/metal/VALIDATION.md` — feature parity, OpenGL elimination, correctness and evidence.
- `docs/metal/POST_PORT_DEFECTS.md` — mandatory defect register; record every discovered defect or reported regression and retain its ID/history through fixes, failures and reverts.
- `docs/metal/PERFORMANCE.md`, `INSTRUMENTATION.md` — benchmark/capture protocol and measurements.
- `docs/metal/OPERATING_MODEL.md` — autonomous execution, worktrees, work ledger and handoffs.
- `docs/metal/state/CURRENT.md` — last *observed* state; do not treat initial assumptions as observations.
- `docs/metal/work/active/metal-port.md` — executable work ledger; update as you implement.
- `doc/BUILD.md`, `doc/ARCHITECTURE.md`, `doc/TESTING.md` — upstream Alchemy reference documentation; verify against actual source.
- `.agents/skills/` — use a scoped skill for renderer work, GPU validation and resumable work.

## Execution contract
1. **Implement**, do not stop after writing a plan. Own discovery, decomposition, code, tests, review, integration, profiling and documentation. Work toward the full product without waiting for a new human prompt at each subsystem boundary.
2. Product constraints: **ARM64-only, macOS-only, native Metal only** for application-owned rendering. No OpenGL, Vulkan, MoltenVK, Rosetta, translation renderer, retained fallback graphics backend or unnecessary cross-platform graphics abstraction in the completed fork. During conversion keep the *original upstream executable* as an external reference, not an in-fork fallback.
3. Keep existing non-rendering features and open-source dependencies functioning; replace GPU-dependent implementations as necessary. Do not silently delete features or declare them out of scope. Preserve media/voice and UI behavior; audit third-party subprocesses separately.
4. Prefer direct, explicit Metal implementations in existing well-understood ownership boundaries. Reject gratuitous frameworks, speculative refactors, and rewriting network, inventory, simulation or unrelated UI logic.
5. Treat M1 Max as the first required execution target, but use runtime Metal feature checks for additional Apple Silicon GPUs. Optimize **measured performance**, not nominal hardware utilization. Do not propose Neural Engine use without an identified supported Core ML inference workload and a benchmark proving net benefit.
6. Source is authoritative. Confirm symbols, dependencies, preset names, build paths and platform capabilities by inspection. Mark unknowns unknown; never invent successful builds, screenshots, GPU captures or benchmark figures.
7. On an Apple Silicon Mac, run `tools/metal/doctor.sh`, `tools/metal/build.sh`, `tools/metal/verify_app.sh <app>`, and tests appropriate to the work. If builds fail, fix environmental or code defects when within scope. Record exact commands, exit status, evidence paths and source SHA.
8. Run `python3 tools/metal/validate_harness.py` after documentation/harness changes and `python3 -m unittest discover -s tools/metal/tests -v` after tool changes. `tools/metal/audit_gl.py` inventory is informational until the Metal-only implementation is ready; `--strict` is a release gate.
9. Read and maintain `docs/metal/POST_PORT_DEFECTS.md` at session start and every defect discovery, repair attempt, retest, revert and handoff. Register reports before fixing; retain closed/reverted records and distinguish confirmed defects from unproved symptom attribution. Builds/tests alone cannot close a defect. Maintain `docs/metal/state/CURRENT.md` and the active work ledger at every substantial handoff with register IDs, verified facts, decisions, tests, failures and next action. Do not replace evidence with confidence statements.
10. Use separate worktrees/agents for **independent** changes only. Assign exclusive ownership for shared files (notably `pipeline.cpp`, `llviewershadermgr.cpp`, shaders and render-state classes) and integrate with tests; no concurrent uncoordinated edits.
11. You may commit non-destructive work locally. **Owner override (2026-11-12): keep proposed fixes local until the owner tests them, agrees they work and explicitly approves pushing. Automated test/build success does not authorize a push.** Push only the approved scope; an explicitly requested revert is authorized. Do **not** force-push others' branches, merge to default, alter repository permissions, publish releases, deploy, upload private captures, or expose credentials without explicit authorization.
12. If a genuine external dependency prevents progress, record the exact blocker, investigate viable in-scope remedies, continue on independent work, and ask the owner only when an actual decision or credential is indispensable. Do not use uncertainty to stop all engineering.

## Session start / end
- Start: check `git status`, current commit/branch, host capability, `CURRENT.md`, active work ledger, and relevant skills; confirm the working tree isn't somebody else's in-progress work.
- Continue: choose the highest-value **unblocked implementation** item, execute, test and integrate. Engineering sub-tasks are yours to coordinate; they are not user-facing approval phases.
- End/handoff: update evidence and next action, commit coherent changes where permitted, identify failures honestly. Never report the product complete until `docs/metal/VALIDATION.md` release gates are met.
