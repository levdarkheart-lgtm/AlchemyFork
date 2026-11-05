# Current engineering state

**Observed:** 2026-11-05 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `c965068e2f`

**Historical evidence:** [archive index](archive/INDEX.md)

## Active implementation checkpoint

### Metal-only presentation, lighting and selection highlights; visual failures queued

Two owner-attended UI candidates were rejected before the accepted font run.
The latter encoded 5,049 Metal UI frames, 509,986 batches and 19,454,172
vertices with no eligible GL fallback, isolating the fault to transparent child
layer composition over the swapped SDL GL drawable. Exact run IDs, executable
and image hashes remain in the active ledger and visual queue.

Commit `68d8792028` makes the opaque Metal surface the sole presentation owner
after startup and removes the transitional presentation setting. The viewer no
longer swaps the application OpenGL drawable after `STATE_PRECACHE`; an
incomplete current native transaction retains the preceding Metal drawable and
retries. Legacy GL traversal still runs for unconverted rendering and resource
side effects, so this is presentation-authority removal, not a no-GL claim.
Font pages still keep synchronized GL and Metal storage until the retained GL
consumers are removed.

Commits `2836849036` and `cf6a821548` remove Snapshot's GL scratch/reconcile
path. Fail-closed Metal readback owns the validated size; visual case 30 remains
open.

The source-exact Xcode Release package is `26.4.0.68386`; its viewer executable SHA-256 is `445f0d5309855c11aa92892afa1469382e130e3350fc130db540ad8ec7bdb925`.
`tools/metal/verify_app.sh` accepted all 349 inspected Mach-O slices as ARM64
and deep strict code-sign verification passed. `git diff --check` passes.

Owner-attended run `font-dual-recovery-20261104-01` accepted readable menu,
location, toolbar and FPS text; exact image hashes remain in visual case 9.
Matched midday/Lightbox reruns still found Metal overbright/clipped and its
mirror black: NOT OK. Snapshot preview, Refresh and Save to Disk also remain
NOT OK. Exact settings, positions and hashes remain in cases 29/30. Both
viewers are closed; no autonomous screenshots were taken.

M-003, M-004, M-007, M-008, M-009 and M-011 are `doing`; M-012 is complete.
Commits through `f04b0b6306` make probes fail closed and directly enumerate the
complete G-buffer/post-deferred scene, lighting and directional-shadow casters.
`23da75f187` extends that native authority to hero/mirror faces, removes their
GL gaussian/mip/radiance producer and makes both probe families use planned
Metal G-buffer/shadow dimensions. `d80a941a60` renders projector-shadow casters
into planned Metal slices. Probe faces never use retained GL projector targets;
missing resources fail closed. `2cea68f5de` deletes the hero GL storage/filter
stack; `21562a1991` deletes four obsolete ordinary-probe GL programs and their
six shaders. `945f071acf` deletes the dead GL terrain-paint bake program and its
two shaders while retaining the tested native bake and live precision input.
`210380eb74` makes native selection ID/depth, alpha cuts, rigging, wireframe,
contour, hover glow, contrast and HUD authoritative and deletes 13 GLSL files.
`ffe4e79dc3` deletes six GL targets, their palette texture, eight GL programs, helpers and the obsolete 3,892-line GL test.
Commits `95619cb3e5` through `d0da9cae1a` remove duplicate GL debug draws; `624441a65e` cuts trees; `79fbf30233` cuts base deferred geometry; `15d7a3b77c` cuts opaque/masked PBR G-buffer GL; `8bd478c19f` cuts opaque PBR glow; `a143d2789e` cuts legacy materials; `189908fc41` and `eab6bd61b3` cut main/alpha-pool glow; `35f43b29e0` cuts legacy bump; `dbff358ab5`/`9d8f96ac6b` cut world/HUD shiny/emboss; `1d0bc66fd5` cuts fullbright; `87546357a2` cuts transparent alpha/impostor/DoF/debug; `7732ab92bb` cuts water surface/exclusion draws, scratch targets, seven GL programs, ten GLSL stages and obsolete GL water tests. Native water features remain in Metal; visual parity is queued.

`12e19499ed` removes the obsolete GL water-refraction target and stops ordinary
resource planning from allocating the unused fallback haze depth copy.
`1f453941fb` then makes atmospheric haze, water haze and sky-behind composition
Metal-only. It deletes the GL haze and water-field draws, target/query/resource
owners, four shader programs and six exclusive GLSL stages while retaining the
already implemented Metal sky-behind, atmosphere and above-/below-water haze
transactions. Visual parity remains queued; neither viewer was launched.

`68ea32d323` makes the main HDRI/WindLight sky family Metal-only. Explicit
camera/dome/body/star matrices now feed native full/split sky, clouds, stars,
sun/moon, meteors and aurora transactions. The retained GL draw methods, eight
program owners and fourteen exclusive GLSL stages are deleted. Probe/hero
faces retain their direct native path. Visual case 48 is queued; no viewer was
launched or captured.

`ad53dfe567` makes ordinary main-view avatars and impostors Metal-only. Direct
capture owns rigid/skinned bodies, impostors, hair/eyelash/skirt masks and
post-deferred transparency; the impostor bake now includes transparent masks.
Three GL programs and four exclusive GLSL stages are deleted, and native
shadow capture no longer draws the opaque-cut body twice. Preview avatars remain
a separate owner. Visual case 49 is queued; no viewer was launched or captured.

`dfbbad949e` makes every main/probe directional and projector shadow slice
Metal-only. Direct caster enumeration replaces the GL draw-pool traversal; the
GL shadow API, 14 program owners, 20 exclusive GLSL stages and dead indexed
helpers are deleted. Visual case 50 is queued; no viewer was launched or
captured.

`dc5368ca5b` removes the 14 retained GL shadow targets, the non-Apple target
binding path and the obsolete 555-line GL shadow-sampler test. The backend-neutral
plan remains the Metal size/format contract. Native allocation failures remain
fail-lit slice-validity failures; no GL storage can mask them.

`1404b65e00` removes GL shadow comparison state; `8216ae2570` deletes receiver
state and the 792-line utility; `629cd38c8a` separates native shadow constants.
`e6f54ce64d` deletes GL deferred sun, SSAO blur and base-light draws, six program
owners, nine GLSL stages and three obsolete tests/benchmarks. Visual case 51 is queued.
`1f3cfeefb6` deletes duplicate GL point/projector lighting; `3e975c4b28` then deletes the three dead pack light-map slots, blurred scratch target and obsolete input.
Metal retains its private light-map/blur implementation; the default 1440p plan sheds 21.1 MiB. Visual cases 51/52 remain queued.
`cbdecfd337` separates native sun/moon direction preparation from GL light setup and deletes the drawless vertex/cull/blend/color-mask state prologue around the native deferred-light chain. `fa746d4cb8` makes direct Metal face capture the sole Darwin edit/touch-highlight producer and deletes four GL programs plus five exclusive GLSL stages. `5d1bf5a394` removes unreachable GL pathing rendering; `497759ebb4` removes its overlay gate. `bbc4be43d9` removes duplicate GL object-update blips, and `c965068e2f` removes the duplicate GL reflection-probe-volume fullscreen draw, its program and two exclusive GLSL stages while retaining their native owners. Visual cases 53/54 are queued. Independent pathfinding UI/data/network code remains. No viewer was launched.
Earlier native terrain, startup/local/fetched resources, pressure, eviction,
accounting and scale-down checkpoints remain active. Native-authoritative
resources avoid GL names; dual backing remains only where retained GL consumers
have not yet been removed. Commits `04edf7b27d`, `c2bcd90b04` and `68d8792028`
make exact ordered world/UI Metal presentation measurable and fail closed. A
failed native frame retains the preceding Metal drawable; no GL swap is shown.

Prior preview, post-effect, world/UI and physical-coverage evidence remains in
the [archive index](archive/INDEX.md). Broader mirrors, projector-shadow visual
parity, moving media, remaining application-owned GL and complete parity remain open.

## Visual review cadence

Per owner direction, Codex does not independently drive reference/candidate
viewers or take comparison snapshots. Visual cases accumulate in the
[owner-attended daily review queue](../work/active/visual-review-queue.md) and
are reviewed together at most once per day. No queued case is a parity pass
until the owner-attended review records that result.

## Build and test evidence

- At `c965068e2f`, Ninja and prescribed Xcode Release 68386 pass warnings-as-errors; the
  physical retained-world case passes 8,022/1 under Metal
  API, GPU, shader and load/store validation. Release verification passes 349/349 ARM64 slices and
  deep strict signing. Executable SHA-256 is
  `445f0d5309855c11aa92892afa1469382e130e3350fc130db540ad8ec7bdb925`;
  metallib SHA-256 remains `581fc1df32c6e76f77087353b483466428877c5a89b2fe2b92df296bb06a164f`.
  All 556,530 renderer assertions and the 2,597-assertion focused resource-plan suite pass. M1 Max host doctor passes.
  Strict inventory is 2,355 and exits 1 as expected; no audit rule changed. Harness validation and 31/31 tool tests pass.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Native producers use the window-owned Metal device rather than the GL image registry. Dead GL light-map plan resources are deleted. Metal solely presents; Snapshot has no GL capture target; selection plus HUD fullbright/shiny/emboss/transparent alpha use native targets only. Live GL traversal/context remain. |
| M-007 world/material | doing | Probe scenes plus ordinary sky/cloud/body, avatars/impostors, fullbright, transparent alpha, water surface/exclusion/haze, atmospheric haze/sky-behind, legacy-material/bump, shiny/emboss and opaque/masked/blended glTF PBR batches are directly enumerated into Metal. Grass, trees and all main/probe directional/projector shadow generation are native; broad parity remains open. |
| M-008 advanced views/effects | doing | Color grading, PBR integration/glow, previews, visual hints, avatar composites, selection, debug effects, material previews, AA, DoF, bloom/cross-filter and eligible legacy glow/presentation have Metal implementations. Alpha-pool, water, atmosphere, sky, impostor, DoF-depth and debug-highlight production are native-only. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 2,355 and exits 1; feature/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Probe publication and volume display, terrain bake, selection/edit highlights, base deferred/PBR/legacy/fullbright/transparent/water geometry, deferred lighting, atmosphere/sky/avatar layers, shadows, shiny/emboss, glow, DoF/debug alpha, trees and converted debug effects have direct Metal owners. Superseded GL owners, drawless deferred-light state and unreachable OSS pathing render code are deleted. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Metal owns every visible viewer frame;
Snapshot has no GL framebuffer, probe shadows and selection effects render
directly in Metal, and failed native work fails closed.

**Still incomplete:** the executable links OpenGL and still creates/shuts down a
live GL context. Application-owned GL traversal and resources remain. Complete
feature/stability, profiling/performance, visual parity and release gates are
not met. Prolonged stability and independent build/review evidence remain open.
The strict no-GL gate remains open.

## Exact next actions

1. Remove the duplicate GL particle-raycast crosshair lines while retaining the existing fail-closed Metal capture.
2. Re-run strict source/link/runtime gates after each owner cut.
3. Keep visual failures queued; do not launch either viewer autonomously.
