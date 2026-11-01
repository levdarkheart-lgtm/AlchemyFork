# Reflection-probe neighbor selection checkpoint — 2026-11-01

This append-only record preserves the bounded M-008 evidence added after
renderer checkpoint `d3cff63d0d`. It changes validation coverage, not production
renderer authority.

## Source audit and missing evidence

The active alpha/PBR capture and replay seam was compared against its GLSL
callers for ordering, depth/color masks, custom RGB blend factors, particle
culling and sampling, mirror/water clipping, glTF transforms and factors,
raw normal/ORM sampling and directional-shadow ownership. No new source
divergence survived that comparison, so no speculative production change was
made.

The adjacent reflection-probe audit found a real evidence gap. Existing GPU
coverage selected the default and a manual sphere, but did not exercise the
production `refNeighbor` traversal or the rule that a containing manual box
shuts out automatic probes already accumulated earlier in the walk. That rule
is shared by `class3/deferred/reflectionProbeF.glsl` and the native forward
probe selector and affects PBR, legacy alpha and water transitions.

## Asymmetric physical regression

The new production-world regression uses a two-layer cube array: red for the
automatic/default family and blue for a manual box. Probe index 1 is an
automatic sphere admitted by the depth-sorted walk. Probe index 2 is a
containing manual box deliberately placed beyond the sorted stopping depth, so
it can be reached only through index 1's neighbor edge.

With that edge absent, the rendered PBR-alpha pixel remains dominated by the
red automatic/default family. Adding only the edge reaches the blue box,
increases the blue result and reduces red. This proves both neighbor traversal
and suppression of the already accumulated automatic/default taps; a mere
linear scan or additive manual tap cannot satisfy both comparisons.

The focused RelWithDebInfo case passed 1,329 assertions. Complete Debug and
RelWithDebInfo physical suites each passed 44,882 assertions across 50 cases
with Metal API and GPU validation enabled. The first attempted invocation used
the obsolete numeric `MTL_DEBUG_LAYER_ERROR_MODE=0`; macOS 26 rejected that
environment value before renderer execution. It is not counted. The accepted
runs use `MTL_DEBUG_LAYER_ERROR_MODE=nslog`.

The post-change neutral command, `ctest --test-dir
build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE 'manual|gl'
--output-on-failure`, passed 484/484 in 239.08 seconds.

This closes one physical probe-selection/transition contract. It does not prove
broad live reflection-probe placement, publication, transition or mirror
parity. Those gates, full parity/stability, strict no-GL authority removal and
qualifying M1 Max performance evidence remain open.
