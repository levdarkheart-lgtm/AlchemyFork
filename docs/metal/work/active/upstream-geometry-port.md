# Coherent upstream geometry port — 2026-11-13

Status: ready_for_owner; combined build/validation passed. Owner authorizes geometry
next, sky afterward. Base0c68a66e2189d79ccf221352ecea1fa905d3ca79, canonical
branch codex/apple-silicon-metal; upstream pinned408fd6c014e6e3b2b90dfe61265f34df0628f450.
Final production/test/CMake sourceb16fa27efab2c88a60793bcc938e5876d4fa9a9f;
preceding complete build3ee7786e67 also passed before final PPD-065 correction.
All work remains local; no push or owner acceptance. Five unrelated pre-existing
untracked research notes remain untouched. Sky is not part of this batch.

## Scope and original behavior

[Original geometry audit](../../research/upstream-geometry-review-2026-11-13.md)
maps final #567/#571/#572/#573/#575/#576/#577 and their landed tails. This is a
coherent port, preserving earlier PPD-022/032–034/043–044/048/052 safety, face,
octree/sculpt and native rendering corrections. Final shared code replaces
volume generators, analytic normals, remap/cache/tangent/planar work, uploader
simplification/normal restoration and avatar triangle ordering. Custom
meshoptimizer1.3#5, thread scratch/compact/SIMD and strict planar arithmetic are
part of the same dependency contract. The old Mikk wrapper/vendor is retired
after its final consumer migrates; retained license metadata remains intact.

Native boundary: existing LLVertexBuffer CPU shadows publish Metal stores into
native draw captures. Preserve buffer publication, PBR tangent handedness,
selection/HUD capture, native uploader preview, sculpt readback, media/voice and
all unrelated functionality. No in-fork GL backend, shaders or timing-policy
changes. LLFace geometry body uses the last geometry reference d1da0360ee;
its native methods and texture visibility remain retained. GLTF keeps tangent
arrays required by native buffers and picking even where final upstream's
other consumer assumptions permit removing them.

## Ownership and integrated source

Independent clean worktrees, exclusively owned paths; root owns all canonical
CMake, shared LLFace/LLVOVolume/pipeline/buffers and documentation. Integrated:

- Infrastructure30fe/2fbe → f5e7028aa3/f01ce20be0: scratch, compact, SIMD,
  single-package registry pin and typed overflow repair.
- Core20e4/d81/8d98/e622 →010b265d8f/e0f61b7406/2876762102/84844a3646:
  final geometry, strict planar SIMD, transactional owners, compatible glTF
  tangent generation/admission and warm flexible tangent owner reuse.
- Uploaderc0ff/fac/452 →9c47c79dd1/7effd61829/34be2c8e04: attribute-aware
  simplification, real model identity normal restore and complete LOD staging.
- Avatar0df →3c0f5906c5: optional triangle ordering retains mesh on allocation
  failure; hair/skirt/eyelash order retained; detail UV and binormals corrected.
- Clone test12ad/e82 →021fa0b910/ebc44ad7a4; integration5ce42a034e and
  in-place failed-fill admission3ee7786e67, base allocation retryb16fa27efa.

## Preregistered defects and adaptations

PPD-053 tracks19retained expected-failure contracts. PPD-048 was reopened for
new constructor/copy/normal/remap/tangent/simplifier/LOD allocation hazards;
all required replacements stage owners before publication and propagate failure.
PPD-054 normal restoration associates pre-mutation originals with real models.
PPD-055 shares planar mapping/tangent/picking frames. PPD-056 admits exact index
slots and bounded vertex slots before writes. PPD-057 rejects typed scratch
multiplication overflow before allocation. PPD-059 rejects short attributes,
unpaired skin data and short strips/fans before reads and preserves runtime
primitive topology/picking. PPD-060 honors detail-UV headers;061 initializes
morph binormals from binormals. PPD-062 preserves warm stale tangent ownership.
PPD-063 propagates clone failure, recalculates layout offsets, preserves U32
physical index capacity/logical counts and warm matching stores. PPD-064 fixes
the base partition's vertex/index dimension comparison. PPD-065 retains dirty
state after failed base grass/terrain allocation, allowing priority retries.

PPD-058: failed fresh fills reserve their slots but never register draw ranges.
Groups retain GEOM_DIRTY and priority-queue ownership, including retry-list OOM.
In-place rebuildMesh failure also withdraws the complete shared group's merged
ranges and face/selection admission before flushing. Animated child transforms
are restored and failed drawable flags retained; a full group retry refills it.
This is failure correctness, not an observed physical OOM or FPS result.

## Executed focused checks and retained failures

Host: Apple M1 Max64GiB, arm64, macOS26.5.2, Xcode26.6. All native/failure probes
run through LLDB, parse actual child status and backtrace/kill any stopped child;
no fault-stopped child is continued. No CrashReporter preference changes.

- Old exact0c68 math archive, only !shouldfail annotations removed:44cases,
  25pass/19fail,4274assertions/23fail, normal child42. Old failures retained.
- Initial core [llvolume]243811assertions/104cases (103pass+1optional real corpus
  skip); SIMD90412/19,vector608/14; final stale-owner regression9/1. Former19 contracts mapped to positive cases;
  updated zero-neighbor-normal name is documented in the core mapping receipt.
- Final math ASan+UBSan334840assertions/138cases (137pass+1optional corpus skip),
  child0; no diagnostics. This is CPU instrumentation, not a sanitized viewer.
- Scratch/compact62assertions/14cases; prerequisite SIMD3682/14. Actual custom
  remap collision fallback and allocator ownership controls pass.
- Registered simplifier/model1244assertions/25cases. Actual preview method
  restoration control old21failed checks, fixed66/3; staged LOD/allocation96/5
  normal and targeted sanitizer pass. Native preview blocks byte-identical to
  base; no live uploader/UI or model upload claim.
- Actual binary LLM loader19555assertions/6cases: optional allocation positions,
  vertex attributes, triangle/winding/order, cache, skin/detail/LOD ownership and
  base/instance binormals. Old detail/binormal controls return42 normally.
- Actual GLTF primitive production192assertions/5cases; old bounded control
  fails49/70checks in4cases. Real generated/authored tangents and picking covered.
- Actual queue-function hosting control: old7/20fail, repaired20/20pass.
  Clone function allocation-feedback control: old5/17fail, fixed17/17pass;
  exact partition predicate old1/4fail, fixed4/4pass. All child42/0 normally.
  In-place rebuildMesh old14/66fail, fixed66/66 across3scenarios; actual body
  with hosting collaborators covers merged ranges, all-face withdrawal, transform
  restoration and retry admission (not real LLFace execution).
- Exact base partition function: old7failed assertions, final67assertions/2cases
  pass. Required allocation failure keeps dirty state, retry succeeds; warm8v12
  buffers and empty/dead/clean cases remain correct. No physical OOM/GPU claim.
- Exact LLFace guard block with real adjacent LLVB slots and native SIMD index
  helper: old19failed assertions, fixed87assertions/1case pass. Rejected fills
  do not write; aggregate-bounded control uses no out-of-bounds access. This is
  guard-block validation, not execution of the complete LLFace method.
- Real Metal clone fixture5265assertions/4cases, five indexed pixel draws;
  old method control8failed assertions, normal child42 before unsafe writes.
- Generated volume→native buffers→G-buffer default/planar normal-map test
  passes851assertions/1case on finalb16 with Metal validation, child0. Broader
  native rendering checks also pass in the combined suite below.

Focused reports: [core](../../research/upstream-geometry-core-validation-2026-11-13.md),
[infrastructure](../../research/upstream-geometry-infra-validation-2026-11-13.md),
[uploader](../../research/upstream-geometry-uploader-validation-2026-11-13.md),
[avatar](../../research/upstream-geometry-avatar-validation-2026-11-13.md),
[clone](../../research/upstream-geometry-clone-validation-2026-11-13.md),
[face slots](../../research/upstream-geometry-slot-validation-2026-11-13.md),
[base partition retry](../../research/upstream-geometry-partition-validation-2026-11-13.md).
Raw local receipts/scripts/logs: artifacts/metal/upstream-geometry-20261113/.

## Combined candidate and final verification

Exact app: `/Users/dekstroza/Programming/AI/Secondlife/Alchemy/build-Darwin-ninja-os-arm64/newview/Release/Alchemy Metal Geometry.app`.
Version26.4.0.68952, sourceb16fa27efab2c88a60793bcc938e5876d4fa9a9f,
executable SHA256b1b1de0b83bd5e02d2d336009977d0c403db3883212c86fb8be1580fc176db10.
app-identity.json records complete bundle metadata, owned helper hashes and
retained compiled feature symbols. No viewer/grid launch or upload performed.

Canonical Ninja meshoptimizer upgrade completed with only1.3#5 changed, tests/
catch2 and all other dependencies retained; meshoptimizer-upgrade.json records
exact command/exit0. Source5ce build intentionally interrupted130 for PPD058
review; build-attempt-5ce42a/ retained. Complete3ee doctor/configure/build exit0
(build940.58s), then finalb16 doctor/configure/build exit0 (11steps,26.70s).
Release/LTOoff/testsON, channel Alchemy Metal Geometry, jobs8. No performance
claim follows from build timings. build-candidate.py holds exact commands.

validate-candidate.py passes against immutable final source. Its JSON receipts
record command arrays, sourceSHA, working directories, exits, elapsed times,
logs and actual contained child results. validation-summary.json totals:

- 192 registered CPU test commands:2794087assertions/2666cases,13existing case
  skips; every process child0/state10. Includes canonical volume242254/90,
  allocation/decode424/13, simplifier1092/23, model152/2, avatar19555/6,
  GLTF192/5 and SIMD kernels90412/19. Default file filters exclude hidden corpus
  tests; focused sanitizer tag run explicitly skips the unset real corpus.
- 22 contained UI processes:21862assertions/205cases, child0.
- Five native Metal processes:269602assertions/35cases, child0, API validation
  enabled. Geometry/buffer publication/clone11328/7; native morph/bake13203/3;
  retained texture/HDR/material/probe/postprocess245071/25. The851/1geometry
  preflight and5265/4isolated clone results above overlap these totals.
- CTest stray-case listings pass;116harness/tool unit tests pass using existing
  build venv. First system-Python tool attempt lacked yaml (114attempted,
  one import error); retained log, corrected environment, no dependency edit.
- verify_app.sh:346Mach-O files, all arm64. Deep strict codesign passes.
  Executable embeds exactb16 source. Main/owned CEF/VLC imports contain no
  forbidden graphics libraries/symbols. Defined native music23/WebRTC477/
  OpenHTJ2K61/OpenJPEG1 markers retained; symbol counts do not prove feature QA.
- Strict source audit3540files/913informational rows,0blockers/0errors. This is
  source/import evidence, not runtime no-context or foreign-media proof.
- Final harness validation33required files passes; CURRENT160/root ledger180
  lines retained. All earlier red controls/failures remain in evidence.

Product advancement: coherent upstream geometry and uploader correctness now
integrates with actual Metal buffers/G-buffer/normal maps and retained native
rendering checks. No scene screenshots, viewer visual parity, physical OOM,
paired M1 FPS/memory results, prolonged stability or full release gates claimed.
PPD-048/053–065 are ready_for_owner, not closed; all push approval not_granted.

Next: owner tests this exact candidate in-world (ordinary/sculpt/flexible prims,
planar/default normal maps, alpha picking, PBR selection/media zoom, avatar
hair/skirt/eyelash, uploader normals/restore/Auto LOD). Keep source/local commits
until owner agreement and explicit scoped push approval. Sky is the following
batch; it remains unchanged. Full product parity/runtime no-GL/performance gates
remain open independently of this geometry delivery.
