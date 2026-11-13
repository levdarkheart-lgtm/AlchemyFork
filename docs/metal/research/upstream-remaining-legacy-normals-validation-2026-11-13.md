# PPD-043 retained legacy zero-normal follow-up — 2026-11-13

Separate local commit `c0820bb184` (parent `51d53a6dee5852abec9e408b5b7be75a88bb1666`) on branch `codex/ppd048-retained-alloc-20261113`; worktree `/private/tmp/alchemy-ppd048-retained-alloc-20261113`. Two files: llvolume.cpp and existing tests/almesh_decode_safety_test.cpp. Source ownership of createTangents normalization was explicitly coordinated with the sculpt agent, whose fields/sculpt body edits are independent. Root explicitly reopened PPD-043 with red evidence before this repair. No canonical source/docs/CMake edits by this child; no push.

## Confirmed scope and change

The earlier zero-preserving Mikk normalization did not cover successful legacy createTangents, including cacheOptimize's weld-width fallback at 65,535 vertices. Normalizing an absent zero normal with normalize3fast produced NaNs. The new branch preserves zero/tiny normals at squared length <= FLT_MIN by clearing them. Ordinary nonzero normals retain the exact established normalize3fast call; no alternate normalization or tangent-generation algorithm was imported. All PPD-048 allocation ownership and bool failure propagation remain intact.

The permanent focused regressions call actual createTangents with zero normals and actual cacheOptimize(true) with 65,535 unique vertices and zero normals, confirming the retained legacy fallback is used. They verify success, tangent publication, vertex count and zero normal preservation over the entire wide face. No artificial replacement normal vector is introduced.

## Tests and controls

All runtime probes ran under LLDB with `process launch -X false`; stopped faults would be backtraced/killed rather than continued or passed to CrashReporter. Every probe below exited normally. These are CPU geometry tests, not visual/GPU acceptance.

- Before repair, frozen PPD-048 production code: direct legacy and actual 65,535-vertex fallback both generated NaNs; **15 failed checks / 17 assertions / 2 cases**, child **42**, LLDB **0**, driver **1**. Raw log `/private/tmp/alchemy-ppd048-checks-20261113/frozen-ppd048/zero-legacy-control-lldb-run.log`. This was a controlled assertion failure, not a signal stop or crash dialog.
- Fixed production llvolume.cpp and focused test compile with ARM64/macOS26/C++23/Release/O3/Werror, exit **0**.
- Fixed focused actual decoder/cache/allocation/zero-normal suite: **311 assertions / 10 cases pass**, child **0**, LLDB **0**.
- Fixed ASan+UBSan instrumented production/test TUs: same **311 / 10 pass**, child/LLDB **0**, no sanitizer diagnostics. Linked canonical dependency archives remain uninstrumented/read-only.
- Old 2a5af4fdac and fixed c082 output controls on ordinary shuffled 20x20 weighted/unweighted meshes, tangent/no-tangent order and direct legacy tangent generation retain the same exact semantic-field/triangle-order fingerprint: **004fd052b5f3a793**. Each control child/LLDB exits **0**. This checks ordinary nonzero output and excludes unused vector padding; it is bounded parity evidence, not an all-assets assertion.

One scratch parity compile initially failed before launching because its copied input file was not yet present. The input-copy ordering was corrected, and subsequent compile/link/control runs all exited 0. No production change was needed for that harness setup issue.

## Exact runtime commands

```sh
python3 /private/tmp/alchemy-ppd043-legacy-run-contained-20261113.py /private/tmp/alchemy-ppd043-legacy-checks-20261113/mesh_tests '[llvolume]' fixed-production
python3 /private/tmp/alchemy-ppd043-legacy-run-contained-20261113.py /private/tmp/alchemy-ppd043-legacy-checks-20261113/mesh_san_tests '[llvolume]' sanitized-production
python3 /private/tmp/alchemy-ppd043-legacy-run-contained-20261113.py /private/tmp/alchemy-ppd043-legacy-checks-20261113/mesh_parity_old '[ppd048-parity]' old-parity
python3 /private/tmp/alchemy-ppd043-legacy-run-contained-20261113.py /private/tmp/alchemy-ppd043-legacy-checks-20261113/mesh_parity_new '[ppd048-parity]' new-parity
```

Checks directory `/private/tmp/alchemy-ppd043-legacy-checks-20261113` contains exact compile/link JSON, source copies, binaries and raw LLDB logs. Scripts `/private/tmp/alchemy-ppd043-legacy-sanitizers-20261113.py`, `/private/tmp/alchemy-ppd043-legacy-parity-build-20261113.py` and the contained runner preserve isolated output paths. Initial normal compile used the same actual compile commands as PPD-048; its final objects/binary and command JSON were moved into this separate checks directory before subsequent work.

Root owns final combined math/app build, native consumer failure guards, register/ledger updates and owner acceptance. No product completion, visual incident attribution, full upstream replacement equivalence or push approval is claimed.

SHA256 `mesh_tests`: `b82d378b788e6275db5155e1d533404b48b3a0883195139cf9f7c5ad4602f57a`

SHA256 `mesh_san_tests`: `1c3db25204fc523ada2cbc1622f7bf194a6d577370b3667ac13a6663461427cc`

SHA256 `mesh_parity_old`: `e53e4c4a2a79c88d668d03123578151399bcdc564429a39debc000cc47fdc2fd`

SHA256 `mesh_parity_new`: `8d9c54a0e8aba14b79ebabfd4ebb3cf077ce2d78e9215c7897490fb8644bba84`
