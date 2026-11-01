# Reflection-probe neighbor selection execution — 2026-11-01

**Renderer checkpoint:** `d3cff63d0d`

The next M-008 source audit time-boxed the active alpha/PBR replay seam and
found no new divergence that justified a production edit. It then converted a
missing probe-transition claim into an asymmetric physical regression.

The regression holds geometry, material, lighting, radiance cube and probe
constants fixed. Without a neighbor edge, a red automatic sphere and red
default probe own the result. Adding one edge reaches a blue containing manual
box that is otherwise beyond the sorted walk; the native selector must then
discard its accumulated automatic family exactly as GLSL does. The accepted
result raises blue and lowers red relative to the control.

Warnings-as-errors Debug and RelWithDebInfo test targets build. The focused
RelWithDebInfo production-world case passes 1,329 assertions and the complete
physical suites pass 44,882 assertions/50 cases in both configurations with
Metal API/GPU validation. The post-change neutral gate passes 484/484 in
239.08 seconds. No GL traversal, fail-closed guard or production authority was
changed.

M-008 remains `doing`. The next executable boundary is the first demonstrated
gap in remaining probe publication, post or another advanced-view owner. A live
comparison is admissible only with an attributable fixture, pinned source and
identical graphics/camera/environment/window/Snapshot contracts.
