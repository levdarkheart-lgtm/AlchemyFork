# Current engineering state — production convergence

Observed 2026-11-21, Apple M1 Max/64GiB/macOS26.5.2/Xcode26.6.
Branch: codex/apple-silicon-metal, canonical checkout.
**Renderer code checkpoint:** `5ce8f94aba`
This is an integrated work checkpoint, not a built owner candidate yet.

Owner authorizes coherent Apple-first scheduling/ownership repairs, removal of
obsolete graphics preferences and abandoned disabled experiments, followed by
one separate candidate for manual testing. Keep fixes local; no push/release or
automatic login. The earlier discussion-only restriction is superseded.

## Current implementation and evidence

[Execution ledger](../work/active/production-convergence-20261121.md) owns the work
and explicit residuals; [source audit](../research/scheduling-source-assessment-20261121.md)
retains original claims and qualifications. PPD528–546 plus527 are tracked in the
[defect register](../POST_PORT_DEFECTS.md), with owner acceptance pending.

- Root5d7dc12078: insertion-only ResourcePlan ownership, completion-aware lighting
  pages, eager point PSOs, truthful required geometry interface, live reply budget,
  LFS foreground sleep removal and obsolete render/window/stat consumers removed.
- Rootb9304951d8: exact retired Global settings policy prevents old profiles from
  recreating dead backend controls; unrelated unknown settings stay preserved.
- Scheduler2c9628e07a integrated60932652bf: validated widths, fair timed retries,
  wake/lifecycle generations, housekeeping coalescing and safe shutdown. Actual
  contained117assertions/10cases pass; full integrated build not yet run.
- Preferences96d937b816 integrated10dc03df11: eleven dead backend controls removed,
  native quality gates corrected, GL help corrected, two panel callbacks properly
  disconnected. Actual-body320checks0 ASan/UBSan; original has62availability
  failures plus callback-after-destruction ASanUAF. No viewer crash attribution.
- Main-overlap retirement9bb0f4e289 and caller cleanup5ce8f94aba retain normal
  point values and owned scalar inputs. Backend world-preparation cleanup and
  bank/visibility retirement are still in independent worktrees.
- Doctor0. Root native target build0; nine relevant Metal CTests0/176.92s with
  API/shader validation, including pending-view lighting isolation and reuse.
  These checks precede the latest cleanup; final source qualification pending.
- OpenHTJ2K remains sole pixel decoder. No new viewer launch, profile, settings
  change, automatic login, push, release or whole-viewer speedup claim.

## Ownership and recovery

Root owns integration, Git writes, CMake, llappviewer and canonical docs.
Asset agent: worktrees/production-assets-20261121, texture/cache/fetch/list/decode
and tests; finishing bounded async handoffs, ready results, cancellation/priority.
Worker agent: worktrees/production-world-cleanup-20261121, worldprepare modules,
prepared-only almetalworld branches and related tests. Shared world.cpp/h remain
exclusively his until frozen handoff. Preferences agent now owns
worktrees/production-experiment-retirement-20261121: pipeline, old bank/page/world
visibility modules and exclusive tests; shared world.cpp/h waits worker handoff.
Root alone runs full build; children do not commit/escalate because their inherited
.git sandbox is read-only. Existing native-im worktree stays untouched.

Owner-tested69417 is preserved at
/Users/dekstroza/Alchemy-candidates/Alchemy Metal Accepted 69417.app,
executable SHA2560b8bab95fd3da4312029734a3b1c0a9a12b3b76d0227bd99d97eb0ce9ae1e1f1.
No current owner session or runtime settings are inferred from the preserved app.
Private new evidence: artifacts/metal/production-convergence-20261121.

## Complete-product advancement assessment

| ID | State | Evidence / remaining boundary |
| --- | --- | --- |
| M-008 | doing | Native correctness/maintenance advanced; broad owner scene parity remains. |
| M-011 | doing | Prior static no-GL/ARM64 evidence retained; final candidate gates pending. |
| M-012 | complete | Existing harness/build tooling retained; production cleanup now uses it. |
| M-013 | doing | Prior RLVa/features retained; broader feature/runtime verification remains. |

No product/release completion. PPD319 GPU completion/performance, broad parity,
loading, prolonged stability, foreign subprocess qualification and owner acceptance
stay open. Reproducible distinct open-source release, strict no-GL and
independent build/review evidence remain required and unproved by this maintenance checkpoint.
Prior release/shoreline/BoM/IM evidence is retained in
[pre-convergence history](archive/CURRENT-pre-production-convergence-20261121.md).

## Exact next actions

Freeze and integrate asset source and prepared-route cleanup, then retire mapped
abandoned bank/visibility code while keeping active CPU/probe/shadow contracts.
Add every discovered repair/failure to the register/ledger. After all code is
committed, configure the existing isolated Release tree with a distinct Maintenance
channel and pinned sole OpenHT decoder, run tools/metal/build.sh, relevant CTests,
strict audit and verify_app; stage and hash a separate app for owner testing.
Do not deliver the older9-test checkpoint as the final changed candidate.
