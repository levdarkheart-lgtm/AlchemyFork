# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `c209340d5e`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,262 findings; strict exits 1. The one-finding increase is a preprocessor-unaware split of the final blit, while Darwin post-AA/sharpen symbols are absent from the binary. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; SSR warm-up is explicitly barred from presentation. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; direct owned reads, RGBA/typed uploads and production native mip generation are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and grayscale/color font-atlas pages are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners, color grading, tone curve, BRDF, post FXAA/SMAA/CAS, DoF and world HDR bloom/cross-filter are native-only; native-eligible PBR previews own geometry/post/publish. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Eligible PBR previews now complete before shared GL-target acquisition; a targetless-only scheduler run performs no GL work. Retained-readiness coupling and unsupported fallback remain. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — targetless native material-preview scheduling

Commit `c209340d5e` makes global GL setup in the dynamic-texture scheduler lazy
and lets an eligible Darwin material preview render and publish before shared
GL-target acquisition. A successful targetless-only update performs no GL
state unbind, Preview/Bake target acquire/bind/clear, final flush or base
viewport restore. Unsupported or failed native attempts enter the original
target-backed callback in the same frame; other preview/bake order is
unchanged. Native readiness still depends on the auxiliary GL screen and
retained `ProbePostPing` plan. No viewer was launched or compared; visual row
17 advanced to build 68175.
[Exact evidence](../../state/archive/CURRENT-native-material-preview-targetless-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Release build `26.4.0d68175` passed at
  `c209340d5e89c9e5832957d1b952365806d92054`; the RelWithDebInfo appearance
  utility also builds at that source.
- The full native Metal binary passes 52,381 assertions across 55 cases with
  API and shader validation enabled. Focused complete PBR material-preview
  coverage passes 426 assertions across two cases.
- The full `alrender_tests` suite passes 557,312 assertions across 137 cases;
  its focused resource-plan matrix passes 3,376 assertions across 37 cases.
- Physical fresh-context renderer validation passes 2,243 assertions across 94
  cases. CPU-only renderer validation passes 1,513 assertions across 203 cases;
  202 pass and one established font-palette case skips.
- The full `newview_tests` binary passes 98,973 assertions across 743 cases;
  one platform CRT case skips as expected.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `443d11a410455a5b81abab8c6d22752b6774e7fd105fd4bad2aee9d4e9a5d5b4`.
  Appearance utility:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
  Source/packaged MSL:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
  Metallib:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- The broad retained-GL suite remains at 8,335/8,340 assertions and 181/224
  cases, with its four established failures and 39 skips.
- Strict inventory is 2,408 files / 3,262 findings, expected exit 1. Exact
  report SHA-256:
  `c6bc62340c80ad112de586e8a31d7c558fc9389021f1c4947184a31eacbc08f1`.
  Both OpenGL and Metal remain linked.
- Static symbol inspection finds none of the retained FXAA/SMAA/CAS/sharpen or
  DoF program globals or retained DoF methods in the exact Darwin objects.
  The retained bloom programs and generator remain present because unsupported
  preview configurations still reference them; eligible native previews bypass
  that transaction.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—a complete native PBR preview owns CPU-derived
   camera matrices, shader geometry, light state, post and publication before
   shared GL-target acquisition; targetless-only scheduler work uses no GL.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict binary/runtime audit;
   measured M1 Max performance; independent review and release proof.
3. **Priority reassessment:** decouple native eligibility from retained GL
   resource readiness, preserve same-frame fallback, then widen eligibility.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Split native preview readiness from the auxiliary GL screen and retained
`ProbePostPing` plan so exact native publication can proceed without those
resources. Preserve same-frame fallback whenever its retained prerequisites
are available; otherwise leave the item pending. Then extend exact ownership
to classic/non-HDR/post-suppressed variants.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
