# Attached-light coverage follow-up — 2026-11-11

The earlier scoped port/release request is closed. This is a newly reported
visual issue against released source9f42b8660639e8827f8449a64cf0248fef6d90c0,
canonical branch codex/apple-silicon-metal at a25a1acf36931fed8a9c194f197c93e413fa970c.
No viewer launch, operation or agent screenshots. Owner supplies observations.

## Owner observations and limits

Owner reports a straight lighting boundary moving across the avatar as idle
animation or camera height changes. Neutral restores general exposure but does
not remove the band. Shadows=None and SSAO disabled do not clear it. Name-tag
correlation was withdrawn; temporary disappearance was not a verified fix.
Owner then isolates an attached beard/facelight: detached removes the band;
attached restores it. Supplied before/after images show a sharp head/hair cutoff.
These are owner observations, not a matched original-viewer parity measurement.
The actual attachment's light type, parameters and emitted native bounds remain
unobserved. No blanket conclusion about the attachment or all local lighting.

## Original-to-native mapping before implementation

Reference1eb44bbd1bdc76d0100c75699bad1fb337e8cb45:
class3/deferred/pointLightV.glsl38–45 rasterizes the projected bounding cube in
original GL row order; pipeline11981–12006 owns camera-outside volume submission.
Native world vertices explicitly negate clip Y to preserve that same lower-row
texture layout (builtin.metal4654–4667). Fullscreen local lighting11745–11786
maps raw row UV to the matching eye-space coordinate. Viewport/scissor wrappers
in almetalpipeline.mm1431–1451 pass native rectangles through without conversion.

Current setMetalLightScreenRect in pipeline.cpp1600–1667 instead computes first/
last Y using0.5−Y/2, vertically reflecting the rectangle relative to G-buffer
storage and its consumers. Both bounded point lights17369–17375 and all projector
bounds17631–17634 use it. Camera-inside point lights and eye-plane-crossing
fallbacks use a full target rectangle and are unaffected by this specific defect.
The native fragment still calculates sphere/range/falloff; the rectangle must
conservatively cover all its contributing pixels. Correct row bounds are
(ymin/2+0.5)H through(ymax/2+0.5)H with existing floor/ceil/padding/clamps retained.

This is a concrete source mismatch consistent with the reported moving edge;
the attachment's use of the affected narrowed rectangle is not yet runtime-proved.
Existing point/projector GPU tests manually author centered symmetric scissors,
which cannot distinguish the two Y mappings. Shader attenuation/facing/cone math
has no newly found mismatch in the bounded original comparison. An unrelated
legacy avatar-alpha water-clip exception found during earlier investigation is
not attributed to this rigged mesh clothing or mixed into this fix.

## Current bounded work

Root owns canonical pipeline/CMake/docs and integration. GL-tail owns only a new
private actual-helper source and focused test in a separate worktree. Preserve
the exact old helper as the baseline; change only two Y formulas plus comment.
Compare real native point/projector accumulation using actual helper bounds
against identical full-screen controls, with asymmetric positive/negative Y,
perspective, both depth conventions, camera-height changes and eye-plane fallback.
Require positive lit controls and exact full-image equality; no lighting/shader
or tolerance change. GPU/API validation is offscreen only and serialized.
Next: collect actual baseline failures/fixed results, independently review,
integrate and build/check the corrected source without touching the running app.
Owner confirmation of the attachment remains a separate runtime boundary.
