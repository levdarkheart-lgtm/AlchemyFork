# Material-table implementation and complete-cost measurements

Observed on physical M1 Max/64GiB/macOS26.5.2/Xcode26.6. Branch
codex/apple-silicon-metal, parent d0c9e2c3c241faee7d183dd72c46d09ffc8cb0ff.
At this checkpoint the implementation is uncommitted and has NOT been run in
the viewer.20FPS remains unmet; infeasibility is not established.

## Mechanism evidence and GPU limits

Private Metal3 argument-buffer fixture Bmrgf1 was actually compiled0, validated
with API/GPU0 (24 strict R32/D32 pairs/CPU alpha oracle), and timed0/168samples.
Per4096draw native encoding8slots1.178021→.471983ms(−59.93%),32slots
3.683972→.472452ms(−87.18%), all7paired blocks. Same implementation controls
were small. Four recurring disjoint materials change every draw; this is a
binding-stress mechanism, not a measured viewer distribution or FPS result.
Same old queue/default retained commands; immutable material buffer1 and
once/encoder unique useResources are included. No Metal4 queue migration.
Packet/private/tmp/alchemy-metal3-argument-materials-20261109.Bmrgf1;
host5416ff2a0e801ea4fc30522967c12b4d88b7579fd2720cf8ba184616abcda64e.
Independent t4mAC6jq/timing-summary.json SHA
f52bead02e167916dcbe8ec55863e120b51ee21ac752a91ffa447de9c96f5b71.

Separate2048² Hf64pq gate passed85 strict full-image R32/D32 pairs:64 legal
coverage/Z/slot/view combinations +21 material/mip/native-view/full-coverage
controls, actual aniso4. PBR32 was excluded because production rejects it.
Two quiet252sample runs passed schedule/thermal checks but do NOT establish
GPU nonregression. Repeat paired median penalties8legacy+4.778%,8PBR+2.214%,
32legacy+4.278%; absolute median per-pass deltas .003918/.001638/.004001ms.
32minified/linear-swizzled stratum was positive in all3blocks in EACH run;
controls also have substantial drift. These are whole-command GPU intervals,
production scratchD32DontCare, not fragment instruction timing or viewerFPS.
Packet/private/tmp/alchemy-metal3-argument-highfill-20261110.Hf64pq;
repeat independent summary SHAfa2dc34d1f1f805ef79b45c431b12e9a8c8342f285a0e0f7930ad3449eaaddea.

## Actual renderer integration and correctness

New native almetalshadowmaterials.h/.mm implement immutable8/32 tables with
actual native view/sampler keys, shared FrameData ownership across views,
previous-frame eviction,1024slot/4MiB physical-byte caps including inflight
slots, pre-encoder complete validation/retention and per-encoder declaration.
Count1 and capacity/allocation pressure use direct native bindings. Strong
command-owned entries preserve resources independently of cache/renderer;
no command/owner cycle. Standalone views also advance age on empty/direct calls.
World shader arithmetic, source caster order, geometry, palettes and per-draw
constants remain unchanged. New resource ABI uses fragmentbuffer1; direct
reference functions remain. Sampler opt-in preserves Ultra4 filtering.

Actual MSL080fba9efdb270d58f41c5a1ccc21bbace3b9d9eb52ee5d16d380fcf39e2363e;
new Release library3932018186338255b0523028441e5376393a51c2b3ddbad2ad538f1c530376c2.
Public integration packet
/private/tmp/alchemy-metal-shadow-material-integration-validation-20261110.a06ti1jw:
doctor0 outside sandbox (initial sandbox sysctl denied); initial build30756
modulecache permission failure, native67762 Werror missingbraces fixed.
FinalRelease87038=0, focusedhelper1354assertions/5cases0, world1927/1=0;
FULL Release90856=0/RWDI52314=0,430654assertions/100cases EACH with native
API+GPUvalidationON. RWDIbuild17717=0. Exact source hashes match both runs.
Tests prove direct-versus-table raw depth and actual table use, bounded fallback,
native-view/sampler/content keys, invalid admission, wrong-command/ended guards,
pending completion/abandonment, and standalone cold retirement. Genuine foreign
native-device rejection remains unexecuted on this single-GPU public factory.

## Complete renderer CPU test exposed an unacceptable cold path

Actual renderer fixture Rc4uL9 compiled0, API+GPUvalidation0 with10 actual
production R32/independent CPU-oracle pairs, quiet196samples0/allthermal0.
It calls renderSpotShadow inside its actual commandbatch, including source
validation, request/key/cache/miss work, arena publication, ownership,
declaration, constants and draw encoding. LLDrawInfo/Input construction remains
outside. Commit, GPU wait, retirement and diagnostic cleanup are separate.
Cleanup drains completed wrappers outside the next arm's measured begin.

|4096draw case|Direct median ms|Material median ms|Change|
|---|---:|---:|---:|
|Four hot materials,8slots|2.271091|1.430929|−36.99%|
|Four hot materials,32slots|7.297896|2.212375|−69.68%|
|Fresh owner,512unique32-slot keys|6.429978|13.960938|+117.12%|
|Fresh owner,4096keys/capacity fallback|6.509313|84.217522|+1193.80%|
|Single-texture caller bypass|1.125035|1.098333|Control range crosses zero|

All7paired hot cases improved; all7paired cold/capacity cases regressed. All
slow cases were retained, including14candidate samples above20ms.512keys
created512tables;4096keys created1024tables and3072 direct fallbacks, with
exact output. Warm gains DO NOT justify shipping this cold behavior.
Frozenhostd151bf7c8304ea7eb4fc3c58ef320329630e5a877a35df6e2dd923b748560e29;
packet/private/tmp/alchemy-metal-shadow-materials-cpu-20261110.Rc4uL9.
Original static library and exact source snapshot preserved there as
libalmetal-before-miss-fix.a and before-miss-fix-source; original exe retained.

## Next repair, not yet a performance result

Private reviewed miss-fastpath patch6d92262ddc425ab6780965927d67b6e26f4b0d012ebe798040ddf49dadc9f417
was applied. It creates at most one temporary argument encoder per family per
prepare AFTER admission, skips impossible free-slot scans when every slot is
cached, and latches capacity/allocation failures for remaining new keys in that
family. Existing hits, validation, actual tables and counters stay unchanged.
Temporary encoders are destroyed before returning; no persistent resource pin.
Current helperSHA08d6f13472f29d9e84c998b056da9e36ddbf35d53cee3755ff43a717143ab89a.
New packet8z5m0_du build32270=0, helper1354/5 andworld1927/1 pass. Full final
validation and unchanged complete-cost remeasurement are required; a small
native-work counter regression is being prepared privately before final build.

All viewers CLOSED/allMiddayplansrestored/TranslatorfullySTOPPED/Xcode replay
STOPPED.68601 baseline preserved/deepstrictsignature0 at
/private/tmp/alchemy-metal-68601-normal-preserved.app, exactf277ea10/61c5.
Latest ready viewer measurement remains9.833798FPS; no new viewer FPS claim.
Full parity/multi-scene/prolonged stability/strict no-GL/independent build-review
release gates remain OPEN. Root owns shared source, all GPU/runtime actions.

## Miss fast path validated; capacity-only regression remains

Rootadded actualprepare-nativeencodercreation/slotinspection counters and v2
regression1dece62b. Four distinct tables per family requireone encoder; full
cached pools requirezeroencoder/zero scans for32newkeys. Genuinely GPU-gated,
evicted slots require boundedscans acrossbothfamilies and preserveheld output.
FinalRelease11289/FULL50161 andRWDI14993/FULL7329 all0:434648assertions/101cases
EACH, native API/GPUvalidationON. Source snapshots match both final builds.

UNCHANGEDhostd151 relinkedtofinalsource, NEW Rc4uL9/miss-fastpath-1 series:
compile0/10productionR32+CPUoraclepairs0/196quiettimings0 (92821terminal0).
All7paired hot8/32 andcold512 cases improve; all7capacitycases stillregress.

|4096draw case|Direct median ms|Repaired median ms|Change|
|---|---:|---:|---:|
|Four hot,8slots|2.247358|1.358813|−39.54%|
|Four hot,32slots|7.219209|2.324302|−67.80%|
|Fresh512unique|6.506406|5.014407|−22.93%|
|Fresh4096/capacity|6.621458|14.833458|+124.02%|
|Single-texture|1.126465|1.131468|Control range crosseszero|

Compared withthe earlier unpairedcandidate run, cold512cost fell64.08% and
capacity82.39%; these cross-run reductions are not an interleavedA/B. Paired
newdirect controls determineadmission, andcapacitystillfails. Samehost/shader/
world source; onlyhelperh/mm/test changed. Newexe4e60af65/archiveece1509a/
bundle39320181 verified by51osey2g. Both code snapshots and staticarchives
preserved; newsource-snapshot +libalmetal-before-pressure-policy.a are inseries.

Next PRIVATEpolicy approved but NOT applied: whenunique NEW8/32keys exceed
maximumslots ANDeachnewkeyappears once, skipallNEWtableallocations andkeep
cachedhits. Strong native view/sampler validation union deduplicatesphysical
object checks beforeanycachewrites; admittedEntryowners builtfrompinnedunion.
No frame/view-specific heuristic orrendering change. Publicpressuretests and
an explicit newfixture expectation(4096samekeys/draws,0newtables) required.
No viewerbuild/newFPSclaim; latestviewer remains9.8338FPS. GoalACTIVE/unmet.

## Pressure policy executed; preparation cost still measurable

The reviewed policy and tests were applied. Final build27856/31677 and full
native API+GPU tests44174/71862 each exited0; BOTH optimized configurations
passed435459 assertions/102cases. Evidence packet:
`/private/tmp/alchemy-metal-shadow-material-pressure-validation-20261110.ovzpy694`.

Root ran `python3 /private/tmp/alchemy-metal-full-renderer-cpu-pressure-run-20261110.py`
with `compile`, `validate` and `timing` separately. Each actual exit0; quiet
session38778 is terminal. Ten production rawR32/independent CPU oracle pairs
pass; all196quiet samples retained. New pressure expectation changes no draw,
key, input, timing scope or schedule:4096once-only keys now create0tables and
bind4096direct requests, rather than a partial1024table prefix.

|4096draw case|Direct median ms|Policy median ms|Change|
|---|---:|---:|---:|
|Four hot,8slots|2.245716|1.361354|−39.38%|
|Four hot,32slots|7.204188|2.262188|−68.60%|
|Fresh512unique|6.481479|4.437063|−31.54%|
|Fresh4096/once-use pressure|6.450644|8.207958|+27.24%|
|Single-texture|1.120160|1.102493|Control range crosseszero|

All7paired warm8/32 and512 cases improve; all7pressure cases regress, with
paired+22.36..+29.32%. Same-backend controls crosszero. Remaining pressure
cost1.757314ms is real preparation work, not native table encoding. Do not
claim universal non-regression or ship solely on the hot-case percentage.

Precommit includes actual renderer preparation/encoding. Upstream LLDrawInfo
construction is excluded; commit, completionwait, retirement and cleanup are
separate. Fresh512 material-arm retirement1.625ms+cleanup1.385ms per2-passCB
must remain visible. Diagnostic polling and empty-command cancellation mean
these are not a pure production lifecycle total; completionwait is not GPUtime.

Strict independent parser report:
`/private/tmp/alchemy-metal-shadow-pressure-cpu-parser-20261110.gNjOWihR/ACTUAL-REVIEW.md`.
SummarySHA4cdf5e2269b999756068abb8e7f868fd57a585a4abf61680277ff7f7a03d32e3;
provenancea11bfe146454b7f17b73bbb236cd9c8ba6f8922f1a1b5dde6d6fe900283dfec3.
Actual hostf5d9885e/exe1134e112/archivef8b932d9/bundle39320181 match records;
all source epochs/raw logs/archives preserved. No new viewer build or FPS result.

Next bounded private proposal replaces only the pass-local per-material node
map with contiguous scratch and a flat collision-safe index. Preserve exact
key equality, first-source order, whole-input validation, counters, native
resource ownership and pressure policy. This was not applied/validated at that checkpoint.

## Flat preparation validated; admit only a controlled viewer trial

Root applied exact helper75dc0cb5662a5a5b90c570e859f640b43fde76c79f92c5cfd14059d8d509a0e4,
patch1aead4278dac78db4d93d9aa3da234a8210ac0edd39a121a58452e99c86bfa22.
The pass-local unique registry now owns contiguous reserved records and a flat
index. Full Key equality still decides reuse after collisions; native validation,
first-source order, admission, cache TTL, command owners and counters stay intact.
Root and independent source review approve that exact epoch. Theoretical
max_size guards were explicitly not integrated; bounded viewer-input review
does not claim to normalize every impossible oversized allocation exception.

Private packetWOiDIA contains the extracted actual-source CPU collision fixture;
root verified both production source sections are byte-identical, compiled and
ran it0. It forces64same-bucket collisions plus a full-hash collision and checks
pointer stability, first-source order, duplicate identities and bounds.
New packet `/private/tmp/alchemy-metal-shadow-material-flat-validation-20261110.hixepcef`:
Releasebuild51506/FULL24530=0;RWDI build/full23445=0. Both native API+GPU suites
pass435459assertions/102cases with unchanged shader080fba9e/library39320181.

SAMEf5 benchmarkhost/input/expectations/schedule inRc4uL9/flat-scratch-1:
compile/validation26026=0, ten strict productionR32/CPUoraclepairs; quiet94009=0,
all196samples/7cases retained. Strict independent parser passes all schedule,
counter, capacity and thermal checks. Per4096draw invocation:

|Case|Direct median ms|Flat median ms|Change|
|---|---:|---:|---:|
|Four hot,8slots|2.228688|1.332558|−40.21%|
|Four hot,32slots|7.232687|2.198511|−69.60%|
|Fresh512unique|6.465614|4.454803|−31.10%|
|Fresh4096/once-use pressure|6.413687|7.868563|+22.68%|

All7paired first-three cases improve; all7pressure cases regress(+19.72..+25.15%).
Single-texture and same-backend controls crosszero. Remaining pressure cost
is1.454876ms; hot32 saves5.034176ms. Commit/retirement/diagnosticcleanup remain
separate, and the previously measured modest32-slot GPU risk is not resolved.
Old pressure source/archivef8b932d9 are preserved as source-snapshot and
libalmetal-before-flat-scratch.a before the current rebuild.

Decision: proceed to an isolated, normal viewer TRIAL against preserved68601.
This changes the earlier decision to withhold trials while cold cases doubled
or worse:512nowwins and the extreme all-one-use capacity case is bounded, but
still regresses. Only actual material-transition/cache distributions and total
frame measurements can determine net value. This is not release promotion or
a uniform non-regression claim. Preserve baseline/quality and reject a measured
whole-viewer regression. No new live FPS result exists yet.
