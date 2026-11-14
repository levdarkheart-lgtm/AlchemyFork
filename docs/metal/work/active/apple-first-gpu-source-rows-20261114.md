# Native GPU source rows and whole-engine continuation

Status: doing. Original Apple-first whole-engine architecture goal remains
ACTIVE and partial. No viewer launch, login, new live profile or push is authorized
in this implementation phase. Local source/build/native test evidence is distinct
from owner visual and whole-viewer performance acceptance.

The previous clean checkpoint is 14d57c02120cd306364ec539987aafb3abc8e6a0,
Alchemy Metal Alpha G2G5 26.4.0.69018. Required doctor/configure/build8 exit0;
build27.040s. Static8 passes346ARM64 Mach-O, identity/source provenance, strict
signatures, imports, media/voice and five diagnostic controls defaultOFF.
Private evidence: artifacts/metal/apple-first-persistent-native-scene-20261114/.
Build8 exact renderer test hashes match build7; native/broad correctness from7
is reused for unchanged binaries, not represented as new execution. Compact
capture actual512-source costs still regress6.6–9.4%CPU; PPD170 remains open.
No current source gain is inferred from the earlier68999 busy-club profile.

## Current implementation boundary

New isolated worktree /private/tmp/alchemy-apple-first-gpu-source-rows, branch
codex/apple-first-gpu-source-rows, starts at14d57c. Root owns world.h/.cpp,
pipeline.cpp/shared capture calls, CMake, docs, integration and all execution.
Architecture owns region model publication and source-row capture helper/actual
owner fixtures. Opaque owns shader/base-instance encoding and new native draw/
renderer parity fixtures. Residency owns source pages/resource bridge and page
fixtures. No agent builds/tests or edits another owner's shared files.

- Completed LLDrawInfo source identity/flavor/revision selects a persistent GPU
  row. Packed position.w continues to select the original material slot.
- Exact region-matrix pointer ownership is qualified on the scene owner. Copied
  region identity/revision and already-computed MVP/modelview/normal176-byte rows
  preserve the current CPU matrix arithmetic; retained inputs contain no LL or
  authoritative matrix pointers. Region model epochs are separate from geometry.
- Source rows encode vertex streams, immutable material uniforms and native
  resource tables. A per-view source-to-region map keeps row identity independent
  of main/probe region order. Shared view globals contain mirror/depth policy.
- Bounded pages use reflected argument-buffer lengths/alignment and actual reader
  tokens for COW. Native FrameView leases survive queueing/encoding/commit or
  abandonment, while source CPU descriptions retire after CPU use. Idle lookup
  records must not own LL objects or producer payload graphs.
- Actual G-buffer Legacy/PBR scalar consumption selects row ID through
  baseInstance/instance_id. The selected scalar branch skips packing/binding the
  old400-byte common record. Prepared jobs are not selected for a source-row view.
  Other draws and query order retain their existing native implementation.
- RenderMetalSourceRows is nonpersistent/defaultOFF until real correctness and
  complete-path cost gates pass. Both selected and ordinary paths use native
  Metal. Optional setup failure remains observable and retains required draws.

## Required local evidence and remaining engine work

First freeze/review one coherent source; integrate into the original experiment
tree and run doctor/configure/build/verify_app. Run real M1 GPU validation for
base-instance ordering, reflected page ABI/COW/readers/abandonment, region
publication, all Legacy/PBR output attachments/depth/query interleaving and old
compiled-library fallback. A parity test must prove source-row draws were bound.
Then measure complete actual-owner cold/warm/churn, small/many-source and
main/probe costs, preserving regressions and publication/bytes/call counts.

This source-row boundary is not the completed architecture. Next are persistent
group command banks with stable inherited view bindings; immutable cull/pass
inputs and useful work before CPU draw discovery; measured bounded preparation
and encoding; avatar/attachment pose and material/texture mutation; coordinated
main/shadow/probe work and decode/upload/source publication admission; and unique
UMA memory/retirement accounting. Alpha, terrain, water, lighting/projectors,
occlusion, UI/media/voice and existing Alchemy behavior remain required. CPU
scene LOD/occlusion/texture priority/RLVa policies cannot be silently removed.

Owner testing will assess the combined engine across representative scenes,
frame pacing, image/feature correctness, streaming and memory. Partial helpers,
nominal Metal4 adoption, CPU microbenchmarks and a shadow-only result cannot
establish the product objective. Next global defect ID178; root allocates IDs.

## First coherent freeze

All disjoint production/test edits frozen. Root integration includes per-view UV
qualification PPD175 and accepted-render bound counting PPD176. PPD173 LRU/lease
union repair and PPD174 withdrawn-region proof repair have authored native
regressions. Capture telemetry records enabled views/models, ordinary candidates,
unsupported/owner-proof skips and metadata failures; renderer logs setup attempts,
accepted draws and source-page bytes. No silent unused-path success is inferred.
Root will commit/integrate locally and execute required build9, then native gates.
