# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `6bc7395fe1`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,258 findings; strict exits 1. Parcel cutover adds no static finding. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; SSR warm-up is explicitly barred from presentation. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; direct owned reads, RGBA/typed uploads and production native mip generation are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media ingress/upload and initial placeholder allocation are native-only; owner-attended moving-media parity remains open. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners are native; visual-only validation is batched in the daily queue. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Media, terrain, water and parcel overlay own no GL texture on Darwin; CPU-generated sky faces are next. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native parcel overlay

Commit `6bc7395fe1` removes Darwin GL allocation, GL-manager gating and GL rebuild
queue dependence from `LLViewerParcelOverlay`. Completed RGBA maps publish to
native storage without changing logical texture identity; failed publication
retains the old complete texture and restarts a full CPU rebuild. Exact RGBA8
create/update readback proves all channels and outside-region preservation with
no GL name. No viewer launch or comparison was performed; parcel appearance is
queued for owner-attended review.
[Exact evidence](../../state/archive/CURRENT-native-parcel-overlay-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Release build `26.4.0d68137` passed at
  `6bc7395fe134d09cada72f0b08d57007e031caa6`; the RelWithDebInfo appearance
  utility also builds at that source.
- The full native Metal binary passes 52,221 assertions across 52 cases with
  API validation enabled; the focused IOSurface import/flip case passes 16.
- Physical renderer validation passes 2,187 assertions across 92 cases with
  API validation enabled, including exact RGBA8 native update/readback.
- The full `newview_tests` binary passes 98,973 assertions across 743 cases;
  one platform CRT case skips as expected.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `1f3af01e9078e0b2f4b65cd97c0aa9639042da4d68d890677fd32e8267393070`.
  Appearance utility:
  `5769d937be848ee0b6edf9c153eefb0b0107898f90bc42fb377399e2ff7b337e`.
  Source/packaged MSL:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
  Metallib:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- The broad retained-GL suite remains at 8,335/8,340 assertions and 181/224
  cases, with its four established failures and 39 skips.
- Strict inventory remains 2,408 files / 3,258 findings, expected exit 1. The
  exact report is byte-identical to the water checkpoint; both OpenGL and Metal
  remain linked.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—the Darwin parcel overlay owns Metal storage
   only, preserves its retained/native consumers and advances without GL
   lifecycle state.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict
   binary/runtime audit; measured M1 Max performance; independent review and
   release proof.
3. **Priority reassessment:** `LLSkyTex` is the next bounded owner. Its paired
   CPU-generated RGBA sky/shiny faces still publish through GL on Darwin.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Convert `LLSkyTex` Darwin face publication and restore behavior to native RGBA8
storage while preserving its current/next double-buffer selection and cubemap
inputs. Add any sky visual question to the owner-attended daily queue only
after deterministic implementation evidence exists.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
