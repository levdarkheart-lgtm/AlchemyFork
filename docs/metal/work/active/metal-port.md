# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `2a32759e7c`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,269 findings; strict exits 1. Reflection manager and `pipeline.cpp` have zero direct `gl_function` findings. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; SSR warm-up is explicitly barred from presentation. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; pipeline scopes, scene probes, pixel ingress and ordinary/default probe publication are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Native world/UI and CEF IOSurface paths exist; moving media parity is queued for owner review. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners are native; visual-only validation is batched in the daily queue. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Probe publication and pipeline diagnostics are native-only; next prove the final production `ALGPUReadback::requestPixels` owner. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native diagnostic readback

Commit `2a32759e7c` deletes the pipeline's synchronous GL scene-pixel fallback
and its scope GL render-target/PBO transaction. Scene probes require a
same-frame, dimension-matched native opaque-post source. Scopes require the
exact native display-post source, point-decimate it on Metal and collect it
through the native bounded readback ring. Missing or failed native production
leaves the diagnostic sample unavailable rather than importing GL pixels.

The static strict scan remains 3,269 because the removed PBO calls were hidden
behind the still-audited shared helper. `ALGPUReadback::requestPixels` now has
one production caller: `DebugShowColor` in `llviewerwindow.cpp`.
[Exact evidence](../../state/archive/CURRENT-native-diagnostic-readback-cutover-checkpoint-2026-11-02.md).

## Validation checkpoint

- Source-exact warnings-as-errors RelWithDebInfo build `26.4.0d68112` passed at
  `2a32759e7c7faa1fd33bad56c622fc66923231c9`.
- Physical Metal validation passes 52,190 assertions across 52 cases with API
  and load-action validation enabled.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Executable SHA-256:
  `e9a1ab921ddac8ce088ebab03c5ff7c5ad65cc228ea174036c17b4adb888d390`.
  Source/packaged MSL:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
  Metallib:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,269 findings, expected exit 1. The linked
  executable still contains both OpenGL and Metal.
- The prior 574-entry broad CTest inventory remains 568 passes, six GL-host
  failures and five skips. It is recorded as failed, not promoted to green.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—pipeline scopes and scene-pixel probes no
   longer copy or synchronously read GL pixels; native exactness remains
   fail-closed.
2. **Deferred critical work:** remaining GL context/traversal/resources and
   readbacks; feature/stability matrices; visual queue; strict binary/runtime
   audit; measured M1 Max performance; independent review and release proof.
3. **Priority reassessment:** continue M-011 on `DebugShowColor`, the final
   production pixel request through `ALGPUReadback`. Do not return to one-off
   viewer driving.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Trace `llviewerwindow.cpp`'s `DebugShowColor` timing, source, coordinate and UI
inclusion contract against the exact native presentation/world-UI owners.
Replace its GL/PBO request only if the native source is exact, then determine
whether the production `ALGPUReadback` pixel API can be deleted.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot or presentation gates.
