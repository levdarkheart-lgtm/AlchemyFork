# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `1518b533ce`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | ARM64 build/package/runtime identity is evidenced; controlled fixed-view reference benchmark remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | Refreshed informational scan covers 2,408 viewer-owned files and 3,351 potential findings including macros/comments/duplicates; strict release audit exits 1 and is not green. |
| M-003 | Native drawable, presentation and lifecycle | doing | Authenticated exact publication and clean lifecycle are observed; preserve while advanced families are validated. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core typed resource paths have physical coverage; remaining advanced/media matrices stay open. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Native login/world UI and CEF IOSurface ingress work; Highlight Transparent and Batch Size have ordered static/rigged HUD replay. Moving CEF/VLC reference parity remains open. |
| M-007 | World scene and material correctness | doing | Bounded active transparency/material and reflection-probe audits found no valid-production divergence. Broader live parity remains incomplete. |
| M-008 | Advanced views/effects and compute | doing | The established native advanced-output set now includes Raycast, Occlusion, Octree, Physics Shapes, Normals and Texel Density. No reachable render-debug world-geometry bit remains in the explicit visual veto. OSS pathfinding is a nonfunctional stub. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, prolonged stability and strict no-GL release evidence remain open. The refreshed 3,351-finding inventory exits 1. Directional shadows are preference-qualified; projector-visible parity is open. Hero cube orientation is proven; forward/material ownership and mirror parity remain open and fail-closed. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-02

Commit `ed9e64736d` admits the scoped native mirror Snapshot diagnostic only in
non-public compiled channels. Retained renderer commit `47d253a91c` captures
and replays every reachable Develop **Texel Density** face draw. Its
current/desired/full modes still perform their exact
texture lookup, checkerboard bind and texture-matrix side effects in source.
The bound production `gDebugProgram`, however, consumes only position and the
current uniform color, so native presentation replays that actual visible
zero-copy indexed overlay without inventing a texture/UV shader path.

Replay preserves live transforms, arbitrary finite current color, u16/u32
indices, disabled culling, fill raster, source-over blend, RGB-only writes and
writable LEQUAL depth under forward/reverse Z. HUD faces remain in native
ordered indexed UI capture. Validation rejects invalid storage, buffers,
ranges, offsets, index width, transforms, values, state/order and all cross-
owner collisions.

Commit `1518b533ce` adds a source-derived physical regression of the GL
cube-camera to Metal texture-cube convention. It publishes asymmetric values
through the production six-face hero path, samples all 96 texel-centre rays and
would fail a face swap, row/column flip or 90-degree rotation. The direct face
copy is correct, so no speculative production transform was added.

RelWithDebInfo, Debug and Release physical Metal suites each pass 52,184
assertions in 52 cases under Metal API/load-action validation. Warnings-as-errors
production integration passes, and the source-exact Release neutral
non-manual/non-GL gate passes 484/484 in 38.39 seconds. The prior unfiltered
574-test inventory was not rerun;
its six GL-only failures and five skips remain recorded without a green claim.
Harness validation covers all 31 required files and all 24 tooling tests pass.

Source-exact Release Development build `26.4.0d68097` (`26.4.0.68097`) at
`1518b533ce48b68a13fdcb0d483bcd7df8e070b4` passes.
Deep strict signing and 349/349 ARM64 inspection pass. Executable SHA-256 is
`06c4e8f7b258dc24a64090a464da908fc4543a4819306a07d885a9dca7c57909`;
packaged/source MSL is
`d263315d1ea0c2c834e9e5a5d37109e9ec5d56325153220dedc1b2a9ab2ffac9`;
compiled metallib is
`ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
The executable still links OpenGL, so strict no-GL remains open.

The explicit render-debug world-output guard is now empty. The latent
pathfinding guard remains separately fail-closed. The main executable still
links OpenGL and Metal, so this is not a native-only claim.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers and save the setting before capture. Shadows do not require a
restart; restart applies only to RLVa state changes. A mismatched off/on pair is
invalid, and a both-off control cannot close shadow parity.

The 2026-11-02 fixed-Midday Grignano run satisfied that workflow. Both isolated
profiles had effective `RenderShadowDetail=2`, all 416 captured visual settings
matched, and both restarted Advanced Graphics panels displayed
`ShadowDetail=2`. Exact-path preflight passed for reference `26.4.0.67607` and
candidate `26.4.0.68089`. With `ALMetalWorldPresentation=true`, candidate logs
proved direct Metal directional/projector shadow slices and continuous exact
native world plus ordered UI/HUD publication without GL swap. Fixed geometry
and directional shadow edges remained coherent (raw SSIM `0.933767`, average
PSNR `22.091122 dB`, half-size sigma-8 structural SSIM `0.968037`). This
preference-qualified run corroborates directional shadows only: the fixture did
not isolate a visible projector contribution, and projector-visible,
alpha/avatar-caster, cascade and PCSS/quality parity remain open. Earlier
Debug-Settings-only evidence cannot independently close a shadow row.

A retained pre-signal sample corrects the next authored-mirror attempt: all
40 main-thread samples were active in renderer work under full Metal validation,
and later `SIGTERM` shutdown completed cleanly. The actual blocker was that the
Release configuration compiled the Development validation setting to false.
Commit `ed9e64736d` now gates the scoped diagnostic by immutable compiled
channel, leaving public Release and normal live authority unchanged.

The source-exact 68093 diagnostic produced a matched 2048x1476 pair with
416/416 visual settings aligned and complete six-face native hero publication.
It exposes real divergence: patterned/stained door glass, wall media/art and
nearby forward content are missing or replaced by gray translucent/blurred
surfaces. Whole-frame raw SSIM is `0.855629`; half-size sigma-8 structural SSIM
is `0.938252`. Normal mirror authority remains fail-closed. The indexed archive
retains exact images, hashes, crops, metrics and provenance.

The banned/uncontrolled authored-mirror region is retired. Its replacement is
an owner-controlled private dry fixture whose exact SLURL remains only in the
ignored evidence tree. Reproduction is: teleport with World Map, sit on the
fixed red chair, press `Esc` twice, wait for convergence, then capture reference
and candidate sequentially as custom 2048x1476 PNGs with UI/HUD off,
post-processing on and `No Filter`. Pause after each save for the owner to check
Graphics Preferences. Water will use a separate controlled fixture.

That procedure produced canonical candidate/reference images with SHA-256
`146158b4dd2ff42e8231e570aae0545cb8d4a2f7f5aec75d4ad5ec9ad316635a`
and `5f2949522861431cd973f040aa3c4692b0aec651909d9b8786689ac40b294f34`.
Fresh profile manifests compare all 416 visual settings with zero mismatches;
mirrors and shadows are on and SSR is off. The ordinary scene is coherent, but
camera alignment is not pixel-exact and the mirror contents visibly differ, so
the pair is diagnostic rather than a parity pass. The candidate logged complete
six-face native mirror-clipped publication/readback and exited cleanly. The
reference accepted `SIGTERM`, then hung in `Display:RenderFlush`, triggered its
watchdog and required termination of the exact verified process (exit 137);
all verified reference helpers were drained. Exact images, settings manifests,
comparison and private reproduction notes are retained under ignored
`artifacts/metal/` and `/private/tmp` paths.

The dry pair cannot assign the visible difference to a renderer owner: the
cameras and mirror contents differ, and log inspection found 64 candidate
versus 24 reference fetch-failure/HTTP/missing-resource matches. Require matched
content/cache convergence before using another pair as causal parity evidence.

Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both. Exact RLVa correctness is deferred M-013; implement only the
minimum behavior needed to unblock Metal/no-GL cutover.

## Complete-product checkpoint gate

1. **Product advancement:** source-derived physical coverage now protects the
   correct six-face hero camera convention and eliminates a false production
   transform candidate. Mirror/material ownership remains fail-closed.
2. **Deferred critical work:** broader post parity; projector-visible,
   alpha/avatar-caster, cascade and quality shadow coverage; controlled live
   comparisons; stability/feature matrices; strict no-GL; independent review;
   and qualifying M1 Max performance evidence.
3. **Priority reassessment:** continue M-009 from the refreshed strict
   viewer-owned GL and presentation-authority inventory, then resolve the next
   concrete reachable release blocker from current source and live evidence.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Trace forward/material batch and texture ownership from source, then obtain a
camera- and content-converged controlled pair before changing mirror authority.
The six-face camera/row/rotation convention is physically proven and is no
longer a production-edit candidate. Continue the 3,351-finding GL inventory
independently. Keep all shadow evidence Preferences/save-qualified and exact
RLVa deferred in M-013.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
