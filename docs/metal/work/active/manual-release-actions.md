# Manual Apple Silicon release workflow correction

2026-11-12. PPD-014. Owner requests stopping unrequested Actions, investigating
prior failures, fixing a manual release from current codex/apple-silicon-metal,
and triggering/following the corrected GitHub workflow. This authorizes the
necessary workflow commit/push, manual dispatch, new tag and release publication;
no account spending limit, billing credentials or repository permissions change.
Source starts at81bff208a7; isolated codex/manual-apple-release worktree. Owner's
`odex/apple-silicon-metal` spelling is interpreted as the existing codex branch.

## Observed failures

- Run43306501400: Metal harness, push event at81bff208, docs path trigger. Harness
  itself passes;10of76tool tests fail because NumPy is absent. Five recent runs
  have this same numerical-test dependency gap.
- Run42930335974: tag26.4.0.68648 lacks-alpha and enters the inherited multi-platform
  build instead of the native job. Windows/Linux fail compiling native Metal-only
  code; Mac acquisition is canceled. This fork rejects non-macOS/non-ARM64 builds.
- Run43094454819 and harness43094412476: GitHub annotations reject jobs before
  startup for failed account payments/spending limit. No runner/build logs exist.
- Run43115393161: stale documentation references to an untracked research folder.
- Native alpha2/alpha3 runs42816882219/42432449254 succeeded on xcode-27. The label
  is currently supported; its name is not evidence of an invalid runner.

Source evidence: owner-linked [run43306501400](https://github.com/levdarkheart-lgtm/AlchemyFork/actions/runs/43306501400)
and prior [release attempt42930335974](https://github.com/levdarkheart-lgtm/AlchemyFork/actions/runs/42930335974).
Private raw metadata/jobs/logs/annotations/timing and report are retained under
`artifacts/metal/actions-manual-release-20261112/history/`; no secrets/captures are
published. Timing APIs report zero billable milliseconds for the linked harness
and matrix; do not infer actual charges or remaining account allowance from that.
The repository is public. GitHub documents standard public hosted runners as
free/unlimited, while an account-level billing gate is a separate observed issue.
[Runner documentation](https://docs.github.com/en/actions/how-tos/write-workflows/choose-where-workflows-run/choose-the-runner-for-a-job).

## Repair scope and service state

Seven legacy automatic workflow IDs are disabled manually: Build404472385,
MetalHarness404476487, CodeQL404472389, Sanitizers404472398, Lint404472394,
CheckPR404472387 and Labeler404472392. Tag a Build404472402 remains manual-only.
Default branch main still owns inherited schedule definitions, so branch YAML
changes alone cannot suppress them; disabled service state is intentional.
[Dispatch/default-branch behavior](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows).

Old runs43095969441/43095632694 appear queued with no active jobs. GitHub's normal
cancel reports completed, while force-cancel reports not queued (409); retain
this inconsistent external state without claiming successful cancellation.
No current in-progress runs were present before the requested manual release.

On the Metal branch, retire the unbuildable upstream matrix/PR/security automations
and keep explicitly manual release/harness/lint entry points. Reuse registered
path tag-release.yaml so manual dispatch on codex works without changing default
branch or merging application source to main. The release is one macos-26 standard
ARM64 job,2compile jobs, bounded retries/cache, no pushes/tags/timers triggering it.
Release assets avoid duplicate large Actions artifact storage.

Preserve accepted OpenHTJ2K0.27.0 by rebuilding clean pinned72ea045 source with
an ARM64 build receipt; don't silently select OpenJPEG or require the original
machine-specific archive hash. Legacy local archive guard stays unchanged.
Fresh build/receipt gates/8real decode cases match accepted output; tool tests84
pass. Static BSD notices must enter the bundle before package signing.

## Current progress

- doing: release workflow source/ref/tag identity, native build, archive validation
  and explicit prerelease publication; manual NumPy installation fixes harness.
- doing: pinned dependency receipt/license/provenance tests and workflow lint.
- todo: commit/push fixes, explicit dispatch of current corrected source, follow
  terminal run, investigate/recover any observed failure and verify release assets.

No new hosted run or release has been declared successful. Owner's live viewer,
cache and preferences remain untouched. Exact next action: finish local workflow
validation, then dispatch tag-release.yaml on codex/apple-silicon-metal with an
immutable source SHA and the next alpha number.

## Local repair validation

New source contains only manual dispatch workflow events; regression tests guard
that property, the single standard ARM64 job/no matrix, and numerical dependency
installation before harness tests. Initial actionlint rejects runner.temp in a
job-level env context; moving it to the toolchain step's GITHUB_ENV fixes that.
Initial bare local Python lacks PyYAML for the new policy tests; central pinned
requirements and a disposable validation venv make the test environment explicit.

Pinned OpenHT builder/source and receipt/legacy gates pass;8real private texture
cases have identical dimensions/channels/pixel hashes with accepted versus rebuilt
archives. Extra exact BSD license asset is installed before signing, and a
real disposable ARM64 staging/signature test passes. Package validator has21
focused tests and the combined tool suite108tests passes. Read-only68898static
checks pass, while its older missing dedicated license path correctly fails the
new asset rule; the owner's running app is never modified.

Dependency setup is inherited from the previously successful native alpha path;
standard macos-26 replaces a multi-platform matrix, with2jobs and public runner
usage. Seven automatic workflows are disabled. Dependabot's dynamic service ID
rejects disable with422; it is separate from the disabled build/test schedules.
Old stale runs also reject cancel/force-cancel and have no acquired jobs.

Final local lint/tests/source gates are being recorded before the single explicit
manual dispatch. Source ref will be the exact pushed corrected commit, not a
branch name that can move between request and checkout.

Final local checks before publication: actionlint1.7.12.25 exit0, harness33files0,
all108tools tests0 (24.25s in declared-dependency venv), bashsyntax0 and diff0.
No renderer/voice/music implementation source is changed. The source dependency
receipt and pre-sign license install are the only application CMake changes.
This is not a claim of a successful hosted release; the next operation is the
explicit owner-requested workflow dispatch after commit/push.

Publication boundary: a concurrent local water investigation reserves PPD-013.
CI is PPD-014 in the canonical local register. Shared register/state/Metal ledger
changes remain local with that investigation; this publication includes only the
requested CI/release scope and this work record, based on approved remote81bff208.

## Hosted attempt and repair

Scoped repair ea1ff8d9d52f1df85bb0a0f7075ea0e5213eee5c was pushed to the
Metal branch. Initial dispatch43312529221 had an incorrectly entered full source
SHA and was immediately canceled before the build. Corrected run43312577772
acquired macos-26 ARM64, checked out exact source, passed identity/SDK checks,
and completed build-tools setup. It failed before compilation because that
setup intentionally creates a cached Python venv without pip, while the next
step tried python -m pip. Add NumPy to the existing cached build-tools requirements
and probe imports directly; retain the standalone manual harness's setup-python
pip install. Rehearse the real pip-free tools environment before a new dispatch.
No release was published by either attempt; no billing gate occurred.

Exact inherited python_tools.py key/build/activate rehearse with all configured
requirements succeeds on this M1 Max. Target venv explicitly has no pip; NumPy
2.4.3/PyYAML6.0.3 imports and all108tool tests pass there (11.863s). Actionlint,
harness and diff checks pass. The next dispatch uses this corrected immutable
commit. Evidence: artifacts/metal/actions-manual-release-20261112/python-tools-rehearsal/.

## Follow-up parallelism correction

Owner notes the usual native build takes about52minutes, consistent with the
prior successful runs. Inspection confirms the old native job uses all logical
CPUs, while the initial replacement limits vcpkg/viewer compilation to2. The
standard macos-26 runner has3CPUs; restore3for subsequent manual runs. Current
run43314101072 retains its immutable2job workflow; do not cancel/rebuild it.
At22:31UTC the live log reaches133/169dependencies and confirms AOM completed.
This is observed build progress, not a successful release. Future parallelism
change stays in this isolated worktree until the hosted outcome is recorded.

Hosted checkpoint22:57UTC: all169dependency packages and native configuration
complete successfully (configure62m30s); Actions dependency binary cache saves
successfully. Viewer compilation is now running. Prior successful native runs
had~20mconfigure/~30mcompile/package; this cold two-job attempt has taken longer.
Do not report a release until packaging/signatures/provenance and assets pass.
