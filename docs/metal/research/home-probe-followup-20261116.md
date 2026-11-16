# Home probe follow-up: overlap and remaining shader cost

The owner requested continued shader-spill/GPU-cost investigation alongside
useful CPU work during completion waits. Runtime source819f98d71c/build69224
remains unchanged. Original architecture acceptance and PPD-319 remain partial.
No viewer was launched or newly recorded; no push occurred.

Private experiment: `artifacts/metal/quiet-home-ambient-probe-20261116T205907Z`.
Source audits: `home-checkpoint-69224-20261116T202910Z/analysis-source/`.

## Useful work during waits

The idea is valid, but the exact boundary matters. Existing
`llviewerdisplay.cpp:530` can run `gMainloopWork` before end-frame pacing;
`display_end_frame:1498` calls it after `display()` returns. Source explicitly
keeps this work outside the mutable rendering scope. Presence of the hook does
not prove it was active or had spare budget during the recordings.

The measured older-group waits instead occur in
`almetalworld.cpp:2493`/`reserveMainFrame`, called by
`beginMainFrameBatch:3946`, inside rendering. It blocks on older committed
commands while enforcing two main groups and completion-based resource lifetime.
It does not run the end-frame queue. Arbitrary queue callbacks could mutate
scene/resource state being encoded, so moving that queue here is unsafe.

There is a concrete preparation boundary to assess: the G-buffer caller begins
its main batch at `pipeline.cpp:17155`, before adopting the owned input packet
at17190 and optional preparation below it. Moving useful, bounded work before
admission requires proving its input lifetime, dependencies and resource costs.
The recorded preparation toggle is OFF and group/cohort paths limit the
existing optional worker path; moving that disabled path alone would not show
an improvement for this workload. Main/light/post preparation needs actual cost
evidence. Increasing frames ahead would change latency/memory, not establish
that useful CPU work was hidden.

Confirmed-Midday80–120s records47.112ms main waits per eligible frame. The
display wall remainder is10.812ms, the ordinary-probe-update wrapper6.962ms and
the application `idle()` wrapper0.854ms. These wall wrappers are not processor
idle time or an additive/movable CPU budget. Wait targets are already-started
frame-minus-two GPU commands and post-GPU delivery is prompt. The evidence does
not prove hardware GPU saturation, CPU starvation or a gain from more CPU work.

## Separating probe updates from main shading

Source and native labels identify both roles, but overlapping envelopes do not
provide exclusive busy cost. In Region60–120s, main-owned envelope union is
59.878s and ordinary-probe union20.237s: at least20.115s overlap within60s.
Do not subtract these into isolated costs or add them as frame time.

Observed face-publication cadence is about15.4/s early and15.33/s late. Faces
per displayed frame rise because display FPS falls; this is not evidence of
an increasing update frequency per wall second. Main and ordinary-probe command
envelopes both lengthen. SH-publication commands exclude the full generation
work and must not be mistaken for its total cost.

The next recorded-scene control should isolate main sampling from ordinary
views. The old late capture proves main **base-lighting** configured20 versus
ordinary base43. Alpha fills a separate snapshot; existing exports do not bind
the seven expensive alpha draws to their attachment/probe-buffer values.
Thus a shared `config.x==20` selector is not yet qualified as main-only. First
verify all affected bindings/draw scopes, then use a diagnostic main-only
sampling intervention and restored control on a separate working copy. The
earlier shared helper intervention affected both roles. Preserve full inputs
in the implementation; diagnostic probe exclusion is never a shipped fix.

## Ambient-only shader hypothesis: tested and rejected

Simple alpha requests literalPBRfalse/gloss0/environment0. Original1eb44's
modern ambient-only policy has the same traversal and32accepted/64neighbor
limits; native32 is not a newly discovered quality mismatch. Retained optimized
AIR still showed a generic112byte accumulator, unused output addresses and
reflection setup, suggesting a dedicated ambient-only call boundary.

The private candidate uses a48byte ambient/weight accumulator and one returned
RGB value. Generic, material/PBR, base, water and High2 behavior remain unchanged.
Both complete offline libraries were compiled from current819High2 source with
identical Metal3.1/fast-math/target26.0/source-recording flags. This avoids
conflating the earlier shadow-quality gain with this new hypothesis.

Existing API/GPU-validated production tests pass7584assertions/2cases;
525image comparisons have maximum absolute error2.38418579e-7, zero failed or
nonfinite components under the existing criteria. Some comparisons are unchanged
helper/base controls; they do not exercise every new-helper traversal edge.
No broader parity or live visual acceptance follows from this contained gate.

The High2 actual shadowed-alpha cost fixture uses20probes, seven blended layers,
1024×576, ten alternating warmups and30alternating pairs per family. Candidate
paired medians are **3.56% slower automatic** and **3.45% slower mixed**;
25/30 and27/30 pairs are adverse. All120observations remain preserved.

The candidate was then checked on the owner's saved69200 late Home frame in
Xcode26.6, M1Max/macOS26.5.2, observed Medium/Overlapping mode. The shared shadow
filter is specialized to quality2 in both arms; captured resources/probe counts
are unchanged. Actual replay results:

| Arm | GPU workload | Simple-shadow registers | Spilled bytes |
| --- | ---: | ---: | ---: |
| A1, High2 control |119.35ms |not reread |not reread |
| B, dedicated ambient helper |126.66ms |226 |304 |
| A2, original helper restored |124.64ms |224 |304 |

B is1.62–6.12% slower than the bracketing controls. A1/A2 drift is retained,
as is untouched-session133.15ms; no single untouched timing is a stable control.
This is recompiled fixed-capture evidence, not same-binary editing, isolated
pass cost, a fresh69224recording or FPS. The112→48byte source change did not
repair the measured spill. Do not select it from smaller intermediate state.

Three undo operations restored A1; another restored the original runtime shadow
expression. Replay stopped, **Don't Save** selected, and Xcode returned to Replay
/Finished running. All private source/library hashes, compile/test commands,
adverse data and UI limitations are retained. A stale temporary replacement was
observed and undone before compiling B; no failed-source timing is used.

## Controlled SH inlining: promising fixture result, incomplete scene check

Fresh optimized AIR explains a distinct change in the rejected candidate:
baseline generic probe tap has355lines with SH inline; the candidate outlines
SH into a288line function and the generic tap shrinks to75lines with a new call.
This also affects source-unchanged generic consumers. The new ambient helper
itself is already inlined. These are front-end call graphs, not final AGX calls.

A second private control adds `always_inline` only to the SH evaluator in that
ambient candidate. Both offline compilation and optimized-IR checks pass; no
separate SH definition remains in that IR. Native7584assertions/2cases pass.
Against currentHigh2,30paired observations per family give GPU medians2.98%
and2.46% shorter; reversing source/library arm order gives1.58% and4.78% shorter.
All240cost observations and adverse pairs remain preserved. This is a modest
contained result, not proof of a spill reduction or useful live-viewer gain.

The next saved-scene session verified Medium/Overlapping and its A1High2control
completed at118.85ms (untouched135.87ms also retained). Before B returned a
result, Xcode exited. The21:47:23UTC crash is EXC_BAD_ACCESS/SIGSEGV in
`clang_getTranslationUnitCursor`/`GTMSLIndex`/`GPUShaderProgramSource` annotation
lookup. CUA and process checks confirm Xcode terminated. **No second-candidate
scene timing or spill number exists.** This is PPD-334, an external-tool failure,
not a demonstrated viewer/GPU execution crash. No discard prompt was observed;
the disposable working copy may retain temporary edits. Original owner captures
and engine source were not edited. A bounded native binary-archive inspection
was used as an alternative to repeated source-editor attempts.

Both currentHigh2 native GPU archives exported and reopened successfully with
FailOnBinaryArchiveMiss, API validationON/shader validationOFF, and identical
production unrigged/HDR simple-shadow descriptors. They contain actual
`applegpu_g13s` GPU executable/object slices, not merely AIR. The installed
`metal-objdump` can read symbols/sections but cannot print this GPU ISA, including
an explicit AGX2 attempt. No new GPR, spill, stack or call metrics were obtained.
Fragment text sizes36416/36418bytes are artifact identity, not performance or
spill evidence. No draws/queues were created; source/library input hashes are
unchanged. Exact commands/failures are in private `native-archive-inspection/`.

Apple documents pipeline-statistics compiler remarks for dynamic-stack spills
independent of register pressure. That makes inspecting the actual remarks a
better next attribution step than assuming the304bytes are the112byte source
aggregate or ordinary call frames. The exact304byte accounting remains unknown.
[Apple's shader-spill guidance](https://developer.apple.com/videos/play/wwdc2020/10632/?time=2404).

## Next shader/native boundary

Actual spill origin remains unproved. Qualify call/outline boundaries and live
SH intermediate values rather than assuming addressable accumulator arrays are
the cause. A register or AIR change must reduce measured GPU work while
preserving all scene lighting; it is not an acceptance result by itself.

The truncated source-profiler deployment-target banner needs full inspection;
no in-place retargeting remedy was verified. Installed `gpudebug` is unavailable.
`metal-objdump` supports device-GPU objects, but AIR alone is not final assembly.
Keep the product deployment minimum unchanged. Exact public capabilities and
local command results are in private `shader-profiler-capability-audit.md`.

The owner's pasted Apple advice was checked against primary documentation and
the installed SDK. Compute `maxTotalThreadsPerThreadgroup` is not a direct
control for these raster fragment shaders; tile pipelines have a separate
threadgroup role. It may be useful for measured compute kernels later. Function
constants already underpin the selected High2 change. Precision/lifetime and
recomputation changes still require workload and pixel checks; blanket half
conversion is not justified for positions, shadow depth or HDR accumulation.
[Apple's compute property](https://developer.apple.com/documentation/metal/mtlcomputepipelinedescriptor/maxtotalthreadsperthreadgroup),
[Apple's shader-core guidance](https://developer.apple.com/videos/play/wwdc2020/10632/).

The narrow remaining-array audit finds reachable PCF `wx[6]`/`wy[6]` allocas and
six runtime local-array GEP sites in retained AIR. That AIR is **pre-PSO** and
still queries optional High2; survival in final High2 GPU code is unproved.
The original source also uses weight arrays, with kernel6 fixed before GLSL
compilation. The five-index removal reported earlier applies only to the
rejected scalar accumulator variant. It never established elimination of all
thread-local indexing, shader storage or the304byte spill.

The next contained shader check is therefore the exact High2 PCF weight
representation/unroll boundary, preserving the full6×6 math/order, derivatives,
all valid probes, quality0/1/3 and the probe-single-tap policy. Compare current
High2, not pre-fix shaders. Do not broadly unroll the whole fragment or reduce
lighting quality to manufacture a gain. Actual compiler remarks remain needed
before claiming the PCF arrays are the measured spill's cause.

The same reference audit finds a separate Metal sampling question: original
High2 uses a comparison gather for each2×2block, while native PCF reads four raw
depth values and compares them explicitly. A supported raw gather followed by
the same comparisons could be tested independently. Texture format, gather
component order, edge behavior, coordinates and reverse depth must match;
neither fewer source calls nor the original API establishes a GPU gain.

The decoded-SH audit also maps a possible producer-owned derived input. Current
GPU projection writes half-rounded packed64byte rows; ordinary publication is
GPU-ordered and need not wait for CPU completion. Only the default preview
reads CPU coefficients after completion. A producer GPU decode/output could
retain that ownership and epoch pairing, but decoded rows grow to108/144bytes
and could worsen cache/register traffic. Do not add a CPU publication barrier,
change half quantization, or infer a performance gain from unified memory.

The probe-admission audit finds completion-owned optional credit is active;
there is no unbounded-group finding. Main GPU completion census does not feed
`FrameInputs`/`tryAcquire`: optional admission uses CPU cost, memory/streaming
and2per-frame/4active caps. Existing ordinary groups retain coherent admission
across12faces. This is an unfinished adaptive main-GPU feedback boundary to
measure, not a demonstrated cause of the Home decline or permission to reduce
probe-update quality. It remains distinct from expensive main pixel sampling.

The large encoder called atmospheric-haze still includes ordered alpha. Earlier
actual haze was only0.7–0.8% of shader cost, versus about34% simple alpha in one
late replay; the new untouched replay has27.84% simple alpha and0.61% actual
haze. These are variable counter-replay shares, not live FPS attribution.
The expensive transparent/probe lighting remains the target, not the label.
