# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `5f64b5ac2b` (`fix(metal): defer default probe publication`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Source tracing found a bounded
default-probe publication race: native cube radiance could be ready while its
small CPU-visible spherical-harmonics row was still GPU-pending, and the
material-preview/default path previously published only when both happened to
be ready in the same pass.

Commit `5f64b5ac2b` retains one private deferred default cube with its probe
generation and capture sequence. It publishes only after the matching SH row
is readable, and discards the cube on error, generation/layout/device change or
sequence mismatch. A coherent native pair suppresses redundant GL/PBO default
mirroring; the last coherent published environment remains on a later failure.
GL traversal and normal presentation authority remain intact.

Source-exact build 68024 passed exact-path preflight and actual Computer Use.
Semantic login reached `STATE_STARTED` at the intended Danim location; the
world/UI was visibly coherent and Metal validation emitted no observed error.
Semantic Quit logged `LogoutReply`, `Goodbye!` and `status: stopped`; the final
viewer/helper census was zero. The one-time delayed-promotion marker did not
occur naturally, so this is runtime regression evidence, not physical coverage
of the rare scheduling branch.

## Build and test evidence

- Warnings-as-errors RelWithDebInfo build 68023 linked, staged and signed from
  the final source content. Source-exact incremental package 68024 was then
  observed and verified at committed/pushed head `5f64b5ac2b`.
- Executable SHA-256:
  `fa718e362311afa7c74bff8cd641620e7a6b2b644f0465d3fee1fea7fa0e5fd3`.
- Deep strict signing passed; `tools/metal/verify_app.sh` verified 349/349
  ARM64 slices.
- Complete physical Metal validation passed 44,882 assertions across 50 cases
  in RelWithDebInfo with Metal API/GPU validation enabled. The preceding probe
  selection checkpoint also passed the same complete Debug suite and its
  focused 1,329-assertion case.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passed 484/484 in 259.50 seconds after the
  production change.
- Harness validation and all tooling tests pass after this documentation
  checkpoint.
- Homebrew Bison 3.8.2 remains selected explicitly. The ignored build-local
  Python environment supplies Linden Research's `llsd 1.2.4` and PyYAML.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-007 renderer | doing | Broad native world/UI/resource owners and physical regressions exist; complete parity remains open. |
| M-008 advanced views/effects | doing | Default native radiance/SH publication now remains generation/sequence coherent under delayed SH completion; broad probe publication, post, alpha/PBR and advanced-view parity stay open. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |

## Complete-product advancement assessment

**Advances the complete product:** yes. The default native reflection
environment can no longer be lost merely because its matching CPU-visible SH
row finishes after the radiance pass. The repair preserves generation/sequence
coherence and fail-closed authority. This is bounded M-008 progress, not broad
probe parity, native-only completion or a performance claim.

**Deferred critical work:** remaining post/alpha/PBR/probe/advanced-view parity;
a source- and scene-identical scored post pair; full feature/stability matrices;
viewer-owned OpenGL removal; independent build/review evidence and reproducible
distinct release proof; and paired quality-equivalent M1 Max measurements.

**Evidence-based priority:** the source race is repaired, the ordinary runtime
path is regression-checked and the rare delayed branch did not occur naturally.
Do not keep forcing this scheduler boundary without new evidence. Continue the
source-led M-008 graph at the next unverified advanced owner. For any
cross-backend pair, pin exact source provenance and keep camera,
environment/time, warm state, window/snapshot contract and all effective
graphics settings identical; fail closed before metrics if any invariant
differs.

## Exact next actions

1. Trace the next open non-default probe-publication, post or advanced-view
   owner through active GLSL call sites, formats, bindings, state and ordering;
   implement the first demonstrated gap.
2. Add an asymmetric physical control, then rerun focused/complete Metal
   validation and the neutral gate.
3. Capture the next live comparison with pinned source provenance and identical
   camera, environment/time, window/snapshot contract and 416-setting
   manifests; keep strict no-GL, feature parity, stability and performance
   gates open until their release evidence exists.
