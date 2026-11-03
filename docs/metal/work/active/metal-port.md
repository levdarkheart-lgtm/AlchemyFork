# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `7f3bbc58a0`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,244 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; dead restore authority/methods and redundant shutdown hooks are deleted. Live GL context creation/shutdown remains. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; image previews, visual hints and avatar composites publish authoritatively, including compatible linear storage for sRGB preview output. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and font atlases are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | World capture consumes native legacy bumps; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Image previews, visual hints, avatar composites and established advanced owners, material previews, FXAA/SMAA/CAS, DoF and HDR bloom/cross-filter are Metal-owned. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Image-upload previews, visual hints, avatar composites, legacy bumps, material previews and edit grid have native authority; live GL initialization, traversal, other dynamic textures and shutdown remain. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native image-upload previews

Commit `7f3bbc58a0` moves Darwin avatar and sculpt image-upload previews to
targetless Metal production and authoritative native publication. CPU camera
matrices retain interaction and framing. The avatar path retains its selected
body/clothing mesh, preview texture, skinning, lighting, tint and alpha cut;
the sculpt path retains generated geometry, scale and solid lighting. Prior
valid results survive incomplete updates. The old Darwin background,
avatar/sculpt GL draws, matrix transaction and framebuffer-copy producer are
deleted.
[Exact evidence](../../state/archive/CURRENT-native-image-preview-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Xcode Release build `26.4.0d68211` passed at
  `7f3bbc58a0`; the exact Ninja tree configured and linked at 68211.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices; deep strict
  signing passes.
- Viewer executable SHA-256:
  `dbe8db4893d4c5cd647fc72b8b439dd3d55a79a5b4ab432a59b90c0ef93a0b64`.
  Source/packaged MSL:
  `de17600b1fec7059e4b6417737d58dc6c47757b9ddee1f9d244096caa920c4a1`.
  Metallib:
  `f0832637526fad05fa96fe8aed5aa3216e2c88b5b09f9f74e71d8f352b13cda7`.
- With Metal API/GPU, shader and load-action validation,
  `almetal_tests` passes 52,447 assertions/55 cases,
  `llappearance_gl_tests` passes 13,207/3, and
  `llrender_gl_fresh_tests` passes 2,612/96.
- `alrender_tests` passes 557,312 assertions/137 cases.
  `newview_tests` self-reports build 68211 and passes 98,973 assertions/743
  cases; one established platform CRT case skips.
- Harness structure validation and all 27 harness/tool tests pass.
- Strict inventory is 2,408 files / 3,244 findings, expected exit 1. Its 2,933
  unique path/kinds/snippet record count is unchanged from build 68209; one
  wrapper snippet changed with the refactor. Exact report SHA-256:
  `07b221b3fccbab07c30cb11c086711b9a4b519c1db67ab4271b13e52b47f3eaf`.
  Both OpenGL and Metal remain linked.
- Exact symbol inspection finds targetless avatar/sculpt preview rendering,
  native camera preparation and authoritative Metal publication.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—the live image-upload preview feature retains
   camera interaction, avatar/clothing and sculpt geometry, real preview
   textures, skinning, lighting, alpha, background, publication and recovery
   behavior with Metal authority.
2. **Replacement boundary:** Darwin GL production was deleted only after the
   native scene owned equivalent inputs and result publication.
3. **Deferred critical work:** remaining GL context/traversal/resources,
   dynamic textures, feature/stability matrices, visual queue, strict
   binary/runtime audit, measured M1 Max performance and release proof.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Replace the Darwin `LLPreviewAnimation` retained-target producer with
targetless authoritative Metal output. Preserve animation playback, camera
interaction, system-avatar rigid/skinned meshes, cloth and alpha behavior,
default texture, publication and retry semantics, then validate physically
before deleting its retained path.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
