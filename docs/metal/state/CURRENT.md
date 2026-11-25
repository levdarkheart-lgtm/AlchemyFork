# Current engineering state — Temporal implementation in progress

Observed 2026-11-25 on Apple M1 Max /64 GiB /macOS26.5.2 /Xcode26.6.
Implementation branch `codex/metalfx-spatial-20261124` starts from usual-branch
`7911e382ce17f8957cd335cabbd30f808bef6ac1`. Separate clean control `f384e2c220`
and implementation base `3df7055a9d` carry the same small, gated quiet-output
observations. The earlier review worktree and private evidence remain separate.

PPD591 owns the optional fixed-half scene modes: linear presentation and classic
MetalFX spatial, retaining output-sized shadow references and native UI. Native
remains the default; global resolution/shadow controls are preserved. Combined
lighting remains OFF. This is a performance/quality trade-off, not exact native
image equivalence. No owner acceptance or push approval is inferred from builds or measurements.

## Active Temporal work — PPD598

Separate `worktrees/metalfx-temporal`/`codex/metalfx-temporal-20261125`, fetched parent41e90aaadb unchanged.
Owner authorizes implementation, offline fixtures, build/private packaging, review push and login if needed.
No usual-branch change. [Integration map](../research/metalfx-temporal-20261125.md) owns current contract.
Native remains default; Spatial preserved. PPD597/599/600 are focused prerequisite repairs.
Root owns pipeline/CMake/docs; agents own scaler, motion/shaders and settings independently.
No Temporal build, fixture or quality result yet; next action is production implementation.

## Active quality expansion — PPD596

The owner requests exact 2/3 and 3/4 Spatial choices and scale-aware material mip sampling, starting at
091243d236 (code 12fb05f, build 69462). The preset, fallback and native snapshot repairs are preserved.
Stable choices 3/4 map centrally to rational dimensions with upward rounding, native UI and output-based
shadow budgets. Material bias is frozen before main draws from actual rounded widths; late linear fallback
retains and reports that policy. Native, saved linear 50%, shadows, probes, impostors, snapshots and debug
views remain unbiased. Shader sample bias is used because the validated M1 Max colored-mip check confirms
that sampler-descriptor bias has no effect on this Apple7 GPU.

Build **69464**, code **a5142ae5c2**, passes the full Release build, deep/strict signing, 346 ARM64 inspections,
three CPU planning/UI/preset suites and fourteen relevant Metal suites with API/GPU Validation. New material,
alpha coverage, terrain gradient, owned-input and zero-bias recovery cases pass after correcting the new
fixture's omitted environment source. The older hidden probe-accumulator suite requires external source
inputs and is excluded; it supplies no new probe experiment or performance result.

Private evidence: `artifacts/metal/metalfx-quality-20261124/`. The [quality review](../research/metalfx-quality-20261124.md)
records bounded 53-probe observed rates: Native15.14, unbiased50 35.68, biased50 35.58, 67%25.41, 75%21.07,
restored unbiased50 35.90 worlds/s. The last row is an explicit20s prefix, not its invalid full25s window.
Warming controls are excluded; only B2 passes all local population gates. Named observer holes and separate
process identities remain. This is useful review evidence, not a lossless release comparison or zero-cost proof.

Five lossless PNGs/crops and motion clips are retained. UI crops are byte-identical. 67% is recommended for
balance;75% is the best scaled still, while50% retains obvious thin-edge artifacts. Modest mip detail gain is
separate from extra source pixels. Motion evidence is qualitative/stepped, with unmatched sampled poses;
continuous shimmer and broader material acceptance remain owner review. Final Native mode/original controls/CAS
are restored, override false, capture utility restored to stills, and PID22729 is closed. Review app/bundle are
on iCloud Desktop. No Temporal implementation. On 2026-11-25 the owner explicitly authorizes
publication to `codex/metalfx-spatial-20261124` for another AI to review. Private evidence stays local;
usual-branch integration, release and product acceptance remain unapproved. Application code remains a5142ae5c2.

PPD597 records an existing hidden/manual probe fixture's incorrect ordinary CTest registration. It was excluded,
not passed; its coherent maintenance disposition is retained separately without reopening probe experiments.

## Observed implementation and verification

Previous integration build69462/source12fb05f compiles and passes14 selected CTest
suites:3CPU and11Metal with API+GPU Validation. Deep/strict signing and346 ARM64
checks pass. PPD593 presets round-trip2→0→2 through real manager/XML I/O; older
presets lacking the key select Standard, failed loads preserve the mode, generated
Default stays Native. PPD594 shows committed effective mode/fallback reason beside
the selector. PPD595 same-size ordinary snapshots explicitly request native scene
detail in50% modes; custom/explicit/cutout/360 paths stay independent. The new
borrowed-main failure case rejects final validation after real MetalFX encoding,
denies commit/publication permission and retains the previous BGRA image.

That69462 integration checkpoint had no new login, live UI/snapshot smoke test or FPS comparison. Owner review remains
pending; precise overhead and zero Native regression are not established. Focused
receipts are under `artifacts/metal/metalfx-integration-20261124/`.
The verified69462 review DMG is separately named on iCloud Desktop; the69457
package is retained unchanged. Neither packaging nor fixtures log in the owner.

Initial candidate69457/source68e59fc and matched native control69456/f384e2c220 build
and app verification pass. Each bundle contains346 ARM64 Mach-O components.
CPU planning/preferences pass2/2; API+Shader Validation GPU scopes pass11/11,
plus11979 assertions across6 display-post cases. Source-review corrections to
target acquisition and staged shader reload are included in the pinned build.

The69457 quiet Home sequence observes15.135/35.570/35.026 successful joined worlds/s
for Native/Half-linear/MetalFX, respectively. Actual main scene sizes are3456×2168
and1728×1084, output/shadow reference3456×2168; the scaler actually executes.
One lost FX lifecycle event prevents full-population qualification. There is no
timed restored control; workload/loss limits and all denominators remain explicit.
These are new-world GPU completions, not scanout FPS or native-image equivalence.

Verified stills show sharper reconstruction than linear, reduced fine scene detail
versus Native and sharp native UI. Matched small camera sweeps are retained as
review videos; temporal quality remains for owner review. Resize/return reuses two
scaler descriptors. Initial69457 same-size Snapshot preview used Half-linear;
the current policy is corrected above. Explicit overrides retain legacy semantics.
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

**Renderer code checkpoint:** `a5142ae5c2c6dd1fe8b2fa89c9479eb213cff0f5` (actual build 69464; later test-fixture/documentation edits do not change its application identity).

| ID | State | Remaining boundary |
| --- | --- | --- |
| M-008 | doing | Broad feature/visual acceptance and representative performance. |
| M-011 | doing | Reproducible release and strict no-GL qualification. |
| M-012 | doing | Maintained scripts verified; PPD597 manual-suite CTest registration remains. |
| M-013 | doing | Retained features and prolonged stability. |

Required independent build/review evidence and release gates remain open.
PPD319 Home performance and broad visual/feature/release qualification remain open.
A reviewable prototype does not establish complete-product or release readiness.

## Exact next actions

Owner reviews69464's new scales, scoped mip policy and bounded quality/performance evidence. Keep Native default
and combined lighting OFF. PPD596 is ready for owner review, not exact native-quality or release acceptance.
PPD591/593–595 broader owner checks, PPD592 media packaging and PPD597 test registration remain recorded.
The next major rendering candidate is a bounded Temporal evaluation after owner direction; no implementation
starts in this task. Preserve existing evidence, do not reopen rejected shader/queue variants, and do not push.
