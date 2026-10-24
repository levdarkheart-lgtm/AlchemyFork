# Codex operating model: one outcome, self-directed execution

The owner gives **one product objective**. Codex manages all internal engineering tasks and context. We do not require the owner to drive phases, approve each workstream, or repeatedly restate the mission. Work is never called complete just because a plan, demo or isolated experiment works.

## Every new session
1. Read `AGENTS.md`, `PRODUCT.md`, this document, `state/CURRENT.md` and `work/active/metal-port.md`. Use relevant `.agents/skills/` guidance only when triggered.
2. Run `git status --short --branch`, inspect `git log -3`, determine local machine capabilities; ensure not to discard uncommitted user/other-agent changes.
3. Confirm environment and build facts; use actual source to update inaccurate assumptions. Select the highest-impact unblocked engineering action and **start modifying code**.
4. Verify at the narrowest practical scope, then integrate. Do not defer validation indefinitely; no fake passing test reports. If GPU execution is unavailable, explicitly record that limit and use Mac when available.

## Work ledger
`work/active/metal-port.md` is the single execution plan with a work table and changelog. Add work items as needed; each has an ID, owner/worktree, dependencies, affected source paths, acceptance/evidence and current state (`todo`, `doing`, `blocked`, `verified`). User does not need to approve every row. Long investigations may get a separate file using `work/TEMPLATE.md`; index it from the active ledger. Move fully verified standalone work records into `work/completed/` (create when needed), retaining links.

`state/CURRENT.md` is a brief recovery snapshot: commit/branch, observed local build, Metal implementation status, test/profiling evidence, current integration owner, high-impact next action. `state/BLOCKERS.md` contains reproducible blockers only with symptoms, commands, attempted remedies, owner/decision and unblocking condition.

## Engineering ownership and parallel agents
- One **integrator** owns app entrypoint, shared render execution, final build and merged work queue. Independent agents may own resource memory, shaders, UI text, media, performance or tests in separate worktrees **only if edit sets do not overlap**.
- No concurrent edits to `pipeline.cpp`, `llrender.cpp`, `llviewershadermgr.cpp`, `llwindowsdl.cpp`, shared shaders, CMake presets or work ledger without explicit serialized integration.
- Delegate an agent a concrete bounded outcome, paths, known contracts, expected tests and target branch. Review its diff and test proof before integration. Never outsource integration correctness to another agent's self-report.
- Prefer compact commits with runnable evidence and ability to bisect. Do not accumulate unrelated incomplete changes in one commit. No force push, default-branch merge, release, credential access or public deployment without explicit owner authorization.

## Autonomous delivery loop
Discover -> choose real work -> implement -> compile/test -> verify visually/with GPU traces as relevant -> self-review -> update ledger/state -> commit -> continue. Continue even after one subsystem is finished. If context or tools limit a session, record a specific executable next command and unfinished source locations so the next Codex session resumes. A new user prompt should not be required for each task while a Codex session can continue.

## Technical changes
- Keep source behavior where possible, rewrite OpenGL-bound implementation where required. No general renderer framework, Vulkan translation, GL emulation or permanent dual-backend toggle.
- Any graphics semantics change must be justified by source/validation evidence. Preserve build and feature correctness of the working fork while progressing, but no requirement to maintain a second renderer inside the fork.
- For rendering changes, inventory read/write dependencies and shader permutations before changing pass ordering. Capture validation traces and compare reference behavior.
- Prioritize engine correctness and concrete performance results; do not produce style refactors while regressions/blockers remain.

## Definition of a verified claim
A table row moves to `verified` only if an executable check ran on the claimed platform with command, exit code, Git SHA, system details, and evidence pointer. `not_run`, `inferred`, `blocked` and `waived_by_owner` are distinct states. No agent self-approval for feature loss or performance regression.

## Process improvement
If a recurring agent failure appears (missing context, flaky build, invisible GPU fault, stale docs), improve the nearest deterministic tool, test or focused instruction. Keep root `AGENTS.md` short, put detail in docs/skills, retire stale instructions. Minimize procedural overhead that isn't protecting correctness.
