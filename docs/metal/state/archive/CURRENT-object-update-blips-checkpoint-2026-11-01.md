# Native object-update blips checkpoint — 2026-11-01

**Observed host:** physical Apple M1 Max.
**Branch:** `codex/apple-silicon-metal`.
**Code checkpoint:** `b328cba4b6` (`feat(metal): replay object update blips natively`).

## Integrated output owner

The developer **Show Updates to Objects** mode retains short-lived colored
world-space points in `LLPipeline::mDebugBlips`. Its GL owner updates each
point's age and height, discards expired entries, then draws eight-pixel points
with RGB writes enabled, alpha writes disabled, an always-pass depth test and
depth writes enabled. This output occurs after the retained native world replay
and outside ordered UI.

Commit `b328cba4b6` captures each surviving point at that authoritative boundary
and appends native replay after Highlight Transparent and before display post.
The built-in Metal vertex stage performs the existing GL-to-Metal depth
conversion and viewport Y flip; the point fragment preserves retained alpha.
Dedicated HDR/non-HDR RGB-only pipelines use always-pass depth comparison with
depth writes. Missing storage, invalid counts, nonfinite positions/colors or a
nonfinite matrix reject native authority. The migration GL draw remains in
place. The same change fixes an existing expired-entry use-after-erase by
continuing immediately after erase.

## Physical validation

- Focused RelWithDebInfo world replay: 1,385 assertions under Metal API/GPU/
  load-store/shader validation. Asymmetric red and green controls prove exact
  eight-pixel footprints and centers, RGB-only alpha preservation, 0.5 depth
  writes, unchanged background, missing-storage rejection and nonfinite-input
  rejection.
- Complete RelWithDebInfo, Debug and Release Metal suites: 45,035 assertions
  across 52 cases in each configuration under the same validators.
- Neutral gate: 484/484 tests passed in 244.24 seconds.
- Warnings-as-errors Metal renderer, built-in MSL, GPU test and production
  pipeline objects compile/link in the validated configurations.

## Source-exact packaged evidence

Release Development `26.4.0d68040` built, linked, staged and signed from
`b328cba4b6ca5671890c4129145594732c3536e5`.

- Deep strict signature validation: pass.
- ARM64 bundle inspection: 349/349 Mach-O files valid; main executable arm64.
- Executable SHA-256:
  `fcba555e47abed64eedafa6f60f5b577104ffeb2a8c0f1692f7e813082e82d32`.
- Packaged MSL SHA-256:
  `ab73e9f26014d8ec13d7e02bf78cf5e23a0f24c75337779d8db3ddd6b9d57146`;
  it matches the committed source MSL.
- Compiled metallib SHA-256:
  `7ca21604f3fe91e3755df3e034266c1cd2b4ce766b2bd31c86d2d90ba3905b30`.
- The executable still links Metal and OpenGL; strict no-GL remains open.

## Pathfinding reachability correction

The current ARM64 OSS target links `llphysicsextensionsos`, not a functional
proprietary pathing library. `LLPathingLib::isFunctional()` returns false, the
stub initializer returns `LLPL_NOT_IMPLEMENTED`, `getInstance()` remains null,
and the stub render methods emit nothing. Pathfinding visualization is therefore
not a reachable retained-Alchemy output in this product build. It is recorded
as noncritical/not-applicable rather than being reimplemented speculatively.
The latent eligibility guard remains safe for a future functional extension.

## Persistent comparison prerequisites

- Shadow parity pairs require shadows enabled in Graphics Preferences in both
  viewers before capture; off/on is invalid and both-off is isolation only.
- Exact RLVa work remains deferred M-013. Any later comparison must enable RLV
  in Preferences in both viewers, save, and restart both viewers.

The next bounded audit covers only reachable general render-debug, wireframe,
reflection-probe-volume and far-plane output. Full feature/stability, strict
no-GL and qualifying performance release gates remain open.
