# Authored-mirror owner-confirmed checkpoint — 2026-11-02

## Scope and result

One controlled private dry-fixture authored-mirror sample is accepted for
visual parity. This supersedes the immediately preceding mismatched screenshots
for this fixture only; it does not close the complete reflection-probe/mirror
matrix, legacy alpha/stained-glass parity, strict no-GL, stability or
performance gates.

The owner inspected the latest reference and candidate images and explicitly
confirmed that they are visually the same: both contain the seated avatar in
the mirror with the same horizon/stair content and equivalent exposure/tone.
The earlier claim that Metal was darker was retracted after the correct pair
was selected. Earlier fallback, wrong-profile and non-converged images remain
rejected.

## Exact binaries and artifacts

- Repository observation HEAD: `3cd2aa0d977edf064418b6e8141862a7a10be69e`.
- Reference: Development `26.4.0.67607`, executable SHA-256
  `fac5225435df2c096d10b856f6e0f186b37e41446576398ee8f4842b884ef28d`.
- Candidate: Development `26.4.0.68099`, executable SHA-256
  `ea063150c286acf7498a3c6e065cbe367e140b401ef3682536d1771d357f602a`;
  its embedded renderer source remains `ae2b43644211a73802d4197efa39a361ad5614bb`.
- Reference run:
  `artifacts/metal/qa-runs/reference-live-inspection-20261102-01/`.
- Candidate run:
  `artifacts/metal/qa-runs/candidate-live-inspection-20261102-01/`.
- Reference image `reference-user-saved.png`: 2048x1476, SHA-256
  `c41e5ad16c37d4d3857edf9022482b28d6b4d86bf983d01013a295a0b06a5b17`.
- Candidate image `candidate-lightbox-preset.png`: 2048x1476, SHA-256
  `926b13a49df4d4efe2f44b27dae3908fddd1fc82effb3877a5b6b5fad5d141a8`.

The exact private SLURL remains only in ignored evidence. No credential or
remembered-password content is stored in this checkpoint.

## Scene and settings qualification

Both viewers report the identical seated local position
`(106.3004913330, 23.3657855988, 24.5636901855)` and world-global position
`(261482.300491333, 241431.365785599, 24.5636901855)`. Both used Midday and
the same 2048x1476 UI/HUD-off local Snapshot flow.

The two generated `session-settings.xml` files are byte-identical, contain the
same 175 merged Graphics/Looks controls and have SHA-256
`ded3d303cd878766505733d2b68ddcdbb09ad973e6e885f36c1fe6aa20069113`.
Source profile hashes are:

- Graphics `Codex Metal Mirror Q`:
  `4fb96d9a4a53f5c969a929896681b6527296066b493c081c25a96f40d30d8316`.
- Looks/Lightbox `CodexMetalMirrorQ`:
  `3a938a1ddba613b0c946fcc72de8148886626f6a6a44a9f3733d20b46857f226`.

Twelve common live controls compare without a difference: quality, far clip,
shadow detail, probe level/detail, SSR, depth of field, dynamic exposure,
manual exposure, tonemap type/mix and lens-flare strength. Candidate alone
sets the intended nonpersistent
`ALMetalHeroMirrorSnapshotValidation=true`; this selects the bounded native
Snapshot route and is not a user-visible appearance difference. The two saved
presets were installed under the active profile with their distinct names so
subsequent viewers can select them in their normal Graphics and Lightbox UIs.

## Native proof and descriptive metrics

Candidate log at `20:44:19Z` records the Development-only authored-mirror
diagnostic and native 2048x1476 color readback; `20:44:22Z` records the exact
saved file. The same run records complete six-face mirror-clipped hero radiance
publication. Reference records its exact GL save at `20:38:03Z`.

The images are not pixel-registered: camera framing, clouds and foliage phase
remain sequential-live differences. Therefore whole-frame raw SSIM
`0.261788`, average PSNR `12.430441 dB`, half-size sigma-8 structural SSIM
`0.741578` and the fixed mirror-interior SSIM `0.504290` are retained only as
descriptive diagnostics. They are not used to contradict the direct owner
inspection or promoted into release thresholds.

The candidate emitted temporary fail-closed WindLight incomplete-layer
warnings before the accepted save; the accepted Snapshot nevertheless records
native color readback. Those warnings remain runtime diagnostics and are not
silently converted into a broad sky/probe pass.

## Shutdown and bounded conclusion

Reference exited normally with `Goodbye!` / `status: stopped` at `20:39:02Z`.
Candidate exited normally with the same markers at `20:46:39Z`. Exact process
inspection found no remaining viewer process.

This accepts one controlled authored-mirror visual sample and retires the prior
wrong-pair darkness/avatar claims. Normal live mirror authority remains
fail-closed until its separate integration decision; broader probes/mirrors,
legacy alpha/stained glass, moving media, alternate environments and the full
feature matrix remain open.
