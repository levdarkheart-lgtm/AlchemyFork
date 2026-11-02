# Native Batch Size checkpoint — 2026-11-02

**Host:** physical Apple M1 Max, ARM64, macOS 26.5.2, Xcode 26.6
**Branch:** `codex/apple-silicon-metal`
**Code commit:** `27c0bec87c` (`feat(metal): replay batch size debug overlay`)

## Observed owner contract

Develop **Batch Size** walks each spatial group's draw map and draws each valid
draw info's indexed triangles with its stable hash-derived debug color. The
source uses alpha byte 200, source-alpha RGB blending, no culling, inherited
forward/reverse `LEQUAL` read/write depth and polygon offset factor `-1` plus
units `+1`.

The native bridge retains existing Metal position/index buffers, index type and
byte range, current modelview-projection, exact normalized hash color and source
traversal order. Static geometry uses the ordinary debug vertex shader.
Object-skinned avatar geometry uses `TYPE_WEIGHT4` and retains the current source
matrix palette: four header floats plus 12 floats per joint with at most 110
joints. The palette and weighted positions are consumed by the dedicated MSL
`almetal_world_batch_size_skinned_vertex` entry point.

The renderer maps source polygon offset into Metal depth bias using the
forward/reverse-Z sign, retains source-alpha RGB-only blending and disables
culling. Batch Size merges with Points, Agent Target, Texture Anim and Update
Type in original source order. HUD-only output uses explicit untextured
`debug`/`debug_skinned` indexed shader families, static/rigged MSL entry points
and the same semantic depth bias. Generic UI draws explicitly reset depth bias.

Capture validates actual GL blend factors/equations, RGB-only color mask,
disabled culling, enabled polygon offset and exact factor/units, plus read/write
`LEQUAL` depth. Incomplete mirrored storage, non-triangle cardinality, invalid
offset/range/alignment, wrong color, non-finite transforms or bias, invalid
palette/weights/order and allocation failure all fail closed. Only
`RENDER_DEBUG_BATCH_SIZE` was removed from native-presentation ineligibility;
the other 12 visual render-debug modes and latent pathfinding guard remain
fail-closed.

## Physical validation

- Focused RelWithDebInfo world replay: 2,330 assertions, pass under Metal
  API/GPU/load-store/shader validation.
- Focused RelWithDebInfo UI replay: 521 assertions, pass under the same
  validation layers.
- Complete RelWithDebInfo, Debug and Release suites: 46,028 assertions in 52
  cases each, pass; warm elapsed times 1.21, 1.39 and 1.13 seconds.
- World controls prove exact hash color and alpha blending, destination-alpha
  retention, forward and reverse depth bias direction, reverse winding/no cull,
  palette-driven rigged movement, both order directions against Update Type,
  collision rejection, invalid storage/color/palette and no-owner identity.
- HUD controls prove BGRA byte-order-correct color, forward bias, reverse
  winding, rigged movement, invalid palette/non-finite bias rejection and no
  texture dependency.
- Warnings-as-errors RelWithDebInfo production integration compiled, linked,
  staged and signed. The non-manual/non-GL gate passed 484/484 in 35.40 seconds.
- Source-exact `AL_METAL_CONFIG=Release AL_METAL_JOBS=8
  tools/metal/build.sh build` passed at `27c0bec87c` after 1,094 cacheable tasks
  and staged Development build `26.4.0d68065`.
- Deep strict signing passed; bundle inspection found 349 ARM64 Mach-O slices
  and zero invalid slices.

SHA-256 provenance:

- executable: `ad71189d24bfb9e250eecf31485aad44ff0f11ca7e1100ca70e4039b196a82e6`;
- source and packaged MSL: `4331aad8f440c6d92473a5d998bf60121521868ba47186982b87524a2cfc0764`;
- compiled metallib: `4d2140cf789ba313ba6cd5aff1bda7f39ea12e46c661f88379b01c4fc99c4475`.

The main executable still links Metal and OpenGL. This checkpoint advances one
bounded native owner; strict no-GL, broad live parity, stability and qualifying
performance gates remain open.

## Persisted owner decisions

- Exact RLVa parity remains deferred non-release nice-to-have M-013. Any exact
  RLVa comparison must enable RLV in Preferences in both viewers, save, and
  restart both; only enough RLVa is mandatory to unblock Metal/no-GL cutover.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers. Off/on is invalid; both-off can isolate a residual but cannot prove
  shadow parity.

## Next executable action

Implement `RENDER_DEBUG_LIGHTS`. Bounded source inspection shows a compact
reachable owner: translucent-cyan face triangles, then a white spatial-extents
outline with depth testing disabled, then a yellow light-radius outline. Prove
those exact contracts before removing only that bit.
