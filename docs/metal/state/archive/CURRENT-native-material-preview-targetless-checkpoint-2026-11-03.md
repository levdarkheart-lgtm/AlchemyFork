# Native material-preview targetless scheduler checkpoint — 2026-11-03

## Observed source state

Source checkpoint `c209340d5e89c9e5832957d1b952365806d92054` adds a narrow
targetless dynamic-texture contract and implements it for Darwin glTF material
previews. `LLViewerDynamicTexture::updateAllInstances()` now asks an item to
complete and publish without the shared GL target before it performs the first
retained shader/vertex-buffer unbind or calls `ALOnDemandTarget::bind()`.
Global GL preparation and the final GL flush are lazy and occur only when some
item in the update requires the retained transaction.

An eligible native PBR material preview snapshots its camera, renders the exact
Metal target and adopts it into the viewer texture inside this targetless
attempt. A run containing only successful targetless items therefore does not
acquire, bind, clear or flush a Preview/Bake GL target and does not perform the
base GL viewport restore. This retains the earlier CPU-matrix, preview-owned GL
setup, geometry, light/probe, post and framebuffer-publication cuts.

The contract is fail-closed. Unsupported eligibility, native render failure or
native publication failure returns the item to the scheduler still pending.
The scheduler then performs its original target acquisition and
`preRender`/`render`/`postRender` transaction in the same frame. The
target-backed retry consumes a one-shot marker and runs the retained callback
instead of repeating the identical Metal attempt. If the shared target cannot
be acquired, a later frame retries native before attempting retained fallback.
Every other UI preview and avatar bake keeps its prior behavior and order.
Non-Darwin behavior is unchanged.

This is not complete material-preview GL independence. `needsRender()` still
requires the auxiliary GL screen and retained `ProbePostPing` plan before the
targetless attempt is considered; the dynamic texture still has dual resource
ownership; classic, non-HDR, post-suppressed and incomplete native inputs still
fall back; and other dynamic textures remain GL-rendered. The global
`gGLManager.mIsDisabled` guard also precedes targetless scheduling. These are
the next ownership boundaries, not waived gates.

No viewer was launched, driven, photographed or compared. The existing
material-preview visual question was advanced to exact build 68175.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and the Metal compiler.
- `tools/metal/build.sh build` exited 0 at exact source
  `c209340d5e89c9e5832957d1b952365806d92054`. The warnings-as-errors Apple
  Silicon Xcode Release build produced Development build `26.4.0d68175`
  (`26.4.0.68175`). The interface change caused 231 expected compilation-cache
  misses; the package linked, signed and validated successfully.
- The exact-source Ninja RelWithDebInfo test targets rebuilt successfully. The
  separate warnings-as-errors appearance-utility configuration reports build
  68175 and its target had no work pending.
- Focused lit/PBR material-preview GPU coverage passed 426 assertions across
  two cases with Metal API and GPU shader validation enabled.
- With the same validation layers, full `almetal_tests` passed 52,381
  assertions across 55 cases. `alrender_tests` passed 557,312 assertions across
  137 cases.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases.
  `llrender_tests` passed all 1,513 assertions across 203 cases: 202 passed and
  the established single-palette font case skipped.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped. Compiled and runtime build numbers both
  reported 68175.
- `llrender_gl_shared_tests` reproduced the known non-gating exit 42: 8,335 of
  8,340 assertions passed; 181 of 224 cases passed, four failed and 39 skipped.
  The failures remain three frame-fence pacing/budget cases and one depth-format
  case. This run is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` exited 0 and checked 349/349 bundled Mach-O slices as
  ARM64.
- Viewer executable SHA-256:
  `443d11a410455a5b81abab8c6d22752b6774e7fd105fd4bad2aee9d4e9a5d5b4`.
  Appearance-utility SHA-256:
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-native-material-preview-targetless.json` exited 1
  as expected at 2,408 files / 3,262 findings. Report SHA-256 is
  `c6bc62340c80ad112de586e8a31d7c558fc9389021f1c4947184a31eacbc08f1`.
- `file`, `otool -L` and `nm -u` confirm both primary binaries are arm64 and
  have no undefined `CGLTexImageIOSurface2D`; both Apple OpenGL and Metal remain
  linked. The strict no-GL gate is not met.

The product remains incomplete. Application-owned GL context, traversal,
dynamic-texture fallback/resources, unsupported material-preview variants,
complete parity and stability, paired profiling, independent review, and strict
linked/runtime no-GL evidence remain open.

## Next executable boundary

Split native preview readiness from retained auxiliary-screen/post-target
readiness so an eligible native attempt does not require those GL resources to
exist. Preserve the current same-frame fallback when retained prerequisites are
available, and leave an unsupported item pending when they are not. Then widen
the native material-preview contract toward classic/non-HDR/post-suppressed
modes and retire its retained resources only after reachability proves dead.
