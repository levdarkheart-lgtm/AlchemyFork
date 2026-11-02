# Native Lights checkpoint

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Code commit:** `4b755ffc95` (`feat(metal): replay lights debug overlay`)

## Observed owner contract

Develop **Lights** is now captured and replayed by the native opaque-post scene.
For each light drawable, the source emits translucent-cyan face triangles,
followed by a white spatial-extents box with depth testing disabled, followed by
a yellow light-radius box with ordinary depth testing. The Metal path preserves
that geometry, state and source order:

- face draws reuse the existing position/index buffers, exact byte ranges,
  transforms and traversal order with source-alpha RGB blending, destination
  alpha retained, disabled culling and forward/reverse `LEQUAL` read/write depth;
- the white procedural outline uses always-pass/write depth;
- the yellow procedural outline uses forward/reverse `LEQUAL` read/write depth;
- the MSL outline emits the same 24 box-edge vertices as `drawBoxOutline`;
- source order is merged with Points, Agent Target, Texture Anim, Update Type
  and Batch Size, with duplicate or non-monotonic order rejected;
- HUD faces use the explicit untextured debug shader, while immediate-mode HUD
  outline geometry remains in ordered generic UI capture; and
- invalid storage, color, transform, range, state, order or allocation fails
  closed, while off/empty/non-light input is an identity operation.

Only `RENDER_DEBUG_LIGHTS` was removed from native-presentation ineligibility.
The remaining 11 visual debug modes and latent pathfinding guard remain
fail-closed.

## Physical validation

- Focused RelWithDebInfo world replay: 2,380 assertions, pass under Metal
  API/GPU/load-store/shader validation.
- Complete RelWithDebInfo, Debug and Release suites: 46,079 assertions in 52
  cases each, pass; warm elapsed times 1.22, 1.38 and 1.16 seconds.
- Controls prove cyan source-alpha blending and destination-alpha preservation,
  reverse winding with no cull, triangle depth writes, white always-pass depth
  writes behind existing depth, yellow forward/reverse `LEQUAL` visibility and
  depth writes, both order directions, collision rejection, invalid input,
  no-owner identity and the offline MSL entry point.
- Exact viewer translation units `pipeline.cpp.o` and
  `llspatialpartition.cpp.o` compile under warnings-as-errors.
- Warnings-as-errors RelWithDebInfo production integration compiled, linked,
  staged and signed across 1,632 cacheable tasks.
- The non-manual/non-GL gate passed 484/484 in 35.40 seconds.
- Source-exact `AL_METAL_CONFIG=Release AL_METAL_JOBS=8
  tools/metal/build.sh build` passed at `4b755ffc95` after 127 cacheable tasks and
  staged Development build `26.4.0d68067`.
- Deep strict signing passed; bundle inspection found 349 ARM64 Mach-O slices
  and zero invalid slices.

SHA-256 provenance:

- executable: `8f2815a0848ae5028aad7f460625d3c3f3228d74ba29f97253bd02913fa1d5d6`;
- source and packaged MSL: `ad52fd989f6a3442eaf0fe38348b9968c8721d68d663cb468418af12ed5bda32`;
- compiled metallib: `b0af557be051b32b1fa07d85ec4d0bc98c883b319e05f7bd6d5c3098da18eaca`.

The main executable still links Metal and OpenGL. Strict no-GL, broad live
parity, stability and qualifying performance gates remain open.

## Persisted owner decisions

- Exact RLVa parity remains deferred non-release nice-to-have M-013. Exact
  comparisons require enabling RLV in both viewers, saving and restarting both;
  mandatory work is only the minimum needed for Metal/no-GL cutover.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers before capture. Off/on is invalid; both-off is diagnostic only and
  cannot close shadow parity.

## Next executable action

Implement `RENDER_DEBUG_IMPOSTORS`. Bounded source inspection shows a single
four-edge white outline around an already native-captured impostor billboard.
Preserve its additive blend, inherited depth, source order and time-decaying
5-to-1 pixel line width; prove those contracts before removing only that bit.
