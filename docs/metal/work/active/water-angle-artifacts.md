# PPD-013 — camera-angle-dependent water coverage

Owner-reported2026-11-12 in Chouchou(180,161,21), at screenshots21:27:05,
21:27:42 and21:29:59UTC. Running26.4.0.68898/sourcef826eea135, combined native
music and owner-verified startup fix. Executable SHA256
9d862876b866fd4ab2cc4bdc2e2422182d9e185f44369e76bf98cbb259b2360e.

The first image has repeated broad diagonal bands across shallow water; the
second yaw changes coverage and largely removes those bands. The third has
a much larger predominantly blue-water region. Read-only live preflight through
the exact app observes both striped and blue-water states while the owner turns.
Location bar independently shows Chouchou(180,161,21). No camera angles, graphics
controls or GPU draw/attachment captures were recorded, so this is not a gated
reference-vs-candidate image comparison or a proved producer attribution.

Private artifacts/metal/water-angle-artifacts-20261112/ retains the three owner
PNGs with SHA256s in owner-screenshots.json. Original temporary PNGs preserved.
No screenshots/raw captures are uploaded. No viewer restart, settings, camera,
account or cache mutation. Original-source audit and native shader/fixture audits
are assigned read-only; integrator owns shared pipeline and documentation files.

## Current investigation

Compare original1eb44bbd ALWaterField/deferred water pass, near/far geometry,
water/terrain depth and exclusion/stencil invariants against native owners.
Existing normal-filter water fixture clears scene depth to far and uses an
orthographic projection; it does not cover shallow water/terrain perspective
angle overlap. Depth conflict, missing raster invariance and coverage/culling
remain candidates. No implementation change or verification claimed.


## Original contract and first confirmed divergence

Reference1eb44bbd class3/deferred/waterHazeV.glsl passes `vary_fragcoord=pos`
(full clip-space position); waterHazeF.glsl computes `vary.xy/vary.w*.5+.5`
perfragment. Native almetal_builtin.metal3553 pre-divides at each vertex into
float2texcoord, then default perspective interpolation divides by W again.
For barycentrics(1/3,1/3,1/3), UVy(0,.5,1), W(1,5,10), correct .5 becomes
.153846. The wrong UV selects retained scene depth, reconstructs a different
viewposition and changes haze plane classification/transmission pertriangle.

Fix restores original full-clip varying and fragment division, preserving Metal
raster Y-flip, depth-range conversion, field geometry, quality, ordering and
resource ownership. Fullscreen haze supplies the same fullGLclip contract.
Shader agent exclusively edits builtinshader; validation agent exclusively adds
a new production-shader physical regression, root owns CMake/shared docs/build.

Original water depth/order/patch mapping audited: procedural percell heights,
wave patches before flat fringe, semantic LEQUAL/reverseGEQUAL with depthwrites
and stencilbit2 match. Apparent occlusion Y discrepancy was explicitly withdrawn:
Metal viewport maps NDC+y totop, raster negatesGLY, giving native row=.5+.5*GLY,
matching compute. No Y-flip or depth-bias workaround is warranted. Missing
flat/wave raster invariance and separate eye/agent-space steepness difference
remain unproved/separate concerns; no speculative change included.

Existing water normal fixture is underwater/orthographic and cannot expose the
confirmed variableW defect. New regression compares actual production field
vertex+fragment against fullscreenvertex+samefragment on asymmetric scene depth
inside valid coverage for forward/reverseZ; it must fail the frozen old library
and pass corrected shader without changing the test or expected results.


## Local implementation checkpoint

Restored homogeneous clip4 interpolation and perfragmentdivision in the actual
builtin water-haze shader. Fullscreen output is algebraically unchanged (W=1);
raster orientation, depth ranges, binds, fog math and all water geometry remain
unchanged. Independent original-contract review passes. New actual-production
shader fixture compares field/fullscreen fog images over nonuniformdepth, two
perspectiveyaw variants plusuniformWcontrol, both depthconventions. Build and
GPU red/green runs remain pending at this source checkpoint.

Preserved frozen baseline shader SHA256b52a0fda894ebb63e015ceaaad4d709e187a59ae0a82029ecf6f9d5d7ec2588b
and original offline library0f69b4aa7d3c8dcdffcaa567a1134f27c324f61a8278d9520409e38fd0796a52
in private baseline/. No change to the active68898 app or running process.
