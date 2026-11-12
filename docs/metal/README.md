# Alchemy Metal — engineering knowledge base

**Objective:** a standalone, independently buildable, ARM64-only, macOS-only Second Life viewer with the existing Alchemy user experience rendered by native Metal, proven functionality and measured Apple Silicon performance improvements.

This directory is the persistent product and engineering record for Codex. `AGENTS.md` is the short entrypoint. This directory is not a set of milestones for the owner to manage: Codex owns decomposition, execution, evidence and continuity.

## Sources of truth
| File | Ownership |
| --- | --- |
| [PRODUCT.md](PRODUCT.md) | Non-negotiable product constraints and outcome |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Actual source boundaries, ownership and replacement strategy |
| [APPLE_SILICON.md](APPLE_SILICON.md) | Hardware capabilities, specialization and NPU policy |
| [VALIDATION.md](VALIDATION.md) | Feature parity and release gates |
| [POST_PORT_DEFECTS.md](POST_PORT_DEFECTS.md) | Mandatory retained register of discovered defects, owner reports, repair attempts and verification |
| [PERFORMANCE.md](PERFORMANCE.md) | Comparable measurements and success thresholds |
| [INSTRUMENTATION.md](INSTRUMENTATION.md) | Codex-owned frame telemetry and capture implementation contract |
| [OPERATING_MODEL.md](OPERATING_MODEL.md) | Autonomous work, review and handoff conventions |
| [DEPENDENCIES.md](DEPENDENCIES.md) | Licensing, distribution and binary provenance |
| [REPO_FACTS.md](REPO_FACTS.md) | Source findings to verify against fork HEAD |
| [state/CURRENT.md](state/CURRENT.md) | Observed status and immediate executable next action |
| [state/archive/INDEX.md](state/archive/INDEX.md) | Indexed superseded recovery evidence |
| [state/BLOCKERS.md](state/BLOCKERS.md) | Only reproducible unresolved blockers |
| [work/active/metal-port.md](work/active/metal-port.md) | Current execution ledger |
| [work/active/visual-review-queue.md](work/active/visual-review-queue.md) | Owner-attended daily visual comparison queue |
| [work/archive/INDEX.md](work/archive/INDEX.md) | Indexed superseded ledger checkpoints |
| [feature-matrix.csv](feature-matrix.csv) | Comprehensive user-facing parity inventory |
| [benchmarks/scenarios.json](benchmarks/scenarios.json) | Required reference workloads |
| [release-invariants.json](release-invariants.json) | Machine-checked product/release coverage map |
| [START_CODEX.md](START_CODEX.md) | One-time startup instruction for local Codex |
| [REFERENCES.md](REFERENCES.md) | Official reference material and provenance |

## Evidence rules
- Commit concise textual proof, decisions and comparisons. Do not commit credentials, simulator login data, huge video files, unredacted traces or raw GPU captures.
- Use ignored `artifacts/metal/` for raw local captures and CSV samples; report hashes and paths in versioned summaries if useful.
- Source notes are not a substitute for `git grep`, compilation, runtime validation or captured frames.
- Do not add routine progress spam. Update work records when material facts, integration state or next actions change.
- Keep `state/CURRENT.md` and the active ledger concise. Archive superseded
  checkpoints through their indexes and run `tools/metal/validate_harness.py`
  after harness/documentation changes.
