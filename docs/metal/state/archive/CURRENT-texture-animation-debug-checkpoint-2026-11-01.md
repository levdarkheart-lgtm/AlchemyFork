# Native Texture Anim checkpoint — 2026-11-01

**Host:** physical Apple M1 Max, ARM64, macOS 26.5.2, Xcode 26.6  
**Branch:** `codex/apple-silicon-metal`  
**Code commit:** `5001b10d20` (`feat(metal): replay texture animation debug overlay`)

## Observed owner contract

Develop **Texture Anim** traverses visible draw infos and selects only entries
whose texture matrix is non-null. Despite the setting name and predicate, the
active GL owner leaves `gDebugProgram` bound: it samples neither a texture nor
the texture matrix and renders indexed triangles with the uniform color
`(1,1,0,0.5)`. It does not switch to a rigged shader when `LLDrawInfo::mAvatar`
is present. The exact source contract is therefore position-only and
non-rigged; the earlier unverified assumption that this path needs a rigged
variant was rejected by source inspection.

The native bridge retains the source Metal position/index buffers, byte ranges,
index type, current modelview-projection and traversal order. It requires the
observed enabled blend, disabled culling, RGB-only color mask and inherited
forward/reverse `LEQUAL` read/write depth state. Texture Anim, Points and Agent
Target are encoded in source order. The HUD transaction uses the established
uniform-color capture while the original GL draw remains during migration.

Invalid or absent buffer storage, non-triangle cardinality, misaligned or
out-of-range offsets, non-finite transforms, duplicate/non-monotonic order and
allocation failure fail closed. Empty/off input is an exact no-op. Only
`RENDER_DEBUG_TEXTURE_ANIM` was removed from native-presentation ineligibility;
the other 14 visual render-debug modes and latent pathfinding guard remain
fail-closed.

## Physical validation

- Focused RelWithDebInfo world replay under Metal API/GPU/load-store/shader
  validation: 2,214 assertions, pass.
- Complete RelWithDebInfo, Debug and Release suites under the same validators:
  45,872 assertions in 52 cases each, pass; elapsed times 5.99, 8.05 and 7.84
  seconds.
- Tests prove exact half-alpha yellow RGB over a controlled destination while
  retaining destination alpha, no-cull reverse winding, forward occlusion and
  depth writes, reverse-Z foreground/depth writes, both ordering directions
  against Points, duplicate-order rejection, invalid input and no-owner
  identity.
- The offline metallib inventory names both new production entry points.
- Warnings-as-errors RelWithDebInfo production integration compiled, linked,
  staged and signed. The non-manual/non-GL gate passed 484/484 in 35.43 seconds.
- Source-exact warnings-as-errors Release Development `26.4.0d68061` at
  `5001b10d20` linked, staged and signed. Deep strict signing passed; bundle
  inspection found 349 ARM64 Mach-O slices and zero invalid slices.

SHA-256 provenance:

- executable: `eac2133bf873934f01c63753864d477804b3f0b838ef6e2960ab31c260e68a1c`;
- source and packaged MSL: `fc55ef6f5161d51eb6ce8b8ee8bfb6a9316c9752f5262592b8254fc3b06701fa`;
- compiled metallib: `4e9ab857f3a312a7ddf8b5fd13982cd4c4037b6525e8631e331edf323a14d7e1`.

The main executable still links Metal and OpenGL. This checkpoint advances one
bounded native authority owner; it does not satisfy strict no-GL, broad live
parity, stability or performance gates.

## Persisted owner decisions

- Exact RLVa parity remains deferred non-release nice-to-have M-013. Any exact
  RLVa comparison must enable RLV in Preferences in both viewers, save, and
  restart both; only enough RLVa is mandatory to unblock Metal/no-GL cutover.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers. Off/on is invalid; both-off can isolate a residual but cannot prove
  shadow parity.

## Next executable action

Implement `RENDER_DEBUG_UPDATE_TYPE`. It is a bounded source-backed owner:
drawables with a known last update type replay each face's indexed triangles
under one of five fixed half-alpha colors using the inherited position-only
debug shader and state. Prove every classification, range/transform/order,
forward/reverse depth, unknown/off identity and invalid input before removing
only that bit.
