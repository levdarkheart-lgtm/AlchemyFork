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

The latest production-boundary negative control holds bump bytes constant and
changes only the texture storage declaration. Before `7b27e5d62e`, an
sRGB-backed standard bump asset changed opaque G-buffer half channels from the
raw result:

- `56820 != 65280` and `15337 != 32895`.

After the fix, the focused RelWithDebInfo case passes 1,322 assertions. Complete
Debug and RelWithDebInfo physical suites each pass 44,865 assertions across 50
cases with Metal API, GPU and shader validation enabled. The earlier material
normal negative control remains in those suites.

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
- This checkpoint did not launch the viewer and does not count an uncontrolled
  live scene as parity evidence.
- The compact live state/ledger, indexed archives, seven-invariant coverage map,
  live-link/command-path checks, Python bytecode checks and shell syntax checks
  pass. `validate_harness.py` now validates 29 required files and all 15 tooling
  tests pass.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-008 native renderer | doing | Broad native world/UI owners and physical regressions exist; advanced-family reference parity is incomplete. |
| M-008 advanced views/effects | doing | Two legacy-normal seams are closed; projector, mirror geometry and remaining advanced-reference fidelity stay open. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. |
| M-010 measured performance | deferred | Structural wins exist, but no qualifying paired M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Development/Diagnostic LLUI accessibility route is validated end to end; Release remains excluded. |

## Complete-product advancement assessment

**Advances the complete product:** yes. The checkpoint fixes a second,
source-proven material-correctness defect in the native owner of fetched and
generated legacy bump data. Physical negative control, complete validated GPU
suites, the neutral gate and a source-exact signed ARM64 package establish the
change; they do not close M-008 or a release gate.

**Deferred critical work:** complete advanced-family parity; feature-matrix and
stability coverage; removal of viewer-owned OpenGL traversal/link/context use;
independent build/review evidence; and paired, quality-equivalent M1 Max
performance measurements. These are requirements, not waivers.

**Evidence-based priority:** foliage cutoff/coverage and PBR base-factor,
normal/ORM, blend and culling contracts matched their reference owners during
this checkpoint, so do not manufacture changes there. Continue M-008 at a
different bounded projector, mirror-geometry or advanced-pass contract. M-009,
M-011 and qualifying M-010 results still depend on stable renderer parity.

## Exact next actions

1. Select the next untested M-008 projector, mirror-geometry or advanced-pass
   source contract, add a causal physical negative control, implement the first
   demonstrated mismatch, and rerun focused plus complete Metal validation.
2. Use actual Computer Use only when a controlled visible fixture is causal;
   otherwise prefer deterministic physical GPU coverage.
3. Keep strict no-GL, feature parity, stability and performance gates open until
   their documented release evidence exists.
