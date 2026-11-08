# Native vertex-buffer upload generations — 2026-11-08

This checkpoint records the repair of Metal vertex/index upload selectivity and
shared-buffer lifetime. It is a source, unit/GPU-test and build checkpoint; no
runtime visual or performance gain is claimed.

## Original behavior and defect

- Exact original renderer source is commit
  `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`; its
  `indra/llrender/llvertexbuffer.cpp` blob has SHA-256
  `9c0f590c730a6566f9da8ad3eaaed04383c856fc103f32aa35d1a1c33d6486ad`.
- The original Apple path uploads only a written vertex or index store, plus a
  store whose GL name has not yet been created. Repeating `glBufferData` keeps
  the name but gives it fresh backing, so already queued draws retain the old
  store without a CPU/GPU wait.
- Native insertion commit `401282ddc7` placed Metal writes inside those GL
  predicates. Under Metal authority `mGLBuffer` and `mGLIndices` deliberately
  remain zero, so both predicates were always true and every dirty flush copied
  both complete stores.
- `ALMetalBuffer::write()` writes directly into shared storage. Reusing the
  same published resource did not reproduce the original orphaning lifetime
  and could overwrite bytes still referenced by queued native work.
- The index resize guard also tested only `mGLIndices`, so native buffers could
  leave the old CPU index allocation behind when resized.

## Implemented repair

Commit `e90ccdf18f076cc3beb6d8fe97e4dd045c8d2cb5`:

- tracks native vertex and index initialization independently of GL names;
- retains the original separate dirty-store contract, so vertex-only changes
  do not upload indices and index-only changes do not upload vertices;
- publishes later writes through a fresh or completed reusable Metal
  generation, retiring the prior generation against `ALFrameFences` instead of
  blocking the GPU;
- keeps generations captured by raw render-batch pointers alive across buffer
  resize/destruction until their logical frame completes, then collects them;
- keeps a failed native upload dirty for retry; and
- releases the CPU index store on native resize by checking the mapped store,
  not the absent GL name.

The generation policy is conservative within the open logical frame. A burst
may allocate another store rather than reuse storage that could still be read;
completed extras are reclaimed at the next update and globally retired stores
are collected during ordinary buffer flushes. Normal shutdown drains the
Metal scheduler before final collection.

## Validation

- `tools/metal/doctor.sh`: exit 0 on MacBookPro18,2 / M1 Max / 64 GiB,
  macOS 26.5.2, Xcode 26.6, Apple GPU family 7.
- Focused Release and assertion-enabled RelWithDebInfo native tests: 46/46
  assertions pass in each configuration.
- The real-device test proves selective vertex/index publication, preservation
  of an encoded old generation, completed-generation reuse, native index
  resize accounting, and raw-pointer validity when `LLVertexBuffer` is
  destroyed before encoding.
- Full Release `llrender_tests`: 1,938/1,938 assertions across 226 cases
  (225 passed, one skipped).
- Full real-device `almetal_tests`: 52,842/52,842 assertions across 59 cases.
- Registered `ctest` case `llrender_tests/llvertexbuffer_test`: pass.
- `python3 tools/metal/validate_harness.py`: 32 required files pass;
  `python3 -m unittest discover -s tools/metal/tests -v`: 31/31 pass.
- Source-exact `tools/metal/build.sh build` from `e90ccdf18f` produces signed
  Xcode Release `26.4.0.68525`; deep strict signing and 349/349 ARM64 slice
  verification pass.
- Executable SHA-256:
  `cb42e7fd22575fba27096ba40ce2e514cade94a82eb8a0a68a2cdb9840315403`.
- Packaged `almetal_builtin.metallib` SHA-256:
  `163168fe50cedaa022a87608f354655246eb110838ad6ba113172555882181a1`.

## Remaining evidence

At the next owner-directed viewer launch, smoke dynamic world/avatar/UI content
at Low, Mid, High and Ultra and watch for geometry corruption or stale indices.
Any performance attribution requires the paired protocol; this repair removes
avoidable upload work and a lifetime risk but does not by itself establish an
FPS improvement.
