# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `daa19101a9` (`fix(metal): reject uncaptured world overlays`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `daa19101a9` closes two
reachable native-publication eligibility holes. Pathfinding visualization can
draw or clear the world for navmesh, path, shape, water and selected-character
capsule modes without setting the general render-debug mask. The developer
**Show Updates to Objects** mode can independently emit depth-tested transient
blips. Both occur after retained native world replay and outside ordered UI,
but previously left native presentation eligible and could disappear silently.

The explicit overlay policy now rejects publication only while either output
actually contributes. Hidden/default pathfinding floaters do not block Metal;
active capsule, console/path output or nonempty blips keep the exact GL frame
authoritative. The existing render-debug, wireframe, reflection-probe-volume
and far-plane boundaries remain unchanged. Native pathfinding visualization is
now a concrete critical M-008/M-009 follow-up; unlike exact RLVa parity, it is
not deferred as a nice-to-have.

Prior commit `8361addd0f` closes a
reachable native-output omission in the viewer's **Highlight Transparent**
diagnostic. `LLDrawPoolAlpha::renderDebugAlpha()` rendered static and rigged
alpha classifications after the retained native world replay, and it also
rendered HUD attachments inside the ordered UI transaction. Native publication
could nevertheless remain eligible without either overlay, so a published
Metal frame could silently omit a core editing/diagnostic feature.

The exact draw boundary now captures the smoke texture, transform, red/blue/
green classification and avatar skin palette. World replay selects static or
rigged Metal pipelines and uses an RGB-only write mask so diagnostic blending
does not corrupt retained glow/coverage alpha. HUD replay uses the ordered UI
renderer with the live GL blend, depth, cull, sampler, viewport and color-mask
state. Missing geometry, textures or palettes fail the corresponding native
transaction closed while the migration GL draw remains intact.

This is a core native-output repair, not exact RLVa work. RLVa can gate whether
the viewer exposes Highlight Transparent, but exact RLVa behavior remains the
owner-deferred M-013 nice-to-have. Minimum RLVa support is required only where
needed to prevent Metal authority or viewer-owned OpenGL removal from blocking.

The preceding bounded transparency/material production audit is closed without
a speculative shader edit. Active legacy material and PBR alpha equations,
cutoffs, sampler/color-space use, specular AA, shadows, probes/SSR/hero inputs,
glare-derived coverage, fog, local lights, custom blend state and glow replay
showed no reachable divergence for valid viewer-produced data. The concrete
Highlight Transparent omission was the next missing output owner.

Any candidate/reference shadow comparison is admissible only when shadows are
enabled in Graphics Preferences in **both** viewers before capture. A both-off
control can isolate a shadow residual, but it cannot prove shadow parity.

The persistent M-013 inventory is [the deferred RLVa backlog](../work/deferred/rlva-parity.md).
It records the mandatory comparison prerequisite: enable RLV in Preferences in
both viewers, save, and restart both viewers before testing.

## Build and test evidence

- Focused physical RelWithDebInfo cases passed under Metal API/GPU/load-store/
  shader validation: world replay 1,364 assertions and ordered HUD replay 485
  assertions. They cover static and rigged geometry, RGB-only alpha retention,
  palette translation, invalid-palette rejection and the unchanged HUD PBR ABI.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  45,015 assertions across 52 cases under the same validation layers. The new
  seven-assertion policy case proves every uncaptured overlay class rejects
  exact publication.
- Warnings-as-errors RelWithDebInfo objects for the Metal renderer, built-in
  MSL, GPU test, `lldrawpoolalpha.cpp` and `pipeline.cpp` compiled and linked;
  the updated Metal and pipeline objects also compile in Debug.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passed 484/484 in 256.86 seconds.
- Source-exact warnings-as-errors Release Development build `26.4.0d68038`
  linked, staged and signed at committed code head `daa19101a9`.
- Executable SHA-256:
  `be70f1e858e43c1d0d059f8369dfbfff1baa766a6770f1e466dfc8e1c37c01b3`.
  Packaged MSL SHA-256:
  `21093c0c3ef9481e55327ecfa17b34dcf25ab771a0021c07d7d7cdcea7bce5d0`;
  compiled metallib SHA-256:
  `656aa2ec5a7c259f6a86198a12f75fd7ce50914c5ff2865ed7a31208cdb6e1bc`.
- Deep strict signing passed; `tools/metal/verify_app.sh` verified all 349
  Mach-O slices as ARM64, and the main executable reports arm64.
- `tools/metal/doctor.sh` passed on Apple M1 Max with Apple7-family Metal and
  unified memory. Harness validation and tooling tests pass after this
  documentation checkpoint.
- The main executable still links both Metal and OpenGL during migration.
  Strict no-GL remains an open release gate; no native-only claim is made.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Broad native world/UI/resource owners and physical regressions exist; complete parity remains open. |
| M-007 world/material | doing | Bounded active transparency/material shader audit found no valid-production divergence; broader live alpha/material parity remains open. |
| M-008 advanced views/effects | doing | Highlight Transparent is native. Active pathfinding and object-update blips now reject exact publication instead of disappearing; native pathfinding remains critical. Broader shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. Pathfinding's GL renderer is now an explicit removal boundary. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Native publication can no longer omit
active pathfinding visualization or transient object-update blips. The exact
frame remains on GL until those owners are represented natively; ordinary
frames retain Metal eligibility. Highlight Transparent remains natively
covered for static/rigged world and HUD geometry.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output
owners; controlled live transparency/material and editing comparisons; full
feature/stability matrices; viewer-owned OpenGL removal; and paired quality-
equivalent M1 Max measurements. Independent build/review evidence and
reproducible distinct release proof also remain open. Exact RLVa behavior/image
parity is separately deferred as non-critical M-013.

**Evidence-based priority:** replace the now-explicit pathfinding GL boundary.
Inventory `LLPathingLib`'s navmesh, edge, path, bookend, shape, water-plane and
capsule buffers; define a retained Metal batch contract that preserves clear,
depth-only, solid, blend, line/x-ray and polygon-offset ordering. Keep the
eligibility guard until deterministic native controls cover every active mode.

## Exact next actions

1. Trace `LLPathingLib` buffer ownership and render calls for navmesh, edges,
   paths/bookends, shapes, water plane and selected-character capsules.
2. Implement the smallest retained Metal batch/pipeline set that covers the
   first coherent pathfinding mode with asymmetric depth/blend proof; keep all
   uncovered modes fail-closed.
3. Port the remaining pathfinding modes, then the small object-update blip
   owner. Continue strict no-GL work; keep exact RLVa parity in M-013.
