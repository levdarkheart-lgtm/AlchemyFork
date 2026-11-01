# Metal-port execution archive — native Agent Target checkpoint

**Date:** 2026-11-01  
**Branch:** `codex/apple-silicon-metal`  
**Code commit:** `0d8b5ccdda` (`feat(metal): replay agent target natively`)

## Implemented boundary

- Added bounded capture around the four original self-avatar-only draws at
  `renderAgentTarget()`.
- Retained the 0.2 m red agent, green drawable, white root and blue pelvis
  crosshairs, each at alpha 0.8, as exactly 24 line vertices and one transform.
- Added dedicated LDR/HDR RGB-only source-alpha pipelines in the native opaque-
  post pass after update blips and before reflection-probe-volume visualization.
- Preserved forward/reverse-Z depth reads and writes.
- Rejected duplicate/partial traversal, missing storage, invalid cardinality,
  overflow, non-finite data, allocation failure and inherited-state mismatch;
  retained empty/no-owner identity.
- Removed only `RENDER_DEBUG_AGENT_TARGET` from the visual-output eligibility
  mask. Eighteen other visual modes and the latent pathfinding guard remain
  fail-closed.

## Validation

- Focused RelWithDebInfo physical Agent Target/world case under Metal API/GPU/
  load-store/shader validation: 1,998 assertions, pass.
- Full RelWithDebInfo, Debug and Release physical suites under the same layers:
  45,654 assertions in 52 cases each, pass.
- Warnings-as-errors RelWithDebInfo `alchemy-bin`: compile, link, stage and sign
  pass.
- Neutral gate: 484/484 pass in 35.57 seconds.
- Source-exact Release Development `26.4.0d68053` at `0d8b5ccdda`: pass.
- Deep strict signing: pass. Bundle inspection: 349 ARM64 Mach-O slices, zero
  invalid slices; main executable arm64.

Provenance hashes:

- executable `ac11e94f7bd088c26fc1df2d2f81ca37b004af471c5021922c8f89b3fff8879f`;
- source/packaged MSL `a33ec7160965a1c6805ad5b2a2f69d95da8bff630cf2b05c346a57deb13e2033`;
- metallib `537685a0c488cded37fc64b7a532f57610771deeaf9f17d6ae9c1772787549b1`.

## Persisted decisions

- RLVa strict parity remains deferred nice-to-have M-013. Exact comparison
  requires enabling RLV in both viewers, saving and restarting both. Minimum
  behavior still must not block Metal/no-GL cutover.
- Shadow pairs require shadows enabled in Graphics Preferences in both viewers.
  Both-off is only an isolation control, not parity evidence.
- Build/test success does not close broader live visual parity, strict no-GL,
  stability or performance gates.

## Next executable action

Implement Develop **Points** as one native white point per valid drawable face
center across region partitions and visible spatial bridges. Prove filtering,
per-traversal transforms, point size, disabled-depth behavior and empty/off
controls before removing only `RENDER_DEBUG_POINTS`; keep the other 17 visual
modes and pathfinding fail-closed.
