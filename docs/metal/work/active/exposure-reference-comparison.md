# Original/native exposure comparison — 2026-11-13

Status: owner-supplied comparison reviewed; controlled pair incomplete, PPD-080/081; PPD-082 remains separate untested transition concern.
Owner explicitly authorizes running prior reference and screenshots at matching
settings/location. Latest owner says **Daylight** (not agent-assumed Midday) and
saved camera **Lev1233**; owner will configure reference view. User asks for fast
launch and takes over candidate shutdown. Root stops optional automation prep,
confirms candidate PID97692 normal exit0/state10, then launches reference.

Reference exact existing app:/private/tmp/alchemy-reference-1eb44bb/
build-Darwin-ninja-os-arm64/newview/Release/Alchemy Metal Development.app,
version26.4.0.67611, executableSHA256
6bee7df4d3317e6f4cb57c941accd3e84d7b0cb23c0b1d13c5496033f5591dc5.
Freshdeep signature/ARM64/embeddedf66observer+1ebrendererbase proof and273packaged
shader exact1eb blob matches retained. External.git/source tree is pruned;
do not claim clean checkout or fresh build. Existing artifact matches archived
67611 hash, so no rebuild needed. It already supports HDRI Preview/EXR loading;
absence of our latest Metal sky port does not imply absence of HDRI.

Candidate exactbuild:/Users/dekstroza/Programming/AI/Secondlife/Alchemy/
build-Darwin-ninja-os-arm64/newview/Release/Alchemy Metal Sky.app,
version26.4.0.68965, executableSHA256
3f629d75890f1bed93f933f6f3f54e685e4c80a47d114b45d419808fc45ba777.
No rendering code changes or new app build in the exposure investigation.

Artifacts: artifacts/metal/exposure-comparison-20261113/ contains fresh provenance
reports, frozen source graphics/Looks/camera XML and hashes, derived per-role
profiles/configs and launch receipts. Graphics LevDarkheart storesExposure.5;
owner Look Lev overrides to1 with all agreed baseline values. Derivation is
explicit graphics->Looks->camera plus shared feature/diagnostic pins. Role-only
original typed RenderHeroReflectionProbeDetail1 repairs its existing unregistered
mirror-detail key for equivalent requested detail1; native usescorrect sharedkey.
No installed skill changes or credentials in commands/config. LLDB wrapper runs
unchanged installed QA launcher and watches native child, killing faults without
resume. Reference first launch uses original read-only LEAP probe.

Reference launched PID10889; explicitSLURL secondlife://Spare%20Time/105/21/24.
CUAobserves exact67611 window, actual location bar105/21/24 and camera presetcombo
Lev1233. Those observations alone are not yet full effective-camera/environment
or settled-scene proof. Owner configuring Daylight/camera now, capture readiness
pending. No paired reference/native screenshot or parity/FPS conclusion yet.

Owner extra observation: at HDRI EV-1.8 horizon appears well blended/no sky-water
line with agreed settings; owner returnsEV-3 for baseline. Current baseline
image14:08 andEV-1.8image14:17 retained. This is an owner observation about the
loaded HDRI, not general water parity or closure of the whole-scene report.

Next: wait owner setup completion; obtain fresh original live position/settings
and actual camera/environment UI proof, run unchanged visual-QA observe/verify/
gate immediately before capture. Save native snapshot/window PNG with explicit
identity, then equivalent exactSky candidate. Skill blocks mismatches; do not
infer success from launch args/preset names or alter immutable run receipts.

## Owner-supplied reference result

Owner14:31:18 supplies reference screenshot, reports same Daylight/Lev1233,
agreed settings and HDRI split.5/EV-3. Owner completed setup and handoff; CUA
phase completed once after observing exact reference window and owner result.
Reference remains open for owner. No automatic restart/switch back performed.
Raw supplied image saved as owner-reference-hdri-minus3.png; native14:08 image
and SHA256manifest retained alongside it. Do not label these agent-gated captures.

Fresh LEAP proves originalPID10889 loggedin at105.017723/20.924770/24.309818,
46queriedcontrols. Preflight observe0/verify2: original RenderFSAAType reports1
(FXAA), plannedprofile2(SMAA); owner intentionally changes HDRI split to.5 while
initial ordinary-sky profile declared0.0. The gate correctly blocks. No own scene
snapshot is taken or capture-pass claimed, and immutable config/run receipts
are unchanged. This is a manual HDRI test differing from the initial ordinary
plan, not proof of a renderer failure. Effective numeric camera matrices/full
EEP/EXR identity and same settled asset/probe state remain unverified.

Visible observations limited to supplied images: both have clear light/dark
HDRI clouds on the left and very pale mostly featureless procedural background
on the right. Both retain green sunlit foliage, deep building/tree shadows and
textured road/crosswalks; no obvious whole-frame bloom veil. Original common
foreground looks somewhat lighter, native somewhat darker in the available
areas. Owner confirms matching framing; the native exposure dialog covers much of
world. The earlier framing objection is withdrawn. These manual images alone
do not measure an exact exposure delta or establish parity/regression cause.
No numerical pixel/FPS result is asserted. Independent second opinion agrees
on those image observations; no renderer correction follows from them alone.

Disposition: source audits still find no cause for the broader pre-existing
brightness report. PPD080/081 remain investigating; owner EV-3 cloud recovery
and EV-1.8 horizon observation retained. PPD082 separate transition concern is
unreproduced. If a controlled follow-up is needed, prepare the intended HDRI.5
profile before launch, match actual AA/camera/effective environment and scene
state, then obtain fresh observe/verify/gate before agent captures. Do not rerun
or alter owner viewer merely to make an already-supplied manual image look gated.

- 2026-11-13 follow-up: Owner disputes the framing explanation and requests the exact pair side by side. Exact original image hashes confirm Metal14:08:20 and reference14:31:18 were used. Camera framing was not independently verified; attributing visible landmark/cloud differences to camera movement was premature. Retain the images and distinguish observed pixel differences from an unproved camera cause.

- 2026-11-13 correction after side-by-side display: Owner explicitly confirms exactly the same framing; only the native exposure dialog obscures part of the scene. Accept the matching visual framing and withdraw the unsupported camera/framing objection. Keep numeric-camera readback as an unmeasured fact, not a reason to dismiss the owner comparison. No image pixels, renderer settings or source changed.

- 2026-11-13 new pair: owner supplies unobscured Metal14:48:40; both OS captures are3456x2234. Display clean Metal14:48 versus reference14:31 unchanged, superseding obscured14:08 for PPD083 comparison. Additional native in-viewer snapshot14:49:55 retained separately to avoid conflating capture paths. Root has not captured or altered the images.
