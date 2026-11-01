# Native Composition debug-grid checkpoint — 2026-11-01

**Observed:** physical Apple M1 Max acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `cc1dcff766` (`feat(metal): replay composition debug grid natively`)
**Source-exact build:** Release Development `26.4.0d68057`

This archive preserves the bounded Develop Composition implementation and
validation checkpoint. It does not claim complete advanced-output parity or a
Metal-only release.

## Implementation observation

Develop **Composition** now has a native Metal owner at its sole current-region
draw. It retains the exact 261-by-261 grid and loop order: X/Y coordinates
0..255 are blue, either coordinate 256..260 is red, and each Z is the original
`getCompositionXY(x, y) * 5 + 50` result.

The CPU uploads only 68,121 live heights (272,484 bytes). The dedicated Metal
vertex stage derives X/Y and color by vertex ID, avoiding redundant streamed
positions and colors. Dedicated LDR/HDR RGB-only source-alpha pipelines draw
after Wind Vectors and before the far-plane composite with the inherited
forward/reverse read/write depth convention. Capture skips the HUD-only pass.

Capture and encoder validation are fail-closed. Publication requires the exact
cardinality plus the observed blend-enabled, RGB-only color-mask, one-pixel
point-size and read/write `GL_LEQUAL` state. Missing storage, allocation failure
or a non-finite height/transform makes native presentation ineligible. No region
or an off setting is an exact no-op.

`RENDER_DEBUG_COMPOSITION` was removed from the native-presentation veto only
after the asymmetric evidence below passed. The explicit render-debug world-
output set therefore decreases from 17 to 16 modes. Those 16 modes and the
latent pathfinding guard remain fail-closed.

## Physical validation evidence

All Metal tests ran with Metal API, GPU, load/store-action and shader validation
enabled:

- focused RelWithDebInfo Composition/world test: 2,092 assertions in one case;
- complete RelWithDebInfo suite: 45,748 assertions in 52 cases;
- complete Debug suite: 45,748 assertions in 52 cases;
- complete Release suite: 45,748 assertions in 52 cases.

The focused tests prove missing/wrong-cardinality storage and non-finite
height/transform rejection; exact blue interior and red border output; RGB-only
destination-alpha preservation; forward-Z occlusion and depth writes;
reverse-Z depth writes; and output/depth identity without an owner.

Warnings-as-errors RelWithDebInfo production integration compiled, linked,
staged and signed. The neutral non-manual/non-GL gate passed 484/484 in 35.65
seconds. Source-exact Release wrapper build `26.4.0d68057` completed at committed
code head `cc1dcff766`. Deep strict signing passed; `verify_app.sh` inspected 349
Mach-O slices with zero non-ARM64 slices, and the main executable reports arm64.

SHA-256 provenance:

- executable: `db5f7ab0f748a1a9017b843858e169b6ac8c076b07ddc7ad675dcab22bec6c56`;
- source and packaged MSL: `d05669c80b490283e9376fb797a48715bb079bc1928022884193130149643a21`;
- compiled metallib: `994eaa49f2c573361c5951c443165ce63c6919b44c032fb6d4e027358dbbcd1b`.

The main executable still links OpenGL during migration. Strict no-GL remains
an open release gate.

## Durable comparison prerequisites

- Any exact RLVa comparison must enable RLV in Preferences in both viewers,
  save, and restart both viewers. Exact RLVa parity is deferred non-release
  nice-to-have M-013; minimum cutover behavior remains required.
- Any shadow candidate/reference comparison must enable shadows in Graphics
  Preferences in both viewers before capture. An off/on pair is invalid, while
  a both-off control can isolate a residual but cannot prove shadow parity.

## Next bounded owner

Source inspection selects Develop **Reflection Probes** neighbor links. Its
sole active renderer emits orange one-pixel line groups unless both endpoints
own viewer objects, then yellow line groups when both do. Only relevant probes
draw; the older node-volume block is compiled out. Remove only
`RENDER_DEBUG_REFLECTION_PROBES` after physical filtering, classification,
order, transform/state and empty/off proof; retain the other 15 modes fail-
closed.
