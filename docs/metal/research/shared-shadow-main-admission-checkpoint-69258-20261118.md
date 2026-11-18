# 69258 Home checkpoint — shared shadow publication and main-wait admission

The integrated boundaries are active, but **Home's FPS decline remains**.
The capture-disabled control falls from 32.657 FPS during world 0–10 s to 16.249 FPS
at 240–300 s. Observed main-completion waits rise 9.166→42.802 ms; the remaining
period falls 21.456→18.739 ms. This is a within-run GPU-completion diagnosis,
not proof of an improvement over a different build or an exclusive shader cost.
All four unchanged GPU controls have now been profiled; results follow below.

## Exact integrated candidate

- Canonical production commit `cfc20af4ba19c7df9d1cf406cc07a5eba4c7eda6`,
  worktree `/private/tmp/alchemy-engine-takeover-20261116`.
- Builder `afc9278d23d7fa5e800e322a7e1fa913e21d1070`, same production indra
  `f926c2ed3b1acd3c9b33a3bd900ffa22051cbf90`, Release26.4.0.69258.
- Separate app: `/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/shared-shadow-and-main-gpu-admission-20261117/candidate-26.4.0.69258/Alchemy Metal Alpha G2G5.app`.
- Executable SHA256 `67b701241d5f78ca41fd69c3938f112ec841a913837cff50c6f1fc0f15a153bb`.
- Active unchanged Metal library `fae7a43b5b48b7375f3a47ae52b6c7c7d89dd6b5f6a7456562d9e99438e223d0`.
- Private evidence root: `/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/shared-shadow-and-main-gpu-admission-20261117`.

## What this group implements

Shared immutable group membership/resource publication now reaches shadow
consumers as well as main/probe views. Cache hits precede repeated source
factories/unions/retains; exact ordered cull/pass membership and current model,
UV and animation state remain authoritative. CPU membership ownership ends
at encoding/join; native ownership survives command completion. Source-aware
stability and admission guards bound optional retries; logical subscription
metadata is charged through its own lifetime. Small families retain scalar
preparation and the existing 8,192-row dispatch gate.

The existing outer frame admission now observes actual required-main wait wall
minus measured waiting-thread CPU, in addition to required-main CPU. Optional
post-display probe work sees that pressure; unknown CPU is not fabricated.
No extra wait, queue depth or worker was added. Earlier texture/stream/upload
work still precedes the later current-display wait and remains a broader gap.

No shader experiment is promoted. PPD-382 isolated source-change/early-denial
cost taxes remain open and unaccepted; stable warm reuse benefits are not a
blanket performance result. Useful small-workload object visibility/commands,
broader dependency scheduling, accepted-light reuse and a complete pass/resource
liveness graph remain partial. See the source/flags audit under
`checks/unfinished-boundaries-audit-01` (report5fa32952).

## Build and contained qualification

Doctor and prescribed full build 03 pass. Final native 978,792 assertions/44 cases,
actual current-ABI owner frontier 427,323/3, MainScope 125/3 and dedicated late
controller 16 fresh processes/384 assertions pass. Appropriate validation is ON
for native correctness, OFF for performance recording. Strict source 3,613 files
has 0 blocking/scan errors; harness 33 files passes; tool suite 180 tests/one opt-in
skip passes in the existing qualified venv. Separate stage matches 8,273 nodes,
346 ARM64/0 invalid, strict signatures 0 and 3 owned direct-import checks0.
Qualification receipt 507dde7a and stage receipt 3c3af1eb retain exact scope.

Actual selected Release commands for pipeline/LLRender/world use O3/ARM64,
not O0. MSL uses Metal3.1/fast-math/source recording, with no explicit MSL-O
claim; LTO is OFF. Compiler/source proof does not establish optimal tuning.

## Runtime coverage and controls

Owner explicitly authorized saved Home Login while absent. The existing button
API is requested after recorder readiness, with stored credentials/location and
no settings changes. All three world runs close normally; final quit 03:33:01 UTC
and process absence are observed. No club teleport occurred.

| Run | Mode | Retained GPU intervals | Coverage result |
| --- | --- | --- | --- |
|Alchemy-home-uphzzchc|capture-disabled360s|none by design|normal coverage PASS|
|Alchemy-home-77ib0k3e|default decline360s|223→224;1588→1589|GPU gap gate FAIL5.123247958s|
|Alchemy-home-sowsq5_l|explicit late180s360s|228→229;3625→3626|GPU gap gate FAIL5.225964750s|

Raw recorders/exporters succeed for both diagnostic runs, but qualification
exits1 remain retained. CPU spans 360.478/360.416 s have max gaps 0.100/0.064 s.
The early and late capture stop calls take 4.194/8.948/3.854/8.410 s CPU wall;
these are capture pauses, not GPU execution. Native streams declare contention
drops 234/169/250; no full/budget/I/O loss, all committed commands complete,
pending 0 at stop. Native cadence is qualified partial, not claimed lossless.
Failed first control Alchemy-home-yna82l0q never leaves Login and is excluded
from world performance; PPD384 repair/retest and all its logs remain retained.

All 9×49 typed settings snapshots are complete and equal across successful runs.
Effective lighting is not proven equal: first encoded probe ambiance differs
1.50006/1.24202/1.07969 and the logger does not continuously sample that float.
Full camera/EEP/workload equality is unproved. Region lighting changes are
expected. Current drawable 3456×1916 differs from69241's3456×2168. No matched
old/new gain or isolated capture-layer A/B conclusion follows. Censoring API
pauses alone does not establish normal-rendering equivalence.

All four traces (20,356,133,647 bytes) have stable original manifests and verified
independent APFS replay copies with internal existing links. Preservation receipt
`replay-preservation-20261118T033443Z/receipt.json` is 5bb9a12c. Originals are
not opened or edited; actual queue/view coverage is qualified by replay below.

## Normal-rendering diagnosis

| World seconds | Observed periods/seconds | Eligible FPS | Mean period ms | Observed main wait ms | Remaining period ms |
| --- | --- | --- | --- | --- | --- |
|0–10|325/9.952|32.657|30.622|9.166|21.456|
|80–120|685/39.943|17.150|58.310|38.999|19.311|
|240–300|974/59.941|16.249|61.541|42.802|18.739|

FPS is period count/summed observed period; it is not an average instantaneous
FPS. Late 42.389 ms of 42.802 ms wait overlaps the target command GPU start/end
envelope, with 0 before-GPU-start overlap. This locates the waiting boundary;
an envelope does not isolate active hardware or a particular shader/pass.

Independent Overview associated cadence corroborates 32.697→16.245 FPS while
sampled main CPU falls 57.81→25.90% of one core. Decoder work 20.579 CPU-s in the
initial 10 s is effectively 0 late. Thus continuing image decode is not the late
normal bottleneck. Late 60 s disjoint main scopes are sun shadow 4.159 CPU-s,
ordinary probes 3.395, deferred geometry 3.202. Inclusive shadow capture 3.433
and source publication 1.299 overlap those scopes and cannot be added.

Exact native results/source/inputs are under `checks/runtime-analysis-01`;
Overview report 9fe63d72/receipt 7bf98287 under `checks/runtime-overview-analysis-01`
retain exports, clock/PID/source checks, phases, commands and failures. PPD-385
wrong-footer-selector failure and helper-archive erratum are preserved; all
corrected CPU/GPU reductions/range checks exit0 without another recording.

## Completed GPU replays and exact prepared-consumer evidence

| Capture | Replay ms | CB/render/compute/blit | Draws | Main probes | Main candidates |
| --- | --- | --- | --- | --- | --- |
|default early223|67.91|29/42/8/17|8221|1|all_full,24eligible/0prepared|
|default late1588|110.26|10/32/7/5|6709|22|16eligible/16prepared|
|late180 early228|66.80|30/42/8/66|8122|1|all_full,24eligible/0prepared|
|late180 late3625|131.43|21/43/8/56|8969|47|16eligible/16prepared|

All are actual Medium/Overlapping controls at 3456×1916, one profile each.
Replay times are not live FPS or exclusive main-pass time. Both late actual
main hot draws 42772/42528 bind candidate PSOs and Fragment 8/9; both early draws
40704/39933 are generic without 8/9. This confirms the existing prepared path,
which was already present before this shadow/admission group. Main configured
points 10 and projectors 47 (2 shadowed), with41,921,469/229,073,073 summed scissor
pixels, are identical in all four exact native joins. Main masks are not missing
because of a pipeline/publication failure in those two inspected late draws.

| Capture | Fused FS M | Fused ALU/FS | Alpha-containing FS M | Alpha ALU/FS |
| --- | --- | --- | --- | --- |
|default early|277.630|112.28|46.201|248.17|
|default late|277.630|191.74|19.605|1233.37|
|late180 early|277.630|112.29|47.640|242.94|
|late180 late|277.630|218.04|19.600|1381.30|

The fused main base/sky/point/projector encoder shades essentially the same
number of fragments, but arithmetic per fragment rises 1.71×/1.94×. The alpha
encoder shades fewer fragments yet arithmetic per fragment rises 4.97×/5.69×.
It is labelled atmospheric haze in the capture but contains multiple shaders
and alpha work; these counters are not true haze-shader-only attribution.
Fused fragment occupancy falls 18.04→15.87/15.55%; alpha 23.23→11.95% and
21.29→10.86%. Buffer L1 reads rise 3.3–4.3× fused and 9.7–12.0× alpha.
Counter-pass occupancy/limiters are not a live whole-GPU utilization estimate.

No buffer-write explosion is observed: fused FS buffer writes remain 0, alpha
L1 writes remain ~818 KiB. That does not establish zero private stack traffic.
Hot simple/BASE/PBR/projector spill cells remain blank/unknown after profiling;
SSAO/lightmap explicitly report 0. Prepared simple/BASE allocate 222/226 GPR,
early generic 214. Cost percentages are reported shader-census fields/proxies,
not isolated elapsed durations or shares exclusively belonging to main.

Lightmap and SSAO blur invocation/ALU work stay nearly fixed. SSAO is a large
baseline cost, but its work growth does not explain this decline. Source MSL
SHA-256 239144bbd96dae880ac2994221bf80fdc3a5cfab4e2183b7b7509a732c1f2a97
and active library remain unchanged; no Reload/ablation/variant is promoted.

All 67/44/116/107 encoder rows in each of 5 categories and 109/95/115/133 shader
rows are retained without collapsing blanks/zeros or duplicate shader labels.
Labelled ordinary-probe work is present in three captures; default-late has no
such labelled work/exact ordinary native row in that interval. Late180 face 3
contains 105 probes/full 21/bounded 83 but declines 10 useful candidate consumers
for admission; main is prepared. That fallback is distinct from unsupported
pipeline/data and must be judged on complete preparation-versus-shading cost.
Actual Metal queue IDs/exhaustive queue-to-view census remain unavailable.
Declared device_all_queues is not proof of absent work outside the interval.

Late180 captures 47 main probes; native later reaches 54, so it represents the
later FPS plateau, not the exact final populated state. This coverage limit
remains explicit. All replay windows stop/close to Welcome to Xcode, CUA summary
is issued, and no further GPU work remains. PPD-386 one reflection-buffer CUA
30 s timeout/recovery is retained; it is not an engine/GPU crash. Transient
unretained buffer quicklook numbers are excluded.

Exact GPU report 913f9bf6/receipt 1ddb185b and native addendum 6a7904 are under
`gpu-replay-69258-20261118/derived`; exact native four-frame extraction 0c02cad9
is under `checks/runtime-analysis-01/final-addendum`. Original hashes/clones
and API intervals remain authoritative; successful profiles do not turn failed
Overview normal coverage gates into passes.

## Complete warning and control audit

All 395 viewer warnings across 39 emitters, plus 12 libpng warnings, are accounted
for; 0 ERROR/FATAL. Per successful run 129/129/126 WARNING across 37/37/36 sites,
plus 3 libpng each; failed Login 11/9 sites plus 3 libpng. Every site has pinned source
context, trigger, functional/performance relevance and limits in
`checks/runtime-analysis-01/warning-sites.md` and source-callsite-proof.json.
Repeated warnings are grouped, not omitted or dismissed by severity/category.

Terrain CPU-capture fallback totals 42/81/15 are present in the first world-entry
census and do not grow later; probe failures 0. Whole-second logs cannot prove
all failures precede the native marker. Encoded pass 93 is terrain 7+86 render
types; fail 14891 checks terrain prerequisites and recent lambda 14865 identifies
the category. The exact operand is unrecorded. Totals match created/abandoned
commands, without a per-event temporal proof; all committed commands complete.
PPD-125 stays open: 20 hero-publication guards per successful run and later silence/
hero_ready0 do not prove recovery. No steady FPS cause is established from those
startup warnings, and no functional closure is inferred.

Native report be4afb82, final receipt 49780002, exact-frame extraction 0c02cad9
and stable failed native 303b0644 retain all failures/errata/inputs. Overview
final-digest receipt 25bc6924 pins detailed CPU outputs while preserving earlier
report/receipt bytes; input hashes were computed during parsing, not invented
from stat-only verification. One default wait lacks its target completion at
world 322.604 s, outside headline windows; that decomposition remains unknown.

## Acceptance and next decision

Combined source/build/static correctness qualification is complete. Home's
FPS decline remains. No matched whole-viewer gain over the old build is shown;
owner visual quality, club retest and broader Apple-first acceptance remain
partial. New source reuse/admission activity is not performance acceptance.
All source/evidence stays local/private; no push is authorized.

The next focused group should separate repeated containment/traversal, valid
accepted weights/SH/radiance, shadow filtering and data-access costs on the
saved late frames, using unchanged and restored controls. Measure actual mask
rejection/accepted-tap efficacy. Include further shared derived-lighting
publication where source proof and controlled costs support an exact benefit.
Current evidence does not select earlier rejected shader prototypes or justify
lower lighting quality, more threads, broad visibility switches or a full new
frame graph as the next Home fix. Keep those broader boundaries open and
prioritize their demonstrated benefit, including club's different CPU profile.

This coherent group is ready for the owner's visual/performance feedback and
club checkpoint. Further implementation pauses at that requested boundary.

Final documentation check: `python3 tools/metal/validate_harness.py` passes
33 required files after the PPD-387 register counter/citation correction.
The failed check and corrected receipt remain under `checks/final-docs-harness-01`
and `checks/final-docs-harness-02`. Post-runtime executable/library hashes still
match the separately verified stage. No production code changed during this
acquisition/analysis handoff.
