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
| M-008 | Advanced views/effects and compute | doing | GPU-owned Lightbox reference wipe/side-by-side is physically and in-world verified; continue post, alpha/PBR, probe and advanced-view parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact app/profile/hash preflight and semantic login/in-world/control/logout operation pass. |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-01

Commit `011ede6cf9` closes the concrete GL-owned Lightbox reference-still gap.
The native owner copies the exact post-AA/CAS RGB10_A2 or RGBA8 source on GPU,
joins the active command batch, publishes only after commit, and clears stale
state on replacement, resize, allocation or batch failure. The final native
presentation shader now receives that coherent still for wipe and side-by-side;
missing demanded data continues to fail closed to established GL authority.

The focused physical test passes 1,850 assertions and byte-compares the captured
GPU texture before asymmetric wipe/side-by-side checks. The full physical suite
passes 44,871 assertions/50 cases. Build 68016 passes deep signing, 349/349 ARM64
slices and the 484/484 neutral gate. Actual Computer Use completed semantic
login, Lightbox Grab, 0.50 wipe, visible side-by-side, Clear, Look restoration
and confirmed clean Quit. The runtime log has no native-reference, Metal
validation or command-buffer error and ends with `Goodbye!`; no process remains.

The source audit also compared base/local lights, energy compensation,
atmosphere/water fog, map decoding and exposure initialization with active GLSL
owners and found no demonstrated divergence. The prior mirror comparison remains
admissible only because post-run manifests prove 416/416 identical visual
settings. Any future image score must likewise fail closed on a graphics-setting
mismatch; matched presets alone are insufficient if per-feature settings differ.

The continuity harness validates 31 live files and 24 tooling tests, including
document-relative skill paths. `gpu-validation` now resolves the real
`docs/metal/benchmarks/scenarios.json` file. Bison 3.8.2 and the build-local
Python environment containing Linden's `llsd 1.2.4` remain explicitly selected.

## Complete-product checkpoint gate

At every substantial engineering checkpoint, update all three fields:

1. **Product advancement:** retained Lightbox comparison now has an exact native
   GPU owner plus physical and actual UI evidence. This is bounded M-008 progress,
   not feature parity, native-only completion or performance proof.
2. **Deferred critical work:** remaining advanced-family parity; full matrix and
   prolonged stability; strict no-GL; independent review/reproducibility; and
   paired quality-equivalent M1 Max performance evidence.
3. **Priority reassessment:** do not revisit aligned light/fog contracts or the
   disproven one-fixture mirror blur without new evidence. Audit the next open
   post or alpha/PBR boundary and implement only a demonstrated contract gap.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Trace the next unverified post and alpha/PBR pass from its active GLSL caller
through resource format, binding, state and ordering. Add an asymmetric physical
negative control for the first demonstrated divergence, implement the bounded
native owner, then run focused/full Metal validation and the neutral test gate.
Any new live image pair must pass exact preflight and identical post-run graphics
manifests before metrics are admissible.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
