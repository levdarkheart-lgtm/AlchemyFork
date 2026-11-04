# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `04d52899ae`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 3,165 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Metal is the mandatory visible presentation owner. Startup/world GL swaps are suppressed; an incomplete native frame retains the previous Metal drawable and is machine-censused without claiming GL fallback. Live GL traversal/context creation/shutdown remain. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Native producers use the window-owned Metal device. Reflection probes use backend-neutral logical slots and Metal-only cube/radiance/SH publication; converted startup/local/HDRI/fetched resources retain native scale-down and eviction. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; wide-line/point preview diagnostics now have explicit native pipelines. |
| M-006 | UI, fonts, HUD and media ingress | doing | Font pages retain synchronized GL+Metal storage while retained GL consumers are removed; owner accepted readable UI text. Snapshot no longer allocates/binds/clears a GL scratch target or reconciles GL targets for capture size, and retries one exact-size Metal render after temporal-resource resize; preview/Refresh/Save-to-Disk await owner revalidation. |
| M-007 | World scene and material correctness | doing | Probe G-buffer and post-deferred families are enumerated directly from CPU scene state, including sorted alpha, transparent avatars, atmosphere, water haze/exclusion/surface and emissive replay. Broad parity remains open. |
| M-008 | Advanced views/effects and compute | doing | Color grading, PBR BRDF integration, model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF, HDR bloom/cross-filter and eligible legacy non-HDR glow/presentation are implemented in Metal. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL (3,165 findings), linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | GL owns neither presentation nor reflection-probe publication. Probe updates fail closed; GL processing/storage and post-deferred pool draws are removed from probe faces. Retained GL face clear/deferred-lighting drivers, context and resource side effects remain. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — remove visible GL fallback and snapshot framebuffer

### Owner-verified transition UI and native atlas ownership

Owner-attended run `font-cache-replay-fix-20261104-01` logged in but was rejected:
menu, location-bar and toolbar text were absent, and the changing FPS field
flashed glyph rectangles. The rejected executable SHA-256 is
`49d5cf47f17e9e740dab38aaee48ef76ec5dd540afb7b9bc83f26522b62695e6`;
its owner-supplied rejection image is retained in the run directory.

The follow-up run `font-metal-overlay-20261104-01` is also rejected. It encoded
509,986 production Metal UI batches with zero eligible fallback batches but no
viewer UI appeared over the GL world. This isolates the failure to transparent
child-layer composition above SDL's OpenGL drawable rather than Metal glyph or
batch coverage. Its retained owner image SHA-256 is
`423d3a34cb43ab9b1f264f04d176b96fa738a6cb5d46299e9b260650b7009c30`.

Commit `68d8792028` removes visible GL fallback. After startup the native layer
stays opaque, world/login GL swaps are suppressed and incomplete native work
retains the previous Metal drawable for a retry. Presentation telemetry now
distinguishes this native-only failure from historical GL fallback. This does
not remove legacy GL traversal, resource side effects or the live context.

Commit `2836849036` removes Snapshot's GL scratch target, GL clear and GL size
limit, performs fail-closed Metal readback and permits one identical-size retry
for resized temporal resources. Hero mirrors participate in normal native
presentation without a hidden validation setting. Owner revalidation of
Snapshot and mirrors remains queued.

Commit `cf6a821548` removes the remaining Snapshot-size call into
`rebuildGLBuffers`. The Darwin override validates both requested and scaled
dimensions against the native device and feeds the exact Metal capture size
directly; unsupported platforms decline rather than fall back. The visual row
stays open because source/build success does not establish floater behavior.

The rebuilt `26.4.0.68285` Release viewer is pinned at
`c9bf95b8a8d6c9f36c437c16b913feb5d3f1e9bcd77e9b5c69965992af2527a8`;
349/349 inspected Mach-O slices are ARM64 and deep strict code-sign verification
passes. Metal-validation runs pass 3,684/110 for
`llrender_gl_fresh_tests`, 36/2 for the dual-atlas cases and 56/1 for the
real-FreeType native UI path. Owner-attended run
`font-dual-recovery-20261104-01` accepted readable menu, location, toolbar and
stable numeric FPS text. Broader font-atlas validation remains queued.

Commits `7a9878949d` and `51425778e1` remove the GL image registry as the
Metal-device authority. `10e08508eb` makes scheduled and realtime probe
transactions fail closed. `437d8a584e` deletes the GL probe post-chain,
scratch resources, cube/SH publication storage and reflection UBO. Direct Metal
radiance and matching SH are now the only published probe generation.

Commits `51282740b5` through `04d52899ae` replace retained reflection scene
triggers with direct CPU enumeration. The native probe transaction now owns the
opaque G-buffer and complete ordered post-deferred scene: water exclusion,
fullbright/shiny/bump/glow, transparent avatars, water haze, sorted pre/post
water alpha, atmosphere and water. Probe faces return before GL pool, stencil,
fullscreen haze and water draws. Remaining GL face clear/deferred-lighting
drivers and resource/context ownership are the next cut, not a fallback.

Earlier terrain, water, startup/local/fetched resources, pressure, eviction,
accounting and scale-down checkpoints remain active. Native-authoritative
resources avoid GL names; dual backing remains only where retained GL consumers
still exist. Commits `04edf7b27d`, `c2bcd90b04` and `68d8792028` make ordered
world/UI Metal presentation measurable and fail closed without exposing a GL
swap. Exact prior evidence remains in the
[archive index](../../state/archive/INDEX.md).

## Validation checkpoint

- Release `26.4.0.68285` builds and packages warnings-as-errors. Viewer SHA-256
  is `c9bf95b8a8d6c9f36c437c16b913feb5d3f1e9bcd77e9b5c69965992af2527a8`.
- `verify_app.sh`: 349/349 ARM64 Mach-O slices; deep strict signing passes.
- Metal-validation: fresh renderer 3,684/110, dual atlas 36/2, real glyph/native
  UI 56/1. Shared baseline retains 5 known Apple GL failures/4 cases.
- `04d52899ae`: warnings-as-errors RelWithDebInfo build passes; focused
  benchmark tests pass 49/8, `alrender_tests` 557,301/136, harness 31/31 and
  app verification 349/349 ARM64 slices. Deep strict signing passes; executable
  SHA-256 is `8d9cfb66f49de3efd6320f3b3531323f757789ccc5f1c5699ab86f57c7f3593d`.
- Strict inventory is 3,165 findings and exits 1; both APIs remain linked.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—Metal is the only visible presentation owner,
   Snapshot has no GL framebuffer, and ordinary avatar/impostor scene families
   feed direct native producers.
2. **Replacement boundary:** indexed masks and remaining pool/face-owned scene
   families plus cube-array consumption remain until complete capture is proven.
3. **Deferred critical work:** remaining GL context/traversal/resources,
   feature/stability matrices, visual queue, strict binary/runtime audit,
   measured M1 Max performance and release proof.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Remove the retained GL face clear and deferred-lighting/fullscreen traversal now
that reflection G-buffer and post-deferred scene enumeration is native. Do not
wait for Snapshot, exposure, mirror or direct-probe visual review: native paths
intentionally expose failures instead of reverting to GL. Continue owner-by-
owner GL resource and context removal without restoring publication or
presentation fallbacks.
Keep visual cases queued and do not launch viewers autonomously.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not let an unreviewed visual queue row block source- and test-proven GL
  authority removal.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
