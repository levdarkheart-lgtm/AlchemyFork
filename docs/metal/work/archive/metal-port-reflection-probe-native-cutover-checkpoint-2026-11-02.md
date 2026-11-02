# Metal-port execution — reflection-probe native cutover — 2026-11-02

## Outcome

Commit `4b6093028e` removes the default- and ordinary-probe GL/PBO mirror from
`LLReflectionMapManager`. Metal consumers now receive radiance cubes and packed
SH rows only from the ordered native six-face producer. Slot generation,
scheduled/realtime lane identity and coherent radiance/SH publication remain
fail-closed.

The legacy renderer's own probe array still updates; it is no longer copied
through CPU memory into a second Metal authority. No traversal suppression or
feature waiver was introduced.

## Evidence

- Source-exact warnings-as-errors RelWithDebInfo build `26.4.0d68110` passed at
  `4b6093028e9241706a5d6bcfe5f21b59a9a6dd78`.
- Physical M1 Max Metal validation passed 52,190 assertions in 52 cases with
  API/load-action validation enabled.
- Bundle verification passed 349/349 ARM64 Mach-O slices.
- Strict inventory: 2,408 files, 3,269 findings, expected exit 1; ten fewer
  than the preceding checkpoint and zero direct `gl_function` findings in
  `llreflectionmapmanager.cpp`.
- The executable still links OpenGL and Metal; native-only is not claimed.
- No viewer launch, manual driving, snapshot or comparison occurred.

The full product remains incomplete: GL context/traversal/resource ownership,
linked/runtime no-GL proof, complete feature/stability parity, owner-attended
visual review, measured M1 Max performance and independent release review are
still open.

## Resume action

Inspect the native scope-sampling and scene-pixel readback owners in
`pipeline.cpp`; remove the next proven GL fallback, then repeat source-exact
build, physical Metal and strict-audit validation.

