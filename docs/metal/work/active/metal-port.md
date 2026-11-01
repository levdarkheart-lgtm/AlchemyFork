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
| M-008 | Advanced views/effects and compute | doing | Native reference stills and main/probe-safe temporal post history are physically and in-world verified; continue remaining post, alpha/PBR, probe and advanced-view parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-01

Commit `f9cc59d963` closes the temporal-post lifecycle gap exposed by a real
nondefault native Snapshot. Main-world renderer warm-up now restarts GL/Metal
flare history at their shared exact-black boundary. Initializing an independent
reflection/hero capture no longer clears main-world exposure/flare readiness,
and auxiliary passes hold the global flare state because their color correction
does not consume it. Eligibility and GL authority remain fail-closed.

The complete physical suite passes 44,871 assertions/50 cases, the neutral gate
passes 484/484 in 245.95 seconds, and build 68018 passes deep signing and
349/349 ARM64 inspection. Actual Computer Use produced a native 2048x1476 PNG
after both main/probe warm-up, then logged out cleanly with no process remaining.
Preflight and post-run effective visual settings match 416/416.

The earlier GL/native image score is deliberately not accepted as parity:
although graphics settings match, source revision, region lighting and avatar
pose changed. A scored pair must also hold source, view, environment/time and
warm state constant. Raw logs and authenticated-world images remain local.

## Complete-product checkpoint gate

1. **Product advancement:** a real temporal-post owner now survives renderer
   warm-up and independent-probe lifecycle boundaries, and native post Snapshot
   readback is observed in-world. This is bounded M-008 progress.
2. **Deferred critical work:** remaining post/alpha/PBR/probe/advanced views;
   source- and scene-identical post comparison; full parity/stability; strict
   no-GL; independent review; and qualifying M1 Max performance evidence.
3. **Priority reassessment:** the repaired lifecycle boundary has direct runtime
   proof. Continue at the next demonstrated source gap rather than accumulating
   more activity on it; improve capture controls before accepting new metrics.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Trace the next unverified post or alpha/PBR pass from its active GLSL caller
through resource format, binding, state and ordering. Add an asymmetric physical
negative control for the first demonstrated divergence, implement the bounded
native owner, then rerun focused/full Metal validation and the neutral gate.
Any new image pair must match source, camera, environment/time, warm state and
effective graphics manifests before metrics are admissible.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
