# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `e6f54ce64d`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,446 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Metal is the mandatory visible presentation owner. Startup/world GL swaps are suppressed; an incomplete native frame retains the previous Metal drawable and is machine-censused without claiming GL fallback. Live GL traversal/context creation/shutdown remain. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Native producers use the window-owned Metal device. Reflection probes use backend-neutral logical slots and Metal-only cube/radiance/SH publication; converted startup/local/HDRI/fetched resources retain native scale-down and eviction. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; wide-line/point preview diagnostics now have explicit native pipelines. |
| M-006 | UI, fonts, HUD and media ingress | doing | Font pages retain synchronized GL+Metal storage while retained GL consumers are removed; owner accepted readable UI text. Snapshot has no GL scratch/reconcile path. Selection effects plus HUD fullbright/shiny/emboss/transparent output are native-only; visual review remains queued. |
| M-007 | World scene and material correctness | doing | Probe scenes plus ordinary sky/cloud/body, avatars/impostors, fullbright, transparent alpha, water surface/exclusion/haze, atmospheric haze/sky-behind, legacy-material/bump, shiny/emboss and opaque/masked/blended glTF PBR batches are directly enumerated into Metal. Grass, trees and all main/probe directional/projector shadow generation are native; broad parity remains open. |
| M-008 | Advanced views/effects and compute | doing | Color grading, PBR integration/glow, previews, visual hints, avatar composites, selection, debug effects, material previews, AA, DoF, bloom/cross-filter and eligible legacy glow/presentation are implemented in Metal. Alpha-pool, water, atmosphere, sky, impostor, DoF-depth and debug-highlight production are native-only. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL (2,446 findings), linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | GL owns neither presentation, probe publication, terrain bake, selection, base deferred/PBR/legacy-material/legacy-bump/fullbright/transparent/water geometry, deferred sun/SSAO/base lighting, water exclusion/haze, atmospheric haze/sky-behind, main sky/cloud/body/avatar/impostor layers, main/probe shadow generation/storage/sampler/receiver state, shiny/emboss, glow, DoF/debug alpha, trees nor converted debug effects. Superseded owners are deleted. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — deferred sun, SSAO blur and base lighting Metal-only

### Retained owner evidence and native presentation boundary

Owner-attended run `font-dual-recovery-20261104-01` accepted readable menu,
location, toolbar and stable FPS text after two rejected candidates; exact
artifacts remain in the archive and visual queue. Commits through `68d8792028`
make Metal the sole post-startup presentation owner while incomplete native
work retains the preceding drawable. Commits `2836849036` and `cf6a821548`
remove Snapshot's GL scratch/reconcile path; visual revalidation remains open.

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

`68ea32d323` removes the retained main HDRI/WindLight sky, cloud, star,
sun/moon, meteor and aurora GL draws, eight GL programs and fourteen exclusive
GLSL stages. Explicit CPU matrices feed the existing mandatory Metal scene
transactions; probe/hero faces retain their direct native path. Visual case 48
is queued and did not block the source-proven cutover.

`ad53dfe567` removes the retained ordinary deferred avatar, avatar-alpha and
impostor draws, three GL programs and four exclusive GLSL stages. Direct Metal
capture now owns rigid/skinned avatars, impostors, hair/eyelash/skirt masks and
post-deferred transparency; the impostor bake includes transparent masks and
native shadow capture no longer double-submits the opaque-cut body. Preview
avatars remain a separate open owner. Visual case 49 is queued; neither viewer
was launched.

`dfbbad949e` replaces every main/probe directional and projector shadow draw
with direct Metal caster enumeration. It removes the GL shadow draw-pool API,
14 program owners, 20 exclusive GLSL stages and dead indexed shadow helpers.
Visual case 50 is queued; neither viewer was launched.

`dc5368ca5b` deletes the 12 pack and two projector `LLRenderTarget` shadow
objects, the non-Apple GL binding path and the obsolete GL shadow-sampler test.
The resource plan still supplies exact Metal descriptors; native slice validity
remains the fail-closed allocation/publication signal.

`1404b65e00` removes GL shadow comparison state and contracts the sampler table;
`8216ae2570` deletes receiver permutations, the 792-line GLSL utility, uniforms,
bindings and reservations. `629cd38c8a` splits native shadow constants from GL
and shrinks `UB_DEFERRED` from 512 to 80 SSAO/specular bytes; native Metal shadow
generation remains authoritative. `e6f54ce64d` then removes duplicate GL deferred
sun, SSAO blur and base-light draws, six programs, nine GLSL stages and obsolete tests.

Earlier terrain, water, startup/local/fetched resources, pressure, eviction,
accounting and scale-down checkpoints remain active. Native-authoritative
resources avoid GL names; dual backing remains only where retained GL consumers
still exist. Commits `04edf7b27d`, `c2bcd90b04` and `68d8792028` make ordered
world/UI Metal presentation measurable and fail closed without exposing a GL
swap. Exact prior evidence remains in the
[archive index](../../state/archive/INDEX.md).

## Validation checkpoint

- `e6f54ce64d`: Ninja and prescribed Xcode Release 68370 pass
  warnings-as-errors; physical validated Metal shadow 119/1 and retained-world
  8,022/1 pass. Release inspection is 349/349 ARM64 and deep signing passes.
  Executable SHA-256 is
  `1042e1a8e28e30260b86d164743aa445833d9710b61dbff84ae6cb7dff589d18`;
  metallib is `581fc1df32c6e76f77087353b483466428877c5a89b2fe2b92df296bb06a164f`.
  Six focused resource/projection/shadow/CAS tests pass. Host doctor passes on
  the M1 Max acceptance host. Strict inventory is 2,446 and exits 1; harness
  validation and all 31 tool tests pass.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—Metal is the only visible presentation owner,
   Snapshot has no GL framebuffer, and ordinary avatar/impostor plus all
   main/probe shadow-generation families feed direct native producers.
2. **Replacement boundary:** obsolete probe allocations and remaining GL-owned
   resource/draw owners still require native cutover.
3. **Deferred critical work:** remaining GL context/traversal/resources,
   feature/stability matrices, visual queue, strict binary/runtime audit,
   measured M1 Max performance and release proof.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Move projector-shadow target selection into the native projector traversal,
then delete duplicate GL local/projector lighting draws, their programs, GLSL
and light-map resources. Preserve distinct post-deferred traversal side effects.
Do not wait for queued visual review, restore fallbacks or launch viewers.

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
