# Metal-port execution — pipeline pixel-ingress cutover — 2026-11-02

## Outcome

Four coherent source commits remove every direct GL pixel fallback used by
`pipeline.cpp` Metal consumers:

- `bdefc6b2d7` — native-only avatar-impostor publication;
- `60c9ee55f2` — native-only directional/projector shadow depth;
- `bc79f48aca` — native-only hero-radiance generation;
- `35082663e5` — native-only base-light-map input.

The implementation deletes PBO allocation/fence collection, GL attachment and
texture reads, CPU staging, format translation, fallback Metal allocation and
re-upload bookkeeping. Native producer descriptors and generation identities
are now the validation authority. Missing data fails the Metal transaction
closed; the source does not silently substitute a partial generation.

## Evidence

- Source-exact RelWithDebInfo build `26.4.0d68108` passed with warnings as
  errors at `35082663e5`.
- Physical M1 Max Metal validation passed 52,190 assertions in 52 cases with
  API/load-action validation enabled.
- Bundle verification passed 349/349 ARM64 Mach-O slices.
- Strict inventory: 2,408 files, 3,279 findings, expected exit 1; 58 fewer than
  the prior active checkpoint and zero direct `gl_function` findings in
  `pipeline.cpp`.
- Harness validation and all 27 harness unit tests passed.
- No viewer launch, manual driving or comparison occurred.

The product is not complete: GL traversal, shader/state/resource classes,
SDL/CGL context ownership, linked/runtime no-GL proof, feature/stability parity,
performance proof and release gates remain open.

## Resume action

Inspect the two ordinary reflection-probe PBO readbacks in
`llreflectionmapmanager.cpp` against the native probe owners, remove the next
proven duplicate crossing, and repeat source-exact build, physical GPU and
strict-audit validation.

