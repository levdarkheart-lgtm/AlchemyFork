# Current authored-mirror validation hang checkpoint — 2026-11-02

This archive records an invalid current-build mirror comparison attempt and a
new bounded M-009 stability boundary. It does not supersede the older accepted
one-fixture 512-square pair, and it does not justify changing mirror authority.

## Exact setup and successful preconditions

Exact-path preflight passed for pinned OpenGL reference `26.4.0.67607`
(`fac5225435df2c096d10b856f6e0f186b37e41446576398ee8f4842b884ef28d`)
and candidate `26.4.0.68089`
(`f5fa05484b57fd77f108838e6d5b82ffde0e8a41697116db742b39b9e9b083f7`).
Both used isolated profiles, the same authored-mirror location and repeated
temporary controls for mirrors, reflections, probe level/detail, fixed
exposure, shadows and 256-square hero probes. Candidate used the scoped
development-only `ALMetalHeroMirrorSnapshotValidation=true` diagnostic; normal
live authority remained guarded.

The reference reached `STATE_STARTED` and saved a coherent 2048x1476 control at
`/private/tmp/alchemy-m009-mirror-reference-shared-256.png`, SHA-256
`b5721310ac7a0cb8a80abc0e5192bf70bf246e7f6454495a03944c33f1927a88`.
It exited cleanly with `Goodbye!` and `status: stopped`.

The candidate reached `STATE_STARTED` and logged:

```text
Native hero-probe radiance published from six complete mirror-clipped Metal faces; generation 76, max LOD 7
```

Metal API, shader and attachment-action validation were enabled. This proves
current native hero-radiance execution; it is not visible full-route parity.

## Invalidating failure

After that publication, the semantic UI observation used to open Snapshot did
not return within its 30-second boundary. No candidate snapshot was produced.
The diagnostic was therefore invalid before any comparison or score.

`SIGTERM` set `status: quitting` but the process did not complete shutdown. At
60 seconds the viewer watchdog reported the main loop expired in
`Pipeline:RenderGeomDeferred` and entered its crash path. After an additional
bounded wait, the still-running process was force-killed. The local isolated
profile contains the watchdog marker and crash diagnostic. No viewer or helper
process remained afterward.

This observation does not yet distinguish a semantic-UI bridge stall, Metal
validation interaction, render-loop stall or shutdown-only defect. It is a
real M-009 stability failure in this validation route, not evidence of ordinary
production behavior. The earlier 512-square attempts also timed out semantic
UI after hero warm-up but exited cleanly on `SIGTERM`; they produced no images
and are not counted as parity evidence.

## Decision and next diagnostic

Keep the active authored-mirror presentation guard fail-closed. Do not remove
it from the older one-fixture pair alone.

Run a candidate-only reproduction with the same 256-square authored mirror and
Metal validation. Avoid a compound full-tree semantic query after warm-up;
request the minimum Snapshot action. If the first action times out, sample the
main thread before any termination signal, preserve the stack plus log, and
classify whether the stall is in the viewer accessibility bridge, native hero
capture, GL traversal or a Metal validation wait. Only then implement a bounded
fix or repeat a new admissible reference/candidate pair.

Exact RLVa parity remains deferred non-release nice-to-have M-013. Shadow pairs
remain subject to the Graphics Preferences, save and both-viewer restart rule.
