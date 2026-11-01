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
| M-002 | GL/shader/pass/resource and binary inventory | doing | Informational owned-source inventory exists; strict release audit is not green. |
| M-003 | Native drawable, presentation and lifecycle | doing | Authenticated exact publication and clean lifecycle are observed; preserve while advanced families are validated. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core typed resource paths have physical coverage; remaining advanced/media matrices stay open. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Native login/world UI and CEF IOSurface ingress work; moving CEF/VLC reference parity remains open. |
| M-007 | World scene and material correctness | doing | Main native world chain publishes exact frames; advanced material/reference coverage remains incomplete. |
| M-008 | Advanced views/effects and compute | doing | A matched 386-setting authored-mirror pair rejects the suspected large blur defect for one fixture; continue the broader post, transparency/PBR, probe and advanced-family matrix. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Tooling and structural reductions exist; qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation passed; fail-closed exact-app/profile/hash/session preflight now prevents stale or ordinary-profile evidence. |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-01

Commit `7b27e5d62e` remains the renderer checkpoint after the legacy material and
bump normal corrections. Corrected focused validation passes 1,322 assertions;
both complete physical configurations pass 44,865 assertions/50 cases. The
neutral gate passes 484/484 tests. Warnings-as-errors Development 68012 builds
source-exact, repeats with no work, passes deep signing and verifies 349/349
ARM64 slices.

An initial authored-mirror comparison was invalid because its effective
profiles differed on 25 of 386 visual settings, including the quality preset
and mirror resolution. Actual Computer Use repeated the fixed 2048x1476 view
with post-run manifests proving 386/386 identical settings: quality `3`,
mirrors enabled and mirror resolution `512`. The full frame scores SSIM
`0.987409` / PSNR `39.185075 dB`; the mirror frame scores `0.988105` /
`37.793470 dB`, and the mirror interior scores `0.991375` / `41.100904 dB`.
Visual and amplified-difference inspection finds no material mirror-region
residual. This rejects the suspected large blur defect for this fixture but
does not close the broader mirror/probe matrix; native authority stays guarded.

The build cache uses Homebrew Bison 3.8.2 and a build-local Python environment
with the repository-declared `llsd 1.2.4` and `PyYAML 6.0.3` requirements.
`llsd` is Linden Research's Python package from `secondlife/python-llsd`, not a
Bison component; CMake selects the virtual-environment interpreter explicitly.

The continuity harness retains superseded checkpoints in indexed archives and
checks compactness, live links, executable commands, cross-document milestone
state and seven immutable release invariants. The new Computer Use preflight
requires the exact Development/Diagnostic app version and SHA-256, a singleton
exact executable, isolated profile environment and live isolated log before UI
binding. `tools/metal/graphics_settings.py` now merges shipped defaults and an
isolated profile, records source hashes, and fails a comparison on any missing,
mismatched or unilateral visual setting. The 31-file harness, all 22 tooling
tests, Python bytecode, diff checks and the M1 Max doctor pass.

The controlled viewer run completed semantic login, fixed-location setup,
Snapshot refresh/save and clean logout. Native-readback markers, exit 0,
`LogoutReply`, `Goodbye!`, stopped status and a zero-process census were
observed. Startup cloud-generation warnings ended before capture and are not
accepted as performance or parity evidence.

## Complete-product checkpoint gate

At every substantial engineering checkpoint, update all three fields:

1. **Product advancement:** name the product/release requirement moved by the
   change and the causal evidence. Current: the matched-settings gate repairs
   validation integrity and the corrected pair rejects a false mirror-defect
   signal; this is not a renderer implementation or broad M-008 closure.
2. **Deferred critical work:** list still-open critical dependencies. Current:
   advanced-family parity, full matrix/stability, strict no-GL, independent
   review/reproducibility and paired M1 Max performance evidence.
3. **Priority reassessment:** choose the next action from dependency/risk
   evidence. Current: stop pursuing the disproven large mirror-blur hypothesis.
   Audit the next open post-effect and alpha/PBR owners, add a causal physical
   control for the first demonstrated gap, and require matched manifests for
   any new live pair. M-009, owner-by-owner M-011 cutover and final M-010 claims
   remain downstream of stable parity.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Audit the authoritative post-effect and alpha/PBR pass graph against the native
owners. Select the first concrete missing data, state or ordering contract, add
an asymmetric physical negative control, implement the bounded correction, and
run focused plus full Debug/RelWithDebInfo validation. Any new Computer Use pair
must pass exact preflight and post-run graphics-manifest comparison before image
metrics are admissible.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
