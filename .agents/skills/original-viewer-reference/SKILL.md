---
name: original-viewer-reference
description: Trace the corresponding original Alchemy/OpenGL behavior before diagnosing or implementing Metal rendering fixes. Use for renderer, shader, presentation, resource-lifecycle, UI-composition, graphics-setting, visual-regression and native GPU crash work; not for a build or launch that makes no code change.
---

# Original Viewer Reference

Treat the original viewer as the behavioral specification, not as a backend to
retain. Use this skill together with `metal-port` for implementation and
`gpu-validation` or `alchemy-viewer-visual-qa` when runtime evidence is needed.

## Mandatory pre-fix gate

Record the reported regression or discovered defect in
`docs/metal/POST_PORT_DEFECTS.md`, or update its existing PPD ID, before a fix.
Keep source proof separate from attribution to an owner-reported incident.
Preserve failed retests and reverts; close only under the register's evidence
and owner-confirmation rules.

Before editing a fix, identify and inspect the original path that owns the same
behavior. Prefer the exact Alchemy fork revision immediately before the relevant
GL code was removed; find it with source history (`git log -S`, `git log -G`,
`git blame`, and `git show <revision>:<path>`). Use `upstream/develop` when it is
the applicable source of truth. For a visual or settings regression, also
observe the original executable under the same account-independent location,
window, graphics preset and environmental state when practical.

Trace enough of the call graph to understand:

- who owns and updates the state or resource;
- ordering, lifetime, synchronization and reset behavior;
- completeness, fallback, warm-up and failure semantics;
- shader math, color space, alpha, depth and texture expectations;
- which visible result or failure the original contract produces.

Record a concise mapping before implementation: the reference revision/path,
the original invariant, the Metal ownership boundary, API-semantic differences,
and the behavior that must remain unchanged. Do not infer the contract only from
the current partially ported code.

If no original equivalent exists, inspect the nearest original producer and
consumer boundaries and explicitly record that the native mechanism is new.
That evidence satisfies the gate; guessing does not.

## Implement the behavior, not OpenGL

Preserve the original viewer's feature and lifecycle semantics with direct
Metal resources and synchronization. Do not copy API mechanics that are not
valid in Metal, and never reintroduce GL, a translation layer, or an in-product
fallback. Reject a workaround that merely hides the symptom by changing probe
selection, pass ordering, presentation ownership, quality behavior or failure
policy. If a deliberate semantic change is necessary, state it explicitly and
tie it to a product requirement or user decision.

Use observed evidence to distinguish the earliest incorrect producer from a
downstream symptom. Add narrowly scoped diagnostics when the original and Metal
paths diverge but the failing invariant is not yet observable.

## Validate and hand off

Build and test the native change, then compare the same behavior against the
original reference. For visual work, use the exact-build and fail-closed gates
from `alchemy-viewer-visual-qa`; for GPU/resource behavior, use
`gpu-validation`. Record the source revisions, commands, settings, observations
and any intentional remaining differences in the active ledger and
`docs/metal/state/CURRENT.md` at a substantial handoff.
