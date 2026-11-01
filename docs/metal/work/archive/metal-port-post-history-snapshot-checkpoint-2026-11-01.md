# Native post-history Snapshot execution record — 2026-11-01

**Renderer checkpoint:** `f9cc59d963`

The next source-led M-008 check exercised an actual post-processed Snapshot
with SMAA quality 3/predication, temporal exposure and lens flare. The first
admissible profiles matched 416/416 effective visual settings, but the native
gate still failed closed at the lens-flare history bit.

Runtime census narrowed the failure to lifecycle ownership. Main-world Metal
pipeline construction begins asynchronously after startup, so GL could leave
the exact-black bootstrap before Metal had a prior state. Once caught up, the
histories remained aligned until the first independent probe owner initialized;
that probe reset the shared main-world readiness, and auxiliary finalize passes
advanced flare history for outputs that never consume the effect.

The implementation now:

- synchronizes GL and Metal flare history to the same black boundary when the
  main-world renderer finishes warming;
- keeps probe-owner resource initialization from clearing main-world exposure
  and flare readiness; and
- holds the global flare history during non-main render passes.

All eligibility remains fail-closed. Physical validation passed 44,871
assertions/50 cases, the neutral gate passed 484/484 in 245.95 seconds, build
68018 passed deep signing and 349/349 ARM64 inspection, and the executable hash
is `0df89b25c134eea1199b61c7255121971aae98cf37ec595edf16c855aa2098f7`.

Actual Computer Use then logged in, waited through main/probe warm-up, opened
Snapshot and saved a native 2048x1476 PNG with post enabled, UI/HUD disabled,
colors/PNG/NoFilter and the matched SMAA configuration. Native readback was
logged at the requested size, no fallback marker appeared, logout was clean and
no process remained. Post-run settings still matched 416/416.

The older reference/candidate image metrics are retained only as descriptive
diagnostics because region lighting and avatar pose changed and the executables
were not source-identical. A future scored post pair must keep settings,
source, camera, environment/time and warm state invariant. M-008 remains active;
the next implementation target is the first demonstrated gap in the remaining
post or alpha/PBR graph, not a broad parity or performance claim.
