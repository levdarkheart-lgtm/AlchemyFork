# Busy-region turning and world freshness — 2026-11-12

Owner reports that LEFT/RIGHT ARROW turning sometimes moves nametags while
the whole background and own avatar briefly freeze, then catch up. This follows
the owner-passed cb840/68841 beard-light fix and is a separate correctness issue.
No viewer launch, camera input, screenshot or new profiling is authorized here.

## Original mapping and diagnosis before implementation

Original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` display draws the current
world followed by current-camera HUD/nametags before the same swap. Original
`LLVOAvatar::renderImpostor` returns zero for that avatar when its target is not
complete; it does not invalidate the rest of the world. Input/agent-camera and
nametag source still match the original; no ordinary camera-restore defect was
found in the bounded source audit.

Native `beginMetalWorldUICompositionCapture` deliberately draws current UI over
a retained pre-UI world when the current scene transaction is incomplete. This
can produce the reported split. The existing session log repeatedly records
that selection but does not preserve the source frame or first rejection across
phase resets. Log repetition counts do not measure visible world age.

One concrete failure-scope regression exists: a missing compatible native
impostor quartet clears every accumulated world batch and invalidates the entire
scene, even though the original skips only the unavailable avatar. The normal
bake scheduler already retries unsuccessful publication, and transactional native
publication preserves an older compatible quartet after replacement failure.
Correct only that not-ready branch; preserve genuine state/allocation failures.
This source defect is not yet tied to the owner's incident frames.

Add rate-limited CPU-only retained-frame age and first-rejection diagnostics.
They must survive phase completeness resets, never change draw admission, and
exclude avatar identities, locations and camera matrices. Existing pre-UI
color/depth leases, current-camera UI behavior and exact snapshots remain owned
by their existing transactions.

## Execution and evidence

Integrated the actual method through `pipeline_impostor_readiness_native.inl`;
only the missing-quartet branch changes draw admission. Existing batch, palette,
texture-lease and error-state behavior elsewhere remains intact. Registered its
actual-body fixture in LLRender tests and the private include in the viewer.

Child old-body baseline: 583 assertions/3 cases, 43 intended failures. Fixed:
695 assertions/3 cases pass with Metal API/GPU validation. Real native cache,
texture publication and G-buffer readback exercise SDR/HDR, both normal formats,
both depth conventions, changed camera inputs, missing-to-ready recovery and
failed replacement retaining the prior compatible lease. The fixture uses named
avatar identity/camera/pass/queue inputs; it does not execute the avatar scheduler,
full application presentation or an actual crowd. Independent source/cohort and
test peers pass. Source projection and exact commands are in local receipts.

The diagnostic records frame age and first invalidation once per second when
an older world is selected. Source line IDs can refer to pipeline.cpp or a private
native include. Early availability/render failures can have no markIncomplete
line; frame/product-presence fields retain that uncertainty explicitly.

Evidence: `artifacts/metal/busy-turn-world-freshness-20261112/` and
`/private/tmp/alchemy-native-impostor-readiness-validation-20261112/`.
Canonical build/checks next. Root preserves concurrent uncommitted profiling
notes and research files. No causal live reproduction or owner verification yet.
