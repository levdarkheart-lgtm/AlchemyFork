# Native transparent-highlight checkpoint — 2026-11-01

**Observed host:** physical Apple M1 Max.
**Branch:** `codex/apple-silicon-metal`
**Code checkpoint:** `8361addd0f` (`feat(metal): replay transparent highlights natively`).

## Bounded audit and reachable omission

The source-led transparency/material audit compared the production legacy and
PBR alpha paths through their equations, alpha cutoffs, sampler/color-space
selection, specular antialiasing, shadow/probe/SSR/hero inputs, glare-derived
coverage, fog, local lights, custom blend state and glow replay. Metal's extra
bounds checks do not alter valid viewer-produced inputs. No reachable
shader/material divergence was demonstrated, so that bounded audit closed
without a speculative edit.

The next exact native-output boundary was `LLDrawPoolAlpha::renderDebugAlpha()`.
The viewer draws Highlight Transparent classifications after the retained
native world replay; HUD attachments are drawn inside the ordered UI capture.
Neither output was represented in native eligibility, so an otherwise eligible
Metal frame could omit a core editing/diagnostic feature that remained visible
in the migration GL frame.

## Integrated repair

Commit `8361addd0f` captures each debug draw after its model matrix and, when
rigged, successful avatar palette upload. It preserves the smoke texture,
texture/model-view-projection matrices and red/blue/green classification.

World replay now supports static and object-skinned highlight vertices plus
write-alpha and RGB-only pipeline variants. Highlight Transparent selects the
RGB-only form so source-alpha blending cannot modify retained glow/coverage
alpha. Ordered HUD replay adds static and skinned variants while preserving the
current GL blend, depth, cull, sampler, viewport and color-write state. The HUD
constant layout retains the complete prior 240-byte PBR prefix and appends the
highlight color at byte 240; the existing PBR assertions guard that ABI.

Malformed buffers, missing Metal textures, invalid indices or incomplete skin
palettes reject the corresponding native transaction. GL traversal and output
remain unchanged during migration.

The feature is exposed through an RLVa permission check, but this is a core
native-output implementation rather than exact RLVa behavior work. Exact RLVa
parity stays in owner-deferred M-013; only minimum behavior needed for Metal/
no-GL cutover is release-critical.

## Validation evidence

- Focused RelWithDebInfo world case: 1,364 assertions under Metal API/GPU/
  load-store/shader validation. It proves RGB-only alpha preservation, one-
  joint rig translation and invalid-palette rejection.
- Focused RelWithDebInfo ordered HUD case: 485 assertions under the same
  validators. It proves static output, alpha preservation, one-joint rig
  translation, invalid-palette rejection and unchanged PBR HUD output.
- Complete physical suites: 45,008 assertions across 51 cases in each of
  RelWithDebInfo, Debug and Release under the same validators.
- Neutral CTest gate: 484/484 in 246.16 seconds.
- Warnings-as-errors RelWithDebInfo touched objects and built-in MSL compiled;
  Release Development `26.4.0d68036` compiled and linked at committed source
  `8361addd0f`.
- Deep strict signing passed. `tools/metal/verify_app.sh` passed all 349 ARM64
  Mach-O slices and the main executable reports arm64.
- Executable SHA-256:
  `064266af7bf95d6e5ddfefe2796ffd0015d8316b22d2bd7fa13ace27cdfd9fdf`.
  Packaged MSL SHA-256:
  `21093c0c3ef9481e55327ecfa17b34dcf25ab771a0021c07d7d7cdcea7bce5d0`.
  Compiled metallib SHA-256:
  `656aa2ec5a7c259f6a86198a12f75fd7ce50914c5ff2865ed7a31208cdb6e1bc`.
- `tools/metal/doctor.sh` passed on the M1 Max acceptance host. Harness and
  tooling tests passed after documentation updates.

## Boundary and next work

This checkpoint does not verify the complete alpha, editing, shadows, post,
stability, strict no-GL or performance gates. The executable still links both
Metal and OpenGL. A live candidate/reference Highlight Transparent comparison
remains part of the controlled editing/alpha matrix.

Continue by tracing final diagnostic-overlay and ordered-UI calls after the
retained world replay. Implement only a demonstrated non-RLVa omission that can
survive native eligibility, with asymmetric physical proof and fail-closed
authority.
