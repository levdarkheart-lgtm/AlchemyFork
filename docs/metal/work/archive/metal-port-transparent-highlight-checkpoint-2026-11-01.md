# Native transparent-highlight execution — 2026-11-01

**Work items:** M-006/M-007/M-008 native output ownership.
**Integrated code:** `8361addd0f`.

## Executed work

1. Completed a bounded production audit of active legacy/PBR transparency,
   materials, lighting, coverage, blend and glow behavior. No valid-production
   divergence was found, so no speculative shader change was made.
2. Traced Highlight Transparent through `LLDrawPoolAlpha::renderDebugAlpha()`
   and proved world and HUD diagnostic draws could be absent while native
   presentation remained eligible.
3. Captured static and rigged draws at the exact model/skin boundary, retaining
   smoke texture, transforms, classification color and avatar palettes.
4. Added static/rigged world pipelines with RGB-only alpha preservation and
   static/rigged ordered-HUD pipelines with live render state.
5. Added strict resource/palette validation that fails native authority closed
   without removing current GL migration output.
6. Kept all exact RLVa behavior/image work in M-013. The renderer work is core
   editing/output support, not an RLVa parity expansion.

## Executed checks

- Focused physical RelWithDebInfo cases: 1,364 world assertions and 485 HUD
  assertions under Metal API/GPU/load-store/shader validation.
- Complete physical Metal suites: 45,008 assertions/51 cases in RelWithDebInfo,
  Debug and Release under the same validators.
- Neutral CTest gate: 484/484 in 246.16 seconds.
- Warnings-as-errors touched-object/MSL build: pass.
- Source-exact Release Development `26.4.0d68036`: build/stage/sign pass at
  `8361addd0f`; deep strict signing and 349/349 ARM64 verification pass.
- Executable SHA-256:
  `064266af7bf95d6e5ddfefe2796ffd0015d8316b22d2bd7fa13ace27cdfd9fdf`.
- M1 Max doctor, harness validation and tooling tests: pass.

## Priority decision

The concrete Highlight Transparent omission is repaired and covered. Continue
at final diagnostic-overlay and ordered-UI publication boundaries, editing only
a demonstrated reachable omission. Full feature/stability, strict no-GL and
qualifying performance evidence remain open. Exact RLVa parity remains the
owner-deferred M-013 nice-to-have with its enable/save/restart prerequisite.
