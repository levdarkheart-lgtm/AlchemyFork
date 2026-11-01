# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | ARM64 build/package/runtime identity is evidenced; controlled fixed-view reference benchmark remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | Informational scan covers 2,408 viewer-owned files and 3,303 potential findings (including macros/comments/duplicates); strict release audit is not green. |
| M-003 | Native drawable, presentation and lifecycle | doing | Authenticated exact publication and clean lifecycle are observed; preserve while advanced families are validated. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core typed resource paths have physical coverage; remaining advanced/media matrices stay open. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Native login/world UI and CEF IOSurface ingress work; moving CEF/VLC reference parity remains open. |
| M-007 | World scene and material correctness | doing | Main native world chain publishes exact frames; advanced material/reference coverage remains incomplete. |
| M-008 | Advanced views/effects and compute | doing | Water/default-probe zero has physical proof and one accepted matched live Grignano fixture; continue broader water, probes/mirrors, shadows, transparency/materials, post and output parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-01

Commit `fd0fa7cea5` closes one source-demonstrated water reflection mismatch.
GLSL keeps default probe zero on the reflected water ray, but Metal applied the
generic sphere-parallax correction. The shared radiance helper now receives
water context and skips parallax only for that exact water/default case; every
other probe caller retains its prior rule. A six-face physical control that
moves only the default centre from +4000 to -4000 X failed before the fix with
aggregate RGB delta 1.596944809 and passes afterward below 0.002.

Source-exact warnings-as-errors build 68026 at the committed/pushed head passed
deep strict signing and 349/349 ARM64 inspection. Executable SHA-256 is
`d33603803c5d45434d06e0332e14980dc095505b060624f09143a41f434e1d4e`.
Focused validation passes 1,349/1,349 after the pre-fix 1,348/1,349 failure;
complete RelWithDebInfo and Debug suites each pass 44,902 assertions/50 cases.
The post-change neutral gate passes 484/484 in 259.22 seconds.

Exact-path preflight and actual Computer Use on 68026 completed semantic login,
reached `STATE_STARTED`, showed a coherent upright world/UI, and completed
semantic logout. Logs contain `LogoutReply`, `Goodbye!` and stopped status; the
final viewer/helper census is zero and launch validation variables are cleared.
No Metal validation error was observed.

The bounded live comparison is now accepted. Actual Computer Use flew and
Map-teleported candidate build 68026 and reference build 67607 to
`Grignano (128, 228, 74)`, selected Midday, and captured fresh 2048x1476 PNGs.
Post-exit capture compared 416/416 effective graphics settings with no mismatch
or missing/one-sided setting. Camera/focus X/Y matched within serialized
precision and Z differed by about 1 cm. Human inspection found the frames
materially the same; blurred whole-frame structural SSIM was 0.993616 and the
bounded water/sky crop was 0.998397. Raw temporal SSIM 0.870878 is descriptive,
not a pixel-identity claim. Both viewers stopped cleanly; raw authenticated
artifacts remain local and untracked.

## Complete-product checkpoint gate

1. **Product advancement:** native water no longer applies sphere parallax to
   default probe zero where the active GLSL contract explicitly does not; a
   matched live fixture corroborates the physical proof. This remains bounded
   M-008 progress.
2. **Deferred critical work:** remaining post/alpha/PBR/probe/advanced views;
   source- and scene-identical post comparison; full parity/stability; strict
   no-GL; independent review; and qualifying M1 Max performance evidence.
3. **Priority reassessment:** the bounded water pair is accepted and must not
   be repeated. Move directly to the next source-demonstrated probe/mirror or
   projector/cascade-shadow gap; activity on this completed fixture would not
   advance another release criterion.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Resume source-led work through probes/mirrors and the isolated
projector/cascade shadow fixture. Implement only the first demonstrated
divergence, add an asymmetric physical negative control, then rerun
focused/full Metal and neutral validation. Continue next through
transparency/material replay, non-default post and advanced output.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
