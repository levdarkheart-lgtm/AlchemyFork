# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `1407fca9cf9a5daf4413f760155b5cc03a4a073d`.
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[campaign](ultra-20fps.md), [archive index](../../state/archive/INDEX.md).

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 reference/build | doing | Source-exact originals/native normal builds verified; independent fresh-checkout release remains. |
| M-002 | GL inventory and release audit | doing | Owned native rendering; strict source/runtime zero gate remains. |
| M-003 | Native presentation/lifecycle | doing | Native world/UI and attended lifecycle; prolonged stability remains. |
| M-004 | Resource/transfer/sync ownership | doing | Arenas/retirement/direct slices/borrowed targets/material ownership validated; net performance open. |
| M-005 | MSL/binding behavior | doing | Original math preserved; material resource ABI validated; full parity open. |
| M-006 | UI/fonts/HUD/media | doing | Native implementation retained; full feature/stability proof remains. |
| M-007 | World/material/shadow correctness | doing | Raw depth/PCSS/casters/reverseZ retained; broad original parity remains. |
| M-008 | Advanced views/effects | doing | Preserve six Ultra hero faces; reflected-content parity/performance open. |
| M-009 | Feature/stability/native-only release | doing | Feature matrix, prolonged stability and independent release review remain. |
| M-010 | M1 Max measured optimization | doing | Controlled baselines/captures collected; multi-scene contract unmet. |
| M-011 | Remaining owned GL authority | doing | Native resources/window/rendering; strict remaining-tail audit open. |
| M-012 | Native UI automation | complete | Exact fullscreen/login/Preset123 flow; unsupported system-helper targeting avoided. |
| M-013 | RLVa parity | deferred | Owner essential-first ordering; retained functionality still required. |
| M-014 | Ultra20FPS campaign | doing | Cache trial found noFPSwin; directdefault restored;20FPS unmet. |

States describe evidence; a bounded completed item never implies product completion.

## Current implementation boundary

Native Metal owns world, materials, avatars, terrain, shadows, sky/water,
atmosphere/effects, UI/font/HUD, media textures and presentation. Existing
network/inventory/simulation/editing/voice features are retained. No fallback
renderer or general cross-platform framework is authorized.

Retain existing repairs: rawR32 direct shadow slices, bounded local-light
coverage, resource borrowing, image/global memory accounting, completion-safe
arenas, per-view probes/cubeSSAO, main visibility queries, Ultra4xanisotropy,
binding caches, static adjacent covered draw coalescing and64/32 probe traversal.
NativeD32 changes original raw-depth semantics; no migration/epsilon waiver.

## Current source ownership and latest result

Root exclusively owns shared source/docs/runtime/GPU. Agents prepare private
source or read-only analysis; no concurrent uncoordinated shared edits.
Translator FULLY STOPPED/NEVERrestart; Xcode replay STOPPED; same location only.

1407 caches immutable8/32 covered-shadow material bindings across views through
FrameData. Actual native IDs/strong leases prevent ABA/inflight writes; TTL and
1024slot/4MiB bounds include retired slots. Count1/capacity/allocation keep direct
binding. Shader coverage/depth/constant math and draw order are unchanged.
Flat pass-local registry preserves complete key equality, validation and order.
Both optimized API+GPU suites435459assertions/102cases PASS; actual collision
fixture0; actual renderer10strictR32pairs+196quiet timing samples0. Warm8/32
precommit−40.21%/−69.60%,512cold−31.10%;4096once-use pressure+22.68%/+1.455ms.
Separate retirement/diagnostic cleanup and probable modest32-slot GPU cost remain.
This admits a controlled trial, not universal non-regression or release promotion.

Normal68605/source1407 doctor/configure/build/verify/signature each0,349ARM64;
independent exact source/exe/library/compiler flags review passed. Source recording
OFF/C++23/O3/NDEBUG/arm64/min26.0. New executablef3aa9190/library39320181.
Candidate68605 completed300readyframes:9.066675FPS; propercontrol68601:
9.410237FPS, bothstart/end43gates0/camera+EEPexact/256ready120/bias1/native1.
Candidate mean draws+2.53%; no isolated regression or FPS gain claim. Allviewers
CLOSED/exactexits+allplansrestored. Both post-sampleCPU10s traces0; hero~50%main.
Restored material_buffers=false and ordinarydiffusesamplerABfalse in source;
optional material sampler is lazy, quality-matched and safely reset onanisotropy.
BOTHoptimizedAPI+GPU435462/102PASS; sourceapproved, normal rebuild pending.

Earlier admitted68601/source77977290 ready300:9.833798FPS/mean101.690106ms.
Preserved/private/tmp/alchemy-metal-68601-normal-preserved.app, exef277/lib61c5.
Fresh68601 trial9.408707FPS/mean106.284533/p50106.109021/p95116.8194 had missing
start gate and unmeasured CUA stall; exploratory only. End43gates0/exact98164
SIGTERM/Goodbye/absence/planrestore0. Camera/EEP equal new68605 at frame5046.
Owner reports fullscreen GameMode notification; leave mode unchanged/drop as
primary lead. Unsupported ControlCenter CUA inspection stalled; do not retry it.
[Detailed current evidence](../../state/archive/CURRENT-material-argument-buffers-2026-11-10.md).

## Complete-product checkpoint gate

1. **Advancement:** source-exact material cache/ownership integrated, real GPU
   correctness and bounded complete CPU-cost evidence; normal viewer trial completed with no FPS gain.
2. **Preserved contract:** original rendering/features/all six hero faces, no
   quality reduction, no translation/fallback backend; exact original remains1eb.
3. **Open gates:**20FPS and multi-scene performance, full pixel/feature parity,
   strict source/runtime GL-zero, prolonged stability and independent release review.

## Immediate executable action

Commit tested direct-default/sampler isolation, normalbuild/verify. Rebase private
whole-shadow2ordered-child implementation+mixedfixture onto newcheckpoint;
source-reviewed ONLY, no compiler/GPU/performance proof. Rootpreparation/pinning,
allcasterfamilies/order/cancel/oneparentCB/completecost gates remain mandatory.
Private immutable regularprobeUBO1 test nc81mpp0 passed200strict/168timings0 but
allpairedrangescrosszero: reject publicflag, no repeat/register/FPS claim.
Latestaudits ro6wnbu4/znv_94xq/v8ggv6hj and profiles rf4lnbpm/84up2rjj.

Prior continuous CPU evidence (~77ms sampled/frame) predates1407 and is not a new
CPU floor. Completed-command GPU sums overlap; no causal shader-ms inference.
Prior hero specialization reduced registers but viewer FPS was flat and spill
remained. SH/predecode/namedaccumulator/scalarcursor/simplemask/ambient variants
remain rejected; mainUltraSSR0. Whole-shadow parallel/ordinary-face batching
proposals remain private source-only, not implemented performance features.
Full historical tests/captures/failures and exact commands live in indexed archives.

## Non-negotiable open gates

No retained OpenGL/Vulkan/MoltenVK/Rosetta/translation fallback. Preserve original
quality and non-rendering functionality. Do not infer parity/FPS from test counts.
PRODUCT/VALIDATION/PERFORMANCE remain authoritative; no release/public capture
upload authorized. No travel under this goal. Retain raw private evidence and
record real commands/exits/source identity at handoff. Goal ACTIVE/unmet.
Do not mark feature parity from build/test success alone.
