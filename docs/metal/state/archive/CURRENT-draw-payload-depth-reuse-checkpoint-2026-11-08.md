# Draw-payload and shared-depth repair — 2026-11-08

The owner authorized implementation after the maximized `68547` comparison
identified continued low FPS. Renderer commit
`175cd155dd90947066113d3dbe8015f1c394a91f` implements the next bounded repairs.
This checkpoint does not claim a new viewer FPS result.

## Measured cause and original contract

The [68547 runtime checkpoint](CURRENT-68547-hero-cpu-runtime-2026-11-08.md)
records a 95%-busy main thread, 62.1% GPU-active union, 8.2 median settled FPS,
and 65.70 CPU encoder ms/present. Current draw-record construction samples
2,217 ms (7.66% of main activity), while shared-data publication and inlined
hashing sample 2,779 ms (9.60%). Hero rendering remains approximately half of
active main-thread samples. GPU base lighting, haze and reflected views also
remain substantial; these repairs do not remove all CPU or GPU work.

Original reference remains `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`:

- `lldrawpoolmaterials.cpp` constructs indexed material arrays inside the
  material draw path and uploads at most eight slots. Original simple draws
  do not construct unused PBR/material/coverage capture records. The native
  records have no original storage equivalent, so preserve their existing
  producer/consumer defaults, ordering and whole-transaction failure policy.
- Original shader uniforms are shared program data. The new native arena is
  completion-safe storage for identical shader bytes; vertex and fragment
  stages may bind the same immutable range without uploading it twice.
- `alrenderresourceplan.cpp` assigns each Screen target the GBuffer's shared
  depth owner; its source SHA-256 is
  `398bbc83dbac2743a5bcd3809994029ec4864f47540ac73ca68a857cc213a6fd`.
  The original `doAtmospherics()` also states that screen shares G-buffer
  depth. The native lighting output instead allocated depth and copied the
  complete source depth/stencil after the base pass, despite already borrowing
  that same source during depth-classified base shading.

## Implemented changes

### Initialize only the draw family that is used

`ALMetalWorldSimpleBatch` stores legacy and PBR slot arrays in separate inline
`std::optional` payloads. Nonconst access constructs the appropriate payload
with exactly the prior slot defaults. Const access to absent data returns
immutable defaults without constructing it. Shadow batches use the same
policy for their texture/cutoff/PBR coverage payload. User-provided default
constructors prevent enclosing value-initialization from zeroing inactive
optional storage.

This avoids constructing 384 bytes of legacy slots and 1,600 bytes of PBR
slots for ordinary simple/avatar/marker draws, and 640 bytes of coverage data
for opaque shadow casters. Terrain PBR may still use PBR slots and terrain
fields together. Null indexed material gaps preserve identity transforms and
all previous default factors. Every producer, consumer and fixture was updated.

There is no new per-draw heap allocation. Physical record sizes increase from
984 to 992 bytes for shadows and 3,536 to 3,552 for simple batches; this is an
initialization repair, not physical compaction. Vector relocation may still
copy inactive storage, while the hot producers retain in-place construction.

### Publish identical stage constants once

The renderer publishes a constant block once and binds that range to both
vertex and fragment stages wherever the bytes and length are identical.
This covers PBR, terrain, forward/effect blocks and covered shadows. Without
an arena, the existing two inline Metal byte setters remain unchanged.
Distinct shader stages retain their original slots and every required binding.

Arena pages now cache the permanently shared buffer's mapped pointer at
creation. Range admission and 256-byte alignment remain checked before writes;
copying uses that address directly. `ALMetalBuffer::write()` previously did
the same shared-memory copy after repeated range/storage/native-contents
queries; it supplied no flush or staging behavior. Dedup comparisons use the
same cached address. Completion tokens, exact-byte fallback, retirement and
all-user-before-reuse semantics are unchanged.

Producer generation shortcuts were investigated but not applied blindly.
Rigged-object palettes have a frame epoch; classic avatar palettes include
the view matrix, and materials lack a trustworthy mutation generation. Pointer
reuse alone cannot safely replace the remaining byte identity checks.

### Carry the original shared scene depth through lighting

The pipeline opts into `ALMetalWorldBaseLightingScene::borrow_depth` while
keeping its G-buffer alive throughout lighting/post/UI. The lighting output
allocates pooled color only and uses `attachBorrowedDepth()` to carry source
depth. The wrapper owns its color-only child target, which returns to the
existing pool with its original descriptor. It borrows externally owned depth;
the source must outlive the wrapper.

This removes one depth allocation and full depth/stencil copy per lighting
output. Simply borrowing depth in the pooled descriptor would disable pool
reuse and recreate HDR color every view; the owned color child prevents that
regression. Standalone callers retain independent copied-depth behavior by
default. Read-only base/local lighting depth state, later water/haze snapshots,
reverse-Z, stencil, six hero faces and directional shadow cadence remain.

## Verification and artifact

- Physical M1 Max/64 GiB, macOS 26.5.2, Xcode 26.6, Apple GPU family 7.
  `tools/metal/doctor.sh` exits 0 with host access. The first sandboxed attempt
  exited 1 on `sysctl`; this was a sandbox restriction, not a GPU failure.
- Release and RelWithDebInfo builds of `almetal_tests` exit 0. Both complete
  suites with `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1` exit 0:
  **53,838 assertions/68 cases each**. The first Release build caught six
  remaining indexed fixture accesses; these were corrected before passing.
- New tests cover lazy payload defaults, independent copies, moves,
  relocation/reset, shared depth writes, unchanged texture allocation over
  pooled frame wrappers, depth/stencil GPU readback after source wrapper
  retirement, and incompatible attachment rejection. The existing full pixel
  suite exercises both shared and copied base-light depth with identical
  accumulated lighting/depth results and all existing material/terrain/avatar
  families.
- `tools/metal/build.sh build` exits 0 with warnings-as-errors. Source-exact
  Xcode Release **26.4.0.68550** embeds full renderer SHA `175cd155dd…` at
  `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
- `codesign --verify --deep --strict <app>` and
  `tools/metal/verify_app.sh <app>` exit 0; 349/349 inspected slices are ARM64.
  The next live comparison is pending.
- Executable SHA-256:
  `b54af1c45224738f9a8f6ec42e17a5ea3cc20478cb900e260c77101d9a66ecf9`.
- Packaged metallib is unchanged, SHA-256:
  `5bd02961a77fdfcef9d28410e9f92fc86d1de08197aec113660e0cd0ccd779d1`.

Local build/test logs are `/private/tmp/alchemy-metal-next-cpu-*`; raw live
captures and account-specific manifests remain local and ignored.
`python3 tools/metal/validate_harness.py` exits 0 (32 required files), and
`git diff --check` exits 0; CURRENT/ledger remain at 160/180 lines.

## Mechanism measurements, not viewer FPS

An opt-in hidden `[frame-data-benchmark]` test uses seven warm repetitions of
20,000 logical operations, with native bindings/GPU/commit time excluded.
At renderer `175cd155dd`, Release, identical 1,296-byte PBR data published twice
costs median 274.723 ns/logical pair; one publication costs 128.981 ns, about
53.05% less. Unique 80-byte appends cost 14.352 ns and repeated 5,296-byte
palette publication 496.713 ns. These figures isolate the publication mechanism
and do not predict viewer FPS. The benchmark exits 0 with 68 checks; all timed
warm samples create zero new arena pages.

The separate source-labelled constructor comparison uses archived headers,
identical `-O2 -std=c++20 -arch arm64` flags, reserved 2,048-record vectors,
64 warm-up rounds and nine measured samples of 512 rounds. Its evidence is
local at `/private/tmp/alchemy-metal-constructors-20261108.gYluae`. The packed
microbenchmark does not model interleaved world traversal or establish how
many milliseconds a live frame saves.

| Constructor | Baseline median, ns | Candidate median, ns | Change |
| --- | ---: | ---: | ---: |
| Opaque shadow | 29.559691 | 12.204528 | -58.71% |
| Plain simple | 140.354197 | 38.707137 | -72.42% |

Candidate header is archived from exact `175cd155dd`, SHA-256
`abf43bf3343ead04e7aa7b1cd2b6268131a11413953908a8893d0b35bbbe4682`;
the constructor comparison JSON SHA-256 is
`d8da4c0ffb77feaf4e29e37371377bd27260099f547bf81acfbc04f867908a99`.

An independent native Metal CPU-encoding experiment used identical draw work,
4x4 targets, 11 samples of 2,048 draws and excluded commit/wait. Bulk texture
setters changed eight-slot CPU time by -2.68% and 32-slot time by +0.37%; the
32-slot sign changed between repeats, so no substantial native texture API win
is established. Bulk vertex buffers helped changed-handle cases 15.18%/20.73%
for four/eight streams, but replacing the existing same-handle offset setters
regressed 70.76%/99.18%. No bulk setter rewrite was applied. Evidence is local at
`/private/tmp/alchemy-metal-bulk-encoding-20261108.5lsbL2`, comparison JSON
SHA-256 `15eb263f21ec7540f0b03240a8b6510451d419539acba8a9a7015011de4d40e7`.

A read-only base-light audit finds the native bucket/neighbor probe traversal
matches the exact original; no extra native double walk was established.
Skipping lighting for fully fullbright legacy pixels without environment, or
skipping sharp hero reflection work whose result is unused at zero environment
intensity, are source-grounded candidates with unmeasured eligible coverage.
They were not added to this candidate. Existing traces have no counter set and
shader timeline disabled, so their labelled durations do not identify texture,
instruction or register bottlenecks. Preserve the negative haze experiments
and measure those candidates before another shader rewrite.

## Next boundary

`68550` is built and GPU validated. The existing `68547` viewer has not yet
been replaced. A specific request to close it and run the maximized
Midday/`Preset123` comparison is pending because the visual QA skill requires
permission before terminating an existing viewer. Preserve quality, complete
window/camera setup before the collector's warm-up ends, collect 300 exact
consecutive frames, then take separate CPU/GPU traces. Do not claim a live FPS
gain until that comparison exists. Product performance/parity/release gates
remain open.
