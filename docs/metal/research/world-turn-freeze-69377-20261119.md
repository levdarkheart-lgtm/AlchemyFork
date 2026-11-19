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
