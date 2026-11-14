# Persistent native scene membership and small mixed commands

Status: doing; continuation of the original active Apple-first architecture goal.
Previous goal turn: **progress**. Exact69008/f3a7ac5f58 was built/tested/staged;
its [results](../../research/apple-first-whole-engine-integration-2026-11-14.md)
and all earlier artifacts remain unchanged. This turn continues remaining source
boundaries; it is not another viewer launch/profile or shadow-only stopping point.

Worktree `/private/tmp/alchemy-apple-first-architecture-g2-g5`, branch
`codex/apple-first-architecture-g2-g5`, starting clean267f89749b (documentation
handoff; compiled candidatef3a7ac5f58). No other worktree is being edited.

## Source/original invariant

Original1eb44bbd and current viewer both require the CPU cull/stateSort work for
LOD, occlusion, texture/cache priority, visibility policy and alpha groups.
Main display updateCull1972/stateSort2069 and probe display_cube_face2402/2427
lead to postSort, which walks each accepted group's mDrawMap and pushes each
LLDrawInfo into per-view render maps. Main deferred pools walk those maps;
native probes' captureMetalReflectionProbeScene walks them too. Both then call
captureMetalPublishedWorldBatch. Do not delete these CPU policy side effects.

A group build is incomplete during registerFace: subsequent faces can extend its
last range/material list. Deferred mesh work completes after postSort's map walk,
then LLVertexBuffer::flushBuffers publishes the native stores. Therefore seal a
complete native group packet after that flush, use it in a later view, and keep
current raw native enumeration when dirty/ineligible/unpublished. Region/model
transform is copied and separately checked; shift alone need not dirty geometry.

G-buffer setters already cache scalar PSOs/buffer ranges/textures/samplers. The
remaining legacy/PBR path still visits up to32 individual texture bindings per
draw. Stable uniform publication does not solve that resource/command boundary.
Native resource tables are a prerequisite for useful ordered mixed ICB work;
changing the old2048shadow threshold does not provide that architecture.

## Implementation in progress

- Root: completed group/pass payload at spatial-group producer state, native
  range admission in LLCullResult and merged main/probe simple capture. Preserve
  raw/group/raw ordering with raw offsets; account native counts, original index
  totals and query positions. First slice: non-rigged region-space PASS_SIMPLE,
  no animated UV/material/avatar/bridge payload. Other families stay on their
  existing native path. No full-batch temporary reconstruction by workers.
  Per-view capture still copies ordinary batches; compact source/instance layout
  remains further work, not falsely described as eliminated.
- Root: clearDrawMap/registerFace/shift withdraw native group publication;
  cached matrix comparison detects independent region rebases. Dirty mesh/group
  stays raw; postSort seals after deferred writes/flush. Extra source->group
  publisher leases must disappear before native COW decisions.
- Architecture agent: lazy weak State::observe dependency signal, mutation and
  eviction notify derived caches before releasing publisher leases, without
  changing child revision on eviction or breaking PPD120 reentrancy. Six new
  native publication cases authored/frozen; not yet executed. Now owns actual
  group publication/capture/range fixture in alpipeline_buffer_publication test.
- Opaque agent: native legacy/PBR resource-source snapshots, exact C++ native
  texture generation leases (native object retains alone do not protect COW),
  bounded resource argument tables, matching shader entrypoints and renderer
  integration. Preserve arithmetic, exact linear/sRGB sampling and anisotropy.
  Tables own resources, never their Source/context; shader/sampler generations
  and pending GPU leases remain explicit. Scalar native fallback remains intact.
- Residency agent: NEW native world-resource correctness/cost fixture; actual
  direct/prepared outputs,8visible indexed slots, formats/masks/reverseZ,
  equal-depth/query order, pending/abandoned GPU ownership, sampler/shader changes,
  allocation/capacity fallback. Cold/warm/churn16/512 and main+two-probe full paths.
- Root: source factory seals resource generation tuples once per dirty flavor,
  using pointer-matched aliasing leases from already-accounted Payload.resources.
  New nonpersistent controls RenderMetalWorldResourceTables and
  RenderMetalGroupSources are OFF until complete-path gates. No default gain
  or useful GPU activation is yet claimed. Logging distinguishes request/bind/
  fallback and actual group admission/capture; typed failure reasons are pending.

Exclusive edits: opaque owns almetalworld.h/.cpp, almetalworldresources.h/.mm,
almetalpipeline.h/.mm and builtin shader. Residency owns new resource test.
Architecture owns existing alpipeline buffer publication fixture; its common
publication files are frozen. Root owns pipeline, group/spatial/volume owners,
settings, CMake and all canonical documentation. Root alone executes builds/tests.

## Compatibility and verification

Independent read-only audit: directional and spot shadows each call sortShadow
with sShadowRender=true before cache/admission/capture, and do not consume main
converted ranges. Impostor/preview own results and are excluded. Picking walks
spatial partitions; highlights use faces; debug is excluded. hasRenderBatches
now includes native counts. RenderDepthPrePass is a remaining raw map consumer;
new group conversion excludes that control (PPD158). Existing generic native
world prepass appears inert; no new visible depth regression is attributed.

PPD157 records the unexecuted test-author self-review finding: a closed synthetic
GPU gate followed by shader reload would wait on itself; shader reload checks
were moved after gate completion. Sampler/cache changes remain tested pending.
No production deadlock or actual hung run is claimed. Next defect159.

Private evidence: canonical artifacts/metal/apple-first-persistent-native-scene-20261114.
Next: finish/freeze sources, run common derived-publication cases, actual-owner
native group/resource tests, quiet complete-path cold/warm/churn cost matrix,
then required clean-source doctor/configure/build/tests/source/static/staging.
Retain failed attempts and reject net regressions. No viewer launch/login/profile
or push. Original GPU-before-discovery/full cull/encoding/pass/memory architecture
and whole-viewer acceptance remain partial and active.


### First executed prerequisite check

Narrow dirty-source diagnostic: cmake --build build-Darwin-ninja-os-arm64-g2-g5
--config Release --target llcommon_tests -j8 exits0/5.729s. Exact common binary
[native-publication] --reporter compact exits0/0.011s,131assertions/11cases.
Source hashes/argv/logs in derived-publication-1.json at the new evidence directory.
This tests actual sixnew dependent-publisher/reentrancy cases and retained source
publication cases; it does not validate the unfinished renderer integration or
a new viewer candidate. Common source assignments are frozen.
