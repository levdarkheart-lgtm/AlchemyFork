# Native-only 360 snapshot cutover checkpoint — 2026-11-02

## Observed source state

Commit `a986999d95aeb67e1ce8528945b6cadb467738a0` removes the production
OpenGL RGB fallback from `LLViewerWindow::simpleSnapshot` and makes its
contract native-only. `LLFloater360Capture` now treats all six cube faces as
one fail-closed transaction: one failed face stops capture, and no partial face
set is encoded, given a JavaScript footer or sent to the WebGL stitcher.

The former whole-panorama fallback loop and its `allow_metal` / `used_metal`
selection API are gone. Common restoration still unfreezes the world, restores
the camera and rendering controls, resumes timeout/stat recording and
re-enables the capture controls. Reused face storage is recreated before each
face so stale data cannot be published after failure.

This removes one production `gGL.readPixels` call site. The only remaining
production call sites are the color/depth row reads in
`LLViewerWindow::rawSnapshot`. Strict inventory remains 2,408 files / 3,266
findings because `audit_gl.py` counts the shared `glReadPixels` implementation,
not each `gGL.readPixels` caller. Strict mode exits 1 as required while findings
remain.

## Feature evidence

The earlier owner-attended 360 checkpoint in
`CURRENT-through-2026-11-01.md` records six native 2048x2048 faces, a coherent
4096x2048 panorama and an OpenGL-reference replay. All twelve faces and both
panoramas were inspected there for cube ordering, upright up/down faces,
continuous seam topology, plausible channel ordering and missing/duplicate
directions. That prior evidence establishes the native producer used here; this
checkpoint changes fallback authority and aggregate failure handling.

Per owner direction, no viewer was launched, driven, photographed or compared
for this cutover. Visual questions continue to accumulate for the
owner-attended daily review rather than being re-run autonomously.

## Exact validation evidence

- Required warnings-as-errors RelWithDebInfo wrapper build passed all 221
  invalidated actions before commit. The source-exact rebuild at
  `a986999d95aeb67e1ce8528945b6cadb467738a0` produced Development build
  `26.4.0d68116` (`26.4.0.68116`).
- With `MTL_DEBUG_LAYER=1`, shader validation disabled, load-action validation
  enabled and errors routed to `nslog`, the physical M1 Max Metal suite passed
  52,209 assertions across 52 cases.
- The source-exact RelWithDebInfo `newview_tests` binary passed 98,973
  assertions across 743 cases: 742 passed and one platform CRT case skipped as
  expected.
- `tools/metal/verify_app.sh` passed 349/349 bundled Mach-O slices as ARM64.
- Executable SHA-256:
  `130eb49ef0ff657933b2d317353712a5c2605c056b9c0cb036c6d3acb7af8d5f`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- `otool -L` still reports both Apple OpenGL and Metal in the main executable.
- Exact strict report:
  `artifacts/metal/gl-strict-360-native-only.json` (ignored evidence artifact),
  2,408 files / 3,266 findings / expected exit 1.

The product is not complete. Application-owned GL context, traversal and
resources remain; the two ordinary snapshot row reads remain; prolonged
stability, independent build/review evidence, strict no-GL, complete parity and
measured performance gates remain open.

## Next executable boundary

Trace both `rawSnapshot` row reads across color, scalar depth, packed depth24,
RGB/RGBA, cutout, UI/HUD, tiling, post-processing and supersampling. Extend the
existing native snapshot owner where required, then remove the GL fallback only
when every supported path remains feature-preserving and fail-closed.
