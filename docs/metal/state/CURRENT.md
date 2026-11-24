# Current engineering state — optional MetalFX spatial prototype

Observed 2026-11-24 on Apple M1 Max /64 GiB /macOS26.5.2 /Xcode26.6.
Implementation branch `codex/metalfx-spatial-20261124` starts from usual-branch
`7911e382ce17f8957cd335cabbd30f808bef6ac1`. Separate clean control `f384e2c220`
and implementation base `3df7055a9d` carry the same small, gated quiet-output
observations. The earlier review worktree and private evidence remain separate.

PPD591 owns the optional fixed-half scene modes: linear presentation and classic
MetalFX spatial, retaining output-sized shadow references and native UI. Native
remains the default; global resolution/shadow controls are preserved. Combined
lighting remains OFF. This is a performance/quality trade-off, not exact native
image equivalence. No commit/push approval is inferred from builds or measurements.

## Observed implementation and verification

Explicit scene/output/shadow-reference planning and CPU coverage pass1942 assertions
in28cases. Native MetalFX helper and integration are under source/GPU qualification;
no new live performance or image-quality result is claimed yet. Source review found
and is correcting final-target acquisition ordering after opaque MetalFX encoding
and staged reload coverage of the new lazy PSOs. Both are within PPD591.

The clean native control Release build has completed. Candidate compilation, app
verification, GPU correctness, Home net-throughput and still/motion quality checks
remain required. Exact commands and private receipts are under
`artifacts/metal/metalfx-spatial-20261124`; the compact
[implementation record](../research/metalfx-spatial-20261124.md) owns this scope.
OpenHTJ2K0.27 remains the sole texture pixel decoder; verify runtime identity before
collection. The viewer is currently closed.

The accepted earlier Review69454 resolution split justifies this feature attempt;
it is not a measurement of the new executable. Probe/SSR/combined-pass experiments
are closed and are not this task. Do not change admission or reopen those variants.

## Complete-product advancement assessment

The [production maintenance record](../research/production-maintenance-20261122.md)
retains previously accepted repairs, register reconciliation and their validation.
The earlier review branch preserves research-only evidence separately. Those facts
are historical, not current claims that this new feature is already qualified.

**Renderer code checkpoint:** `3df7055a9d` (clean implementation base; feature changes pending).

| ID | State | Remaining boundary |
| --- | --- | --- |
| M-008 | doing | Broad feature/visual acceptance and representative performance. |
| M-011 | doing | Reproducible release and strict no-GL qualification. |
| M-012 | complete | Maintained build/verification harness. |
| M-013 | doing | Retained features and prolonged stability. |

Required independent build/review evidence and release gates remain open.
PPD319 Home performance and broad visual/feature/release qualification remain open.
A reviewable prototype does not establish complete-product or release readiness.

## Exact next actions

Clean-pin and build the candidate, run focused native correctness coverage,
then compare Native/Half-linear/MetalFX and inspect still/moving images, resize,
native return and existing special-view behavior. Restore settings after testing.
