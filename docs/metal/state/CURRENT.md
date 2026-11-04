# Current engineering state

**Observed:** 2026-11-04 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `31fa27392e`

**Historical evidence:** [archive index](archive/INDEX.md)

This concise recovery snapshot delegates superseded detail to the indexed archives.

## Active implementation checkpoint

### Owner-accepted transition UI; exposure/mirror parity failed

Two owner-attended candidates are explicitly rejected. Run
`font-cache-replay-fix-20261104-01` (executable SHA-256
`49d5cf47f17e9e740dab38aaee48ef76ec5dd540afb7b9bc83f26522b62695e6`)
showed icons and world geometry but no menu/location/toolbar text, with flashing
glyph rectangles in the FPS field. Run `font-metal-overlay-20261104-01`
(executable SHA-256
`bfe362b614de240f5e8ea45d183b61c77d0937b58e09605d08b1c1dacffa200b`)
then showed no viewer UI at all above the logged-in world. The second owner
image is retained as `owner-ui-not-ok.png` with SHA-256
`423d3a34cb43ab9b1f264f04d176b96fa738a6cb5d46299e9b260650b7009c30`.
Neither run is a visual pass.

The second run's shutdown census is decisive: the production Metal UI encoded
5,049 frames, 509,986 batches and 19,454,172 vertices with zero eligible GL
fallback batches and every fallback-reason counter at zero. The failure was
presentation composition, not missing glyph rasterization or Metal batch
coverage: a transparent child `CAMetalLayer` does not reliably appear above
SDL's OpenGL drawable after its swap.

Commit `31fa27392e` follows the fail-closed cutover boundary. While the GL
world is still authoritative, it retains its complete visible UI. The same
font-atlas identity owns synchronized GL and Metal storage, and every glyph
publication updates both; ordered native world/UI capture continues sampling
the Metal copy. Only a complete fused Metal world/UI transaction may make the
surface opaque and suppress the matching GL swap. This is transitional
recovery, not feature deletion or a claim that the final native cutover is
complete; the GL atlas copy is removed with the complete presentation owner.

The rebuilt Release package remains `26.4.0.68285`; its viewer executable
SHA-256 is
`c9bf95b8a8d6c9f36c437c16b913feb5d3f1e9bcd77e9b5c69965992af2527a8`.
`tools/metal/verify_app.sh` accepted all 349 inspected Mach-O slices as ARM64
and deep strict code-sign verification passed. Under Metal API/GPU validation,
`llrender_gl_fresh_tests` passes 3,684 assertions in 110 cases; the focused
dual-atlas cases pass 36 assertions in two cases and the real-FreeType native
UI path passes 56 assertions. The older shared-context suite still reports its
known Apple GL fence/depth-format failures (5 assertions across 4 cases), none
in the changed font/UI ownership paths. `git diff --check` passes.

Owner-attended run `font-dual-recovery-20261104-01` accepted readable menu,
location, toolbar and FPS text. Metal evidence
`owner-metal-midday-lightbox.png` has SHA-256
`418b56cfdd491ca2d7259a9907018597217147dfd7655c961dcb2f9418bdad92`.
The owner explicitly excluded the temporarily absent Metal avatar from review.

Reference run `reference-midday-lightbox-20261104-01` used pinned executable
SHA-256 `fac5225435df2c096d10b856f6e0f186b37e41446576398ee8f4842b884ef28d`;
its image SHA-256 is
`be955aea1f385d000b660ff07812c71a49f32b40ec780052632588232167adbc`.
The gross brightness comparison is blocked as settings-inequivalent: live LEAP
showed Metal had drifted to dynamic exposure on / exposure 1.0, while reference
remained off / 1.1. That candidate state drift is itself open. Separately, the
Metal mirror was black while the reference showed reflected scene geometry, an
owner-attended NOT OK result. No autonomous screenshots were taken.

M-003, M-004, M-007, M-008, M-009 and M-011 are `doing`; M-012 is complete.
Commit `b27df920fc` completes direct ordinary/control-avatar probe enumeration,
including transparent skirt/eyelash/hair state, while full GL remains
fail-closed fallback. Indexed masks and remaining pass owners still block
removal of the GL resource gate, `display_cube_face` and cube array.

Earlier native terrain, startup/local/fetched resources, pressure, eviction,
accounting and scale-down checkpoints remain active. Native-authoritative
resources avoid GL names; dual backing remains only where complete per-frame GL
recovery is still required. Commits `04edf7b27d` and `c2bcd90b04` keep exact
ordered world/UI Metal presentation default-on, measurable and fail closed:
only a committed current-frame fused drawable suppresses its GL swap.

Prior preview, post-effect, world/UI and physical-coverage evidence remains in
the [archive index](archive/INDEX.md). Broader mirrors, projector shadows,
moving media, remaining application-owned GL and complete parity remain open.

## Visual review cadence

Per owner direction, Codex does not independently drive reference/candidate
viewers or take comparison snapshots. Visual cases accumulate in the
[owner-attended daily review queue](../work/active/visual-review-queue.md) and
are reviewed together at most once per day. No queued case is a parity pass
until the owner-attended review records that result.

## Build and test evidence

- Apple M1 Max host doctor passes; Release `26.4.0.68285` builds and packages.
- `verify_app.sh`: 349/349 ARM64 Mach-O slices; deep strict signing passes.
  Viewer SHA-256: `c9bf95b8a8d6c9f36c437c16b913feb5d3f1e9bcd77e9b5c69965992af2527a8`.
- Metal-validation: fresh renderer 3,684/110, dual atlas 36/2, real glyph/native
  UI 56/1. Shared-context baseline retains 5 known Apple GL failures/4 cases.
- Strict GL inventory remains 3,214 findings and exits 1. Both APIs remain
  linked; static, linked and runtime no-GL release gates remain open.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | System/default/local raw, decoded-memory and all eleven explicit local preloads publish natively with truthful state when GL is disabled; report screenshots, local previews, HDRI, fetched publication, sculpt metadata, pressure scale-down and eviction are backend-correct at converted sites. Exact presentation remains Metal-first/fail-closed. Live GL context creation/shutdown remains. |
| M-007 world/material | doing | Reflection-probe simple/material/PBR/bump opaque batches, tree, terrain, complete ordinary avatar/impostor, HDRI/WindLight sky and water are enumerated directly from CPU scene state; transforms/state are GL-independent. Remaining pass enumeration and broad parity remain open. |
| M-008 advanced views/effects | doing | Color grading, native PBR BRDF integration, model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF, HDR bloom/cross-filter and eligible non-HDR legacy glow/presentation have Metal implementations at their documented boundaries. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,214 and exits 1; feature/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Logical probes are backend-neutral and core opaque/bump, tree, terrain, ordinary avatar/impostor including transparent multipass, sky and water have a direct native producer. `LLCubeMapArray` remains live until every probe family, consumer and failure path is complete. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Probe avatars now feed direct Metal
producers for impostor, rigid, opaque skinned/cloth and transparent multipass
geometry while preserving selection, LOD, texture, state, order and failure
contracts. The complete fallback remains intact; no feature was deleted.

**Still incomplete:** the executable links OpenGL and still creates/shuts down a
live GL context. Application-owned GL traversal and resources remain. Complete
feature/stability, profiling/performance, visual parity and release gates are
not met. Prolonged stability and independent build/review evidence remain open.
The strict no-GL gate remains open.

## Exact next actions

1. Pause at the requested visual-validation checkpoint; the reference viewer
   remains owner-controlled.
2. After explicit resume, reproduce and repair exposure-setting drift, then
   repair black-mirror parity under exact matched settings.
3. Revalidate before continuing indexed pass removal or GL-gate cleanup.
