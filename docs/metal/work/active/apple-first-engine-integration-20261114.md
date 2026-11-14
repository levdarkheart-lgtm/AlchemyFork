# Whole-engine Apple-first architecture integration — 2026-11-14

Status: doing. Owner explicitly steers away from another shadow-only tuning
cycle: finish the remaining original architecture, then assess one integrated
viewer. Tracked goal is active. No viewer launch/login/new profiling or push.

Baseline clean branch codex/apple-first-architecture-g2-g5, HEAD627d1a698d;
last compiled/staged4918fd7/26.4.0.68999. Both prior apps and all profile evidence
are preserved. [Retest report](../../research/apple-first-retest-profile-2026-11-14.md)
is the current measurement; recovered average FPS does not meet the product
architecture/performance objective. PPD110–117 remain retained.

## Acceptance and scope

Implement the original [route](../../research/apple-first-implementation-route.md)
across the actual engine owners, not an unrelated renderer or more isolated
shadow gates. Owner updates scene, animation, materials and published resources;
immutable source/instance publications feed main/probe/shadow views. Bounded CPU
jobs prepare those inputs, followed by ordered native publication/encoding and
actual-completion retirement. Streaming/decode/upload and optional view work
consume the same frame plan. Preserve lighting/projector/query/alpha order,
geometry/material/pose invalidation, texture callbacks, UI, media/voice and all
other Alchemy behavior. M1Max native Metal ARM64/macOS only.

No per-subsystem owner test stop. Run meaningful contained correctness and
complete-path cost checks while integrating; reject measured harmful variants
without calling a smaller experiment the completed original architecture. Final
combined build/doctor/native/source/ABI/signature/staging gates and a separately
staged app precede owner whole-engine runtime evaluation. Live performance and
full product release gates remain unproved until actually exercised.

## Exclusive engineering ownership

| Owner | Paths and work |
| --- | --- |
| Root | pipeline.cpp/.h/native capture inlines, CMake, device/command lifetime, viewer display/main/probe/hero scheduling hooks, source working set, documentation/register/integration/build/test. |
| memory_policy_fix | ALNativePublication Signal/State; LLVB publication/direct-shared fresh buffer producer; LLDrawInfo/spatial/volume; material classes and actual viewer-object/editor/import/bake binding notifications. Root transferred llrender.cpp genBuffer ownership for fresh-buffer policy. |
| opaque_diagnostics | almetalworld.h/.cpp, new almetalworldprepare.h/.mm and world preparation tests; owner input adoption, pure range jobs and ordered prepared native consumer. |
| residency_diagnostics | ALNativeFrameBudget; decoder scheduler/worker; LLImageGL; fetched texture/create-list publication and callback path; image/upload credit tests. |

Existing alnative_generation_leases_metal_test.cpp is ROOT-OWNED/FROZEN after
both resource streams appended separate cases. Future vertex tests use new
alnative_vertex_publication_test.cpp; texture tests use a separate new
alnative_texture_publication_test.cpp. No concurrent uncoordinated edits or
native runs. Root serializes verification; agents do not build/launch/profile.

## Concrete source work underway

1. Producer invalidation: Signal emits will_write before lease/COW decisions,
   and a separate published revision only when the owner-visible tuple changes.
   State owns the sole cache publication; dirty notifications release publisher
   leases while queued/GPU readers retain theirs. LLVB/image CPU content stamps
   alone are insufficient because dirty writes precede successful publication.
2. Actual dirty owners: draw-map assembly/merged ranges, mesh updates, material
   setters/direct binding changes, image sync/immediate/render/scale/discard and
   avatar attachment poses. Animated/model/view-dependent state stays separate.
   System-body packed palettes are view-dependent and cannot be reused blindly.
3. Shared source residency: root's ALNativeSourceWorkingSet keeps State and weak
   resource allocation identities, not extra strong backing leases. Explicit
   per-publication serial guards late packet cleanup; count/unique bytes/age
   bounds and current-frame overflow protection remain. Application consumer
   integration is pending; helper existence is not the scene architecture.
4. Whole world preparation: adopt immutable owned batch/query/view inputs,
   copy raw pose values before detaching, prepare native constants in disjoint
   slots, publish one packed range on owner, encode original ordered records.
   Main/ordinary probe/hero/impostor use the same native renderer. Worker jobs
   never mutate arena caches, commands, LL objects or renderer pipeline maps.
5. Shared admission: fetched uploads currently bypass create-list budgeting.
   Queue before starting upload, preserve raw/ref/callback/commit-token ordering,
   retain credits to actual GPU completion, and feed downstream debt to existing
   decoder thermal/adaptive controls. Main/probe owners will use the same plan.
   No GPU-completion wait is introduced before publication on the same queue.
6. UMA producer writes: fresh immediate/UI LLVB buffers may use shared native
   storage as their initial CPU store where exact layout/readers permit it;
   general picking/clone/particle stores are not silently changed. Native COW
   reader safety and texture-index aliases must be tested.
7. Persistent scene membership/visibility and useful command preparation remain
   required after shared source publication; large-run shadow pilots do not
   complete them. Keep rejected ICB/Metal4/whole-invocation batching results and
   measure actual small mixed scene distributions with full mechanism overhead.

## Original behavior and constraints

Original1eb44bbd draw-map assembly at registerFace/rebuildGeom/rebuildMesh,
per-view cull/stateSort ordering, raw image callback ownership and attachment
palette once-per-frame behavior are mapped. Native immutable publications/jobs
are new mechanisms at those boundaries, not an OpenGL backend to retain.

Important source facts: LLImageGL worker touchContents may precede syncTexName;
LLVB content stamp advances before upload; publication needs separate epochs.
Existing retained cache leases can force orphan/COW unless invalidated before
writes. World captures currently borrow raw resource/pose pointers, so ownership
must precede detachment. Shared native frame arenas and command objects remain
single-owner mutable state. Most fetched uploads post directly to image worker,
whose16item batching has no shared frame/byte debt. Original callbacks are
publication-ordered at command commit, not GPU completion; preserve that.

## Verification in progress

Agent implementation sources are not proof of successful builds or performance.
Root registered new common/world files in CMake; no new integrated binary yet.
First pure common policy/ownership compile and tests are next, followed by
prepared-world native parity and complete-path checks and actual producer/view
consumer integration. New staging will use its own evidence directory/app;
68999 remains the exact profiled reference. Never launch automatically.


## Integration checkpoint — 2026-11-14, work continues

Source changes now connect the actual consumers:

- `LLDrawInfo` publication dependencies and invalidation from geometry, image,
  material and binding owners feed one `ALScenePublicationNative::Payload` with
  simple/legacy/PBR world variants and shadow variants. Main, ordinary/hero
  probes, impostor and shadow capture use that producer state. Per-view model,
  UV and current attachment pose remain separate. A bounded working set owns
  State and weak allocation identities, avoiding extra cache resource leases.
- `LLViewerJointMesh` snapshots world-space joint matrices and pivot binding
  revisions; pure composition packs the original45vec4 body palette for each
  view. Root joint mutation epochs and lifecycle changes invalidate same-frame
  poses. Attachment validity stamps publish after successful packing.
- Main/probe/impostor G-buffer inputs move into owned view packets. The CPU
  kernel builds disjoint constant ranges; the owner runs one range and at most
  one credited Dispatch range, joins, then publishes one native constant range
  and encodes the original draw/query order. Shared arenas/caches remain owner
  controlled. Failure/cancellation does not publish a partial packet.
- `LLAppViewer::doFrame` starts one serial admission plan before idle/streaming/
  render work; actual thread CPU excludes admitted optional probe work. Both
  probe managers retain a credit across their existing coherent transactions,
  preserving maintenance and configured face counts. Bootstrap stays required.
  Texture create FIFO, worker upload bytes/counts, publication debt and decoder
  starts use that same plan/age domain. GPU owners retire credits on actual
  completion; teardown drains without another display tick.
- Fresh immediate/UI LLVB writers use shared native storage where supported.
  Failed writes, old readers, empty maps and CPU fallback retain explicit
  publication semantics. Terrain/media/fetched fallbacks now acquire the actual
  native generations; detached packets do not depend on mutable image facades.
- Persistent indexed rigid bounds are derived from actual published indices.
  Ordered native visibility members can generate same-command indirect
  arguments without reordering queries or alpha. `RenderMetalNativeVisibility`
  is nonpersistent and OFF until complete-path cost gates justify enabling it.
  Unknown/rigged/body/cloth/terrain bounds stay visible. This is not a claim that
  the original G4 runtime/performance gate has passed.

Actual checks so far (private evidence directory
`artifacts/metal/apple-first-engine-integration-20261114`, canonical workspace):
common-policy-2 build/test exit0,180assertions/19cases. Initial native build
exit0,77.410s. Native prepared world585/4, conservative visibility398/4,
submission ownership21/2, joint epochs19/3 and mesh epoch5/1 pass with Metal API
and shader validation. UV comparison exit42 is retained: reverse-Z fixture
placed geometry at its clear depth. Direct/prepared pixels agreed; corrected
fixture changes matching reference/compact projection depth and awaits retest.
Initial syntax checks pass main and both probe owners; pipeline had two unused
lambda captures, corrected by actual special-shadow resource retention. New
source/producer/consumer fixture, direct shared-write and typed image tests are
not yet executed. No new viewer launch, profile, FPS claim or push.

PPD118–121/128–141 track integration/fairness/reentrancy/lease/UV/coverage/bounds
and credit edges; records are retained, not closed by these checks. PPD122–127
preserve the independent whole-session warning audit aliases. Warning source
fixes through9deff6 were integrated as source hunks, preserving root CMake and
producer hooks; their isolated evidence does not substitute for our final build.
PPD126 remaining-volume attribution remains unknown; quiescent hero CPU scene
owner retirement is implemented without a new per-frame/GPU wait.

Next: finish forward/impostor completion-owner PPD141, rebuild and run actual
producer/packet tests, run complete-path cost fixtures with no competing build,
then freeze/commit source, required doctor/build/tests/static/source/ABI/signature
and separately stage one exact combined candidate. Preserve68999 and all prior
apps/profiles. Original whole-engine performance/release acceptance remains
partial until owner runtime evaluation; no per-shadow stopping point.


Source checkpoint before combined checks: PPD141 owner fields/native consumers,
143 no-throw borrowed-transaction failure guard,142 finite probe fairness helper
and actual budget tests,144 pose header prerequisite are now implemented. The
initial diagnostic build failed only the new pose test include prerequisite
(exit1/649.919s); full production source changed during that diagnostic build,
so no resulting app is accepted as a candidate. All diagnostic evidence remains.
Production/header/test assignments are frozen. Next command rebuilds all changed
native/common/producer/pose/frame/fairness test targets and viewer, followed by
focused tests, complete-path costs and final required build/static/staging.
No source change is yet a measured FPS gain or owner-verified defect closure.


## Measured policy selection after a69d6df872

Frozen required doctor/configure/build pass0 (build284.389s), exact69002 static
identity/346ARM64/import/signature/features/decoder/exception checks pass0.
85selected CTest groups plus10specialization and3archive cases pass (functional
summaries640025assertions/571cases); direct offline HTTP completion140/1 also
passes. Strict native source audit3565files/0blockers passes. Corrected UV,
source-owner/transaction, pose, actual producer and frame/fairness tests are in
this passing run. No viewer launch or live FPS claim.

Complete-path costs include equal per-frame batch construction, owned adoption,
job admission/Dispatch/join, constant publication/encode/commit/GPU completion.
At16/128draws, two-job overhead loses; serial128 ticket median0.563750ms vs
native direct0.541479ms. At1024draws two-job2.03013ms vs direct2.07631ms and
max2.36046ms vs2.53175ms. Small views therefore retain the SAME immutable input
packet but encode scalar native commands, pinning that packet on completion.
Large views use prepared jobs at1024+, and denied CPU credits execute required
jobs locally. This is execution policy inside the architecture, not an alternate
renderer/backend or reverting to mutable scene discovery.

GPU conservative visibility loses all six initial contained rows and remains
OFF; the first periodic-mask percentage labels are qualified by PPD147 and the
corrected exact-half fixture will be rerun. Native source bounds qualification
is now requested only by the opt-in visibility flavor (PPD146), so disabled work
does not consume default dirty-source CPU time. Source reuse/build/fail events,
encoded scalar/prepared/credited views, prepared bytes and both probe managers'
admitted/deferred/required group counts are logged for the next whole-engine
run. Added512/4096prep cost points resolve the unmeasured crossover/size range;
new results are not yet claimed.

PPD145–147 preserve the actual overhead/inactive work/fixture reporting findings.
Original G4 runtime/performance acceptance remains conditional; no claim that
GPU visibility, ICBs or Metal4 objects inherently make this workload faster.


PPD148 correction before final acceptance: second expanded cost pass at69003
still regressed4096draws (two-job6.91150ms vs direct6.59273ms). Actual68999
internal captured census span10:19:28–10:24:22 has6,128,700main constant binds
with3,982,822reuses (64.9864%), and1,287,434probe binds with494,234reuses
(38.3891%). The new prepared branch counted equal400byte common constants but
always bound a new slab offset; it now actually reuses the previous range,
including restoration after terrain overwrites slot4. This preserves the real
Alchemy region/agent-space draw pattern rather than assuming unique generic
object transforms. A new native fixture compares distinct region-space triangles,
mixed families, changed constants, terrain, all MRT/depth/stencil pixels and
query samples in both Z conventions. Source/tests frozen, retest pending. The
corrected exact-half visibility matrix still does not establish a reliable
median-and-tail benefit; its single favorable median is not a gate pass.


PPD149/150 source repairs before final acceptance: prepared slabs now compact
exact repeated400byte common values in place after joined jobs, remapping all
common/material/PBR spans in original order with dense16byte structure alignment
(slab-base alignment is separate). No compaction allocation; original bytes and
shared records are counted. Native resource leases no longer unnecessarily hold
CPU batch/query/constants storage through GPU completion. Owner-only retirement
after encoding can reclaim empty capture vector capacities, guarded against any
remaining strong CPU reader and requiring quiescence. Commands still retain
exact native generations/destinations and arena pages; null resource leases are
legal for empty prepared clears. Native tests poison/reuse retired CPU vectors
while GPU is held pending and require exact MRT/depth/stencil/query results.
No new test results claimed until actual final build/run. Source counter logs
include compact/original bytes and CPU packet reclamation/refusal.


PPD151/152 integration checkpoint: actual69005 native compaction/retirement run
passes11/12cases,1473/1475assertions; remaining byte assertions omitted448bytes
of query uniforms. Source test now measures independent query-only publication,
retains exact geometry/slab saving and pixel/query/lifetime assertions.

The remaining stable material publication boundary is now implemented in source:
legacy/PBR G-buffer constant tables publish once at source revision into native
shared buffers, with sealed COW input identity and exact bounded weak content
reuse. PBR publishes both normal-target formats required by its actual ABI.
Dynamic camera/UV/poses remain per-view. Direct/prepared G-buffer consumers bind
certified native tables, while manual/mutated data keeps original validation and
native packing. Terrain/forward lighting use different/view-dependent ABIs and
retain those exact paths; factory avoids publishing unused forward-only tables.
The source working set accounts all native table buffers; cache metadata owns no
extra resource lease and is reset at device/scene cleanup. Device identity check
compares captured wrapper+native handles without dereferencing an old wrapper.
Runtime logs distinguish native table draws from dynamic table draws as well as
publications/content hits/failures. No source-only success or FPS claim.

New native material-source tests cover packed single/indexed legacy/PBR, both
normal formats, direct/prepared output, actual publication-byte savings, mutable
alias sealing, COW edits, pending/abandoned GPU work, stale/manual fallback and
malformed data. Root owns serialized build/run; all source/test assignments are
frozen. GPU visibility remains OFF; CPU sharing/native resource lifetime remain
required regardless of its optional gate.
