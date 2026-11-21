# Moving shoreline water line —2026-11-21

PPD-526 records the owner's new69417 report: animated water appears to cross
land/sand at the shoreline, apparently unlike the normal viewer. Actual region,
coordinates, screenshot and logs remain private under ignored
artifacts/metal/water-land-line-20261121. Identify terrain versus object land
from capture rather than assume it. No root cause or correction established.

Exact executable is primary workspace build-Darwin-ninja-os-arm64-shadow-occlusion/
newview/Release/Alchemy Metal Rigged Depth Fix.app/Contents/MacOS/Alchemy Metal
Rigged Depth Fix; version26.4.0.69417, source8f2e56b5dbeb1eb0ee7e1dc800bf62645ceb6bcc,
indra05091a15a707be1694ac37495414c42d90cbe951, executable SHA256
0b8bab95fd3da4312029734a3b1c0a9a12b3b76d0227bd99d97eb0ce9ae1e1f1.
The normal open launch PID43448 starts10:11:12Z, its actual log confirms
OpenHTJ2K0.27.0 decode/one caller thread per image. Required capture environment
is absent. This binary reads diagnostic/capture configuration once at startup;
framecapture.mm requires MTL_CAPTURE_ENABLED1 at process launch.

Owner explicitly requests another trace, accepts the required same-build restart,
and confirms logged out/closed. PID43448 is absent; copied initial-viewer.log
ends Goodbye/status stopped10:28:34Z. LaunchServices exit status is not available;
do not invent exit0. Screenshot owner-shoreline-report.png and initial-identity.json
are private. Earlier bounded SystemTrace failed finalization for the BoM issue;
do not repeat that unusable attach path when a replayable frame is required.

Use the existing automatic early and late60s complete-device display captures,
with separate owned mode0700 diagnostics. No forced graphics/environment/region
or cache/account change. Owner manually logs in and returns to the shoreline
view. Retain early wrong/loading-view capture as baseline, independently qualify
late affected view. The API stop/error0 and nonzero file size alone do not prove
useful contents; replay to inspect water/land depth, coverage and materials.
Keep user control of camera/login/logout; do not silently rearm elsewhere.

Verified PPD-013 homogeneous haze/SSR-history correction and PPD-497 remain
accepted, open PPD-015 eye/agent vertical is a separate source candidate, and
accepted PPD-522/523 BoM fix is retained. No attribution to any existing ID
before new evidence. Read-only independent original/native water audit proceeds
in parallel; root owns all source/docs. New proposed fixes must remain local
until owner testing and explicit push approval. Existing Alpha8 workflow remains
pinned9ef1da4d37; this report does not authorize altering or cancelling it.

## Captured and inspected

Same executable PID44179 starts10:31:01Z with only the documented capture and
diagnostic options, sole OpenHTJ2K0.27.0 verified again. Both complete-device
display intervals stop with native_error0: early264–265,4030files/3913005540bytes;
late2157–2158,4483files/4215959793bytes. Early contents remain unqualified.
An APFS clone of the late capture replays in Xcode and visibly reproduces the
curving wet/dry-looking shoreline. Its summary has19commandbuffers,42render,
10compute and10blit encoders,2774draws/10dispatches,3456x2168. These are capture
contents, not performance measurements. Owner is told normal logout is safe;
the child actually exits0 at10:36:28.767636Z. Original capture bundles retained.

Actual tessellated above-water draw17023 uses the wave vertex/above-water
fragment entry points. Its reverse-Z state is GreaterEqual/depthWriteYES;
stencil uses exclusion read mask0x01 and surface write mask0x02. The1MiB frame
buffer, pre-water refraction color/depth and post-wave color/depth are exported;
the last flat/fringe draw17032's depth is exported separately. All remain
private. A first raw combined depth-stencil KTX export contains stencil ONLY;
retain it as a failed numeric-depth attempt. Correct ImageViewer Depth-aspect
exports are3456x2168 FLOAT32 KTX,29970528bytes each. Never analyse the stencil
file as depth. Xcode replay is explicitly stopped; Replay/Finished running is
observed before offline analysis continues.

Independent captured-constant/depth reconstruction finds current-cell base
height20m, camera4.6428m above it, wave-height dial0.5, hextile1, density32,
reach399.973m and conservative displacement bound0.121883m. Vertex/fragment
constant packs match byte-for-byte. Wave draw changes3681292depth pixels, all
closer under reverse-Z. Of3404236 pairs with finite pre-water geometry, the
highest replaced geometry is20.029452m: about2.95cm above the static plane.
Resulting water spans19.949470–20.045352m, within the captured displacement
bound. No replaced current-cell geometry is more than5cm above the plane.
48324replaced pixels lie above the base plane. Near-flat beaches can therefore
show a large horizontal shoreline movement from a small vertical crest.

Color comparison changes3681293RGB pixels;3681282 also change depth, with only
11RGB-only and10depth-only pixels. The curved edge is actual water geometry
coverage, not a broad color-only haze/refraction/history overlay. The four
subsequent flat/fringe draws change77062additional pixels, all previously far
depth0, confined to raw rows2124–2155 outside the principal foreground shoreline.
They replace no existing finite water/ground depth. Analytical images are
vertical-flipped for the renderer's source-row contract and visually checked
against the recognizable captured scene; raw KTXs remain unchanged. Exact
opaque sand draw type is not yet identified. No paired reference runtime or
waves-off owner test has occurred; do not infer full port correctness.

## Original contract and clarified comparison

Original GL1eb44bb includes real displaced waves, not just animated shading.
Original ALWaterStencil::Surface tests exclusion bit0 and writes surface bit1;
the haze pass writes neither depth nor stencil. There is no discovered static
shoreline admission mask omitted by Metal. Its SHORELINE_FADE shader branch
has no active definition in the inspected original source. PPD015's separate
eye-space/agent-space steepening defect can affect shading/refraction; it is
not established as this incident's cause and cannot account for the measured
geometry displacement alone. Layout-eye and true-eye match in this capture,
independently of that separate defect. Retain its open status.

History independently establishes waves were upstream functionality before the
fork:17160804c498f31cb315b3ec861b504f64ba849e introduces displacement,
a95ea0ca7f4244c3226933e81ed7243cc9b96525 enables it by default, and
ed543fd9e520bd2e77c67e4e16a1f7330d1c2f75 disables it in Low/LowMid. These are
ancestors of fork baseline65c39b7bad and inspected upstream/develop408fd6c014.
They are not Metal-only experiments. GL shader compilation failure or settings
can leave the original flat, but no such runtime failure was observed here.

Owner clarifies the fixed-line comparison is **Firestorm or another viewer**,
not a verified same-build Alchemy reference. Official Firestorm release source
[waterV.glsl](https://github.com/FirestormViewer/phoenix-firestorm/blob/Firestorm_7.1.13/indra/newview/app_settings/shaders/class1/environment/waterV.glsl)
transforms the supplied flat positions unchanged; waves animate shading
coordinates. Its [water geometry](https://github.com/FirestormViewer/phoenix-firestorm/blob/Firestorm_7.1.13/indra/newview/llvowater.cpp)
builds planar quads with a shared height. This supports a feature/settings
difference, not proof about every Firestorm version or the owner's exact build.

Existing Lightbox **Water waves** control binds AlchemyRenderWaterWaves.
Source traces setting changes through render-input invalidation, ALWaterSurface
and LLDrawPoolWater into the native flat pipeline when disabled, retaining
water shading/refraction. Next discriminating owner comparison: same location,
camera and environment, turn this existing control off and check whether the
advancing/retreating shoreline disappears. No setting was changed automatically,
no feature disabled permanently and no renderer correction is selected.
PPD526 remains investigating pending that comparison/owner acceptance.

PPD527 separately records stale OpenGL4.0 requirement text in this control's
tooltip and setting comment; the active implementation uses native Metal.
No tooltip repair or new build is claimed in this investigation checkpoint.

## Reproducibility and limits

Private analysis/exports are under artifacts/metal/water-land-line-20261121;
export-manifest.json pins sizes/SHA256. Initial capture receipt is updated to
qualify late replay only. Independent source, actual constants, depth statistics
and analytic images are retained in the private sibling
Alchemy-ppd526-source-review-20261121; the history audit has its own sibling
Alchemy-ppd526-reference-history-20261121. Raw scene identities, geometry and
images are not committed or uploaded. No engine edit/build, account/cache
change, performance improvement or incident closure is claimed.

Documentation validation: existing isolated Python runs
tools/metal/validate_harness.py, exit0/33required files; git diff --check0.
CURRENT/active ledger retain their160/180-line caps. No source/tool edits mean
no new build, native-GPU tests or harness unit tests are claimed or necessary.

Independent read-only checkpoint review passes source/history/control gates,
quoted private numeric/exit facts, no private SL identifiers, resolved links and
line caps. Keep pre-water geometry type unknown; do not relabel it terrain.


## Owner comparison launch

Owner explicitly requests another launch for the existing-setting comparison.
Same69417/exe0b8bab95fd3d starts asPID48466; window title independently inspected,
process path/hash verified, startup log confirms sole OpenHTJ2K0.27 and reaches
STATE_STARTED11:12:48Z. Launch uses only private --logfile and AutoLogin=false;
child capture/diagnostic environment is cleared. No forced location, graphics,
cache/profile or water setting change. Owned launcher session21857 retains
normal shutdown receipt when it occurs; viewer remains running at handoff.
Private owner-comparison-01 contains launch/runtime records and logs.

Source-confirmed navigation is Me > Lightbox... > Scene >
Performance - Advanced > Water waves. Owner should compare the same shoreline,
first with the existing state, then unchecked, then checked again; keep adjacent
Water anti-repeat/Wave height unchanged. Actual region/profile and active wave
value are not independently attested yet. No comparison screenshot or visual
pass is claimed. PPD526 remains investigating/pending; no new code or push.


## Owner waves-off result

Owner replies "yeah that makes is stable" to the guided existing Water waves
comparison. Record this as owner-reported stable shoreline with displacement
off in the launched69417. No independent live value/profile read, new capture,
paired screenshot gate, or separate waves-on confirmation is claimed.

Together with actual captured water-depth/coverage and original Alchemy
geometry/mask/history proof, this resolves PPD526 as an inherited 3D-wave
feature/settings difference from planar-water viewers. Status dismissed,
owner verification passed for the scoped waves-off outcome. No renderer fix,
product-default change or new push authorization. Existing Water waves remains
available; owner may keep it off for a fixed shoreline. PPD015/527 remain open;
accepted013/497 and BoM522/523 remain unchanged. Viewer is not closed by Codex.
Reopen526 if the moving boundary persists with geometry displacement off or
new evidence establishes a source-parity defect.
