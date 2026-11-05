# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `1f453941fb`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,676 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Metal is the mandatory visible presentation owner. Startup/world GL swaps are suppressed; an incomplete native frame retains the previous Metal drawable and is machine-censused without claiming GL fallback. Live GL traversal/context creation/shutdown remain. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Native producers use the window-owned Metal device. Reflection probes use backend-neutral logical slots and Metal-only cube/radiance/SH publication; converted startup/local/HDRI/fetched resources retain native scale-down and eviction. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; wide-line/point preview diagnostics now have explicit native pipelines. |
| M-006 | UI, fonts, HUD and media ingress | doing | Font pages retain synchronized GL+Metal storage while retained GL consumers are removed; owner accepted readable UI text. Snapshot has no GL scratch/reconcile path. Selection effects plus HUD fullbright/shiny/emboss/transparent output are native-only; visual review remains queued. |
| M-007 | World scene and material correctness | doing | Probe scenes plus ordinary fullbright, transparent alpha, water surface/exclusion/haze, atmospheric haze/sky-behind, legacy-material/bump, shiny/emboss and opaque/masked/blended glTF PBR static/rigged batches are directly enumerated into Metal. Grass, trees and shadows are native; broad parity remains open. |
| M-008 | Advanced views/effects and compute | doing | Color grading, PBR integration/glow, previews, visual hints, avatar composites, selection, debug effects, material previews, AA, DoF, bloom/cross-filter and eligible legacy glow/presentation are implemented in Metal. Alpha-pool, water, atmosphere, impostor, DoF-depth and debug-highlight production are native-only. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL (2,676 findings), linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | GL owns neither presentation, probe publication, terrain bake, selection, base deferred/PBR/legacy-material/legacy-bump/fullbright/transparent/water geometry, water exclusion/haze, atmospheric haze/sky-behind, world/HUD shiny/emboss, glow, impostor/DoF/debug alpha, trees nor converted debug effects. Superseded owners are deleted. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — remove visible GL fallback and selection stack

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

Commits `51282740b5` through `f04b0b6306` replace retained reflection scene,
lighting and directional-shadow triggers with direct CPU/Metal ownership.
`23da75f187` extends that direct path to hero/mirror faces, removes their GL
gaussian/downsample/mip/radiance producer, takes all native face/G-buffer/shadow
dimensions from the backend resource plan and defers a face when its native
transaction cannot start. No hidden GL face can publish or mask the failure.
`d80a941a60` adds direct Metal projector-shadow production for reflection and
hero faces. It enumerates CPU casters with explicit light matrices, writes the
planned native slices, caches only valid native work and never binds, clears,
flushes or draws retained GL projector targets during cube capture.
`2cea68f5de` removes the now-dead hero GL cube array, scratch target, mip chain,
filter vertex buffer, gaussian/mip/hero-radiance programs and retained GL hero
binding. Logical scheduling and publication identity now reconcile directly
with the native resource plan.

`21562a1991` deletes the ordinary-probe radiance and SH projection GL programs,
their six exclusive GLSL files and the obsolete two-pass projection setting.
The native Metal cube/radiance/SH path remains authoritative; the analytical
SH oracle now documents and verifies the native packed-coefficient contract.

`945f071acf` deletes the unreferenced terrain-paint GL bake program and its two
exclusive shaders. Production paintmap baking remains owned by
`ALMetalTerrainPaintRenderer`; the live 1-to-8-bit quantization input remains
and its physical Metal test covers the native height/noise bake.

`210380eb74` and `ffe4e79dc3` make selection/highlight authoritative in Metal,
delete 13 GLSL files, six GL targets, the palette texture, eight GL programs
and the obsolete GL test; its physical case passes 4,732 assertions.
Commits `95619cb3e5` through `d0da9cae1a` remove duplicate GL far-plane, wind,
self-agent crosshair and light-debug draws/state checks; `624441a65e` cuts trees; `79fbf30233` cuts base deferred geometry; `15d7a3b77c` cuts opaque/masked PBR G-buffer GL; `8bd478c19f` cuts opaque PBR glow; `a143d2789e` cuts legacy materials; `189908fc41`/`eab6bd61b3` cut main/alpha-pool glow; `35f43b29e0` cuts legacy bump; `dbff358ab5`/`9d8f96ac6b` cut world/HUD shiny/emboss; `1d0bc66fd5` cuts fullbright; `87546357a2` cuts transparent alpha/impostor/DoF/debug; `7732ab92bb` cuts water surface/exclusion draws, scratch targets, seven GL programs, ten GLSL stages and obsolete GL water tests. Visual parity is queued.

`12e19499ed` removes the obsolete GL water-refraction target and ordinary
fallback depth-copy allocation. `1f453941fb` removes the retained GL
atmospheric-haze, water-haze, water-field-query and sky-behind owners, four GL
programs and six exclusive GLSL stages. The corresponding Metal atmosphere,
sky-behind and above-/below-water haze implementations remain mandatory. Their
owner-attended visual review is queued and did not block the source cutover.

Earlier terrain, water, startup/local/fetched resources, pressure, eviction,
accounting and scale-down checkpoints remain active. Native-authoritative
resources avoid GL names; dual backing remains only where retained GL consumers
still exist. Commits `04edf7b27d`, `c2bcd90b04` and `68d8792028` make ordered
world/UI Metal presentation measurable and fail closed without exposing a GL
swap. Exact prior evidence remains in the
[archive index](../../state/archive/INDEX.md).

## Validation checkpoint

- `7732ab92bb`: source-exact Release 68351; physical world/water 8,022/1 and wave occlusion 250/1; CPU water field 166,400/21, height bake 386,067/8, far-plane/MSL 336/9 and tessellation 47/8 pass; 349/349 ARM64/deep signing pass; executable SHA-256 `2191fa5788cdc6778aeff1b8dd20a0d6146ecfe4cd1e247c933dda31c4fb1040`; strict inventory 2,701 exits 1. Runtime/visual parity remains open.
- `1f453941fb`: focused resource/far-plane tests pass 3,340/43; the physical
  retained-world case passes 8,022/1 under Metal API, GPU, shader and
  load/store validation. Ninja/Xcode Release 68354 passes warnings-as-errors,
  349/349 ARM64 inspection and deep strict signing. Executable SHA-256 is
  `7c1ff31183ad3024be17229a84eec6c6bb76693baa37198dbb3c2eb3faa069bd`;
  packaged metallib SHA-256 is
  `581fc1df32c6e76f77087353b483466428877c5a89b2fe2b92df296bb06a164f`.
  Strict inventory is 2,676 and exits 1 as expected; no audit rule changed.

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
2. **Replacement boundary:** obsolete probe allocations and remaining GL-owned
   resource/draw owners still require native cutover.
3. **Deferred critical work:** remaining GL context/traversal/resources,
   feature/stability matrices, visual queue, strict binary/runtime audit,
   measured M1 Max performance and release proof.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Audit the retained main-path WindLight sky/cloud GL draws against the native
capture and remove the next proven duplicate owner. Do not wait for queued
visual review, and never restore publication or presentation fallbacks.
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
