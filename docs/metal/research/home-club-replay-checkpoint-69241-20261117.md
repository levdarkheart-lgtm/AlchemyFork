# 69241 Home and club diagnostic checkpoint

The owner completed Home login and club teleport on separately staged69241,
then logged out; the viewer is closed. This is a new diagnostic recording,
not a matched performance comparison. The CPU/native evidence distinguishes
Home GPU-completion backpressure from substantially more CPU-active club
rendering. Four device GPU frame archives stopped successfully. Initial Xcode
replay/profile controls and the controlled saved-frame experiments are complete.
All temporary changes were restored/discarded, GPU replay stopped and Xcode quit
normally. No shader experiment is selected. Capture metadata alone does not
prove replay contents, shader costs or compiler statistics.

## Source and retained inputs

- Builder`cb3e148ec9bc883d9160142aa71bf280229f990d`, root implementation
  `d5e93bbed22ceae4596dbb4243c705cb42a7663c`; shared indra tree
  `4008c621935d5abd374f33e83c94c1241c6bae37`.
- App`26.4.0.69241`, executable SHA256
  `c8092bcea8f59ba4e3034bf94db0d2c274fe5690971c011427cd0b3e48bfaa18`;
  metallib`fae7a43b5b48b7375f3a47ae52b6c7c7d89dd6b5f6a7456562d9e99438e223d0`.
  Production shader math is unchanged from69238.
- Private run`/Users/dekstroza/Alchemy-home-4slyg4mv`, PID91744. Native first
  Home world marker21:26:44.228206854UTC; teleport invalidation21:31:00.901897521;
  club generation2 world marker21:31:04.958681229. Cross-clock precision is
  limited by at least0.5ms TOC rounding plus the native calibration bracket.
- Owner observations: Home29→31→16FPS; club arrival31→18→19→20FPS. These are
  reported readings, not frame-correlated samples. No visual-parity answer
  was supplied. Primary log ends with Goodbye21:36:00UTC; process absence was
  observed. Actual process exit status is unknown.

The four retained device-all-queue captures cover one eligible display
interval each, with intervening ordinary probe and upload work subject to
actual replay inspection. Preexisting in-flight work may be partial:

| Scene | Early source frames | Late source frames | Native stop result |
|---|---|---|---|
| Home |223→224|1472→1473|Both stopped/error0|
| Club generation2 |4505→4506|5617→5618|Both stopped/error0|

The late triggers occur around world62s and sample the decline. They are not
continuous GPU recordings of all later frames: Home's captured25 configured
probes continue growing to29→41 during80–120s. CPU/native/log phase context
extends beyond the replayable intervals; that distinction remains explicit.

All original archives are immutable, approximately34.1GB total; exact manifests,
internal links and independent working copies are retained. Preservation receipt
`analysis/gpu/preservation-20261117T213849Z/receipt.json` SHA256
`41639af89d172a506fe42458c4ec791f0178b6473ce21fdaec3b2df684b63990`.
Only those working copies may be opened or edited in Xcode.

Initial Home unchanged controls, Medium/Overlapping:66.93ms early and129.43ms
late. Early contains11command buffers/31render/5compute/7blit encoders,
4,283draws/5dispatches; late18/40/8/8 and5,513draws/8dispatches. Both targets
are3456×2168. Xcode texture inventory2.02→8.00GiB is replay resource inventory,
not physical resident memory. Late prepared simple/BASE rows report25.27%/
22.49% under the UI heading **Cost**,222/226allocated registers and blank
Spilled Bytes; early generic simple/BASE report214/214registers. Cost shares
aggregate program/PSO usages and are not main-only or exclusive elapsed times.
The actual main draw binding proves its selected variant separately. These
different early/late workloads show growth, not an isolated preparation cost
or performance gain. Full tables/counters and final receipt are being retained.
Both Home traces completed profiling before the blank spill cells were recorded.
General documentation that an unprofiled trace lacks profiling data does not
explain these specific cells or establish the column's absence semantics.
Blank remains unknown; explicit zero values on other rows stay distinct.
Subsequent fresh comment-only Reload control128.10ms reports explicit **0bytes**
for prepared simple222GPR, prepared BASE226GPR and inspected projector188GPR.
This is a new recompiled-profile observation, not a retroactive replacement of
the original captured blank fields or a demonstrated explanation of their
absence. Its unchanged-math source and profiling evidence remain distinct.
Apple's [replay instructions](https://developer.apple.com/documentation/xcode/replaying-a-gpu-trace-file)
and [workload analysis instructions](https://developer.apple.com/documentation/xcode/analyzing-your-metal-workload)
support profiling during or after replay, including waiting for Profile to
finish. These consulted pages do not specify the missing Spilled Bytes cell's
meaning. Apple's [performance guidance](https://developer.apple.com/documentation/xcode/optimizing-gpu-performance)
also distinguishes overlapping shader-instruction sampling from detailed
per-pass counter measurements taken in isolation; those regimes are retained
separately here.

Both club unchanged profiles have also completed:50.46ms early and96.83ms late,
Medium/Overlapping. Late has17command buffers/38render/1compute/8blit encoders,
6,409draws/1dispatch, texture inventory12.57GiB/buffers2.31GiB. Exact draw
uniforms, encoder counters and source binding scopes are being qualified before
the final four-frame receipt. None of these cross-scene replay numbers is a
matched FPS comparison or an exclusive main-view GPU duration.

Fresh isolated counter rows localize main-view growth; the largest main
encoder rows are kept separate from similarly named ordinary-probe encoders:

| Main encoder metric | Home early | Home late |
|---|---:|---:|
|Fused BASE/light-chain fragment invocations|251,649,408|251,649,472|
|Fused BASE/light-chain fragment ALU instructions|29.196billion|55.372billion|
|Fused BASE/light-chain fragment occupancy|17.96%|15.65%|
|Transparency-containing haze encoder fragment invocations|59.992million|23.214million|
|Same encoder fragment ALU instructions|14.571billion|29.894billion|
|Same encoder sampler operations/invocation|7.47|16.08|
|Same encoder fragment occupancy|23.99%|12.80%|

The transparency-containing encoder performs about5.3times more ALU instructions
per invocation despite fewer invocations. Its name includes the alpha work;
this is not attribution to atmospheric haze alone. The fused lighting chain
also includes local/projector work, not exclusively BASE. Exact source223/1472
native rows keep point count9/rectangle area40,259,010 and projector count39/
two shadowed/rectangle area203,870,474 unchanged; target3456×2168 andSSR25/4
also match. Configured probes change1→25. This supports growing per-fragment
lighting work, without isolating SH, traversal, shadow, masks or cache traffic.

Club actual late draw37110 is generic High2/pair with no8/9 and probe config10.
Raw bound lighting parameters.w is exactly1.0 and environment_config.x is0.
Thus it reaches the shared helper in classic no-lobe mode; the source-level
all-disabled return has a real captured qualifying draw. Whether the compiler
already removes the unused traversal and whether an explicit return improves
cost are still unmeasured.

The mask consumer triplet is128.10→124.60→130.38ms: a modest3.59% bracket-mean
signal from one workload, unselected. Only prepared simple-shadow lookup/bit
rejection was bypassed; valid lighting and recorded resources/commands remained.
The candidate producer compute is absent from this late capture because its
mask is a cached input, so no producer creation/publication cost is measured.
The independent shared no-output guard is130.38→121.67→117.67ms: control drift
covers the variant, so it is **inconclusive**, neither a benefit nor regression.

PPD368 retains the non-atomic Undo failure caught before invalid-source Reload,
then exact explicit restoration and completed controls. Final original lookup
and lobe flags were verified, Reload/profile117.67ms completed, Stop/Don't Save/
Finished/normal Quit observed. No partial P/probe-geometry export was started.
Expected edited-source hashes are reconstructed recipes, not exported complete
editor buffers. Original captured blank spill fields remain unknown; recompiled
simple/BASE/projector rows explicitly report0B, while inspected PBRalpha254GPR
reports32B and wave/flat-above-water254GPR96B. Allocation is not per-frame traffic.

Final private GPU report`analysis/gpu-derived/report.md` SHA256
`3e2a054e7f4e63f064120397ed70d17a7920aa7cfc3089f7c607596f883e8c9e`;
receipt`a86d3540df697622bc32219e15c9cdfca775800a36d8881120a9d61654b0fcdf`
pins181small evidence hashes, full AX tables, four-frame metrics and experiment
history. UI inspection corroborates High2/pair through labels/native census and
source; exact captured function-constant values were not independently expanded.


## Actual preparation decisions

New source-tagged rows are successful CPU-encoding observations, not completed
GPU work, physical residency or per-pixel accepted-tap counts:

| Main scene/source frame | Decision | Probes | Nondefault full / bounded / empty | Eligible / prepared batches |
|---|---|---:|---|---|
|Home223|all_full|1|0 /0 /0|24 /0|
|Home1472|ready|25|20 /4 /0|16 /16|
|Club4505|all_full|2|1 /0 /0|137 /0|
|Club5617|all_full|10|9 /0 /0|93 /0|

All625 retained Home main calls inside80–120s are ready,10,000/10,000 prepared
batches. All716 club main calls in the corresponding phase are all_full,
100,511 eligible and0 prepared batches. All-full is decided before publication
admission. Raising publication credits does not make these masks useful.
Full rectangles are conservative producer results; they do not prove every
probe geometrically covers every visible surface. Actual GPU PSO/buffers8/9
does independently confirm current Home binding: actual2256-index draw30764
selects`almetal_world_pre_water_simple_shadow_candidates_fragment`/High2pair,
with arena projection at fragment8 and immutable per-view probe tile masks at9.
This does not retroactively explain generic draw38566 in the older69238 capture.

Ordinary-probe candidate publication admission denials are a distinct event:
the view still renders through its generic shader. These are not denials of
the entire ordinary-probe update group. Prepared ordinary consumers occur in
both scenes; sparse last-call zeros do not erase cumulative activity.

## CPU, completion and instrumentation

Both Overview raw recorders and all exports exit0. Both normal coverage
qualifications remain **failed** on the unchanged five-second GPU timestamp
gap guard. Actual retained common CPU/GPU world coverage is Home−56.729 to
122.938s and club−37.638 to142.201s, including login/teleport and settlement.
The largest in-world missing intervals overlap the capture-stop brackets;
post-return tails of18.5/19.2/21.8/46.9ms remain explicit. No gap threshold is
relaxed and no missing samples are reconstructed.

| Retained80–120s diagnostic phase | Home | Club |
|---|---:|---:|
|Native capture-censored period|63.946ms|55.869ms|
|Corresponding cadence|15.64FPS|17.90FPS|
|Main GPU-completion wait union|38.770ms|4.027ms|
|Period outside that wait union|25.176ms|51.843ms|
|Main running CPU samples /40s|14.154CPU-s|31.851CPU-s|
|Main running as one-core fraction|35.38%|79.63%|
|Decoder workers /40s|0.001CPU-s|50.541CPU-s|
|Exclusive leaves in capture images /40s|1.226CPU-s|1.817CPU-s|

These are diagnostic-mode timings, not a normal-rendering control or a measured
gain. Capture API calls and first post-capture periods are censored from native
cadence, but the armed capture layer continues to cost CPU outside those calls.
Inclusive instrumentation stack unions5.731/9.617CPU-s contain descendants;
they cannot be subtracted as removable overhead. Running-only CPU samples do
not measure blocked wall time. Native waits provide the separate wait evidence.
Associated GPU layer envelopes are neither exclusive pass durations nor
hardware occupancy and must not be added as costs.

Home's largest disjoint main rendering owner in this phase is ordinary-probe
work5.134CPU-s, then sun shadows3.063 and deferred geometry2.696. Club sun
shadows consume10.615CPU-s, geometry4.269 and deferred lighting4.302, with
scene sorting and continuing decoding also visible. Inclusive publication,
binding and geometry categories overlap these owners. The two scenes cannot
be assigned one exclusive bottleneck. Required-main completion targets are
typically two display frames old; small GPU-end→wait-return tails do not
support callback delivery latency as the main Home cost.

Root Overview report/receipt:
`analysis/overview/report.md`, `analysis/overview/receipt.json` SHA256
`6926a3a507149db4b22dc291318d3e7451e3ed83dec7cb86ad4bb66873905d4a`.
Exact operands, parser adaptation, original export hashes, scope and failed
coverage receipts are retained there. Native`analysis/native/findings.md`
SHA256`49125717416627246cab95398a277b2efd3a836a96c3a49605eb5722696719cc`;
final receipt`60764f2b38b26b191f163dc1a2266e1d7525a2f4b3c34d943401c4b935d89824`
pins source/decision/wait operands and the CPU join addendum.

## Logs, effective environment and bounded retention

All238 structured warnings at44 sites are source-mapped; zero ERROR/FATAL and
zero unparsed structured warnings. Three stderr libpng warnings are retained.
The complete audit groups asset delivery113 rows, transport timeouts28,
appearance25 and native hero publication20, plus every smaller category.
Asset403/timeout and animation failures are real functional/loading concerns;
they are not proved causes of Home GPU-wait growth. Mirror guards preserve
incomplete-data refusal, with no proof of later six-face recovery; PPD125/241/242
remain open. Seven additional logical warning sites relative to69238 are
inherited source paths, not demonstrated new native regressions.

All49 queried typed controls are unchanged and exactly equal to69238 Home
snapshots. Effective region environment differs: Home ambiance2/classic0/
exposure-tonemap1; club sampled ambiance0/classic1/exposure-tonemap0. This is
logged scene policy, not evidence that the owner changed graphics preferences.
The owner explicitly confirms different region lighting is expected; it is workload
context, not a defect, test blocker or reason to change either environment.
Stored settings equality does not make the two scenes equivalent workloads.
No club49-control snapshot or visual-parity confirmation is invented.

Log audit`analysis/log-audit/report.md` SHA256
`1d8fcde39e189a52e72e67a79e766cd9f12a22a5f405465d5e97113975255401`;
receipt`4eba7b038c52efa94634f1ed2e7dec57873ce63b6747a5797c3c31b6127688e5`.
Exact input copies, every warning's body hash/raw line/source site and privacy
preserving categories remain private.

Native telemetry ends at its600s duration limit21:35:46.119306188UTC,
244,354,432B,300 contention drops and zero full/budget/I/O drops. Five committed
commands remain pending at cutoff; their completions are unknown, not failed.
Requested Home/club phases precede cutoff; logs retain the subsequent quit tail.
It is event-partial, not complete process-lifetime telemetry.

## Next bounded investigation

The owner redirects emphasis to completing missing architecture, then recording
after coherent groups. All current GPU experiments are restored and unselected.
Proceed with [shared shadow publication and main completion admission](../work/active/shared-shadow-and-main-gpu-admission-20261117.md),
using correctness and complete-path cost checks before another combined stage.
The owner authorizes unattended Home login after that qualification; club and
owner visual acceptance remain for the owner’s return. Current
Home readiness rules out describing this slowdown as globally inactive masks.
General conservative-bounds usefulness, accepted lighting/data access and CPU
publication/visibility preparation remain candidates, not demonstrated fixes.
Read-only architecture audit identifies a concrete club boundary: selected group
publication excludes`sShadowRender`, while much shadow capture/sorting precedes
the existing encoding worker. Producer-sealed membership/range/resource-union
reuse is a candidate for those measured CPU owners; enabling more workers alone
does not move that earlier work. Current per-type CPU shadow dispatch threshold
is8192inputs, reflecting earlier small-run cost failures.

For Home, optional fresh-probe admission currently uses main thread CPU load
without logical-main GPU-completion feedback. A bounded completion-aware policy
could reduce competing work while preserving bootstrap, coherent continuations
and fairness; it cannot remove valid main-view accepted SH/radiance cost.
Preparing more work inside the render admission wait is unsafe without a
separate immutable dependency boundary. The source/measurement audit is private
`analysis/threading-boundary-audit/report.md` SHA256
`fcc2fd2a4dee85b432a0be9d2f3e42aed50703ca200e3fb86a5d876e292c3b0e`.

Older automatic-only specialization remains unselected: the broad qualification
failed strict water output, and its narrower direct BASE/simple fixture only
compiled; it never executed or passed cost acceptance. A current candidate-aware
BASE/simple-only closure would be new scope requiring exact bound-pack eligibility
and fresh qualification, not an already verified fix. Private audit
`analysis/automatic-specialization-audit/report.md` SHA256
`d4d53022a228d4ea4f1cc7c32123916d051db2eb76a85ce13c68eebfbc07e7fa`.
The original whole-engine architecture remains partial; no quality cut,
scene-specific tuning, new viewer run, source optimization or push follows
from this evidence alone.
