# PPD-008 — black world during login with visible UI

Owner report2026-11-12 during the first adaptive68880 destination test: the
world area was completely dark/black for approximately5–10seconds while the
UI remained visible, then the world appeared and continued loading. The owner
recalls this once before, with no established cause. Timing is approximate;
this does not establish a new adaptive-decoder regression.

Retain this as PPD-008, separate from busy-turn PPD-002 and source-confirmed
projector PPD-001. No rendering fix is authorized by a source assumption.
Investigate with the original-viewer reference workflow before any repair.

Candidate source98f41e652beb6150d9012cd25af102bd6c5e76df/build68880, PID36734,
SHA256243b5787b6b3d9789953a34b58e6b58615c859861c41c29dfe19c1400b5cd3d4.
Private evidence is the first180.840185s recording under
`artifacts/metal/adaptive-decoders-20261112/runtime-180348/new-destination/`.
Capture18:05:45.035–18:08:45.876UTC includes login/world entry; initial movement
is+18.965s and STATE_STARTED+25.965s. The owner's black interval has not been
time-aligned to a specific frame or these events. Per-frame Overview tables
are empty; aggregate presentation cadence cannot prove pixel content.

## Original mapping and observed native interval

Read-only comparison against original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45
uses eight successful Git source reads; exact argv/hashes are retained in
`runtime-180348/black-world-source-commands.json`. Candidate renderer/startup/
progress/background files byte-match98f41e652b. No original viewer was launched.

Both displays begin ordinary world traversal at STATE_PRECACHE (original
llviewerdisplay.cpp628, candidate1679). Native pipeline.cpp2130 additionally
withholds G-buffer capture and renderer preparation until STATE_STARTED;
canCaptureMetalWorldPresentation also requires STARTED. The guard deliberately
protects partial startup water/alpha/environment ordering.

pipeline.cpp2678–2809 posts renderer creation to General and its completion
back to mainloop. WorkQueue::postTo is asynchronous, not waitForResult. When
there is no real pre-UI world, pipeline.cpp10969–10983 uses
ALMetalRetainedBackground::makeCleared; almetalretainedbackground.mm249–286
clears RGBA(0,0,0,1). Original/current llstartup calls setStartupComplete before
STATE_STARTED; normal no-intro llprogressview behavior fades its curtain in1s,
with no native-world-readiness predicate.

| Capture event | UTC / trace seconds |
| --- | --- |
| Login cleanup |18:05:57 /+11.965 |
| Initial world movement |18:06:04 /+18.965 |
| Enter PRECache, continuous native UI messages |18:06:05 /+19.965 |
| STARTED and asynchronous renderer warm start |18:06:11 /+25.965 |
| Continued UI submissions during warm-up |18:06:12–17 /+26.965–31.965 |
| Warm complete, measured6.60176s; first logged final BGRA8 world available |18:06:18 /+32.965 |

This is a source-confirmed readiness difference and a strong timing match to
the owner's5–10s report. Actual curtain visibility, exact black pixels and the
first world present on the display remain unobserved. UI publication flags can
include the cleared loading lease and are not sufficient real-world readiness
proof. Zero-Overview buckets0–9.529s occur before login cleanup and do not
establish this interval. PPD-003 diagnostic suppression remains relevant; absence
of warnings does not prove every world transaction succeeded.

Next: decouple early asynchronous renderer/PSO prewarm from scene-capture
admission, retaining device/owner generation and STARTED ordering checks; then
measure curtain visibility and first coherent native-world source/present on
one clock. A readiness-qualified curtain is another boundary to assess. Do not
simply remove the partial-startup capture guard. No rendering repair, pixel
reproduction or owner verification is claimed; PPD-008 remains investigating.


## Owner pixels and exact-run correlation — 19:45 login

Owner supplies Screenshot2026-11-12at19.45.42.png from the explicitly launched
music-button candidate68889/source9f8d17583097d25a7d086296d9553858f6c9853b,
PID77975. It shows black world pixels with rendered UI, HUD surfaces, nametags
and statistics. Owner reports5–6seconds after the loading indicator reaches100%.
The screenshot is owner evidence, not an automated QA-gated scene capture or a
measurement of duration by itself. Original temporary PNG is privately preserved
with SHA256dda480e7ff7a3fbeef9798cb5d2b897c0035f90b3c8cdeced2e28b188206a2c0.
No screenshot or raw capture is uploaded.

Private artifacts/metal/login-black-world-20261112/ contains the screenshot,
owner-screenshot-receipt.json, warmup-key-events.log and original-startup-proof.json.
Exact candidate log is music-startup-indicator-20261112/runtime-194511/viewer.log.

| Current-run event | UTC |
| --- | --- |
| Generic complete-world+UI message, actually clear background eligible |19:45:32 |
| PRECache→cleanup→STARTED, main renderer warm dispatched |19:45:38 |
| Owner screenshot, black world with working overlays |19:45:42 |
| Main renderer factory completes, measured8.20129s |19:45:46 |
| First final BGRA8 world source available for UI composition |19:45:46 |
| Probe renderer completes,0.0556325s |19:45:47 |

The screenshot now proves the reported pixel pattern during the measured warm-up
interval. Its filename has one-second precision and is not a hardware present
clock. First genuine world-source availability is logged, but exact first coherent
on-screen world presentation and actual curtain-hide time are still unmeasured.
The8.20129s factory span excludes General queue wait and main-thread callback delay.

## Why the original avoids this readiness gap

Original1eb44bbd: LLAppViewer::initWindow calls gPipeline.init at3666, before
login; LLPipeline::init calls loadRenderInputs at527, which forces program rebuild
and applies it through shaders->setShaders before reconciling resources at844.
The shader manager synchronously loads water/effects/interface/object/avatar/
deferred programs at700–714. Ordinary original world drawing begins at
STATE_PRECACHE (llviewerdisplay628), beneath the loading curtain.

Native world preparation is coupled to its first capture. pipeline.cpp2130
returns before STATE_STARTED, before reaching the only renderer-create dispatch
at2698–2714. That factory, almetalworld.cpp2553–6320, eagerly prepares a broad
pipeline set, including HDR/blend/rigged variants.104render/11compute creation
sites include loops; these are source sites, not measured pipeline totals or
per-call costs. The measured8.20129s belongs to the whole factory, not a texture
fetch/decode task. Other loading work may continue independently.

llstartup2723 requests startup completion before STATE_STARTED2756; normal
LLProgressView fades its curtain in1s. The native renderer therefore begins
preparing when the original path is already allowed to expose world frames.
Without a real world target, pipeline10969 substitutes makeCleared, opaque black,
then draws independent UI/HUD overlays. This precisely matches the screenshot.
It is a readiness-ordering difference introduced by the native path; no claim is
made that original runtime can never be black under unrelated conditions.

The generic complete current-frame world+UI message at11093 does not test whether
its background came from a real world target; it also logs clear+UI. Its19:45:32
occurrence cannot establish real world readiness. Use final BGRA8 world-source
availability plus actual presentation provenance for future readiness evidence.

## Next correction, not implemented in this diagnosis

Owner explicitly requests publishing the music fix first, then understanding this
cause and original behavior. Music scope is published627e735fa5; renderer code
is unchanged. Next implementation should decouple early asynchronous PSO/renderer
preparation from scene admission, preserving the pre-STARTED water/alpha guard.
First startup idle is a candidate hook after queues/device/pipeline initialize.
Assign capture.device consistently so first-frame reconciliation does not discard
early work; preserve build-ticket identity, main-thread publication and shutdown
drain ordering. Shader reload must invalidate/version pending warm work.
General has one thread in this session; measure startup contention instead of
assuming overlap is free. A curtain readiness predicate must distinguish a usable
world source from clear+UI, without waiting for every texture or hiding failures
indefinitely. No black-startup repair or successful retest is claimed yet.


## Local repair implementation — owner requested

Owner explicitly requests fixing PPD-008 after diagnosis. Base166268ac05;
concurrent audio study4a3ebace81 is documentation-only in this checkout, with
native music implementation isolated in its own worktree. Root exclusively owns
pipeline.cpp/h, llstartup.cpp, llprogressview.cpp/h and panel_progress.xml.
Two peers own only private extracted-production regression fixtures.

Renderer creation is factored from frame admission and called from startup idle.
The complete original device-reset block is shared so early work is initialized
consistently and survives first capture. Work stays on General/mainloop with
exact build identity, current device and builtin shader-generation publication
checks. Exceptions return an error result instead of stranding the pending job.
A failed build is latched until device change or explicit shader reload; stale
completion cannot clear a newer ticket. Pre-STARTED scene admission is retained.

A successful ordered window submission records real pre-UI world provenance,
never the UINT64_MAX cleared-background sentinel. Readiness validates startup
epoch, post-epoch source frame, shader generation and full device/surface/size/
reverse-Z context. New login and shader reload retire old readiness.

The normal and intro-close paths request one shared curtain fade. The curtain
stays opaque at99% with Quit until actual world-backed submission, then fades
normally from100%. Lost readiness during fade restores the opaque hold. After30s
it offers a localized wait-or-quit message; timeout never reveals black. Waiting
Quit remains a real quit action even after STATE_STARTED. Intro selection and
ordinary teleport Cancel retain their owners. Misleading clear+UI completion
logging is separated from real-world-backed UI publication.

Private evidence: artifacts/metal/login-black-world-fix-20261112/.
Actual-body curtain fixture passes63assertions/13cases; baseline fixture has2
expected failures for late warm scheduling and premature curtain fade. Renderer
helper/readiness fixture and full candidate build remain in progress. No active
viewer restart or user music preference/cache change; no push authorization.


First source63af048d20 configures/builds successfully as68893; lifecycle+curtain
actual-body fixtures pass127assertions, and the pipeline fixture passesASAN/UBSAN.
Focused physical Metal validation passes31,825assertions/16cases with API/GPU
validation: retained-background provenance/context, shader reload, completion
and instrumented render/compute cache lifetime. No viewer restart performed.

Final source review also adds the two new-state clears to the existing impostor
device-reset branch: an impostor-first device transition must clear a previous
renderer failure and startup readiness before main capture sees the new device.
This is not an added startup drawing guard: original offscreen impostors are
allowed during PRECache and remain separate from startup-world readiness. First
build receipts are preserved under first-build-63af048/; final source fixture
and incremental build follow this consistency correction.


## Verified local candidate and owner acceptance

Final sourceb0585e4c1ad5f7fad45461876a6c2a1dbf7adaa4 builds26.4.0.68894,
Alchemy Metal Startup Readiness Fix.app under the shadow-occlusion Release
staging directory. Executable SHA256
9bcea485e118976d29438839c7ebdf83bd73bc4288850c1cf774d6412e8a27c1;
full source SHA is embedded. Final configure/wrapper build exit0/0, Release/O3,
LTO OFF. Staged progress XML exactly matches source. verify_app0 checks346
ARM64 Mach-O/zero invalid; deep strict signature0; final source audit3516files/
912informational/zero blockers/errors. Prior68889 music-fix binary remains
unchanged; native audio work stays isolated in its own worktree.

Final extracted-production pipeline checks70/70 pass normally and with
ASAN/UBSAN; unchanged curtain checks63/63, combined133. A secondary-reset
negative control restoring only63af's old branch fails4 of70checks, including
failure to recover prewarm after an impostor-first device change. Previous
fixture/receipt variants remain preserved. Physical Metal checks31,825assertions/
16cases pass with API/GPU validation and explicit render+compute validation
instrumentation. Those backend libraries are unchanged by the final two frontend
reset clears; recorded binary/library/metallib hashes bind that evidence.

The earlier viewer is already closed when launch is requested by the fix
validation workflow; no active viewer is killed. Exact68894/hash9bcea485 starts
PID8114, runtime-202201/run.json, with retained populated cache29384files,
normal4/max6/adaptive1, OpenHT0.27.0, manual login and no recording. No music or
graphics preferences changed. Source main warm starts20:22:04 before
STATE_FIRST→BROWSER_INIT; LOGIN_WAIT is reached that second. Main prewarm
completes20:22:11 after7.36349s, before login/world entry.

Owner logs in manually. At20:23:29 the log records startup cleanup/STARTED,
first real native world submitted for epoch1/sourceframe141, then real-world
readiness and curtain fade. Probe renderer completes0.0306265s. Owner states
"its good, black screen is resolved". PPD-008 is verified with owner acceptance;
no exact GPU display timestamp or overall login/FPS speedup is inferred.

Evidence root artifacts/metal/login-black-world-fix-20261112/ contains final
configure/build.json+logs, build-identity.json, source-stage-preservation.json,
arm64/signature/strict-source-final receipts, prewarm-tests/summary.json,
gpu-tests/receipt.json, runtime-prewarm-proof.json and owner-verification.json.
Exact final build/verification commands:

```sh
cmake -S indra --preset ninja-os-arm64 -B build-Darwin-ninja-os-arm64-shadow-occlusion '-DAL_CHANNEL:STRING=Alchemy Metal Startup Readiness Fix' -DAL_BUILD_TESTS=OFF
AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64-shadow-occlusion AL_METAL_CONFIG=Release AL_METAL_JOBS=3 nice -n 10 tools/metal/build.sh build
tools/metal/verify_app.sh 'build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/Alchemy Metal Startup Readiness Fix.app'
codesign --verify --deep --strict --verbose=2 'build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/Alchemy Metal Startup Readiness Fix.app'
python3 tools/metal/audit_gl.py --strict --output artifacts/metal/login-black-world-fix-20261112/strict-source-final.json
```

All exit0. Final harness/diff checks follow this handoff. The repair remains
local: owner acceptance does not authorize pushing. Next action is scoped push
only on explicit owner approval; unrelated native-audio work remains independent.


## Owner-authorized scoped publication

2026-11-12: Owner explicitly says "you can push the black screen fix to
GitHub". Prepare one coherent descendant of published627e735fa5 containing the
six PPD-008 production files byte-for-byte from owner-testedb0585e4c1a/68894, plus
this investigation, PPD-008 register entry and state/ledger checkpoints. Original
local commits166268ac05/63af048d20/b0585e4c1a/226c334be9 remain retained.

The interleaved native-music study4a3ebace81 and later native-audio commits are
excluded from this publication. The shared local branch and independent audio
worktree remain intact; no force push, release or raw evidence upload is authorized.
The original build/fixture/owner evidence above remains source-pinned; this
publication does not modify production code relative to the verified candidate.

Publication checks2026-11-12: tools/metal/doctor.sh exits0 on the same M1Max;
python3 tools/metal/validate_harness.py exits0 (33requiredfiles); git diff --check
exits0. Git byte comparison confirms all six changed production files and the
entire tracked non-documentation tree exactly match testedb0585e4c1a. The
production build/verify_app/source/GPU evidence above is therefore unchanged;
no new viewer build or launch is claimed for this documentation/scope operation.
