# BoM overlay camera-motion rendering report — 2026-11-21

PPD-522 records the owner's reported camera-dependent polygon-shaped patches on
a mesh attachment's BoM overlay. Screenshot is private and is not committed.
No root cause or fix is established. Keep source, surface topology, depth,
material/alpha inputs and pass ordering separate from the visual hypothesis.

The existing viewer5199 is exact owner-tested69404: source
7d607c955ab8b027a745bf5406b537f0640d99bb, indra tree
6cfca54e0398bb26d97ac33c39941a3ead123512, executable SHA256
1a8758659155dfe6cb9015cc41b5184d2f12f85827cf8c2ff5ffcf7c90b2e068.
Actual startup log says OpenHTJ2K0.27.0 decode and one caller thread per image;
OpenJPEG header/encode support is distinct and no pixel fallback is enabled.

Private root-workspace evidence stays in ignored
artifacts/metal/rendering-issue-20261121: identity.json, doctor.log, initial
record command/receipt/log, failed trace and export log. Owner moves camera for
a requested3s Metal System Trace. It reaches its recording limit, then emits
many already-known DWARF symbol-analysis messages. The55s finalization bound
signals only the recorder; SIGINT then KILL ends recorder-9 at63.259s, trace
712839904bytes. TOC export exits10 Document Missing Template. Retain as failed;
file existence/size and requested duration do not qualify captured GPU coverage.
The viewer was not signalled by the recorder wrapper.

The shipped controller reads capture environment only at initialization; the
ordinary existing session cannot arm GPUTraceDocument in place. Owner explicitly
agrees to normal logout and restart of the same build, confirmsclosed. Root
launches exact unchanged69404 at00:38:29UTC, newPID7831, AutoLoginfalse and a
separate private log. Existing AlchemyNext cache/account/settings retained.
MTL_CAPTURE_ENABLED1, private diagnostic directory, quiet auto mode, late60s
and256MiB telemetry budget are the only capture additions. This existing method
captures an initial baseline and a later complete-device display interval; it
is not a newly invented owner-cue one-shot trigger. Owner is asked to log in and
frame the affected view before the later capture. Actual log confirms decoder
and armed diagnostics; captures pending qualification. No other recorder,
profile, build, replay or graphics-setting change is started concurrently.

Next: observe capture metadata/state and complete trace bundle; keep wrong-view
baseline distinct. Inspect the affected-frame final output, draw/depth/material
inputs and original source contracts before proposing a repair. Register all
failed/replacement captures and preserve raw local evidence; do not upload it.

Alpha7 release task remains in progress administratively: hosted44626887693 and
publication452061721 succeeded on2026-11-19, but interrupted independent local
verification and final records still need completion. Its published source/tag
must not be changed by this new investigation. See the separate release ledger.

## Qualified capture and offline investigation

Both startup-armed GPUTraceDocument intervals stopped without error. Early
frames162–163 contain7377files/3569811793bytes; late1327–1328 contain19429files/
13489512409bytes. The late capture was cloned with APFS clone into a private
working copy, opened in Xcode26.6 and visibly reproduces the affected overlay.
Twelve command buffers,36render encoders,5compute and5blit encoders,3239draws,
5dispatches and3456x2168output qualify a complete-display frame. No FPS claim.
Replay was stopped after export; the owner stays logged in as requested.

Draw16866 uses almetal_world_legacy_alpha_skinned_vertex with legacy-material
alpha High-shadow pipeline, reverse-Z GreaterEqual depth and depth writes.
Exported diffuse1024RGBA8-sRGB, vertex174064bytes/index21144bytes (10572UInt16),
frame/palette each1048576bytes preserve the selected binding inputs. The index
view is the used range, not the whole underlying32KiB allocation. Vertex data
contains1892slots/1889referenced vertices. Palette origin is182.447189,30.125402,
27.968327 with10joints. Texture alpha is1–255; visual inspection alone does not
establish texture corruption. Earlier draw16486 diffuse is unrelated hair.
No same10572-index opaque draw was found: competing surface identity remains
unproved. Do not infer identical geometry from proximity or count alone.

PPD-523 records a separately source-confirmed precision contract divergence.
Original1eb44 objectSkinV.glsl29–57/109–124 deliberately transforms avatar-local
coordinates and origin separately; original diffuseV59–61, materialV83–87,
alphaV82–86 and fullbrightShinyV66–67 use split modelview then projection.
Current Metal simple/material combine origin first; shiny/legacyalpha split
MVP local/origin. Current fast-math and absent position-invariance options do
not guarantee equal cross-family depth. CPU explicit-F32 input experiment is
only a diagnostic: GPU compilation/rasterization and incident causality remain
to be established. No depth bias, alpha-mode or sorting workaround is selected.


## Local repair and discriminating GPU controls

Root exclusively owns shader/compiler changes in persistent
Alchemy-worktrees/bom-overlay-depth-20261121, codex/bom-overlay-depth-20261121.
Separate test-only worktree rigged-depth-test-20261121 supplies reviewed fixture
2fa6ce0da0be109a64f595e49678bad0df98f046 (integrated9c3581e513). The fixture loads
the actual offline production metallib and changes only a constant diagnostic
fragment; independent double-precision authored geometry checks screen coverage.
Nine actual vertex families x3synthetic origins x3camera orientations exercise
162ordered coplanar overlays plus genuine front/behind/read-only depth controls.
No owner geometry, texture or camera is included in committed tests.

On actual M1Max the SAME fixture executable,765checks each, reports:
unchanged69404 library35fail (13depth-bit/22coplanar visibility failures),
shared-transform-only30fail, invariance-only80fail, both changes0fail.
All independent coverage/front-behind/read-only controls pass in all4arms;
comparisons require exact32-bit depth equality, with no ULP tolerance. Native
workloads each take~0.4s; this is correctness evidence, not viewer performance.
Original library SHAbf2a3bb4d977bd1768c3b62f6cfdd207c3dae03e47b973d2b4a167d72a91210c,
first candidate SHAecb548373845a761efaed891dd0b01ceadc684b4e23af1f7df9468cc8cef45a3.
Raw private control sources, receipts/logs live in test worktree
artifacts/metal/rigged-depth-20261121; fixture source independently reviewed.

The correction keeps avatar-local and large-origin transforms separate through
one helper, marks compatible position outputs invariant and enables preservation
for offline and runtime compilation. No layout, normal/tangent, material/alpha,
depth comparison/write policy, pass sorting or bias change. Independent review
found2retained animated material/PBR entrypoints still combined; root routes
those through the same helper before full candidate qualification. Existing
animated parity checks must cover this correction. Seventeen world entrypoints
now use the helper. Selection/HUD/preview targets remain separately owned;
this is not a blanket assertion about every rigged pipeline.

Neighbor16844 uses a different origin and25joints, not the affected draw's10.
Its buffers were exported for comparison. Requested numeric depth export saved
PNG, so retain it as visual evidence only; do not decode it as F32/KTX. Earlier
CPU calculation included3unused padded vertex slots with zero weights; its
fallback handling was corrected, referenced1889vertices remain finite. CPU
F32 diagnostic shows495depths behind/1358ahead/36equal, delta-86..145ULP for
the two formulas; actual GPU regression above is the decisive mechanism test.

Next: run repository build, focused GPU/animated tests, static app/decoder checks
and source-pinned candidate identity; keep owner69404 session untouched until
normal logout. Owner must test the exact new candidate at the same view before
closing PPD-522/523 or approving any new fix push. No private capture upload.


## Full candidate qualification in progress

Canonical integration is now the primary workspace, codex/apple-silicon-metal,
tracked-clean source8f2e56b5dbeb1eb0ee7e1dc800bf62645ceb6bcc/revision69417.
The separate shader worktree preserves241f486943. No proposed fix is pushed;
public Alpha7 remains immutablebd930/69412. Actual owner69404 PID7831 exited
normally0 at00:55:26.306879Z; no process was killed or account changed.

PPD-524 retains the first documentation-harness failure (two copied links);
corrected links pass33files. PPD-525 retains initial project configure failure
because the new standalone fixture omitted required al::flags. Linking that
mandatory target resolves configure. First configured Python lacked llsd/yaml;
use the existing isolated known tool-venv via explicit Python3_EXECUTABLE,
restoring Python peer tests and LL compiler without installing global packages.
configure-03 exits0 and verifies required OpenHT0.27.0/pinned72ea0450 archive.
The final build-generated shader library SHA256
e69a532f059290fa2bd7cc8e262fe68dc5fd9bae3075ca66df3e59f581365eab
includes both animated-entry corrections; SAME765check fixture reports0fail.
Firstcandidate765/0 therefore is corroborated by final-library765/0, not reused
as evidence for changed shader inputs. Broad animated/world checks stillpending.

Full repository tools/metal/build.sh Release is running against configured
build-Darwin-ninja-os-arm64-shadow-occlusion with a distinct app channel
Alchemy Metal Rigged Depth Fix. Initial4job build is deliberately interrupted
with SIGINT to the uniquely verified Ninja child after the owner session exits;
exit130/log retained, no compiler-error claim. It resumes8jobs reusing completed
objects. Do not treat an incomplete staged app as the source-pinned candidate.
App/static/signature/source/decoder qualification and owner retest remainpending.


## Captured-session warning accounting (separate from the depth defect)

Closed source7d607 session log is4351031bytes/SHA256
05fe7a657efcb1a09df57404e7a1e86e61fcdfa25140a8bc7b02a38401b1af79:
261WARNING rows/28sites,0ERROR/FATAL. A literal fallback+rigging search returns
230INFO rows, but all are pipeline.cpp3325 census (115main/115probe), every
fallback counter0 and last_fallback=none. These are schema labels, not230rigging
failures. No skin/palette/nonfinite, decoder-failure or missing-auxiliary-mask
warning row occurs in this session. Absence is not blanket rigging correctness.

Seventeen distinct textures reach terminal missing-asset handling with stored
HTTP403 status01930001,9material assets fail with status-1, and117coreHTTP
Easy_28 timeouts have0retries with request ownership unknown. Four inherited
alpha-analysis warnings use float0x1406;30stale appearance updates are rejected.
Corresponding warning/failure branches are inherited from original1eb44, and
there are no WarningInputs records providing typed request/bake context here.
Do not join recycled HTTP handles or connect these failures to the affected
attachment without an object/face-to-texture mapping. PPD-311/466 and existing
asset-transport causes remain separate/open; no new decoder or rigging cause
for PPD-522 is established. The selected draw's diffuse is validRGBA8 in the
GPU capture. Private warning-sites.json/shutdown-log-summary.json and source
review retain the accounting without committing account/asset identifiers.


## Ready for owner retest —69417

Full tools/metal/build.sh Release resumes and exits0. Source
8f2e56b5dbeb1eb0ee7e1dc800bf62645ceb6bcc, indra tree
05091a15a707be1694ac37495414c42d90cbe951, version26.4.0.69417.
Exact app is primary workspace build-Darwin-ninja-os-arm64-shadow-occlusion/
newview/Release/Alchemy Metal Rigged Depth Fix.app; executable same name under
Contents/MacOS. Executable SHA256
0b8bab95fd3da4312029734a3b1c0a9a12b3b76d0227bd99d97eb0ce9ae1e1f1
contains the full built source hash. Bundled metallib exactly equals tested
build library e69a532f059290fa2bd7cc8e262fe68dc5fd9bae3075ca66df3e59f581365eab.
tools/metal/verify_app.sh346ARM64/0invalid and strict codesign both exit0.
Strict source gate3647first-party files/0blocking/0scanerrors (925informational).

Thirteen focused CTests pass in37.41s: classic avatar, broad native GPU/world,
resource/direct routing, compact draws, runtime shader reload, shadow material/
coverage, pipeline cache, attachment palettes/animated material+PBR, G-buffer
constants, shared library and animated pipeline policy, plus new765check test.
Broad GPU fixture passes131939assertions/65cases. All comparisons use the newly
built executable/library, not the preceding old test bundle. Two opt-in cache
cases are skipped under ordinary settings, then explicitly run under required
shader-validation environments:882 and98assertions, both pass. These settings
are subprocess-only and never passed to the owner candidate.

Owner candidate launched PID39451 at02:00:11Z through exact verified executable,
manual AutoLoginfalse and separate private log only. No capture, experiment,
graphics-preset, region, cache or account change. Startup independently confirms
version69417, OpenHTJ2K0.27.0 decoding/one caller thread per image and
STATE_LOGIN_WAIT; no ERROR/FATAL observed before login. lsof confirms the live
process opens /Users/dekstroza/Library/Caches/AlchemyNext/texturecache/FastCache.cache
(the quiet startup log does not itself print that path). Mac is locked; native
UI inspection cannot unlock it, and owner is asked to unlock manually. No scene
screenshot, graphics/location verification or owner visual acceptance claimed.

PPD-523/525 ready_for_owner; PPD-522 still investigating until actual attachment
camera-motion retest. All failed
traces, partial4job build, first helper-only/invariance-only failures and two
harness/configure errors remain retained. Proposed fixes and checkpoint commits
are LOCAL, no push; public Alpha7/69412 is untouched. Next: after owner unlocks,
verify exact running69417, let owner login/reproduce, inspect current log, and
record whether the overlay remains complete while rotating the camera. Only a
confirmed owner result can close the incident or support a requested fix push.
