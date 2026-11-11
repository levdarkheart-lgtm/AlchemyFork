# Native settings and retained resource tail — 2026-11-11

Source checkpoint `83567e407d` on
`codex/apple-silicon-metal`. Original behavior is pinned to
`1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`. This extends
[finalization/preview/media closure](native-finalization-and-media-closure.md).
The goal remains active. No viewer, utility, real media host or native window
has run at this checkpoint; no screenshots. The owner subsequently authorizes
launching the exact fresh viewer after checks for their manual confirmation.
CPU facades and device-only tests have bounded scope.

## Retired phase descriptions

Integrated `d78cc2d779` from child `37cce80e7c` removes exactly 34 identities:
PostPing/PostPong and seven Bloom levels from each of three packs; SMAA edges
and blend weights; three Glow targets; Preview and Bake. Both retained allocator
entry points already skipped these descriptions. Their actual Metal owners
remain unchanged; this removes metadata rather than feature allocations.

Original `alrenderresourceplan.cpp:174-199,276-287,320-335` and the corresponding
original finalizer/preview/bake paths establish their former duties. Native
post/Bloom/SMAA/glow/preview/bake transactions own those duties now. Keep the
actual native target pool, shared BloomShape math, PREVIEW_SIZE, all native
settings, physical pack/shadow/noise plans and MainGBuffer history/scope keys.
Only the always-false material-preview fallback input and two bake-size copies
became dead with these descriptions; they are removed too.

The plan now has 25 IDs and six pack slots. Actual compiled old/new Plan
operations over 9,216 configurations produced identical 239,616 named physical
records: 11,437,516 bytes, SHA256
`7cd3ae249b0af1f5acdf631745e32262d991de98e2f8466e995723ea04075e1b`.
Descriptors, hashes, ownership, changed bits, manager keys, SSR/refraction/cascade
intent and classifications are covered. Six authorized whole-pipeline
substitutions preserve every other byte, including native finalization,
size overrides, history, settings publication and retry/reset bodies.

Full input equality still schedules reconciliation and publishes native-only
settings even when no physical descriptor changes. The new actual Application/
Plan case covers ten such policies under HDR/SDR; a deliberately wrong
descriptor-only scheduler fails 40 of its 120 checks. This is a source mutation
control, not an original viewer run. Six removed cases tested only displaced
phase metadata. Existing native GPU coverage is retained.

Child six Release checks and 1,898 checks/41 CPU cases passed. Independent
source peer pinned all six hashes, whole-file/enum projections and both physical
dump bytes. Receipts:
`/private/tmp/alchemy-native-phase-retirement-validation-20261111/`.

Integrated `c65e5d588e` from child `3aa664f716` separately removes unused
Plan::describes declaration, definition and sole helper test: three exact spans,
24 deletions. Original `pipeline.cpp:10320` used this only to retain a pooled DoF
blur when it matched another planned allocation; that complete GL leaf is gone.
No current production reader remained. Child five Release checks and 1,894/40
CPU cases pass. No native DoF or pool owner changed. Receipt:
`/private/tmp/alchemy-native-plan-describes-validation-20261111/`.

Matching canonical builds for resource/policy targets and then the tiny helper
follow-up exited 0. Root registered resource/input tests pass 1,894/40; native
finalization/lens/reflection policy tests pass 652/20. These also admit the
previously integrated legacy cutout retirement. Commands, source/WIP state,
binary hashes and logs are in `artifacts/metal/feature-completion-20261111/`:
`native-phase-policy-registered-build.json`,
`native-plan-helper-registered-build.json`,
`native-phase-resource-registered-validation.json` and
`native-cutout-legacy-policy-registered-validation.json`.

## Snapshot allocation-failure control

Integrated `4f5f23251b` restores SimulateFBOFailure at direct native snapshot
size admission. Original `pipeline.cpp:862-877,1282-1344` rebuilt requested
snapshot resources; forced failure exhausted retries and declined that size.
Original `llviewerwindow.cpp:5730-5769` reset the failed scope, rejected cutouts
and retained ordinary RGB tiling. Current callers retain those recovery duties.

Native capture owns its targets directly, so the fix rejects the simulated
failure after beginning the request scope and clearing native dimensions. It
does not rebuild GL targets or use mFullSize as an availability guard: reduced
successful allocations also have mFullSize=false. The original same-request
retained full-size cache could bypass rebuilding and therefore the simulation;
native direct admission has no corresponding retained-target cache exception.
The debug comment now describes allocation failure rather than a removed method.

Actual native begin/end, ALRenderSizeOverride constructor/destructor and six
request methods compile in a standalone CPU probe. Device/window/settings are
facades, with no native device/window initialized. Across simulation off/on/off,
resolution divisor/multiplier, invalid extents and missing device/window cases:
184 checks expose 16 baseline failures and pass after the fix. RAII cleanup,
deferred window/program/input requests and normal exact dimensions are checked.
Production Release textual syntax exits 0; independent peer pins both changed
file hashes and the exact 12-line insertion/one XML comment projection.
`artifacts/metal/feature-completion-20261111/native-snapshot-failure/validation-receipt.json`.

This proves the snapshot admission boundary. Ordinary exhausted-plan native
pooled fallback remains a separate candidate requiring stronger evidence;
neither an ordinary-frame failure policy nor actual snapshot execution is
claimed by this probe.

## Remaining source audit and implementation

The read-only Debug/Developer audit surveys 203 Develop leaves and 246 declared
graphics/debug literal-key candidates. It finds an original-active water
normal-filter option missing from native rendering: original AnisoWrap versus
PointWrap for both normals, formerly native always filtered. Integrated6473c5c77d
from child35c8d0778a restores the setting reader, a default-true scene flag and
the common fragment sampler0 selection. Existing point-repeat state matches
nearest min/mag, no mips and repeat. Height-map vertex sampling, all MSL and
normal texture ownership/bytes stay unchanged. Root registers the new focused
test in almetal_tests. Common binding covers above/below and flat/wave variants;
the new actual-owner fixture directly exercises underwater rendering.

Valid same-test unchanged-world baseline389 checks/2cases has17 failures;
the corrected binding passes389/2 with API/shader validation. Independent
all-levelA/B normal renders discriminate mip selection under HDR/Z for both
normal inputs; a no-mip checker independently discriminates magnification.
Off/on recovery, input texture identity/bytes and source color remain checked.
An earlier missing-haze fixture rejection is retained as a setup failure, not a
product defect. Caller Release syntax passes; source peer pins all four hashes.
`/private/tmp/alchemy-native-water-normal-filter-validation-20261111/validation-receipt.json`.
`/private/tmp/alchemy-native-developer-remaining-audit-20261111/audit-receipt.json`.
Literal-token coverage does not prove exhaustive dynamic setting resolution.

The RLVa source audit pins nine original-identical command/action/helper/
environment files and eleven ordered overlay/policy bodies. No new confirmed
native wiring omission was found in that bounded overlay/touch/tween, forced
diffuse, visibility/text, debug-restriction and environment subset. Current
environment updates own pack refresh before native consumers; getters alone
are not claimed to refresh. Actual command/reply/touch/timing/multi-effect/
restart behavior remains unexecuted; RLVa parity stays unverified.
`/private/tmp/alchemy-rlva-render-closure-audit-20261111.json`.

Integrated7f6633a3fe from childbf4f4d9319 removes the unreachable preview=true
impostor branch/API flag and sole far-quad helper. Original morph was its only
true producer; the actual native preview now uses prepareMetalAvatarPreview and
ALMetalLitPreviewRenderer. Default ordinary and explicit profile callers retain
their former false specialization. Executable-token projection including
literals is exact; native preparation/bake/stamp/publication bodies remain
byte-identical. Pipeline/avatar/morph Release syntax passes; source peer pins
both hashes. No mirrored test or unchanged GPU rerun was added for dead code.
`/private/tmp/alchemy-native-impostor-preview-tail-validation-20261111/validation-receipt.json`.
Integratedce65f508a4 from childdb58435309 separately removes the original-
identical, uncalled hero renderDebug wrapper/declaration (17 exact deletions).
Tracked-source census, two Release checks and source peer pass. Keep gDebugProgram,
shared ordinary/native probe links, edit/joint/debug and hero resource owners.
`/private/tmp/alchemy-native-hero-debug-retirement-validation-20261111/validation-receipt.json`.

## Optional Compute policy and redundant copy

Original shader-local Compute selected exactly two compute families: SSAO blur
(original manager2123/pipeline11759, with fragment fallback) and optional water
depth pyramid/culling (manager3296/water213, waves still render). Native SSAO
already uses fragment blur axes. Native water had lost the policy gate.
Integrated83567e407d from child1bcd83a254 adds LLRender::sCompute to the existing
main-view/occlusion-cull condition. The original real setting-to-shader-local
derivation and setShaders latch remain intact; the wave producer and unchanged
native owner receive false when the optimization is disabled. Waves remain.

Actual extracted setting-read/latch/caller code compiles with real control-group,
ShaderInputs and LLRender state: 64 combinations/352 checks expose6 baseline
flag failures, then pass. The named wave receiver observes forwarding only;
this is not geometry emulation or a GPU proof. Caller Release syntax passes;
World/header/MSL/fixtures remain byte-identical to the normal-filter checkpoint.
`/private/tmp/alchemy-native-water-compute-policy-validation-20261111/validation-receipt.json`.
The debug comment now explains optional water culling and independent native
exposure/BRDF/probe compute; it makes no all-native-compute-off promise.

Integrated2c16d75147 from child2827c07e3b removes only the redundant resource
Inputs::mCompute member/producer and three obsolete plan-context rows. Original
Plan294 used this copy solely for the retired MainLightBlurred allocation.
Actual shader-local state, setting callback/latch, water gate and full-input
publication remain. A new actual Application case requires shader policy reload
without unnecessary resource reconciliation. Six Release checks and1893/41 CPU
controls pass; the same9,216-config physical-plan dump is still byte-identical.
Independent source peer passes; root matching dependent rebuild remains.
`/private/tmp/alchemy-native-resource-compute-copy-validation-20261111/validation-receipt.json`.

## Complete-product advancement and next action

These changes advance the requested dead-code and Debug feature closure through
preserved resource ownership and restored native failure admission. They do not
establish complete menu/RLVa/runtime parity. The latest full application remains
the earlier ae2 source-pinned build; combined full app/static verification and
optional appearance utility refresh follow this frozen combined closure.
Broader feature, foreign dependency/runtime, prolonged stability, performance
and independent release gates remain open. After build/static/affected checks,
launch the exact fresh viewer for the owner's requested manual confirmation.
No screenshot comparison is requested; the utility is not to be executed.
