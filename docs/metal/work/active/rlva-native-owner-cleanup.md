# RLVa native post ownership and displaced GL removal

**Observed source:** `bccbb7be48000fab91b43688bc585f2f9ab474cf`, 2026-11-11.
**State:** implementation integrated with the active port; live RLVa parity remains
unverified. The owner prohibits launching either viewer or screenshot comparisons.

## Original contract inspected before changes

Reference `dd9aee0de1^:indra/newview/rlveffects.cpp` owns sphere policy in
`RlvSphereEffect::setShaderUniforms`: sample current tweens, clamp maximum distance
to minimum, transform avatar/camera origin through the current view matrix, and
export distance-extension bits. `run` draws one pass for blend, chromatic
aberration and pixelation; blur draws horizontal then vertical with separate
source and destination targets. The source sampler is point/mirrored-repeat and
output alpha is zero. The corresponding `deferred/rlvF.glsl` supplies the five
mode algorithms. `LLVfxManager` orders by effect code, descending priority and
pointer tie-break, and executes enabled effects only.

The native replacement already exists in `pipeline.cpp` and
`ALMetalWorldGBufferRenderer::renderRlvSphere`, with shader implementation in
`almetal_builtin.metal`. It passes each completed native target into the next
enabled effect in the manager's existing order; the native owner allocates
explicit targets and encodes horizontal/vertical blur passes. RLVa retains CPU
policy, tweening and current-view parameter export in `rlveffects.cpp`.

## Changes

- Remove the displaced `RlvSphereEffect` shader draw, uniform-binding and
  framebuffer ping/pong helpers. The native world/display post transaction is
  the sole sphere renderer.
- Remove generic `LLVisualEffectParams`, `LLShaderEffectParams`, effect-type
  dispatch and virtual shader execution from `llvisualeffect.*`. Their only
  remaining shader consumer was the displaced sphere implementation. Overlay
  execution remains typed and ordered at its UI composition boundary.
- Remove the frame-number-only sphere parameter cache introduced by
  `dd9aee0de1`. That cache stored a view-space origin and command state across
  distinct executions in the same frame; the original sampled those at each
  execution. The sole native consumer now obtains current view/state directly.
  No command parsing, restriction policy, effect order, shader math or tween
  interpolation is changed.

The integrator owns matching removal of the old pipeline sphere invocation and
`gRlvSphereProgram` loader/readiness dependencies, plus the direct typed overlay
call in `llviewerdisplay.cpp`.

## Validation boundary and next checks

Child source checks: `git diff --check` exits 0. No child build or viewer launch.
The existing real-GPU fixture is
`almetal: RLVa vision sphere owns every post mode` in
`indra/almetal/tests/almetal_gpu_test.mm`; it exercises all five modes, depth
reconstruction, output alpha and invalid native scene rejection. Root must run
that fixture and the full application build after integrating caller changes.
It does not establish script command/reply, touch-mask, tween timing, multi-effect
visual parity, restart-qualified RLVa, or snapshot behavior end to end.

`ALSnapshotCutout::blockedByRlv` already blocks sphere/overlay cutout bypass in
the inspected source. Overlay tint, alpha, texture sampling and CPU touch masks
remain in their existing owners. Camera, chat, attachment and other restrictions
remain CPU/UI policy; this cleanup makes no broad RLVa verification claim.
