# Next session — covered-shadow coalescing validated; build and measure

Read AGENTS.md and CURRENT.md completely. The unbudgeted20FPS Ultra goal is
ACTIVE/unmet. Previous turn made progress: source optimization, native GPU
regressions, Apple-primary research and actual Metal4 interoperability proof.
Do not create a new goal, mark complete, or declare hardware infeasibility.

## Ownership and runtime

- Branch: codex/apple-silicon-metal. Root owns ALL shared source/docs/runtime,
  builds, GPU experiments and UI. Agents prepare/review private files only.
- ALL viewers CLOSED. Exact50766/SIGTERM/Goodbye/PID absence29248=0; continuous
  plan restored0. ALL older setup plans restored. Translator FULLY STOPPED;
  NEVER restart/resume it. Xcode GPU replay STOPPED. No capture/profile jobs.
- Same private location/numericPreset123 ONLY. No travel or avatar movement.
  Preserve Ultra3456×2168/Midday/six hero1024faces/two cascades/PCSS/raw-depth/
  reverseZ/clamp/alpha/caster behavior. Prefer Mac shortcuts/fullscreen BEFORE
  login. Autonomous quit/build/login/measure is already authorized.
- No private uploads, releases, cache wiping or unrequested system changes.

## Current implementation and exact next build

Renderer5e1ecb76459de8a0bc90dc16da56aec75f1fc02d implements conservative adjacent
STATIC COVERED shadow-draw coalescing. Test-only96af75c0d978b1c61c64b2b74ef9aa239ba376bc
adds independent actual encoder draw/commit counter checks. No shader change;
MSL SHA920d2e2d927b94a43cd396478bfa3776bc2edccaf5e5dd3ef8eb10bbbfcb1fa1.

All source records still undergo complete validation before encoder creation.
Only complete contiguous triangles with equal geometry/offsets/range/index type,
coverage1–6/count/cull/MVP/texturematrix/allcutoffs/PBRtransforms/usedtexture
identities merge. No sorting/remapping or caster omission. Avatar, rigged,
terrain and ICB retain original emission. Source exact1eb shadowAlphaMask and
PBR shader contracts inspected; alpha bias/stipple/depth math unchanged.
Separate covered_input_draws/covered_coalesced_draws success-published counters
and bounded main/probe log fields expose actual eligibility. No live gain yet.

Both optimized full native API+GPU suites428946assertions/95cases PASS;
final focused1747/1 PASS each with ACTUAL encoder counts and no new commits or
draws/retained output changes on later invalid records. Packet:
/private/tmp/alchemy-metal-covered-shadow-validation-20261109.ainfndy_.
Release47183=0,RWDI47338=0,finalfocusedRelease0/RWDI0; no competing GPU jobs.
Source-only reviews approved twice. Initial reverse-depth fixture issue fixed
before GPU execution; do not invent an executed red failure.

NEXT after this documentation commit:
python3 /private/tmp/alchemy-metal-covered-shadow-app-build-20261109.py
Wrapper records exact source and each doctor/configRECORDOFF/fullbuild/verify
exit in a new private packet (pointer covered-shadow-app-build-latest-path).
Do not launch an unverified or unpinned candidate. Then prepare pinned private
QA config, fresh Midday setup plan, launch exact app/fullscreen/login/fresh43
control/location/binary gates. First check shadow_covered/probe_shadow_covered
merged/input counters; if no meaningful eligible population, do not spend a
long run claiming a speedup. If useful, collect >=300 ready stable frames with
same quality and compare the source-exact baseline. No profiler/capture/build
or unrelated GPU work during timing. No blanket FPS forecast from synthetic
merges. Public scalar oracle uses scene.bulk_draws=false; viewer defaults true.

## Preserved baseline and canonical old app

Canonical app currently normal68597/source6f6207698720bc4455c43f68e007715135643074:
build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app.
It is ALSO preserved /private/tmp/alchemy-metal-68597-normal-preserved.app.
ExecutableSHA47670056bc6ee2fcb26c5b3b90959ebb384850b28efe3d2c9e983dd4420f75fa;
normalmetallib61c5dcb3fa1677818cd3ef6b069837979a00cf9cf4d56cf1594e46d011d41585.
Doctor/config/build/verify25454=0; independent94909=0/349ARM64/deepstrictsignature.
Older68594 preserved /private/tmp/alchemy-metal-68594-normal-preserved.app;
corrected64parent68588 preserved/unlaunched. Do not confuse app/source roles.
The68597 baseline already contains selected-PSO census4e833, so new covered
candidate versus68597 isolates this CPU change better than older31-neighbor
best68579. Actual assets/pose/thermal variation still require paired controls.

## Apple research and Metal4 proof — new actionable direction

Bounded primary research report:
/private/tmp/alchemy-metal-primary-performance-research-20261109.md
SHA bb30ee13b4538aa92d5f4c354bc4aa37aead929338560534111436c22d019b3c.
No matching published AGXG13/resource-list/state-emission bug found. Exact
macOS26.5.2 advisory unknown; inspected26.5/Xcode26.6 notes do not establish a
runtime fix. CAS/DWARF warnings causing FPS loss remain unproved. Normal
commands use default errorOptions, not extra encoderExecutionStatus reporting.
Apple docs explain individual binding/residency CPU cost and argument buffers.
Metal4 core supports M1 and later and can reuse existing pipeline compilation;
it is NOT restricted to M3 ray-tracing/tensor features. Runtime guard confirmed.
Explicit residency, barriers, ownership and allocator completion are mandatory.

PRIVATE proof packet /private/tmp/alchemy-metal3-metal4-covered-shadow-20261109.25axBX:
- source metal3_metal4_shadow.mm SHA cb19ab4f82335ae694c190261605ba7a5d6f2177ed8afc260bcd160215054fa1;
  compiled actual0, native API+GPU run actual0 (root-native-api-gpu-exit.json).
- SAME existing currentMSL3.1 library61c5/sameMTLRenderPipelineState and sampler,
  actual608Bconstants/two distinct MVPs at768B offsets/singletexture/64²R32+D32.
- All12coverage/Z pairs plus2cutoff positive-control cases full rawR32/D32 BITWISE
  equal; nonempty output and same-target poison avoid stale/vacuous equality.
- M3 uses real encoder-local binding cache/offset-only updates; M4 persistent
  per-stage argument tables. One fixture-only sampler supportArgumentBuffersYES
  is shared by both. Explicit residency includes PSO(MTLAllocation), buffers and
  textures, plus strong owners/sharedEvents/queuebarriers/feedback and GPUdone
  before allocator reset. Feedback header guarantees workload completion.
- No CPU timing,8/32texture,clamp,perspective,rigged/terrain/tessellation or full
  viewer Metal4 claim. Production viewer remains the existing native API path.
- trace_recheck now prepares SEPARATE quiet CPU benchmark source-only packet;
  do not mutate the proven25axBX source. N1024/4096, real constoffset changes,
  native M3 instance1/base0 calls, M4tableupdates, full precommit cost split,
  explicit residency/setup/reset/commit costs, newNparity,7counterbalanced
  ABBAblocks/two targets/samebackend controls. No execution until root release.

## Continuous startup evidence guiding priorities

Full12min packet:
/private/tmp/alchemy-metal-continuous-startup-profile-20261109.3a9on15l.
CPU14305=0,TOC33140/export16452/thermal60378=0;920867activeCPUrows/596968main,
720.726s22:20:40.373–22:32:41.099. Cached streaming analysis e50ce5df and root
startup-correlated-phases.json SHA4e46f5c4afbe2ad4516b5a428d2d23f4a726151fd7d00e13df2ff1220e0f9c55.
Do not reparse274MB export unnecessarily. Main activity fell92→79%onecore while
FPSfell; approximate aligned CPU/frame77.56→76.46ms at11.78→10.75FPS. 30s bins
use2391Hzflush anchors with~.087–1.094s lag; not an exact CPU floor. No CPU-busy
ramp. World stays on Pcores (only.001ECPU-s afterfirst30s). CPU still needs
substantial reduction for50ms/20FPS, alongside GPU work. Do not sum CPU/GPU.
Thermal Nominal ALL720.726s; user heard louder fans, not proof of throttling.
Clocks unmeasured: powermetrics requiresroot/sudo-n unavailable; no more attempts.
GPU completed-CB duration SUM grew~226→473ms/frame as probe lists grew while
~52k draw counts plateaued. Overlap/completionlag prevents activeGPU/criticalpath
attribution. CPU evidence suggests investigating GPU work/backpressure, not
proof of a specific shader's causal milliseconds.
Raw6000frames199–6198/527.46s ALLnativeexact1/fallback0/bias1, fresh43gates0.
Counter41→12→11→10; owner clarified variable initial~38vsprior160. Raw log ends
before12minCPU trace, so later bins usewidget/process/probelogs only. Readiness
observerOFF in startupdiagnostic; false fields do not mean missing probes.
Prior68597 ready300diagnostic8.159FPS is separate, not a causal comparison.
Prior68594 normal300ready9.63558FPS vsbest68579 9.60867, effectivelyflat; older
best lacks64-neighbor correctness repair. Original67611/exact1eb6.09552FPS;
full parity remains open. Older68576/77 contaminated by Translator GPU work.

## Deferred/rejected work and release state

Whole-shadow-pass parallel encoding is feasible only after main-thread immutable
packet preparation; source proposal zfT48h2n/proposal.json. Staticbulk-only is
just2.22%mainCPU and was deferred. Current covered coalescing targets scalar
coverage work. CPU shadow2.533s/10s profile partitions overlap with other scopes;
do not attribute all to bulk or source line6372. Shader/CPU line metadata has
limits; changing compilation cache is not an FPS repair.
Do not repeat unchanged rejected SH lifetimes/predecode/named accumulators,
mainbaseclassicSSR/materialsplit/layered/scalarcursor/simpletilemask rewrites.
Ambient-only rewrite has no consistent timing gain. LiveUltraSSR0. NativeD32
migration differs from rawR32reference; no epsilon waiver. Retain prior binding
cache/arenas/mainocclusion/Ultra4xaniso/boundedlights/directR32/borrowedtargets/
image-globalpressure/cubeSSAO/per-viewprobes and allfeatures/mirrors/shadows.
PBR/water functionconstant private fixtures unexecuted; do not combine changes
before controlled per-change evidence. Details and hashes are in archive
CURRENT-shader-registers-and-neighbor64-2026-11-09.md and indexed earlier history.
Full parity/multiscene/prolonged stability/strict no-GL/independent build/review
release gates remain OPEN.20FPS unmet; infeasibility unproved. No goal pause.
