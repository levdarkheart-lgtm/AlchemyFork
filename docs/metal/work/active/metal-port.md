# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `d17e94c4c694`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,406 files and 3,217 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact HDR and eligible non-HDR Metal publication exist; dead restore authority/methods and redundant shutdown hooks are deleted. Live GL context creation/shutdown remains. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Color-grade volume/tone-curve and world/preview PBR BRDF LUTs and the shared dynamic-texture framework are Metal-native on Darwin; previews, visual hints and avatar composites publish authoritatively. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; wide-line/point preview diagnostics now have explicit native pipelines. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and font atlases are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | World capture consumes native legacy bumps; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Color grading, PBR BRDF integration, model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF, HDR bloom/cross-filter and eligible legacy non-HDR glow/presentation are implemented in Metal. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | The residual GL color-volume, tone-curve and PBR BRDF lookup owners are deleted after Metal replacement; Darwin dynamic previews/avatar composites and their shared framework are native-only. Continue producer-by-producer. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native non-HDR legacy glow and presentation

Commit `d17e94c4c694` implements the legacy non-HDR glow and complete RGBA8
presentation transaction in Metal. The preserved feature behavior includes
512-wide reduced targets, selectable RGBA8/RGBA16F precision, true-noise,
luminance/warmth/alpha extraction, alternating eight-tap blur and additive
combine after FXAA/SMAA and CAS. It also prevents HDR-only readiness checks and
second-boundary dithering from incorrectly blocking or altering non-HDR.

This checkpoint does not delete the GL owner. Eligible frames publish the
native transaction; any missing resource, unsupported state or failed stage
retains GL authority for the frame. The 6-file change adds 1,099 lines and
deletes 101. Physical Metal coverage exercises both precision paths, noise,
combine, invalid state and command-batch consumption. The
[exact evidence](../../state/archive/CURRENT-native-non-hdr-legacy-glow-checkpoint-2026-11-03.md)
is separate; owner-attended visual presentation remains queued.

## Validation checkpoint

- The source-exact warnings-as-errors Xcode Release build `26.4.0d68229`
  passed at `d17e94c4c694`; the exact Ninja tree configured and linked at 68229.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices; deep strict
  signing passes.
- Viewer executable SHA-256:
  `757305e9e3be84b91522291ab001e763d255b91d5213ce4fe16068eab7316cb0`.
  Source/packaged MSL:
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`.
  Metallib:
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- With Metal API/GPU, shader and load-action validation,
  `almetal_tests` passes 52,526 assertions/56 cases; its focused legacy-glow
  case passes 42 assertions,
  `llappearance_gl_tests` passes 13,207/3, and
  `llrender_gl_fresh_tests` passes 2,612/96.
- The focused Metal-only upload case passes 17/17 assertions in the shared
  binary. The complete shared diagnostic suite remains non-green at
  8,352/8,357 assertions (182 pass, 4 fail, 39 skip): four legacy/headless
  frame-fence timing assertions and one depth-format expectation fail; the new
  case passes in that same environment.
- `alrender_tests` passes 557,301 assertions/136 cases.
  `newview_tests` self-reports build 68229 and passes 98,973 assertions/743
  cases; one established platform CRT case skips.
- Harness structure validation, all 27 harness/tool tests and diff checks pass.
- Strict inventory is 2,406 files / 3,217 findings / 2,906 unique
  path/kinds/snippet records, expected exit 1. Exact report SHA-256:
  `487aeae27ab89b19b7d9d3fc10d148b582b291ba9865e039cee13fa89d43cf8b`.
  Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—legacy non-HDR glow and the surrounding RGBA8
   presentation chain now have real Metal implementations rather than being
   deleted to lower the audit count.
2. **Replacement boundary:** the GL owner remains deliberately retained while
   native precision, ordering, command-batch and presentation behavior gain
   the evidence needed for safe authority cutover.
3. **Deferred critical work:** remaining GL context/traversal/resources,
   feature/stability matrices, visual queue, strict binary/runtime audit,
   measured M1 Max performance and release proof.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Census the remaining non-HDR presentation eligibility/fallback responsibilities
and implement deterministic evidence for them. Retire the GL owner only after
the complete native behavior is authoritative; do not trade functionality for
a lower GL count.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
