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

## Actual-helper proof and integration

Childf907d5ec4e003bf5eaa4f1e219794ff79cc6dde1 integrated as
b7bc118d684253db1841738032e85fff0825803d. Root includes the private helper at the
same anonymous-namespace location and lists/registers both new files in existing
CMake targets. No shader, color, falloff or light-selection changes. Exact old
function/full notice retained in the test baseline; fixed body differs only in
two Y formulas and two explanatory comment lines. Root independently checks this
projection before replacing the old body. X/padding/clamps/fallbacks unchanged.

Same actual-helper fixture: baseline1204assertions/2cases has32 targeted failures;
fixed1204/2passes under observed API/GPU validation. Both real accumulation APIs,
HDR/SDR, normal/reverse Z, positive/negative Y, camera heights0/.3 and eye-plane
fallback execute. Positive controls have24/26lit pixels; wrong bounds drop72/78
RGB half-storage channels. Fixed output is exactly equal to the identical
unbounded control over the whole image, without a tolerance change. This uses a
named flat legacy receiver and authored white-cookie projector; it is not full
avatar/attachment/LLViewerCamera execution or a live light-parameter observation.
All23frozen supporting inputs match the built9f42 renderer/metallib cohort.

Independent peer found no blockers and pinned committed hashes. Evidence:
/private/tmp/alchemy-native-light-screen-rect-validation-20261111/validation-receipt.json
and /private/tmp/alchemy-native-light-screen-rect-peer-rlva-20261111/final-peer-review.json.
Root doctor-host.json exits0; sandbox-only sysctl denial is separately preserved.
GPU/test jobs terminal and removed; root owns next combined build/checks.

Owner now explicitly requests launch of the integrated viewer for their test.
Build/check first; then launch the exact candidate binary, record path/hash/PID
and leave visual confirmation to the owner. No agent screenshots/comparisons.
Scope census: the helper serves bounded point lights and projectors indoors or
outdoors; camera-inside/fullscreen point lights bypass it. Directional sun/moon
base lighting has no such projected local-light rectangle; this finding is not
a statement that every other lighting path is verified.

## Matching build and owner test handoff — 2026-11-12 UTC

Frozencb840a5b9d68eec51a6cf5e7c0d6a3e3dbaea656 builds version26.4.0.68841,
Release/LTO OFF. Doctor/configure/tools build exit0/0/0 (build30.30sec). Existing
stage preserved before configure; installed app and published68836ZIP unchanged.
Matching static346ARM64/332exact VLC/signatures/owned imports/resources/zeroGLSL
pass; strict3515files/0blockers/0errors. Identity+policy662/21 and full LLRender
454290assertions/476passed+one known CPAL skip pass; API/GPU validation observed.
The extra existing native point/projector regression did not run: wrapper used
almetal/Release/almetal_tests instead of its actual .app/Contents/MacOS binary.
Original failed wrapper receipt/log retained; do not call it a test failure or
claim that case was in LLRender. viewer-ready-validation.json records completed
primary checks and this explicit supplemental not-run. No passed suite repeated.

Owner requests immediate viewer launch for bedtime test. At launch no old Alpha4
process remained. Exact followup/newview/Release/Alchemy Metal Alpha 4.app starts
via absolute open -n -a path, exit0, and exact new executable PID74291 observed.
Binary SHA256736d56f5fd940803a2e1c24cb236cfa7a48c9fe33b88f06aa8002eed02064875.
No settings override, login action or screenshots; explicit owner-provided region
URL passed for manual login. Actual login/attachment confirmation remains owner-
controlled. Evidence artifacts/metal/facelight-cutoff-20261111/{canonical-build-state,
canonical-validation,canonical-strict-source,registered-validation,
viewer-ready-validation,viewer-launch,integration-peer}.json. All build/check jobs
terminal/removed; running candidate is deliberately left available for the owner.

## Owner confirmation — 2026-11-12 UTC

After the requested exact68841 candidate launch, owner reports that it fixes the
issues seen with the line, then explicitly requests commit and push. This closes
the reported attached-beard-light cutoff for tested sourcecb840 and binary736d56f5.
It does not assert complete lighting/parity acceptance across all scenes. Root
records owner-confirmation.json and pushes the reviewed fix plus test/handoff
evidence normally to codex/apple-silicon-metal. No additional release publication
or viewer operation is part of this commit/push request.
