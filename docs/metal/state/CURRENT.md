# Current engineering state — experimental native Aurora prototype

Observed2026-11-26 on AppleM1Max/64GiB/macOS26.5.2/Xcode26.6. Isolated branch
`codex/aurora-native-temporal-20261126` starts at `318f0ac8b6a8073fe8d22d0848c84c282ad8b212`.
Evaluation, Temporal, Spatial and usual branches/builds are preserved. Owner authorizes
implementation, offline validation, feature-branch push and private iCloud packaging;
no live login or whole-viewerFPS claim. Native default and combined lighting OFF remain.

## Active PPD610 prototype / PPD614 port qualification

**Renderer code checkpoint:** `b73abe1acdb65f96239ce70ac4cdf4c0ca83827a`; prior69507productionidentity.
Current uncommitted native implementation is tracked in the
[native report](../research/aurora-native-temporal-20261126.md). SavedID8/effective4 selects
exact2/3 AuroraOFF, using existing sizing, shadowbudget, motion/poses, jitter, main-post
transaction and retirement. Genuine pre-transparencyHDR preservation is encoded before
compositing; native component runs the pinned translatedWGSL graph and resolves glow.
No runtimeRust/wgpu or normalframeCPUwaits. Temporal/Spatial IDs0–7 retain meaning.

PPD610 remainsopen: original67/75andAuroraarea/settling failures are unchanged.
The [evaluation](../research/aurora-reconstruction-evaluation-20261126.md) supports only
an experimental quality review: ~80%lessstationaryvariation, slowdetailredistribution,
~11msOFF versus~4.14msMetalFX at2304x1446→3456x2168; notviewerFPS.
Closed nativeMetalFX isolation staysclosed. No furtheralgorithmselection needed.

PPD614 blocks packaging until native port response is correct: firstworldexact but
historyworlds divergeMAE.0021012/max.293335. MatchingMSL4toevaluatedbackend doesnot
changefailure. Component/validation agents are isolating firstdivergentintermediate;
failed468worldoutputs retained privately under `artifacts/metal/aurora-native-20261126/`.
FirstAPI/GPU lifecycle checks pass; they donotqualifyportquality. FullReleasebuild is
underway in `build-aurora-native-release`, OpenHTJ2K0.27 solepixeldecoder.
198repositorytooltests pass(1existingskip); native/preset/source tests remain inprogress.

Next: resolvePPD614, complete main+mature-reference parity/frozenquality scoring,
focused main integration/NativeSpatial/transaction regressions, targetcomponentcost,
then commit source, rebuildpinnedapp, login-onlysmoke and privateiCloudDMG/ZIP/checksums.
Never call the owner-scene foliage fixed from fixtures. Owner performs firstlive review.

## Preserved69494 Temporal checkpoint — PPD598

Preserved69494 source: `663a591f2c543158d4e0bf5a68e4e7236f20a559`.

Separate `worktrees/metalfx-temporal` / `codex/metalfx-temporal-20261125`, fetched parent
`41e90aaadb938d77955f7c7e752362aab875fff6` unchanged. App **26.4.0.69494**, production source
**663a591f2c543158d4e0bf5a68e4e7236f20a559**;841358a changes only new test-oracle code.
Owner authorizes feature-branch publication and private iCloud package; usual/Spatial branches preserved.
[Temporal review](../research/metalfx-temporal-20261125.md) is the authoritative implementation/evidence map.

Native remains default; Temporal50/67/75, HDR reconstruction after reduced-depth DoF and before bloom/tonemap,
output shadows/native UI, camera/rigid/body/attachment/cloth motion, finite reactive policies and submit-owned
history are implemented. Early fallback stays unjittered; late failure cancels world publication. Saved AA/CAS
remain available for other modes. Snapshots/captures do not advance main history. Live quality/FPS is unclaimed.

Full Release build,346 ARM64 inspections, deep/strict signing and readonly-DMG hash verification pass.
API/GPU validation passes18 scaler/post/material cases and10 motion cases, plus15 existing regression suites.
CPU history13/resource-plan34 cases,398 UI and116 real preset checks pass. Login smoke reaches fullscreen
with AutoLoginfalse, then quits;17 original settings files restored byte-exact. No live login performed.
PPD597/599/600 inherited repairs and PPD601–607 corrections retain failed evidence and verification limits.
Fractional mip tests use a calibrated native explicit-LOD oracle; no production formula/tolerance relaxation.

Known review limits: reactive-only transparent/sky/water regions, terrain-edit and texture-content animation
not explicitly classified, the historical allocation churn corrected under PPD608, and inherited third-party release gaps.
Next action is the owner's live Temporal67 review: detail, moving avatars/camera, hair/foliage/transparency,
water, resize/mode changes and snapshots. No additional Home FPS investigation or optimization is authorized
by this handoff; the review branch and iCloud receipts record publication/delivery separately.

## Preserved Spatial parent checkpoint

The sections below are historical Spatial observations; statements that Temporal was not implemented refer
to that parent checkpoint and are superseded by the active Temporal state above.

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

Initial69457 native/linear/Spatial performance, image and resize evidence remains in the
[Spatial implementation record](../research/metalfx-spatial-20261124.md); it is historical and is not
Temporal69501 evidence. OpenHTJ2K0.27 remains the sole pixel decoder.

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

**Historical Spatial code checkpoint:** `a5142ae5c2c6dd1fe8b2fa89c9479eb213cff0f5` (actual build 69464; later test-fixture/documentation edits do not change its application identity).

| ID | State | Remaining boundary |
| --- | --- | --- |
| M-008 | doing | Broad feature/visual acceptance and representative performance. |
| M-011 | doing | Reproducible release and strict no-GL qualification. |
| M-012 | doing | Maintained scripts verified; PPD597 manual-suite classification repaired; broader coverage remains. |
| M-013 | doing | Retained features and prolonged stability. |

Required independent build/review evidence and release gates remain open.
PPD319 Home performance and broad visual/feature/release qualification remain open.
A reviewable prototype does not establish complete-product or release readiness.

## Exact next actions

Continue the active native prototype qualification above. Previous sections are preserved
historical checkpoints, not claims that this new mode is already qualified or delivered.
