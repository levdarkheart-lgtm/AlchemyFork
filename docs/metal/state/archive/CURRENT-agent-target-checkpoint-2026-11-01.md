# Native Agent Target checkpoint — 2026-11-01

**Observed:** physical Apple M1 Max acceptance host.  
**Branch:** `codex/apple-silicon-metal`  
**Renderer code checkpoint:** `0d8b5ccdda` (`feat(metal): replay agent target natively`)  
**Source-exact build:** Release Development `26.4.0d68053`

This archive preserves the bounded Agent Target implementation and validation
checkpoint. It does not claim complete advanced-output parity or a Metal-only
release.

## Implementation observation

Develop **Agent Target** now has a native Metal owner at the exact self-avatar-
only `renderAgentTarget()` boundary. The capture retains all four original
0.2 m crosshairs: agent position red, drawable position green, root world
position white and pelvis world position blue, each with alpha 0.8.

The native opaque-post scene accepts exactly 24 line vertices and one current
transform. Dedicated LDR/HDR pipelines encode after object-update blips and
before reflection-probe-volume visualization. Source-alpha blending affects RGB
only, preserving destination alpha. Forward and reverse-Z variants retain
depth reads and writes, matching inherited `GL_LEQUAL`/`GL_GEQUAL` behavior.

Capture and encoder validation are fail-closed. Duplicate or partial traversal,
missing storage, allocation failure, invalid cardinality, overflow, mismatched
inherited state or non-finite data makes native presentation ineligible. Empty
or absent Agent Target ownership is an exact no-op.

`RENDER_DEBUG_AGENT_TARGET` was removed from the native-presentation veto only
after the asymmetric evidence below passed. The explicit render-debug world-
output set therefore decreases from 19 to 18 modes. Those 18 modes and the
latent pathfinding guard remain fail-closed.

## Physical validation evidence

All Metal tests ran with Metal API, GPU, load/store-action and shader validation
enabled:

- focused RelWithDebInfo Agent Target/world test: 1,998 assertions in one case;
- complete RelWithDebInfo suite: 45,654 assertions in 52 cases;
- complete Debug suite: 45,654 assertions in 52 cases;
- complete Release suite: 45,654 assertions in 52 cases.

The focused tests prove missing-storage, wrong-cardinality and non-finite
rejection; all four exact alpha-0.8 blend equations; RGB-only destination-alpha
preservation; forward/reverse-Z depth pass, rejection and writes; covered-
surface near/behind behavior; and output/depth identity without an owner.

Warnings-as-errors RelWithDebInfo production integration compiled, linked,
staged and signed. The neutral non-manual/non-GL gate passed 484/484 in 35.57
seconds. Source-exact Release wrapper build `26.4.0d68053` completed at committed
code head `0d8b5ccdda`. Deep strict signing passed; `verify_app.sh` inspected 349
Mach-O slices with zero non-ARM64 slices, and the main executable reports arm64.

SHA-256 provenance:

- executable: `ac11e94f7bd088c26fc1df2d2f81ca37b004af471c5021922c8f89b3fff8879f`;
- source and packaged MSL: `a33ec7160965a1c6805ad5b2a2f69d95da8bff630cf2b05c346a57deb13e2033`;
- compiled metallib: `537685a0c488cded37fc64b7a532f57610771deeaf9f17d6ae9c1772787549b1`.

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

Source inspection selects Develop **Points**. Its single drawing function emits
one white point at each valid drawable face's local center with depth explicitly
disabled. Traversal spans region partitions and visible spatial bridges, so the
native owner must retain each traversal transform. Remove only
`RENDER_DEBUG_POINTS` after physical filtering, transform, color, size, disabled-
depth and empty/off proof; retain the other 17 modes fail-closed.
