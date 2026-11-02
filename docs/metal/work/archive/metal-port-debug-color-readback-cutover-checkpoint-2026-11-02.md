# Metal-port execution — debug-color native readback — 2026-11-02

## Outcome

Commit `a9a0edd4e2` replaces the final production `ALGPUReadback` consumer with
an exact ordered native Metal pixel snapshot and bounded asynchronous readback.
It then deletes the unused GL buffer/pixel readback class and its GL-only tests.

The source preserves `DebugShowColor`'s historical pre-debug-text boundary,
lower-left coordinates and RGBA display without a recurring CPU/GPU wait.
Missing or incomplete native production leaves the prior diagnostic text in
place rather than importing GL pixels. No traversal suppression, feature
deletion or parity waiver was introduced.

## Evidence

- Source-exact warnings-as-errors RelWithDebInfo build `26.4.0d68114` passed at
  `a9a0edd4e225681e49ae5b06f47284af31b70594`; the initial dependency rebuild
  completed all 1,392 actions.
- Physical M1 Max Metal validation passed 52,209 assertions in 52 cases with
  API/load-action validation enabled, including exact ordered-pixel coverage.
- Focused surviving `algpubuffer` coverage passed 108 assertions in nine cases.
- Bundle verification passed 349/349 ARM64 Mach-O slices.
- Strict inventory: 2,408 files, 3,266 findings, expected exit 1; this is a
  measured reduction of three findings.
- The executable still links OpenGL and Metal; native-only is not claimed.
- No viewer launch, manual driving, snapshot or comparison occurred.

The full product remains incomplete: GL context/traversal/resource ownership,
snapshot fallback reads, linked/runtime no-GL proof, complete feature/stability
parity, owner-attended visual review, measured M1 Max performance and
independent release review are still open.

## Resume action

Trace the three remaining production `gGL.readPixels` sites in
`LLViewerWindow::rawSnapshot` and `simpleSnapshot`. Map all supported snapshot
formats, tiling, cutout, UI/HUD and 360 behavior to existing native producers,
implement uncovered variants, then remove GL fallback only with exact
feature-preserving validation.
