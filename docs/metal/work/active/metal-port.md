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
| M-008 | Advanced views/effects and compute | doing | Native reference stills, synchronized exposure/flare startup and one neighbor-selected manual-box probe transition are physically verified; continue remaining post, alpha/PBR, broader probe and advanced-view parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-01

Commit `d3cff63d0d` closes the cold-start exposure half of the temporal-post
lifecycle. If GL adapts while the main-world Metal renderer warms, both restart
from EV 0: native eligibility is reset immediately and the GL ping-pong target
is cleared at its established safe pass boundary. Prior flare/probe fixes remain
intact. Eligibility and GL authority stay fail-closed.

The complete physical suite passes 44,882 assertions/50 cases, the neutral gate
passes 484/484 in 239.08 seconds, and warnings-as-errors build 68020 passes deep
signing and 349/349 ARM64 inspection. Actual Computer Use produced a native
2048x1476 authored-mirror PNG, then ran pinned OpenGL 67607 sequentially under
the same view/Midday/Snapshot contract. Preflight and post-run effective visual
settings match 416/416; both sessions logged out cleanly and no process remains.

The binaries have exact but deliberately different source provenance. The
sequential avatar pose changed, so whole-frame scores are descriptive; the
stable mirror interior is SSIM `0.985163` / PSNR `34.480 dB` and has no material
visible residual for this fixture. Raw logs and authenticated-world images
remain local.

The next source audit time-boxed the active alpha/PBR replay seam and found no
new divergence worth a speculative production edit. It then added the missing
probe-selection control: a physical Metal scene reaches a containing manual box
only through a selected automatic probe's neighbor list and proves that the box
suppresses the already accumulated automatic/default family. Removing only the
neighbor edge restores that family. The focused case passes 1,329 assertions;
complete Debug and RelWithDebInfo physical suites each pass 44,882 assertions
across 50 cases under Metal API/GPU validation. This is test evidence, not a
claim of broad live probe parity. The post-change neutral gate also passes
484/484 in 239.08 seconds.

## Complete-product checkpoint gate

1. **Product advancement:** temporal exposure survives cold renderer warm-up,
   authored-mirror native readback is observed under a matched cross-backend
   contract, and a previously uncovered probe neighbor/box transition now has
   an asymmetric physical regression. This is bounded M-008 progress.
2. **Deferred critical work:** remaining post/alpha/PBR/probe/advanced views;
   source- and scene-identical post comparison; full parity/stability; strict
   no-GL; independent review; and qualifying M1 Max performance evidence.
3. **Priority reassessment:** the repaired lifecycle and probe-neighbor
   boundaries have direct evidence. Continue at the next demonstrated source
   gap rather than accumulating more activity on them; use a live probe fixture
   only when its transition is visually attributable, and reject future metrics
   unless exact provenance plus the settings/view/environment/window contract
   is recorded.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Trace the next unverified probe-publication, post or advanced-view pass from its
active GLSL caller through resource format, binding, state and ordering. Add an
asymmetric physical negative control for the first demonstrated divergence,
implement the bounded native owner, then rerun focused/full Metal validation
and the neutral gate.
Any new cross-backend image pair must pin exact source provenance and match
camera, environment/time, warm state, window/snapshot contract and effective
graphics manifests before metrics are admissible.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
