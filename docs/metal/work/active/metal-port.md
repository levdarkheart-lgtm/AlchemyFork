# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | ARM64 build/package/runtime identity is evidenced; controlled fixed-view reference benchmark remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | Informational owned-source inventory exists; strict release audit is not green. |
| M-003 | Native drawable, presentation and lifecycle | doing | Authenticated exact publication and clean lifecycle are observed; preserve while advanced families are validated. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core typed resource paths have physical coverage; remaining advanced/media matrices stay open. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Native login/world UI and CEF IOSurface ingress work; moving CEF/VLC reference parity remains open. |
| M-007 | World scene and material correctness | doing | Main native world chain publishes exact frames; advanced material/reference coverage remains incomplete. |
| M-008 | Advanced views/effects and compute | doing | Legacy normal raw-data seam fixed at `80924cdae2`; next bounded alpha/foliage or PBR fidelity seam is active. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Tooling and structural reductions exist; qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Actual semantic login/settings/Snapshot/logout validation passed; extend only for a named blocked flow. |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-01

Commit `80924cdae2` preserves raw tangent-space legacy normal data in the opaque,
lit-alpha and impostor-alpha native owners. The pre-fix physical negative control
failed all three paths; corrected focused validation passes 1,316 assertions and
both complete physical configurations pass 44,859 assertions/50 cases. The
expanded neutral gate passes 484/484 tests. Warnings-as-errors Development 68010
builds source-exact from the renderer code checkpoint, repeats with no work,
passes deep signing and verifies 349/349 ARM64 slices.

The build cache now uses Homebrew Bison 3.8.2 and a build-local Python environment
with the repository's `llsd` and `PyYAML` requirements. This converts the former
environment omission into executed LL compiler and Python test coverage.

The continuity checkpoint compacts the live state and ledger to 97 and 81 lines,
retains their full predecessors in indexed archives, fixes the stale verifier
path, and adds live-link, executable-command, cross-document and declarative
release-invariant checks. The 29-file harness and all 15 tooling tests pass.

## Complete-product checkpoint gate

At every substantial engineering checkpoint, update all three fields:

1. **Product advancement:** name the product/release requirement moved by the
   change and the causal evidence. The current fix advances material correctness
   and test coverage; it does not close M-008.
2. **Deferred critical work:** list still-open critical dependencies. Current:
   advanced-family parity, full matrix/stability, strict no-GL, independent
   review/reproducibility and paired M1 Max performance evidence.
3. **Priority reassessment:** choose the next action from dependency/risk
   evidence. Current: another bounded M-008 source contract precedes M-009,
   owner-by-owner M-011 cutover and final M-010 claims.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Time-box a different M-008 alpha/foliage or PBR fidelity source comparison.
Trace the OpenGL sampler/pass-state contract to its native owner, add a physical
negative control that fails for the suspected divergence, implement only the
demonstrated correction, and run focused plus complete Debug/RelWithDebInfo Metal
validation. Use Computer Use only for a controlled fixture that visibly
attributes the family under test.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
