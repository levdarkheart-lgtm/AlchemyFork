# Render-debug eligibility audit — 2026-11-01

## Implementation

Commit `9e28d64932` adds the explicit
`LLPipeline::RENDER_DEBUG_GL_WORLD_OUTPUT` aggregate and uses it for Metal world
overlay eligibility instead of treating every nonzero render-debug bit as world
output.

The source audit followed every `LLRenderDebugMask` consumer. It found 20
world-pixel/geometry modes, nine post-world LLHUD/debug-text modes and four
unused definitions. Only the latter two groups were removed from the world
fallback. Physics Shapes remains included even though it is emitted later in
final composition; Shadow Frusta remains included because it also changes
shadow-render traversal; Impostors remains included because its outline is
drawn during avatar geometry; Reflection Probes remains included because its
manager emits probe shapes and links.

This is an ownership correction, not a visual-parity waiver. The explicit mask
is deliberately conservative and future world-pixel consumers must remain in
it until a native owner has asymmetric physical evidence.

## Evidence

Warnings-as-errors RelWithDebInfo production integration passes. The neutral
gate passes 484/484 in 35.39 seconds. Source-exact Release Development
`26.4.0d68049` at committed head `9e28d64932` links, stages and signs. Deep
strict signing and 349/349 ARM64 slice inspection pass; immutable hashes are in
`docs/metal/state/archive/CURRENT-render-debug-eligibility-checkpoint-2026-11-01.md`.

## Next owner

Wind Vectors is selected next because its production output is fixed and
bounded: one 16x16 simulator lattice, one green point and one red line segment
per cell. Capture/replay must preserve the live region origin, agent-relative
altitude, wind scale, camera transform, depth/order and one-pixel point size.
All other visual-debug bits remain fail-closed.

Standing ledger rules remain unchanged: shadow pairs require shadows enabled in
both viewers; exact RLVa pairs require enable/save/restart in both viewers; exact
RLVa parity is deferred M-013 nice-to-have.
