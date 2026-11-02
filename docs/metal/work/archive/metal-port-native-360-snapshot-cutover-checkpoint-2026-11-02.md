# Metal-port execution — native-only 360 snapshots — 2026-11-02

## Outcome

Commit `a986999d95` removes six-face capture's production GL RGB readback and
whole-panorama fallback loop. `simpleSnapshot` has one exact native contract;
360 capture publishes only a complete six-face native aggregate. Any failed
face prevents encoding, JavaScript completion and stitcher navigation while
the established restoration path still runs.

This preserves the feature without mixed renderer ownership or partial output.
Prior owner-attended evidence covers the six-face producer's orientation,
seams and channel ordering. No viewer launch, snapshot or comparison occurred
for this cutover.

## Evidence

- Source-exact warnings-as-errors Development build `26.4.0d68116` passed at
  `a986999d95aeb67e1ce8528945b6cadb467738a0` after the preceding 221-action
  invalidated rebuild.
- Physical M1 Max Metal validation passed 52,209 assertions in 52 cases with
  API/load-action validation enabled.
- `newview_tests` passed 98,973 assertions across 743 cases with one expected
  platform skip.
- Bundle verification passed 349/349 ARM64 Mach-O slices.
- Executable SHA-256:
  `130eb49ef0ff657933b2d317353712a5c2605c056b9c0cb036c6d3acb7af8d5f`.
- Strict inventory remains 2,408 files / 3,266 findings, expected exit 1; the
  scanner counts the shared GL helper rather than each caller.
- The executable still links OpenGL and Metal; native-only product completion
  is not claimed.

## Resume action

Trace the two remaining production `gGL.readPixels` row reads in
`LLViewerWindow::rawSnapshot`. Preserve color/depth/depth24, RGB/RGBA, cutout,
UI/HUD and tiled/supersampled output through the existing native producer
before deleting the fallback.
