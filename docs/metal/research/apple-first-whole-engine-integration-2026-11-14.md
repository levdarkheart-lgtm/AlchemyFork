# Whole-engine Apple-first ownership integration — 2026-11-14

The combined candidate extends beyond shadows into the actual main/probe scene,
material, pose, streaming and submission boundaries. It is ready for owner
whole-viewer evaluation. **The original complete architecture and performance
acceptance remain partial.** Default GPU work generation before CPU discovery,
broader detached culling/encoding and a complete pass/memory graph are still
missing. A successful candidate delivery does not close those original goals.

## Exact candidate and evidence

- Branch: `codex/apple-first-architecture-g2-g5`, isolated worktree
  `/private/tmp/alchemy-apple-first-architecture-g2-g5`.
- Compiled code/test SHA: `f3a7ac5f58ee1acddabc8443a9a63022f13dc24f`.
- App: `Alchemy Metal Alpha G2G5`, version `26.4.0.69008`.
- Identifier: `org.alchemyviewer.viewer.metal-alpha-g2g5`; existing development
  preferences/cache identity retained, release URL handlers remain isolated.
- Main executable SHA256:
  `9fccffeef72549cf9ed17bd2f8253b02ceceaee8760a287db61f9bf650fa5dea`.
- Separately staged app, relative to the canonical workspace:
  `artifacts/metal/apple-first-engine-integration-20261114/staged/69008/Alchemy Metal Alpha G2G5.app`.
- Private evidence directory:
  `artifacts/metal/apple-first-engine-integration-20261114/`.
- Profiled68999 baseline remains in `artifacts/metal/apple-first-retest-20261114/staged/`;
  executable SHA256 `13e44a15bfbe746072b5c5642f123310a8365d01947e621a25775637e19d351f`
  was rechecked unchanged before staging. No old app or capture was overwritten.

Both reference68999 and this candidate already render natively with Metal.
These changes address ownership and scheduling inside the native viewer;
previous FPS results cannot be assigned to this new source. No viewer launch,
login, live profile, owner acceptance or push occurred during this integration.

## Actual Second Life boundaries changed

| Owner/consumer | Implemented behavior | Remaining owner work |
|---|---|---|
| `LLDrawInfo`, group draw maps, geometry and material/image publishers | Dirty notifications invalidate a shared `ALNativePublication::State`; a revision-qualified `ALScenePublicationNative::Payload` holds world/shadow variants and exact native resource generations. Main/probe/shadow consumers share it. | First-use sealing remains lazy. Per-view scene/cull discovery and ordered reference gathering remain on the scene owner. |
| `LLVertexBuffer` / `LLImageGL` | Prewrite invalidation occurs before COW lease decisions; successful visible publication has its own revision. Older CPU/uncommitted/GPU readers retain their generations. | Failed writes/publications retain explicit failure/retry semantics; dirty CPU stamps alone do not certify new native data. |
| Legacy/PBR G-buffer materials | Stable uniform tables publish once into immutable shared Metal buffers. A bounded weak cache checks hash, length and exact live bytes. Both PBR normal-target ABIs are published. Sealed per-view values share their snapshot without copying inactive authored arrays. | Dynamic camera, UV and pose stay per-view. Manual/mutated inputs keep ordinary native validation/packing; terrain and forward lighting retain their distinct ABIs. |
| Main/probe/impostor capture | Batches, ordered queries and current raw poses enter `ALMetalWorldViewInputs`; native resource ownership is separate from reusable CPU arrays. | The packet still contains per-view batch values; it is not yet a fully compact source-ID scene database. |
| Body/attachment animation | Joint/mesh revisions qualify immutable body snapshots; pure per-view composition preserves the original45vec4 body palette. Attachment validity publishes after successful packing. | Simulation, lazy joint resolution and mutable avatar traversal remain on their established owner. Body/cloth are not assumed cross-view invariant. |
| World preparation/submission | One owner range plus at most one credited Dispatch range can prepare disjoint constants, join, compact exact common values and encode original draw/query order. Scalar consumption uses the SAME owned packet. | Extra G-buffer jobs are diagnostic OFF after complete-path costs failed to establish a reliable crossover. Native command encoding remains owner-controlled. |
| `LLAppViewer::doFrame`, both probe managers | One outer-frame admission epoch covers main CPU work, preparation, probe groups and streaming. Required CPU accounting excludes admitted optional work. Coherent probe transactions retain credits; bootstrap/maintenance remain required, refreshes have finite progress. | Deferring optional refresh can change freshness under load; whole-viewer tests must assess reflections together with input/frame pacing. This is not a complete global GPU pass DAG. |
| Texture FIFO, uploads, decoders | Queue age, downstream pending publication and upload count/bytes feed the same frame plan. Credits survive actual GPU completion; create/downscale service cannot indefinitely starve one another. | Network/asset arrival remains asynchronous. Actual texture response under load requires runtime evaluation. Commit-ordered callbacks and shared native queue dependencies are preserved. |
| Fresh immediate/UI vertices | Supported fresh producers write directly into shared native storage, with reader-safe generations and explicit failed-write/CPU fallback handling. | This does not convert every CPU/picking/particle store or make private GPU textures universally shared. |

Source map: `pipeline_scene_publication_native.inl`,
`pipeline_buffer_publication_native.inl`, `pipeline.cpp`,
`alnativepublication.h`, `alnativesourceworkingset.h`,
`alnativeframebudget.h/.cpp`, `alnativeframeplan.cpp`,
`almetalworldmaterialdata.h/.mm`, `almetalworldprepare.h/.mm`,
`almetalworldvisibility.h/.mm`, `lljoint.cpp`, `llviewerjointmesh.cpp`,
`llimagegl.cpp`, `llvertexbuffer.cpp`, and both reflection-map managers.

## Enabled policy and measured rejections

Shared producer publications, native material tables, owned inputs/poses,
CPU-storage reclamation, frame-wide admission and scoped direct shared writes
are active. CPU arrays retire after joined preparation/encoding; commands retain
native generations, destinations and arena pages to actual completion. Delayed
GPU tests poison/reuse retired CPU storage and require unchanged native results.

Prepared constants preserve Alchemy's real region/agent-space reuse. PPD148
restores binding the previous common offset; PPD149 compacts repeated400byte
common records in place after jobs join. Material/PBR offsets are remapped without
reordering draws or queries. PPD150 separates CPU capacity from GPU lifetime;
PPD151 independently accounts448bytes of query constants in the byte fixture.
PPD152/156 extend stable material ownership through native tables and per-view
copy/move semantics, preserving old mutable aliases on original producers.

`RenderMetalWorldPreparation` and `RenderMetalNativeVisibility` are both
nonpersistent, default OFF in the staged app. Preparation's opt-in branch starts
at1024batches and uses the common budget; denied credit executes required ranges
locally. GPU visibility has persistent rigid bounds and same-command native
indirect arguments, but uncertain/rigged/body/cloth/terrain bounds remain visible.
Its measured overhead did not earn default activation. Existing ICB/Metal4 pilots
remain conditional; neither Metal4 availability nor worker occupancy proves gain.

Corrected costs advance FrameData once per outer frame. Older costs1–3 never
advanced beyond their initial protected frame and are retained as provisional
same-generation observations (PPD154). Final costs5 use2warmups+12samples per
row, actual main/probe renderer owners sharing FrameData, main plus two128draw
probes, eight stable material variants, matched region-space cameras, capture
construction/adoption/reclamation, joined work, native encode/commit and one
GPU completion wait. Second-warmup MRT/depth/stencil parity passes for all views.
They are mechanism diagnostics, not300frame product gates, p95 or viewer FPS.

| Family / main draws | Raw encode median ms | Stable scalar ms | Stable adaptive ms | Raw / scalar completion median ms |
|---|---:|---:|---:|---:|
| Legacy128 |0.188084|0.177000|0.176729|0.554542 /0.542687|
| Legacy1024 |0.520167|0.526875|0.503292|1.378130 /1.238600|
| Legacy4096 |2.107420|1.920040|1.832250|3.529120 /3.053080|
| PBR128 |0.278479|0.209417|0.206229|0.571626 /0.529458|
| PBR1024 |0.770083|0.564083|0.600041|1.181100 /1.009210|
| PBR4096 |2.992120|2.228770|2.250310|4.367210 /3.597270|

Do not hide the legacy1024 scalar encode regression (~1.3%) or sample maxima:
its scalar maximum0.739875ms exceeds raw0.586875ms. These short ordered runs
cannot decide live tails. PBR scalar encode improves all three sizes; legacy
improves two of three. At4096 PBR aggregate owner+worker CPU is2.374690ms
adaptive versus2.221900ms scalar despite similar completion wall time. Extra jobs
can give back reuse savings (PPD155). Simple preparation costs also change sign
between runs/sizes; draw count alone does not establish a reliable default.

Actual per-frame constant publication falls3376->1200bytes legacy and
11568->1200bytes PBR after one-time eightvariant publications2176/20736bytes.
Source setup CPU in this run is0.036125/0.135542ms. These fixtures do not include
all live SL invalidation, cull, lighting or post work. PPD156 removes known copy
work; differences between short runs are not attributed solely to that repair.

## Executed checks and retained failures

M1 Max/64GiB, macOS26.5.2, Xcode26.6. Required `tools/metal/doctor.sh`, exact
configure and `tools/metal/build.sh build` all exit0 at the compiled SHA
(`frozen-source-build-7.json`, build157.170s). Release ARM64 uses effective-O3,
LTOOFF and8jobs, accepted OpenHT0.27.0 archive, CEF/VLC/media/voice retained.

- Focused final native material/preparation/CPU-GPU ownership:3621assertions,
 17cases, exit0 with Metal API and shader validation.
- Broad selected CTest:86/86groups pass, exit0/124.778s. Ten additional exact
  native rendering cases and three non-skipped archive policy processes pass.
  Passing functional summaries total642599assertions/579cases; focused/cost
  reruns are excluded from that total.
- Costs5: preparation1083/1, visibility1042/1 and three-view material6598/1 pass;
  timing selections remain subject to the limits above.
- Strict production source audit:3567files,0blocking,903informational,0scanerrors,
  exit0. Native compatibility vocabulary is not a runtime GL-context test.
- `tools/metal/verify_app.sh`:346Mach-O slices,0non-arm64; deep strict signatures,
  exact embedded SHA/version, owned native imports, media/voice/codec/provenance,
  system exception RTTI and diagnostic defaults all pass.
- Staging source-before/source-after/destination inventories match exactly:
  7839files,432directories,2symlinks, identical bytes/modes/link targets/hashes.
  Staged static verification also passes. No app execution is inferred.

Receipts: `combined-correctness-2.json`, `final-functional-summary.json`,
`native-final-boundary-correctness-2.json`, `complete-path-costs-5.json`,
`strict-source-audit-2.json`, `built-app-static-2.json`, `stage-candidate-1.json`.
Exact argv, source pins, environment, exits and raw logs stay local in evidence.
Earlier dirty build failure (PPD144), UV clear-depth fixture failure (PPD134),
query-byte assertion failure (PPD151), namespace build failure (PPD153) and adverse
cost rows remain preserved. No successful check closes owner-pending defects.

## Remaining original route and whole-engine assessment

The [original route](apple-first-implementation-route.md) remains authoritative.
This candidate completes the current integrated delivery, not every architecture
or product goal. Remaining engineering includes persistent scene membership and
GPU work generation before repeated CPU discovery, useful animated conservative
bounds/small mixed command runs, broader immutable cull/pose/encoding work and
explicit whole-pass/unique-memory dependencies. Tile-local main/light/post,
vertex amplification, skin-once and sparse asset ideas remain conditional work;
they must preserve actual SL semantics and earn complete-frame benefit.

The next live assessment uses one combined candidate across busy/static/loading,
camera motion and teleport transitions. Online content will vary: retain workload,
quality, avatar, draw-family, decode/upload, probe and memory operands alongside
frame p50/p95, input and texture response. Explain interactions among shadows,
lighting/projectors, probes, alpha, animation, streaming and post; a shadow win,
helper win or higher GPU activity alone does not pass the engine objective.

PPD125 teleport rejection and PPD126 remaining-volume attribution remain open.
The active [integration ledger](../work/active/apple-first-engine-integration-20261114.md)
and [defect register](../POST_PORT_DEFECTS.md) retain all histories and owner gates.
