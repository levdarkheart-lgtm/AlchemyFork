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
