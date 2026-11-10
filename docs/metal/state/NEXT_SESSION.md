# Next session — implement the missing PBR alpha probe specialization

Read AGENTS.md and CURRENT.md completely. The20FPS Ultra goal is ACTIVE and
unmet; infeasibility is unproved. Latest retained viewer result is9.785FPS.
The last implementation tested a probe-buffer address-space change and rejected
it on actual GPU cost. Do not repeat it or claim a new FPS gain.

## Current source and runtime

Branch codex/apple-silicon-metal. Inspect git HEAD/status. Retained renderer
checkpoint f549a796d2c734222665dd10c836e56c0df352b9. All four address-space
prototype files were restored exactly to stable7772da0422a9d589f1bcbd8fd5e3805531e4a5d6.
Only documentation is currently modified. Root owns shared source/docs/GPU/runtime;
Shadow/Trace prepare PRIVATE PBR source/test packets. Poll those agent tasks for
actual status and paths. No viewer/profiler/build/GPU test is live.

Translator is FULLY STOPPED: NEVER restart it. Xcode replay stopped. All earlier
viewer environment plans are restored; never restore them again. Prefer Mac
shortcuts and fullscreen BEFORE login. Never CUA-bind ControlCenter (prior7654s
stall). Owner authorizes autonomous quit/build/login/measure at SAME location/
Preset123 only, no travel or avatar movement. Keep Ultra3456×2168/Midday, all six
1024 hero faces/two cascades/PCSS/rawR32/reverseZ/clamp/casters/viewer features.
No quality cuts/nativeD32/epsilon waiver/fallback backend/private-capture uploads.

## Exact next implementation

PBR color-alpha PSOs omit the existing optional fragment bool0 despite calling
the same directional-shadow helper as already-specialized SIMPLE/water. Both
standard and custom factories omit it. Exact original1eb PBRalpha calls
sampleDirectionalShadowGrad, whose cube_snapshot branch is single-tap.

Existing evidence supports PBR alpha before light-map:18hero light-map stage
reports median0.020687ms/max0.252459; atmospheric/alpha aggregates face1~4ms/
face3~3ms and earlier hero1 census175PBR versus57legacy-material batches.
These are different sampled frames and not exclusive PBR fragment cost.

Shadow is preparing PRIVATE3-file core against stable7772water:
- scene.probe_pbr_shadow_specialization defaultsfalse;
- lazy8 HDR/coverage/rigged standard PSO clones restore owned library and change
  ONLY FC0=true; custom PBR uses existing shadow-mode cache identity;
- activate only actual shadowed PBR color, not avatar/DoF-depth, validated probe
  pack count>1 and actual bound directional pack probe_single_tap bit4;
- existing opaquePostDiagnostics gains independent pbr_shadow_psos tuples for
  actual selected descriptors, timing-opt-in only/whole-success publication;
- no shader/viewer-producer edit before actual mechanism admission.

Trace is preparing PRIVATE strict+optional cost fixture, coordinating API with
Shadow. Old unrun t54uaujb PBR fixture must NOT compare its historical two
libraries; both arms must use the SAME current normal39320181 library with
scene flag off/on. Cover rigid/skinned,1/2/8selectors, standard/custom/coverage,
main→probe→main, default/no-shadow guards, F/R/checkerboard/cascade/PCSS contrast,
water cutoff/mirror, rawRGBA16/depth/stencil and actual descriptor receipts.
Learned fixture contracts: hdr and normal_hdr are independent, and inactive hero
must be nullptr, not a supplied invalid count0 pack. No tolerance relaxation.

Root surgically integrates/reviews first. Ninja test outputs STILL contain the
REJECTED address variant: rebuild almetal_tests Release and its normal metallib
before reuse, package fresh test.app with only offline library. Run actualAPI+
GPU strict/preflight0 before separate scrubbed quiet fixed counterbalanced full
CPU/wholeCB GPU cost with SAME controls. No more capture before this code gate.
If admitted, complete optimized native suites, normal source-exact build/verify,
then controlled live comparison. Otherwise freeze/remove only that prototype.
Keep canonical68610 unchanged until admission.

## Retained app and measured improvement

Canonical build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app
is68610/source8eea2c6895c1c907ce513a5c83872a631874a3bc. Exe
 dc30d261ddbef780c6ffb459c2c87f9d08500785a062e1e834d5a4c49b0f2909;
library3932018186338255b0523028441e5376393a51c2b3ddbad2ad538f1c530376c2.
Preserved /private/tmp/alchemy-metal-68610-water-specialization-preserved.app.
Doctor/configure/build/verify/signature each0; independent identityfc35; both
optimized nativeAPI+GPU suites482529assertions/104cases each0.

Waterf549 retains exact shader source/math/mirror/depth/blend/tessellation and
specializes only shadowed above-water, actual pack bit4/count>1. Lazy copied
PSO descriptors MUST restore library=library.get(). Guarded normal producer is
on; bounded opt-in receipts prove encoded PSO selection. Isolated eligible GPU
savings15.42–17.17% in7/7blocks; no CPU gain claimed.

Three300frame live runs at matching camera/EEP/43controls/native/bias1/256ready:
68608before9.069479FPS;68610candidate9.785333;68608after9.257293. Candidate+5.704%
versusfresh. Main-visible probes98/95/100 and draws differ: no isolated code-only
percentage. All viewer exits/Goodbye/PIDabsence and own-plan restorations0.
Full details: archive/CURRENT-water-probe-specialization-2026-11-10.md.

## Closed address-space experiment and other evidence

Final packet /private/tmp/alchemy-metal-main-probe-device-valid-controls-20261110.jftrhxxe:
build/preflight/quiet each0.3319/1 actualAPI+GPU,5521/1 quiet,40strict+6large pairs,
168samples/R4. Dense modern/classic meanGPU+5.205/+5.876%, all7blocks slower;
no convincing CPU benefit.162out-of8–20ms rows retained. Independent audit8df2480a
and findingsd2807f7b agree REJECT. Two earlier fixture42failures are preserved.
All4files restored byte-exactly7772; no viewer trial/new viewer build.
57rawfiles preserved underignored artifacts/metal/private-main-probe-device-rejection-20261110/.
See archive/CURRENT-probe-address-space-rejection-2026-11-10.md.

Coupled baseline6b80 data finalized:2461 matchedCPU/GPU CBs, viewerActive87.60%
(not utilization), two~0.48s gaps coincide with profiler metadata workers;
outside those intervals viewer96.85%Active. No exact critical path/hardware ceiling.
Do not optimize profiler annotation pauses as ordinary frame stalls. Haze source
audit found no justified local copy/math deletion; timed encoder includes later
alpha and depth snapshot preserves real read/write dependencies.

Do not revive early-tests/materialABcache/two-childparallelshadow/SH/predecode/
register/scalar/mask/ambient/SSR/immutableUBO or whole-buffer device variants.
Retain64examined/32accepted, per-view packs, bindings/arenas, main occlusion,
aniso4, bounded lights, directR32, borrowed targets, image/global budgets/cubeSSAO.
Full parity/multi-scene/stability/strictnoGL/independentbuild-review remainOPEN.

## GitHub request

Owner requested downloadable existing-workflow build. Stable7772, includingf549,
was pushed successfully; unfinished experiments excluded. Metal harness run
42799555537 passed. CLI has no auth; browser signed out; connector cannot dispatch.
Owner chose to trigger from another device. Instructions already sent: Tag a Build,
branch codex/apple-silicon-metal, channelProject, projectMetal Alpha3, override
26.4.0.68611-alpha3 (confirmed tag absent at check). Existing action publishes
ARM64 tar.xz app/checksum and ZIP-wrapped Actions artifact. No app-packaging run
observed yet. This request authorizes that alpha packaging; broader release gates
remain open. Do not move/reuse an existing tag or upload private evidence.
