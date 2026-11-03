# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `a33f57df9c`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,244 findings; strict exits 1. Reflection manager and `pipeline.cpp` have zero direct `gl_function` findings. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; SSR warm-up is explicitly barred from presentation. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; direct owned framebuffer and texture-image reads are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin accelerated CEF IOSurface ingress is native-only; ordinary media upload and owner-attended moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners are native; visual-only validation is batched in the daily queue. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Zero direct production framebuffer or texture-image readback calls remain; live duplicate GL resource/traversal owners are next. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native accelerated CEF ingress

Commit `a33f57df9c` removes the Darwin accelerated-CEF path's GL rectangle
texture, CGL IOSurface binding and framebuffer blit. IOSurfaces now import
directly into Metal and copy into the native media mirror; playing state is
published only after native import/copy/adoption succeeds.

Missing native capability rejects accelerated paint so the CPU path remains
available. Windows/Linux interop is unchanged. The packaged viewer has no
`CGLTexImageIOSurface2D` dependency. No viewer launch or comparison was
performed.
[Exact evidence](../../state/archive/CURRENT-native-cef-ingress-cutover-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors RelWithDebInfo build `26.4.0d68127` passed at
  `a33f57df9c70ce4cff5434f622437f78b54a801f`.
- The full native Metal binary passes 52,221 assertions across 52 cases with
  API validation enabled; the focused IOSurface import/flip case passes 16.
- Physical renderer validation passes 1,216 assertions across 85 cases with
  API validation enabled.
- The full `newview_tests` binary passes 98,973 assertions across 743 cases;
  one platform CRT case skips as expected.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `970a08af785d28f1ca111188c2a41beca3aedc9adc1ebcb93cc6aafc5c04decb`.
  Source/packaged MSL:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
  Metallib:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,244 findings, expected exit 1, down five
  from the prior exact checkpoint. Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—Darwin accelerated moving-media ingress now
   owns no GL object or GL copy and publishes only its exact native result.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict
   binary/runtime audit; measured M1 Max performance; independent review and
   release proof.
3. **Priority reassessment:** ordinary CPU-painted CEF/VLC frames still allocate
   and upload both GL and Metal storage on Darwin. This is the next live duplicate
   owner in the same well-understood media boundary.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Add a native-only Darwin media allocation/partial-update/publication boundary to
`LLImageGL`, route ordinary CPU-painted CEF/VLC frames through it, and physically
prove byte formats, dirty rectangles and async ordering. Keep visual parity in
the owner-attended daily queue.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
