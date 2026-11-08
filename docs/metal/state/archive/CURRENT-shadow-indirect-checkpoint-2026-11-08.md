# Cached native shadow commands — 2026-11-08

Implementation is complete following the owner's instruction to address
continued single-digit FPS. The predecessor is renderer `175cd155dd` and
source-exact Release `68550`; its matched runtime evidence is in
[the predecessor report](CURRENT-68550-draw-payload-runtime-2026-11-08.md).
This source/test checkpoint is not yet a new viewer performance result.

## Measured target and original contract

The predecessor records 59.33 ms/present of disjoint CPU encoder intervals,
a 94.68%-busy sampled main thread, and 67.07% GPU-active union. Hero views
account for 29.56 CPU encoder ms/present and separate shadow encoders 16.82.
Inclusive native draw, texture-binding and vertex-binding stacks occupy
14.76%, 10.38% and 6.87% of main-thread activity; these overlap and must not be
added. Current culling is also substantial. Explicit completion waiting was
not the dominant running stack.

Exact original remains `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
Original `pipeline.cpp` 12820–12839 culls/sorts each shadow camera separately,
then 12883–13063 submits the selected caster families. Per-face membership
cannot be reused from another view. Ultra's six faces and two directional maps
per face remain required. Static opaque draws use the same indexed triangle
positions, MVP, culling and depth-clamp state as the native direct path.
Covered, rigged, avatar and tessellated terrain draws retain their scalar path.

## Implementation

Long contiguous static opaque runs with one culling mode use cached native
`MTLIndirectCommandBuffer` commands. Runs preserve exact order and stop at
all other draw families; at least 64 draws/run and 128/view are required.
Up to 16,384 draws/view are admitted and remaining runs stay scalar. A single
immutable 80-byte-per-draw constant slab replaces individual arena publication.
The arena and command slots retire against real completion tokens. A leased
or pending slot cannot be rewritten. Cached native references remain strongly
owned until their corresponding commands are replaced, preventing native
address reuse from creating a false cache hit. Indirect resources are declared
once per encoder; scalar vertex/fragment binding caches are invalidated after
execution. Static PSOs explicitly opt into indirect support on supported GPUs.
The direct path remains available for device compatibility and equivalence
proof, without introducing another graphics backend or changing quality.

Because Metal's `useResources` declaration does not retain the referenced
buffers, the parent command explicitly retains the shared ICB pool. Before
submission these owners live only on the C++ command wrapper; an abandoned
command releases them without a native-handler cycle. Commit moves the list
into a completion handler that clears it at actual GPU completion, breaking
the pool/token/native-command/handler cycle even if Metal keeps the finished
handler. Commands without owners allocate neither a list nor this callback.
Thus destroying the renderer, helper, batch leases and source wrappers while
work is pending cannot release its indirect resources early.

Terrain factor collection is folded into full shadow validation and its vector
is allocated only when a terrain batch exists. Every later batch is still
validated before rendering begins.

The base-light shader also skips work whose result is unused by the original
legacy equation: exact fullbright with zero environment returns linear albedo
with the original classic 1.1 scale, and the sharp environment SSR/hero finish
runs only with positive environment intensity. PBR and partial fullbright keep
the existing equations. Original `class3/deferred/softenLightF.glsl` source
SHA-256 is `edb59377bb7e1ee30f687abe3738372e41e297e2b730e7adc2646345fbc17b9e`.
Eligibility in the current live scene is not yet measured.

Benchmark draw telemetry now counts native encoded logical draws, including
populated indirect commands, instead of the dormant GL counter. It remains
explicitly armed and aggregates once per encoder without per-draw atomics.

## Verification status

- Doctor passes on physical M1 Max/64 GiB/macOS 26.5.2/Xcode 26.6.
- Offline shader and isolated Objective-C++ syntax checks pass.
- Final full Release and RelWithDebInfo suites each pass **56,620 assertions
  across 73 cases** with `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1`, exit 0.
  Focused validation passes 2,934 assertions/six cases. Tests include exact
  direct/ICB raster output, reverse-Z, clamp/cull, both index widths and byte
  offsets, scalar-family transitions, same-parent two-view preparation,
  maximum/range/admission checks, late-invalid fail-closed behavior, destroyed
  helpers/source wrappers, actual completion/abandonment, command/token cycles,
  shared-owner alias identity, native draw census, and 20 shader boundary cases.
- The initial focused fixtures exposed missing Metal diagnostic labels and an
  incorrect analytic assumption that fragment raster Z is itself clamped;
  fixtures were corrected without weakening direct/ICB byte equality. The
  subsequent SDK review added explicit native resource retention before the
  final full runs. No viewer FPS is inferred from these checks.
- Product build and matched live comparison are pending.
- Isolated cached native-command prototypes improve CPU encoding on this host;
  they do not prove a viewer FPS gain or the real many-resource tradeoff.

## Measured mechanism and remaining work

A 3,000-draw wrapper-versus-helper benchmark alternates order, rotates three
pending views and checks exact GPU output. Across 1–1,000 geometry pairs
(3–2,001 unique buffers), cached helper CPU preparation/encoding is roughly
53–85% lower with both stable and changing native identities. It excludes
source-vector construction, resource creation, submission and waiting. The
old viewer and other local work were active; this is a mechanism test, not a
net viewer FPS result. Raw comparison SHA-256 is
`97485890e62bc37d12dcf2d23e13e8a880ae23023cb65b216bed82861f198edb` in
`/private/tmp/alchemy-metal-shadow-indirect-tests-20261108.HGz01r`.

The pool retains its high-water slots and cached native references until
replacement or renderer teardown; live main/probe slot telemetry was added.
The main pending-batch limit is not a global ICB bound: hero faces allow six
pending commands and main shadows are standalone. Track actual driver memory
and slots, without multiplying shared resource references into fake bytes.

The predecessor's 15.74% broad substring-inclusive culling category is not all
frustum traversal: `updateCull` is 6.72%, visible extents 5.24%, and `postSort`
includes render-map bookkeeping (categories overlap). Read-only exact-original
tracing identifies future span appends, resetting private raw-pointer lists by
size/end, and skipping unused genuine-shadow alpha-group collection as narrow
candidates. They are not yet implemented or measured. Existing geometry and
bound rebuild dirty gates already avoid much repeated work; another view's
frustum membership cannot safely replace current per-view traversal.

Independent read-only review covered ownership, callback cycles, scalar cache
invalidation, original view/caster boundaries and native telemetry arming.
`python3 tools/metal/validate_harness.py` and `git diff --check` exit 0; CURRENT
and the active ledger remain at their 160/180-line recovery limits.
