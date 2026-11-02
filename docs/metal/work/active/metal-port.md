# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `6d82ef5405`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,265 findings; strict exits 1. Reflection manager and `pipeline.cpp` have zero direct `gl_function` findings. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; SSR warm-up is explicitly barred from presentation. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; pipeline/debug, Snapshot and appearance framebuffer readbacks are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Native world/UI and CEF IOSurface paths exist; moving media parity is queued for owner review. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners are native; visual-only validation is batched in the daily queue. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Zero direct production `glReadPixels` calls remain; direct texture-image readback and copy paths are next. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native appearance readback

Commit `6d82ef5405` assigns avatar morph-mask and out-of-process appearance-bake
readback to an ordered `ALMetalUIRenderer` RGBA8 checkpoint. It submits the
current output, reads the exact region, retains that completed texture as the
background and resumes drawing on a separate target. Short storage, unsupported
formats and native backend failures propagate through mask and bake publication;
there is no GL fallback.

The hidden Darwin window now owns a Metal device and scheduler without a
drawable surface. The appearance utility publishes them to `LLRender` and
`LLImageGL`, begins a native offscreen capture and reads the complete color
result. Production `llappearance` and `llappearanceutility` now have zero direct
framebuffer/texture readback calls; repository production has zero direct
`glReadPixels` calls. No viewer launch or comparison was performed.
[Exact evidence](../../state/archive/CURRENT-native-appearance-readback-cutover-checkpoint-2026-11-02.md).

## Validation checkpoint

- Source-exact warnings-as-errors RelWithDebInfo build `26.4.0d68121` passed at
  `6d82ef5405096cf108fa8a1419f72d16599697c0`.
- Physical Metal validation passes 52,221 assertions across 52 cases with API
  validation enabled. Focused appearance coverage passes 449 assertions across
  two cases against the hidden-window native backend.
- The full `newview_tests` binary passes 98,973 assertions across 743 cases;
  one platform CRT case skips as expected.
- The separate warnings-as-errors appearance-utility configuration compiles and
  links. Its executable and the viewer executable are ARM64 Mach-O binaries;
  a full external bake fixture was not executed.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `75fda49173bf127566b0f1e0698222128c3ca8e00e72d378618188b2901e30b1`.
  Source/packaged MSL:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
  Metallib:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,265 findings, expected exit 1. The count is
  unchanged because the scanner does not include the appearance directories.
  Both OpenGL and Metal remain linked.
- Content-equivalent broad `llrender_gl_shared_tests` retains the documented
  baseline of four frame-fence/depth-format failures and 39 skips; it is not
  promoted to green.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—avatar morph-mask generation and the retained
   appearance-baking utility now use exact native ordered readback and fail
   closed, eliminating all direct production framebuffer reads.
2. **Deferred critical work:** remaining GL context/traversal/resources and
   texture-image readbacks; feature/stability matrices; visual queue; strict
   binary/runtime audit; measured M1 Max performance; independent review and
   release proof.
3. **Priority reassessment:** `LLImageGL::readBackRaw` already has a native-mirror
   fast path but retains GL fallback/metadata/compressed reads, while cube-map
   array growth performs an all-layer GL CPU round-trip. These are the next
   source-proven direct texture-read owners.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Trace `LLImageGL::readBackRaw` callers and every accepted host/storage format.
Make supported Darwin fetched/sculpt readback require the native mirror, prove
discard-level and failure semantics, and remove its `glGetTexImage` fallback.
Then give cube-map-array growth an explicit native six-face-per-probe copy/scale
without a CPU round-trip.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
