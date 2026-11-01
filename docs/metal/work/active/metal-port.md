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
| M-006 | UI, fonts, HUD and media ingress | doing | Native login/world UI and CEF IOSurface ingress work; Highlight Transparent now replays through ordered static/rigged HUD capture. Moving CEF/VLC reference parity remains open. |
| M-007 | World scene and material correctness | doing | The active transparency/material shader audit found no valid-production divergence; static/rigged Highlight Transparent world output is now native. Broader live alpha/material/reference coverage remains incomplete. |
| M-008 | Advanced views/effects and compute | doing | World/HUD Highlight Transparent can no longer be omitted from an eligible native frame. Continue non-RLVa shadows, post, diagnostic-overlay and output parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-01

Commit `8361addd0f` repairs a reachable publication omission in
`LLDrawPoolAlpha::renderDebugAlpha()`. Highlight Transparent draws occur after
the retained native world replay and, for HUD attachments, during ordered UI
capture. The prior native eligibility accounting did not consume either draw,
so a Metal-published frame could omit the feature while the migration GL frame
showed it.

The capture now occurs after each successful model/skin setup and preserves the
smoke texture, texture/model-view-projection matrices, classification color and
avatar skin palette. World replay has static/rigged and alpha-write/RGB-only
pipeline variants; the diagnostic uses RGB-only output so underlying glow and
coverage alpha survive. Ordered HUD replay preserves live blend, depth, cull,
sampler, viewport and color-mask state. Invalid geometry, textures or skinning
rejects native authority; existing GL draws are unchanged.

This core editing/diagnostic output happens to be permission-gated by RLVa, but
the renderer repair is not an exact-RLVa-parity expansion. M-013 remains the
owner-designated nice-to-have. Only minimum RLVa behavior required for Metal/
no-GL cutover stays on the release path.

The preceding bounded production audit compared active legacy-material and PBR
alpha equations, cutoffs, data/color sampling, specular AA, shadow/probe/SSR/
hero inputs, glare coverage, fog, local lights, blend state and glow. It found
no reachable difference for valid viewer-produced data, so no speculative
shader change was made.

Focused physical RelWithDebInfo validation passes 1,364 world assertions and
485 ordered-HUD assertions under Metal API/GPU/load-store/shader validation.
Complete RelWithDebInfo, Debug and Release suites each pass 45,008 assertions/
51 cases. The neutral gate passes 484/484 in 246.16 seconds. Warnings-as-errors
RelWithDebInfo touched objects and the full Release viewer compile and link.

Source-exact Development `26.4.0d68036` at `8361addd0f` stages and signs. Deep
strict signing and 349/349 ARM64 inspection pass. Executable SHA-256 is
`064266af7bf95d6e5ddfefe2796ffd0015d8316b22d2bd7fa13ace27cdfd9fdf`;
packaged MSL is
`21093c0c3ef9481e55327ecfa17b34dcf25ab771a0021c07d7d7cdcea7bce5d0`;
compiled metallib is
`656aa2ec5a7c259f6a86198a12f75fd7ce50914c5ff2865ed7a31208cdb6e1bc`.
The executable still links OpenGL during migration, so strict no-GL is open.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers before capture. A mismatched off/on pair is invalid, and a
both-off control may isolate a residual but cannot close shadow parity.

Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both. Do not consume release-gate time on exact RLVa work without a
demonstrated native-cutover blocker.

## Complete-product checkpoint gate

1. **Product advancement:** static/rigged world and HUD Highlight Transparent
   output now participates in native eligibility, with physical skinning,
   alpha-retention and invalid-input proof.
2. **Deferred critical work:** remaining non-RLVa advanced output/post/shadows;
   controlled live material/editing parity; full stability/feature matrices;
   strict no-GL; independent review; and qualifying M1 Max performance
   evidence. Exact RLVa parity remains non-critical M-013.
3. **Priority reassessment:** the broad shader audit found no valid-production
   mismatch and is closed without an edit. Continue at concrete presentation
   eligibility boundaries after retained world replay and inside ordered UI.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Trace post-water/final diagnostic-overlay and ordered-UI production calls.
Implement the first reachable non-RLVa draw that can be omitted while native
presentation remains eligible, add an asymmetric physical control, and rerun
focused/full physical plus neutral validation. If source inspection proves
valid parity, close that bounded audit without a speculative edit and move to
the next concrete owner.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
