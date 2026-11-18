# 69264 Home/club retest —2026-11-18

The retest supports the integrated startup/probe/terrain/wearable repairs,
but **does not establish a whole-viewer FPS improvement**. Owner observations
remain Home~31→16FPS and club~28–29→stable19. Main16x16 raster lighting was
actually active; probe/hero rendering remainedautomatic32x32. The controller
preserved the original19architecture overrides and selected the new
RenderMetalSmallLightingTiles=true option. Existing quality/environment
settings were not changed; all49common typed controls match the prior run.

Home still develops substantially more GPU lighting work as probes populate.
The expensive settled BASE and simple-alpha GPU entries are the **prepared**
variants, so the current hotspot is not explained by missing prepared-path
activation. Club combines growing GPU/light/geometry workloads with much more
main-thread CPU work. Completing unrelated architecture first is not justified.

## Exact run and coverage

Source25faff429e27aae824321e686fc75cda8bead272, build26.4.0.69264,
executable46fb9270a18f79ec30868b34402709c6f576cd0849220e9406efe8581bf8ca63,
metallibfae7a43b5b48b7375f3a47ae52b6c7c7d89dd6b5f6a7456562d9e99438e223d0.
Exact verified stage and [component qualification](native-lighting-raster-tiles-20261118.md)
are preserved. Runtime private root: `/Users/dekstroza/Alchemy-home-jhdlfjso`.

Owner authorized this run and drove Login, teleport and logout. ViewerPID55666
started12:03:45UTC, recorderready12:03:49, Homeworld12:04:27, clubarrival12:08:51.
All four saved device-allqueue intervals stopped with nativeerror0: Home
219→220/3326→3327; club4498→4499/7893→7894. Original captures total
32,764,575,184logicalbytes, independently preserved and copied for replay.
Xcode26.6 replayed/profiled all four working copies sequentially, Medium requested,
Overlapping, fresh sessions; all workloads stopped and documents closed.

The original Overview collector remains **FAILED PPD408raw-sizeguard** during
finalization: observedpeak2,223,515,824bytes versus2048MiB, underlyingxctrace0,
final86,782,294byte trace retained. Separate bounded offline exports allpass0:
CPU723,953rows spans519.318s/max85msgap, GPU16,096rows spans519.696s.
Expectedready→stop518.452286s is covered. The unchanged **normal5-second GPU
gap gate still FAILS**, max9.028589s around capture pauses. The failed collector
was not relabeled; no replacement viewer run was required.

Native stream234,499,467bytes parses without errors; footer600sdurationlimit,
285contentiondrops/0budget-full-I/Odrops/23pendingcommands atcutoff. It is lossy
and continues beyond Overview; tails are not silently pooled. No current-binary
capture-OFF control was recorded. Full frame contents/counters and normal
frame-pacing qualification are separate claims.

## Home decline and coupled frame cost

Capture-API intervals and first following contaminated periods are excluded
from the native cadence estimates below. Remaining period is not pureCPUtime.

|Scene/age|Mean native period ms|Observed main-caller wait ms|Period minus wait ms|
|---|---:|---:|---:|
|Home30–60s|50.708|25.557|25.152|
|Home80–120s|61.827|38.044|23.783|
|Home120–180s|66.129|42.058|24.070|
|Club30–60s|46.182|1.292|44.890|
|Club80–120s|52.391|6.614|45.777|
|Club120–180s|52.668|5.584|47.084|

The first Home0–10s estimate28.622FPS/34.939ms covers only5.555retainedseconds;
owner~31 is a separate approximate observation. Home120–180 mainCPU falls to
29.88%ofonecore with decoderCPU0; growing completion waits, not growing main
CPU execution, dominate its later period increase. Most wait starts are inside
already-running required-main command envelopes, with only~0.49ms perperiod
after GPU-end; this does not look like a growing completion-callback tail.
GPU command envelopes overlap and are not hardware busy or additive pass time.

Club120–180 mainCPU is77.37%ofonecore and decode9.285CPU-s/60s. Its source
ownership classification includes sun-shadow13.523CPU-s, geometry9.674,
lighting7.588 and ordinaryprobes3.251 over60s. These include instrumentation
descendants; they are not pureengine/removable savings. Source/stack audit
identifies generateSunShadow, captureMetalShadowScene, geometry/publication,
post-alpha and native API paths. A single GPU-shader or thread-count fix will
not resolve both scenes.

## Fresh GPU replay results

These are saved-frame diagnostic replay envelopes, **not live frame times or
FPS**. All five counter categories retain every render+compute+blit encoder row,
including duplicate labels and tails. A fresh complete per-draw slot8/9 and
function-constant dictionary was not exported for every draw; actual compiled
candidate entrypoints plus native supported-consumer decisions prove selected
prepared work. No source/shader edits or diagnostic
variants were made during this replay session.

|Phase|Replay GPU ms|Commands|Render/compute/blit|Draws|Summary vertices|
|---|---:|---:|---:|---:|---:|
|Homeearly|77.47|22|43/8/58|5,345|7,552,113|
|Homelate|146.26|15|37/8/7|5,989|15,636,962|
|Clubearly|53.88|82|38/2/73|4,521|24,346,955|
|Clublate|116.19|20|42/4/9|8,156|136,317,047|

Home's combined BASE/sky/point/projector encoder executes~273.188M fragment
invocations in both frames, but ALU instructions grow32.022B→63.616B:
**117.22→232.87ALU/invocation,1.99x**. BufferL1reads445.88→953.94MiB and
textureL16.42→13.88GiB grow too. The large alpha-containing encoder executes
fewer fragments56.385M→23.216M yet ALU14.389B→32.387B:
**255.19→1395.03ALU/invocation,5.47x**. This locates growth in work per shaded
fragment rather than merely more command submissions or pixels. It does not
isolate every SH/weight/PCF/texture term into an additive time budget.

Configured probe counts are not accepted SH taps or positive-scale density.
Configured Homeprobes1→32; pointcount9/rectanglearea40,259,010 staysfixed,
projectors51/225,410,029→225,409,466 nearlyfixed. The late main alpha candidate
packet has23full/8bounded locals and20/20supported batches prepared. Generic
early fallback is conservativeall_full; late dominant GPU shader entries are
preparedBASE and preparedHigh2simplealpha. Their reported shader shares25.21%
and19.07% are cost shares, not isolated elapsed durations.

All three phases Homeearly/Homelate/Clublate retain depth-classified BASE plus
explicit reflection Gaussian/downsample chains, corroborated by native ordinary
face inputs. Clubearly retains spots/impostor/73blits, but no explicit probe
filter chain or exact ordinary-face row; absence of that row does not prove all
ordinary work absent. Distinct native-queue identities were not fully exported.

Clubprobes2→16, pointlights57→44 and projectors2→32 materially change during
loading. MainG-buffer VS invocations1.754→8.471M and vertexALU.466→3.356B
(7.20x); late9.961Mprimitives/91.08%culled is submitted-work evidence, not proof
how much can safely be removed earlier. Late supported simplealpha192/192
prepared; main fused fragment work
and shadow/geometry work must be assessed together. Late shader shares include
projector23.24%, preparedBASE17.83%, point12.36%, SSAO9.86%, riggedspotshadow
vertex3.27% and two distinct material-alpha rows2.56%/1.88%. Do not collapse
same-named shader/encoder rows or multiply those shares into a live FPS budget.

Prepared consumption currently supports BASE/simple-family alpha. Source
world21270/22700 and material/PBRMSL9449/12961 demonstrate that material/PBR
alpha candidate masks are **not implemented**; these call helpers with default
nullmask. Their generic use is a pipeline-consumer capability boundary, not an
invented admission/residency/publication failure. The observed4.44%clubmaterial
share is smaller than required projector/BASE work and does not justify making
that extension the sole next priority.

After actual profiling, hot BASE/simple/projector compiler spill cells remain
blank. Across411shaderrows,181blank/unknown and230explicitzeros are retained;
no observed nonzero field is not proof of zero spills. Late preparedBASE226GPR
and simple222GPR versus generic214 identify register pressure, not physical
spill traffic. The selected Home alpha-containing encoder’s FS BufferL1BytesWritten
remains~1.02MiB; its FS BufferDeviceMemoryBytesWritten is409.44→589.31KiB.
Clubalpha10.21MiB is the device-level write counter, not the same counter as
Home’s1.02MiBL1figure. These need attribution, not a compiler-spill interpretation.

## Every warning and repaired behavior

All **293WARNING/42sites**,0ERROR/FATAL, plus3untimed libpng warnings are mapped
to source/trigger/original behavior. Full repeated messages and phase counts are
private, not omitted. Largest groups106asset delivery,64missinganimationjoints,
33transporttimeouts,22appearance lifecycle,18startup/UI/settings,13capability
readiness. Remaining groups include10missing baked-mask auxiliary images,
4early appearance messages,3animation loads,3CPUalpha-format checks, paired
mip/create, handler-ownership, teleport-map, complexity warnings and9singletons.
PNG warnings are2grayICC/RGBmetadata and1interlace-handling; exact callers/
images are unknown without a stack.

Three newly observed sites/15rows match original1eb44bbd branches: missing
baked-mask auximage, deferred appearance before avatar creation, animation for
an unknown avatar. Inherited behavior is not automatically harmless; content/
visual impact remains unknown. Asset/transport issues can affect loading and
animation warnings can affect content behavior; none is established as the
cause of the late Home shader-work growth.

Targeted warning families did not recur:0pipeline.cpp/almetalworld.cpp and
0llwearabledata.cpp warningrows. More strongly, **all136matched main/probe
shadow census rows have failed_views0 and all8CPUfailurestages0**. This supports
PPD125/241/399 repair behavior in this run, including the old pool-hook startup
pattern. It is not just suppressed logging, blanketvisual acceptance or an FPS
claim. Ownerconfirmation/general feature gates remain pending.

## Comparison limits and next recommendation

All four current main targets are3456x2168 versus prior69258normal3456x1916,
**13.1524%morepixels**. This is not proof the owner changed graphics settings
or physical window size. All49common typed controls match both complete50-key
Home snapshots; only newtiletrue is added. NoClubsettingssnapshot is retained.
Region lighting is expected to differ: Homeobservedambiance1/classic0, Club0/1.
Club includes thermalFair(raw1); no actual clock/throttling magnitude measured.
Changing assets/geometry, viewport, EEP and armed-capture overhead prevent a
matched old/new FPS verdict. Within-Home stable-target growth remains valid.

1. Keep tile16 provisional/defaultOFF until same-binary automatic/16 capture-OFF
   controls at matched actual viewport/workloads establish net FPS/pacing.
2. Prioritize required GPU work: use controlled variants/restored controls on the
   exact late Home frame to optimize accepted probe lighting without dropping
   contributions; also audit PPD344native point/projector volume-depth rejection
   against original behavior because those shaders consume a large baseline.
3. For Club, target the measured residual small-workload shadow capture, native
   bindings/resource and command preparation. Shared shadow membership caching
   already exists and is active; do not reimplement it or enable banks/parallel
   work without cold/warm/churn total-cost proof. Bounded work over published
   immutable inputs is relevant here where it reduces actual owners' costs.
4. Refine coordinated admission only where actual backlog/pressure proves a
   useful change. Main-wait/ordinary/streaming/decode feedback is already wired,
   with timing/liveness acceptance still partial; it cannot remove required
   lighting or valid shadow work. Keep broader Apple-first acceptance partial.

Xcode separately reports API-usage insights, including redundant bindings
(9,943insights in the earlyHome summary), plus RuntimeIssue markers on some
resource calls. These are not the293application warning rows or proof of a
rendering failure/removable FPS cost. They motivate measured native binding/
command-preparation review; current capture-OFF cost and exact benefit are
unproven. All retained summary/details remain private.

No new engine change, viewer relaunch, shader edit or push followed this retest.
All private evidence/failures and prior candidates remain preserved.

## Evidence index

Private runroot `analysis/native-and-log-audit/` contains reportc54cb29c, all
42site/source mappings, control and counter proofs, raw/censored native phases.
`analysis/overview-audit/` contains report4c6657e1, boundedoffline exports,
failed5sgate, CPU/GPU reductions and original failedsession digest. Its
`source-priorities-01/` pins original/source/CPUpriority audits.
`analysis/gpu-replay/` contains exact preservation receipt53f7a2e0, full rawUI
summary/profile-controls/5countercategories/shaders/stopped states for each
phase, row-gated reductions, input recovery and final closed-Xcode evidence.
`analysis/gpu-replay/ui-profile-session.json` records every observed replay
setup and result; Cost-derived proxies are never isolated timing claims.


Four-frame final report SHA2561ad9a46255f348420bcd0e8729f012c03e33941081b94f862d18928a9f0b92d9,
receiptcc783cae2cf01a2f2175fabe14a6401f555d782237775a31c247c7fa83b12c2a,
under `analysis/gpu-replay/four-frame-01/`. Its exact five-category encoder
identity/order gate passes in all4frames. Post-UI source/evidence verification
is metadata-only; originals were fully byte-hashed before replay, only working
copies were opened, and no payload rehash or shader ablation is invented.
