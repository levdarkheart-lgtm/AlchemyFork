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
| M-002 | GL/shader/pass/resource and binary inventory | doing | Informational scan covers 2,408 viewer-owned files and 3,303 potential findings (including macros/comments/duplicates); strict release audit is not green. |
| M-003 | Native drawable, presentation and lifecycle | doing | Authenticated exact publication and clean lifecycle are observed; preserve while advanced families are validated. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core typed resource paths have physical coverage; remaining advanced/media matrices stay open. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Native login/world UI and CEF IOSurface ingress work; moving CEF/VLC reference parity remains open. |
| M-007 | World scene and material correctness | doing | Main native world chain publishes exact frames; advanced material/reference coverage remains incomplete. |
| M-008 | Advanced views/effects and compute | doing | Native `@setsphere` plus ordered-UI `@setoverlay` minimum support remove the explicit RLVa rendering rejections; continue non-RLVa shadows, transparency/materials, post and output parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact remaining inventory and restart-qualified test prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-01

The bounded production-call reflection-probe audit found no reachable
viewer-produced divergence. Metal's additional bounds checks leave valid
viewer data unchanged, so that audit is closed without a speculative edit.

The owner designated exact parity for **all RLVa functionality** as a
non-release nice-to-have. Minimum native implementation must still prevent
RLVa from blocking Metal authority and OpenGL removal. The durable M-013
[RLVa backlog](../deferred/rlva-parity.md) records every current family, the
minimum cutover boundary, the deferred exact work, and the required comparison
setup: enable RLV in Preferences in both viewers, save, and restart both.

Checkpoint `9c141e0887` implements the minimum native `@setsphere` owner. It shares
one per-frame shader-ready policy/tween sample with GL, consumes retained depth
and inverse projection, and chains blend, fixed/variable blur, chromatic
aberration and pixelation in visual-effect priority order. It fails closed on
missing or malformed state. The focused physical case passes 44 assertions;
complete RelWithDebInfo and Debug suites each pass 44,947 assertions/51 cases.
Commit `79356bfd4f` then removes the unconditional `@setoverlay` rejection from
the ordered world/UI transaction. The existing UI mirror consumes that exact
textured `gUIProgram` draw, including AnisoWrap, streamed tint/alpha and blend;
missing Metal texture state still fails the whole transaction closed. The
focused composition case passes 455 assertions in both build modes. Source
audit finds no remaining explicit RLVa native-eligibility rejection: forced
textures, avatar silhouettes and camera restrictions enter shared state before
native capture. Exact reference behaviour remains deferred in M-013.

The post-change neutral gate passes 484/484 in 247.94 seconds.

Source-exact warnings-as-errors Development build 68031 at `79356bfd4f`
staged and signed; an immediate repeat reported no work. Deep strict signing
and 349/349 ARM64 inspection pass. Executable SHA-256 is
`3f481298d69ecd8892d14271af072568996065ab10bd44c7571bf5eacae37efd`;
packaged MSL is byte-identical to source at
`06454d17c307c6e1b9493a15edbf16e0c59bc385707f59bac327f5b4f71f006c`;
the compiled metallib is
`53ac4db074d648096a2a5bb578a30b8b5406579e8bc2c4369e0c1470948e1013`.

## Complete-product checkpoint gate

1. **Product advancement:** active `@setsphere` and `@setoverlay` no longer
   automatically reject native presentation. Their minimum native owners are
   physically covered; this is not strict RLVa parity.
2. **Deferred critical work:** remaining non-RLVa post/alpha/PBR/probe/advanced
   views; full parity/stability; strict no-GL; independent review; and
   qualifying M1 Max performance evidence. Exact RLVa parity is separately
   deferred as non-critical M-013.
3. **Priority reassessment:** the RLVa minimum-cutover boundary is closed. Keep
   exact RLVa work in M-013 and return immediately to non-RLVa advanced owners,
   beginning with the isolated projector/cascade-shadow fixture.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Resume the isolated projector/cascade-shadow fixture. Trace the active GLSL and
Metal production calls, implement only the first demonstrated divergence, add
an asymmetric physical control, and rerun focused/full physical plus neutral
validation. Continue transparency/material replay and native-authority/no-GL
work afterward; do not pull M-013 exact RLVa work back onto the release path.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
