# Native Update Type checkpoint — 2026-11-02

**Host:** physical Apple M1 Max, ARM64, macOS 26.5.2, Xcode 26.6  
**Branch:** `codex/apple-silicon-metal`  
**Code commit:** `0ac23c10b1` (`feat(metal): replay update type debug overlay`)

## Observed owner contract

Develop **Update Type** skips drawables with no viewer object or an unknown last
update class. Every valid face is drawn through the position-only, non-rigged
`gDebugProgram` under one fixed half-alpha color:

- full: green `(0,1,0,0.5)`;
- terse-improved: cyan `(0,1,1,0.5)`;
- compressed and cached: red `(1,0,0,0.5)`;
- compressed and not cached: yellow `(1,1,0,0.5)`;
- full-cached: blue `(0,0,1,0.5)`.

The native bridge retains each face's existing Metal position/index buffers,
byte range, index type, current modelview-projection, color and traversal order.
It deliberately has no texture or skin-palette input because the source does
not bind a rigged variant here. Update Type, Points, Agent Target and Texture
Anim are merged in original source order. HUD-only output uses the established
uniform-color capture transaction while GL remains unchanged during migration.

Capture requires complete mirrored storage, triangle cardinality, aligned and
in-range offsets, one of the exact five colors, enabled blend, disabled culling,
RGB-only writes and inherited forward/reverse `LEQUAL` read/write depth.
Non-finite transform, invalid color/storage/order and allocation failure fail
closed. Unknown/off/no-face input is an exact no-op. Only
`RENDER_DEBUG_UPDATE_TYPE` was removed from presentation ineligibility; the
other 13 visual render-debug modes and latent pathfinding guard remain
fail-closed.

## Physical validation

- Focused RelWithDebInfo world replay under Metal API/GPU/load-store/shader
  validation: 2,293 assertions, pass.
- Complete RelWithDebInfo, Debug and Release suites under the same validators:
  45,951 assertions in 52 cases each, pass; elapsed times 5.94, 6.18 and 5.94
  seconds.
- Tests prove all five colors, RGB-only half-alpha blending with destination
  alpha retained, no-cull reverse winding, forward occlusion and depth writes,
  reverse-Z depth writes, both order directions against Texture Anim,
  duplicate-order rejection, invalid storage/color/transform and no-owner
  identity.
- Warnings-as-errors RelWithDebInfo production integration compiled, linked,
  staged and signed. The non-manual/non-GL gate passed 484/484 in 35.57 seconds.
- Source-exact warnings-as-errors Release Development `26.4.0d68063` at
  `0ac23c10b1` linked, staged and signed. Deep strict signing passed; bundle
  inspection found 349 ARM64 Mach-O slices and zero invalid slices.

SHA-256 provenance:

- executable: `2cbd61d22685d650f66861d9e2cdefc22d30156bdf2caf7345454f8631525ab6`;
- source and packaged MSL: `fc55ef6f5161d51eb6ce8b8ee8bfb6a9316c9752f5262592b8254fc3b06701fa`;
- compiled metallib: `4e9ab857f3a312a7ddf8b5fd13982cd4c4037b6525e8631e331edf323a14d7e1`.

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

Implement `RENDER_DEBUG_BATCH_SIZE`. It is the remaining direct draw-info
triangle owner, but source parity additionally requires `getDebugColor()`,
polygon offset and rigid/skinned variants with the current matrix palette.
Prove those contracts before removing only that bit.
