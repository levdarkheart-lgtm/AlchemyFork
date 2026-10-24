---
name: agent-continuity
description: Resume, coordinate and persist long-running Codex implementation work on the Alchemy Apple Silicon Metal fork. Use at task/session start, context handoff, parallel worktree assignment, integration, blocked work or preparing a coherent commit/PR.
---

# Persistent implementation continuity

Read `AGENTS.md`, `docs/metal/OPERATING_MODEL.md`, `state/CURRENT.md`, and `work/active/metal-port.md`. Confirm branch, source SHA, worktree and uncommitted files before edits.

- Choose the highest-impact unblocked *implementation* item; action is preferred to another abstract plan.
- If parallel agents are available, assign disjoint source paths and concrete acceptance tests; the integrator alone modifies shared renderer execution files and canonical work ledger. Cross-worktree changes require validation before integration.
- Track status `todo`, `doing`, `blocked`, `verified` with actual commands, exit codes, hardware, changed paths and evidence links. A compile-only result cannot verify a GPU/rendering feature.
- If unable to finish in current context, update `state/CURRENT.md` with exact next command, impacted paths, source SHA, current ownership, active branch and unresolved effects. Do not strand work with a generic 'continue porting' note.
- Commit coherent non-destructive changes in a feature branch when allowed; pushing/creating PRs is permitted, merging main/force pushes/releases require owner approval.
- Re-evaluate source assumptions against fork HEAD; update stale knowledge. Never quietly alter `PRODUCT.md` immutable constraints to close a blocker.
- Continue autonomously through the product release gates during the active Codex task. If an external decision/permission is indispensable, log its exact scope and continue independent engineering.
