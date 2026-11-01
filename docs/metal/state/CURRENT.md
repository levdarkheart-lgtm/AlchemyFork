# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `80924cdae2` (`fix(metal): sample legacy material normals linearly`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is a recovery snapshot, not a chronological log. Older checkpoint detail is
retained in the indexed archive and must not be copied back into this file.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Source comparison found that legacy
material normal maps use raw `ALSamplers::AnisoWrap` in the OpenGL reference,
while three native Metal owners decoded sRGB-backed normal assets as colour.
Commit `80924cdae2` changes only those normal bindings to
`setFragmentTextureLinear`; diffuse/specular colour maps keep colour decoding.

The production-boundary negative control held normal-map bytes constant and
changed only the texture storage declaration. Before the fix it failed in all
three owners:

- opaque G-buffer half channels: `56820 != 65280` and `15337 != 32895`;
- impostor-alpha coverage: `64 != 189`;
- lit alpha half output: `11900 != 11936`.

After the fix, the focused RelWithDebInfo case passes 1,316 assertions. Complete
Debug and RelWithDebInfo physical suites each pass 44,859 assertions across 50
cases with Metal API, GPU and shader validation enabled.

## Build and test evidence

- Homebrew Bison 3.8.2 is selected explicitly in the Ninja cache. The ignored
  build-local Python 3.14 environment contains repository-declared `llsd 1.2.4`
  and `PyYAML 6.0.3`; CMake now builds the LL script compiler for tests and
  registers 574 tests.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passes 484/484 tests in 242.64 seconds.
- `env AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64
  AL_METAL_CONFIG=RelWithDebInfo AL_METAL_JOBS=8 tools/metal/build.sh build`
  produced warnings-as-errors Development 26.4.0.68010 from the code checkpoint;
  the immediate repeat reports `ninja: no work to do`.
- `codesign --verify --deep --strict --verbose=2` passes.
  `tools/metal/verify_app.sh` checks 349 Mach-O slices with zero non-ARM64
  results.
- Executable SHA-256:
  `9e11fb690f2afb0216073bec1eeed985ef1eca2da70ca4b4799295065dc52fe4`.
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
| M-008 advanced views/effects | doing | Latest legacy-normal seam is closed; projector, mirror geometry, alpha/foliage and remaining PBR fidelity stay open. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. |
| M-010 measured performance | deferred | Structural wins exist, but no qualifying paired M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Development/Diagnostic LLUI accessibility route is validated end to end; Release remains excluded. |

## Complete-product advancement assessment

**Advances the complete product:** yes. The checkpoint fixes a source-proven
material-correctness defect in every native owner of the affected legacy normal
data and converts previously omitted toolchain-dependent tests into an executed
gate. The harness checkpoint reduces context/execution drift but does not
substitute for renderer parity. Neither change by itself closes M-008 or a
release gate.

**Deferred critical work:** complete advanced-family parity; feature-matrix and
stability coverage; removal of viewer-owned OpenGL traversal/link/context use;
independent build/review evidence; and paired, quality-equivalent M1 Max
performance measurements. These are requirements, not waivers.

**Evidence-based priority:** continue M-008 at a different bounded, source-led
alpha/foliage or PBR fidelity seam. M-009, M-011 and qualifying M-010 results
depend on stable renderer parity, so activity volume is not a reason to jump to
premature GL removal or performance claims.

## Exact next actions

1. Select the next untested M-008 alpha/foliage or PBR source contract, add a
   causal physical negative control, implement the first demonstrated mismatch,
   and rerun focused plus complete Metal validation.
2. Use actual Computer Use only when a controlled visible fixture is causal;
   otherwise prefer deterministic physical GPU coverage.
3. Keep strict no-GL, feature parity, stability and performance gates open until
   their documented release evidence exists.
