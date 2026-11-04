# Current engineering state

**Observed:** 2026-11-04 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `15d7a3b77c`

**Historical evidence:** [archive index](archive/INDEX.md)

## Active implementation checkpoint

### Metal-only presentation and selection authority; visual failures queued

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

Commit `68d8792028` makes the opaque Metal surface the sole presentation owner
after startup and removes the transitional presentation setting. The viewer no
longer swaps the application OpenGL drawable after `STATE_PRECACHE`; an
incomplete current native transaction retains the preceding Metal drawable and
retries. Legacy GL traversal still runs for unconverted rendering and resource
side effects, so this is presentation-authority removal, not a no-GL claim.
Font pages still keep synchronized GL and Metal storage until the retained GL
consumers are removed.

Commit `2836849036` also removes the Snapshot path's GL scratch framebuffer,
GL clears and GL size limit. Native snapshot readback now uses the Metal device
limit and may render the same requested size twice so resized temporal targets
can stabilize before fail-closed readback. Hero mirrors are no longer excluded
from native presentation and no hidden validation setting remains. Snapshot and
mirror fixes are queued for owner revalidation, not claimed from source alone.

Commit `cf6a821548` removes Snapshot's remaining GL render-target reconcile.
On Darwin the scoped override now validates the requested and resolution-scaled
dimensions against the selected Metal device, records only the exact native
capture size and restores no GL target. Unsupported platforms decline rather
than enter a legacy fallback. This is source/build evidence; case 30 remains
open for owner-attended preview, Refresh and Save-to-Disk revalidation.

The last fully verified Xcode Release package remains `26.4.0.68285`; its viewer executable SHA-256 is
`9d8f6b1313ef0c1bf7cae936a30152b3b1c5ab60728d126809fa3ea0bb8f284f`.
`tools/metal/verify_app.sh` accepted all 349 inspected Mach-O slices as ARM64
and deep strict code-sign verification passed. `git diff --check` passes.

Owner-attended run `font-dual-recovery-20261104-01` accepted readable menu,
location, toolbar and FPS text. Metal evidence
`owner-metal-midday-lightbox.png` has SHA-256
`418b56cfdd491ca2d7259a9907018597217147dfd7655c961dcb2f9418bdad92`.
The owner explicitly excluded the temporarily absent Metal avatar from review.

The first brightness pair was settings-inequivalent and is superseded. Clean
owner-attended runs `metal-midday-lightbox-rerun-20261104-01` and
`reference-midday-lightbox-rerun-20261104-01` forced dynamic exposure off and
exposure 1.1, matched all other asserted controls and ended at identical global
position `[261482.3005, 241431.3658, 24.5637]`. Metal image SHA-256 is
`4aa3fb673ca3c0e650288cf36daf41d78e7fa0d3c08aaf94b175e097c501fc9e`;
reference is `9330a018b41a00cf55b9056c7c6a8716129db3d34cf6f30765ccea533b6d79e1`.
Window dimensions were close (2042x1538 versus 2054x1544). Metal remains much
brighter with clipped pavement/foliage highlights and reduced tonal range:
NOT OK. Its mirror is black while reference reflects sky, scene and avatar:
NOT OK. The owner also reports the Metal Snapshot floater opens without a
preview, Refresh has no effect and Save to Disk performs no action: NOT OK.
Both viewers are closed; no autonomous screenshots were taken.

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
Commits `95619cb3e5` through `d0da9cae1a` remove duplicate GL far-plane, wind, self-agent crosshair and light-debug draws/state eligibility; `624441a65e` cuts the tree GL owner; `79fbf30233` cuts base deferred GL geometry; `15d7a3b77c` directly enumerates opaque/masked scalar/indexed static/rigged glTF PBR into Metal and deletes four GL programs, two shaders and the unreachable full-map GL helper mode. Visual parity is queued.

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

- Apple M1 Max host doctor passes; Release `26.4.0.68285` builds and packages.
- `verify_app.sh`: 349/349 ARM64 Mach-O slices; deep strict signing passes.
  Viewer SHA-256: `c9bf95b8a8d6c9f36c437c16b913feb5d3f1e9bcd77e9b5c69965992af2527a8`.
- Metal-validation: fresh renderer 3,684/110, dual atlas 36/2, real glyph/native
  UI 56/1. Shared-context baseline retains 5 known Apple GL failures/4 cases.
- At `ffe4e79dc3`, Ninja/Xcode, selection GPU 4,732/1, benchmark 49/8, renderer
  557,301/136, tools 31/31, 349/349 ARM64 and signing pass; executable SHA-256
  is `9d8f6b1313ef0c1bf7cae936a30152b3b1c5ab60728d126809fa3ea0bb8f284f`.
- At `15d7a3b77c`, RelWithDebInfo harness build/staging, physical world/G-buffer 8,022/1 and direct-shadow 119/1 pass; `verify_app.sh` accepts 349/349 ARM64 slices and deep signing passes.
  Executable SHA-256 is `3aced9f3143426b72415c9e0146df19816e68ed98e7e10942615319624d36fb6`; strict inventory is 2,959. Runtime/visual parity remains open.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Native producers use the window-owned Metal device rather than the GL image registry. Converted resources/previews retain truthful native state. Metal solely presents; Snapshot has no GL capture target; selection effects use native targets only. Live GL traversal/context remain. |
| M-007 world/material | doing | Reflection-probe scene families plus ordinary and opaque/masked glTF PBR static/rigged G-buffer batches are directly enumerated into Metal. Grass, tree and shadow batches are native; broad material/scene parity remains open. |
| M-008 advanced views/effects | doing | Color grading, PBR integration, previews, visual hints, avatar composites, selection, far-plane/wind/agent/light debug effects, material previews, AA, DoF, bloom/cross-filter and eligible legacy glow/presentation have Metal implementations. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 2,959 and exits 1; feature/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Probe publication, terrain bake, selection, base deferred and opaque/masked PBR geometry, trees and converted debug effects have direct Metal owners. Their obsolete GL storage, targets, programs, shaders, state dependencies and draws are deleted. |
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

1. Continue through the next retained GL resource/draw owner.
2. Re-run strict source/link/runtime gates after each owner cut.
3. Keep visual failures queued; do not launch either viewer autonomously.
