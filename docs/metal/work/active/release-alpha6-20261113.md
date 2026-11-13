# Alpha 6 — full upstream-port release

2026-11-13. Owner explicitly requests building and publishing the entire pushed
upstream-port work, with a local ZIP/checksum fallback if GitHub Actions budget
blocks execution. This authorizes one new release and its assets; no billing,
permissions, default-branch merge, forced update or private capture publication.

## Verified publication outcome

- Source pinned by manual [run43473591856](https://github.com/levdarkheart-lgtm/AlchemyFork/actions/runs/43473591856),
  source `af72868396053aecd3862b6bd63383470387bd80`, tag
  `26.4.0.68984-alpha6`, channel `Alchemy Metal Alpha 6`.
- The existing remote branch matches this exact source. Five unrelated untracked
  research notes are excluded. No application or renderer source change.
- The ARM64 Mac job started, so this run passed the historical account startup
  gate. Toolchain, source/harness validation, dependency preparation, configure
  and precompile source-identity checks pass. The previous hosted compile took
  56min34sec; a local incremental package completed first. Run43473591856 was
  deliberately canceled during compilation, with publication skipped. This is
  not a budget or compiler failure.
- Local `tools/metal/doctor.sh` exits0 on Apple M1 Max/64GiB, macOS26.5.2,
  Xcode26.6. Local configure/build,346ARM64 slices, strict signatures,
  source identity/imports/features/dependency receipt,116tool tests and strict
  source audit3540files/0blockers/0errors pass.
- The archive extracts with all8272entries (7839files/2links plus directories),
  hashes, modes and link targets identical; extracted signature, ARM64 and
  release-provenance rechecks pass. Viewer binary SHA256
  `fd3eee275281f3fed93cec905ff83d975710e3a1a1285f027a9b61e469f31c9f`.
- ZIP320285701bytes, SHA256
  `1839e2109946fd79255e58d2b595c20802240501c1d8355e48226bc714a5efff`.
  [Alpha6](https://github.com/levdarkheart-lgtm/AlchemyFork/releases/tag/26.4.0.68984-alpha6)
  is published at18:04:02UTC, release444559882; exact tag source matches. ZIP,
  checksum and build-provenance assets are present with matching GitHub digests.
- Private evidence and prepared release notes:
  `artifacts/metal/release-alpha6-20261113/`.

Dispatch:

```sh
gh workflow run tag-release.yaml -R levdarkheart-lgtm/AlchemyFork \
  --ref codex/apple-silicon-metal \
  -f source_ref=af72868396053aecd3862b6bd63383470387bd80 \
  -f alpha_number=6 -f release_tag=26.4.0.68984-alpha6
```

## Failure history and next action

PPD-014 retains all earlier Actions failures: missing pip/NumPy, source-SHA
discovery and historical payment/spending-limit startup rejection. The latest
prior manual run43338929395 succeeded and published Alpha5; earlier failures do
not prove the current budget is exhausted. No billing limit or workflow service
state is changed here.

The local incremental build uses the existing Release objects and a clean pinned
OpenHTJ2K build with the workflow's portable M1 tuning/receipt, keeping the same
accepted decoder. Source-code files are unchanged from the pushed revision;
only local documentation receipts are dirty. Configure embeds exactaf72868396,
numeric revision68984 and channel `Alchemy Metal Alpha 6` before compilation.

Validation commands and exits are retained in the private logs. In particular,
`AL_METAL_PRESET=ninja-os-arm64 AL_METAL_CONFIG=Release AL_METAL_JOBS=4 tools/metal/build.sh`
exits0; `verify_app.sh`, deep strict `codesign`, `verify_release.py`, strict
`audit_gl.py` and the archive manifest comparison exit0. The first bare-python
tool invocation fails1 because PyYAML is missing; the configured build venv has
NumPy2.5.4/PyYAML6.0.3 and reruns all116tests successfully. An initial checksum
invocation uses the wrong directory and exits1; rerun in package/ exits0 and
`shasum -c` confirms the ZIP. No source/tool repair was necessary.

Completed: the published tag points to exactaf72868396 and release metadata
also names that source. Full-scope release notes cover the entire upstream ports.
All three independently downloaded assets match GitHub digests and local files;
extracted signatures,346ARM64 slices, source identity, imports and retained
features pass. Private `publication-receipt.json` records the final outcome.
A validation attempt during download correctly rejects the partial file; the
completed download passes. No additional checks after the owner asks to finish.
Next action: owner may download/install the prerelease; no further release work remains.

This is an owner-directed early prerelease. PPD-083/086 have owner-confirmed
colour-repair evidence; publication does not close other unresolved records.
Broad visual/feature parity, prolonged stability, independent review and measured
quality-equivalent performance remain open product gates.
