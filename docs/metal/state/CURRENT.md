# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `d2a863186a` (`fix(metal): refresh projector shadows per view`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `d2a863186a` fixes a reachable
same-frame projector-shadow divergence in native hero/reflection cube capture.
`fillMetalWorldProjectorShadows()` cached the raw spot-depth slice and its
derived constants by `gFrameCount`. A cube update renders multiple camera faces
in one viewer frame using the same probe capture owner. The raw depth map is
intentionally retained for those faces, but `generateSunShadow()` recomputes
`mSunShadowMatrix[4..5]` from each face's current eye-space transform. The old
cache hit therefore left face 2 and later with face 1's projector matrices.

The cache hit now republishes the retained depth texture through the normal
publication path. This preserves the intended depth reuse while refreshing the
current pass matrices, clip planes, resolution, bias/filter, reverse-Z and
unbuilt state. An asymmetric physical control renders one retained depth slice
through two consecutive projector matrices: the first selects the shadowed
half and the second selects the lit half. The distinct expected outputs prove
the second view does not inherit the first view's constants.

The bounded production-call reflection-probe audit remains closed without a
speculative edit: no other reachable viewer-produced divergence was found, and
Metal's additional bounds checks do not alter valid viewer-produced data.

Any candidate/reference shadow comparison is admissible only when shadows are
enabled in Graphics Preferences in **both** viewers before capture. A both-off
control can isolate a shadow residual, but it cannot prove shadow parity.

The owner designated **all exact RLVa parity** as a non-release nice-to-have.
Only enough native implementation is required so RLVa cannot block Metal
authority or viewer-owned OpenGL removal. This exception does not relax core
viewer, native-only, stability or performance gates. The persistent M-013
inventory is [the deferred RLVa backlog](../work/deferred/rlva-parity.md). It
records the mandatory comparison prerequisite: enable RLV in Preferences in
both viewers, save, and restart both viewers before testing. The minimum
`@setsphere` and `@setoverlay` cutover owners remain integrated; do not reopen
their exact-parity work without a concrete Metal/no-GL blocker.

## Build and test evidence

- Warnings-as-errors Release Development build `26.4.0d68033` linked, staged
  and signed at committed code head `d2a863186a`. An immediate repeat emitted
  no `CompileC` work; the always-run packaging phase relinked, staged and
  re-signed successfully.
- Executable SHA-256:
  `a2bb8510781871bb7598b992ea4b1814424f33f4efdf864ae561270d9c501785`.
- Packaged MSL SHA-256:
  `06454d17c307c6e1b9493a15edbf16e0c59bc385707f59bac327f5b4f71f006c`;
  compiled metallib SHA-256:
  `53ac4db074d648096a2a5bb578a30b8b5406579e8bc2c4369e0c1470948e1013`.
- Deep strict signing passed; `tools/metal/verify_app.sh` verified all 349
  Mach-O slices as ARM64, and the main executable reports arm64.
- The focused two-view projector physical case passes 1,918 assertions in both
  RelWithDebInfo and Debug with Metal API/GPU/load-store/shader validation.
  Complete RelWithDebInfo and Debug physical Metal suites each pass 44,963
  assertions across 51 cases under the same validation layers.
- Warnings-as-errors `pipeline.cpp` and `almetal_gpu_test.mm` object builds pass
  in both RelWithDebInfo and Debug.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passed 484/484 in 242.94 seconds.
- Harness validation and all tooling tests pass after this documentation
  checkpoint.
- Homebrew Bison 3.8.2 remains selected explicitly. The ignored build-local
  Python environment supplies Linden Research's `llsd 1.2.4` and PyYAML.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-007 renderer | doing | Broad native world/UI/resource owners and physical regressions exist; complete parity remains open. |
| M-008 advanced views/effects | doing | Retained projector depth now republishes per-view constants across same-frame cube faces. RLVa minimum cutover is integrated and exact RLVa parity is M-013. Transparency/material replay, broader shadows/post and output parity stay open. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog separates minimum Metal/no-GL cutover work from exact behaviour/image parity and records the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. A valid native hero/reflection cube
update can no longer reuse stale first-face projector matrices on later faces.
The change retains raw depth reuse and has asymmetric physical two-view proof;
it is a concrete non-RLVa M-008 correctness repair, not a broad visual replay or
a claim that the shadows row is verified.

**Deferred critical work:** transparency/material replay and remaining
non-RLVa advanced-view/post/output parity; full feature/stability matrices;
viewer-owned OpenGL removal; independent build/review evidence and
reproducible distinct release proof; and paired quality-equivalent M1 Max
measurements. Strict no-GL remains open. Exact RLVa behaviour/image parity is
separately deferred as non-critical M-013.

**Evidence-based priority:** move directly to source-led
transparency/material replay. Trace active GLSL and Metal production calls,
implement only a demonstrated reachable divergence, add an asymmetric physical
control, and retain whole-frame fail-closed GL authority until each owner has
correctness and side-effect proof. The informational GL scan currently covers
2,408 viewer-owned files and reports 3,303 potential findings; macros, comments
and duplicates mean this is not an equal-sized task count.

## Exact next actions

1. Audit the active transparency/material replay owners and implement the first
   demonstrated non-RLVa divergence with an asymmetric physical control.
2. Continue owner-by-owner native-authority removal. Keep full
   feature/stability, strict no-GL and qualifying performance gates open until
   their release evidence exists; keep RLVa exact work in M-013.
