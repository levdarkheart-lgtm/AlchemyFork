# Native reflection-probe neighbor-link checkpoint

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Code commit:** `b76c52576d` (`feat(metal): replay reflection probe neighbor links`)

## Implemented boundary

The active Develop **Reflection Probes** neighbor-link output is captured at
`LLReflectionMapManager::renderDebug()` and replayed in the native opaque-post
scene. Only relevant probes contribute. Each source neighbor is retained once
and in source order: orange when either endpoint lacks a viewer object and
yellow when both endpoints own viewer objects. The historical node-volume block
is compiled out and is not part of the active output contract.

The bridge pre-counts links with overflow protection and retains two compact
position/color vertices per link plus the current transform. Capture requires
the inherited enabled blend, RGB-only color mask, one-pixel line width and
forward/reverse read/write depth state. The native draw follows spatial
Points/Agent Target traversal and precedes the reflection-probe-volume display,
matching source ownership order.

Count overflow, missing or odd storage, allocation failure, non-finite data or
transform, invalid homogeneous position and colors outside exact orange/yellow
fail closed. Empty/off input is an exact no-op. Only
`RENDER_DEBUG_REFLECTION_PROBES` was removed from the native-authority veto;
the other 15 visual modes and latent pathfinding guard remain fail-closed.

## Physical validation

- Focused RelWithDebInfo world case under Metal API/GPU/load-store/shader
  validation: 2,162 assertions, pass.
- Full RelWithDebInfo, Debug and Release suites under the same validators:
  45,818 assertions in 52 cases each, pass; measured elapsed times 5.95, 6.22
  and 5.90 seconds.
- Tests cover exact orange/yellow output, RGB-only alpha preservation, forward
  occlusion and depth writes, reverse-Z depth writes, invalid storage/data
  rejection and no-owner identity.
- Warnings-as-errors RelWithDebInfo production integration: compile, link,
  stage and sign pass.
- Neutral non-manual/non-GL gate: 484/484 pass in 35.39 seconds.
- Source-exact Release Development `26.4.0d68059` at `b76c52576d`: pass.
- Deep strict signing: pass. Bundle inspection: 349 ARM64 Mach-O slices, zero
  invalid slices; main executable arm64.

SHA-256 provenance:

- executable: `8460b7356784d39d0c1163a4b157e9f9bc25faf85a53c31cc84aeba99dec53e1`;
- source and packaged MSL: `d05669c80b490283e9376fb797a48715bb079bc1928022884193130149643a21`;
- compiled metallib: `994eaa49f2c573361c5951c443165ce63c6919b44c032fb6d4e027358dbbcd1b`.

The executable still links OpenGL during migration. Strict no-GL, controlled
live parity, stability and qualifying performance evidence remain open.

## Persisted owner decisions

- Exact RLVa parity is deferred nice-to-have M-013. Implement only enough RLVa
  to avoid blocking Metal authority/no-GL cutover. Any exact comparison must
  enable RLV in Preferences in both viewers, save and restart both viewers.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers. Off/on is invalid; both-off is an isolation control and cannot prove
  shadow parity.

## Next executable action

Implement `RENDER_DEBUG_TEXTURE_ANIM`. Its sole active output is a flat
translucent-yellow replay of visible draw-info triangles with a non-null texture
matrix. Prove static/rigged geometry, exact color/alpha semantics, traversal and
transform, inherited state, depth interaction and empty/off controls before
removing only that bit; retain the other 14 visual modes and pathfinding guard.
