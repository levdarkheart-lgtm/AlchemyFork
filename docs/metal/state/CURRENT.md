# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `fd0fa7cea5` (`fix(metal): keep water default probe unshifted`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Source tracing found a bounded water
reflection mismatch. GLSL deliberately samples default probe zero along the
reflected water ray without parallax, while the native Metal water path used
the generic sphere-intersection correction. Moving the camera-relative default
probe centre could therefore steer water into a different cubemap face.

Commit `fd0fa7cea5` passes water context into the shared native radiance helper
and uses the unshifted reflected ray only for water/default-probe zero. Other
water probes, boxes and all non-water callers retain their existing parallax
rules. A six-distinct-face physical regression moves only the default centre
from +4000 to -4000 X: before the fix it failed with aggregate RGB delta
1.596944809; afterward it passes below 0.002. GL traversal and normal
presentation authority remain intact.

Source-exact build 68026 passed exact-path preflight and actual Computer Use.
Semantic login reached `STATE_STARTED` at the intended Danim location; the
world/UI was visibly coherent and no Metal validation error was observed.
Semantic Quit logged `LogoutReply`, `Goodbye!` and `status: stopped`; the final
viewer/helper census was zero and launch validation variables were cleared.

The subsequent bounded live-water pair is admissible and accepted. Actual
Computer Use flew and Map-teleported candidate build 68026 and reference build
67607 to `Grignano (128, 228, 74)`, selected Midday and captured fresh
2048x1476 snapshots. All 416 effective graphics settings matched; camera/focus
X/Y matched within serialization precision and Z differed by about 1 cm. Human
inspection found the views materially the same. Blurred structural whole-frame
SSIM was 0.993616 and the bounded water/sky crop was 0.998397. Raw temporal
SSIM 0.870878 is retained descriptively, not treated as pixel identity. Both
viewers stopped cleanly and the final process census was zero.

## Build and test evidence

- Source-exact warnings-as-errors RelWithDebInfo build 68026 linked, staged and
  signed at committed/pushed head `fd0fa7cea5`.
- Executable SHA-256:
  `d33603803c5d45434d06e0332e14980dc095505b060624f09143a41f434e1d4e`.
- Packaged MSL SHA-256:
  `fb170421c26a1d75e556a46d14e52d660fe989a379bb62fbf80ba2eb5a317a1e`;
  compiled metallib SHA-256:
  `40617ce97a852bb2a92454020d050d7ee146501bff24bf992b0432c006423b33`.
- Deep strict signing passed; `tools/metal/verify_app.sh` verified 349/349
  ARM64 slices.
- The pre-fix focused physical test failed only the new asymmetric assertion at
  1,348/1,349. After the fix it passed 1,349/1,349 with Metal API/GPU
  validation enabled. Complete RelWithDebInfo and Debug physical suites each
  passed 44,902 assertions across 50 cases.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passed 484/484 in 259.22 seconds after the
  production change.
- Harness validation and all tooling tests pass after this documentation
  checkpoint.
- The accepted live-pair image hashes, executable provenance, commands and
  scope are retained in the indexed water-live-pair archive; authenticated
  images, profiles and logs remain local and untracked.
- Homebrew Bison 3.8.2 remains selected explicitly. The ignored build-local
  Python environment supplies Linden Research's `llsd 1.2.4` and PyYAML.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-007 renderer | doing | Broad native world/UI/resource owners and physical regressions exist; complete parity remains open. |
| M-008 advanced views/effects | doing | Water/default-probe zero has physical proof plus one accepted matched live Grignano fixture; broader water, probes/mirrors, shadows, transparency/materials, post and output parity stay open. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |

## Complete-product advancement assessment

**Advances the complete product:** yes. A source-demonstrated GLSL/Metal water
reflection divergence is closed under a physical six-face negative control and
corroborated by one admissible live fixture. This is bounded M-008 progress,
not broad water/reflection/mirror parity, native-only completion or a
performance claim.

**Deferred critical work:** remaining post/alpha/PBR/probe/advanced-view parity;
a source- and scene-identical scored post pair; full feature/stability matrices;
viewer-owned OpenGL removal; independent build/review evidence and reproducible
distinct release proof; and paired quality-equivalent M1 Max measurements.
The strict no-GL gate remains open.

**Evidence-based priority:** the bounded water pair is accepted; do not repeat
it. Return to source-led tracing and close the first demonstrated probe/mirror
or projector/cascade-shadow divergence with an asymmetric physical control,
then continue transparency/material replay, non-default post and advanced
output. The informational GL scan currently covers 2,408 viewer-owned files
and reports 3,303 potential findings; macros, comments and duplicates mean
this is not an equal-sized task count, and the strict release gate remains
open.

## Exact next actions

1. Continue the source-led M-008 graph through probes/mirrors, the isolated
   projector/cascade shadow fixture, transparency/material replay, non-default
   post and advanced output; add an asymmetric physical control for each
   demonstrated divergence.
2. Keep live-water breadth, moving mirrors, full feature/stability, strict
   no-GL and qualifying performance gates open until their release evidence
   exists.
