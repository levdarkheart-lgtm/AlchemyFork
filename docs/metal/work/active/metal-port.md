# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `04edf7b27d`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,406 files and 3,217 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact HDR/non-HDR world, presentation and ordered-UI Metal publication is default-on and fail-closed per frame; dead restore authority/methods and redundant shutdown hooks are deleted. Live GL context creation/shutdown remains. |
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

## Active checkpoint — Metal-first default presentation authority

Commit `04edf7b27d` makes the exact native Metal
world/presentation/ordered-UI transaction the default path. Both the source
and packaged `settings_alchemy.xml` declare `ALMetalWorldPresentation=1`, and
the C++ cached-control fallback agrees. The preceding non-HDR glow/RGBA8
implementation and all earlier native feature owners are unchanged.

The path first earns one complete native drawable submission, then unlocks
continuous publication. Every frame independently starts transparent and only
a committed current-frame drawable suppresses its GL swap; incomplete frames
retain the complete GL result. The hidden nonpersistent override remains only
for controlled fallback diagnosis during transition. The 2-file change adds 9
lines and deletes 8. The
[exact evidence](../../state/archive/CURRENT-metal-first-presentation-default-checkpoint-2026-11-03.md)
is separate; owner-attended visual presentation remains queued.

## Validation checkpoint

- The source-exact warnings-as-errors Xcode Release build `26.4.0d68231`
  passed at `04edf7b27d`; the exact Ninja tree configured and linked at 68231.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices; deep strict
  signing passes.
- Viewer executable SHA-256:
  `58026eb643ad39f15b0c42e3f0ea50f752762157a99a12ef26797b1ceff800d0`.
  Source/packaged MSL:
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`.
  Metallib:
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source/packaged settings:
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged Metal-presentation default: `1`.
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
  `newview_tests` self-reports build 68231 and passes 98,973 assertions/743
  cases; one established platform CRT case skips.
- Harness structure validation, all 27 harness/tool tests and diff checks pass.
- Strict inventory is 2,406 files / 3,217 findings / 2,906 unique
  path/kinds/snippet records, expected exit 1. Exact report SHA-256:
  `eb8b00822f94cd6b3e7a2149891e6bb359a5ee04b670b5778de35c65ad7ff449`.
  Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—the complete native presentation transaction
   is requested by default rather than remaining a development opt-in.
2. **Replacement boundary:** the existing Metal feature implementation becomes
   primary; full GL remains only as fail-closed per-frame transition recovery.
3. **Deferred critical work:** remaining GL context/traversal/resources,
   feature/stability matrices, visual queue, strict binary/runtime audit,
   measured M1 Max performance and release proof.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Instrument and census default Metal publication failures and remaining GL side
effects without independently driving a viewer. Continue replacing those
responsibilities, then retire fallback only when complete native behavior is
durable; do not trade functionality for a lower GL count.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
