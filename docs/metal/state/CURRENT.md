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

Clean candidate69457/source68e59fc and matched native control69456/f384e2c220 build
and app verification pass. Each bundle contains346 ARM64 Mach-O components.
CPU planning/preferences pass2/2; API+Shader Validation GPU scopes pass11/11,
plus11979 assertions across6 display-post cases. Source-review corrections to
target acquisition and staged shader reload are included in the pinned build.

One quiet Home sequence observes15.135/35.570/35.026 successful joined worlds/s
for Native/Half-linear/MetalFX, respectively. Actual main scene sizes are3456×2168
and1728×1084, output/shadow reference3456×2168; the scaler actually executes.
One lost FX lifecycle event prevents full-population qualification. There is no
timed restored control; workload/loss limits and all denominators remain explicit.
These are new-world GPU completions, not scanout FPS or native-image equivalence.

Verified stills show sharper reconstruction than linear, reduced fine scene detail
versus Native and sharp native UI. Matched small camera sweeps are retained as
review videos; temporal quality remains for owner review. Resize/return reuses two
scaler descriptors. Same-size Snapshot preview reads back through Half-linear,
then resumes MetalFX; explicit size overrides retain legacy resolution semantics.
Mode0, all original66 controls, CAS and the fixed camera were verified restored;
the test viewer shut down normally at16:00:12 UTC. Exact receipts are under
`artifacts/metal/metalfx-spatial-20261124`; the compact
[implementation record](../research/metalfx-spatial-20261124.md) owns this scope.
Actual runtime OpenHTJ2K0.27 was verified as the sole texture pixel decoder.

Owner-requested private DMG69457 is on iCloud Desktop. Mounted bytes/modes/symlinks,
signing and346 ARM64 checks pass; exact68e59fc app retained. Apple Silicon/macOS26+
is required; ad-hoc signing is not notarization. External-machine test is pending.
PPD592 records inherited optional-VLC missing framework references; actual plugin
selection/media impact remain unproved. No speculative package changes were made.

The accepted earlier Review69454 resolution split justifies this feature attempt;
it is not a measurement of the new executable. Probe/SSR/combined-pass experiments
are closed and are not this task. Do not change admission or reopen those variants.

## Complete-product advancement assessment

The [production maintenance record](../research/production-maintenance-20261122.md)
retains previously accepted repairs, register reconciliation and their validation.
The earlier review branch preserves research-only evidence separately. Those facts
are historical, not current claims that this new feature is already qualified.

**Renderer code checkpoint:** `68e59fc7b6a538f092d03f1869e06395ecae16c7` (actual candidate build; later documentation does not change its identity).

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

Owner reviews candidate69457, still/moving image quality and the graphics selector.
Keep Native default and combined lighting OFF. Retain PPD591 pending owner quality
acceptance and broader stability/special-view coverage. Do not pool earlier private
measurements or reopen rejected shader/queue variants. No push is authorized.
