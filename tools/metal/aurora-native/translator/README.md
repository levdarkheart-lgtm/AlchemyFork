# Reproducible offline Aurora WGSL → native MSL

This is an **offline developer tool**. It is not linked into the viewer and adds
no Rust, Naga, wgpu or WebGPU runtime dependency to the application.

Source algorithm: Aurora `ee458fbf1d95e98c5b3a55c0027a523bb28d7e7f`, the WGSL
port of FidelityFX FSR 3.1 from SDK 1.1.4. The four pass modules are concatenated
with the unmodified pinned `common.wgsl` from the evaluation vendor directory.
Aurora's notices, GPL licence and AMD's MIT notice remain there. The generated
files retain provenance comments. They are produced rather than hand edited.

Run from the repository root with the pinned Rust compiler in the evaluation
record (Rust 1.99.0):

```
cargo run --release --locked --manifest-path tools/metal/aurora-native/translator/Cargo.toml -- "$PWD"
python3 tools/metal/aurora-native/translator/manifest.py --write
```

Naga is the **same Aurora fork/revision as the evaluated wgpu Metal backend**:
`6c13013b3f9c8a7e45154d45d120ada0cbe55a23`. Its MSL backend emits Metal 4.0,
explicit binding slots, bounds restriction for indexing/buffers/image loads,
normal loop bounding and integer-division checks. These retain the evaluated
backend's normal checked shader translation
policies, including its selected MSL 4.0 language on this macOS26 host. This
uses classic Metal APIs; it is not a Metal 4 command-system migration. Full
chronological output parity is checked independently. Whole modules retain all entry
points, including RCAS; the viewer never creates or executes RCAS pipelines.

Bindings are intentionally auditable: uniform buffer 0, sampler 0, texture N
for WGSL binding N, storage buffer N for binding N. Runtime-array byte lengths
are bound at buffer 30, in Naga's global declaration order. The component uses
Alchemy's existing completion-owned frame pages for the 208-byte constant ABI,
not per-world staging buffers.

`indra/almetal/shaders/aurora/*.metal` are the generated inspectable MSL.
`sources.h` embeds exactly these source bytes for the experimental viewer.
The native component compiles each module once during early preparation, using
`MTLLanguageVersion4_0` and `preserveInvariance=YES`, matching the evaluated
Metal backend's selected language, invariance and floating-point defaults. Defaults for floating-point compilation remain Metal's
normal defaults. A compilation failure rejects eligibility before jitter.
The compiler version is therefore part of application/test provenance; no
claim of compiler-independent binary equivalence is made.

The generated header also contains Naga's authoritative **entry-point name
reflection**, and every native PSO uses those names. Names need not be identical
to the WGSL spelling: the pinned `fs_farthest_depth_mip1` and
`fs_shading_change_mip0` become trailing-underscore MSL identifiers. The factory
requires both vertex and fragment functions before creating a PSO, because
Metal otherwise permits a vertex-only pipeline that silently clears a target.
The reflection/embedding tests retain this actual counterexample:

```
python3 -m unittest discover -s tools/metal/aurora-native/translator -p 'test_*.py' -v
```

The only handwritten shaders are the small convention adapter and glow-alpha
resolve in `almetalaurora.mm`. Adapter: reverse-Z depth is preserved, Alchemy's
RG previous-minus-current input pixels become Aurora's ZW current-minus-previous
UV, and a nonzero composition flag becomes Aurora's documented normal-X marker
2. The reconstruction still generates its own reactive mask from **genuine**
opaque-before-transparency versus final HDR color. Glow resolve preserves the
reconstructed RGB and samples the original alpha linearly at the jitter-adjusted
UV, matching Alchemy's existing glow convention. These integration passes are
included in measured component cost.

The runtime graph and fixed constants follow pinned `fsr.rs`. The only host
lifecycle adaptation stages parity/frame index/previous jitter until successful
main-world submission; an abandoned encode cannot advance history. Native
tracked resources are retained by the existing command-completion mechanism.

Verify generated identities without regenerating:

```
python3 tools/metal/aurora-native/translator/manifest.py
```
