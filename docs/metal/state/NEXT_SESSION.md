# Next-session handover — code-grounded finding review and Metal repair

Use this file as the detailed continuation record for the next Alchemy Metal
session. `CURRENT.md` remains the concise authoritative state, the active
ledger remains the executable plan, and the indexed archives remain the
evidence source. This handover explains how the latest findings were checked,
fixed and measured so that the next session does not repeat or bypass that
work.

## Copy into the next session

> Continue Alchemy Metal work in
> `/Users/dekstroza/Programming/AI/Secondlife/Alchemy` on branch
> `codex/apple-silicon-metal`. Read `AGENTS.md`,
> `docs/metal/state/NEXT_SESSION.md`, `docs/metal/state/CURRENT.md`, and
> `docs/metal/work/active/metal-port.md` completely before acting. Use the
> `agent-continuity`, `original-viewer-reference`, `metal-port`, and
> `gpu-validation` skills. I will give you another claimed code finding. Do
> not accept or reject it from the description alone: inspect current HEAD,
> trace exact original viewer commit
> `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, classify the claim as confirmed,
> partial, false, or an unproven risk, and implement the smallest source-correct
> native Metal repair if confirmed. Preserve original behavior rather than
> merely removing GL-looking conditions. Add focused regression proof, run the
> appropriate real-device/full suites and source-exact build verification,
> update the state/ledger, commit and push. Do not launch/login/capture without
> my explicit owner-attended request and do not publish a release.

## Repository and artifact state

- Repository: `/Users/dekstroza/Programming/AI/Secondlife/Alchemy`.
- Branch: `codex/apple-silicon-metal`.
- Current code checkpoint: `177c443cfd` (`Restore binding cache on measured
  renderer`). It is measured renderer `8f4d5654e5` plus only the exact
  `f8af790f32` encoder-local texture/sampler binding cache; `7cd1993d8e`
  texture-pressure separation and `1682010004` projector sharing remain
  reverted, while all commits/evidence remain in history.
- Latest renderer implementation commit: `177c443cfd`.
- Implementation began from clean synchronized commit `7a76893312`; recheck
  current branch/worktree state and never discard unexpected later edits.
- Physical host: MacBookPro18,2, M1 Max, 64 GiB unified memory, macOS 26.5.2,
  Xcode 26.6, Metal Apple family 7. `tools/metal/doctor.sh` exits 0.
- Current source-exact candidate is Xcode Release `26.4.0.68543` at
  `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
  It was built from binding-cache commit `177c443cfd`, passes deep strict signing
  and 349/349 ARM64 slice inspection, and has executable SHA-256
  `dfc5b4e92e36ee23df8da327232f69b42973b492bd6f119ef85e8d131298f82d`.
- Packaged `almetal_builtin.metallib` SHA-256:
  `4f4778e6d6558654b52986ac4caa69b42add1ec1fe940bed331844a9f1ad97bb`.
- Owner-attended PID `59683` remains the already-running source-exact `68541`
  process under the pinned Ultra profile from run directory
  `artifacts/metal/qa-runs/candidate-restored-68532-68541-20261108-01`.
  The bundle path has since been rebuilt in place as `68543`; the live process
  cannot validate the binding-cache candidate. Do not terminate it without
  owner permission or claim `68543` runtime evidence from it.
- The published alpha-2 remains `26.4.0.68512-alpha2`; current source is newer.
  Do not publish another tag/release without explicit authorization.

## Exact OpenGL reference — do not substitute another viewer

- Required source behavior is original commit
  `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
- Complete reference bundle:
  `/tmp/alchemy-reference-1eb44bb/build-Darwin-ninja-os-arm64/newview/Release/Alchemy Metal Development.app`.
- Its executable SHA-256 is
  `de8fc8f817c0afb2063f641e9c24cbd58054c32c3053f4d0e863232498bb1797`.
- External validation HEAD `734afd3179` is that renderer base plus three
  instrumentation-only commits. The app contains `settings_files.xml`, is
  deeply signed and passes 349/349 ARM64 verification.
- Never use `/Applications/Alchemy Beta.app` for acceptance comparison. It is
  version `7.1.9.2516`, SHA-256
  `a361437f58866d9f41b029b21048cebc6dee635ee8678e41d40dfa5a20daa75a`,
  and caused an earlier invalid comparison.
- The old incomplete reference survives only at
  `/tmp/alchemy-reference-67607-incomplete-backup.app`; do not launch it.
- The reference executable is confusingly named “Alchemy Metal Development”
  even though this exact bundle is the OpenGL reference. Validate by full path
  and hash, not window title.

## Required finding-to-fix method

For every new claimed issue, use this order:

1. Confirm branch, HEAD, clean worktree and host. Read the applicable current
   owners and callers; GitHub line links in a finding may point at an older
   commit and are not current evidence.
2. Identify the exact behavioral contract in original commit `1eb44bbd1b` with
   `git show`, `git log`, `git blame` and nearby call sites. Record the source
   blob/hash when the interpretation matters.
3. Trace history around the Metal insertion. Determine whether a GL field or
   predicate represents actual behavior, merely legacy object identity, or an
   implicit GL lifetime/synchronization guarantee.
4. Classify each part separately: confirmed bug, settings mismatch,
   correctness risk needing proof, performance hypothesis, or false claim.
   Never turn “could be expensive” into “caused the observed FPS” without a
   measurement.
5. Implement the smallest direct native Metal behavior at the current owner.
   Do not re-enable GL, create a fallback backend, change visual quality to win
   FPS, or “fix” a predicate while leaving the missing native mechanism absent.
6. Add a focused test that fails for the actual defect. Prefer real-device
   Metal proof for resource visibility, ordering, format or lifetime. Also run
   assertion-enabled RelWithDebInfo where release assertions could hide a
   precondition or accounting failure.
7. Run the narrow test, full owning suite, full real-device Metal suite when
   affected, then a warnings-as-errors product build and bundle verification.
8. Separate implementation and evidence commits when useful: commit the code,
   rebuild source-exact from that code commit, then record hashes/evidence in a
   documentation commit.
9. Update `CURRENT.md` and `metal-port.md` without exceeding their 160/180-line
   recovery limits. Move detail into an indexed archive checkpoint.
10. Make no visual or performance claim from compilation/tests alone. Runtime
    viewer work is owner-attended and must use exact executable paths.

This source-first method prevented three plausible but wrong shortcuts:

- simply changing `isComplete()` would not have implemented native occlusion
  results;
- merely lowering a hardcoded sampler would not have preserved live preference
  changes and literal original levels; and
- merely replacing `!mGLBuffer` would not have reproduced `glBufferData`'s
  fresh-backing lifetime.

## Recent finding investigations and repairs

### Native occlusion queries

Finding: `ALQueryPool::supports()`/`canBegin()` returned false, acquisition was
invalid, and `llviewerdisplay.cpp` disabled occlusion when the GL-only shader
predicate was incomplete. This was confirmed; frustum/distance culling still
worked, but scene/probe visibility-result rejection did not.

Commit `e52de13accac687b84a6889e1b0a391a915b9b8f` implements native Metal
visibility-result buffers rather than weakening the shader gate. Exact original
tracing established cube-fan geometry, opaque-batch issue order,
Boolean/conservative versus Counting/exact modes, asynchronous consumption and
fail-open behavior while a result is unavailable. The implementation covers
main scene and reflection-probe queries and attaches tickets only after the
submission exists. Evidence commits are `7bf46083de` and `cbf4e31ca9`.

Focused query proof passes 49 assertions; full Release llrender and real-device
Metal suites passed. Release `68519` built, signed and verified. Attended
Release `68521` showed live `UseOcclusion=1`, but query issued/ready/culled
counters were not captured. Do not claim culling effectiveness or its isolated
FPS gain until that telemetry and a dense-scene no-popping check exist.

### Live anisotropy preference

Finding: native persistent world and terrain samplers forced 16× while Ultra
requested literal `RenderAnisotropicLevel=4`; lower presets request `2` or `0`.
Confirmed. Original code proves these settings are filtering levels, not
indexes. `0` means anisotropy disabled, represented in Metal as
`maxAnisotropy=1`, followed by literal `2/4/8/16`.

Commit `f7992629714efc3fc531affa6974c06ab26e9c97` routes the effective live
preference into persistent world/terrain/sky/probe samplers and replaces the
affected descriptors when it changes; it does not rebuild shaders. Mapping,
state and real-device replacement proofs pass. Attended `68521` confirmed live
Ultra value `4`.

### Vertex/index uploads and shared-buffer lifetime

Finding: native `_unmapBuffer()` used `!mGLBuffer`/`!mGLIndices`, which are
always true under Metal ownership, and directly overwrote one shared
`MTLBuffer`. Confirmed as redundant selective-upload loss plus a correctness
risk; it did not prove that every static mesh uploaded every frame or that this
caused the observed Ultra FPS.

Exact original source uploads only the written store plus an uninitialized GL
store. Repeated `glBufferData` keeps the name but gives it fresh backing, so
queued draws keep the old bytes without a CPU/GPU wait. Commit
`e90ccdf18f076cc3beb6d8fe97e4dd045c8d2cb5` therefore:

- gives vertex and index stores independent native initialization state;
- preserves independent dirty uploads;
- rotates fresh or completed reusable Metal generations on later writes;
- retires old generations against `ALFrameFences` rather than waiting;
- preserves raw wrappers captured before resize/destruction until completion;
- keeps failed uploads dirty for retry; and
- fixes native index resize to release the CPU store despite a zero GL name.

The real-device regression test deliberately changes an unmarked CPU index
shadow during a vertex-only update, verifies index identity/content stay
unchanged, verifies queued GPU work reads old vertex bytes, proves completed
generation reuse, checks native resize accounting, and destroys
`LLVertexBuffer` before encoding through a captured pointer. Focused Release
and RelWithDebInfo each pass 46 assertions; full Release llrender passes
1,938/1,938 assertions across 226 cases; full Metal passes 52,842/52,842 across
59 cases. Runtime Low/Mid/High/Ultra smoke remains open, and no FPS gain is
claimed.

Detailed evidence:
`docs/metal/state/archive/CURRENT-native-vertex-buffer-generations-checkpoint-2026-11-08.md`.

### Ordinary deferred point-light coverage

Finding: the native renderer collected all qualifying ordinary lights into one
full-target draw and looped over that complete list in every geometry fragment,
whereas exact original uses bounded cube volumes for camera-outside lights and
full-screen batches only for lights surrounding the camera. Confirmed.

Commit `8247d09c5d77f6fcb82a8f1451aaba40953230fe` preserves the original
camera-inside predicate and maximum-16 inside batches. Camera-outside lights use
conservative projected cube scissors and one-light buffer ranges; fragment
lighting math remains unchanged. Focused Release/RelWithDebInfo validated GPU
tests each pass 1,990 assertions, full validated Release Metal passes 52,861
assertions/59 cases, and source-exact Release `68528` builds, signs and verifies
349/349 ARM64 slices. No viewer launch or FPS improvement is claimed.

Detailed evidence:
`docs/metal/state/archive/CURRENT-bounded-local-point-lights-checkpoint-2026-11-08.md`.

### Direct retained shadow slices

Finding: directional and projector shadows used a pooled R32 color plus D32
depth target, exported raw depth through the opaque fragment shader, then
copied R32 into a persistent array slice. Confirmed. The D32 store action was
already `dont_care`, so the issue was the extra color representation, fragment
destination and explicit copy, not a second stored depth surface. Hero batching
did not remove the copy; ordinary probe and main shadow pairs also remain less
fully transacted than the later main-world work.

Exact original `1eb44bbd1b` binds and renders each final directional/projector
shadow target directly; its `pipeline.cpp` and `alrenderresourceplan.cpp`
SHA-256 values are respectively
`bb52adcb83cf493d57246b6410294bb8579f429454b024aeae1ae66efa23231b` and
`398bbc83dbac2743a5bcd3809994029ec4864f47540ac73ca68a857cc213a6fd`.
Commit `f93c9f192709cac151bfd76f4eccb20cdf262d67` makes borrowed array slices
valid color attachments, allocates retained R32 arrays with render-target plus
shader-read usage, and renders raw depth directly into the selected slice with
a reusable private D32 test texture. The existing fragment output, pipelines,
reverse-Z, clamping, alpha coverage, caster admission and raw-depth PCSS
sampling remain unchanged. Diagnostic readback now runs only after a committed
non-batched pass.

Focused validated Release and RelWithDebInfo tests each pass 129 assertions;
full validated Release Metal passes 52,862 assertions/59 cases. Source-exact
Release `68530` builds, signs and verifies 349/349 ARM64 slices. Combined
owner-attended Ultra `68532` evidence below confirms that the former retention
copy submission stream is absent; visual acceptance and isolated FPS gain are
not claimed. Converting the retained
representation to sampled depth arrays and depth-only opaque pipelines remains
a separate change because it alters every raw-depth consumer and coverage path.

### Retained world resources and per-draw setup

Findings: the retained Main/Probe/Hero G-buffers and direct renderer both
allocated physical attachments; water and atmospheric haze held two sequential
full-resolution depth snapshots; material/PBR batches bound 32 base slots
before overwriting most of them; and shadow/G-buffer skinning created palette
buffers per invocation. Confirmed. This was not a second scene render, and the
haze copy/pass boundary is still required by the current read/write contract.

Exact original `1eb44bbd1b` renders deferred geometry into retained
`deferredScreen` and caps indexed legacy/glTF material families at eight slots.
Its `pipeline.cpp`, resource-plan and `llglslshader.h` SHA-256 values are
`bb52adcb83cf493d57246b6410294bb8579f429454b024aeae1ae66efa23231b`,
`398bbc83dbac2743a5bcd3809994029ec4864f47540ac73ca68a857cc213a6fd` and
`93edb05d16724dd632f0f21af215e600c4fbafa772bfc391c9e12b5e11a8424a`.

Commit `8f4d5654e53f0540a263c4f954ccbbc10d46a7c4` strictly borrows compatible
four-color retained G-buffer attachments, alternates with the pool for in-flight
safety, shares the sequential haze snapshot, binds eight material/PBR base
textures and uses completion-safe inline command data for palettes through
4,096 bytes (85 rigged joints). Emissive-disabled three-color plans, larger and
diagnostic palettes, retained screen/post targets, other constants and both
haze copies/pass boundaries remain.

Validated Release/RelWithDebInfo G-buffer and shadow cases pass 8,075/129
assertions each under Metal API/GPU validation; full validated Release Metal
passes 52,869 assertions/59 cases. Source-exact Xcode Release `68532` builds,
signs and verifies 349/349 ARM64 slices. Its combined owner-attended Ultra
measurement is recorded below. Detailed source/validation and runtime evidence:
`docs/metal/state/archive/CURRENT-native-world-resource-churn-checkpoint-2026-11-08.md`
and `docs/metal/state/archive/CURRENT-native-world-runtime-measurement-checkpoint-2026-11-08.md`.

### Texture versus global Metal memory pressure

Finding: the Metal streamer derived a deliberately bounded image capacity from
`recommendedMaxWorkingSetSize / 16`, then compared its 2.90-GiB post-headroom
target with `max(texture + biased vertex estimate, currentAllocatedSize)`.
Confirmed. `currentAllocatedSize` includes all device resources, while the
small target is a streamed-image policy. Registered and live
`RenderTextureVRAMDivisor` is `1`; the constructor's `2` is only a fallback.

Exact original `1eb44bbd1b` compares its tracked texture plus 2x-biased vertex
estimate with one dedicated `gGLManager.mVRAM` pool. Its
`llviewertexture.cpp` SHA-256 is
`27b94a757e9cdbba99d3a5b2b07d6ab592455c4546e5191b6bad5c323b806c61`.
Conversion commit `ab98aae4a9` introduced the mismatched Metal maximum.

In the still-running pre-fix `68532` Ultra process, 86 settled samples recorded
`2,947.87–3,066.48 MiB` tracked texture bytes, `4,807.58–4,931.12 MiB` total
device allocation and discard bias `4`. The targets were respectively
`2,969.6 MiB` and the full `59,392 MiB` recommendation. This proves the
pressure mechanism was active but does not prove fetched-image decode/upload
oscillation or an FPS cost; generic wrapper churn is not sufficient attribution.

Commit `7cd1993d8ed7a3ba04beb200e7dc007ecae19e0c` pairs Metal tracked texture
bytes with the existing bounded texture target and total device allocation with
the full recommendation, taking the worse signal only under real global
overcommit. System-memory pressure remains independent, exact-original GL
accounting is preserved, a zero divisor is clamped safely, and the texture
allowance is not inflated to unified-memory size. Focused proof passes 22
assertions/6 cases; full viewer tests pass 99,012 assertions/752 cases with one
expected skip. Release `68535` builds, signs and verifies 349/349 ARM64 slices.

Detailed evidence:
`docs/metal/state/archive/CURRENT-native-texture-pressure-accounting-checkpoint-2026-11-08.md`.
Fresh-launch `68537` before evaluating post-fix discard bias. If the next goal
is cache-churn attribution, instrument fetched-image decode/upload/downscale
events rather than inferring them from aggregate Metal resource counters.

### Probe projector sharing, filtering and bounded encoder cleanup

The owner-supplied assessment identified two additional projector differences.
Both are confirmed against exact original `1eb44bbd1b`. Original
`generateSunShadow()` recomputes projector matrices for each cube face but
guards caster camera/cull/cache/draw with `!gCubeSnapshot`; its shared shadow
utility also takes the cube single-tap shortcut before PCSS for both sun and
spot maps. Metal instead rebuilt a probe-owner projector pair and omitted the
projector flag.

Commit `16820100044d46d66ed306dd77e8f0ca14797780` keeps per-face matrices but
borrows the main-world retained projector texture/validity during hero and
ordinary-probe captures, prevents cube faces from entering projector caster
cull/draw, publishes `probe_single_tap`, and accepts that shared flag in
projector validation. It also skips G-buffer visibility depth/fill restoration
when no query changed state, and renames broad capture labels to describe haze
plus later replay accurately. Discard-capable opaque variants remain an
unproven HSR risk requiring a capture; no coverage-semantic shader change was
made.

Focused projector and visibility cases pass 1,991 and 10 assertions; the full
real-device Metal suite passes 52,870 assertions/59 cases. Source-exact Xcode
Release `68537` builds warnings-as-errors, signs and verifies 349/349 ARM64
slices. Its executable SHA-256 is
`60c6e2772eff0c51f76db3a5e36782c40d9ce2e3662db9fa77d39f77e9985c6b`.
Owner-attended projector continuity and zero cube-view caster-traversal/draw
proof remain open; no visual or FPS gain is claimed.

Detailed evidence:
`docs/metal/state/archive/CURRENT-probe-projector-sharing-checkpoint-2026-11-08.md`.

### Ultra 68537 regression and exact-original binding cache

The owner confirmed the live scene and fixed camera matched the settled
`68532` run. Under a fresh 21-control gate, `68537` recorded 5.188 traced
presents/s and 6.0 median log FPS versus `68532` at 8.633/9.9. This is a real
bounded regression signal, not scene variance. GPU-active union fell to 57.5%
and labelled hero GPU time fell 17.1%, while hero-face CPU encoder time rose
379.7% and sampled standard `setFragmentTexture` stacks rose from 3,305 to
6,117 ms. Projector sharing worked: hero direct-spot interval count fell to the
expected 24.063/present.

The earlier 32-base-slot claim was already repaired: current material/PBR
batches bind eight base diffuse slots. A separate exact-original discrepancy
remained. Original `ALTextureSlot` skips unchanged texture and sampler
identities; `ALMetalRenderEncoder` forwarded every setter. Commit `f8af790f32`
adds encoder-local native texture/sampler handle caches, preserving distinct
linear and sRGB views and per-pass reset semantics.

The full real-device Metal suite still passes 52,870 assertions/59 cases.
Source-exact Release `68539` builds warnings-as-errors, signs and verifies
349/349 ARM64 slices. Executable/metallib SHA-256 values are
`427414330ab546c034536fc27227c1bad335726290bc963a1eb0087fe631a6cb` and
`4f4778e6d6558654b52986ac4caa69b42add1ec1fe940bed331844a9f1ad97bb`.

The first `68539` trace was rejected because discard bias was still converging
and it contained 1,062 immediate scale-down submissions. After stabilization,
the repeated trace recorded 6.009 presents/s, 29.935 submissions/present,
56.36% GPU-active union, 4.917–4.998 GiB allocation and Nominal thermals;
twenty pretrace log samples had median 7.1 FPS. Versus `68537`, traced/log FPS
rose 15.8%/18.3%, sampled standard texture binding fell 23.4%, and hero/main
encoder time fell 21.3%/19.6%. The cache is therefore a measured partial
recovery.

It remains 30.4% below `68532` in traced presents/s. Hero labelled GPU interval
count/time are similar to `68532`, but hero CPU encoder time remains 277.7%
higher and texture-binding samples remain 41.8% higher. Next quantify
issued/skipped bindings and palette/constant allocations per view; do not
revert projector sharing or claim the binding cache closed the regression.

Raw settled evidence is under
`/private/tmp/alchemy-metal-68539-binding-cache-ultra-measurement-20261108-2`;
summary SHA-256 is
`a67d1d2c0772a6edb83c429a6c6d24c36635a3c275f0576460f8a59cd1c0ccc1`.
Detailed evidence:
`docs/metal/state/archive/CURRENT-68537-texture-binding-runtime-checkpoint-2026-11-08.md`.

## Other important corrections already completed

- `af65f062a1`: directional shadows. Original sparse null GLTF material slots
  are batch gaps, and streaming textures use the default image; Metal had
  invalidated whole cascades. Four populated cascades and moving shadows were
  observed in Release `68502`.
- `8a201381e2`: High SSAO noise. Original tiled noise is `GL_RGB16F`; the
  source-correct native resource is `rg16_float`, not the stale `rg32_float`
  consumer check. The apparent High “freeze” was a repeated fail-closed frame
  loop, not a deadlock.
- `921aed5ca9`: Ultra mirror shadow validation. Resolution comes from the live
  retained depth texture, and only the original probe single-tap path is exempt
  from PCSS scale. Changing presets can disable Mirrors by design; High doing
  so was not a mirror-renderer failure.
- `c9ab1af180`: batches mirror face shadow work into the hero transaction,
  reducing command buffers/present by 30.2% in the bounded comparison.
- `3bd0451efcf09ee665b62aa7532e922bd4e33b4f`: applies original camera-outside
  projector volume bounds as conservative Metal scissors with full-target
  fallback at the eye plane. Main/hero projector labelled work fell
  78.9%/86.7% in the bounded traces.
- `c436b9fe6eba07e31400ca65883445d33cef0c86`: restores original deferred-soften
  depth classification, using the exact G-buffer depth so clear sky avoids the
  expensive geometry shader. Main base-light labelled work fell from 11.470 to
  2.688 ms/present (-76.6%) in that bounded trace.
- Two haze experiments were measured and rejected: shader specialization was
  +3.2%, and fractional-power replacement was +3.9% in labelled haze time.
  Both were reverted. Preserve negative results; do not reintroduce them.
- `1e962c84e2dbf87939955a350dc52cddedc3338e`: benchmark collection now requires
  both 120 eligible warm-up frames and 30 elapsed seconds by default.

## How performance was measured and what is admissible

The owner expected approximately 24 FPS at Ultra on the M1 Max. That objective
remains open. Recent 5–9 FPS observations are not considered satisfactory, but
neither nominal “pure Metal” nor a single FPS counter establishes where time
went.

### Resident-workload rule

The viewer can start above 100 FPS and count down over roughly 20 seconds as
world, shadows, six hero faces, probes, textures and pipelines become resident.
In attended build `68516`, FPS was 22.8 before hero radiance, fell to 9.1 when
that workload published, and settled at 8.2–8.9 while Metal allocation rose
from 1.87 GiB to about 4.9 GiB. Selecting Midday afterward held 8.3–8.7; the
environment click did not cause a second collapse. Never use startup FPS or a
frame-count-only warm-up as steady-state evidence.

Wait for both the configured elapsed/frame thresholds and stable residency.
If FPS continues to decline after the threshold, keep waiting and document it.

### Capture protocol used

- Record exact Git SHA, executable path/hash, drawable, AC power, thermal
  state, region/global position, active camera/preset, all compared graphics
  controls and live viewer process path.
- Verify active settings after launch; do not assume a preset retained Mirrors,
  anisotropy, occlusion, exposure or draw distance.
- Use a fully resident approximately 30-second Metal System Trace and Time
  Profiler capture, then export present rows, GPU labelled intervals and
  command-buffer submissions. Preserve export hashes.
- Normalize a labelled channel as summed labelled GPU interval duration divided
  by captured presents. These labelled intervals can overlap and are not GPU
  frame time; encoder durations and inclusive CPU categories also overlap.
- Use the viewer's settled ten-second log samples for a bounded median/mean/range.
  The formal gate still requires the quality-equivalent, source-exact,
  camera-pinned 300-consecutive-frame protocol in `PERFORMANCE.md`.
- Compare only matched quality and scene state. A live Second Life region is
  nondeterministic, so any missing camera transform, unmatched residency,
  changed avatar/content or nonconsecutive aggregation must be stated as a
  limitation.
- Raw live-scene captures remain local; do not upload them.

### Latest settled bounded evidence

Release `68521`, after longer residency, recorded a 30.695-second trace with
163 presents (`5.310` traced presents/s), 6,315 submissions
(`38.74`/present), `77.0%` GPU-active union, 4.778–4.910 GiB allocation and
Nominal thermals. Fifty-four subsequent ten-second samples had median `6.35`,
mean `6.322`, range `6.1–6.5` FPS.

The closest predecessor `68515` had 21 samples at median `5.9`, mean `5.833`,
range `5.6–5.9`. The apparent +7.63% median/+8.38% mean is directional only:
the predecessor was not retained as a final source-exact artifact, camera
transform was not captured, the scene is live, and these were aggregates. Do
not assign that change separately to occlusion or anisotropy.

Time Profiler inclusive main-thread attribution was:

| Category | Share |
| --- | ---: |
| Sun/shadow generation | 56.4% |
| Native spot-shadow encoding | 35.0% |
| Hero/reflection-probe work | 32.1% |
| Main G-buffer capture | 18.1% |
| Cull traversal | 6.4% |
| State sort | 2.5% |
| Post sort | 2.4% |
| Occlusion-query mechanics | 0.4% |

The settled GPU-labelled channels were opaque post-deferred `55.307`, haze
`17.544`, base light `17.267`, light map `5.365`, projector `3.864`, local
light `3.566`, SSAO horizontal/vertical `3.014/2.966` ms per present. Because
the earlier trace started around 32 seconds after launch and contained only
`0.579` ms/present opaque-post work, it was not fully resident. It cannot prove
that haze/base light is the next priority. Trace exact-original fully resident
opaque post-deferred content/state/order before optimizing it.

Release `68532` was then measured owner-attended at the same `3456x2168`
drawable after the owner selected Ultra and fixed the camera. A settled
30.697-second repeat recorded 265 presents (`8.633` traced presents/s), 8,401
submissions (`31.702`/present), `80.5%` GPU-active union, 4.661–4.795 GiB Metal
allocation and Nominal thermals. Thirty-seven untraced ten-second samples had
median/mean `9.9`/`9.746` and range `7.2–10.5` FPS.

This is direct structural evidence for the combined repair set: the prior
shadow-retention copy stream fell from `7.589` submissions and `8.002` labelled
GPU ms per present to zero, while direct spot-shadow renders increased from
`7.583` to `8.211` per present and hero transactions remained six. Total
submissions fell 18.2%; allocation floor/ceiling were directionally about
120/117 MiB lower. Time Profiler sampled standard `setFragmentTexture` stacks
at 16.6% versus the prior 20.1% and native buffer creation at 2 versus 9 ms,
directionally consistent with the eight-slot ladder and inline palettes.

Do not call the present/FPS difference a product gain: the previous camera
transform was not preserved, live content and source differ, and opaque-post
work changed from `55.307` to `1.386` labelled ms/present. The launch manifest
was Mid and the owner changed it to Ultra manually, so the formal visual-QA
gate remained blocked; no agent screenshot or visual conclusion was produced.

Local raw evidence directories currently exist at:

- `/private/tmp/alchemy-metal-68521-post-fixes-ultra-20261108-1`
- `/private/tmp/alchemy-metal-base-classification-trace-20261108-1`
- `/private/tmp/alchemy-metal-68532-resource-churn-ultra-20261108-1`

The evidence hashes and limitations are in:

- `docs/metal/state/archive/CURRENT-native-occlusion-anisotropy-performance-2026-11-08.md`
- `docs/metal/state/archive/CURRENT-base-light-depth-classification-2026-11-08.md`
- `docs/metal/state/archive/CURRENT-ultra-mirror-performance-diagnosis-2026-11-07.md`
- `docs/metal/state/archive/CURRENT-native-world-runtime-measurement-checkpoint-2026-11-08.md`

## Visual/reference state

- The controlled Mid visual setup used `Preset123`, Spare Time around
  `(105,21,24)`, Mid, 128 m, HDR/emissive, Sun/Moon + Projector shadows,
  mirrors 512/every third frame, ACES exposure 1.0/mix 0.7, sharpening 0 and
  Midday/Lightbox. Exposure was verified as 1.0 in both viewers.
- The owner accepted broad directional-shadow, sky, lighting and color
  similarity enough to continue. This was bounded owner evidence, not a formal
  visual gate pass.
- The mirror itself works. Moving to High disables Mirrors as original preset
  behavior. A blue-gray Metal versus green-terrain OpenGL upper reflected
  background remains an open content/clipping question; trace original mirror
  terrain/content before changing it.
- Game Mode appeared correctly. It is not an open issue.
- Latest geometry generations, bounded ordinary lights, direct shadow slices
  and `8f4d5654e5` resource-churn reductions have build/GPU-test proof plus the
  combined bounded Ultra runtime evidence above. Ordinary-light camera-boundary
  and moving-shadow visual smoke remain open.
- The latest process launched with the Mid QA manifest before the owner changed
  it to Ultra and fixed the camera. Treat its runtime probes/traces as bounded
  evidence only; the fail-closed screenshot gate was blocked and no visual
  comparison was made.
- Use `alchemy-viewer-visual-qa` and exact executable-path/process checks for
  any new reference/candidate launch. Launch/login/capture only on explicit
  owner request.

## Validation commands and practical notes

Start each implementation with:

```sh
git status --short --branch
git rev-parse HEAD
tools/metal/doctor.sh
```

Narrow and full tests depend on the owner changed. For the latest
texture-pressure change the commands were:

```sh
cmake --build build-Darwin-ninja-os-arm64 --config Release --target newview_tests
build-Darwin-ninja-os-arm64/newview/Release/newview_tests '[texture][pressure]'
build-Darwin-ninja-os-arm64/newview/Release/newview_tests
```

Darwin Metal-only configuration intentionally omits legacy GL test binaries;
do not treat the absent `llrender_gl_shared_tests` target as a product failure.
Put new native tests in the Darwin native test list.

Product/build evidence:

```sh
tools/metal/build.sh build
tools/metal/verify_app.sh \
  "build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app"
codesign --verify --deep --strict \
  "build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app"
```

The Xcode wrapper may perform a full warnings-as-errors rebuild after CMake
test-list changes. After committing renderer code, run it again so the final
artifact metadata is source-exact; the second build is normally incremental
apart from staging/signing.

After documentation/harness changes:

```sh
python3 tools/metal/validate_harness.py
```

After tool changes:

```sh
python3 -m unittest discover -s tools/metal/tests -v
```

Keep `CURRENT.md` at or below 160 lines and the active ledger at or below 180.
Use `apply_patch` for edits, preserve unrelated work, run `git diff --check`,
then commit coherent changes and push the feature branch. Do not force-push,
merge the default branch, publish releases or upload private traces without
explicit owner authorization.

## Immediate next boundaries

1. Against the `68532`/`68537`/settled-`68539` traces, quantify issued/skipped
   texture/sampler bindings and palette/constant allocations per main, hero and
   ordinary-probe view; attribute the remaining per-present hero/shadow CPU
   expansion before selecting a bounded repair.
2. Trace exact-original fully resident opaque post-deferred content, state and
   ordering before attempting that optimization.
3. Add bounded native-query issued/ready/culled telemetry and run a dense-scene
   no-popping gate; live `UseOcclusion=1` alone is insufficient.
4. Capture the camera transform and run the formal source-exact,
   quality-equivalent 300-frame Metal/OpenGL protocol; separately smoke
   ordinary-light boundaries and directional/projector shadow continuity. Use
   a matched capture before changing discard-capable opaque variants for HSR.
5. Strict no-GL source/runtime, feature/visual parity, prolonged stability,
   independent review and the 24 FPS objective remain open. The product is not
   complete.
