# Overall exposure investigation — PPD-081

Status: investigating. Owner reports sky and overall scene appeared overexposed
before the new sky port. Current source10b1ab3e72 /renderer efc3dc17d0, Sky68965;
exact first affected build unknown. Original1eb44bb and final408 source are the
behavioral references. Do not conflate improved per-file HDRI EV-3 with a fix for
common scene exposure. No proposed code correction or original runtime outcome.

Owner-provided current screenshots show global exposure1.0,ACES,mix.70,auto
exposure enabled,HDR/emissive/SSR enabled,Midday,drawdistance112. The saved
LevDarkheart label alone is not active-setting evidence; these UI values are.
Raw images retained with PPD080 under artifacts/metal/hdri-preview-exposure-20261113/.

Root owns canonical docs/register and meter/history audit; read-only independent
agents audit common post/exposure/tonemap/color boundaries and ordinary sky
packing/gamma/HDR-classic transfer. No shared files edited concurrently. Keep
running owner viewer and all user controls untouched. If a real defect is found,
record original invariant and actual failing producer before implementation;
then narrowly test actual shader/CPU path under LLDB before integration.

Next executable work: trace scene meter/current exposure history constants and
almetal_world_resolve_exposure against exposureHistogram/exposure GLSL, plus
root caller/LLSettingsVOSky policy; integrate source findings from other audits.
Matched original/native raw/display outputs and actual exposure values remain
unknown; source agreement cannot by itself dismiss the reported appearance.

## Read-only findings and owner baseline

Owner13:56 display image: Built-in Display, Apple XDR Display(P3-1600nits)
preset, brightness slider near high end, automatic brightness and True Tone on,
ProMotion. This identifies selected settings, not measured cd/m2 or active layer
EDR state. Native production source uses BGRA8UNORM, explicit sRGB colorspace,
EDR=false, with no found EDR opt-in writer. HDR/emissive selects internal float
scene lighting, not extended-dynamic-range display presentation. Native display
post converts linear to sRGB once; UI/presentation copies encoded values to the
unorm drawable. Original1eb44bb/final408 also tone-map to SDR sRGB. No doubled
presentation encoding found. Actual ColorSync/WindowServer/physical brightness
and active runtime layer properties have not been measured or inspected.

Root comparison: meter grid,64bins over[-16,+12]log2 luminance, percentile
trimming, spatial/sky weights, key.18, compensation and exponential adaptation
match final408 exposureHistogram/exposure shaders and caller packing. Native
bootstrap adapts from fixedEV0; no current actual history texel/EV readback.
No doubled automatic exposure found in this boundary. Probe-only log lines with
main_world=false and exposure=0 are not main-view failure evidence.

Independent common-post audit: static RenderExposure times adapted texture.r
exactly once; texture replaces the scalar adapted factor. ACES Hill fit/matrices,
blend with clamped-linear image, bloom extract/upsample/add and optical order
match original1eb/final408. Ordinary sky CPU producer/UBO terms and18relevant
source defaults also match original1eb/pre-port2144/final408; sky2x/2sqrtgamma
and classic1.1 are inherited factors applied once, not newly identified gains.
Raw token comparisons and read-only reports retained in the artifact directory.
Source agreement is not runtime parity or dismissal of this owner report.

Effective classic-sky policy remains relevant: absence of authored probe ambiance
sets canAutoAdjust, and legacy auto-adjust defaultoff selects classic fixed
exposure; effective tonemap mix can be0 despite the enabled UI checkbox/configured
mix. The exact active environment's flags/gamma/effective transfer are not yet
verified. PPD-082 separately records a permitted blend-boundary inconsistency;
it is not evidence for this steady overall-brightness report.

Owner14:02 image shows Exposure1,ACES,mix1,auto checkboxon,compensation0,
average metering,EVrange[-3,+4],speeds3/1,shadowcut11,highlight95,skyweight.25,
ColorGradingoff. This range is a saved runtime choice, not current XMLdefaults.
Requested consistent diagnostic baseline: Exposure1,ACES,mix1,autoON,comp0,
center-weighted,EV[-2,+2],speeds3/1,shadow10/highlight95,skyweight.25,gradeOFF.
Mix1 is a diagnostic choice; shipped/source defaultmix.7 remains unchanged.
Prefs and Lightbox edit the same controls, not stacked exposures. Owner14:05
image confirms the complete requested baseline. No main scene image at that
baseline/reference comparison has been supplied or captured by agent yet.

Disposition: no confirmed cause for PPD081 or corrective renderer change.
Next useful comparison uses the same effective environment, camera, controls and
asset state in original/native, with actual adaptation/output evidence. Owner
viewer remains open and under owner control; no agent UI/display manipulation,
restart, new build, GPU test, performance claim, defect closure or push.

## Separate PBR-to-classic transition concern — PPD-082

Source permits a blend to copy target mCanAutoAdjust immediately while lerping
probe ambiance. With a legacy target, classic can become true before ambiance
reaches0. Native main_dynamic_exposure then stops, but no_post remains false and
exact_dynamic_exposure still requires a current history. Original generateExposure
writes fixedEV0 when meter=false. Steady loaded classic skies correctly have
no_post=true and bypass adapted texture. This transition concern has not been
reproduced at runtime, and its impact/relationship to any owner symptom is
unknown. Next: exercise actual blend plus history/publication state in a bounded
production-path control before choosing or claiming a correction.
