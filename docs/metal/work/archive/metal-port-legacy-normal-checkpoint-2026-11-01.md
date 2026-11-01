# Archived execution checkpoint — legacy normal-data contracts

**Renderer checkpoint:** `7b27e5d62e`
**Superseded:** 2026-11-01 by the controlled authored-mirror checkpoint in the
active ledger.

## Decision and result

Source comparison isolated two raw-data contracts rather than a broad renderer
rewrite. Commit `80924cdae2` corrected material normal-map storage in opaque,
lit-alpha and impostor-alpha owners. Commit `7b27e5d62e` corrected
standard/generated bump-map storage in the native G-buffer owner.

The standard-bump physical negative control failed before the fix with half
channel mismatches `56820 != 65280` and `15337 != 32895`, then passed after the
storage correction. Focused validation passed 1,322 assertions; complete Debug
and RelWithDebInfo physical suites each passed 44,865 assertions/50 cases. The
neutral gate passed 484/484 tests, and source-exact build 68012 passed deep
signing plus 349/349 ARM64 slice verification.

## Environment resolution

The Ninja cache selected Homebrew Bison 3.8.2. A build-local Python 3.14 virtual
environment supplied the repository-declared `llsd 1.2.4` and `PyYAML 6.0.3`
modules required by generated LSL compiler inputs and Python-peer tests. Bison
and Python modules were independent configure requirements.

## Product assessment and next boundary

The checkpoint advanced retained material correctness with causal physical
controls, but did not close M-008. Foliage cutoff/coverage and inspected PBR
contracts matched their reference owners, so the next evidence-based action was
a bounded projector, authored-mirror or other advanced-pass comparison. M-009
feature/stability coverage, M-011 owner-by-owner OpenGL removal and qualifying
M-010 performance evidence remained critical and deferred, not waived.
