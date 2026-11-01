# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `79356bfd4f` (`fix(metal): admit RLVa overlay UI capture`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. The bounded production-call
reflection-probe audit is closed without a speculative edit: no reachable
viewer-produced divergence was found, and Metal's additional bounds checks do
not alter valid viewer-produced data.

The owner designated **all exact RLVa parity** as a non-release nice-to-have.
Only enough native implementation is required so RLVa cannot block Metal
authority or viewer-owned OpenGL removal. This exception does not relax core
viewer, native-only, stability or performance gates. The persistent M-013
inventory is [the deferred RLVa backlog](../work/deferred/rlva-parity.md). It
also records the mandatory comparison prerequisite: enable RLV in Preferences
in both viewers, save, and restart both viewers before testing.

Commit `9c141e0887` removes the concrete `@setsphere` native-publication
blocker. The Metal post owner consumes the same per-frame policy/tween sample
as GLSL, retained depth and inverse projection, and handles blend, fixed and
bounded-variable blur, chromatic aberration and pixelation in visual-effect
priority order. Missing, malformed or failed state retains the complete GL
chain for that frame. A physical asymmetric depth/reconstruction control and
all five modes pass; exact live RLVa image/behaviour parity remains deferred.

Commit `79356bfd4f` removes the second explicit RLVa cutover rejection.
`@setoverlay` already renders as a textured `gUIProgram` batch inside the
ordered world/UI stream. Native capture now mirrors its texture, AnisoWrap
sampler, streamed tint/alpha and blend instead of rejecting the transaction
before encoding. A missing texture mirror or unsupported batch still discards
the complete native transaction and leaves GL authoritative for that frame.
Source audit found the other renderer-affecting RLVa families enter shared
state before capture: forced textures mutate the `LLFace`/`LLDrawInfo` texture,
avatar-distance silhouettes select the shared impostor/color route, and camera
restrictions update the camera consumed by both paths. No other explicit RLVa
native-eligibility rejection remains.

## Build and test evidence

- Source-exact warnings-as-errors RelWithDebInfo Development build 68031
  linked, staged and signed at committed code head `79356bfd4f`; an immediate
  repeat reported no work.
- Executable SHA-256:
  `3f481298d69ecd8892d14271af072568996065ab10bd44c7571bf5eacae37efd`.
- Packaged MSL SHA-256:
  `06454d17c307c6e1b9493a15edbf16e0c59bc385707f59bac327f5b4f71f006c`;
  compiled metallib SHA-256:
  `53ac4db074d648096a2a5bb578a30b8b5406579e8bc2c4369e0c1470948e1013`.
- Deep strict signing passed; `tools/metal/verify_app.sh` verified 349/349
  ARM64 slices.
- The focused RLVa sphere physical case passes 44 assertions; the focused case
  plus offline builtin contract passes 255 assertions across two cases.
  The ordered UI composition case covering the `@setoverlay` sampler,
  tint/alpha and blend contract passes 455 assertions in both RelWithDebInfo
  and Debug.
  Complete RelWithDebInfo and Debug physical Metal suites each pass 44,947
  assertions across 51 cases with Metal API/GPU validation enabled.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passed 484/484 in 247.94 seconds after the
  production change.
- Harness validation and all tooling tests pass after this documentation
  checkpoint.
- Homebrew Bison 3.8.2 remains selected explicitly. The ignored build-local
  Python environment supplies Linden Research's `llsd 1.2.4` and PyYAML.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-007 renderer | doing | Broad native world/UI/resource owners and physical regressions exist; complete parity remains open. |
| M-008 advanced views/effects | doing | RLVa `@setsphere` and `@setoverlay` no longer disqualify native presentation; exact RLVa parity is M-013 nice-to-have. Broader non-RLVa shadows, transparency/materials, post and output parity stay open. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog separates minimum Metal/no-GL cutover work from exact behaviour/image parity and records the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. The two explicit RLVa rendering
boundaries no longer automatically reject an otherwise exact native frame.
Sphere has physical five-mode/depth coverage; overlay uses the physically
covered ordered UI texture/tint/alpha/sampler owner. This is minimum cutover
support, not a claim of strict RLVa parity, native-only completion or a
performance improvement.

**Deferred critical work:** remaining non-RLVa post/alpha/PBR/probe/advanced
view parity; full feature/stability matrices; viewer-owned OpenGL removal;
independent build/review evidence and reproducible distinct release proof; and
paired quality-equivalent M1 Max measurements. Strict no-GL remains open.
Exact RLVa behaviour/image parity is separately deferred as non-critical M-013.

**Evidence-based priority:** the bounded RLVa minimum-cutover audit is closed;
do not spend release-gate time on its exact-parity backlog. Return directly to
the non-RLVa advanced/native-authority graph, beginning with the isolated
projector/cascade-shadow fixture and then transparency/material replay. The
informational GL scan currently covers 2,408 viewer-owned files and reports
3,303 potential findings; macros, comments and duplicates mean this is not an
equal-sized task count.

## Exact next actions

1. Resume source-led non-RLVa work at the isolated projector/cascade-shadow
   fixture; implement only a demonstrated divergence and add an asymmetric
   physical control.
2. Continue transparency/material replay and native-authority removal. Keep
   full feature/stability, strict no-GL and qualifying performance gates open
   until their release evidence exists; keep RLVa exact work in M-013.
