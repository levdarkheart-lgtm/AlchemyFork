# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `8361addd0f` (`feat(metal): replay transparent highlights natively`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `8361addd0f` closes a
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
  45,008 assertions across 51 cases under the same validation layers.
- Warnings-as-errors RelWithDebInfo objects for the Metal renderer, built-in
  MSL, GPU test, `lldrawpoolalpha.cpp` and `pipeline.cpp` compiled and linked.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passed 484/484 in 246.16 seconds.
- Source-exact warnings-as-errors Release Development build `26.4.0d68036`
  linked, staged and signed at committed code head `8361addd0f`. The committed-
  head repeat reconfigured version provenance and rebuilt only affected
  integration/version work before restaging successfully.
- Executable SHA-256:
  `064266af7bf95d6e5ddfefe2796ffd0015d8316b22d2bd7fa13ace27cdfd9fdf`.
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
| M-008 advanced views/effects | doing | Static/rigged world and HUD Highlight Transparent output now replays natively and fails closed. Broader shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Native publication can no longer omit
the reachable Highlight Transparent overlay for static objects, rigged avatars
or HUD attachments. Physical asymmetric controls prove skinning movement and
RGB-only alpha preservation, while malformed inputs reject native authority.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output
owners; controlled live transparency/material and editing comparisons; full
feature/stability matrices; viewer-owned OpenGL removal; and paired quality-
equivalent M1 Max measurements. Independent build/review evidence and
reproducible distinct release proof also remain open. Exact RLVa behavior/image
parity is separately deferred as non-critical M-013.

**Evidence-based priority:** inspect the remaining draws executed after the
retained world replay and inside ordered UI capture. Implement only the first
reachable non-RLVa output that can be omitted while native presentation stays
eligible, add an asymmetric physical control, then continue owner-by-owner
authority removal. The informational GL scan currently covers 2,408 viewer-
owned files and reports 3,303 potential findings; macros, comments and
duplicates mean this is not an equal-sized task count.

## Exact next actions

1. Trace the post-water/final diagnostic-overlay and ordered-UI production
   calls and identify the next reachable output owner missing from native
   publication; do not edit when source inspection shows valid parity.
2. Implement and physically validate the first demonstrated non-RLVa omission,
   preserving fail-closed whole-frame authority and current GL migration
   output.
3. Continue full feature/stability, strict no-GL and qualifying performance
   work; keep exact RLVa parity in M-013.
