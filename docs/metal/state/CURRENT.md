# Current engineering state — production convergence

Observed 2026-11-21, Apple M1 Max / 64 GiB / macOS 26.5.2 / Xcode 26.6.
Branch: codex/apple-silicon-metal, canonical checkout.
**Renderer code checkpoint:** `a2aaaf2942`
Indra tree: `6e3b13c1f4076c5244bfa708ce564d8e03cd6cdd`.
Maintenance **26.4.0.69445 is staged and ready for owner testing**.
[Candidate report](../research/maintenance-candidate-69445-20261121.md) records scope,
exact app/source hashes, validation and residuals.

Owner authorizes coherent Apple-first repairs, removal of obsolete graphics
preferences and abandoned disabled experiments, plus a separate candidate for
manual testing. All changes remain local. Candidate creation performed no
push/release, viewer launch/login, runtime settings modification or live profiling.
The owner subsequently requested launch: the exact staged69445 executable/hash
was verified and process11514 observed running. No login action or scene/profile
collection was requested or performed (owner-launch-69445.json).

## Current implementation and evidence

[Execution ledger](../work/active/production-convergence-20261121.md) retains exact
commands, source identities, failures, implemented scope and actionable residuals.
[Source audit](../research/scheduling-source-assessment-20261121.md) distinguishes
confirmed mechanisms from unmeasured costs. PPD-527–559 are retained in the
[defect register](../POST_PORT_DEFECTS.md); owner acceptance remains pending.

- Worker configuration is validated/logged. Timed retries release service,
  priority has bounded fairness and lifecycle/ticket identity, no-op updates
  coalesce, and shutdown retains requests until actual owner termination. PPD-558
  repairs inherited timed-wait deadline/head changes; actual baseline 10 failures
  versus fixed 27 assertions / 4 cases. Generic full-capacity request progress PPD-556
  remains explicit next architecture work; housekeeping cannot block admission.
- Cache promotion is connected; bounded async fast-cache, pinned cache→fetch
  completion, dedicated ready-result delivery and accepted-decode cancellation
  replace avoidable frame handoffs. Publication stays bounded/owner-safe.
- ResourcePlan hits avoid owning temporaries; lighting uses completion-aware
  immutable frame slices; point PSOs are eager. Geometry's required owner phase
  has a truthful API, regular reply budget is live, LFS foreground sleeps removed.
- Eleven obsolete backend settings and thirteen abandoned experiment controls
  retire from source and old-profile preservation. Live quality/media/HiDPI/mesh
  features remain. Advanced-panel setting callbacks disconnect on destruction.
- Rejected world preparation, command banks/rows/animated GPU records, early/world
  GPU visibility, orphan resource tables, main-light overlap and forced rejected
  E6 consumers plus exclusive tests are removed. Retained CPU/probe/shadow/terrain
  and rigged output/lease contracts remain, with actual live test fragments
  restored/registered. PPD-551 source-routing counters persist through shader reload.
- OpenHT 0.27 is an unconditional production pixel decoder. Unreachable alternate
  pixel decode/selection removed; OpenJPEG header/encoding bodies unchanged.
  Required archive/source/receipt checks and licence/attribution stay mandatory.
- Final normal Release build 06 passes. CPU/asset/settings 55/55 and native
  API/shader-validation 56/56 scopes pass, with no skips. Routing/reload tests
  pass 1,864 assertions; actual offline-library rigged depth passes 765 checks.
  Tool suite: 184 pass, one optional retained-stream case skipped. Pipeline-cache
  ordinary/instrumented/selective modes pass separately. Earlier failed fixtures,
  build attempts and environment/script errors remain in the execution ledger.
- Staged app: signature/deep-strict verification passes; all 346 Mach-O files are
  ARM64-only. Strict source audit: zero blocking findings/scan errors. Exact
  source/stage inventory: 7,839 files and two internal symlinks; no external links.
  Required OpenHT licence/attribution present; native decoder test passes.

## Ownership and recovery

All three agents are frozen; root exclusively owns integration/build/test/staging
and canonical docs. Task implementation worktrees are integrated and retained for
source/evidence recovery. Existing worktrees/native-im-translation is untouched.
Private evidence: artifacts/metal/production-convergence-20261121.

Owner-tested baseline remains at
/Users/dekstroza/Alchemy-candidates/Alchemy Metal Accepted 69417.app; executable
SHA256 0b8bab95fd3da4312029734a3b1c0a9a12b3b76d0227bd99d97eb0ce9ae1e1f1.
No current owner session or settings are inferred from this preserved app.

## Complete-product advancement assessment

| ID | State | Evidence / remaining boundary |
| --- | --- | --- |
| M-008 | doing | Native maintenance advanced; broad scene/feature acceptance remains. |
| M-011 | doing | Final static source/ARM64/signature checks pass; runtime/foreign subprocess qualification remains. |
| M-012 | complete | Existing harness/build tools retained, maintained and exercised. |
| M-013 | doing | Alchemy/RLVa/media features retained; broader runtime qualification remains. |

No product/release completion. PPD-319 GPU completion/performance, broad parity,
loading, prolonged stability, foreign subprocess qualification and owner acceptance
stay open. Reproducible distinct open-source release, strict no-GL and
independent build/review evidence remain required. Source/build/component checks
do not establish whole-viewer runtime or performance acceptance. Prior evidence
remains in [pre-convergence history](archive/CURRENT-pre-production-convergence-20261121.md).

## Exact next actions

Owner tests the staged app at
`/Users/dekstroza/Alchemy-candidates/Alchemy Metal Maintenance 69445.app`,
covering loading/teleport/refinement, graphics controls, shader reload and shutdown.
Executable SHA256: `0c5de0c90878f85c7e71296c279d303adda475be39a9c31923b10a942e2b9d89`.
Do not launch/login/profile without the applicable owner instruction. No push is
authorized by automated success or candidate delivery.

Engineering follow-through starts with PPD-556's bounded request scheduler and
nonfatal admission design. Handoff/service-age instrumentation, immutable geometry
preparation, queue service/capacity and upload CPU work remain in the linked ledger.
PPD-527–555/557–559 are ready_for_owner; PPD-556 remains investigating.
Owner acceptance, whole-viewer performance and release gates remain open.
