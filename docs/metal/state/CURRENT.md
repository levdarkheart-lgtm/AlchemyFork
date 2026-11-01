# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `d3cff63d0d` (`fix(metal): reset exposure after renderer warmup`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. A cold-profile authored-mirror run
exposed the remaining temporal startup defect: GL could adapt exposure while
the asynchronous main-world Metal renderer was still warming. Metal then had
no exact prior EV, so the native Snapshot gate correctly remained fail-closed.

Commit `d3cff63d0d` restarts both histories from EV 0 when main-world warm-up
finishes. Native state is reset immediately; the GL exposure target is cleared
only at `generateExposure()`'s established safe ping-pong boundary. Missing or
divergent history still fails closed, the development-only authored-mirror
Snapshot waiver remains scoped, and GL stays authoritative.

Actual Computer Use on build 68020 then completed the same isolated candidate
and pinned OpenGL-reference flow. Both used the same SLURL, persisted camera,
fixed Midday, 2048x1476 window/snapshot contract and matched 416/416 effective
visual settings before and after the runs. Candidate logs record the EV-reset
boundary, scoped authored-mirror admission and native 2048x1476 readback with
no GL-readback fallback. Both semantic exits completed and no viewer/media
process remains.

The exact binaries differ by design and have pinned provenance; they are not
mislabelled as one source. Whole-frame image scoring is descriptive because the
avatar pose changed sequentially. The stable mirror interior reached SSIM
`0.985163` / PSNR `34.480 dB`, with no material residual on direct inspection.
This accepts one fixture only, not broad probe/mirror parity.

The next bounded M-008 audit found no new divergence in the active alpha/PBR
capture and replay seam, so no production behavior was changed speculatively.
It did identify missing physical coverage for the live reflection-probe walk.
A new asymmetric GPU regression now proves that a containing manual box reached
only through a selected probe's neighbor list suppresses already accumulated
automatic/default taps, while removing that edge restores the automatic/default
family. This closes one probe-selection/transition contract only.

## Build and test evidence

- Warnings-as-errors RelWithDebInfo build 68020 linked, staged and signed.
- Executable SHA-256:
  `42717a577f42cfd5e553d8e09e4ece86dc88ad4e283aa49eedfb24d4cbe71763`.
- Deep strict signing passed; `tools/metal/verify_app.sh` verified 349/349
  ARM64 slices.
- Complete physical Metal validation passed 44,882 assertions across 50 cases
  in both Debug and RelWithDebInfo with Metal API/GPU validation enabled. The
  focused production-world case passed 1,329 assertions.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passed 484/484 in 239.08 seconds after the
  probe-neighbor regression was added.
- Harness validation and all tooling tests pass after this documentation
  checkpoint.
- Homebrew Bison 3.8.2 remains selected explicitly. The ignored build-local
  Python environment supplies Linden Research's `llsd 1.2.4` and PyYAML.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-007 renderer | doing | Broad native world/UI/resource owners and physical regressions exist; complete parity remains open. |
| M-008 advanced views/effects | doing | Reference stills, synchronized exposure/flare history and one neighbor-selected manual-box probe transition have physical evidence; remaining post, alpha/PBR, broader probes and advanced views stay open. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Production temporal exposure survives
cold asynchronous renderer warm-up, an authored-mirror native Snapshot
completed under a matched cross-backend contract, and the native probe selector
now has an asymmetric physical control for a previously untested neighbor/box
transition. This is bounded M-008 progress, not broad feature parity,
native-only completion or a performance claim.

**Deferred critical work:** remaining post/alpha/PBR/probe/advanced-view parity;
a source- and scene-identical scored post pair; full feature/stability matrices;
viewer-owned OpenGL removal; independent build/review evidence and reproducible
distinct release proof; and paired quality-equivalent M1 Max measurements.

**Evidence-based priority:** do not revisit the repaired temporal or proven
probe-neighbor boundaries without new evidence. Continue the source-led M-008
graph at the next unverified advanced owner; use a live probe transition only
when a controlled fixture visibly attributes it. For any cross-backend pair,
pin exact source provenance and keep camera, environment/time, warm state,
window/snapshot contract and all effective graphics settings identical; fail
closed before metrics if any comparison invariant differs.

## Exact next actions

1. Trace the next open probe-publication, post or advanced-view owner through
   active GLSL call sites, formats, bindings, state and ordering; implement the
   first demonstrated gap.
2. Add an asymmetric physical control, then rerun focused/complete Metal
   validation and the neutral gate.
3. Capture the next live comparison with pinned source provenance and identical
   camera, environment/time, window/snapshot contract and 416-setting
   manifests; keep strict no-GL, feature parity, stability and performance
   gates open until their release evidence exists.
