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
| M-008 | Advanced views/effects and compute | doing | Default native radiance/SH publication is generation/sequence coherent even when CPU SH completion is delayed; continue broader probe publication, post, alpha/PBR and advanced-view parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-01

Commit `5f64b5ac2b` closes one default-probe publication timing gap. Native
radiance that finishes before its 64-byte CPU-visible SH copy is retained in a
private cube with the capture generation and sequence. It is published only
when the matching row becomes readable; error, stale/recycled generation,
sequence mismatch and device/layout reconciliation discard it. A coherent
native pair suppresses redundant GL/PBO default mirroring, while normal GL
traversal and presentation authority remain unchanged.

Warnings-as-errors build 68023 succeeded from the final source content. The
committed/pushed head then produced source-exact package 68024; deep strict
signing, 349/349 ARM64 inspection and executable SHA-256
`fa718e362311afa7c74bff8cd641620e7a6b2b644f0465d3fee1fea7fa0e5fd3`
pass. Complete RelWithDebInfo physical Metal validation passes 44,882
assertions/50 cases, and the post-change neutral gate passes 484/484 in 259.50
seconds.

Exact-path preflight and actual Computer Use on 68024 completed semantic login,
reached `STATE_STARTED`, showed a coherent upright world/UI, and completed
semantic logout. Logs contain `LogoutReply`, `Goodbye!` and stopped status; the
final viewer/helper census is zero. No Metal validation error was observed. The
rare delayed-promotion marker did not occur naturally, so the live run is a
regression smoke and not direct coverage of that scheduling branch. Raw logs
and authenticated-world images remain local.

## Complete-product checkpoint gate

1. **Product advancement:** default native probe radiance and SH publication
   now remains coherent when CPU SH completion is delayed. This is bounded
   M-008 progress toward retained PBR/material-preview behavior.
2. **Deferred critical work:** remaining post/alpha/PBR/probe/advanced views;
   source- and scene-identical post comparison; full parity/stability; strict
   no-GL; independent review; and qualifying M1 Max performance evidence.
3. **Priority reassessment:** the source race is repaired and the ordinary live
   path is regression-checked; the rare delayed branch did not naturally fire.
   Continue at the next demonstrated source gap rather than spending more time
   forcing scheduler timing without evidence. Reject future metrics unless
   exact provenance plus the settings/view/environment/window contract is
   recorded.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Trace the next unverified non-default probe-publication, post or advanced-view
pass from its active GLSL caller through resource format, binding, state and
ordering. Add an asymmetric physical negative control for the first
demonstrated divergence, implement the bounded native owner, then rerun
focused/full Metal validation and the neutral gate.
Any new cross-backend image pair must pin exact source provenance and match
camera, environment/time, warm state, window/snapshot contract and effective
graphics manifests before metrics are admissible.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
