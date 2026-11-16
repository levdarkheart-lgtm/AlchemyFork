# Build69187 owner checkpoint — findings for discussion

Exact renderer source92ddb0966c21677956eab3ecfe8dbee44a9ab813,
26.4.0.69187, staged executable SHA256
9ecefb4e6cf61e1308bb0343b40e8a0c31ac073170e0bf27167e933a393f7a9e.
Doctor/configure/build/static346ARM64/signature/import/retained-feature gates
pass, strict3603files/0blockers/0errors, targeted1021025assertion-executions/
210case-executions pass.8273staged app nodes are exact. These are correctness
and provenance checks, not performance acceptance. Owner closed normally.

## Scope

Private evidence root: `artifacts/metal/engine-checkpoint-startup-20261116T150121Z`.
Continuous Overview466.3998s includes prelogin, actual STATE_STARTED15:02:44,
quiet decline, actual teleport15:05:28–31, loading and settledclub through
15:09:09.565. Manual/logging markers were late; analysis uses source log and
actual position transitions. The first attempted session missed initial quiet
loading (PPD318); restarted recording covers it. No engine/profiling work
continues before owner discussion.

## Fixed contracts and remaining performance

PPD315 admits two logical main frame groups with three early transactions each,
not two commands for three transactions. Runtime peak2groups/6commands and zero
failure/cancel/width-denial counters corroborate activation. Current waits are
for older GPU work; the same-frame admission defect is repaired. PPD316 removes
duplicate Signal subscriptions while preserving ordered topology; remaining
unique/lifecycle work is real. PPD317 selects only covered shadow weak CPU
recipes;1.67millionwhole-session restores/0failures show operation, not FPSgain.
World/SIMPLE/opaque recipe proposal is preserved and unselected after cost
qualification. Original architecture and owner performance acceptance remain
partial; GPU command/visibility/general world-preparation routes remain off.

## Quiet decline

Aligned quiet windows report23.00→15.52→14.22→13.88FPS, compatible with owner
initial25→15–17 report. Main CPU falls38.3→25.5%wall; late124s have zero sampled
decoder work. Main runs on Pcores; foreground timer spans the run. GPU-associated
frame envelopes rise70.84→136.61ms and older-group waiting~23→50ms/frame-group.
GPU-end/display tail stays near4ms. Envelopes overlap buffered frames and must
not be treated as isolated GPU execution. The evidence points downstream to
render completion/backpressure, not continued decoder/main-core saturation
or final presentation waits alone. Exact quiet GPU shader/pass, clock,
bandwidth or hazard cause is not identified by Overview.

Native source cache reaches its3,892,314,112estimated-byte cap with substantial
rejection/eviction/republication. Late44quiet seconds contain0.616CPU-s native
cache and1.412CPU-s publication inclusive (overlapping). This is genuine engine
waste but does not account for~50ms/frame-group waiting by CPU cost alone.

## Club CPU and GPU

Arrival main CPU83.1%wall plus six decoder/twoMeshLOD workers; later decoding
and mesh work diminish. Late99.565s main CPU62.656s: sunshadow34.0%,geometry15.9%,
lighting12.4%,ordinaryprobe11.2%,simulation11.1%,other9.6%,UI5.1%,post0.7%.
Those partition main CPU, not GPU. Shadow encoder workers add12.183CPU-s.

The detailed club trace is1.662s, not the requested3s: raw-size guard fired,
status retained. Initial206ms workerjoin correlates with179.68ms AGX annotation/
signpost startup on a worker and must not be blamed on ordinary engine work.
Post-startup950ms gives appGPU86.29%/allGPU92.80%active; fragment78.60%,vertex31.06%,
compute4.45% overlap and are not ALU occupancy. Older-main-group waiting177.63ms
has175.68msGPU overlap; drawable/presentation waits0.045ms. Measured physical
encoder fragment means: main fusedlighting20.870ms,haze7.728,lightmap5.154,
SSAO horizontal3.228/vertical3.090. Gbuffer vertex5.401ms and repeatedshadow
vertex work are also material. These overlap; generic labels can mix main/probe
owners. No shader-line/bandwidth counters identify the precise hardware limiter.

## Warnings

223structured warnings/41callsites,0ERROR/FATAL,3PNGstderr warnings.20HTTP403
textureassets cause80stage warnings;18material callbacks fail for14assets;
28transport deadlines and4fetch failures for one away-animation asset are real
content/loading concerns.16range-response recoveries and optional/configuration
notifications have different meanings and are not independent asset failures.
20native hero/mirror warm-up warnings end15:02:45; later hero six-face recovery
is unproved and ordinary probe publication is a different owner(PPD125). Old
region map cleanup856/861 marks objects dead(PPD294); not proof of a GPU leak.
TwoICC andoneinterlace PNG warnings remainPPD312, not diagnosed FPS causes.
37of41callsites share original Alchemy warning contracts;4are native mirror
publication checks. No new crash or native admission/recipe failure is recorded.
Warnings alone neither explain the FPS curve nor certify correctness.

## Detailed private reports

- `analysis-cpu/cpu-loading-findings.md` and phase/stack/census JSON.
- `analysis-gpu/findings.md`, final-findings.json, bounded22export receipts,
  post-startup GPU summary and trace-start stall qualification.
- `analysis-warnings/report.md`,223records/3stderr/source mapping and recount.
- Parent `analysis-main-admission-census.json`, phase markers,LEAP and full logs.

No matched baseline, formal raw-frame p50/p95, full feature parity or architecture
performance acceptance. No further implementation or profiling until discussion.
