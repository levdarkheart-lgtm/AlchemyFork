# Classic avatar position-W shader repair

Owner's68638 live screenshots identified skin/head triangles extending from a
specific avatar while coat and legs remained mostly coherent. Corruption varied
with movement/visibility and sometimes stopped after arrival. Normal session logs
had no nonfinite palette warning. No occlusion-box color-write defect was found.

## Confirmed original contract and incorrect native consumer

Exact original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`:
`class1/avatar/avatarV.glsl` and deferred avatar shaders declare position vec3 and
explicitly form `vec4(position.xyz,1.0)` before joint-row dot products.
`llappearance/llpolymesh.cpp` clears all4 base-coordinate lanes, loads ONLY XYZ,
and copies16bytes into morph coordinates; `LLViewerJointMesh::updateFaceData`
copies all4 lanes to the VBO without imposing W1. Stored SIMD padding therefore
is not homogeneous position. Morph data does not supply a W1 invariant.

Native `almetal_lit_preview_avatar_skin_vertex` and `almetal_avatar_cloth_vertex`
used raw float4 position for palette dots. W0 omits the joint translation; other
padding values scale it. The matrix can remain entirely finite, so the previous
finite-palette diagnostics do not detect the defect. Object four-influence
skinning is a different contract and is unchanged.

Repair: construct `float4(position.xyz,1.0)` at entry of those two shared helpers.
This covers main opaque/masked, forward avatar alpha, cloth and preview consumers.
Rigid and spot-shadow avatar entries already normalize XYZ/W1. No weight clamp,
palette rewrite, geometry omission, quality change or visibility workaround.
Root applied the shared shader change; independent source review approved it.

## Actual GPU regression — M1 Max, Release, API+shader validation

New `almetal_classic_avatar_pose_test.mm` builds an indexed30-vertex surface,
nonzero index offset,15 distinct affine transforms with scale/shear/translation,
scalar fractional weights including the joint14 endpoint, and an independent
original-style CPU endpoint-transform/blend oracle rendered through rigid code.
Compares all four MRTs and raw D32 exactly under forward and reverse Z. Position
padding0 and per-vertex−17/4096 must be irrelevant. Real nonzero cloth wind has a
positive effect; its padding invariance and shadow counterparts are checked too.

The SAME corrected test executable:

- Actual previous68638 packaged metallib: exit42,256 assertions,10 failures.
  Failures are the wrong padding/output/coverage results; no fixture-init error.
- Repaired source library: exit0,256 assertions,1case passed.

Private mini test bundle pins the exact old library SHA
`bca357a8868e3c4a82c9e8eb21cc3b27fe783806e83651742e2919b4671c3f0d`.
First fixture iteration mistakenly used a2D shadow destination where the direct
API requires a one-slice2D-array; both preliminary runs stopped at that fixture
error after color checks. Corrected fixture reruns above are the decisive proof.
Root executed all builds/GPU runs; peers source-reviewed the repair and fixture.

Exact commands/logs/hashes and three owner screenshots are preserved under ignored
`artifacts/metal/private-classic-avatar-position-w-20261110-01/`.
Canonical app rebuild/verify and live visual confirmation are next. The isolated
GPU result proves the shader ABI defect and repair, not full live mesh/hierarchy,
teleport/loading or all rendering parity. Earlier repairs remain included.

## Independent additional owner requests

Teleport camera CPU handlers, interpolation and startup reset paths match exact
original source. Retained native presentation may hold a prior image, but that is
not proof of the reported continuous fly-in/zoom; no guessed camera change made.

Parcel music requests reach LibVLC, but the actual playback child exits code1.
Audio-only initialization, package module paths and host failures are under
investigation. This is separate from user volume and the avatar shader repair.

The possible dirty-buffer publication boundary repair remains private/unapplied;
it was not proved to cause these live triangles. No broad renderer changes are
bundled into this two-helper repair.
