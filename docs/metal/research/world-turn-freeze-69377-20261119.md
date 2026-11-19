# World-turn freeze after parallel loading correction

## Exact closed owner run

Candidate26.4.0.69377 built from74d9264b1ff8f27f2ce3b1ba07299135601465dc,
indra b5fb88cd9679a35606f15793beb21a365cfdeca4, executable SHA256
3ad5bf0ee7a2ca395d627daacd8d4a017b69fb1a22edd54549a022e34c95b62c.
Private run `Alchemy-loading-69377-hkf9xk7j`, PID65754, owner manual login and
Home/club tour, no profiling, frame capture, build or test jobs during the run.
Owner reports textures/HUDs loaded almost instantly and much better, but
occasional world/camera freezes persist even while turning at Home. Possible
camera-triggered recovery was explicitly uncertain. Club arrival was independently
observed in the exact viewer UI; Home was owner-reported, not inferred from the
requested startup URL. No paired FPS, scene-load latency or visual-parity claim.
Cache/account controls were retained, OpenHTJ2K only; runtime reports10cores,
8fastest-tier,6decode workers,normal4/adaptive1 and native texture upload worker1.

The owner-approved normal logout was followed by Goodbye11:18:35UTC and observed
PID absence. Process exit status is not recovered from PID absence. Final private
log:3,429,459bytes,9,642complete lines, SHA256
3f883b3be5174027d38d76883eed7fd7050a3b9e248c912e5eb5b16389303cdd;
338warnings/45exact source sites, zero ERROR/FATAL. [Every source site and outcome](warning-runtime-69377-source-audit-20261119.md) is reconciled;36emitting bodies
match original1eb and44match69343. The56unknown animation-joint warnings use the
original tolerant downloaded-animation lookup path; they are distinct from skin
palette fallbacks. Other newly observed rows include one handled server teleport
failure (three handler-site rows), a missing landmark and an obsolete media
request. These outcomes do not establish incorrect porting. Source-site reconciliation and
numeric/rigging/asset receipts remain under the run's private `analysis` directory.
No raw statistics packets, account/asset identifiers or credentials are copied here.

Final native census:36,453dequeued/36,420worker posts, no synchronous create
attempts/retries and zero pending creates. The33difference is not independently
attributed. Cumulative worker input19,860,765,511bytes is not resident memory,
elapsed throughput, latency or a performance measurement. Successful runtime
worker activation corroborates the owner's positive loading observation without
isolating which integrated correction caused it. PPD463/486–490 retain remaining
coverage/owner-acceptance requirements.

No skin fallback or missing-mask consumer event was observed. All28observed
requested-aux decodes produced1component;63producer events/57assets approach the
64-key cap and contain no primary-only to later-demand transition. PPD311/471
remain open. The shutdown statistics packet positively exercises PPD491's safe
preference schema: only allowed numeric/Boolean graphics/pool fields, no account
children, no unknown keys or type violations. This is structural screening,
not whole-packet anonymity.

## What the freeze logs establish

Owner freeze mark11:11:55.814614UTC has a private prefix receipt. Actual retained
pre-UI world notices at11:11:51,11:11:57 and11:12:01 bracket it while current UI
continues. This establishes stale-world/fresh-UI operation around the episode,
not a CPU/GPU failure attribution or an exact receipt for every user-visible pause.

The first deduplicated world-input snapshot is earlier,11:10:40/frame927:
current complete G-buffer and light map; no base/fused/composed/post/presentation
product, all attempt flags0 and incomplete line0. Fused staging can be consumed
before these flags are set, and silent early returns exist. Neither zero attempts
nor that75-second-old snapshot proves the current freeze branch. A second snapshot
at11:17:08/frame7735 has the same missing lighting products but different exposure
readiness; it is not the earlier owner freeze receipt. No coherent
ordinary-probe-pair wait, base encoder failure or compound-lighting failure warning
was emitted. PPD003 must acquire fresh repeated bounded receipts instead of
deduplicating one readiness tuple for the whole run.

## PPD492 original-to-native contract, before repair

Reference `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`:
`llheroprobemanager.cpp::updateUniforms`491–535 advertises the selected live mirror;
`class3/deferred/reflectionProbeF.glsl::tapHeroProbe`779–816 uses count0 to skip
only mirror contribution, preserving ordinary probe lighting. Native manager
selection remains camera/frustum dependent. Native radiance is a separate
completed publication with explicit selected-owner/epoch/generation identity.
The native exact-resource guard is required; it has no direct GL equivalent.

Concrete source counterexample: a completed publication belongs to mirrorA.
Turning selects mirrorB in `LLHeroProbeManager::update`; preparation can defer or
B can be occluded, so `renderProbes` does not publishB. `mMetalRadianceReady`
still describesA. `updateUniforms` currently advertisesB with count1 from that
Boolean alone. `fillMetalImpostorHeroProbe` correctly rejects the owner mismatch;
`renderMetalWorldBaseLighting` returns before ordinary base lighting. Real display
ordering (selection/renderProbes before the later uniform flush) permits this
state. This is source-confirmed; the recorded owner incident is not joined to
these owner identities and its cause remains unproved.

Repair the producer's advertisement to require a live selected owner equal to
the successful publication owner and the current publication epoch. Keep
same-owner completed radiance available during refresh, all exact native
resource guards, original mirror selection, geometry/math, quality and world
completeness rules. Do not turn an invalid active mirror into a downstream
lighting-success claim. Qualify the actual uniform-production body against
oldA/newB, completeB, stale epoch, disabled/empty/dead/null and same-owner refresh
controls; then build/verify before any new owner candidate.

## Next evidence if the symptom remains

Fresh retained-world receipts must include current-frame source/attempt policy,
selected-versus-published mirror coherence (Boolean/numeric metadata only), and
the exact first main-lighting stop reason. Record onset/repeat/recovery without
polling the GPU or printing runtime identities. A new diagnostic result must be
joined to the owner freeze time; merely repeating a successful frame is insufficient.

Metal frame capture already exists in `almetalframecapture.h/.mm` with device-
all-queue scope and explicit bounded display/pass boundaries. A trace recorded
after a CPU-side lighting return can contain only the retained-world/UI work;
it cannot retroactively show a skipped producer. First use the fresh branch
receipt. If it says all current world products encoded/published yet the display
is stale, collect a device-all-queue display interval around the first retained
event plus its frame/owner-free metadata, and a healthy turning interval from the
same exact binary/settings/scene. Capture start/stop overhead must be marked and
excluded from normal frame pacing. No new trace or reference comparison has
been executed, and no capture success, GPU cause or freeze repair is claimed.

## PPD493 final-snapshot failure

At11:18:33 the known last-screen snapshot request precedes native/raw snapshot
failure and LogoutRequest. Native exact-frame guard rejects before readback:
current frame9542, complete capture/G-buffer and matching device, but no composed
lighting/post/presentation/world-UI target; presentation/UI frame unknown.
`saveFinalSnapshot` ignores `saveSnapshot` false and unconditionally latches
`mSavedFinalSnapshot`; shutdown also sets its own attempt latch. An existing saved
image is not proved overwritten, and owner login-background acceptance is unknown.
This creation failure is separate from PPD480's loading fallback. Inspect original
caller and actual native production before selecting a correction; preserve the
exact-current capture guard and avoid arbitrary repeated attempts.

## Offline repair qualification in progress

PPD492 actual `updateUniforms` body is extracted into the production include
`llhero_uniforms_native.inl` so the controlled fixture executes real count and
shape/matrix code. Initial fixture compile failed because required math headers
preceded neither SIMD nor matrix headers; corrected include order then compiled.
The old production body fails14of135assertions in3of6cases; adding successful
publication owner/epoch qualification passes all135assertions/6cases and its
registered CTest. Cases retain coherent same-owner refresh, successfulB, stale
epoch, mirrors-off clear, null/dead/empty states and box/sphere transforms. This
proves the source invariant correction, not a fix of the owner's runtime freeze.

PPD003 per-capture metadata observes actual base-light return/staging/encoding
state with its frame stamp and selected/published mirror coherence. A fixed-size
Observer admits64distinct states plus64repeat receipts per capture-owner lifetime,
reports budget caps, records a first unchanged-background repeat after0.1s and
subsequent repeats at1s, and resets timing on recovery without refilling budgets.
No identities enter keys or output. Default-off observation collects neither
clock nor metadata; actual stage execution/admission/native guards are unchanged.
The historical retry-counter suppression remains separate and unresolved.

Eleven focused registered CTests pass: new hero uniform body, probe admission and
readiness, native hero capture readiness, all4retention modes, and actual native
hero overwrite/HDR publication/probe formats. Native tests report Metal API and
GPU validation enabled. Refreshed observer checks4/47/33/9 pass withASan/UBSan;
actual old-header recurrence control5checks fails3 versus new0, while progressing
background no-flood control passes both. Observation durations are wall time
since last observed background progress, not CPU stall durations. Hero Boolean
fields are current manager samples, not separately stamped uniform-production
events; staged base does not prove later fused composition success.

PPD493 result-only repair1fca2c26cf→root2f173468cc captures saveSnapshot's Boolean
result, returns on failure before marking saved or copying an older image into
Home, and adds no retry loop/timing/guard change. Exact pre-fix whole caller
matches original1eb; the false-success latch is inherited. Actual-body synthetic
fixture12cases/68checks passesASan/UBSan versus12baseline failures; root normal
CMake target/CTest passes, independent source review passes and full isolated TU
syntax0 (initial missing submodule include failure retained, then exact pinned
read-only header fallback). This does not repair or prove the missing native
current-world products; existing shutdown-attempt latch remains unchanged.

[Exact existing capture controls and next-run recipe](world-turn-capture-controls-20261119.md)
are source-verified. No existing UI/LEAP control arms first-retention capture and
no pre-event rolling buffer exists. Prefer fresh diagnostic receipts first; a
CPU early-return needs branch/owner evidence rather than a post-failure GPU
trace. If GPU evidence is needed, the device-all-queue display route can be
time-targeted at launch under explicit gates, with healthy/stalled diagnostics
joined to its interval. Automatic120s is illustrative timing only, not a first-
failure trigger. A new event-trigger implementation would have to latch a
retention event and start before commands on the next eligible display; it is
not implemented or claimed by this checkpoint. No new capture was run.

## Qualified offline candidate69381

Built source **434f4df20a4e9170fb2f07d3752d02f34064ceac**, indra
**e9803807ea1e81a270db9b879c238625afa976eb**, version26.4.0.69381,
CFBundleVersion26.4.0d69381. Separate stable app:
`/Users/dekstroza/Alchemy-warning-inputs-wh3ienda/candidates/Alchemy Metal Functional Fixes 69381.app`.
Executable `Contents/MacOS/Alchemy Metal Warning Inputs`, SHA256
**1143f9af52167919a6da718772f39cd8205227865a12dcb7221347ec467cf28b**.
Full source SHA is present in configured cache, generated header and executable.
The first receipt assertion incorrectly expected numeric-only CFBundleVersion and
stopped before copying; inspecting/asserting its actual full development format
corrected the receipt, with unchanged build/source.69377 executable rehashed to
its original3ad5bf0e value; neither prior candidate nor cache/account files changed.

Executed onM1Max/64GiB/macOS26.5.2/Xcode26.6:

- `tools/metal/doctor.sh`:0;10cores/8fastest-tier, nativeARM64/Metal device observed.
- `AL_METAL_BUILD_DIR=build-Darwin-warning-audit AL_METAL_CONFIG=Release AL_METAL_JOBS=6 tools/metal/build.sh build`:0,16steps.
- `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 ctest --test-dir build-Darwin-warning-audit -C Release -R '<12-test filter>' --output-on-failure`:0,12/12; exact filter/argv were issued in this session and retained in the CTest detail/test files. Native test outputs explicitly enable Metal API/GPU validation.
- `tools/metal/verify_app.sh <exact stable69381app>` and `codesign --verify --deep --strict --verbose=2 <app>` combined command:0;346Mach-O files/0invalidARM64 slices, valid signature/designated requirement.
- `python3 tools/metal/audit_gl.py --mode strict --output <private combined-strict.json>`:0,3638source files/0blockers/0errors/923informational compatibility rows.
- `otool -L` owned main/CEF-host/VLC-host:all0, no forbidden direct graphics import. This is not a foreign/runtime-loaded-library or live-context claim.
- Documentation harness and `git diff --check`:0 before code freeze; final doc-only handoff checks recorded separately. No tools/metal implementation changed, so no redundant tool-suite rerun.

Private `analysis/combined-artifact.json`, `combined-source.json`,
`combined-focused-detail.log` and canonical `functional-fixes-69381.json` retain
identities, actual output/exit results and hashed evidence. Source492actual-body
135assertions/6cases, diagnostic4modes4/47/33/9checks and snapshot68checks/12cases
are controlled CPU correctness evidence. Native tests preserve prior resource
guards, but do not exercise a connected owner camera/mirror scene.

**Not launched.** All viewer/build/test/profile/capture jobs are closed. Next
owner run should use this exact app with opt-in fresh world diagnostics and
ordinary turning/walking/teleport, no concurrent profiler/build. Test occasional
world freezes and a successful logout-image capture; do not assume either has
recovered from passing source/GPU unit checks. General loading improvement remains
the69377 owner observation; no new whole-viewer FPS/latency/quality claim.
PPD002/003/125/311/463/464/471/493 and denied-asset causes remain scoped/open;
PPD492 is ready_for_owner, not verified. No push authorized or performed.

## Continued native snapshot production audit

After preserving69381, offline original/current audit rules out the same-frame
retry premise: `display` increments gFrameCount for every snapshot attempt, and
`snapshot_request` explicitly refreshes per-frame products/attempt gates. Cull
references remain owned through deferred lighting/UI and clear afterward. The
fused main-lighting path excludes gSnapshot. Normal cleanup alone starts resource
draining, after the recorded snapshot failure. Nested final-snapshot idle can
reenter shutdown/send logout in another admissible ordering, and a late fallback
after isExiting cannot generate a new native frame, but safe69377marker order is
requestQuit→snapshotfailure→LogoutRequest→QUITTING→cleanup. Neither alternate
ordering is proved to explain this observed initial failure.

The existing exact-snapshot error reports GBuffer target and composition/post
attempts, not lightmap/base attempts. Add only opt-in stamped base branch, current
atmospheric policy and selected/published hero coherence at that existing error;
no retained-world Observer or absent UI-background dereference. Initial extraction
selected the generic fail lambda by mistake; corrected againstHEAD before any
compile or newcandidate. The detailed error/false result and exact-current guard
remain intact.

PPD494 is a separate source-confirmed parity gap. Original1eb pipeline11819
unconditionally clears transparent screen, skips only the sunlight/soften-sky
when RenderDeferredAtmospheric=false, then executes local/projectors and forward
geometry. Native skips its sole base producer, and downstream local/projector/
composition guards cannot proceed. The persisted setting is supported and defaults
true. Actual69377value is unknown; no incident attribution is made. Repair with a
direct native transparent-zero color owner carrying the current GBuffer depth,
then the unchanged local/projector/post sequence. Do not force settings, reuse
stale worlds, execute normal sun shading with guessed zero constants or relax
completeness. Native/actual-caller qualification and owner acceptance remain
pending;69381 remains preserved/notlaunched.

Continued493 failure diagnostics pass the real included error/return body in
separate cached-gate processes: disabled2cases/6checks, enabled12cases/67checks,
ASan/UBSan. A poisoned selected pointer is untouched while disabled/null-error;
metadata and append-allocation exceptions preserve the original error. No
background/timer/GPU API exists in the fixture.

494 root CPU18cases/194checks each disabled/enabled pass actual prelude and both
view dispatches. Negative controls use exact22aa production snippets: oldcaller
32/50failures, oldprelude37/46, both47/68. These are synthetic provider boundaries,
not a renderer/whole-world test. Native backend0eef007590→root68e75ac0f4 and
physical native tests compile as separateARM64 objects; normalroot nativebuild/
GPU clear/depth/accumulation execution are pending. True-mode shading/math/native
guards remain unchanged by scoped source review.

494 native root tests initially failed1of511assertions due to the new fixture's
reversed normal_hdr/hdr arguments, not a renderer correction. After fixing only
argument order and failure context, actual native3cases/641assertions pass with
Metal API/GPU validation. They verify exacttransparentzero, unchangeddepth/stencil/
allGBufferstores across8format/reverse combinations, positive point/projector
continuation and malformed-input/borrowed-prefix cancellation/retry controls.
Header/production native method remain independently reviewed and unchanged.

A further bounded492physical-publication lifetime audit finds no new normal-path
stale-ready gap: ordinary probe scratch swaps preserve hero publications; settings/
plan/reset/teardown invalidate the manager before or together with losing native
resources; filter publication commits before updating generation. Conditional
device-change reset is unpaired, but the current SDL production device is assigned
only at construction and retained by context switches. No normal camera/resize
source counterexample was found; do not infer runtime recovery or weaken guards.

## Qualified combined candidate69384

Built source **0bacb370fa298fa497505a9f374b3fd13666fc16**, indra
**8f350bf47e4ded73f90aa238dea5ac34ea88e9e9**, version26.4.0.69384,
CFBundleVersion26.4.0d69384. Stable app:
`/Users/dekstroza/Alchemy-warning-inputs-wh3ienda/candidates/Alchemy Metal Functional Fixes 69384.app`,
executable `Contents/MacOS/Alchemy Metal Warning Inputs`, SHA256
**27f1595ae6e49a2aca0b508eaff988699b150c74cabe1a4a4e10727396567c2a**.
It includes every earlier69377/69381 confirmed local correction plus494 and
expanded493 failure inputs.69377/69381 executable hashes are independently
unchanged. User returned and explicitly requests inclusion of all confirmed
fixes plus a procedure to capture the freeze. No push approval or push.

On the sameM1Max/64GiB/macOS26.5.2/Xcode26.6: full173-step wrapperReleasebuild0;
20selectedCTest invocations0 with actual Metal API/GPU validation; strict3641
source files/0blockers/0errors/923informational; exact stable app346ARM64/0invalid
slices, deepstrictsignature0 and all3owned-host directimport checks0. Header/
cache/executable contain the full built source SHA. Private canonical manifest
`functional-fixes-69384.json`, `continued-artifact.json`, final-continued-* logs
and detail preserve exact results. Full command filter and source/hashes are
retained in the qualification receipt; no runtime/FPS/complete-port assertion.

Next run is prepared privately as `Alchemy-freeze-69384-xjxh4etn`: manual owner
login, Home request only, existing graphics/cache/account data retained and
OpenHTJ2Konly. Live read-only controls additionally include
RenderDeferredAtmospheric and RenderMetalMainLightingRenderPass, removing those
unobserved policy operands from another freeze. No fixed-location/paired visual
comparison is asserted; actual arrival must be independently observed. All
build/test/capture work ends before launch/measurement; source stays frozen.

Capture sequence:

1. Log in normally atHome; after arrival turn/walk as usual. The bounded watcher
   records fresh retained-world base stage/frame, attempt/readiness/coherence and
   exposed policy controls; user marks a visible freeze and whether menus respond.
   Automatic >=0.1s unchanged-background events receive private byte cutoffs,
   explicitly not owner reports. Full-prefix copying is deferred until after the
   run to avoid repeated I/O in the event. No GPU wait/sample/profiler is added.
2. If rendering stopped at a CPU readiness/producer branch, inspect that exact
   frame and source prerequisite. A post-event GPU trace cannot recreate work the
   CPU never encoded. The snapshot failure appends the same stamped base/coherence
   context without depending on a missing world-UI target.
3. If current products are encoded/published and the cause remains GPU/presentation,
   use the saved short device-all-queue display-interval capture procedure with
   healthy and stalled context. Existing launch-time quiet capture is timed, not
   first-retention-triggered; no UI/LEAP arm/pre-event buffer is claimed. Mark
   native capture overhead and exclude disturbed periods from FPS inference.

No connected69384 observation or capture has occurred at this qualification
checkpoint. The launch sentinel will retain Popen.wait and exact exit status;
private run.json is the subsequent authoritative runtime record.


## Closed owner run69384 and offline follow-up

The prepared run subsequently completed, exact source/binary unchanged. Owner
reported no freeze atHome, teleported toClub, reported fast enough loading/no
freezing, heard parcel music correctly, returnedHome without noticed issues and
logged out normally. Club UI and exact69384 build were independently observed
atWarehouse21/MysticHarbor(51,197,2004); Home and return are owner reports, not
independent requested-login-location proof. Club14–20FPS is one reported range,
not a matched performance result. There was no Metal capture/profiler during this
run. Prior69377 freeze evidence is retained; this bounded positive test does not
prove all freeze causes absent or isolate each repair's effect.

The sentinel retained actual viewerexit0 at2026-11-19T12:48:55.993985Z. All run
watchers stopped. Private run `Alchemy-freeze-69384-xjxh4etn` has5,208,249bytes/
15,260lines, SHA25671a491ac1d71815f3c765704092f82752fd34812cb8a7cf3bfe9b69d1f6ca9ce,
395WARN/46source-line sites, zeroERROR/FATAL/unparsed severity. Every warning still
needs semantic disposition; row/source accounting is not proof that every
runtime cause is cleared. PPD495 records one decode failure, PPD496six native
legacy bump-generation failures; source/error provenance audits continue without
extra requests or manual cache mutation. Asset/status caps remain explicit.

Two retained-world samples at12:48:53 follow successful saved-final-snapshot and
LogoutRequest. Current13066/13070 differ from background13065; capture/base/post/
presentation/exposure all carry old13065. Encoded base is explicitly not current.
`display()` intentionally skips fresh geometry once logout request is sent and
the final snapshot saved. This is a shutdown cohort, not an owner-visible freeze
recurrence or a current base-light failure. Private source join02/row receipts
preserve the ordering.91other displaypost-withheld records do not establish an
eligible presentation-without-exposure failure.

Fresh native snapshot completed at12:48:53Z; the exact logged last-image file
has matching freshmtime12:48:53.470109Z,3456×2168RGB8,14,158,389bytes and SHA256
9214ddee909af8f1edd45416b9dce0d38dbdb397da016c8aaa17eaabf3b5c524.
Read-only PNG checks validate all1728chunkCRCs,22,479,992decompressedscanlinebytes,
filter types and nonblack pixel content. Account path/identity and image remain
private. This verifies normal logout file production, not the previously failed
native producer's root cause or a subsequent login's UI display.

Final typed decode census39,270submitted/started/completed, zeroactive/wait;
native60,218posts/60,241dequeued, createpending0/retries0,69,740actualworkeruploads/
15,060batchcommits/failures0. Downscale305prevents a whole-pipeline-drained claim;
cumulative29,360,678,834inputbytes is neither resident RAM nor elapsed throughput.
No skin fallback/nonfinite/product warning, no missing bake-mask or skin-input
capture;3invalid-animation-joint warnings are separate.64bakeevents hit cap:
38requested auxiliary masks present/onecomponent,26primary-only; absence after
cap is not coverage. ActualAtmo1/fusedmain0 means494false branch was not exercised.
Owner audibility, rather than requested playback state, verifies audio here.

After exit, root integrated revieweda305f1f17b5c8ebd2f321db30426b543e4381616 source
only. PPD003 fresh-product reset now preserves local/projector failure streaks;
local warning emits once at120. A separate owner-local actual-batch-commit streak
survives per-transaction composition success and resets only on an actual
successful commit; canceled/absent commands are neither success nor new failure.
Existing product invalidation, attempt gates, retry/failure rendering policy,
generic GBuffer counters and lifecycle resets remain unchanged. The original
viewer has no equivalent native counter; nearest original traversal/reporting
contract is retained. This improves failure visibility, not rendering speed or
proof of GPU completion, and was not in the owner-tested69384 executable.

Initial normal fixture build failed because Catch2 forbids decomposing logical
Boolean expressions without parentheses. Two test-only syntax corrections led
to normal newview_tests build0/CTest0. Actual four production-body CPU fixture
passes5cases/1741assertions underASan/UBSan. Exactoriginal controls compile0 and
fail as expected: all-original383checks/5cases, reset-only252/3, local-only2/1,
batch-only131/2; fixed body0failures. Private counter-integrated-* logs and
ppd003-counter-fix-01/integrated-control-results.json retain exact argv/exits.
Fullapp/source/static qualification follows; no push or new launch authorized
by this evidence. Next inspect PPD495/496source errors, qualify counters and
retain stage/coherence capture procedure if an owner-visible freeze recurs.


Counter-only build69386 subsequently qualified at source
7a2709d08674ee0eae81aabe9819ec287648eb5e / indra
6772b628d6aeb3c7711a542f84673820989c4fa3. Doctor,14-step wrapper Release build,
21 selected CTest invocations with Metal API/GPU validation, strict3645files/
0blockers/0scanerrors,346ARM64 slices, deep strict signature and3owned-host direct
import checks all pass. Stable local app `Alchemy Metal Warning Counters 69386.app`
and private receipt are preserved; executable SHA256
a82ebb294485e1cc7789518e217f2474d8b5e6671173d981e6965f81ef171868.
No new viewer launch or push. This later warning correction was not the code in
the owner's successful69384 run. All four old-source negative controls fail as
expected and all actual current bodies pass; no GPU-completion interpretation
of a successful CPU batch submission.

PPD496 now has a source reproducer matching all six fixed error categories:
a generated8×8 texture retains4mips after logical resize to2×2, whose maximum is2.
Native factory and adoption both prefer that stale allocation count; original1eb
bump conversion derives it from current destination dimensions. Old native backing
can then suppress retries because the bump check uses only changed logical size
and nonnull backing. Actual runtime dimensions/identity/recovery remain unknown.
An isolated repair will derive the new render allocation/adoption chain from live
extent and mip policy and compare actual bump backing size for retry. Allocation/
adoption guards and full mipmap generation remain required. No repair is claimed
qualified until root integrates/builds and runs actual native image regressions.


### Complete warning reconciliation and PPD496 repair

The [closed69384 warning audit](warning-runtime-69384-source-audit-20261119.md)
accounts for all395warning rows /46sites, including shutdown.35 emitting whole
bodies are identical to original1eb; all46are unchanged from69377. Emitter
identity does not certify the upstream producer or clear unresolved requests.
Complete source mapping and numeric transport/material/cap limits are retained
without raw asset/account/stream identifiers or payloads.

The lonePPD495failure used the actual J2C/OpenHT wrapper exception path. A fresh
request's ownership/error propagation rules out unrelated stale reason text.
The complete122-byte underlying exception detail is already recorded privately;
longest source-literal/fragment checks, escaped/raw strings and ANSI/whitespace
normalization do not provide a stable pinned-source category. Input bytes,
primary/aux stage and eventual same-request recovery remain unknown. Do not add
more raw error logging or change decoders; prepare bounded failure provenance
and matching input before choosing a repair.

PPD496source19c5d5f327 integrates as97cb7ff705. Shared private requested mip count
now uses current live dimensions and mip policy for both allocation/adoption,
leaving old allocation metadata and global setters unchanged. Both bump lookup
and generation compare the actual native backing extent, permitting retry after
a failed resized replacement. Actual-class/native-generator fixtures pass
4800assertions /3cases under Metal API/GPU validation: shrink/regrow, both
rectangular orientations, NPOT/discard, canceled candidate, old leased-owner
readback, one-level media and incompatible-adoption guards. Exact old factory,
adoption and both expression controls compile0 and each fail2checks/2cases,
including the same mip-count-exceeds-dimensions class. Guaranteed restoration
finally verifies the candidate cpp hash before final build. One prematurely
invoked selector ran the prior test binary before build completion: no tests
matched/exit2; excluded from qualification. The actual post-build run passes.
Root and independent peer source reviews found no actionable concern; media,
dynamic preview inls, GLTF preview and terrain-paintmap adoption owners preserve
their full-chain/one-level contracts. Whole bump-manager runtime recovery is not
proved by these fixtures; owner retest remains pending. Full application build
and expanded image/preview checks are the remaining qualification step.


### Qualified combined texture candidate69388

Source97cb7ff705a17fb2dde73da737230f3cf97d91c4, indra
65fb19d0eafb8471e60c89dfaf634aebf93603b5, version26.4.0.69388 /
CFBundleVersion26.4.0d69388; exact executable SHA256
a3051a443a6afd892e5b73eb5c7c645aa4990d8b7f6de456fd7d6fbbabbbebd8.
Separate stable app `Alchemy Metal Texture Fixes 69388.app` and
`texture-fixes-69388.json` preserve all69384corrections plus003/496.
Full1312-step wrapper Release build0,27selectedCTest/API/GPU0, strict3645files/
0blockers/0scanerrors/925info,346ARM64/deepstrictsignature/3owned-host directimport
checks0. Native3/4800 and all exact old-source controls remain as above. No tool
implementation changed; documentation harness and diffcheck pass. Full command/
log detail is private ppd496-* evidence under the closed69384run; the actual
built SHA remains97cb despite subsequent documentation commits.

Owner explicitly authorizes launch and will manually log in/test. Fresh private
run `Alchemy-texture-69388-gu0cgm5v` is prepared: requested Home only, saved
settings/cache retained, no validation layers/profiler/Metal capture, bounded
read-only LEAP/world/asset/rigging observers. Launch sentinel retains actual
Popen.wait exit status and verifies exact executable path/hash. Requested Home
is not arrival proof, and this is not a paired visual or performance test.
Previous69384/69386 apps remain preserved; no push. Live run.json will record
actual launch and owner/UI observations after this qualification checkpoint.
