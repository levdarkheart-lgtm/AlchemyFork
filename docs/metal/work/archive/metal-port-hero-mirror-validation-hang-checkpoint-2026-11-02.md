# Metal port execution — authored-mirror validation hang — 2026-11-02

## Executed validation

Exact-path preflight passed for reference `26.4.0.67607` and candidate
`26.4.0.68089`. Both isolated sessions used the same authored-mirror location
and temporary rendering controls, including 256-square hero probes. The pinned
reference reached `STATE_STARTED`, saved a coherent 2048x1476 control and quit
cleanly. Its local SHA-256 is
`b5721310ac7a0cb8a80abc0e5192bf70bf246e7f6454495a03944c33f1927a88`.

The candidate ran the scoped development-only native mirror Snapshot
diagnostic under Metal API/shader/load/store validation. It reached
`STATE_STARTED` and published complete native hero radiance from six
mirror-clipped faces at generation 76 and max LOD 7.

## Failure and containment

The subsequent semantic UI observation timed out at 30 seconds, before a
candidate Snapshot existed. A comparison would therefore be invalid and no
image metric is reported. `SIGTERM` entered `status: quitting` but did not
complete. After 60 seconds the watchdog reported
`Pipeline:RenderGeomDeferred`; after another bounded wait the remaining process
was force-killed. All viewer/media helpers are now absent.

The failure is contained to this validation route. It does not yet prove an
ordinary production renderer hang, because the run combined the semantic UI
bridge, native hero capture and full Metal validation. It also does not clear
the presentation gate. The older accepted 512-square one-fixture result remains
bounded historical evidence; the normal authored-mirror guard stays
fail-closed.

## Next executable action

Reproduce candidate-only at the same 256-square fixture. Use the minimum
Snapshot action rather than a compound full-tree query. On the first timeout,
sample the main thread before sending a termination signal and retain the
symbolized stack. Classify the semantic-UI, GL traversal, native hero and Metal
validation boundaries before implementing a correction or attempting another
reference/candidate pair.

After that bounded stability result, resume strict GL finding classification.
Exact RLVa parity remains deferred M-013, and all shadow comparisons retain the
Preferences/save/restart prerequisite.
