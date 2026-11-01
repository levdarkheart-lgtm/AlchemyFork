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
path in source-exact build 68012. A fixed upper reflected band improved from
SSIM `0.710348` / PSNR `12.456081 dB` to SSIM `0.806706` / PSNR
`17.472924 dB` against the pinned aligned OpenGL reference. The complete mirror
interior improved from SSIM `0.859180` / PSNR `17.934595 dB` to SSIM
`0.874149` / PSNR `22.417276 dB`. The lower reflected floor still loses tile
detail and its diagonal grout line, so this is progress evidence, not parity.
Continuous live native authority remains fail-closed.

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
- The compact live state/ledger, indexed archives, seven-invariant coverage map,
  live-link/command-path checks, Python bytecode checks and shell syntax checks
  pass. The new fail-closed Computer Use preflight makes exact app version,
  executable hash, singleton process, isolated profile and open-log identity
  mandatory before binding. `validate_harness.py` validates 30 required files
  and all 19 tooling tests pass.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-008 native renderer | doing | Broad native world/UI owners and physical regressions exist; advanced-family reference parity is incomplete. |
| M-008 advanced views/effects | doing | Guarded authored-mirror colour improved against the pinned reference, but reflected floor detail remains over-blurred; parity and continuous authority stay open. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. |
| M-010 measured performance | deferred | Structural wins exist, but no qualifying paired M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Development/Diagnostic LLUI route remains complete; exact-path isolated-session preflight now rejects stale or ordinary-profile bindings. |

## Complete-product advancement assessment

**Advances the complete product:** yes. The source corrections now have a
controlled same-location authored-mirror comparison showing materially closer
upper reflected colour and better fixed-crop metrics. Exact binary/profile
attestation and native-readback logging make the live evidence attributable.
The residual reflected-floor blur prevents M-008 or any release gate from
closing.

**Deferred critical work:** complete advanced-family parity; feature-matrix and
stability coverage; removal of viewer-owned OpenGL traversal/link/context use;
independent build/review evidence; and paired, quality-equivalent M1 Max
performance measurements. These are requirements, not waivers.

**Evidence-based priority:** the fixed mirror crop localizes the remaining
failure to reflected floor detail rather than the whole scene. Trace hero-probe
LOD, roughness/normal inputs, mip generation and sampling against the OpenGL
owner, then add a causal asymmetric physical control only for a demonstrated
mismatch. M-009, M-011 and qualifying M-010 still depend on stable parity.

## Exact next actions

1. Trace authored-mirror hero-probe LOD, roughness/normal data, mip construction
   and sampling to explain the over-blurred reflected floor; compare source
   owners before changing code.
2. Add a physical negative control and implement a correction only after a
   specific divergence is demonstrated; rerun focused and complete Metal
   validation, then repeat the attested fixed-view comparison.
3. Keep strict no-GL, feature parity, stability and performance gates open until
   their documented release evidence exists.
