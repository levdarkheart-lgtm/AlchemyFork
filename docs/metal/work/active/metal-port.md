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
| M-007 | World scene and material correctness | doing | Main native world chain publishes exact frames; transparency/material/reference coverage remains incomplete. |
| M-008 | Advanced views/effects and compute | doing | Same-frame cube faces now republish retained projector depth with current-view constants. Continue non-RLVa transparency/material replay, broader shadows, post and output parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact remaining inventory and restart-qualified test prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-01

Commit `d2a863186a` fixes a reachable native projector-shadow cache bug. Hero
and reflection cube capture can render multiple camera faces in one
`gFrameCount` with the same probe capture owner. The raw spot-depth texture is
deliberately retained during cube snapshots, but `generateSunShadow()` derives
`mSunShadowMatrix[4..5]` from each face's current eye-space transform. The old
`fillMetalWorldProjectorShadows()` cache hit reused both the texture and stale
first-face constants.

The cache hit now sends the retained depth texture through the existing
publication lambda, refreshing matrices, clips, resolution, bias/filter,
reverse-Z and unbuilt state without rebuilding depth. The asymmetric physical
control uses one left-shadowed/right-lit depth slice and two consecutive X
translations (0.25 then 0.75); the first render is shadowed and the second
matches the lit reference. This proves per-view refresh rather than merely
re-exercising one matrix.

Focused RelWithDebInfo and Debug validation each pass 1,918 assertions. Full
physical suites pass 44,963 assertions/51 cases in each build mode under Metal
API/GPU/load-store/shader validation. The neutral gate passes 484/484 in 242.94
seconds, and warnings-as-errors object builds pass in both modes.

Source-exact warnings-as-errors Release Development build `26.4.0d68033` at
`d2a863186a` staged and signed. A repeat emitted no `CompileC`; always-run
packaging relinked/staged/signed successfully. Deep strict signing and 349/349
ARM64 inspection pass. Executable SHA-256 is
`a2bb8510781871bb7598b992ea4b1814424f33f4efdf864ae561270d9c501785`;
packaged MSL is
`06454d17c307c6e1b9493a15edbf16e0c59bc385707f59bac327f5b4f71f006c`;
compiled metallib is
`53ac4db074d648096a2a5bb578a30b8b5406579e8bc2c4369e0c1470948e1013`.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers before capture. A mismatched off/on pair is invalid, and a
both-off control may isolate a residual but cannot close shadow parity.

The owner decision for all RLVa remains unchanged: only minimum Metal/no-GL
cutover support is release work; exact parity is the persistent M-013
[RLVa backlog](../deferred/rlva-parity.md). Any admissible comparison must
enable RLV in Preferences in both viewers, save, and restart both. Do not
consume release-gate time on exact RLVa work without a demonstrated native
cutover blocker.

## Complete-product checkpoint gate

1. **Product advancement:** later hero/reflection cube faces no longer consume
   stale first-face projector constants while reusing the valid raw depth map.
2. **Deferred critical work:** transparency/material replay and remaining
   non-RLVa advanced views/post/output; full parity/stability; strict no-GL;
   independent review; and qualifying M1 Max performance evidence. Exact RLVa
   parity remains separately deferred as non-critical M-013.
3. **Priority reassessment:** the projector per-view divergence has an
   implemented and physically validated repair. Move directly to the active
   transparency/material production graph and edit only a demonstrated
   reachable divergence.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Trace the active GLSL and Metal transparency/material replay calls, implement
the first demonstrated non-RLVa divergence, add an asymmetric physical control,
and rerun focused/full physical plus neutral validation. Continue
owner-by-owner native-authority/no-GL work afterward; keep M-013 exact RLVa work
off the release path.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
