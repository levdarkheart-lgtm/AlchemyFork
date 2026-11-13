# HDR ordinary probe publication format — PPD-086

Status: investigating, actual owner-runtime failure reproduced in retained logs.
Registered before a corrective edit. Root owns canonical manager/pipeline/shared
format contract; independent agent owns a new bounded physical regression test.

Owner-candidateSky68965/PID12146 viewer.log (private exposure-comparison artifact)
records at14:46:01 and14:48:01UTC:
`Could not publish native reflection-probe face 5: Metal texture copy requires matching pixel formats`.
There are exactly two matching warnings. Native reset records at14:45:53 and
14:47:59 report no completed default retained. This is actual runtime evidence,
not a speculative source-only candidate or evidence from probe-only post gates.

Current producer ALMetalWorldGBufferRenderer::updateReflectionProbeFace at12738
chooses rg11b10_float forHDR; filterReflectionProbeRadiance at12965 preserves
sourceformat. Manager bootstrap382,publication852,reset2556 still choose
rgba16_float. ALMetalTexture::copyFrom requires identical formats. Need inspect
which publication copy fails and exact generation state before changing code.
Face5 may already set validfaces0x3f before array publication fails; retry
admission expects preceding0x1f, so scheduler can stall rather than recover.

A failed first/default generation can leave black radiance/zeroSH bootstrap.
That is a plausible missing cool environment-light source for owner PPD083,
but exact main boundSH/readiness and corrective owner comparison remain required.
Keep PPD081 pre-existing overexposure and intentional old/new sky-ray differences
separate. No source repair, closure or push at initial registration.


Independent review confirms two downstream admissions also need attention:
world.cpp validReflectionProbes only accepts RGBA16/RGBA8 arrays; add packedR11
for both legacy/PBR actual draws. Material preview accepts only RGBA16 cubes,
so it needs packedR11 for the086 HDR repair; separatePPD087 records its already
unsupported non-HDRRGBA8 input. Test actual draw consumers, not a blit-only fix.
Original1eb manager/components3 radiance allocation resolves toR11G11B10HDR.

Fresh default-publication failure still renders: bootstrap is blackradiance+
zeroSH; refmapCount0 returns coherent bootstrap pointers without generation
checks. Shaderdefault0 with fresh refParams.x0 returns EEPambient, while actual
HDRIambient/reflections are absent. On later resets old CPUparams may persist;
do not claim zeroambient/zeroparam at every owner frame without a readback.
The face5 retry mismatch is source-confirmed:3f afterproducer versus1f required
on retry; rejection is debug-only, explaining quiet warnings after firstcopyfail.


## Physical baseline and selected repair

Actual producer→shared manager-allocation policy physical test confirms HDR
failure: baseline1ae2dfc8ff (behavior-preserving extraction of oldRGBA16 policy),
source/filterR11 versus publicationarrayRGBA16; actualcopyFrom returns exact
owner-log error. SDR copy and real SH projection pass. LLDB27930 child42/state10,
291of292assertions pass, no fault. Frozen baseline source/object/binary/metallib
and failing receipt remain private evidence, not a claimed successful build.

Selected correction b5dfa1b571 makes shared ordinary-radiance policy packedR11
forHDR, keepsRGBA8 forSDR, and uses it at producer/bootstrap/array/default/reset.
World probe-array validator also admits packedR11 (existingRGBA16 remains valid),
otherwise a blit-only fix would fail actual lighting admission. Separate preview
physical baseline087:1166of1170assertions pass, LLDB28271 normal42; R11/SDR both
fail actual preview render while RGBA16, SH reads and invalid inputs behave as
expected. Minimal preview admission fix is being tested in its exclusive tree.

A scoped face-publication guard now restores pre-face admission mask when any
post-face operation fails; only successful face publication commits the newmask.
Realtime failure restarts with fresh irradiance because its next attempt starts
atface0, whereas scheduled updates retry the failedface. Both preserve preceding
published pairs. Physical retry/base-light consumption tests and independent
review remain pending. New default publication milestone is explicitly QUEUED,
not a GPU-completion or owner-colour-parity claim. No push.


Independent review accepts normal encode/filter/copy/commit-failure recovery:
matching SH identities survive a scheduled retry, failed uncommitted publication
commands do not touch preceding pairs, deferred default publication remains
queue-ordered, and the guard reference outlives its scope. SeparatePPD088 records
an already-existing ASYNC SH EXECUTION-error edge: an errored ticket is consumed,
but scheduled radiance cannot regenerate SH. No such GPU error was observed in
owner session; that path has not been repaired or physically injected here.
Do not describe086 as solving every possible GPU failure.

Private fixed-path physical test now passes899assertions/1case (LLDB28536,
normal0/state10, nofault): all6faces/4mips copy byte-identically into slot1 with
slot0 preserved; realSHrow matches projection; actual base lighting admits and
reads published packedR11. Forced actual incompatiblecopy inside scoped guard
restores1f; realface5retry/copy/commit returns3f. Preview fixed47d7991dea passes
2202assertions/2cases (LLDB28466 normal0, API/GPUvalidation); all three supported
formats produce identical blue-only reflection/diffuse results, alpha irrelevant,
invalid sRGB/type rejected. Canonical fullbuild/integrated tests still pending.


Test iteration qualification: added count0bootstrap consumption control initially
asserted direct sunlight while its fixture left the sunlight multiplier at0.
The renderer accepted packedR11, but expected nonzero pixels correctly failed;
this is a fixture setup error, not another product cause. Retain that failed
attempt with the corrected test evidence; prior899publication/SH/consumer/retry
checks remain separately green. Canonical integrated result is still pending.


Source history pins the format disagreement before today's sky work:
2066d5c507ce9ecc93fbefcf58130ec66af8e483 (2026-11-11, floating-point world light)
introduced ordinary probe rg11b10_float while manager/consumer admissions remained
RGBA16. That explains why the failure can predate latest sky changes; it does
not independently prove every broader exposure impression PPD081 shares it.


## Integrated result / owner handoff

Source b81a4645e0f6acccd9c860da25cd1d05302e11f4, signed Sky26.4.0.68980,
executable SHA2568118f71cf6af9216a3f7c8532f4e9b22239d3e1ab38ca4a08cc847d86a6db779.
Root integrates test91a2/b37f, preview47d7991dea, guarddb25 and manager/CMakeb81;
format/consumer fixb5dfa1b571, shared allocation baseline1ae retained in history.
No blanket tonemap/exposure/gamma/EEP/default or sky-ray alteration.

Exact root commands, all exit0 (private logs under colour-cause artifact):
- `tools/metal/doctor.sh`.
- `AL_METAL_PRESET=ninja-os-arm64 AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 AL_METAL_JOBS=4 tools/metal/build.sh build`.
- `tools/metal/verify_app.sh <exactSkyapp>`:346ARM64 MachO slices.
- `codesign --verify --deep --strict <exactSkyapp>`.
- Owned viewer/CEF/VLC `otool -L` and `nm -u`:no forbidden graphics imports;
  this bounded owned-import check does not audit every foreign CEF subprocess.
- `python3 tools/metal/audit_gl.py --strict --output <privateReport>`:
  3540files,0blockers,0scanerrors. Compatibility names remain informational.
- Canonical almetal_tests underLLDB, MTL_DEBUG_LAYER1+MTL_SHADER_VALIDATION1:
  [probe-hdr-publication],[probe-formats] →3903assertions/3cases, PID32384
  child0/state10; related original ordinary-probe, material-preview,
  reflection-policy and SSR-isolation →4504assertions/6cases,PID41119child0.
  Actual API/GPUvalidation enabled lines retained; no fault resumed or reported.
- Harness33requiredfiles passes after docs changes. No tool source changed.

The strengthened physical test verifies SH READS, not just acceptance: known
sunRGB.125/.25/.5 remains in zero-probe bootstrap, while completed HDR pack gives
.247925/.495850/.983887; SDR .250488/.500977/.997559. These are controlled fixture
pixels, not owner scene measurements or a universal gain correction. All six
faces/fourmips publish byte-identically, SHrow matches actual GPU projector,
untouchedslot0 preserved, failedcopy restores1f and successfulrealretry commits3f.
Separate glossy-radiance preview uses all three formats and actualSHdata.

Ready_for_owner086/087;083stillinvestigating until fresh owner image comparison.
Owner logged out; no viewer relaunched. New private68980QAconfig pins new binary,
preserving agreed controls/destination and old immutable run receipts; installed
QA launch dryrun exits0, but this is NOT login/location/render gate evidence.
Next live test must observe/verify/gate again and confirm actual probe publication.
Sky shown in prior images remains exact68965, not this new executable. All local;
no owner closure, performance claim or push permission inferred.088asyncSHerror,
085extreme-sun cap and082transition concerns remain independently open.


Owner handoff completed: exact68980/PID41725 launched16:21:28 and is leftopen,
locationSpareTime105/21/24 verified live and in suppliedimagebar. 46livecontrols
pass unchanged QAverification. Actual16:22:06 defaultHDR R11 radiance/SH queued
milestone observed; oldcopyerrors absent in boundedreview. Owner says newcolour
looks fixed; authoritative clean16:28:21 same-material comparison reducesRGBmean
error94.21%, all15means within1/255 oforiginal. PPD086 and reported083 are owner-
verified for this tested failure; fullpixel/scene/performance parity notclaimed.
087previewowneracceptance remains pending;088error-ticket edge separatelyopen.
No push approval. See neutral-colour note for exactimages/hashes/measurements.


2026-11-13 publication authorization: owner confirms the colour issue resolved
and explicitly approves pushing the ENTIRE committed upstream-port series,
including this work and its tests/records. [Publication scope](upstream-port-publication-20261113.md).
Earlier local-only statements are historical; unverified/partial defect status
and independent release/performance gates remain unchanged. No future unrelated
fixes, raw captures or release artifacts are included in this approval.
