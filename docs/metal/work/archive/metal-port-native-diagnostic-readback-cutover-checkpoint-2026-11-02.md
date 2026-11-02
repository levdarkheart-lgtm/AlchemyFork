# Metal-port execution — native diagnostic readback — 2026-11-02

## Outcome

Commit `2a32759e7c` removes the pipeline's synchronous GL scene-pixel fallback
and scope GL/PBO transaction. Scene-pixel probes and scopes now publish only
from exact native sources; missing or failed native work leaves the diagnostic
sample unavailable rather than importing GL pixels.

The source remains fail-closed and retains the existing coordinate and
post-owner census. No traversal suppression, feature deletion or parity waiver
was introduced.

## Evidence

- Source-exact warnings-as-errors RelWithDebInfo build `26.4.0d68112` passed at
  `2a32759e7c7faa1fd33bad56c622fc66923231c9`.
- Physical M1 Max Metal validation passed 52,190 assertions in 52 cases with
  API/load-action validation enabled.
- Bundle verification passed 349/349 ARM64 Mach-O slices.
- Strict inventory: 2,408 files, 3,269 findings, expected exit 1. The unchanged
  count reflects the still-present shared helper implementation, not retained
  pipeline fallbacks.
- The executable still links OpenGL and Metal; native-only is not claimed.
- No viewer launch, manual driving, snapshot or comparison occurred.

The full product remains incomplete: GL context/traversal/resource ownership,
linked/runtime no-GL proof, complete feature/stability parity, owner-attended
visual review, measured M1 Max performance and independent release review are
still open.

## Resume action

Prove `llviewerwindow.cpp`'s `DebugShowColor` source/timing/coordinate contract
against the native presentation/world-UI producer. If exact, replace the final
production `ALGPUReadback::requestPixels` caller, then repeat source-exact
build, physical Metal and strict-audit validation.

