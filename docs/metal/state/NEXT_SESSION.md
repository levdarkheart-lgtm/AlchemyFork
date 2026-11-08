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
- Code checkpoint before the evidence-only follow-up commit:
  `8f4d5654e53f0540a263c4f954ccbbc10d46a7c4`.
- Latest renderer implementation commit:
  `8f4d5654e53f0540a263c4f954ccbbc10d46a7c4`.
- Implementation began from clean synchronized commit `7a76893312`; recheck
  current branch/worktree state and never discard unexpected later edits.
- Physical host: MacBookPro18,2, M1 Max, 64 GiB unified memory, macOS 26.5.2,
  Xcode 26.6, Metal Apple family 7. `tools/metal/doctor.sh` exits 0.
- Current source-exact candidate is Xcode Release `26.4.0.68532` at
  `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
  It was built from renderer commit `8f4d5654e5`, passes deep strict signing
  and 349/349 ARM64 slice inspection, and has executable SHA-256
  `f242b041ebcba41cdc07a003b4fa040321c87368190b8aa192c803655953e9f9`.
- Packaged `almetal_builtin.metallib` SHA-256:
  `4f4778e6d6558654b52986ac4caa69b42add1ec1fe940bed331844a9f1ad97bb`.
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
Release `68530` builds, signs and verifies 349/349 ARM64 slices. No viewer
launch, visual acceptance or FPS gain is claimed. Converting the retained
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
signs and verifies 349/349 ARM64 slices. No launch, allocation capture or FPS
gain is claimed. Detailed evidence:
`docs/metal/state/archive/CURRENT-native-world-resource-churn-checkpoint-2026-11-08.md`.

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

Local raw evidence directories currently exist at:

- `/private/tmp/alchemy-metal-68521-post-fixes-ultra-20261108-1`
- `/private/tmp/alchemy-metal-base-classification-trace-20261108-1`

The evidence hashes and limitations are in:

- `docs/metal/state/archive/CURRENT-native-occlusion-anisotropy-performance-2026-11-08.md`
- `docs/metal/state/archive/CURRENT-base-light-depth-classification-2026-11-08.md`
- `docs/metal/state/archive/CURRENT-ultra-mirror-performance-diagnosis-2026-11-07.md`

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
  and `8f4d5654e5` resource-churn reductions have build/GPU-test proof but not
  owner-attended visual, allocation or performance evidence.
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

Narrow and full tests depend on the owner changed. For the latest resource
change the commands were:

```sh
cmake --build build-Darwin-ninja-os-arm64 --config Release --target almetal_tests
cmake --build build-Darwin-ninja-os-arm64 --config RelWithDebInfo --target almetal_tests
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 build-Darwin-ninja-os-arm64/almetal/Release/almetal_tests 'almetal: direct spot-shadow array slices preserve strict raw raster depth'
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 build-Darwin-ninja-os-arm64/almetal/Release/almetal_tests 'almetal: world simple pass writes the native four-attachment G-buffer'
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 build-Darwin-ninja-os-arm64/almetal/RelWithDebInfo/almetal_tests 'almetal: direct spot-shadow array slices preserve strict raw raster depth'
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 build-Darwin-ninja-os-arm64/almetal/RelWithDebInfo/almetal_tests 'almetal: world simple pass writes the native four-attachment G-buffer'
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 build-Darwin-ninja-os-arm64/almetal/Release/almetal_tests
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

1. At the next explicitly requested launch, verify ordinary-light and shadow
   continuity, then capture matched resident allocation/CPU/GPU evidence for
   retained borrowing, haze reuse, bindings and palettes across presets.
2. Trace exact-original fully resident opaque post-deferred content, state and
   ordering before attempting that optimization.
3. Add bounded native-query issued/ready/culled telemetry and run a dense-scene
   no-popping gate; live `UseOcclusion=1` alone is insufficient.
4. Run the formal source-exact, camera-pinned, quality-equivalent 300-frame
   Metal/OpenGL protocol before making a product performance claim.
5. Strict no-GL source/runtime, feature/visual parity, prolonged stability,
   independent review and the 24 FPS objective remain open. The product is not
   complete.
