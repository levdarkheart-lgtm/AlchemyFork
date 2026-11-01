# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `f9cc59d963` (`fix(metal): preserve main-world post history`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. A controlled nondefault post run
(SMAA quality 3 with predication, dynamic exposure and lens flare) exposed a
real lifecycle gap: main-world Metal pipelines warm after GL temporal history
has begun, then an independent probe owner could clear the main-world readiness.
Auxiliary finalize passes also advanced a global flare history for outputs whose
color-correction path never consumes the effect.

Commit `f9cc59d963` synchronizes main-world GL/Metal flare history at the
known-black boundary after pipeline warm-up, keeps probe-owner initialization
from invalidating main exposure/flare history, and holds flare history outside
the main-world pass. Every missing or divergent history still fails closed and
GL remains authoritative while broader parity is incomplete.

Actual Computer Use on build 68018 authenticated in-world, waited for
main/probe warm-up, opened Snapshot, selected Disk, refreshed and saved a native
2048x1476 PNG with post enabled, UI/HUD disabled, colors/PNG/NoFilter and the
controlled SMAA settings. The accepted log records native color readback at
2048x1476 with no GL fallback marker. Confirmed Quit ended with `Goodbye!`,
`status: stopped` and an empty viewer/media process census.

Preflight and post-run manifests both prove 416/416 identical effective visual
settings. The older reference/candidate SSIM/PSNR are not parity evidence:
source revisions, region lighting and avatar pose changed between captures.
Settings equality is necessary but does not substitute for identical
scene/environment/time and source provenance.

## Build and test evidence

- Warnings-as-errors RelWithDebInfo build 68018 linked, staged and signed.
- Executable SHA-256:
  `0df89b25c134eea1199b61c7255121971aae98cf37ec595edf16c855aa2098f7`.
- Deep strict signing passed; `tools/metal/verify_app.sh` verified 349/349
  ARM64 slices.
- Complete physical Metal validation passed 44,871 assertions across 50 cases
  with Metal API/load-action validation enabled.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passed 484/484 in 245.95 seconds.
- Harness validation covers 31 live files; all 24 tooling tests pass after this
  documentation checkpoint.
- Homebrew Bison 3.8.2 remains selected explicitly. The ignored build-local
  Python environment supplies Linden Research's `llsd 1.2.4` and PyYAML.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-007 renderer | doing | Broad native world/UI/resource owners and physical regressions exist; complete parity remains open. |
| M-008 advanced views/effects | doing | Reference stills and temporal post history have native owners with physical/in-world evidence; remaining post, alpha/PBR, probes and advanced views stay open. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |

## Complete-product advancement assessment

**Advances the complete product:** yes. A production temporal-post owner now
survives asynchronous renderer and independent-probe lifecycle boundaries, and
an actual nondefault post Snapshot completed through native readback. This is
bounded M-008 progress, not broad feature parity, native-only completion or a
performance claim.

**Deferred critical work:** remaining post/alpha/PBR/probe/advanced-view parity;
a source- and scene-identical scored post pair; full feature/stability matrices;
viewer-owned OpenGL removal; independent build/review evidence and reproducible
distinct release proof; and paired quality-equivalent M1 Max measurements.

**Evidence-based priority:** do not revisit the repaired temporal boundary
without new evidence. Continue the source-led M-008 graph at the next
unverified post or alpha/PBR owner. For the next scored image pair, keep source,
camera, environment/time, warm state and all effective graphics settings
identical; fail closed before metrics if any invariant differs.

## Exact next actions

1. Trace the next open post or alpha/PBR owner through active GLSL call sites,
   formats, bindings, state and ordering; implement the first demonstrated gap.
2. Add an asymmetric physical control, then rerun focused/complete Metal
   validation and the neutral gate.
3. Capture the next live comparison under identical source, camera,
   environment/time and 416-setting manifests; keep strict no-GL, feature
   parity, stability and performance gates open until their release evidence
   exists.
