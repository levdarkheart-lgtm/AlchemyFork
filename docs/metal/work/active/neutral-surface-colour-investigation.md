# Neutral surface colour — PPD-083

Status: investigating. Owner confirms the same framing in the supplied comparison;
only the Metal exposure dialog occludes part of that image. Owner now identifies
a hue in the lower grey pavement/road areas of Metal14:08:20 versus more neutral
looking grey in original14:31:18. Keep separate from general brightness PPD081
and do not explain it away with the withdrawn camera/framing claim.

Existing raw image pair and exact byte hashes retained under ignored
artifacts/metal/exposure-comparison-20261113/. Need corresponding same-material
regions, lighting/shadow classification and image ICC/transfer inspection before
claiming a numerical colour shift. A pavement tile and asphalt patch are not
interchangeable neutral targets merely because both look grey. No analysis or
repair is established from differing arbitrary corners alone.

Owner requests restarting our Metal viewer to capture a cleaner image. Existing
reference PID10889 already normal-exited0/state10 at14:44:11UTC. Exact candidate
Sky26.4.0.68965/executable3f629d75890f1bed93f933f6f3f54e685e4c80a47d114b45d419808fc45ba777
launched underLLDB at14:45:37/PID12146, observed exact window logged in.
New immutable candidate-hdri profile/config deliberately sets split.5/EV-3 before
launch, retaining full agreed Look, graphics and Lev1233 camera controls. Actual
EXR resource is process-local; owner reloads it and captures. No current viewer
or display controls manipulated by agent; no new build/shader changes/push.

Next: inspect image metadata and same-material colour samples in the original
pair while owner supplies the new Metal image; use new image once available.
Root owns canonical docs and diagnosis. Keep exact colour measurements separate
from possible causes (lighting/probes, material, tone mapping, grading, colour
space, screenshot conversion); source-only earlier audit found no duplicate
exposure or sRGB encode. No defect closure or colour-parity claim.

Owner provides clean Metal macOS screenshot14:48:40 (3456x2234, matching reference dimensions), saved unchanged as owner-metal-clean-144840.png, and an in-viewer Snapshot PNG14:49:55, saved as owner-metal-viewer-snapshot-144955.png. Keep capture methods distinct: the requested side-by-side uses clean Metal14:48 and reference14:31 macOS screenshots; viewer export is additional evidence for isolating screenshot colour conversion. No pixel edits or alignment warps. Read-only corresponding-region/profile analysis delegated; results pending.

## Measured clean OS-image pair

Read-only analysis of Metal14:48:40/reference14:31:18 uses unchanged3456x2234
RGBA8images and direct same-coordinate same-material patches. Both contain the
EXACTsame4064-byte embedded Display ICC profile, SHA256
5662d0b79d03ba06449be9e660ba7a573d38aeaa4aff5c4ae8d1376978746579.
This excludes differing embedded-profile bytes as the explanation; it does not
prove identical live app/window colour transforms or raw lighting inputs.

Representative sunlit asphalt box[x600,y2010,x640,y2050],top-left pixel origin:
Metal meanRGB67.72/63.34/60.17 versus original74.51/71.67/70.67; medians67/63/60
versus74/71/70. Sunlit lower-left concrete[x35,y1920,x115,y2000] meanMetal
152.13/139.41/122.99 versus original158.93/148.23/135.01. Two concrete andthree
asphalt patches consistently show darker and relatively warmer/less-blue Metal
encoded pixels; both concrete samples already warm, so do not assume a neutral
albedo or claim whole-scene white balance from them. A common UI menu-grey patch
is31/31/31 in both. Initial proposed patch landing in vegetation was excluded.
No resizing, geometric alignment, image editing or resampling was performed.
Raw method/coordinates/measurements/report under comparison artifact/pavement-analysis/.

The in-viewer Metal SnapshotPNG3456x2168 contains no iCCP/sRGB/gAMA/cHRM chunk.
Keep its capture method/profile handling distinct from the two OS screenshots;
it is additional evidence, not a drop-in identically-profiled reference image.

Disposition: image-level colour difference is now measurable; renderer cause
still unproved. Small numerical/AA/filter differences are possible in a native
port; a systematic material-colour shift is not excused merely by API choice.
Intentional landed upstream fixes or differing runtime inputs/state can also
change output, because the original reference predates the new sky changes.
Correctness target is equivalent appearance for corresponding intended behaviour,
not forcing newer corrected algorithms to reproduce an older bug. Next localize
first differing producer: material/albedo, linear lighting/environment/haze,
then exposure/tonemapping/grading, then presented/exported image. Do not apply a
blanket colour/exposure correction before proving that boundary.

## Active diagnosis after explicit owner instruction

Owner explicitly requests finding the cause. SourceHEAD0a27a00283, source tree
clean apart from five old untracked notes. Root owns canonical pipeline/shared
shader and diagnostic integration; independent read-only agents audit texture/
material sampling, lighting/environment producers, and OS-vs-viewer-export colour
conversion. CurrentMetalPID12146 remains running with controls untouched. Do not
repeat only earlier common-post audits or propose a global colour correction.

PPD084 registered before physical test: oldGL explicitly skips sRGB decode when
generating mips; native blits original sRGB texture instead of raw view. Need
actual driver/LLImageGL2x2 test and output proof; not yet a cause for083. Ordinary
probes ingest sharp linear post-deferred scene before display/exposure. New64cube
is not stable probes-on lighting input; actual readiness/state still unknown.


## Presentation boundary isolated

Owner authorizes logging out while diagnosis continues. Candidate PID12146
subsequently normal-exits0/state10 at15:12:34UTC; LLDB receipt records no fault.
No viewer is required for the current isolated GPU tests or read-only audits.

Read-only export-vs-OS analysis measures the actual integer pixel offset0,+66
using static scene edges (correlation.995822; next offset.245621), without scale
or warp. Interpreting viewer PNG as encoded sRGB is grounded in the final shader,
BGRA8 readback and raw PNG writer. Transforming the OS image's actual embedded
DisplayICC to sRGB reproduces five same-material viewer-export patches with
98.94–99.44%of pixels within2/255 in every RGB channel; forward profile conversion
also predicts99.66–99.94%of OS pixels within2. Tint is already in the viewer
export before WindowServer. This excludes a large added OS colour cast in these
samples, not every display effect or every pixel. Full private method/results
are in artifacts/metal/colour-cause-20261113/presentation-boundary/.

Surface/probe ingress audits find matching original formulas for packed lighting,
probe SH normalization, HDRI EV, diffuse/ambient conversions, classic1.1, and
separate haze blend. These are source results, not observed identical resource
inputs. Need same-frame actual albedo, normals/ORM, effective EEP/probe branch,
linear lit colour and actual adapted exposure to isolate the first difference.
Existing RenderBufferVisualization IDs0..3 expose actual Gbuffer attachments;
ID4 is ground/sky luminance meter, NOT adapted exposure. Debug copies bypass
common tonemap but later presentation effects still apply. Do not describe them
as lossless HDR/raw dumps or treat packed normals as XYZ visualization.

PPD084 native physical baseline: actual LLImageGL2x2 generated mip1 reads
188/128/192/128 from sRGB storage versus128/128/192/128 from its raw view and
linear-storage controls. LLDB PID14528 normal-exits42/state10 (38/39 assertions
pass; one intended failing raw-mip policy assertion), no fault. Separate sampling
control31assertions/1case passes. OriginalGL driver control and minimal repair
are still pending in exclusive /private/tmp/alchemy-colour-mips-20261113 worktree.
This proves the native path difference, not the original driver result or083's
cause: the red checker mip gets brighter, while owner pavement is darker/warmer.


## Candidate corrections rejected by controls

PPD084 dismissed: exact external originalCGL sRGB+SKIP_DECODE+sampler0 mip
control ALSO188/128/192/128, same as native. Explicitdecode also188, rawRGBA8
128. AllGLerrors0, actualM1Max OpenGL4.1Metal90.5. LLDB14772 normal-exits42
because the proposed128 expectation fails, no fault. The old comment is not an
actual driver oracle. No raw-mip producer fix: that would introduce divergence.
A positive regression test will preserve observed original/native188 semantics.

Uniform exposure fit (source-exact ACES Hill inverse and actualICC normalization,
no assumed neutral colour) estimates Metal input.87146x reference, about-.1985EV.
It removes93.38%of squared error across15ROIchannelmeans, but all five patches
retain positive-red/negative-blue residuals (meanRGB RMSE2.293/255). Independent
per-patch scalars also leave chroma error. Thus a scalar exposure difference can
explain much brightness, not the entire observed colour shift. Do not bake the
fitted gains into rendering: this is a diagnostic hypothesis, not measuredGPU
exposure or evidence of a white-balance-control defect. Private source/method/
clipping/inversion/result records in colour-cause artifact/ACES fit directory.

PPD085 separately records unported upstream sunlight contribution cap; old1eb
and native share old ordering, so it is not evidence for ordinary pavement tint.
No shader change. Active next boundary is actual original-vsnew sky input to
reflection faces: final upstream intentionally replaces old cap/folded-skirt
rays with sphere+50m raise. Quantify sampled directions/irradiance before any
causal claim or native change. Main split.5 becomes fullHDRI on every probe face
in both implementations; EV-3 applied once in both.


## Retained negative-control test and integration

Integrated test3b4ddbfdbe (agentcommit a0b3e75ec9) adds only the actual LLImageGL
mip/view regression fixture and CMake registration; no producer/rendering fix.
Canonical M1Max doctor0; tools/metal/build.sh build0 with ninja-os-arm64 preset;
verify_app0 (346ARM64 slices). Actual canonical llrender_tests underLLDB,
MTL_DEBUG_LAYER1, filter[texture-mip-parity]:70assertions/2cases pass, child26821
normalexit0/state10, no fault. Native and externalGL green receipts and retained
failed raw128 assumptions are private colour-cause artifacts. No crash resumed,
CrashReporter preference changed, viewer relaunched or push performed.

Build-only provenance changes Sky app to26.4.0.68972/executableSHA256
b2fc6b85f0e391739ef51df42de03d313ae839dcfe7a9548ad845f86476f3791.
Owner images remain evidence for exact old68965/3f629d7589 executable, not the
new binary. Re-derive immutable role profile/hash before any future launch;
production renderer source did not change in the test commit.

Controlled sky-ray model now finds a plausible colour producer: final upstream's
intended full sphere changes below-camera HDRI directions versus original folded
skirt. At azimuth5/elevation-45 degrees, old hit2631.90m versus sphere21196.87m
makes a.68438degree raised-ray delta; +15degree ray delta only.00726degrees.
For controlled Kloppenheim pure-sky/-3EV,128square faces reduced to8square SH
model produce upward-normalRGB old.001701/.009153/.022323 versus new
.002041/.009502/.022676 (ratios1.200/1.038/1.016). Direct cosine integral is
nearly unchanged; truncatedSH therefore amplifies the lower-hemisphere change.
This is NOT yet ownerfile, productionGaussian/halfpack/worldgeometry or actual
bound SH proof. It is intentional upstream behavior, not a native-only defect.
Independent physical productionModernSky/SH control is the next test; no sky
rollback, arbitrary correction or083closure selected.


## Actual owner-session probe publication failure

PPD086 registered before repair: owner-candidate14:46:01/14:48:01 face5 publication
fails matching-pixel-format checks. SourceHDRproducer/filter rg11b10 differs from
managerbootstrap/publication/reset rgba16float. Failed firstdefault may retain
black/zeroSH and can block retries atface5. This actual-runtime path takes
priority over the small intentional sky-ray effect. Physical source→array and
state-transition controls now underway; no owner tint closure inferred yet.
Owner has confirmed Kloppenheim06pureSky4k filename for both screenshots.
