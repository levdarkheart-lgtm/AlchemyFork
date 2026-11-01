# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `7b27e5d62e` (`fix(metal): preserve legacy bump normal data`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is a recovery snapshot, not a chronological log. Older checkpoint detail is
retained in the indexed archive and must not be copied back into this file.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Source comparison found two distinct
legacy normal-data seams. Commit `80924cdae2` preserves raw material normal maps
in opaque, lit-alpha and impostor-alpha owners. Commit `7b27e5d62e` preserves
raw standard/generated bump maps in the native G-buffer owner, matching
`LLDrawPoolBump::bindBumpMap`; colour textures retain colour decoding.

Controlled Computer Use then exercised the guarded authored-mirror Snapshot
path in source-exact build 68012. The first cross-backend scores were invalid:
the effective profiles differed on 25 of 386 captured `Render*` settings,
including quality `3` versus `0` and mirror resolution `512` versus `256`.
Those scores are retained only as superseded history and are not parity
evidence.

Actual Computer Use repeated the fixed view with identical effective settings.
Post-run manifests compare 386/386 settings with zero mismatches: quality `3`,
mirrors enabled and mirror resolution `512`. The complete 2048x1476 pair scores
SSIM `0.987409` / PSNR `39.185075 dB`; the mirror frame scores `0.988105` /
`37.793470 dB`, and the mirror interior scores `0.991375` / `41.100904 dB`.
Visual and amplified-difference inspection finds no material mirror-region
residual. This rejects the alleged large blur regression for this controlled
fixture; it does not close broader probe/mirror parity. Continuous live native
authority remains fail-closed.

## Build and test evidence

- Homebrew Bison 3.8.2 is selected explicitly in the Ninja cache. The ignored
  build-local Python 3.14 environment contains repository-declared `llsd 1.2.4`
  and `PyYAML 6.0.3`; CMake now builds the LL script compiler for tests and
  registers 574 tests.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passes 484/484 tests; the preserved CTest log
  records 484 passed and zero failed.
- `env AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64
  AL_METAL_CONFIG=RelWithDebInfo AL_METAL_JOBS=8 tools/metal/build.sh build`
  produced warnings-as-errors Development 26.4.0.68012 from the code checkpoint;
  the immediate repeat reports `ninja: no work to do`.
- `codesign --verify --deep --strict --verbose=2` passes.
  `tools/metal/verify_app.sh` checks 349 Mach-O slices with zero non-ARM64
  results.
- Executable SHA-256:
  `de6165c5a7a79236f13a8828414987b96b6e8ae431581df99628b8c37e936cd0`.
  Repository and packaged MSL are byte-identical at
  `3b385c1eb6330ee96cc0e62ec48f1f03ee5658f851852baa9fb3be4331c40b5f`;
  packaged metallib is
  `ed21aecadcdc0feabbcb83245fba5946ec53625099d059930e0e159feda37abf`.
- Exact-path launch used disposable profile `/tmp/alchemy-metal-m012.UGPJCd`,
  Metal API validation and nonpersistent
  `--set ALMetalHeroMirrorSnapshotValidation true`. The saved 2048x1476 native
  PNG has SHA-256
  `40cb056d15ac6c60364396a7c0603aa1d0599c372f8a63dcd193b56dd356e557`;
  the isolated log records guarded validation and native colour readback.
- Semantic login, teleport, Midday, Snapshot refresh/save and logout succeeded.
  The process exited 0; `LogoutReply`, `Goodbye!` and stopped status were
  observed, and no viewer/media process remained. Transient incomplete cloud
  generations occurred during scene arrival but ended before the successful
  native capture; they are not counted as parity or performance evidence.
- Matched reference/candidate PNG SHA-256 values are respectively
  `b9de1870760ea028e0bfa2f03297625ad589edefdb6f1f0a887eb04d6e70eea7` and
  `126771e01bf02ac117cc0b633026d63f726721e19177d82fe8d7eb7f52a26f80`.
  Both sessions exited semantically and left zero viewer processes.
- The compact live state/ledger, indexed archives, seven-invariant coverage map,
  live-link/command-path checks, Python bytecode checks and shell syntax checks
  pass. The new fail-closed Computer Use preflight makes exact app version,
  executable hash, singleton process, isolated profile and open-log identity
  mandatory before binding. Effective graphics manifests now make image
  comparisons fail closed on mismatched or unilateral visual settings.
  `validate_harness.py` validates 31 required files; all 22 tooling tests,
  Python bytecode compilation, `git diff --check` and the M1 Max doctor pass.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-008 native renderer | doing | Broad native world/UI owners and physical regressions exist; advanced-family reference parity is incomplete. |
| M-008 advanced views/effects | doing | A matched 386-setting authored-mirror pair rejects the suspected large blur defect for one fixture; broader probes, post, transparency/PBR and advanced-family parity remain open. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. |
| M-010 measured performance | deferred | Structural wins exist, but no qualifying paired M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Development/Diagnostic LLUI route remains complete; exact-path isolated-session preflight now rejects stale or ordinary-profile bindings. |

## Complete-product advancement assessment

**Advances the complete product:** yes. The checkpoint corrects the validation
contract: every image comparison now requires identical effective graphics
settings, and the corrected fixed-view pair rejects a false mirror-defect
signal. Exact binary/profile attestation and native-readback logging make the
result attributable. This is validation-integrity progress, not a new renderer
implementation or broad M-008 parity claim.

**Deferred critical work:** complete advanced-family parity; feature-matrix and
stability coverage; removal of viewer-owned OpenGL traversal/link/context use;
independent build/review evidence; and paired, quality-equivalent M1 Max
performance measurements. These are requirements, not waivers.

**Evidence-based priority:** stop spending implementation time on the disproven
large mirror-blur hypothesis. Resume the open M-008 matrix with a source-led
audit of post effects and alpha/PBR advanced owners; add a physical control for
the first demonstrated contract gap and use the graphics-manifest gate for any
new live pair. M-009, M-011 and qualifying M-010 still depend on stable parity.

## Exact next actions

1. Audit the next open M-008 post-effect and alpha/PBR owners against the
   authoritative pass graph; identify one concrete missing or divergent data,
   state or ordering contract before editing.
2. Add an asymmetric physical control for that divergence, implement the
   bounded correction, then run focused and complete Metal validation. Require
   identical post-run graphics manifests before scoring any new live pair.
3. Keep strict no-GL, feature parity, stability and performance gates open until
   their documented release evidence exists.
