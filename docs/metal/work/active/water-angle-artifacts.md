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


## Executed corrective validation and candidate

Sourcea6f4e8ff41d48eff2a48c6b740cd1579eda88944; same M1Max/64GiB/macOS26.5.2/
Xcode26.6. Doctor0; test configure0 and actual test-target build0. Identical
test executable SHA643eab51e10f00b610e0291baf933097bd2dc8521c5dd3305e1e916d1a415754
loads baseline0f69b4aa library then correctedca44d100 library, with Metal API
and GPU validation explicitly logged enabled. Old library exits42:8failed of
171318checks across4perspective/Z cases (all3545/3547interiorpixels differ);
maximumchannelerror .117880/.293213. UniformW controls pass. Corrected library
exits0 with171318assertions; no test/expectation changes or weakened tolerances.
The fixed test bundle is restored after the controlled swap. Existing actual
water-normal/world and water-cull cases pass32197assertions/4cases, exit0.
This tests variableW interpolation and retained scene-depth coordinates, not
near-plane crossing or full live-region depth/stencil/geometry classification.

Full viewer configure0 and wrapperbuild0, Release/O3/LTOOFF, native music and
startup source byte-identical to approved8f11241c9. Exact channel Alchemy Metal
Water Haze Fix, version26.4.0.68894 (revision counter follows scoped history).
New executable SHA4994146e0fb4ab55494fc841955f67f772c261f060c42fcee84f6ac6fd5945d9.
Packaged library SHAca44d100f0c6cfc5c1ef56ed3f01e7ac2b0b5f7c87d672165de3899aac2f9fe7
exactly matches physical GPU test. verify_app0:346MachO/zero invalidARM64;
deep strict signature0; strict source3520files/912info/0blockers/0errors.

Immutable owner candidate copied to private
artifacts/metal/water-angle-artifacts-20261112/Alchemy Metal Water Haze Fix.app;
copy executable hash equals source, copy strict signature0. Active68898 app
SHA9d862876 remains unchanged. No viewer restart, settings/cache mutation,
new login, public capture or push. PPD-013 ready_for_owner, not verified.
Next: owner restarts into the exact new candidate when ready and repeats the
reported Chouchou camera angles; retain failed retests and reopen analysis if
visible bands remain. Original runtime comparison and full product gates stay
unverified; this does not claim a frame-rate or login-speed improvement.

Private receipts: configure-tests.json/log, build-tests.json/log,
gpu-tests/projection-receipt.json and baseline/fixed.log,
gpu-tests/existing-water-and-world.json/log, configure-viewer.json/log,
build-viewer.json/log, doctor.json/log, verify-app.json/log, signature.json,
strict-source.json and build-identity.json. These retain exact argv, statuses,
source identity and hashes. Initial harness failed only because two new
checkpoint lines exceeded CURRENT160/ledger180limits; checkpoints folded inline,
then harness33files passes. No test assertion was changed after execution.

Independent remoteea1ff8d9d5 manual-release/tooling work arrived during this
local fix; not integrated or modified here. Its untracked manual-release note
and the five preexisting research notes are outside this commit. Preserve
that scope during later normal integration; never force-push this local branch.

## Separate PPD-015 coordinate substitution

Originalclass1/environment/waterVaryingsV.glsl50–53 defines eye-space
vary_position separately from agent-relative `view=position-eyeVec`. Original
class3/environment/waterF.glsl194–197 steepens `wavef` using normalizedview.z.
Nativebuiltin3921–3923 also preserves both; above-waterfragment7989–7990
normalizes eye and agent vectors, but8028 uses eye-spaceview_vector.z instead
ofagent_view_vector.z. The grazing-water adjustment therefore uses the optical
axis component rather than the vertical relative to water. Refraction uses
steep_wave.xy at8038 and sun-normal conversion at8030. Source mismatch proved,
physical effect/owner-band attribution unknown. Register separately asPPD-015;
no correction combined with this haze interpolation experiment.


Canonical registry coordination: an independent workflow session reserved
PPD-014 for CI/release behavior while this water followup was being recorded.
Preserve that existing record unchanged; the newly discovered uncommitted
water-coordinate record receives PPD-015. Corrective haze history stays under
PPD-013, not the neighboring CI record. No shared stable ID is renumbered.
