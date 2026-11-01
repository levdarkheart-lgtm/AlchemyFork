# Water/default-probe parallax execution — 2026-11-01

**Work item:** M-008, still `doing`.  
**Integrated commit:** `fd0fa7cea56e3a9ba933f1405a6be68e9099374e`.  
**Touched owners:** `indra/almetal/shaders/almetal_builtin.metal` and
`indra/almetal/tests/almetal_gpu_test.mm`.

## Executed change

1. Traced default-probe sampling from the active GLSL water branch into the
   shared native Metal radiance helper.
2. Demonstrated that GLSL uses an unshifted reflected ray for water/default
   probe zero while Metal applied the generic sphere-parallax correction.
3. Added explicit water context and limited the special rule to exactly that
   caller/probe combination; retained all other parallax behavior.
4. Added a six-distinct-face physical negative control that moves only the
   default probe centre across the 4 km automatic sphere. It failed before the
   fix with delta 1.596944809 and passes after the fix below 0.002.
5. Kept GL traversal and normal presentation authority unchanged.

## Executed checks

- Pre-fix focused physical control: 1,348/1,349, expected new failure.
- Post-fix focused physical control with Metal API/GPU validation:
  1,349/1,349, pass.
- Complete RelWithDebInfo physical validation: 44,902 assertions/50 cases,
  pass.
- Complete Debug physical validation: 44,902 assertions/50 cases, pass.
- Architecture-neutral gate: 484/484 in 259.22 seconds, pass.
- Source-exact warnings-as-errors build 68026: deep strict signing and 349/349
  ARM64 pass; executable SHA-256
  `d33603803c5d45434d06e0332e14980dc095505b060624f09143a41f434e1d4e`.
- Exact-path build-68026 Computer Use: preflight, semantic login,
  `STATE_STARTED`, coherent world/UI observation, semantic logout, zero final
  viewer/helper process census and launch-environment cleanup, pass.

## Priority decision

This advances the product because it closes a demonstrated GLSL/Metal
correctness gap with a regression that the prior implementation fails. It does
not close live water parity or M-008. Attempt the controlled water-visible pair
only when exact provenance, all 416 effective settings, camera,
environment/time, warm state and window/snapshot contracts match; otherwise
fail closed without metrics. Continue next through probes/mirrors, the isolated
projector/cascade shadow fixture, transparency/material replay, non-default post
and advanced output. Keep strict no-GL, full parity, stability and performance
gates open.
