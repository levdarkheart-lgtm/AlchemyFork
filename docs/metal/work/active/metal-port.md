# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `35082663e5`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,279 findings; strict exits 1. `pipeline.cpp` has zero direct `gl_function` findings. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; SSR warm-up is explicitly barred from presentation. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; SSR/impostor/shadow/hero/base-light pixel ingress is native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Native world/UI and CEF IOSurface paths exist; moving media parity is queued for owner review. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners are native; visual-only validation is batched in the daily queue. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Four further pipeline pixel bridges are removed; next inspect ordinary reflection-probe PBO ingress. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is now reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — pipeline pixel-ingress cutover

Commits `bdefc6b2d7`, `60c9ee55f2`, `bc79f48aca` and `35082663e5` delete the
remaining GL pixel fallbacks used by `pipeline.cpp` Metal consumers. Native
avatar-impostor attachments move directly between Metal targets; directional
and projector lighting consume native caster-depth arrays; hero finish consumes
the identity-matched native radiance cube; and base lighting requires the native
directional-shadow/SSAO light map.

Every missing/mismatched producer now leaves the native frame incomplete. No
fallback maps a PBO, reads a GL attachment/texture, stages CPU pixels or uploads
them back to Metal. The retained legacy renderer remains active for still-open
owners; global traversal was not suppressed.

The strict scan fell from 3,337 to 3,279. `pipeline.cpp` has zero direct
`gl_function` findings, though it still has GL shader/state/traversal references
and the executable still links OpenGL.
[Exact evidence](../../state/archive/CURRENT-pipeline-pixel-ingress-cutover-checkpoint-2026-11-02.md).

## Validation checkpoint

- Source-exact RelWithDebInfo build `26.4.0d68108` at `35082663e5` compiles,
  links, stages and signs with warnings as errors.
- Physical Metal validation passes 52,190 assertions across 52 cases with API
  and load-action checks enabled.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Executable SHA-256:
  `10086bb6ef963c3168658a5d83a2bb208bae1728bfe3d30fd32dcf02189e47bc`.
  Source/packaged MSL:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
  Metallib:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- The 574-entry broad CTest inventory completed with 568 passes, six GL-host
  failures and five skips in 1,268.61 seconds. Focused reruns reproduce three
  unique failures in untouched GL test code: frame-fence pacing, depth format,
  and DoF clip-control availability. This is recorded as a failed broad gate,
  not hidden or promoted to green.
- Harness validation and its unit suite pass with the daily review queue in the
  mandatory live-document inventory.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through the ready rows together. Existing artifacts may
be summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—four more GL-to-Metal pixel dependencies and
   58 audited findings are removed after the SSR cutover.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict binary/runtime audit;
   measured M1 Max performance; independent review and release proof.
3. **Priority reassessment:** continue M-011 on the next synchronous GL/PBO
   ingress. Do not return to one-off viewer comparison driving.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Inspect the two `llreflectionmapmanager.cpp` GL/PBO face readbacks against the
existing native ordinary-probe face/radiance owners. Remove the next
source-proven crossing, then rebuild, rerun the physical suite and refresh the
strict count.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot or presentation gates.
