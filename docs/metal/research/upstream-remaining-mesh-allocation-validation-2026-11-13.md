# PPD-048 retained optimizer allocation safety — 2026-11-13

Local commit `51d53a6dee5852abec9e408b5b7be75a88bb1666`, branch `codex/ppd048-retained-alloc-20261113`, worktree `/private/tmp/alchemy-ppd048-retained-alloc-20261113`, base `2a5af4fdaca0196f508f8f077f9b5c3de07555de`. Three source files only: llvolume.cpp/h and existing tests/almesh_decode_safety_test.cpp. Canonical source/build/docs/CMake untouched by this child. Parent owns PPD-048 register and final native caller guards/full build. No push.

## Reference / ownership

Read upstream dc0f4b478c's allocation handling and the actual retained U16/Mikk implementation. Its required tangent/weld work must report failure nonfatally; optional cache ordering may preserve valid existing triangles. The allocation half of the upstream replacement cannot be cherry-picked into this older implementation. The actual native geometry owner remains LLVolumeFace; no renderer API or tangent math/geometry algorithm was replaced.

The old retained path raised LL_ERRS for worker allocation failures, wrote an unchecked tangent allocation, gave up index ownership before optional allocation/ordering, and used U16 meshoptimizer adapter destructors that copy scratch output even during unwinding. The patch removes worker fatal handling, allocates an entire reweld replacement under local aligned RAII owners before publication, and retains original positions/normals/UV/weights/indices on required failure. Optional order uses the same cache algorithm with its native U32 input/output and copies a completed result into the existing U16 indices only on success; bad_alloc retains original valid triangles and returns true. No buffer handoff remains in optional ordering.

Legacy fallback output/scratch allocations are guarded and published only on success. `LLCalculateTangentArray`, `LLVolumeFace::createTangents`, and `LLVolume::genTangents` return bool; cacheOptimize propagates fallback failure. Its lineSegmentIntersect caller skips a face whose requested tangents cannot be generated. `mOptimized` remains set on entry on success/failure, matching the established one-attempt contract; new tests check this.

Native consumer follow-up owned by parent: four llface.cpp genTangents calls must return false from getGeometryVolume on failure; material preview sphere creation must return an empty preview when createTangents fails before its later raw tangent read. Bench/tests otherwise call the helpers only with successful ordinary allocations. No other production LLCalculateTangentArray call exists.

## Validation on actual ARM64 production code

All runtime probes ran inside LLDB containment. Launch used `process launch -X false`; any stopped child is backtraced and killed by the driver, never resumed/passed a signal. All runs below exited normally; no fault-stop/crash-dialog control was needed. One initial LLDB `run` invocation failed before launching because shell expansion of a Catch tag was rejected by lldb-argdumper; corrected to explicit no-expansion launch and retained that distinction.

- Compile: `/private/tmp/alchemy-ppd048-validate-20261113.py`, exit 0. Actual llvolume.cpp and existing focused test file compile with canonical Release/O3/C++23/ARM64/macOS26/Werror flags, PCH removed, outputs isolated under checks directory. Actual canonical dependency archives are read-only link inputs.
- Production tests: `/private/tmp/alchemy-ppd048-run-contained-20261113.py .../mesh_tests '[llvolume]' final-production`: **293 assertions / 8 cases passed**, child exit 0, LLDB exit 0. These include real compressed decoder regressions, zero/bad-index admission, high-valence order preservation, required weld allocation injection, and every meshoptimizer optional-order scratch allocation. New allocator tests verify original buffer identity/order, no outstanding injected scratch blocks, and mOptimized semantics.
- ASan + UBSan: `/private/tmp/alchemy-ppd048-sanitizers-20261113.py`, compile/link exit 0; contained mesh_san_tests `[llvolume]` also **293 / 8 passed**, child/LLDB exit 0, no sanitizer diagnostics. Production llvolume and test TUs are instrumented; existing dependency archives are not rebuilt/instrumented by this child.
- Negative control: exact base 2a5af4fdac llvolume.cpp, linked with the new allocation tests, fails **both cases**: required vertex remap raises LL_ERRS; optional order allows std::bad_alloc to escape. Child exits **42**, LLDB exits 0. No signal stop. Driver correctly reports unsuccessful child status separately.
- Ordinary-output parity: old/base and fixed implementations run the same shuffled 20x20 weighted/unweighted grid through tangent and no-tangent cacheOptimize, plus direct legacy tangent generation. Fingerprints include counts, exact triangle order, position/normal XYZ, UVs, tangent XYZW and all weights. Both produce **004fd052b5f3a793** with child/LLDB exit 0. Unused position/normal padding lanes are intentionally excluded. This is bounded bit parity evidence, not a claim for every possible mesh.

Private real allocation-null tests use a preincluded compile-only wrapper around the existing aligned allocation calls in llvolume.cpp, plus scoped operator-new failure in a separate scratch binary. No test allocator hook enters production source or app runtime. It verifies required replacement position/tangent/weight null returns, each Mikk input array's bad_alloc, legacy output/scratch null returns, and the actual 65,535-vertex weld fallback propagating failure. Final frozen-source repeat: **217 assertions / 4 cases passed**, child/LLDB exit 0; raw final-aligned-new-faults log retained in checks directory. The wrapper is confined to private fault test builds; it is not an alternate algorithm.

## Evidence / limits

Checks directory: `/private/tmp/alchemy-ppd048-checks-20261113` contains compile/link command JSON, raw compiler/runtime logs, exact base source copy, paired parity source and binaries, private faultshim.hpp and faults_test.cpp. Reproduction scripts: `/private/tmp/alchemy-ppd048-validate-20261113.py`, `...-run-contained-20261113.py`, `...-fault-build-20261113.py`, `...-parity-build-20261113.py`, `...-sanitizers-20261113.py`.

No physical out-of-memory viewer incident, scene launch, GPU/visual acceptance, speedup, full replacement-geometry equivalence, or push approval is asserted. The parent must integrate native failure guards and run its full math/app suite. Separate follow-up: successful legacy normalization still uses normalize3fast on zero normals; root requested a PPD-043 control/reopen before any corresponding math edit. This allocation patch deliberately preserves that preexisting math while documenting its distinct risk.

SHA256 `mesh_tests`: `d7a925dc5b50df2b1ecbac229139c1aee0b3ad4e6b86897c750b0f081c40ac69`

SHA256 `mesh_san_tests`: `5c3ab4e3b34e9f41e0cb537ed70ff976cfdc0337b285ad57d3800a385bd0a223`

SHA256 `mesh_parity_old`: `39009f15cbc1cc44527f801b8f83683f9b3c636a8e1609fd66abe5b29620bfda`

SHA256 `mesh_parity_new`: `56c3a52706b50f19d42d84176d63432fe9e54845236c40d314141c6ac308ea3c`

## Exact focused runtime commands

```sh
python3 /private/tmp/alchemy-ppd048-run-contained-20261113.py /private/tmp/alchemy-ppd048-checks-20261113/mesh_tests '[llvolume]' final-production
python3 /private/tmp/alchemy-ppd048-run-contained-20261113.py /private/tmp/alchemy-ppd048-checks-20261113/mesh_san_tests '[llvolume]' sanitized-production
python3 /private/tmp/alchemy-ppd048-run-contained-20261113.py /private/tmp/alchemy-ppd048-checks-20261113/mesh_fault_tests '[ppd048-private]' final-aligned-new-faults
python3 /private/tmp/alchemy-ppd048-run-contained-20261113.py /private/tmp/alchemy-ppd048-checks-20261113/mesh_old_failure_tests '[mesh-allocation-safety]' old-fail-control
python3 /private/tmp/alchemy-ppd048-run-contained-20261113.py /private/tmp/alchemy-ppd048-checks-20261113/mesh_parity_old '[ppd048-parity]' old-parity
python3 /private/tmp/alchemy-ppd048-run-contained-20261113.py /private/tmp/alchemy-ppd048-checks-20261113/mesh_parity_new '[ppd048-parity]' new-parity
```

Each command uses a separate contained child. Expected failed old control returns driver 1 for child 42; all others return driver/child 0. LLDB returns 0 even for child 42 and is reported separately in JSON/raw logs.

## Separate PPD-043 legacy zero-normal control

Contained `mesh_fault_tests '[ppd043-zero-control]'` tested direct successful createTangents and the actual 65,535-vertex cacheOptimize(true) fallback with zero normals. Both returned success but produced NaNs, yielding 15 failed checks / 17 assertions / 2 cases, child exit 42, LLDB exit 0, driver 1. No signal stopped. This is a distinct preexisting successful-output math issue, not an allocation-test failure. Parent notified; no math edit applied in frozen PPD-048 commit. Exact run:

```sh
python3 /private/tmp/alchemy-ppd048-run-contained-20261113.py /private/tmp/alchemy-ppd048-checks-20261113/mesh_fault_tests '[ppd043-zero-control]' zero-legacy-control
```

Historical artifact note: subsequent separately authorized PPD-043 follow-up reused the normal compile output path once. The original normal `mesh_tests` binary was replaced after its 293/8 run; its recorded checksum describes that historical run. Its raw final-production log remains unchanged. The exact 51d53a6dee ASan binary, parity binaries, allocator negative-control and private fault binary were copied with matching checksums into `checks/frozen-ppd048/` before further follow-up. PPD-043 now uses `/private/tmp/alchemy-ppd043-legacy-checks-20261113` separately. Do not treat the mutable top-level normal binary as the frozen PPD-048 artifact.
